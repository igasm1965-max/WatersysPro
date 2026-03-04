import axios from 'axios';

const API_URL = 'http://10.0.0.100:3000/api'; // <-- set to your dev machine IP or backend URL

// Attach JWT token to requests when available
api.interceptors.request.use(async (config) => {
  const token = await (global as any).authToken;
  if (token && config.headers) config.headers.Authorization = `Bearer ${token}`;
  return config;
});
const api = axios.create({ baseURL: API_URL, timeout: 8000 });

export default {
  async getDevices() { const r = await api.get('/devices'); return r.data; },
  async getStatus(deviceId: string) { const r = await api.get(`/status/${deviceId}`); return r.data; },
  async getHistory(deviceId: string, days = 7) { const r = await api.get(`/history/${deviceId}?days=${days}`); return r.data; },
  async getAggregated(deviceId: string, metric = 'pressure', days = 7) { const r = await api.get(`/history-aggregate/${deviceId}?metric=${metric}&days=${days}`); return r.data; },
  async sendCommand(deviceId: string, command: any) { const r = await api.post(`/command/${deviceId}`, command); return r.data; },
  async registerPushToken(token: string) { return api.post('/push/register', { token }); },
  async login(email: string, password: string) { const r = await api.post('/auth/login', { email, password }); return r.data; },
  async register(email: string, password: string) { const r = await api.post('/auth/register', { email, password }); return r.data; }
};
