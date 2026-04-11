import * as SecureStore from 'expo-secure-store';

const SECURE_KEYS = ['adminToken', 'mqttPass', 'mqttUser'];

/**
 * Save a setting — sensitive keys go to SecureStore, others to AsyncStorage.
 */
export async function saveSetting(key: string, value: string): Promise<void> {
  if (SECURE_KEYS.includes(key)) {
    await SecureStore.setItemAsync(key, value);
  } else {
    const AsyncStorage = (await import('@react-native-async-storage/async-storage')).default;
    await AsyncStorage.setItem(key, value);
  }
}

/**
 * Load a setting — checks SecureStore first for sensitive keys.
 */
export async function loadSetting(key: string): Promise<string | null> {
  if (SECURE_KEYS.includes(key)) {
    return await SecureStore.getItemAsync(key);
  }
  const AsyncStorage = (await import('@react-native-async-storage/async-storage')).default;
  return await AsyncStorage.getItem(key);
}
