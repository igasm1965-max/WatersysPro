import React, { useEffect, useState } from 'react';
import { ScrollView } from 'react-native';
import { Card, Title, Button, Switch, TextInput, List } from 'react-native-paper';
import AsyncStorage from '@react-native-async-storage/async-storage';
import * as Notifications from 'expo-notifications';
import ApiService from '../services/api';

export default function SettingsScreen(){
  const [notifications, setNotifications] = useState(true);
  const [serverUrl, setServerUrl] = useState('http://10.0.0.100:3000');
  const [autoFlush, setAutoFlush] = useState(false);

  useEffect(()=>{ loadSettings(); }, []);
  const loadSettings = async () => {
    const url = await AsyncStorage.getItem('serverUrl');
    const notif = await AsyncStorage.getItem('notifications');
    const auto = await AsyncStorage.getItem('autoFlush');
    if (url) setServerUrl(url);
    if (notif) setNotifications(notif === 'true');
    if (auto) setAutoFlush(auto === 'true');
  };
  const saveSettings = async () => {
    await AsyncStorage.setItem('serverUrl', serverUrl);
    await AsyncStorage.setItem('notifications', String(notifications));
    await AsyncStorage.setItem('autoFlush', String(autoFlush));
    if (notifications) await Notifications.requestPermissionsAsync();
    alert('Settings saved');
  };

  const registerPush = async () => {
    const tokenObj = await Notifications.getExpoPushTokenAsync();
    if (tokenObj && tokenObj.data) await ApiService.registerPushToken(tokenObj.data);
    alert('Push token registered');
  };

  return (
    <ScrollView style={{flex:1, backgroundColor:'#f5f5f5'}}>
      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Подключение</Title>
          <TextInput label="Адрес сервера" value={serverUrl} onChangeText={setServerUrl} placeholder="http://your-server:3000" mode="outlined" style={{marginTop:16}} />
          <Button mode="contained" onPress={saveSettings} style={{marginTop:16}}>Сохранить</Button>
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
    </ScrollView>
  );
}
