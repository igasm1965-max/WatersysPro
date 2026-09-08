/**
 * @file notificationService.js
 * @brief Отправка уведомлений об аварийных ситуациях.
 *
 * Каналы доставки:
 *  - Telegram (основной) — официальный Bot API через long-polling / прямой вызов sendMessage.
 *    Бот сам инициирует исходящие HTTPS-запросы, поэтому работает и в облаке,
 *    и на локальной машине без публичного URL (не требуется webhook).
 *  - SMS (резервный) — исходящий HTTP-запрос к SMS-шлюзу (SMS Aero или Twilio).
 *
 * Использует только встроенный модуль Node.js `https` — без дополнительных зависимостей.
 */

const https = require('https');
const http = require('http');

// ─── конфигурация ────────────────────────────────────────────────────────────
const TG_BOT_TOKEN = process.env.TG_BOT_TOKEN || '';
const TG_CHAT_ID = process.env.TG_CHAT_ID || '';

const SMS_PROVIDER = (process.env.SMS_PROVIDER || 'none').toLowerCase(); // none | smsaero | twilio
const SMS_PHONE = process.env.SMS_PHONE || '';
const SMS_FROM = process.env.SMS_FROM || 'WATERSYS';

// SMS Aero
const SMSAERO_EMAIL = process.env.SMSAERO_EMAIL || '';
const SMSAERO_API_KEY = process.env.SMSAERO_API_KEY || '';
// Twilio
const TWILIO_ACCOUNT_SID = process.env.TWILIO_ACCOUNT_SID || '';
const TWILIO_AUTH_TOKEN = process.env.TWILIO_AUTH_TOKEN || '';
const TWILIO_FROM = process.env.TWILIO_FROM || '';

// MAX (WQTT MQTT-мост через REST API dash.wqtt.ru)
const WQTT_TOKEN = process.env.WQTT_TOKEN || '';
const WQTT_TOPIC = process.env.WQTT_TOPIC || '';
const WQTT_HOST = process.env.WQTT_HOST || 'dash.wqtt.ru';
// Опционально: несколько топиков через запятую (например, для чата и устройства)
const WQTT_TOPICS = (process.env.WQTT_TOPICS || WQTT_TOPIC).split(',').map(s => s.trim()).filter(Boolean);

// Эскалация: повторное уведомление, если авария не сброшена
const NOTIFY_REPEAT_MIN = Number(process.env.NOTIFY_REPEAT_MIN || 10); // минут
const NOTIFY_REPEAT_MAX = Number(process.env.NOTIFY_REPEAT_MAX || 5);   // повторений

// ─── HTTPS-помощники ─────────────────────────────────────────────────────────

/**
 * Выполнить HTTPS POST-запрос с JSON-телом и вернуть распарсенный ответ.
 * @param {string} hostname
 * @param {string} path
 * @param {object} body
 * @param {object} [headers]
 * @returns {Promise<object>}
 */
function httpsPostJson(hostname, path, body, headers = {}) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(body);
    const req = https.request(
      {
        hostname,
        path,
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(payload),
          ...headers,
        },
      },
      (res) => {
        let data = '';
        res.on('data', (chunk) => { data += chunk; });
        res.on('end', () => {
          let parsed = null;
          try { parsed = data ? JSON.parse(data) : null; } catch (_) { parsed = null; }
          if (res.statusCode < 200 || res.statusCode >= 300) {
            return reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 300)}`));
          }
          resolve(parsed);
        });
      }
    );
    req.on('error', reject);
    req.setTimeout(10000, () => { req.destroy(new Error('Request timeout')); });
    req.write(payload);
    req.end();
  });
}

/**
 * Выполнить HTTPS POST с application/x-www-form-urlencoded телом.
 * @param {string} hostname
 * @param {string} path
 * @param {string} urlEncodedBody
 * @param {object} [headers]
 * @returns {Promise<object>}
 */
function httpsPostForm(hostname, path, urlEncodedBody, headers = {}) {
  return new Promise((resolve, reject) => {
    const req = https.request(
      {
        hostname,
        path,
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          'Content-Length': Buffer.byteLength(urlEncodedBody),
          ...headers,
        },
      },
      (res) => {
        let data = '';
        res.on('data', (chunk) => { data += chunk; });
        res.on('end', () => {
          let parsed = null;
          try { parsed = data ? JSON.parse(data) : null; } catch (_) { parsed = null; }
          if (res.statusCode < 200 || res.statusCode >= 300) {
            return reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 300)}`));
          }
          resolve(parsed);
        });
      }
    );
    req.on('error', reject);
    req.setTimeout(10000, () => { req.destroy(new Error('Request timeout')); });
    req.write(urlEncodedBody);
    req.end();
  });
}

