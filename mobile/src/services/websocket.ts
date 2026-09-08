import { io, Socket } from 'socket.io-client';
import AsyncStorage from '@react-native-async-storage/async-storage';

// Production builds must use HTTPS/WSS. Real server URL is configured in Settings.
const DEFAULT_SERVER_URL = 'https://your-domain.com';

class WebSocketService {
  private socket: Socket | null = null;

  async connect(): Promise<void> {
    if (this.socket?.connected) return;
    if (this.socket) { this.socket.disconnect(); this.socket = null; }
    const stored = await AsyncStorage.getItem('serverUrl');
    const url = (stored || DEFAULT_SERVER_URL).replace(/\/$/, '');
    this.socket = io(url);
  }

  onStatusUpdate(cb: (data: any) => void) {
    this.socket!.on('status_update', cb);
    return () => { this.socket?.off('status_update', cb); };
  }

  disconnect() { if (this.socket) { this.socket.disconnect(); this.socket = null; } }
}

export default new WebSocketService();
