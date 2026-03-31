import React, { useEffect, useState } from 'react';
import { ScrollView } from 'react-native';
import { Card, Title, Button, Switch, TextInput, List } from 'react-native-paper';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { useAppContext } from '../context/AppContext';

export default function SettingsScreen(){
  const { lockApp } = useAppContext();
  const [notifications, setNotifications] = useState(true);
  const [serverUrl, setServerUrl] = useState('http://192.168.0.103:3000');
  const [adminToken, setAdminToken] = useState('');
  const [autoFlush, setAutoFlush] = useState(false);

  useEffect(()=>{ loadSettings(); }, []);
  const loadSettings = async () => {
    const url = await AsyncStorage.getItem('serverUrl');
    const token = await AsyncStorage.getItem('adminToken');
    const notif = await AsyncStorage.getItem('notifications');
    const auto = await AsyncStorage.getItem('autoFlush');
    if (url) setServerUrl(url);
    if (token) setAdminToken(token);
    if (notif) setNotifications(notif === 'true');
    if (auto) setAutoFlush(auto === 'true');
  };
  const saveSettings = async () => {
    await AsyncStorage.setItem('serverUrl', serverUrl);
    await AsyncStorage.setItem('adminToken', adminToken.trim());
    await AsyncStorage.setItem('notifications', String(notifications));
    await AsyncStorage.setItem('autoFlush', String(autoFlush));
    alert('Settings saved');
  };

  const registerPush = async () => {
    alert('В Expo Go remote push недоступен. Для push нужен development build.');
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
