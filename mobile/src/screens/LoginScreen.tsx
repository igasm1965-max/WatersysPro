import React, { useState } from 'react';
import { View } from 'react-native';
import { TextInput, Button, Title } from 'react-native-paper';
import ApiService from '../services/api';

export default function LoginScreen({ navigation, onLogin }: any) {
  const [email, setEmail] = useState('admin@example.com');
  const [password, setPassword] = useState('password');

  const login = async () => {
    try {
      const r = await ApiService.login(email, password);
      (global as any).authToken = r.token;
      alert('Logged in');
      if (onLogin) onLogin(r.token);
    } catch (err) { alert('Login failed'); }
  };

  return (
    <View style={{flex:1,justifyContent:'center',padding:16}}>
      <Title style={{marginBottom:16}}>Login</Title>
      <TextInput label="Email" value={email} onChangeText={setEmail} mode="outlined" />
      <TextInput label="Password" value={password} onChangeText={setPassword} secureTextEntry mode="outlined" style={{marginTop:8}} />
      <Button mode="contained" onPress={login} style={{marginTop:16}}>Login</Button>
    </View>
  );
}
