import React, { useState } from 'react';
import { View } from 'react-native';
import { TextInput, Button, Title } from 'react-native-paper';
import ApiService from '../services/api';

export default function LoginScreen({ navigation, onLogin }: any) {
  const [email, setEmail] = useState('your_email@example.com');
  const [password, setPassword] = useState('your_password');
  const [isRegistering, setIsRegistering] = useState(false);

  const login = async () => {
    try {
      const r = await ApiService.login(email, password);
      (global as any).authToken = r.token;
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
      (global as any).authToken = token;
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
