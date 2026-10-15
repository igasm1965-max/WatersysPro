/**
 * @file App.tsx
 * @brief Корневой компонент приложения (v1.3.6)
 *
 * Изменения в версии 1.3.6:
 * - Добавлено состояние deviceIp для передачи IP устройства между экранами
 * - IP передаётся через AppContext: DashboardScreen устанавливает, LogsScreen читает
 *
 * @see plans/logs_screen_redesign_plan.md
 */

import React, { useCallback, useEffect, useState } from 'react';
import { NavigationContainer, createNavigationContainerRef } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';
import * as SecureStore from 'expo-secure-store';
// Push-уведомления (добавлено сегодня): глобальный обработчик уведомлений,
// подписка на FCM через Expo Notifications и навигация по нажатию.
import * as Notifications from 'expo-notifications';

import DashboardScreen from './src/screens/DashboardScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import LogsScreen from './src/screens/LogsScreen';
import PinScreen, { PIN_KEY } from './src/screens/PinScreen';
import { AppContext } from './src/context/AppContext';
import MqttService from './src/services/mqttService';

const Tab = createBottomTabNavigator();

/**
 * Глобальный обработчик уведомлений (Expo Notifications).
 * Позволяет показывать push-уведомления поверх экрана даже когда приложение
 * открыто (foreground). Без этого при активном приложении уведомления не видны.
 */
Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowBanner: true,
    shouldShowList: true,
    shouldPlaySound: false,
    shouldSetBadge: false,
  }),
});

// Ref для навигации из обработчика нажатия на push-уведомление.
// Создан сегодня вместе с функционалом push-уведомлений, чтобы из
// фонового обработчика переходить на нужный экран приложения.
const navigationRef = createNavigationContainerRef();

export default function App() {
  // null = loading, 'create' = no pin yet, 'unlock' = pin exists, 'open' = authenticated,
  // 'change' = смена PIN (требует ввода старого кода)
  const [pinState, setPinState] = useState<'loading' | 'create' | 'unlock' | 'change' | 'open'>('loading');
  // IP устройства (ESP32) — устанавливается DashboardScreen из MQTT телеметрии
  const [deviceIp, setDeviceIp] = useState<string | null>(null);

  useEffect(() => {
    // PIN хранится в SecureStore (не в открытом AsyncStorage)
    SecureStore.getItemAsync(PIN_KEY).then(stored => {
      setPinState(stored ? 'unlock' : 'create');
    });
  }, []);

  const handlePinSuccess = useCallback(() => setPinState('open'), []);

  const lockApp = useCallback(() => {
    // Смена PIN: старый код не удаляется, требуется его ввод для подтверждения
    setPinState('change');
  }, []);

  // Централизованное подключение к MQTT: один раз после аутентификации.
  // Экраны только подписываются на события и отписываются.
  useEffect(() => {
    if (pinState === 'open') {
      MqttService.connect().catch(() => {});
    }
  }, [pinState]);

  // Обработка нажатия на push-уведомление (добавлено сегодня):
  // переходим в раздел «Логи» (или «Главная», если в payload указан
  // data.screen === 'dashboard'). Подписка снимается при размонтировании.
  useEffect(() => {
    const sub = Notifications.addNotificationResponseReceivedListener((response) => {
      const data = response.notification.request.content.data || {};
      const target = data.screen === 'dashboard' ? 'Главная' : 'Логи';
      if (navigationRef.isReady()) {
        navigationRef.navigate(target as never);
      }
    });
    return () => sub.remove();
  }, []);

  if (pinState === 'loading') return null;

  if (pinState === 'create' || pinState === 'unlock' || pinState === 'change') {
    return <PinScreen mode={pinState} onSuccess={handlePinSuccess} />;
  }

  return (
    <AppContext.Provider value={{ lockApp, deviceIp, setDeviceIp }}>
      {/* ref подключён для программной навигации из push-уведомлений (сегодня) */}
      <NavigationContainer ref={navigationRef}>
        <Tab.Navigator screenOptions={({ route }) => ({
          tabBarIcon: ({ focused, color, size }) => {
            const icons: Record<string, [string, string]> = {
              'Главная': ['home', 'home-outline'],
              'Настройки': ['settings', 'settings-outline'],
              'Логи': ['document-text', 'document-text-outline'],
            };
            const pair = icons[route.name] ?? ['help-circle', 'help-circle-outline'];
            const iconName = focused ? pair[0] : pair[1];
            return <Ionicons name={iconName as any} size={size} color={color} />;
          },
          tabBarActiveTintColor: '#2196F3',
          tabBarInactiveTintColor: 'gray'
        })}>
          <Tab.Screen name="Главная" component={DashboardScreen} />
          <Tab.Screen name="Логи" component={LogsScreen} />
          <Tab.Screen name="Настройки" component={SettingsScreen} />
        </Tab.Navigator>
      </NavigationContainer>
    </AppContext.Provider>
  );
}
