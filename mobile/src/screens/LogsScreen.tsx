import React, { useCallback, useState } from 'react';
import { View, ScrollView, Text, StyleSheet } from 'react-native';
import { Card, Title, Button, ActivityIndicator, SegmentedButtons, Paragraph, Chip } from 'react-native-paper';
import api from '../services/api';

type LogSource = 'direct_events' | 'direct_sd' | 'mqtt_db';

export default function LogsScreen() {
  const [logs, setLogs] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [logSource, setLogSource] = useState<LogSource>('mqtt_db');
  const [deviceIp, setDeviceIp] = useState<string | null>(null);
  const [connectionMode, setConnectionMode] = useState<'direct' | 'proxy' | 'mqtt' | null>(null);

  const fetchLogs = useCallback(async () => {
    setLoading(true);
    setError(null);
    setLogs(null);
    setConnectionMode(null);

    try {
      // Get device list first
      let devices: string[];
      try {
        devices = await api.getDevices();
      } catch (netErr: any) {
        const netMsg = (netErr?.message || '').toLowerCase();
        if (netMsg.includes('network error') || netMsg.includes('econnrefused') || netMsg.includes('timeout')) {
          setError(
            'Не удалось подключиться к серверу.\n\n' +
            'Проверьте:\n' +
            '• Адрес сервера в Настройках (Settings → Адрес сервера)\n' +
            '• Сервер должен быть запущен и доступен из интернета\n' +
            '• Если сервер на локальной машине — телефон должен быть в той же Wi-Fi сети\n\n' +
            'Текущий адрес сервера можно изменить в меню Settings'
          );
          setLoading(false);
          return;
        }
        throw netErr;
      }

      if (devices.length === 0) {
        setError('Нет устройств. Убедитесь, что устройство отправляет телеметрию.\n\n' +
          'Возможные причины:\n' +
          '• Устройство не подключено к Wi-Fi\n' +
          '• MQTT брокер недоступен\n' +
          '• Устройство недавно перезагружено — подождите 1-2 минуты');
        setLoading(false);
        return;
      }
      const targetDevice = devices[0];

      if (logSource === 'mqtt_db') {
        // Fetch logs from database (published by device via MQTT every 60s)
        // Works from ANY network — no direct connection needed
        setConnectionMode('mqtt');
        try {
          const data = await api.getDeviceLogsFromDb(targetDevice);
          setLogs(data);
        } catch (dbErr: any) {
          const status = dbErr?.response?.status;
          const errMsg = (dbErr?.message || '').toLowerCase();

          // Network-level error (server unreachable)
          if (errMsg.includes('network error') || errMsg.includes('econnrefused') || errMsg.includes('timeout')) {
            setError(
              'Не удалось подключиться к серверу для получения логов.\n\n' +
              'Проверьте:\n' +
              '• Адрес сервера в Настройках\n' +
              '• Сервер запущен и доступен\n\n' +
              'Текущий адрес: откройте Settings → Адрес сервера'
            );
          } else if (status === 404) {
            setError(
              'Логи ещё не опубликованы устройством через MQTT.\n\n' +
              'Устройство публикует логи раз в 60 секунд.\n' +
              'Подождите 1-2 минуты после запуска устройства и попробуйте снова.\n\n' +
              'Если проблема сохраняется:\n' +
              '• Проверьте подключение устройства к Wi-Fi\n' +
              '• Проверьте настройки MQTT в инженерном меню\n' +
              '• Попробуйте режимы "RAM" или "SD" (в одной сети с устройством)'
            );
          } else {
            throw dbErr;
          }
        }
        setLoading(false);
        return;
      }

      // Direct connection modes — need device IP
      const ip = await api.getDeviceIp(targetDevice);
      setDeviceIp(ip);

      if (!ip) {
        setError('Не удалось получить IP устройства из телеметрии.\n\n' +
          'Возможные причины:\n' +
          '• Устройство не подключено к Wi-Fi\n' +
          '• Телеметрия ещё не отправлена (подождите 10-30 секунд)');
        setLoading(false);
        return;
      }

      // Try direct connection to ESP32
      setConnectionMode('direct');
      const logType = logSource === 'direct_sd' ? 'sd' : 'events';
      const data = await api.fetchDeviceLogsDirect(ip, logType);
      setLogs(data);
    } catch (err: any) {
      const status = err?.response?.status;
      const detail = err?.response?.data?.detail || err?.response?.data?.error || '';
      const msg = err?.message || 'Unknown error';
      const errMsgLower = msg.toLowerCase();

      // Network-level error (server unreachable)
      if (errMsgLower.includes('network error') || errMsgLower.includes('econnrefused') || errMsgLower.includes('timeout')) {
        setError(
          'Не удалось подключиться к серверу.\n\n' +
          'Проверьте:\n' +
          '• Адрес сервера в Настройках (Settings → Адрес сервера)\n' +
          '• Сервер должен быть запущен и доступен из интернета\n' +
          '• Если сервер на локальной машине — телефон должен быть в той же Wi-Fi сети'
        );
      } else if (status === 502) {
        setError(
          `Устройство недоступно по IP ${deviceIp || '?'}.\n\n` +
          'Возможные причины:\n' +
          '• Телефон не в той же Wi-Fi сети, что и устройство\n' +
          '• Устройство выключено или перезагружается\n' +
          '• Изменился IP устройства'
        );
      } else if (status === 504) {
        setError('Устройство не ответило вовремя. Возможно, оно перегружено или занято.');
      } else if (detail) {
        setError(`Ошибка: ${detail}`);
      } else {
        setError(`Ошибка: ${msg}`);
      }
    } finally {
      setLoading(false);
    }
  }, [logSource, deviceIp]);

  return (
    <ScrollView style={styles.container}>
      <Card style={styles.card}>
        <Card.Content>
          <Title>Логи устройства</Title>
          <Paragraph style={styles.paragraph}>
            Выберите источник логов:
            {'\n'}• <Text style={{fontWeight:'bold'}}>Через сервер (MQTT)</Text> — работает из любой сети, логи обновляются раз в минуту
            {'\n'}• <Text style={{fontWeight:'bold'}}>Напрямую (RAM)</Text> — только в одной WiFi-сети с устройством, актуальные логи из ОЗУ
            {'\n'}• <Text style={{fontWeight:'bold'}}>Напрямую (SD)</Text> — только в одной WiFi-сети, лог-файл с SD-карты
          </Paragraph>

          {deviceIp && (
            <Chip icon="wifi" style={styles.chip}>
              IP устройства: {deviceIp}
            </Chip>
          )}

          {connectionMode && (
            <Chip
              icon={connectionMode === 'direct' ? 'flash' : connectionMode === 'mqtt' ? 'cloud' : 'server'}
              style={[
                styles.chip,
                connectionMode === 'direct' ? styles.chipDirect :
                connectionMode === 'mqtt' ? styles.chipMqtt : styles.chipProxy
              ]}
            >
              {connectionMode === 'direct' ? 'Прямое подключение' :
               connectionMode === 'mqtt' ? 'Через сервер (MQTT)' : 'Через сервер (прокси)'}
            </Chip>
          )}

          <SegmentedButtons
            value={logSource}
            onValueChange={(val) => setLogSource(val as LogSource)}
            buttons={[
              { value: 'mqtt_db', label: 'MQTT' },
              { value: 'direct_events', label: 'RAM' },
              { value: 'direct_sd', label: 'SD' },
            ]}
            style={styles.segment}
          />

          <Button
            mode="contained"
            onPress={fetchLogs}
            loading={loading}
            disabled={loading}
            style={styles.button}
            icon="file-document"
          >
            {loading ? 'Загрузка...' : 'Загрузить логи'}
          </Button>
        </Card.Content>
      </Card>

      {error && (
        <Card style={[styles.card, styles.errorCard]}>
          <Card.Content>
            <Text style={styles.errorText}>{error}</Text>
          </Card.Content>
        </Card>
      )}

      {loading && (
        <View style={styles.loadingContainer}>
          <ActivityIndicator size="large" />
          <Text style={styles.loadingText}>Получение логов...</Text>
        </View>
      )}

      {logs && (
        <Card style={styles.card}>
          <Card.Content>
            <View style={styles.logHeader}>
              <Title>Содержимое лога</Title>
              <Text style={styles.logLineCount}>
                {logs.split('\n').length} строк
              </Text>
            </View>
            <ScrollView
              horizontal
              style={styles.logScrollH}
              contentContainerStyle={styles.logScrollHContent}
            >
              <ScrollView style={styles.logScrollV}>
                <Text style={styles.logText} selectable>
                  {logs}
                </Text>
              </ScrollView>
            </ScrollView>
          </Card.Content>
        </Card>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  card: {
    margin: 16,
  },
  errorCard: {
    backgroundColor: '#ffebee',
  },
  paragraph: {
    marginBottom: 12,
    color: '#555',
  },
  chip: {
    marginBottom: 8,
    alignSelf: 'flex-start',
  },
  chipDirect: {
    backgroundColor: '#e8f5e9',
  },
  chipProxy: {
    backgroundColor: '#fff3e0',
  },
  chipMqtt: {
    backgroundColor: '#e3f2fd',
  },
  segment: {
    marginBottom: 16,
  },
  button: {
    marginTop: 4,
  },
  errorText: {
    color: '#c62828',
  },
  loadingContainer: {
    alignItems: 'center',
    padding: 32,
  },
  loadingText: {
    marginTop: 12,
    color: '#555',
  },
  logHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  logLineCount: {
    color: '#888',
    fontSize: 12,
  },
  logScrollH: {
    maxHeight: 500,
    borderWidth: 1,
    borderColor: '#e0e0e0',
    borderRadius: 4,
    backgroundColor: '#1e1e1e',
  },
  logScrollHContent: {
    minWidth: '100%',
  },
  logScrollV: {
    padding: 12,
  },
  logText: {
    fontFamily: 'monospace',
    fontSize: 11,
    color: '#d4d4d4',
    lineHeight: 16,
  },
});