import React, { useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, Alert } from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';

export const PIN_KEY = 'appPin';
const PIN_LENGTH = 4;

interface Props {
  mode: 'create' | 'unlock';
  onSuccess: () => void;
}

export default function PinScreen({ mode, onSuccess }: Props) {
  const [pin, setPin] = useState('');
  const [confirmPin, setConfirmPin] = useState('');
  const [step, setStep] = useState<'enter' | 'confirm'>('enter');

  const currentInput = step === 'enter' ? pin : confirmPin;

  const title =
    mode === 'unlock'
      ? 'Введите код доступа'
      : step === 'enter'
      ? 'Придумайте код (4 цифры)'
      : 'Повторите код';

  const handleDigit = (d: string) => {
    if (mode === 'unlock') {
      const next = pin + d;
      if (next.length > PIN_LENGTH) return;
      setPin(next);
      if (next.length === PIN_LENGTH) verifyPin(next);
    } else {
      if (step === 'enter') {
        const next = pin + d;
        if (next.length > PIN_LENGTH) return;
        setPin(next);
        if (next.length === PIN_LENGTH) setTimeout(() => setStep('confirm'), 150);
      } else {
        const next = confirmPin + d;
        if (next.length > PIN_LENGTH) return;
        setConfirmPin(next);
        if (next.length === PIN_LENGTH) checkConfirm(pin, next);
      }
    }
  };

  const handleDelete = () => {
    if (mode === 'unlock') {
      setPin(p => p.slice(0, -1));
    } else if (step === 'enter') {
      setPin(p => p.slice(0, -1));
    } else {
      setConfirmPin(p => p.slice(0, -1));
    }
  };

  const verifyPin = async (entered: string) => {
    const stored = await AsyncStorage.getItem(PIN_KEY);
    if (entered === stored) {
      onSuccess();
    } else {
      setPin('');
      Alert.alert('Неверный код', 'Попробуйте ещё раз');
    }
  };

  const checkConfirm = async (first: string, second: string) => {
    if (first === second) {
      await AsyncStorage.setItem(PIN_KEY, first);
      onSuccess();
    } else {
      Alert.alert('Коды не совпадают', 'Начните заново');
      setPin('');
      setConfirmPin('');
      setStep('enter');
    }
  };

  const keys = ['1','2','3','4','5','6','7','8','9','','0','⌫'];

  return (
    <View style={styles.container}>
      <Text style={styles.appName}>WaterSys</Text>
      <Text style={styles.title}>{title}</Text>

      <View style={styles.dots}>
        {Array.from({ length: PIN_LENGTH }).map((_, i) => (
          <View
            key={i}
            style={[styles.dot, i < currentInput.length && styles.dotFilled]}
          />
        ))}
      </View>

      <View style={styles.keypad}>
        {keys.map((k, i) => {
          if (k === '') return <View key={i} style={styles.keyEmpty} />;
          const isDelete = k === '⌫';
          return (
            <TouchableOpacity
              key={i}
              style={[styles.key, isDelete && styles.keyDelete]}
              onPress={() => (isDelete ? handleDelete() : handleDigit(k))}
              activeOpacity={0.7}
            >
              <Text style={[styles.keyText, isDelete && styles.keyDeleteText]}>{k}</Text>
            </TouchableOpacity>
          );
        })}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#e8f5e9',
  },
  appName: {
    fontSize: 28,
    fontWeight: '700',
    color: '#1b5e20',
    letterSpacing: 2,
    marginBottom: 8,
  },
  title: {
    fontSize: 16,
    color: '#555',
    marginBottom: 36,
  },
  dots: {
    flexDirection: 'row',
    marginBottom: 48,
  },
  dot: {
    width: 20,
    height: 20,
    borderRadius: 10,
    borderWidth: 2,
    borderColor: '#2e7d32',
    marginHorizontal: 12,
    backgroundColor: 'transparent',
  },
  dotFilled: {
    backgroundColor: '#2e7d32',
  },
  keypad: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    width: 252,
  },
  key: {
    width: 72,
    height: 72,
    justifyContent: 'center',
    alignItems: 'center',
    margin: 6,
    borderRadius: 36,
    backgroundColor: '#ffffff',
    elevation: 3,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.15,
    shadowRadius: 2,
  },
  keyEmpty: {
    width: 72,
    height: 72,
    margin: 6,
  },
  keyDelete: {
    backgroundColor: '#ffebee',
    elevation: 1,
  },
  keyText: {
    fontSize: 26,
    fontWeight: '500',
    color: '#1b5e20',
  },
  keyDeleteText: {
    fontSize: 22,
    color: '#c62828',
  },
});
