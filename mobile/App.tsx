import React, { useCallback, useEffect, useState } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';
import AsyncStorage from '@react-native-async-storage/async-storage';

import DashboardScreen from './src/screens/DashboardScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import LogsScreen from './src/screens/LogsScreen';
import PinScreen, { PIN_KEY } from './src/screens/PinScreen';
import { AppContext } from './src/context/AppContext';

const Tab = createBottomTabNavigator();

export default function App() {
  // null = loading, 'create' = no pin yet, 'unlock' = pin exists, 'open' = authenticated
  const [pinState, setPinState] = useState<'loading' | 'create' | 'unlock' | 'open'>('loading');

  useEffect(() => {
    AsyncStorage.getItem(PIN_KEY).then(stored => {
      setPinState(stored ? 'unlock' : 'create');
    });
  }, []);

  const handlePinSuccess = useCallback(() => setPinState('open'), []);

  const lockApp = useCallback(async () => {
    await AsyncStorage.removeItem(PIN_KEY);
    setPinState('create');
  }, []);

  if (pinState === 'loading') return null;

  if (pinState === 'create' || pinState === 'unlock') {
    return <PinScreen mode={pinState} onSuccess={handlePinSuccess} />;
  }

  return (
    <AppContext.Provider value={{ lockApp }}>
      <NavigationContainer>
        <Tab.Navigator screenOptions={({ route }) => ({
          tabBarIcon: ({ focused, color, size }) => {
            let iconName: any = 'home-outline';
            if (route.name === 'Главная') iconName = focused ? 'home' : 'home-outline';
            if (route.name === 'Настройки') iconName = focused ? 'settings' : 'settings-outline';
            if (route.name === 'Логи') iconName = focused ? 'document-text' : 'document-text-outline';
            return <Ionicons name={iconName} size={size} color={color} />;
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
