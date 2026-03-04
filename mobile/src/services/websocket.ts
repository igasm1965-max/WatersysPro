import { io, Socket } from 'socket.io-client';

class WebSocketService {
  private socket: Socket | null = null;

  connect() {
    if (this.socket) return;
    this.socket = io('http://10.0.0.100:3000'); // <-- set to your backend URL
  }

  onStatusUpdate(cb: (data: any) => void) {
    if (!this.socket) this.connect();
    this.socket!.on('status_update', cb);
    return () => { this.socket!.off('status_update', cb); };
  }

  disconnect() { if (this.socket) { this.socket.disconnect(); this.socket = null; } }
}

export default new WebSocketService();
