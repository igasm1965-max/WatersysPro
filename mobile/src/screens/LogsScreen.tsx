/**
 * @file LogsScreen.tsx
 * @brief Экран логов устройства — упрощённая версия (v1.3.6)
 *
 * Изменения в версии 1.3.6:
 * - Удалены неработающие режимы: RAM (direct_events), SD (direct_sd), ручной ввод IP
 * - Удалён SegmentedButtons с выбором источника логов
 * - Удалён WebView внутри приложения (react-native-webview больше не требуется)
 * - Удалены все Chip-индикаторы режимов подключения
 * - Удалена функция fetchLogs() и связанные HTTP-запросы к устройству
 * - Новая логика: при входе подписываемся на MQTT топик watersystem/logs (полный лог)
 *   и watersystem/logs/event (новые события в реальном времени)
 * - Кнопка "Открыть веб-интерфейс" использует IP из глобального контекста
 *
 * @see plans/logs_screen_redesign_plan.md
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import { View, ScrollView, Text, StyleSheet, Linking, Alert } from 'react-native';
import { Card, Title, Button, ActivityIndicator, Paragraph, Chip } from 'react-native-paper';
import MqttService from '../services/mqttService';
import { useAppContext } from '../context/AppContext';

export default function LogsScreen() {
  const [logs, setLogs] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [mqttConnected, setMqttConnected] = useState(false);
  const logsRef = useRef<string | null>(null);
  // Флаг: получен ли первый полный лог. После получения игнорируем
  // последующие полные логи (раз в 60с), т.к. новые события уже приходят
  // по одному через watersystem/logs/event
  const initialLogReceived = useRef(false);
  // IP устройства из глобального контекста (устанавливается на DashboardScreen)
  const { deviceIp } = useAppContext();

  // Синхронизируем ref с состоянием для использования в MQTT колбэках
  logsRef.current = logs;

  /**
   * MQTT: подключение и подписка на логи
   *
   * При входе на экран:
   * 1. Подписываемся на watersystem/logs — получаем полный лог (последние ~30 событий)
   * 2. Подписываемся на watersystem/logs/event — новые события в реальном времени
   * 3. Каждое новое событие добавляется в конец списка
   *
   * Важно: полный лог публикуется устройством раз в 60 секунд. После получения
   * первого полного лога (initialLogReceived) последующие игнорируются, чтобы
   * они не затирали события, добавленные по одному через onSingleEvent.
   */
  useEffect(() => {
    setLoading(true);
    setError(null);
    // Сбрасываем флаг при монтировании (перезаходе на экран)
    initialLogReceived.current = false;

    let unsubLogs: (() => void) | null = null;
    let unsubConn: (() => void) | null = null;
    let unsubEvent: (() => void) | null = null;

    // Отслеживаем состояние подключения к MQTT брокеру
    unsubConn = MqttService.onConnectionChange((connected) => {
      setMqttConnected(connected);
      if (connected) {
        setLoading(false);
      }
    });

    // Получаем полный лог (последние ~30 событий) из топика watersystem/logs
    // Публикуется устройством раз в 60 секунд, а также при подключении к MQTT
    // ВАЖНО: используем только первый полный лог как начальное состояние.
    // Последующие полные логи (раз в минуту) игнорируем, чтобы не затирать
    // события, уже добавленные по одному через watersystem/logs/event.
    unsubLogs = MqttService.onLogs((logsText) => {
      if (!initialLogReceived.current) {
        initialLogReceived.current = true;
        setLogs(logsText);
        setError(null);
      }
      // После получения первого полного лога — игнорируем последующие,
      // т.к. новые события уже приходят по одному через onSingleEvent
    });

    // Подписываемся на новые события в реальном времени (watersystem/logs/event)
    // Публикуется устройством при каждом новом событии
    unsubEvent = MqttService.onSingleEvent((event) => {
      setLogs((prev) => {
        const line = `${event.ts} - ${event.desc}\n`;
        return prev ? prev + line : line;
      });
    });

    MqttService.connect().then(() => {
      if (MqttService.connected) {
        setLoading(false);
        setMqttConnected(true);
        // Отправляем команду устройству на принудительную публикацию логов
        // Устройство подписано на watersystem/cmd/getlogs и вызовет vqtt_publishLogs()
        MqttService.publish('getlogs', '1');
      }
    }).catch((err) => {
      console.warn('[LogsScreen] MQTT connect error', err);
      setLoading(false);
      setError(
        'Не удалось подключиться к MQTT брокеру.\n\n' +
        'Проверьте:\n' +
        '• Настройки MQTT в Settings (шестерёнка)\n' +
        '• Брокер m1.wqtt.ru должен быть доступен\n' +
        '• Телефон должен иметь доступ в интернет'
      );
    });

    // Отписываемся при уходе с экрана (не отключаем MQTT — может использоваться DashboardScreen)
    return () => {
      if (unsubLogs) unsubLogs();
      if (unsubConn) unsubConn();
      if (unsubEvent) unsubEvent();
    };
  }, []);

  /**
   * Открыть веб-интерфейс устройства в браузере
   * Использует IP из глобального контекста (устанавливается на DashboardScreen)
   * Работает только когда телефон в одной Wi-Fi сети с устройством
   */
  const openWebInterface = useCallback(() => {
    if (!deviceIp) {
      Alert.alert(
        'IP не известен',
        'IP устройства пока не получен. Дождитесь телеметрии на главном экране.'
      );
      return;
    }
    const url = `http://${deviceIp}/`;
    Linking.openURL(url).catch(() => {
      Alert.alert('Ошибка', 'Не удалось открыть браузер');
    });
  }, [deviceIp]);

  return (
    <ScrollView style={styles.container}>
      {/* Карточка статуса подключения */}
      <Card style={styles.card}>
        <Card.Content>
          <Title>Логи устройства</Title>
          <Paragraph style={styles.paragraph}>
            Логи поступают в реальном времени через MQTT.
            {'\n'}• При входе загружаются последние события
            {'\n'}• Новые события добавляются автоматически
          </Paragraph>

          {/* IP устройства (берётся из контекста, устанавливается на DashboardScreen) */}
          {deviceIp && (
            <Chip icon="wifi" style={styles.chip}>
              IP устройства: {deviceIp}
            </Chip>
          )}

          {/* Статус MQTT подключения */}
          <Chip
            icon={mqttConnected ? 'check-circle' : 'alert'}
            style={[styles.chip, mqttConnected ? styles.chipConnected : styles.chipDisconnected]}
          >
            {mqttConnected ? 'MQTT подключён' : 'MQTT не подключён'}
          </Chip>

          {/* Кнопка открытия веб-интерфейса в браузере */}
          {/* Неактивна, пока IP не получен с главного экрана */}
          <Button
            mode="contained"
            onPress={openWebInterface}
            style={styles.button}
            icon="open-in-new"
            disabled={!deviceIp}
          >
            Открыть веб-интерфейс
          </Button>
        </Card.Content>
      </Card>

      {/* Блок ошибок */}
      {error && (
        <Card style={[styles.card, styles.errorCard]}>
          <Card.Content>
            <Text style={styles.errorText}>{error}</Text>
          </Card.Content>
        </Card>
      )}

      {/* Индикатор загрузки */}
      {loading && !logs && (
        <View style={styles.loadingContainer}>
          <ActivityIndicator size="large" />
          <Text style={styles.loadingText}>
            {mqttConnected ? 'Ожидание логов...' : 'Подключение к MQTT...'}
          </Text>
        </View>
      )}

      {/* Блок с логами */}
      {logs && (
        <Card style={styles.card}>
          <Card.Content>
            <View style={styles.logHeader}>
              <Title>События</Title>
              <Text style={styles.logLineCount}>
                {logs.split('\n').length} строк
              </Text>
            </View>
            <Paragraph style={styles.autoUpdateHint}>
              Логи обновляются автоматически в реальном времени
            </Paragraph>
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
  chip: {
    marginVertical: 4,
  },
  chipConnected: {
    backgroundColor: '#e8f5e9',
  },
  chipDisconnected: {
    backgroundColor: '#ffebee',
  },
  button: {
    marginVertical: 8,
  },
  paragraph: {
    marginBottom: 8,
    lineHeight: 20,
  },
  loadingContainer: {
    alignItems: 'center',
    padding: 32,
  },
  loadingText: {
    marginTop: 12,
    color: '#666',
  },
  logHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  logLineCount: {
    color: '#666',
    fontSize: 14,
  },
  autoUpdateHint: {
    fontStyle: 'italic',
    color: '#666',
    marginBottom: 8,
  },
  logScrollH: {
    maxHeight: 400,
  },
  logScrollHContent: {
    minWidth: '100%',
  },
  logScrollV: {
    maxHeight: 380,
  },
  logText: {
    fontFamily: 'monospace',
    fontSize: 11,
    lineHeight: 16,
    color: '#333',
  },
  errorText: {
    color: '#c62828',
    fontSize: 14,
    lineHeight: 20,
  },
});