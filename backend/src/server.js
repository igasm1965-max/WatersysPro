/*
  Minimal backend PoC:
  - subscribes to MQTT (device/+/telemetry, device/+/alert)
  - saves telemetry/alerts to Postgres
  - emits realtime updates via Socket.IO
  - REST endpoints for status/history/command
  - push notifications via Expo (optional)
*/

require('dotenv').config();
const express = require('express');
const http = require('http');
const path = require('path');
const fs = require('fs');
const cors = require('cors');
const bodyParser = require('body-parser');
const socketIo = require('socket.io');
const mqtt = require('mqtt');
let Expo;
let expo = null;
try {
  Expo = require('expo-server-sdk').Expo;
  expo = new Expo();
} catch (err) {
  console.warn('expo-server-sdk not installed — push notifications disabled');
  expo = null;
}
const { pool, initDb } = require('./db');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');

const PORT = process.env.PORT || 3000;
const MQTT_BROKER = process.env.MQTT_BROKER || 'mqtts://esp32:REDACTED_HIVEMQ_PASS@REDACTED_HIVEMQ_HOST:8883';
const MQTT_TOPIC_BASE = process.env.MQTT_TOPIC_BASE || 'watersystem';
const JWT_SECRET = process.env.JWT_SECRET || 'change_this_secret';
const REMOTE_ADMIN_TOKEN = process.env.REMOTE_ADMIN_TOKEN || '';
const REMOTE_REQUIRE_TOKEN = (process.env.REMOTE_REQUIRE_TOKEN || 'true') !== 'false';
const REPO_ROOT = path.join(__dirname, '..', '..');
const DOCS_APP_ROOT = path.join(REPO_ROOT, 'docs', 'app');
const DOCS_ALLOWED_FILES = new Set([
  'DOCUMENTATION_INDEX.md',
  'README.md',
  'README_RU.md',
  'CHANGELOG.md',
  'CHANGELOG_RU.md',
  'RELEASE_NOTES_v1.1.0.md',
  'RELEASE_NOTES_v1.1.0_RU.md',
  'PROJECT_STATUS.md',
  'docs/WORKTREE_CHANGES_2026-03-19.md',
  'docs/PRODUCTION_HARDENING_TRACKER.md',
  'docs/SD_INTEGRATION.md',
  'docs/REFRACTOR_AND_SMOKE_SUMMARY.md',
  'mobile/START_HERE.md',
  'mobile/BUILD_APK_GUIDE.md',
  'mobile/CHECKLIST.md',
  'mobile/DEVICE_CONTROL_GUIDE.md',
]);

const REMOTE_TARGETS = new Set(['pump1', 'pump2', 'aeration', 'ozone', 'filter', 'backwash', 'set_mode', 'reset_emergency']);


const app = express();
app.use(cors());
app.use(bodyParser.json());

const server = http.createServer(app);
const io = socketIo(server, { cors: { origin: '*' } });

// Simple in-memory cache for push tokens (also persisted to DB)
let pushTokensCache = new Set();
let dbReady = false;
let latestRemote = {
  topicBase: MQTT_TOPIC_BASE,
  deviceId: MQTT_TOPIC_BASE,
  status: 'unknown',
  telemetry: null,
  state: null,
  lastSeenTopic: '',
  lastSeenAt: null,
};

function updateRemoteSeen(topic) {
  latestRemote.lastSeenTopic = topic;
  latestRemote.lastSeenAt = new Date().toISOString();
}

function parseIncomingTopic(topic) {
  // New format used by current ESP firmware: watersystem/<kind>
  if (topic.startsWith(`${MQTT_TOPIC_BASE}/`)) {
    const kind = topic.substring(MQTT_TOPIC_BASE.length + 1);
    return { family: 'base', deviceId: MQTT_TOPIC_BASE, kind };
  }

  // Legacy PoC format: device/<id>/<kind>
  const parts = topic.split('/');
  if (parts.length >= 3 && parts[0] === 'device') {
    return { family: 'legacy', deviceId: parts[1], kind: parts[2] };
  }

  return null;
}