// ─── Telegram ────────────────────────────────────────────────────────────────

/**
 * Проверить валидность токена бота (запрос getMe).
 * @returns {Promise<{ok: boolean, username?: string, error?: string}>}
 */
async function checkTelegram() {
  if (!TG_BOT_TOKEN) return { ok: false, error: 'TG_BOT_TOKEN не задан' };
  try {
    const res = await httpsPostJson('api.telegram.org', `/bot${TG_BOT_TOKEN}/getMe`, {});
    if (res && res.ok && res.result) {
      return { ok: true, username: res.result.username, chatIdConfigured: !!TG_CHAT_ID };
    }
    return { ok: false, error: (res && res.description) || 'getMe failed' };
  } catch (err) {
    return { ok: false, error: err.message };
  }
}

/**
 * Автоматически определить chatId первого пользователя, написавшего боту.
 * @returns {Promise<string>}
 */
async function resolveChatId() {
  if (TG_CHAT_ID) return TG_CHAT_ID;
  try {
    const res = await httpsPostJson('api.telegram.org', `/bot${TG_BOT_TOKEN}/getUpdates`, {});
    if (!res || !res.ok || !Array.isArray(res.result)) return '';
    for (const upd of res.result) {
      const msg = upd.message || upd.channel_post;
      const chat = msg && (msg.chat || (msg.sender_chat && { id: msg.sender_chat.id }));
      if (chat && chat.id) return String(chat.id);
    }
  } catch (err) {
    console.warn('[Notify] resolveChatId error:', err.message);
  }
  return '';
}

/**
 * Отправить текстовое сообщение в Telegram.
 * @param {string} text
 * @param {object} [options] { reply_markup }
 * @returns {Promise<boolean>}
 */
async function sendTelegram(text, options = {}) {
  if (!TG_BOT_TOKEN) return false;
  const chatId = await resolveChatId();
  if (!chatId) return false;
  const body = {
    chat_id: chatId,
    text,
    parse_mode: 'HTML',
    disable_web_page_preview: true,
    ...options,
  };
  try {
    const res = await httpsPostJson('api.telegram.org', `/bot${TG_BOT_TOKEN}/sendMessage`, body);
    return !!(res && res.ok);
  } catch (err) {
    console.error('[Notify] Telegram send error:', err.message);
    return false;
  }
}

// ─── SMS ─────────────────────────────────────────────────────────────────────

/**
 * Отправить SMS через выбранного провайдера.
 * @param {string} text
 * @returns {Promise<boolean>}
 */
async function sendSms(text) {
  if (SMS_PROVIDER === 'none') return false;
  if (!SMS_PHONE) {
    console.warn('[Notify] SMS: SMS_PHONE не задан');
    return false;
  }
  if (SMS_PROVIDER === 'smsaero') {
    return sendSmsAero(text);
  }
  if (SMS_PROVIDER === 'twilio') {
    return sendTwilio(text);
  }
  console.warn('[Notify] SMS: неизвестный провайдер:', SMS_PROVIDER);
  return false;
}

/** SMS Aero (https://smsaero.ru/api) */
async function sendSmsAero(text) {
  if (!SMSAERO_EMAIL || !SMSAERO_API_KEY) {
    console.warn('[Notify] SMSAero: email/api key не заданы');
    return false;
  }
  const auth = Buffer.from(`${SMSAERO_EMAIL}:${SMSAERO_API_KEY}`).toString('base64');
  try {
    const body = { number: SMS_PHONE, text, sign: SMS_FROM };
    const res = await httpsPostJson('gate.smsaero.ru', '/v2/sms/send', body, {
      Authorization: `Basic ${auth}`,
      Accept: 'application/json',
    });
    return !!(res && res.success);
  } catch (err) {
    console.error('[Notify] SMSAero send error:', err.message);
    return false;
  }
}

/** Twilio */
async function sendTwilio(text) {
  if (!TWILIO_ACCOUNT_SID || !TWILIO_AUTH_TOKEN || !TWILIO_FROM) {
    console.warn('[Notify] Twilio: accountSid/authToken/from не заданы');
    return false;
  }
  const auth = Buffer.from(`${TWILIO_ACCOUNT_SID}:${TWILIO_AUTH_TOKEN}`).toString('base64');
  const body = new URLSearchParams({
    To: SMS_PHONE,
    From: TWILIO_FROM,
    Body: text,
  }).toString();
  try {
    const res = await httpsPostForm(
      'api.twilio.com',
      `/2010-04-01/Accounts/${TWILIO_ACCOUNT_SID}/Messages.json`,
      body,
      { Authorization: `Basic ${auth}` }
    );
    return !!(res && res.sid);
  } catch (err) {
    console.error('[Notify] Twilio send error:', err.message);
    return false;
  }
}

