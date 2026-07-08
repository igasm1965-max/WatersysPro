/**
 * @file AppContext.ts
 * @brief Глобальный контекст приложения (v1.3.6)
 *
 * Изменения в версии 1.3.6:
 * - Добавлены поля deviceIp и setDeviceIp для передачи IP устройства между экранами
 * - IP устанавливается на DashboardScreen из MQTT телеметрии
 * - IP используется на LogsScreen для кнопки "Открыть веб-интерфейс"
 *
 * @see plans/logs_screen_redesign_plan.md
 */

import { createContext, useContext } from 'react';

interface AppContextType {
  lockApp: () => void;
  /** IP устройства (ESP32) в локальной сети */
  deviceIp: string | null;
  /** Установить IP устройства (вызывается из DashboardScreen) */
  setDeviceIp: (ip: string | null) => void;
}

export const AppContext = createContext<AppContextType>({
  lockApp: () => {},
  deviceIp: null,
  setDeviceIp: () => {},
});

export const useAppContext = () => useContext(AppContext);
