/**
 * MQTT over WebSocket service for wqtt.ru broker.
 * v2.0 — Исправлен race condition: подписка на топики выполняется
 * ДО оповещения слушателей. Добавлен топик watersystem/logs/ack для
 * подтверждения получения команды getlogs от устройства.
 *
 * Broker:  m1.wqtt.ru  |  WSS port: 19163
 */

import { MqttWsClient } from './mqttClient';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { loadSetting } from './secureStorage';
import { Telemetry, RawTelemetry, LogEvent, SystemState } from '../types';

// Default MQTT credentials (can be overridden in Settings)
const DEFAULT_BROKER = 'm1.wqtt.ru';
const DEFAULT_PORT   = '19163';
const DEFAULT_USER   = '';
const DEFAULT_PASS   = '';

let BROKER_URL = `wss://${DEFAULT_BROKER}:${DEFAULT_PORT}/mqtt`;
let MQTT_USER  = DEFAULT_USER;
let MQTT_PASS  = DEFAULT_PASS;

/** Load MQTT settings from AsyncStorage/SecureStore (falls back to defaults). */
async function loadMqttSettings(): Promise<void> {
  const broker = await AsyncStorage.getItem('mqttBroker');
  const port   = await AsyncStorage.getItem('mqttPort');
  const user   = await loadSetting('mqttUser');
  const pass   = await loadSetting('mqttPass');
  BROKER_URL = `wss://${broker || DEFAULT_BROKER}:${port || DEFAULT_PORT}/mqtt`;
  MQTT_USER  = user || DEFAULT_USER;
  MQTT_PASS  = pass || DEFAULT_PASS;
}

const TOPIC_TELEMETRY = 'watersystem/telemetry';
const TOPIC_STATUS    = 'watersystem/status';
const TOPIC_LOGS      = 'watersystem/logs';
const TOPIC_LOG_EVENT = 'watersystem/logs/event';
const TOPIC_LOG_ACK   = 'watersystem/logs/ack';
const TOPIC_CMD_BASE  = 'watersystem/cmd';

// Integer state → name (matches SystemState enum in firmware)
const STATE_NAMES: Record<number, string> = {
  [SystemState.IDLE]: 'IDLE',
  [SystemState.FILLING_TANK1]: 'FILLING_TANK1',
  [SystemState.OZONATION]: 'OZONATION',
  [SystemState.AERATION]: 'AERATION',
  [SystemState.SETTLING]: 'SETTLING',
  [SystemState.FILTRATION]: 'FILTRATION',
  [SystemState.BACKWASH]: 'BACKWASH',
};

/** Normalize raw MQTT telemetry to the shape DashboardScreen expects. */
export function normalizeTelemetry(raw: RawTelemetry): Telemetry {
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
  } as Telemetry;
}

type TelemetryCallback   = (data: Telemetry) => void;
type ConnectionCallback  = (connected: boolean) => void;
type LogsCallback        = (logs: string) => void;
type SingleEventCallback = (event: LogEvent) => void;
type LogAckCallback      = () => void;

class MqttService {
  private client: MqttWsClient | null = null;
  private _connected = false;
  private _connecting = false;  // Флаг для предотвращения множественных параллельных connect()
  private telemetryListeners:  TelemetryCallback[]   = [];
  private connListeners:       ConnectionCallback[]   = [];
  private logsListeners:       LogsCallback[]         = [];
  private singleEventListeners: SingleEventCallback[] = [];
  private logAckListeners:     LogAckCallback[]       = [];

  /**
   * Подключиться к MQTT-брокеру через WSS.
   * Идемпотентно: если уже подключены — мгновенный resolve;
   * если подключение в процессе — ждём его завершения.
   * Промис всегда разрешается (успех или неудача) и не зависает.
   */
  connect(): Promise<void> {
    if (this.client && this._connected) {
      return Promise.resolve();
    }
    if (this._connecting) {
      return new Promise<void>((resolve) => {
        const unsub = this.onConnectionChange(() => {
          unsub();
          resolve();
        });
      });
    }
    if (this.client && !this._connected) {
      this.client.disconnect();
      this.client = null;
    }
    this._connecting = true;
    return loadMqttSettings()
      .then(() => new Promise<void>((resolve) => this._startNewConnection(resolve)))
      .catch((err) => {
        this._connecting = false;
        throw err;
      });
  }

