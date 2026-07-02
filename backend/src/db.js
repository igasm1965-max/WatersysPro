const { Pool } = require('pg');
const fs = require('fs');
const path = require('path');
require('dotenv').config();

const databaseUrl = process.env.DATABASE_URL;
const useDatabaseSsl = process.env.DATABASE_SSL
  ? process.env.DATABASE_SSL === 'true'
  : !!(databaseUrl && !/localhost|127\.0\.0\.1/.test(databaseUrl));

let _pool = null;
let _dbAvailable = false;

// In-memory fallback stores (used when PostgreSQL is unavailable)
const memStore = {
  telemetry: [],        // { device_id, ts, payload }
  deviceLogs: [],       // { device_id, content, ts }
  alerts: [],           // { device_id, type, severity, message, ts }
  devices: new Set(),   // Set<device_id>
};

function initPool() {
  if (_pool) return;
  _pool = databaseUrl
    ? new Pool({
        connectionString: databaseUrl,
        ...(useDatabaseSsl ? { ssl: { rejectUnauthorized: false } } : {}),
      })
    : new Pool({
        host: process.env.PGHOST || 'localhost',
        user: process.env.PGUSER || 'postgres',
        password: process.env.PGPASSWORD || 'postgres',
        database: process.env.PGDATABASE || 'watersys',
        port: process.env.PGPORT ? Number(process.env.PGPORT) : 5432,
      });
}

async function initDb() {
  try {
    initPool();
    await _pool.query(`SELECT 1`);
    _dbAvailable = true;
  } catch (err) {
    console.warn('[DB] PostgreSQL unavailable, using in-memory fallback:', err.message);
    _dbAvailable = false;
    return;
  }

  await _pool.query(`
    CREATE TABLE IF NOT EXISTS telemetry (
      id SERIAL PRIMARY KEY,
      device_id TEXT NOT NULL,
      ts TIMESTAMPTZ DEFAULT now(),
      payload JSONB
    );
  `);

  await _pool.query(`
    CREATE TABLE IF NOT EXISTS alerts (
      id SERIAL PRIMARY KEY,
      device_id TEXT,
      type TEXT,
      severity TEXT,
      message TEXT,
      ts TIMESTAMPTZ DEFAULT now(),
      acknowledged BOOLEAN DEFAULT false
    );
  `);

  await _pool.query(`
    CREATE TABLE IF NOT EXISTS push_tokens (
      id SERIAL PRIMARY KEY,
      token TEXT UNIQUE,
      created_at TIMESTAMPTZ DEFAULT now()
    );
  `);

  await _pool.query(`
    CREATE TABLE IF NOT EXISTS users (
      id SERIAL PRIMARY KEY,
      email TEXT UNIQUE NOT NULL,
      password_hash TEXT NOT NULL,
      role TEXT DEFAULT 'admin',
      created_at TIMESTAMPTZ DEFAULT now()
    );
  `);

  await _pool.query(`
    CREATE TABLE IF NOT EXISTS audit_log (
      id SERIAL PRIMARY KEY,
      ts TIMESTAMPTZ DEFAULT now(),
      source TEXT NOT NULL,
      action TEXT NOT NULL,
      target TEXT,
      value TEXT,
      user_info TEXT,
      ip TEXT,
      details JSONB
    );
  `);

  await _pool.query(`
    CREATE TABLE IF NOT EXISTS device_logs (
      id SERIAL PRIMARY KEY,
      device_id TEXT NOT NULL,
      content TEXT NOT NULL,
      ts TIMESTAMPTZ DEFAULT now()
    );
  `);

  await _pool.query(`CREATE INDEX IF NOT EXISTS idx_device_logs_device_id ON device_logs(device_id);`);
  await _pool.query(`CREATE INDEX IF NOT EXISTS idx_device_logs_ts ON device_logs(ts DESC);`);

  // Run SQL migrations
  await runMigrations();
}

async function runMigrations() {
  await _pool.query(`
    CREATE TABLE IF NOT EXISTS schema_migrations (
      filename TEXT PRIMARY KEY,
      applied_at TIMESTAMPTZ DEFAULT now()
    );
  `);

  const migrationsDir = path.join(__dirname, '..', 'migrations');
  if (!fs.existsSync(migrationsDir)) return;

  const files = fs.readdirSync(migrationsDir)
    .filter(f => f.endsWith('.sql'))
    .sort();

  for (const file of files) {
    const { rows } = await _pool.query('SELECT 1 FROM schema_migrations WHERE filename=$1', [file]);
    if (rows.length > 0) continue;

    console.log(`[DB] Applying migration: ${file}`);
    const sql = fs.readFileSync(path.join(migrationsDir, file), 'utf8');
    await _pool.query(sql);
    await _pool.query('INSERT INTO schema_migrations(filename) VALUES($1)', [file]);
    console.log(`[DB] Migration ${file} applied`);
  }
}

// --- In-memory helper functions (used when PostgreSQL is unavailable) ---

function memInsertTelemetry(deviceId, payload) {
  memStore.telemetry.push({ device_id: deviceId, ts: new Date().toISOString(), payload });
  memStore.devices.add(deviceId);
  // Keep only last 1000 entries per device
  const deviceEntries = memStore.telemetry.filter(t => t.device_id === deviceId);
  if (deviceEntries.length > 1000) {
    const idx = memStore.telemetry.indexOf(deviceEntries[0]);
    if (idx >= 0) memStore.telemetry.splice(idx, 1);
  }
}

function memGetLatestTelemetry(deviceId) {
  const entries = memStore.telemetry.filter(t => t.device_id === deviceId);
  if (entries.length === 0) return null;
  return entries[entries.length - 1];
}

function memGetDevices() {
  return Array.from(memStore.devices).sort();
}

function memInsertDeviceLog(deviceId, content) {
  memStore.deviceLogs.push({ device_id: deviceId, content, ts: new Date().toISOString() });
  memStore.devices.add(deviceId);
  // Keep only latest 5 log entries per device
  const deviceEntries = memStore.deviceLogs.filter(l => l.device_id === deviceId);
  while (deviceEntries.length > 5) {
    const oldest = deviceEntries.shift();
    const idx = memStore.deviceLogs.indexOf(oldest);
    if (idx >= 0) memStore.deviceLogs.splice(idx, 1);
  }
}

function memGetLatestDeviceLog(deviceId) {
  const entries = memStore.deviceLogs.filter(l => l.device_id === deviceId);
  if (entries.length === 0) return null;
  return entries[entries.length - 1];
}

function memInsertAlert(deviceId, type, severity, message) {
  memStore.alerts.push({ device_id: deviceId, type, severity, message, ts: new Date().toISOString() });
}

// Export getters so server.js always gets the current values
function getPool() { return _pool; }
function isDbAvailable() { return _dbAvailable; }

module.exports = {
  getPool,
  isDbAvailable,
  initDb,
  memStore,
  memInsertTelemetry,
  memGetLatestTelemetry,
  memGetDevices,
  memInsertDeviceLog,
  memGetLatestDeviceLog,
  memInsertAlert,
};