function remoteTokenOk(req) {
  if (!REMOTE_REQUIRE_TOKEN) return true;
  if (!REMOTE_ADMIN_TOKEN) return false;
  const token = req.headers['x-admin-token'] || req.query.token || (req.body && req.body.token);
  return token && token === REMOTE_ADMIN_TOKEN;
}

function requireRemoteToken(req, res, next) {
  if (!REMOTE_REQUIRE_TOKEN) return next();
  if (!REMOTE_ADMIN_TOKEN) {
    return res.status(503).json({ error: 'REMOTE_ADMIN_TOKEN is not configured on server' });
  }
  if (!remoteTokenOk(req)) {
    return res.status(401).json({ error: 'invalid admin token' });
  }
  next();
}

async function loadPushTokens() {
  const r = await pool.query('SELECT token FROM push_tokens');
  r.rows.forEach(r => pushTokensCache.add(r.token));
}

async function sendPushNotification(title, body, data = {}) {
  if (!expo) return;
  const messages = [];
  for (let token of pushTokensCache) {
    if (!Expo.isExpoPushToken(token)) continue;
    messages.push({ to: token, sound: 'default', title, body, data });
  }
  if (messages.length === 0) return;
  const chunks = expo.chunkPushNotifications(messages);
  for (const chunk of chunks) {
    try {
      const receipts = await expo.sendPushNotificationsAsync(chunk);
      console.log('Push receipts', receipts);
    } catch (err) {
      console.error('Push error', err);
    }
  }
}

// --- MQTT client ---
const client = mqtt.connect(MQTT_BROKER);
client.on('connect', () => {
  console.log('MQTT connected to', MQTT_BROKER);
  console.log('Remote topic base:', MQTT_TOPIC_BASE);
  client.subscribe('device/+/telemetry');
  client.subscribe('device/+/alert');
  client.subscribe(`${MQTT_TOPIC_BASE}/#`);
});

client.on('message', async (topic, payload) => {
  try {
    const txt = payload.toString();
    const parsed = parseIncomingTopic(topic);
    if (!parsed) return;

    const { deviceId, kind } = parsed;
    updateRemoteSeen(topic);

    let obj = null;
    if (kind !== 'status') {
      obj = JSON.parse(txt);
    }

    if (topic === `${MQTT_TOPIC_BASE}/status` || kind === 'status') {
      latestRemote.status = txt;
      io.emit('device_presence', { deviceId, status: txt, topic, ts: latestRemote.lastSeenAt });
      return;
    }

    if (kind === 'telemetry') {
      latestRemote.telemetry = obj;
      latestRemote.deviceId = deviceId;
      await pool.query('INSERT INTO telemetry(device_id, payload) VALUES($1,$2)', [deviceId, obj]);
      io.emit('status_update', { deviceId, payload: obj });
    } else if (kind === 'state') {
      latestRemote.state = obj;
      latestRemote.deviceId = deviceId;
      io.emit('device_state', { deviceId, payload: obj });
    } else if (kind === 'alert') {
      await pool.query('INSERT INTO alerts(device_id, type, severity, message) VALUES($1,$2,$3,$4)', [deviceId, obj.type || 'alert', obj.severity || 'warning', obj.message || JSON.stringify(obj)]);
      io.emit('alert', { deviceId, ...obj });
      // send push (optional)
      await sendPushNotification(`Alert on ${deviceId}: ${obj.type || 'alert'}`, obj.message || 'See device', { deviceId });
    }
  } catch (err) {
    console.error('MQTT message handler error', err);
  }
});

// Serve static assets (firmware for OTA)
app.use('/firmware', express.static('static'));
app.use('/remote', express.static(path.join(__dirname, '..', 'public')));
app.use('/docs', express.static(DOCS_APP_ROOT));

