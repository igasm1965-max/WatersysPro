import React, { useEffect, useState } from 'react';
import { ScrollView } from 'react-native';
import { Card, Title, Button, Switch, TextInput, List } from 'react-native-paper';
import AsyncStorage from '@react-native-async-storage/async-storage';
import * as Notifications from 'expo-notifications';
import Constants from 'expo-constants';
import { saveSetting, loadSetting } from '../services/secureStorage';
import { useAppContext } from '../context/AppContext';
import MqttService from '../services/mqttService';
import api from '../services/api';

export default function SettingsScreen(){
  const { lockApp } = useAppContext();
  const [notifications, setNotifications] = useState(true);
  const [serverUrl, setServerUrl] = useState('http://192.168.0.103:3000');
  const [adminToken, setAdminToken] = useState('');
  const [autoFlush, setAutoFlush] = useState(false);
  const [mqttBroker, setMqttBroker] = useState('m1.wqtt.ru');
  const [mqttPort, setMqttPort] = useState('19163');
  const [mqttUser, setMqttUser] = useState('');
  const [mqttPass, setMqttPass] = useState('');

  useEffect(()=>{ loadSettings(); }, []);
  const loadSettings = async () => {
    const url = await AsyncStorage.getItem('serverUrl');
    const token = await loadSetting('adminToken');
    const notif = await AsyncStorage.getItem('notifications');
    const auto = await AsyncStorage.getItem('autoFlush');
    const broker = await AsyncStorage.getItem('mqttBroker');
    const port = await AsyncStorage.getItem('mqttPort');
    const user = await loadSetting('mqttUser');
    const pass = await loadSetting('mqttPass');
    if (url) setServerUrl(url);
    if (token) setAdminToken(token);
    if (notif) setNotifications(notif === 'true');
    if (auto) setAutoFlush(auto === 'true');
    if (broker) setMqttBroker(broker);
    if (port) setMqttPort(port);
    if (user) setMqttUser(user);
    if (pass) setMqttPass(pass);
  };
  const saveSettings = async () => {
    await AsyncStorage.setItem('serverUrl', serverUrl);
    await saveSetting('adminToken', adminToken.trim());
    await AsyncStorage.setItem('notifications', String(notifications));
    await AsyncStorage.setItem('autoFlush', String(autoFlush));
    await AsyncStorage.setItem('mqttBroker', mqttBroker.trim());
    await AsyncStorage.setItem('mqttPort', mqttPort.trim());
    await saveSetting('mqttUser', mqttUser.trim());
    await saveSetting('mqttPass', mqttPass);
    MqttService.reconnect();
    alert('Settings saved');
  };

  const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

  const getExpoTokenWithRetry = async (projectId?: string) => {
    const maxAttempts = 4;
    let lastError: any;

    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        return await Notifications.getExpoPushTokenAsync(
          projectId ? { projectId } : undefined
        );
      } catch (error: any) {
        lastError = error;
        const msg = String(error?.message || error || '').toUpperCase();
        const retryable =
          msg.includes('SERVICE_NOT_AVAILABLE') ||
          msg.includes('TIMEOUT') ||
          msg.includes('ECONNRESET') ||
          msg.includes('NETWORK');

        if (!retryable || attempt === maxAttempts) {
          throw error;
        }

        // Short backoff helps when Google services temporarily fail.
        await sleep(attempt * 1200);
      }
    }

    throw lastError;
  };

  const registerPush = async () => {
    try {
      const current = await Notifications.getPermissionsAsync();
      let status = current.status;
      if (status !== 'granted') {
        const req = await Notifications.requestPermissionsAsync();
        status = req.status;
      }

      if (status !== 'granted') {
        alert('Разрешите уведомления в настройках телефона.');
        return;
      }

      const projectId =
        Constants.expoConfig?.extra?.eas?.projectId ||
        (Constants as any)?.easConfig?.projectId;

      const tokenData = await getExpoTokenWithRetry(projectId);
      const token = tokenData?.data || '';
      if (!token) {
        alert('Не удалось получить push token.');
        return;
      }

      await api.registerPushToken(token);
      await saveSetting('expoPushToken', token);
      alert('Push token зарегистрирован.');
    } catch (e: any) {
      const msg = String(e?.message || e || 'unknown error');
      if (msg.toLowerCase().includes('expo go')) {
        alert('В Expo Go remote push недоступен. Для push нужен development build или APK.');
      } else if (msg.toUpperCase().includes('SERVICE_NOT_AVAILABLE')) {
        alert('Google Push Service временно недоступен (SERVICE_NOT_AVAILABLE). Проверьте интернет на телефоне, отключите VPN/Private DNS и повторите через 1-2 минуты.');
      } else if (
        msg.includes('Default FirebaseApp is not initialized') ||
        msg.toLowerCase().includes('fcm-credentials')
      ) {
        alert('FCM для Android не настроен. Добавьте google-services.json в проект mobile и загрузите FCM credentials в EAS (Firebase service account), затем пересоберите APK.');
      } else {
        alert('Ошибка регистрации push: ' + msg);
      }
    }
  };

  return (
    <ScrollView style={{flex:1, backgroundColor:'#f5f5f5'}}>
      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Подключение</Title>
          <TextInput label="Адрес сервера" value={serverUrl} onChangeText={setServerUrl} placeholder="http://your-server:3000" mode="outlined" style={{marginTop:16}} />
          <TextInput label="Admin token устройства (ESP32)" value={adminToken} onChangeText={setAdminToken} placeholder="токен из веб-интерфейса устройства" mode="outlined" style={{marginTop:12}} secureTextEntry />
          <Button mode="contained" onPress={saveSettings} style={{marginTop:16}}>Сохранить</Button>
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>MQTT (брокер)</Title>
          <TextInput label="Адрес брокера" value={mqttBroker} onChangeText={setMqttBroker} placeholder="m1.wqtt.ru" mode="outlined" style={{marginTop:8}} />
          <TextInput label="WSS порт" value={mqttPort} onChangeText={setMqttPort} placeholder="19163" mode="outlined" style={{marginTop:8}} keyboardType="numeric" />
          <TextInput label="MQTT пользователь" value={mqttUser} onChangeText={setMqttUser} placeholder="username" mode="outlined" style={{marginTop:8}} />
          <TextInput label="MQTT пароль" value={mqttPass} onChangeText={setMqttPass} placeholder="password" mode="outlined" style={{marginTop:8}} secureTextEntry />
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Уведомления</Title>
          <List.Item title="Push-уведомления" description="Получать оповещения об авариях" right={() => (<Switch value={notifications} onValueChange={setNotifications} />)} />
          <List.Item title="Автоматическая промывка" description="Запускать промывку по расписанию" right={() => (<Switch value={autoFlush} onValueChange={setAutoFlush} />)} />
          <Button mode="contained" onPress={registerPush} style={{marginTop:16}}>Зарегистрировать push token</Button>
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Безопасность</Title>
          <Button
            mode="outlined"
            icon="lock-reset"
            onPress={lockApp}
            style={{marginTop:8}}
            textColor="#c62828"
          >
            Сменить PIN-код
          </Button>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}
