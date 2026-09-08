import React, { useEffect, useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, Alert } from 'react-native';
import * as SecureStore from 'expo-secure-store';

export const PIN_KEY = 'appPin';
const PIN_LENGTH = 4;

// Защита от подбора кода (brute-force)
const MAX_ATTEMPTS = 5;
const LOCK_MS = 30000; // 30 секунд блокировки после MAX_ATTEMPTS неудач
const ATTEMPTS_KEY = 'pinFailedAttempts';
const LOCK_UNTIL_KEY = 'pinLockUntil';

interface Props {
  mode: 'create' | 'unlock' | 'change';
  onSuccess: () => void;
}

type Step = 'verify' | 'enter' | 'confirm';

/** PIN хранится в SecureStore (не в открытом AsyncStorage). */
async function readPin(): Promise<string | null> {
  return SecureStore.getItemAsync(PIN_KEY);
}
async function writePin(pin: string): Promise<void> {
  await SecureStore.setItemAsync(PIN_KEY, pin);
}
async function readAttempts(): Promise<number> {
  const raw = await SecureStore.getItemAsync(ATTEMPTS_KEY);
  const n = raw ? parseInt(raw, 10) : NaN;
  return Number.isFinite(n) && n > 0 ? n : 0;
}
async function readLockUntil(): Promise<number> {
  const raw = await SecureStore.getItemAsync(LOCK_UNTIL_KEY);
  const n = raw ? parseInt(raw, 10) : NaN;
  return Number.isFinite(n) ? n : 0;
}

export default function PinScreen({ mode, onSuccess }: Props) {
  const [step, setStep] = useState<Step>(mode === 'create' ? 'enter' : 'verify');
  const [pin, setPin] = useState('');
  const [confirmPin, setConfirmPin] = useState('');
  const [lockedUntil, setLockedUntil] = useState(0);
  const [now, setNow] = useState(Date.now());

  // Проверка блокировки при монтировании + таймер обратного отсчёта
  useEffect(() => {
    let timer: ReturnType<typeof setInterval> | null = null;
    readLockUntil().then((until) => {
      if (until > Date.now()) {
        setLockedUntil(until);
        timer = setInterval(() => setNow(Date.now()), 1000);
      }
    });
    return () => { if (timer) clearInterval(timer); };
  }, []);

  // Остановить таймер, когда блокировка истекла
  useEffect(() => {
    if (lockedUntil > 0 && lockedUntil <= Date.now()) setLockedUntil(0);
  }, [now, lockedUntil]);

  const currentInput = step === 'verify' || step === 'enter' ? pin : confirmPin;
  const lockRemaining = lockedUntil > now ? Math.ceil((lockedUntil - now) / 1000) : 0;

  const title =
    mode === 'unlock'
      ? 'Введите код доступа'
      : mode === 'change'
      ? step === 'verify'
        ? 'Введите текущий код'
        : step === 'enter'
        ? 'Придумайте новый код'
        : 'Повторите новый код'
      : step === 'enter'
      ? 'Придумайте код (4 цифры)'
      : 'Повторите код';

  const handleWrongPin = async () => {
    const attempts = (await readAttempts()) + 1;
    setPin('');
    if (attempts >= MAX_ATTEMPTS) {
      const until = Date.now() + LOCK_MS;
      await SecureStore.setItemAsync(ATTEMPTS_KEY, '0');
      await SecureStore.setItemAsync(LOCK_UNTIL_KEY, String(until));
      setLockedUntil(until);
      setNow(Date.now());
      Alert.alert('Слишком много попыток', `Ввод заблокирован на ${Math.round(LOCK_MS / 1000)} секунд`);
    } else {
      await SecureStore.setItemAsync(ATTEMPTS_KEY, String(attempts));
      Alert.alert('Неверный код', `Попробуйте ещё раз (осталось попыток: ${MAX_ATTEMPTS - attempts})`);
    }
  };

  const doVerify = async (entered: string) => {
    const stored = await readPin();
    if (entered === stored) {
      await SecureStore.setItemAsync(ATTEMPTS_KEY, '0');
      if (mode === 'unlock') {
        onSuccess();
      } else {
        // Режим change — переходим к установке нового PIN
        setPin('');
        setStep('enter');
      }
    } else {
      await handleWrongPin();
    }
  };

  const doConfirm = async (first: string, second: string) => {
    if (first === second) {
      await writePin(first);
      onSuccess();
    } else {
      Alert.alert('Коды не совпадают', 'Начните заново');
      setPin('');
      setConfirmPin('');
      setStep('enter');
    }
  };

  const handleDigit = (d: string) => {
    if (lockRemaining > 0) return;
    if (step === 'verify') {
      const next = pin + d;
      if (next.length > PIN_LENGTH) return;
      setPin(next);
      if (next.length === PIN_LENGTH) doVerify(next);
    } else if (step === 'enter') {
      const next = pin + d;
      if (next.length > PIN_LENGTH) return;
      setPin(next);
      if (next.length === PIN_LENGTH) setTimeout(() => setStep('confirm'), 150);
    } else {
      const next = confirmPin + d;
      if (next.length > PIN_LENGTH) return;
      setConfirmPin(next);
      if (next.length === PIN_LENGTH) doConfirm(pin, next);
    }
  };

  const handleDelete = () => {
    if (step === 'confirm') {
      setConfirmPin((p) => p.slice(0, -1));
    } else {
      setPin((p) => p.slice(0, -1));
    }
  };

  const keys = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '', '0', '⌫'];

  if (lockRemaining > 0) {
    return (
      <View style={styles.container}>
        <Text style={styles.appName}>WaterSys</Text>
        <Text style={styles.lockTitle}>🔒 Ввод заблокирован</Text>
        <Text style={styles.lockTimer}>Повторите через {lockRemaining} с</Text>
      </View>
    );
  }

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
  lockTitle: {
    fontSize: 20,
    fontWeight: '600',
    color: '#c62828',
    marginBottom: 12,
  },
  lockTimer: {
    fontSize: 16,
    color: '#555',
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