app.get('/docs/content', async (req, res) => {
  try {
    const relPath = String(req.query.path || '').replace(/\\/g, '/');
    if (!DOCS_ALLOWED_FILES.has(relPath)) {
      return res.status(404).send('Document is not available');
    }

    const absPath = path.join(REPO_ROOT, relPath);
    const data = await fs.promises.readFile(absPath, 'utf8');
    res.setHeader('Content-Type', 'text/plain; charset=utf-8');
    return res.status(200).send(data);
  } catch (err) {
    console.error('Docs content error', err);
    return res.status(500).send('Failed to load document');
  }
});

app.get('/api/remote/config', (req, res) => {
  res.json({
    topicBase: MQTT_TOPIC_BASE,
    requireToken: REMOTE_REQUIRE_TOKEN,
  });
});

app.get('/api/remote/latest', requireRemoteToken, (req, res) => {
  res.json({
    ok: true,
    ...latestRemote,
  });
});

app.post('/api/remote/command', requireRemoteToken, (req, res) => {
  const target = String(req.body.target || '').trim();
  const value = String(req.body.value || '').trim().toLowerCase();

  if (!REMOTE_TARGETS.has(target)) {
    return res.status(400).json({ error: 'unsupported target' });
  }

  // Validate value based on target type
  const RELAY_TARGETS = new Set(['pump1', 'pump2', 'aeration', 'ozone', 'filter', 'backwash']);
  if (RELAY_TARGETS.has(target)) {
    if (value !== 'on' && value !== 'off') {
      return res.status(400).json({ error: 'value must be on/off' });
    }
  } else if (target === 'set_mode') {
    if (value !== 'manual' && value !== 'auto') {
      return res.status(400).json({ error: 'value must be manual/auto' });
    }
  } else if (target === 'reset_emergency') {
    // Any value accepted, device expects '1'
  }

  const topic = `${MQTT_TOPIC_BASE}/cmd/${target}`;
  const ok = client.publish(topic, value, { qos: 0, retain: false });
  if (!ok) {
    return res.status(500).json({ error: 'publish failed' });
  }

  res.json({ ok: true, topic, payload: value });
});

// --- REST API ---
app.get('/api/health', (req, res) => {
  res.json({
    ok: true,
    dbReady,
    docsReady: true,
  });
});

// Firmware metadata (used by devices to check updates)
app.get('/api/firmware/latest', (req, res) => {
  // In production store version in DB or CI pipeline
  const version = '0.3.0';
  const url = `${req.protocol}://${req.get('host')}/firmware/firmware.bin`;
  res.json({ version, url });
});

app.get('/api/devices', async (req, res) => {
  const r = await pool.query("SELECT DISTINCT device_id FROM telemetry ORDER BY device_id");
  res.json(r.rows.map(r => r.device_id));
});

app.get('/api/status/:deviceId', async (req, res) => {
  const deviceId = req.params.deviceId;
  const r = await pool.query('SELECT payload, ts FROM telemetry WHERE device_id=$1 ORDER BY ts DESC LIMIT 1', [deviceId]);
  if (r.rows.length === 0) return res.status(404).json({ error: 'no data' });
  res.json({ deviceId, ts: r.rows[0].ts, payload: r.rows[0].payload });
});

app.get('/api/history/:deviceId', async (req, res) => {
  const deviceId = req.params.deviceId;
  const days = Number(req.query.days || 7);
  const r = await pool.query('SELECT ts, payload FROM telemetry WHERE device_id=$1 AND ts >= now() - ($2::int || \" days\")::interval ORDER BY ts ASC', [deviceId, days]);
  res.json(r.rows.map(row => ({ ts: row.ts, ...row.payload })));
});

