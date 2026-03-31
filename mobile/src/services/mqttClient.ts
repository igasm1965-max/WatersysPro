/**
 * Minimal MQTT v3.1.1 over WebSocket for React Native / Expo Go.
 * No external dependencies — uses the global WebSocket built into RN.
 *
 * Supported packets: CONNECT, CONNACK, SUBSCRIBE, SUBACK,
 *                    PUBLISH (QoS 0, rx + tx), PINGREQ, PINGRESP, DISCONNECT.
 */

// ─── encoding helpers ──────────────────────────────────────────────────────────

function encodeLength(n: number): number[] {
  const out: number[] = [];
  do {
    let byte = n % 128;
    n = Math.floor(n / 128);
    if (n > 0) byte |= 0x80;
    out.push(byte);
  } while (n > 0);
  return out;
}

function encodeStr(s: string): number[] {
  const bytes = Array.from(unescape(encodeURIComponent(s))).map(c => c.charCodeAt(0));
  return [(bytes.length >> 8) & 0xff, bytes.length & 0xff, ...bytes];
}

function u16(n: number): number[] {
  return [(n >> 8) & 0xff, n & 0xff];
}

// ─── CONNECT packet ────────────────────────────────────────────────────────────

function buildConnect(clientId: string, user: string, pass: string, keepalive = 60): Uint8Array {
  const protocolName = encodeStr('MQTT');
  const level = [0x04];
  // connect flags: clean session=1, username=1, password=1
  const flags = [0b11000010];
  const ka = u16(keepalive);

  const payload = [
    ...encodeStr(clientId),
    ...encodeStr(user),
    ...encodeStr(pass),
  ];

  const varHeader = [...protocolName, ...level, ...flags, ...ka];
  const remaining = [...varHeader, ...payload];

  return new Uint8Array([0x10, ...encodeLength(remaining.length), ...remaining]);
}

// ─── SUBSCRIBE packet ──────────────────────────────────────────────────────────

function buildSubscribe(topics: string[], packetId = 1): Uint8Array {
  const topicBytes: number[] = [];
  for (const t of topics) {
    topicBytes.push(...encodeStr(t), 0x00); // QoS 0
  }
  const varHeader = u16(packetId);
  const remaining = [...varHeader, ...topicBytes];
  return new Uint8Array([0x82, ...encodeLength(remaining.length), ...remaining]);
}

// ─── PUBLISH packet (QoS 0) ────────────────────────────────────────────────────

function buildPublish(topic: string, payload: string): Uint8Array {
  const msg = Array.from(unescape(encodeURIComponent(payload))).map(c => c.charCodeAt(0));
  const topicBytes = encodeStr(topic);
  const remaining = [...topicBytes, ...msg];
  return new Uint8Array([0x30, ...encodeLength(remaining.length), ...remaining]);
}

// ─── PINGREQ ───────────────────────────────────────────────────────────────────

const PINGREQ = new Uint8Array([0xc0, 0x00]);
const DISCONNECT_PKT = new Uint8Array([0xe0, 0x00]);

// ─── decode remaining length ───────────────────────────────────────────────────

function decodeLength(buf: Uint8Array, offset: number): { value: number; bytesRead: number } {
  let multiplier = 1;
  let value = 0;
  let bytesRead = 0;
  for (let i = 0; i < 4; i++) {
    const byte = buf[offset + i];
    value += (byte & 0x7f) * multiplier;
    multiplier *= 128;
    bytesRead++;
    if ((byte & 0x80) === 0) break;
  }
  return { value, bytesRead };
}

// ─── parse a UTF-8 string at offset (2-byte length prefix) ────────────────────

function parseStr(buf: Uint8Array, offset: number): { str: string; end: number } {
  const len = (buf[offset] << 8) | buf[offset + 1];
  const bytes = buf.slice(offset + 2, offset + 2 + len);
  const str = decodeURIComponent(escape(String.fromCharCode(...bytes)));
  return { str, end: offset + 2 + len };
}

// ─── MqttWsClient ──────────────────────────────────────────────────────────────

export type MessageHandler = (topic: string, payload: string) => void;
export type ConnHandler    = (connected: boolean) => void;

export class MqttWsClient {
  private ws: WebSocket | null = null;
  private pingTimer: ReturnType<typeof setInterval> | null = null;
  private _connected = false;

  private msgHandlers:  MessageHandler[] = [];
  private connHandlers: ConnHandler[]    = [];

  private readonly url:      string;
  private readonly clientId: string;
  private readonly user:     string;
  private readonly pass:     string;
  private readonly keepalive: number;

  private reconnTimer: ReturnType<typeof setTimeout> | null = null;
  private destroyed = false;

  constructor(url: string, clientId: string, user: string, pass: string, keepalive = 60) {
    this.url       = url;
    this.clientId  = clientId;
    this.user      = user;
    this.pass      = pass;
    this.keepalive = keepalive;
  }

