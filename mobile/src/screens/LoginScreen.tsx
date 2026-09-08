import React, { useState } from 'react';
import { View } from 'react-native';
import { TextInput, Button, Title } from 'react-native-paper';
import AsyncStorage from '@react-native-async-storage/async-storage';
import ApiService from '../services/api';

const AUTH_TOKEN_KEY = '@watersys/auth_token';

/**
 * LoginScreen — экран аутентификации через backend (JWT).
 *
 * ВНИМАНИЕ: Этот экран НЕ ПОДКЛЮЧЁН в App.tsx.
 * Текущая версия приложения использует PinScreen для локальной аутентификации.
 *
 * Для подключения:
 * 1. Импортировать LoginScreen в App.tsx
 * 2. Добавить в навигацию перед PinScreen или как отдельный экран
 * 3. Передавать onLogin callback для сохранения JWT токена
 *
 * @deprecated Экран создан для будущего использования.
 *             Не удалён, чтобы сохранить архитектуру для доработки.
 */

async function storeToken(token: string): Promise<void> {
  await AsyncStorage.setItem(AUTH_TOKEN_KEY, token);
}

export default function LoginScreen({ navigation, onLogin }: any) {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [isRegistering, setIsRegistering] = useState(false);

  const login = async () => {
    try {
      const r = await ApiService.login(email, password);
      await storeToken(r.token);
      alert('Logged in');
      if (onLogin) onLogin(r.token);
    } catch (err) { alert('Login failed'); }
  };

  const register = async () => {
    try {
      const r = await ApiService.register(email, password);
      let token = r?.token;
      if (!token) {
        const loginResponse = await ApiService.login(email, password);
        token = loginResponse?.token;
      }
      if (!token) throw new Error('No token received after registration');
      await storeToken(token);
      alert('Registered successfully');
      if (onLogin) onLogin(token);
    } catch (err) { alert('Registration failed'); }
  };

  return (
    <View style={{flex:1,justifyContent:'center',padding:16}}>
      <Title style={{marginBottom:16}}>{isRegistering ? 'Register' : 'Login'}</Title>
      <TextInput label="Email" value={email} onChangeText={setEmail} mode="outlined" />
      <TextInput label="Password" value={password} onChangeText={setPassword} secureTextEntry mode="outlined" style={{marginTop:8}} />
      <Button mode="contained" onPress={isRegistering ? register : login} style={{marginTop:16}}>
        {isRegistering ? 'Register' : 'Login'}
      </Button>
      <Button mode="text" onPress={() => setIsRegistering(!isRegistering)} style={{marginTop:8}}>
        {isRegistering ? 'Already have account? Login' : "Don't have account? Register"}
      </Button>
    </View>
  );
}