  /** Создать новый клиент и запустить подключение (гарантированно резолвится). */
  private _startNewConnection(resolve: () => void): void {
    if (!BROKER_URL) {
      console.warn('[MQTT] Broker not configured — check Settings');
      this._connecting = false;
      resolve();
      return;
    }

    const clientId = `mob_${Math.random().toString(16).slice(2, 10)}`;
    this.client = new MqttWsClient(BROKER_URL, clientId, MQTT_USER, MQTT_PASS, 20);

    // Таймаут-страховка: если соединение не установилось (например, WS-клиент
    // не вызвал onConnChange при полном отказе), всё равно завершаем промис.
    const failTimer = setTimeout(() => {
      if (this._connecting) {
        this._connecting = false;
        console.warn('[MQTT] connect timeout');
        resolve();
      }
    }, 20000);

    this.client.onConnChange((connected) => {
      clearTimeout(failTimer);
      this._connected = connected;
      if (connected) {
        // ВАЖНО: сначала подписываемся на ВСЕ топики, потом оповещаем слушателей —
        // это предотвращает race condition (getlogs до активации подписки).
        this.client!.subscribe([
          TOPIC_TELEMETRY,
          TOPIC_STATUS,
          TOPIC_LOGS,
          TOPIC_LOG_EVENT,
          TOPIC_LOG_ACK,
        ]);
      }
      this._connecting = false;
      this._notifyConn(connected);
      resolve(); // resolve в любом случае (успех или неудача)
    });

    this.client.onMessage((topic, payload) => {
      if (topic === TOPIC_TELEMETRY) {
        try {
          const raw  = JSON.parse(payload) as RawTelemetry;
          const data = normalizeTelemetry(raw);
          this.telemetryListeners.forEach(cb => cb(data));
        } catch (_) {}
      } else if (topic === TOPIC_LOGS) {
        this.logsListeners.forEach(cb => cb(payload));
      } else if (topic === TOPIC_LOG_EVENT) {
        try {
          const event = JSON.parse(payload) as LogEvent;
          this.singleEventListeners.forEach(cb => cb(event));
        } catch (_) {}
      } else if (topic === TOPIC_LOG_ACK) {
        // Устройство подтвердило получение команды getlogs
        this.logAckListeners.forEach(cb => cb());
      }
    });

    this.client.connect();
  }

  /** Subscribe to telemetry updates. Returns unsubscribe fn. */
  onTelemetry(cb: TelemetryCallback): () => void {
    this.telemetryListeners.push(cb);
    return () => { this.telemetryListeners = this.telemetryListeners.filter(l => l !== cb); };
  }

  /** Subscribe to log updates from MQTT. Returns unsubscribe fn. */
  onLogs(cb: LogsCallback): () => void {
    this.logsListeners.push(cb);
    return () => { this.logsListeners = this.logsListeners.filter(l => l !== cb); };
  }

  /** Subscribe to single log events (streaming mode). Returns unsubscribe fn. */
  onSingleEvent(cb: SingleEventCallback): () => void {
    this.singleEventListeners.push(cb);
    return () => { this.singleEventListeners = this.singleEventListeners.filter(l => l !== cb); };
  }

  /** Subscribe to log ack (confirmation that device received getlogs). Returns unsubscribe fn. */
  onLogAck(cb: LogAckCallback): () => void {
    this.logAckListeners.push(cb);
    return () => { this.logAckListeners = this.logAckListeners.filter(l => l !== cb); };
  }

  /** Subscribe to connection state changes. Returns unsubscribe fn. */
  onConnectionChange(cb: ConnectionCallback): () => void {
    this.connListeners.push(cb);
    // Синхронно уведомляем о текущем состоянии.
    // setTimeout(0) создавал асинхронный разрыв: LogsScreen подписывался,
    // но колбэк вызывался в следующем тике, что могло привести к race condition
    // (getlogs отправлялся до того, как подписка на onLogs активировалась).
    // React useState корректно обрабатывает синхронные вызовы setState
    // во время рендера — они ставятся в очередь и применяются после
    // завершения текущего рендера.
    cb(this._connected);
    return () => {
      this.connListeners = this.connListeners.filter(l => l !== cb);
    };
  }

  /**
   * Подписка на лог-топики больше не требуется — подписка происходит
   * автоматически в connect() при установке соединения.
   * Метод оставлен для обратной совместимости, но ничего не делает.
   * @deprecated Подписка на топики происходит автоматически в connect()
   */
  subscribeLogs(): void {
    // Нет операции — подписка выполняется в connect()
  }

  /** Publish a command: e.g. publish('pump1', 'on') */
  publish(target: string, payload: string): void {
    if (!this.client || !this._connected) {
      console.warn('[MQTT] Not connected, cannot publish');
      return;
    }
    this.client.publish(`${TOPIC_CMD_BASE}/${target}`, payload);
  }

  /**
   * Отправить getlogs и дождаться подтверждения от устройства.
   * Возвращает true, если подтверждение получено в течение timeoutMs.
   */
  sendGetlogsWithAck(timeoutMs: number = 15000): Promise<boolean> {
    return new Promise((resolve) => {
      if (!this._connected) {
        console.warn('[MQTT] Cannot send getlogs: not connected');
        resolve(false);
        return;
      }

      const timeout = setTimeout(() => {
        console.warn('[MQTT] getlogs ack timeout');
        resolve(false);
      }, timeoutMs);

      const unsub = this.onLogAck(() => {
        clearTimeout(timeout);
        resolve(true);
      });

      this.publish('getlogs', '1');
    });
  }

  /** Force reconnect with fresh settings (call after saving MQTT credentials). */
  reconnect(): Promise<void> {
    this.disconnect();
    // Небольшая задержка, чтобы старый клиент гарантированно закрылся
    return new Promise((resolve) => {
      setTimeout(() => {
        this.connect().then(resolve).catch(resolve);
      }, 500);
    });
  }

  disconnect(): void {
    this._connecting = false;
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