// ─── MAX (WQTT MQTT-мост через REST API dash.wqtt.ru) ───────────────────────

/**
 * Проверить валидность токена WQTT и наличие настроенных топиков.
 * @returns {Promise<{ok: boolean, error?: string}>}
 */
async function checkMax() {
  if (!WQTT_TOKEN) return { ok: false, error: 'WQTT_TOKEN не задан' };
  if (WQTT_TOPICS.length === 0) return { ok: false, error: 'WQTT_TOPIC не задан' };
  try {
    // Проверяем доступность API и токен через запрос информации о брокере
    const res = await httpsPostJson(
      WQTT_HOST,
      '/api/broker',
      {},
      { Authorization: `Token ${WQTT_TOKEN}` }
    );
    if (res && res.server) {
      return { ok: true };
    }
    return { ok: false, error: 'Не удалось подключиться к WQTT API' };
  } catch (err) {
    return { ok: false, error: err.message };
  }
}

/**
 * Отправить текстовое сообщение в MAX через WQTT MQTT-мост.
 * Публикует сообщение в настроенные топики через POST /api/broker/messages/pub.
 * @param {string} text
 * @returns {Promise<boolean>}
 */
async function sendMax(text) {
  if (!WQTT_TOKEN) return false;
  if (WQTT_TOPICS.length === 0) {
    console.warn('[Notify] MAX: WQTT_TOPIC не задан');
    return false;
  }
  // Формируем массив сообщений для публикации (по одному на каждый топик)
  const messages = WQTT_TOPICS.map(topic => ({ topic, payload: text, retain: false }));
  try {
    const res = await httpsPostJson(
      WQTT_HOST,
      '/api/broker/messages/pub',
      messages,
      { Authorization: `Token ${WQTT_TOKEN}` }
    );
    return !!(res && res.result === 'ok');
  } catch (err) {
    console.error('[Notify] MAX (WQTT) send error:', err.message);
    return false;
  }
}

// ─── эскалация / дедупликация ───────────────────────────────────────────────

// Ключ: deviceId -> { firstTs, lastNotifyTs, count, active }
const emergencyState = new Map();

/**
 * Обработать смену состояния аварии устройства.
 * При rising-фронте отправляет уведомление сразу (Telegram + SMS).
 * При длительной невыключенной аварии повторяет уведомление с интервалом NOTIFY_REPEAT_MIN.
 *
 * @param {string} deviceId
 * @param {boolean} active
 * @param {string} message
 * @param {string} [severity]
 * @returns {Promise<{sentTelegram: boolean, sentSms: boolean, escalated: boolean}>}
 */
async function notifyEmergency(deviceId, active, message, severity = 'critical') {
  const now = Date.now();
  const state = emergencyState.get(deviceId) || { firstTs: now, lastNotifyTs: 0, count: 0, active: false };

  // Сброс состояния, если авария прекратилась
  if (!active) {
    if (state.active) {
      console.log(`[Notify] Авария сброшена для ${deviceId}`);
    }
    emergencyState.delete(deviceId);
    return { sentTelegram: false, sentSms: false, escalated: false };
  }

  const msg = String(message || 'Аварийный режим');
  const isFirst = !state.active;              // первый rising-фронт
  const intervalMs = NOTIFY_REPEAT_MIN * 60 * 1000;
  const isEscalation =
    state.active &&
    state.lastNotifyTs > 0 &&
    now - state.lastNotifyTs >= intervalMs &&
    state.count < NOTIFY_REPEAT_MAX;

  if (!isFirst && !isEscalation) {
    return { sentTelegram: false, sentSms: false, escalated: false };
  }

  state.active = true;
  state.lastNotifyTs = now;
  state.count = isFirst ? 1 : state.count + 1;
  emergencyState.set(deviceId, state);

  const escalationNote = isEscalation
    ? `\n⚠️ Повторное уведомление (${state.count}/${NOTIFY_REPEAT_MAX}). Авария не устранена за ${NOTIFY_REPEAT_MIN} мин.`
    : '';
  const ts = new Date().toLocaleString('ru-RU');

  const tgText =
    `🚨 <b>АВАРИЯ</b> на устройстве <b>${deviceId}</b>\n` +
    `Сообщение: <code>${msg}</code>\n` +
    `Критичность: ${severity}\n` +
    `Время: ${ts}` +
    escalationNote +
    `\n\nОткройте приложение WatersysPro для просмотра состояния и действий.`;

  // Основной канал — Telegram, резервные — MAX и SMS (на первый сброс и на эскалацию)
  const sentTelegram = await sendTelegram(tgText);
  // Для MAX отправляем текст без HTML-разметки
  const plainText = tgText.replace(/<[^>]+>/g, '').replace(/&/g, '&').replace(/</g, '<').replace(/>/g, '>');
  const sentMax = await sendMax(plainText);
  let sentSms = false;
  if (isFirst || isEscalation) {
    const smsText = `WATERSYS: АВАРИЯ ${deviceId}: ${msg}. Подробнее в приложении.`;
    sentSms = await sendSms(smsText);
  }

  if (!sentTelegram && !sentMax && !sentSms) {
    console.warn(`[Notify] Уведомление о ${deviceId} не отправлено ни одним каналом (каналы не настроены?)`);
  } else {
    console.log(`[Notify] ${deviceId}: telegram=${sentTelegram}, max=${sentMax}, sms=${sentSms}, escalation=${isEscalation}`);
  }

  return { sentTelegram, sentMax, sentSms, escalated: isEscalation };
}

