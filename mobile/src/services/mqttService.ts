/**
 * MQTT over WebSocket service for wqtt.ru broker.
 * Uses a pure-JS MQTT client (no native modules) — works in Expo Go.
 *
 * Broker:  m1.wqtt.ru  |  WSS port: 19163
 */

import { MqttWsClient } from './mqttClient';

const BROKER_URL = 'wss://m1.wqtt.ru:19163/mqtt';
const MQTT_USER  = 'REDACTED_MQTT_USER';
const MQTT_PASS  = 'REDACTED_MQTT_PASS';

const TOPIC_TELEMETRY = 'watersystem/telemetry';
const TOPIC_STATUS    = 'watersystem/status';
const TOPIC_CMD_BASE  = 'watersystem/cmd';

// Integer state → name (matches SystemState enum in firmware)
const STATE_NAMES: Record<number, string> = {
  0: 'IDLE',
  1: 'FILLING_TANK1',
  2: 'OZONATION',
  3: 'AERATION',
  4: 'SETTLING',
  5: 'FILTRATION',
  6: 'BACKWASH',
};

/** Normalize raw MQTT telemetry to the shape DashboardScreen expects. */
export function normalizeTelemetry(raw: any): any {
  const stateNum = typeof raw.state === 'number' ? raw.state : -1;
  const stateName = STATE_NAMES[stateNum] ?? String(raw.state ?? '');
  return {
    ...raw,
    state: stateName,
    // REST API uses control_mode, MQTT telemetry uses mode
    control_mode: raw.mode ?? raw.control_mode ?? 'auto',
    timers: {
      ...(raw.timers ?? {}),
      // REST API used filterRemaining, MQTT telemetry uses filter_next
      filterRemaining: raw.timers?.filter_next ?? raw.timers?.filterRemaining ?? 0,
    },
    emergency: raw.emergency ?? { active: false },
  };
}

type TelemetryCallback  = (data: any) => void;
type ConnectionCallback = (connected: boolean) => void;

class MqttService {
  private client: MqttWsClient | null = null;
  private _connected = false;
  private telemetryListeners: TelemetryCallback[]  = [];
  private connListeners:      ConnectionCallback[]  = [];

  /** Connect to wqtt.ru via WSS. Resolves when CONNACK received. */
  connect(): Promise<void> {
    if (this.client && this._connected) return Promise.resolve();
    // If previous client exists but lost connection — stop its reconnect loop first
    if (this.client && !this._connected) {
      this.client.disconnect();
      this.client = null;
    }

    return new Promise((resolve) => {
      const clientId = `mob_${Math.random().toString(16).slice(2, 10)}`;

      this.client = new MqttWsClient(
        BROKER_URL, clientId, MQTT_USER, MQTT_PASS, 60
      );

      this.client.onConnChange((connected) => {
        this._connected = connected;
        this._notifyConn(connected);
        if (connected) {
          this.client!.subscribe([TOPIC_TELEMETRY, TOPIC_STATUS]);
          resolve();
        }
      });

      this.client.onMessage((topic, payload) => {
        if (topic === TOPIC_TELEMETRY) {
          try {
            const raw  = JSON.parse(payload);
            const data = normalizeTelemetry(raw);
            this.telemetryListeners.forEach(cb => cb(data));
          } catch (_) {}
        }
      });

      this.client.connect();
    });
  }

  /** Subscribe to telemetry updates. Returns unsubscribe fn. */
  onTelemetry(cb: TelemetryCallback): () => void {
    this.telemetryListeners.push(cb);
    return () => { this.telemetryListeners = this.telemetryListeners.filter(l => l !== cb); };
  }

  /** Subscribe to connection state changes. Returns unsubscribe fn. */
  onConnectionChange(cb: ConnectionCallback): () => void {
    this.connListeners.push(cb);
    return () => { this.connListeners = this.connListeners.filter(l => l !== cb); };
  }

  /** Publish a command: e.g. publish('pump1', 'on') */
  publish(target: string, payload: string): void {
    if (!this.client || !this._connected) {
      console.warn('[MQTT] Not connected, cannot publish');
      return;
    }
    this.client.publish(`${TOPIC_CMD_BASE}/${target}`, payload);
  }

  disconnect(): void {
    if (this.client) {
      this.client.disconnect();
      this.client = null;
      this._connected = false;
    }
  }

  get connected(): boolean { return this._connected; }

  private _notifyConn(state: boolean) {
    this.connListeners.forEach(cb => cb(state));
  }
}

export default new MqttService();
