/**
 * @file LogsScreen.tsx
 * @brief Экран логов устройства — v4.0 (убраны RAM/SD кнопки, исправлен скролл)
 *
 * Ключевые изменения относительно v3.x:
 * - Убран SegmentedButtons (MQTT/RAM/SD) — только MQTT
 * - Убран ScrollView, используется FlatList для производительности
 * - Добавлен авто-скролл к новым строкам
 * - Добавлена кнопка "↓ Вниз" при скролле вверх
 *
 * @see plans/logs_complete_rewrite_v3_plan.md
 */

import React, { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react';
import { View, FlatList, Text, StyleSheet, Linking, Alert, TouchableOpacity } from 'react-native';
import { Card, Title, Button, ActivityIndicator, Chip } from 'react-native-paper';
import MqttService from '../services/mqttService';
import { useAppContext } from '../context/AppContext';

export default function LogsScreen() {
  // --- Состояния ---
  const [renderVersion, setRenderVersion] = useState(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [mqttConnected, setMqttConnected] = useState(false);
  const [showScrollDown, setShowScrollDown] = useState(false);

  // --- Refs ---
  const flatListRef = useRef<FlatList>(null);
  const logLinesRef = useRef<string[]>([]);
  const isNearBottomRef = useRef(true);
  const { deviceIp } = useAppContext();

  // --- Подписки при монтировании ---
  useEffect(() => {
    logLinesRef.current = [];
    setRenderVersion(0);
    setLoading(true);
    setError(null);
    setShowScrollDown(false);
    isNearBottomRef.current = true;

    let destroyed = false;

    console.log('[LOGS-DIAG] LogsScreen mounted, MqttService.connected=' + MqttService.connected);

    const unsubLogs = MqttService.onLogs((logsText: string) => {
      if (destroyed) return;
      console.log('[LOGS-DIAG] onLogs received, length=' + logsText.length);

      const trimmed = logsText.trim();
      if (trimmed === '(no events)' || trimmed === '(no events)\n') {
        logLinesRef.current = [
          '(нет событий) — устройство не зафиксировало событий с момента запуска',
        ];
      } else {
        logLinesRef.current = logsText.split('\n').filter((line) => line.length > 0);
      }

      setError(null);
      setLoading(false);
      setRenderVersion((v) => v + 1);
    });

    const unsubEvent = MqttService.onSingleEvent((event) => {
      if (destroyed) return;
      console.log('[LOGS-DIAG] onSingleEvent received: ' + event.ts + ' - ' + event.desc);
      const line = `${event.ts} - ${event.desc}`;
      logLinesRef.current.push(line);
      setRenderVersion((v) => v + 1);
    });

    const unsubConn = MqttService.onConnectionChange((connected: boolean) => {
      if (destroyed) return;
      console.log('[LOGS-DIAG] onConnectionChange called with connected=' + connected);
      setMqttConnected(connected);
      if (connected) {
        console.log('[LOGS-DIAG] onConnectionChange=true, sending getlogs');
        MqttService.publish('getlogs', '1');
      }
    });

    if (MqttService.connected) {
      console.log('[LOGS-DIAG] MqttService.connected is true at mount, setting state and sending getlogs');
      setMqttConnected(true);
      MqttService.publish('getlogs', '1');
    } else {
      console.log('[LOGS-DIAG] MqttService.connected is false at mount, waiting for onConnectionChange');
    }

    return () => {
      destroyed = true;
      console.log('[LOGS-DIAG] LogsScreen unmounted');
      unsubLogs();
      unsubEvent();
      unsubConn();
    };
  }, []);

  // --- Авто-скролл ---
  useLayoutEffect(() => {
    if (logLinesRef.current.length > 0 && isNearBottomRef.current) {
      const timer = setTimeout(() => {
        flatListRef.current?.scrollToEnd({ animated: true });
      }, 50);
      return () => clearTimeout(timer);
    }
  }, [renderVersion]);

  // --- Обработчик скролла ---
  const handleScroll = useCallback((e: any) => {
    const { contentOffset, contentSize, layoutMeasurement } = e.nativeEvent;
    const isNearBottom =
      contentOffset.y + layoutMeasurement.height >= contentSize.height - 60;
    isNearBottomRef.current = isNearBottom;
    setShowScrollDown(!isNearBottom);
  }, []);

  // --- Скролл вниз по кнопке ---
  const scrollToBottom = useCallback(() => {
    isNearBottomRef.current = true;
    setShowScrollDown(false);
    flatListRef.current?.scrollToEnd({ animated: true });
  }, []);

  // --- Открыть веб-интерфейс ---
  const openWebInterface = useCallback(() => {
    if (!deviceIp) {
      Alert.alert(
        'IP не известен',
        'IP устройства пока не получен. Дождитесь телеметрии на главном экране.'
      );
      return;
    }
    Linking.openURL(`http://${deviceIp}/`).catch(() => {
      Alert.alert('Ошибка', 'Не удалось открыть браузер');
    });
  }, [deviceIp]);

  // --- Обновить логи ---
  const handleRefresh = useCallback(() => {
    setLoading(true);
    setError(null);
    logLinesRef.current = [];
    setRenderVersion((v) => v + 1);
    isNearBottomRef.current = true;

    if (MqttService.connected) {
      MqttService.publish('getlogs', '1');
    } else {
      setError('MQTT не подключён. Проверьте настройки подключения.');
      setLoading(false);
    }
  }, []);

  // --- Рендер строки лога ---
  const renderLogItem = useCallback(
    ({ item }: { item: string }) => (
      <Text style={styles.logText} selectable>
        {item}
      </Text>
    ),
    []
  );

  const keyExtractor = useCallback((item: string, index: number) => String(index), []);

  return (
    <View style={styles.container}>
      {/* Верхняя карточка с информацией */}
      <Card style={styles.headerCard}>
        <Card.Content>
          <Title>Логи устройства</Title>
          {deviceIp && (
            <Chip icon="wifi" style={styles.chip}>
              IP устройства: {deviceIp}
            </Chip>
          )}
          <Chip
            icon={mqttConnected ? 'check-circle' : 'alert'}
            style={[
              styles.chip,
              mqttConnected ? styles.chipConnected : styles.chipDisconnected,
            ]}
          >
            {mqttConnected ? 'MQTT подключён' : 'MQTT не подключён'}
          </Chip>
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

      {/* Индикатор загрузки */}
      {loading && (
        <View style={styles.loadingContainer}>
          <ActivityIndicator size="large" />
          <Text style={styles.loadingText}>
            {mqttConnected ? 'Загрузка логов...' : 'Подключение к MQTT...'}
          </Text>
        </View>
      )}

      {/* Ошибка */}
      {error && (
        <Card style={[styles.card, styles.errorCard]}>
          <Card.Content>
            <Text style={styles.errorText}>{error}</Text>
            <Button
              mode="contained"
              onPress={handleRefresh}
              style={styles.retryButton}
              icon="refresh"
            >
              Повторить
            </Button>
          </Card.Content>
        </Card>
      )}

      {/* Список логов — FlatList для производительности */}
      {!loading && !error && (
        <View style={styles.logContainer}>
          <View style={styles.logHeader}>
            <Title>События</Title>
            <Text style={styles.logLineCount}>
              {logLinesRef.current.length} строк
            </Text>
          </View>
          <FlatList
            ref={flatListRef}
            data={logLinesRef.current}
            keyExtractor={keyExtractor}
            renderItem={renderLogItem}
            style={styles.logScroll}
            initialNumToRender={100}
            maxToRenderPerBatch={50}
            windowSize={5}
            onScroll={handleScroll}
            scrollEventThrottle={100}
          />

          {/* Плавающая кнопка "↓ Вниз" */}
          {showScrollDown && (
            <TouchableOpacity
              style={styles.scrollDownButton}
              onPress={scrollToBottom}
              activeOpacity={0.7}
            >
              <Text style={styles.scrollDownText}>↓ Вниз</Text>
            </TouchableOpacity>
          )}

          <Button
            mode="outlined"
            onPress={handleRefresh}
            icon="refresh"
            style={styles.refreshButton}
          >
            Обновить
          </Button>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  headerCard: { margin: 16 },
  card: { margin: 16 },
  errorCard: { backgroundColor: '#ffebee' },
  chip: { marginVertical: 4 },
  chipConnected: { backgroundColor: '#e8f5e9' },
  chipDisconnected: { backgroundColor: '#ffebee' },
  button: { marginVertical: 8 },
  retryButton: { marginTop: 12 },
  loadingContainer: { alignItems: 'center', padding: 32 },
  loadingText: { marginTop: 12, color: '#666' },
  logHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  logLineCount: { color: '#666', fontSize: 14 },
  logContainer: {
    flex: 1,
    marginHorizontal: 16,
    marginBottom: 16,
    backgroundColor: '#fff',
    borderRadius: 8,
    elevation: 2,
    padding: 16,
  },
  logScroll: { flex: 1 },
  logText: {
    fontFamily: 'monospace',
    fontSize: 12,
    lineHeight: 18,
    color: '#333',
  },
  errorText: { color: '#c62828', fontSize: 14, lineHeight: 20 },
  refreshButton: { marginTop: 8 },
  scrollDownButton: {
    position: 'absolute',
    bottom: 60,
    right: 16,
    backgroundColor: '#2196F3',
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 20,
    elevation: 4,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.3,
    shadowRadius: 4,
  },
  scrollDownText: {
    color: '#fff',
    fontSize: 14,
    fontWeight: 'bold',
  },
});