// Aggregated history (daily averages for a metric)
app.get('/api/history-aggregate/:deviceId', async (req, res) => {
  const deviceId = req.params.deviceId;
  const days = Number(req.query.days || 7);
  const metric = req.query.metric || 'pressure';
  try {
    const q = `SELECT date_trunc('day', ts) AS day, avg( (payload ->> $3)::numeric ) AS avg_val FROM telemetry WHERE device_id=$1 AND ts >= now() - ($2::int || '' days'')::interval GROUP BY day ORDER BY day`;
    const r = await pool.query(q, [deviceId, days, metric]);
    res.json(r.rows.map(row => ({ day: row.day, avg: row.avg_val })));
  } catch (err) { console.error(err); res.status(500).json({ error: 'db error' }); }
});

// Protected: send command to device (requires JWT)
app.post('/api/command/:deviceId', authenticate, async (req, res) => {
  const deviceId = req.params.deviceId;
  const cmd = req.body;
  client.publish(`device/${deviceId}/command`, JSON.stringify(cmd));
  // optional: log who sent the command (req.user.email)
  res.json({ ok: true });
});

// Public: register Expo push token
app.post('/api/push/register', async (req, res) => {
  const { token } = req.body;
  if (!token) return res.status(400).json({ error: 'token required' });
  try {
    await pool.query('INSERT INTO push_tokens(token) VALUES($1) ON CONFLICT (token) DO NOTHING', [token]);
    pushTokensCache.add(token);
    res.json({ ok: true });
  } catch (err) {
    console.error('push register', err);
    res.status(500).json({ error: 'db error' });
  }
});

// --- AUTH: register/login + middleware ---
app.post('/api/auth/register', async (req, res) => {
  const { email, password } = req.body;
  if (!email || !password) return res.status(400).json({ error: 'email+password required' });
  const hash = await bcrypt.hash(password, 10);
  try {
    const insert = await pool.query(
      'INSERT INTO users(email, password_hash) VALUES($1,$2) RETURNING id, role',
      [email, hash]
    );
    const user = insert.rows[0];
    const token = jwt.sign({ userId: user.id, email, role: user.role }, JWT_SECRET, { expiresIn: '8h' });
    res.json({ ok: true, token });
  } catch (err) {
    console.error('register error', err);
    res.status(500).json({ error: 'db error' });
  }
});

app.post('/api/auth/login', async (req, res) => {
  const { email, password } = req.body;
  if (!email || !password) return res.status(400).json({ error: 'email+password required' });
  try {
    const r = await pool.query('SELECT id, password_hash, role FROM users WHERE email=$1', [email]);
    if (r.rows.length === 0) return res.status(401).json({ error: 'invalid credentials' });
    const u = r.rows[0];
    const ok = await bcrypt.compare(password, u.password_hash);
    if (!ok) return res.status(401).json({ error: 'invalid credentials' });
    const token = jwt.sign({ userId: u.id, email, role: u.role }, JWT_SECRET, { expiresIn: '8h' });
    res.json({ token });
  } catch (err) {
    console.error('login error', err);
    res.status(500).json({ error: 'server error' });
  }
});

function authenticate(req, res, next) {
  const h = req.headers.authorization;
  if (!h || !h.startsWith('Bearer ')) return res.status(401).json({ error: 'unauthorized' });
  const token = h.slice(7);
  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    req.user = decoded;
    next();
  } catch (err) { return res.status(401).json({ error: 'invalid token' }); }
}


// --- Socket.IO ---
io.on('connection', (socket) => {
  console.log('ws client connected', socket.id);
  socket.on('disconnect', () => console.log('ws client disconnected', socket.id));
});

// --- start ---
(async () => {
  try {
    await initDb();
    await loadPushTokens();
    dbReady = true;
  } catch (err) {
    dbReady = false;
    console.error('Startup warning: DB unavailable, running in degraded mode', err && err.code ? err.code : err);
  }

  server.listen(PORT, () => {
    console.log(`Backend listening on http://localhost:${PORT}`);
    if (!dbReady) {
      console.log('Docs are available at /docs/, DB-dependent API may fail until PostgreSQL is up.');
    }
  });
})();
