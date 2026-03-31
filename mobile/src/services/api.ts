import axios from 'axios';
import AsyncStorage from '@react-native-async-storage/async-storage';

const DEFAULT_API_URL = 'http://192.168.0.103:3000/api';

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
  async register(email: string, password: string) { const r = await api.post('/auth/register', { email, password }); return r.data; }
};
