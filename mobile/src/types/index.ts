/**
 * @file index.ts
 * @brief Типы данных мобильного приложения WatersysPro
 *
 * Централизованные интерфейсы вместо `any` в экранах и сервисах.
 * Соответствуют форматам телеметрии из прошивки ESP32 (MQTT) и REST-бэкенда.
 */

/** Состояния системы (совпадает с enum SystemState в прошивке). */
export enum SystemState {
  IDLE = 0,
  FILLING_TANK1 = 1,
  OZONATION = 2,
  AERATION = 3,
  SETTLING = 4,
  FILTRATION = 5,
  BACKWASH = 6,
}

/** Состояния реле. Значение 0 = выкл, 1 = вкл. */
export type RelayValue = 0 | 1;

/** Целевые реле, управляемые из приложения. */
export interface RelayState {
  pump1?: RelayValue;
  pump2?: RelayValue;
  aeration?: RelayValue;
  ozone?: RelayValue;
  filter?: RelayValue;
  backwash?: RelayValue;
  [key: string]: RelayValue | undefined;
}

/** Флаги состояния фильтра. */
export interface FlagsState {
  filterCleaningNeeded?: boolean;
  backwashInProgress?: boolean;
  [key: string]: unknown;
}

/** Таймеры этапов и оставшееся время. */
export interface TimersState {
  ozonation?: number;
  aeration?: number;
  settling?: number;
  backwash?: number;
  /** Оставшееся время до необходимости промывки (из MQTT — filter_next). */
  filterRemaining?: number;
  filter_next?: number;
  [key: string]: number | undefined;
}

/** Аварийный режим. */
export interface EmergencyState {
  active: boolean;
  message?: string;
  [key: string]: unknown;
}

/**
 * Телеметрия устройства (нормализованная, см. normalizeTelemetry в mqttService).
 */
export interface Telemetry {
  device_id?: string;
  ip?: string;
  device_ip?: string;
  tank1?: number;
  tank2?: number;
  uptime?: number;
  state: string;
  control_mode: 'manual' | 'auto' | string;
  relays?: RelayState;
  timers?: TimersState;
  flags?: FlagsState;
  emergency: EmergencyState;
  [key: string]: unknown;
}

/** Сырые данные MQTT-телеметрии до нормализации. */
export interface RawTelemetry {
  state?: number | string;
  mode?: string;
  control_mode?: string;
  timers?: Partial<TimersState>;
  emergency?: Partial<EmergencyState>;
  ip?: string;
  device_ip?: string;
  device_id?: string;
  tank1?: number;
  tank2?: number;
  uptime?: number;
  relays?: RelayState;
  flags?: FlagsState;
  [key: string]: unknown;
}

/** Отдельное событие лога (стриминговый режим, топик watersystem/logs/event). */
export interface LogEvent {
  ts: string;
  type: number;
  level: number;
  desc: string;
}

/** Настройки приложения (хранятся в AsyncStorage/SecureStore). */
export interface Settings {
  serverUrl: string;
  adminToken: string;
  notifications: boolean;
  autoFlush: boolean;
  mqttBroker: string;
  mqttPort: string;
  mqttUser: string;
  mqttPass: string;
}

/** Тип подключаемых логов. */
export type LogType = 'events' | 'sd';

/** Ответ REST-бэкенда для логов устройства. */
export interface DeviceLogsResponse {
  deviceId?: string;
  logType?: string;
  content?: string;
  ts?: string;
}

/** Учётные данные авторизации (REST /auth). */
export interface AuthResponse {
  token?: string;
  [key: string]: unknown;
}