  connect(): void {
    if (this.destroyed) return;
    try {
      this.ws = new WebSocket(this.url, ['mqtt']);
      this.ws.binaryType = 'arraybuffer';

      this.ws.onopen = () => {
        console.log('[MqttWs] WS open, sending CONNECT');
        this._send(buildConnect(this.clientId, this.user, this.pass, this.keepalive));
      };

      this.ws.onmessage = (ev) => {
        try {
          let buf: Uint8Array;
          const data = ev.data;
          if (data instanceof ArrayBuffer) {
            // Standard path (desktop browsers, some RN builds)
            buf = new Uint8Array(data);
          } else if (typeof data === 'string') {
            // React Native / Expo Go delivers binary WS frames as base64 strings
            const binary = atob(data);
            buf = new Uint8Array(binary.length);
            for (let i = 0; i < binary.length; i++) buf[i] = binary.charCodeAt(i);
          } else {
            console.warn('[MqttWs] unexpected data type:', typeof data, data);
            return;
          }
          this._handlePacket(buf);
        } catch (e) {
          console.warn('[MqttWs] onmessage parse error', e);
        }
      };

      this.ws.onerror = (e) => {
        console.warn('[MqttWs] error', e);
        this._setConnected(false);
      };

      this.ws.onclose = () => {
        this._setConnected(false);
        this._stopPing();
        if (!this.destroyed) {
          this.reconnTimer = setTimeout(() => this.connect(), 5000);
        }
      };
    } catch (e) {
      console.warn('[MqttWs] connect exception', e);
      if (!this.destroyed) {
        this.reconnTimer = setTimeout(() => this.connect(), 5000);
      }
    }
  }

  subscribe(topics: string[]): void {
    if (this._connected) {
      this._send(buildSubscribe(topics));
    }
  }

  publish(topic: string, payload: string): void {
    if (this._connected) {
      this._send(buildPublish(topic, payload));
    } else {
      console.warn('[MqttWs] not connected, cannot publish');
    }
  }

  onMessage(h: MessageHandler): () => void {
    this.msgHandlers.push(h);
    return () => { this.msgHandlers = this.msgHandlers.filter(x => x !== h); };
  }

  onConnChange(h: ConnHandler): () => void {
    this.connHandlers.push(h);
    return () => { this.connHandlers = this.connHandlers.filter(x => x !== h); };
  }

  get connected(): boolean { return this._connected; }

  disconnect(): void {
    this.destroyed = true;
    this._stopPing();
    if (this.reconnTimer) clearTimeout(this.reconnTimer);
    if (this.ws) {
      try { this._send(DISCONNECT_PKT); } catch (_) {}
      try { this.ws.close(); } catch (_) {}
      this.ws = null;
    }
  }

  // ─── private ──────────────────────────────────────────────────────────────

  private _send(data: Uint8Array): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(data.buffer as ArrayBuffer);
    }
  }

  private _setConnected(v: boolean): void {
    if (this._connected !== v) {
      this._connected = v;
      console.log('[MqttWs] connected =', v);
      this.connHandlers.forEach(h => h(v));
    }
  }

  private _handlePacket(buf: Uint8Array): void {
    if (buf.length < 2) return;
    const type = (buf[0] & 0xf0) >> 4;
    console.log('[MqttWs] pkt type=', type, 'len=', buf.length, 'bytes=', buf[0], buf[1], buf[2], buf[3]);

    switch (type) {
      case 2: // CONNACK
        if (buf[3] === 0x00) {
          this._setConnected(true);
          this._startPing();
        } else {
          console.warn('[MqttWs] CONNACK error code:', buf[3]);
        }
        break;

      case 3: { // PUBLISH
        const dup    = (buf[0] & 0x08) !== 0;
        const qos    = (buf[0] & 0x06) >> 1;
        const lenInfo = decodeLength(buf, 1);
        let offset = 1 + lenInfo.bytesRead;
        const { str: topic, end } = parseStr(buf, offset);
        offset = end;
        if (qos > 0) offset += 2; // packet id
        const payloadBytes = buf.slice(offset, 1 + lenInfo.bytesRead + lenInfo.value);
        const payload = decodeURIComponent(escape(String.fromCharCode(...payloadBytes)));
        this.msgHandlers.forEach(h => h(topic, payload));
        void dup; // suppress unused warning
        break;
      }

      case 13: // PINGRESP
        break;

      default:
        break;
    }
  }

  private _startPing(): void {
    this._stopPing();
    this.pingTimer = setInterval(() => {
      if (this._connected) this._send(PINGREQ);
    }, (this.keepalive * 1000) / 2);
  }

  private _stopPing(): void {
    if (this.pingTimer) {
      clearInterval(this.pingTimer);
      this.pingTimer = null;
    }
  }
}