/**
 * Тестовая отправка по обоим каналам.
 * @returns {Promise<{telegram: object, sms: object}>}
 */
async function testChannels() {
  const tgCheck = await checkTelegram();
  const telegramResult = { configured: tgCheck.ok, username: tgCheck.username, ...tgCheck };
  if (tgCheck.ok) {
    const chatId = await resolveChatId();
    telegramResult.chatId = chatId || null;
    telegramResult.sent = chatId ? await sendTelegram('🧪 Тестовое сообщение от WatersysPro. Канал Telegram настроен корректно.') : false;
    telegramResult.issue = chatId ? null : 'Не определён chatId. Напишите боту любое сообщение и повторите тест.';
  }

  const smsConfigured = SMS_PROVIDER !== 'none' && !!SMS_PHONE;
  const smsResult = { configured: smsConfigured, provider: SMS_PROVIDER, sent: false };
  if (smsConfigured) {
    smsResult.sent = await sendSms('WATERSYS: тестовое SMS-оповещение.');
  }

  const maxConfigured = !!WQTT_TOKEN && WQTT_TOPICS.length > 0;
  const maxResult = {
    configured: maxConfigured,
    tokenOk: false,
    topics: WQTT_TOPICS,
    sent: false,
  };
  if (maxConfigured) {
    const check = await checkMax();
    maxResult.tokenOk = check.ok;
    if (check.ok) {
      maxResult.sent = await sendMax('🧪 Тестовое сообщение от WatersysPro. Канал MAX настроен корректно.');
      maxResult.issue = maxResult.sent ? null : 'Публикация не подтверждена сервисом WQTT. Проверьте топик.';
    } else {
      maxResult.issue = check.error;
    }
  }

  return { telegram: telegramResult, sms: smsResult, max: maxResult };
}

/**
 * Статус конфигурации каналов уведомлений.
 * @returns {Promise<object>}
 */
async function getStatus() {
  const tg = await checkTelegram();
  const max = await checkMax();
  return {
    telegram: {
      configured: !!TG_BOT_TOKEN,
      tokenOk: tg.ok,
      username: tg.username || null,
      chatId: TG_CHAT_ID || null,
      autoResolve: !TG_CHAT_ID && !!TG_BOT_TOKEN,
    },
    max: {
      configured: !!WQTT_TOKEN && WQTT_TOPICS.length > 0,
      tokenOk: max.ok,
      error: max.ok ? null : (max.error || null),
      topics: WQTT_TOPICS,
    },
    sms: {
      provider: SMS_PROVIDER,
      configured: SMS_PROVIDER !== 'none' && !!SMS_PHONE,
      phone: SMS_PHONE ? (SMS_PHONE.slice(0, 3) + '****' + SMS_PHONE.slice(-2)) : null,
    },
    escalation: {
      repeatMin: NOTIFY_REPEAT_MIN,
      repeatMax: NOTIFY_REPEAT_MAX,
    },
  };
}

module.exports = {
  notifyEmergency,
  sendTelegram,
  sendSms,
  sendMax,
  testChannels,
  getStatus,
  checkTelegram,
  checkMax,
  resolveChatId,
};