import axios from 'axios';
import AsyncStorage from '@react-native-async-storage/async-storage';

// Production builds must use HTTPS. The real server URL is configured in Settings
// (stored in AsyncStorage 'serverUrl'); this is only a safe default fallback.
const DEFAULT_API_URL = 'https://your-domain.com/api';

const api = axios.create({ baseURL: DEFAULT_API_URL, timeout: 8000 });

// Dynamic server URL from Settings + JWT token
api.interceptors.request.use(async (config) => {
  const serverUrl = await AsyncStorage.getItem('serverUrl');
  if (serverUrl) {
    const base = serverUrl.replace(/\/$/, '') + '/api';
    config.baseURL = base;
  }
  const token = await (global as any).authToken;
  if (token && config.headers) config.headers.Authorization = `Bearer ${token}`;
  return config;
});

export default {
  async getDevices() { const r = await api.get('/devices'); return r.data; },
  async getStatus(deviceId: string) { const r = await api.get(`/status/${deviceId}`); return r.data; },
  async getHistory(deviceId: string, days = 7) { const r = await api.get(`/history/${deviceId}?days=${days}`); return r.data; },
  async getAggregated(deviceId: string, metric = 'pressure', days = 7) { const r = await api.get(`/history-aggregate/${deviceId}?metric=${metric}&days=${days}`); return r.data; },
  async getRemoteConfig() { const r = await api.get('/remote/config'); return r.data; },
  async sendRemoteRelay(target: string, value: 'on' | 'off', adminToken?: string) {
    const headers: any = {};
    if (adminToken) headers['x-admin-token'] = adminToken;
    const r = await api.post('/remote/command', { target, value }, { headers });
    return r.data;
  },
  async sendRemoteMode(mode: 'manual' | 'auto', adminToken?: string) {
    const headers: any = {};
    if (adminToken) headers['x-admin-token'] = adminToken;
    const r = await api.post('/remote/command', { target: 'set_mode', value: mode }, { headers });
    return r.data;
  },
  async sendRemoteReset(adminToken?: string) {
    const headers: any = {};
    if (adminToken) headers['x-admin-token'] = adminToken;
    const r = await api.post('/remote/command', { target: 'reset_emergency', value: '1' }, { headers });
    return r.data;
  },
  async sendCommand(deviceId: string, command: any) { const r = await api.post(`/command/${deviceId}`, command); return r.data; },
  async registerPushToken(token: string) { return api.post('/push/register', { token }); },
  async login(email: string, password: string) { const r = await api.post('/auth/login', { email, password }); return r.data; },
  async register(email: string, password: string) { const r = await api.post('/auth/register', { email, password }); return r.data; },

  /** Get device IP from the latest telemetry stored on the backend */
  async getDeviceIp(deviceId: string): Promise<string | null> {
    try {
      const status = await this.getStatus(deviceId);
      const ip = status?.payload?.ip || status?.payload?.device_ip || null;
      return ip;
    } catch {
      return null;
    }
  },

  /**
   * Fetch logs DIRECTLY from the ESP32 device on the local network.
   * The phone must be on the same WiFi as the device.
   * Falls back to the backend proxy if direct connection fails.
   * Uses axios directly (not the pre-configured `api` instance) to avoid
   * interceptors that would rewrite the URL to the backend server.
   */
  async fetchDeviceLogsDirect(deviceIp: string, type: 'events' | 'sd' = 'events'): Promise<string> {
    let url: string;
    if (type === 'sd') {
      url = `http://${deviceIp}/api/sd_file?name=/events.log`;
    } else {
      url = `http://${deviceIp}/logs.txt`;
    }

    // Diagnostic: log the actual URL being requested
    console.log(`[API] fetchDeviceLogsDirect: ${url}`);

    // Retry up to 2 times on 500/network errors
    const MAX_RETRIES = 2;
    for (let attempt = 0; attempt <= MAX_RETRIES; attempt++) {
      try {
        const response = await axios.get(url, {
          timeout: 15000,
          headers: { 'Accept': 'text/plain, */*' },
          // Don't let axios throw on non-2xx — we handle it ourselves
          validateStatus: () => true,
        });

        if (response.status >= 200 && response.status < 300) {
          return response.data;
        }

        // 500 error — retry if attempts remain
        if (response.status === 500 && attempt < MAX_RETRIES) {
          console.warn(`[API] fetchDeviceLogsDirect got 500, retry ${attempt + 1}/${MAX_RETRIES}`);
          await new Promise(r => setTimeout(r, 1000 * (attempt + 1))); // 1s, 2s backoff
          continue;
        }

        // Include response body in error for better diagnostics
        const body = typeof response.data === 'string' ? response.data : JSON.stringify(response.data);
        const detail = body ? ` (${body.substring(0, 100)})` : '';
        throw new Error(`HTTP ${response.status}: ${response.statusText}${detail}`);
      } catch (err: any) {
        if (err.code === 'ECONNABORTED' || err.message?.includes('timeout')) {
          throw new Error('Таймаут подключения к устройству');
        }
        // On network error, retry if attempts remain
        if (attempt < MAX_RETRIES) {
          console.warn(`[API] fetchDeviceLogsDirect error, retry ${attempt + 1}/${MAX_RETRIES}:`, err.message);
          await new Promise(r => setTimeout(r, 1000 * (attempt + 1)));
          continue;
        }
        throw err;
      }
    }
    throw new Error('Не удалось загрузить логи после нескольких попыток');
  },

  /** Fetch event logs from the device via backend proxy (fallback) */
  async getDeviceLogs(deviceId: string, type: 'events' | 'sd' = 'events') {
    const r = await api.get(`/device-logs/${deviceId}?type=${type}`, { timeout: 15000 });
    // New format: { deviceId, logType, content }
    if (r.data && r.data.content) return r.data.content;
    // Fallback: plain text (old format)
    return r.data;
  },

  /**
   * Fetch logs from the database (published by device via MQTT every 60s).
   * Works from any network — no direct connection to ESP32 needed.
   */
  async getDeviceLogsFromDb(deviceId: string): Promise<string> {
    const r = await api.get(`/device-logs-db/${deviceId}`, { timeout: 10000 });
    // New format: { deviceId, content, ts }
    if (r.data && r.data.content) return r.data.content;
    // Fallback: plain text (old format)
    return r.data;
  }
};
