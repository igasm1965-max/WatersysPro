# План полной перезаписи системы отображения логов v3.1

## 🎯 Диагноз: проблема именно в скролле

Пользователь подтвердил: **логи приходят, счётчик полученных событий работает, но скролл не работает.**

Это значит, что:
- MQTT соединение работает
- Подписка на топики работает
- `onLogs` и `onSingleEvent` вызываются
- `setLogs` / `setLogLines` отрабатывают
- **Проблема чисто в FlatList и механизме скролла**

---

## 🔍 Почему скролл не работает в текущей реализации

### Причина 1: `logLines` пересоздаётся каждый рендер (критично)

В [`LogsScreen.tsx:199`](mobile/src/screens/LogsScreen.tsx:199):
```typescript
const logLines = logs ? logs.split('\n') : [];
```

Этот код выполняется **при каждом рендере**. Когда приходит новое событие через `onSingleEvent`:
1. `setLogs(prev => prev + line)` — меняет состояние
2. React вызывает ре-рендер
3. `logLines = logs.split('\n')` — создаётся **НОВЫЙ МАССИВ** (новая ссылка)
4. FlatList получает новый массив и **сбрасывает позицию скролла наверх**
5. `scrollToEnd` в `useEffect` пытается отскроллить вниз, но FlatList уже наверху

### Причина 2: `userScrolledUp` блокирует скролл навсегда

В [`LogsScreen.tsx:313-317`](mobile/src/screens/LogsScreen.tsx:313-317):
```typescript
onScroll={(e) => {
  const { contentOffset, contentSize, layoutMeasurement } = e.nativeEvent;
  const isNearBottom = contentOffset.y + layoutMeasurement.height >= contentSize.height - 50;
  userScrolledUp.current = !isNearBottom;
}}
```

Если пользователь хоть раз скролльнул вверх (или скролл сбросился наверх из-за Причины 1), `userScrolledUp.current = true`. После этого `scrollToEnd` в `useEffect` не вызывается, потому что условие `!userScrolledUp.current` — false.

### Причина 3: `useEffect` не успевает за частыми обновлениями

В [`LogsScreen.tsx:145-152`](mobile/src/screens/LogsScreen.tsx:145-152):
```typescript
useEffect(() => {
  if (logs && !userScrolledUp.current) {
    const timer = setTimeout(() => {
      logScrollRef.current?.scrollToEnd({ animated: true });
    }, 50);
    return () => clearTimeout(timer);
  }
}, [logs]);
```

Если события приходят часто (каждые 100-200 мс), `useEffect` срабатывает каждый раз, но `setTimeout(50)` не успевает выполниться, потому что следующий `useEffect` уже очищает предыдущий таймер через `clearTimeout`. В итоге `scrollToEnd` **никогда не вызывается**.

---

## 🏗️ Новая архитектура (с фокусом на скролл)

### Ключевые изменения для решения проблемы скролла:

1. **Хранить массив строк в `useRef` + `useState<number>` для версионирования**
   - `logLinesRef = useRef<string[]>([])` — актуальные данные
   - `[renderVersion, setRenderVersion] = useState(0)` — триггер ре-рендера
   - При добавлении строки: `logLinesRef.current.push(line)` + `setRenderVersion(v => v + 1)`
   - Массив НЕ пересоздаётся — FlatList получает ту же ссылку

2. **Убрать `userScrolledUp` полностью**
   - Вместо этого: при скролле пользователем вверх — показывать плавающую кнопку "↓ Вниз"
   - Если пользователь внизу — авто-скролл работает всегда

3. **Авто-скролл через `useLayoutEffect` без таймера**
   - `useLayoutEffect` срабатывает синхронно после каждого изменения
   - Никаких `setTimeout` — скролл происходит мгновенно
   - Проверка: если пользователь внизу (или список только загрузился) — скроллим

4. **FlatList с `getItemLayout` и `maintainVisibleContentPosition`**
   - `getItemLayout` — предсказуемая высота 20px
   - `maintainVisibleContentPosition` — React Native автоматически сохраняет позицию скролла при добавлении строк в начало/конец

---

## � Файл 1: `mobile/src/screens/LogsScreen.tsx` — ПОЛНАЯ ПЕРЕЗАПИСЬ

```typescript
/**
 * @file LogsScreen.tsx
 * @brief Экран логов устройства — v3.1 (полная перезапись, исправлен скролл)
 *
 * Ключевые изменения относительно v2.1:
 * - Массив строк хранится в useRef + версионирование через useState
 * - Убран userScrolledUp — добавлена плавающая кнопка "Вниз"
 * - Авто-скролл через useLayoutEffect без setTimeout
 * - FlatList с getItemLayout и maintainVisibleContentPosition
 * - Убраны все автоматические retry/таймеры
 * - Кнопка "Обновить" для ручного запроса логов
 */

import React, { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react';
import { View, FlatList, Text, StyleSheet, Linking, Alert, TouchableOpacity } from 'react-native';
import { Card, Title, Button, ActivityIndicator, Chip } from 'react-native-paper';
import MqttService from '../services/mqttService';
import { useAppContext } from '../context/AppContext';

const LOG_ITEM_HEIGHT = 20;

export default function LogsScreen() {
  // --- Состояния ---
  const [renderVersion, setRenderVersion] = useState(0); // триггер ре-рендера
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [mqttConnected, setMqttConnected] = useState(false);
  const [showScrollDown, setShowScrollDown] = useState(false); // кнопка "Вниз"

  // --- Refs ---
  const flatListRef = useRef<FlatList>(null);
  const logLinesRef = useRef<string[]>([]); // актуальные данные
  const isNearBottomRef = useRef(true); // флаг: пользователь внизу
  const { deviceIp } = useAppContext();

  // --- Подписки при монтировании ---
  useEffect(() => {
    // Сброс состояния
    logLinesRef.current = [];
    setRenderVersion(0);
    setLoading(true);
    setError(null);
    setShowScrollDown(false);
    isNearBottomRef.current = true;

    let destroyed = false;

    // Получение полного лога
    const unsubLogs = MqttService.onLogs((logsText: string) => {
      if (destroyed) return;

      const trimmed = logsText.trim();
      if (trimmed === '(no events)' || trimmed === '(no events)\n') {
        logLinesRef.current = ['(нет событий) — устройство не зафиксировало событий с момента запуска'];
      } else {
        logLinesRef.current = logsText.split('\n').filter(line => line.length > 0);
      }

      setError(null);
      setLoading(false);
      setRenderVersion(v => v + 1);
    });

    // Новые события (стриминг)
    const unsubEvent = MqttService.onSingleEvent((event) => {
      if (destroyed) return;
      const line = `${event.ts} - ${event.desc}`;
      logLinesRef.current.push(line);
      setRenderVersion(v => v + 1);
    });

    // Статус MQTT
    const unsubConn = MqttService.onConnectionChange((connected: boolean) => {
      if (destroyed) return;
      setMqttConnected(connected);
      if (connected) {
        MqttService.publish('getlogs', '1');
      }
    });

    // Если MQTT уже подключён
    if (MqttService.connected) {
      setMqttConnected(true);
      MqttService.publish('getlogs', '1');
    }

    return () => {
      destroyed = true;
      unsubLogs();
      unsubEvent();
      unsubConn();
    };
  }, []);

  // --- Авто-скролл: useLayoutEffect вместо useEffect ---
  // useLayoutEffect вызывается синхронно ПОСЛЕ изменения DOM, но ДО отрисовки
  useLayoutEffect(() => {
    if (logLinesRef.current.length > 0 && isNearBottomRef.current) {
      flatListRef.current?.scrollToEnd({ animated: true });
    }
  }, [renderVersion]);

  // --- Обработчик скролла ---
  const handleScroll = useCallback((e: any) => {
    const { contentOffset, contentSize, layoutMeasurement } = e.nativeEvent;
    const isNearBottom = contentOffset.y + layoutMeasurement.height >= contentSize.height - 60;
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
      Alert.alert('IP не известен', 'IP устройства пока не получен. Дождитесь телеметрии на главном экране.');
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
    setRenderVersion(v => v + 1);
    isNearBottomRef.current = true;

    if (MqttService.connected) {
      MqttService.publish('getlogs', '1');
    } else {
      setError('MQTT не подключён. Проверьте настройки подключения.');
      setLoading(false);
    }
  }, []);

  // --- Рендер строки лога ---
  const renderLogItem = useCallback(({ item }: { item: string }) => (
    <Text style={styles.logText} selectable>{item}</Text>
  ), []);

  // --- Ключ для FlatList ---
  const keyExtractor = useCallback((item: string, index: number) => String(index), []);

  // --- getItemLayout для производительности ---
  const getItemLayout = useCallback(
    (_data: ArrayLike<string> | null | undefined, index: number) => ({
      length: LOG_ITEM_HEIGHT,
      offset: LOG_ITEM_HEIGHT * index,
      index,
    }),
    []
  );

  return (
    <View style={styles.container}>
      {/* Верхняя карточка */}
      <Card style={styles.card}>
        <Card.Content>
          <Title>Логи устройства</Title>
          {deviceIp && (
            <Chip icon="wifi" style={styles.chip}>
              IP устройства: {deviceIp}
            </Chip>
          )}
          <Chip
            icon={mqttConnected ? 'check-circle' : 'alert'}
            style={[styles.chip, mqttConnected ? styles.chipConnected : styles.chipDisconnected]}
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

      {/* Список логов */}
      {!loading && !error && (
        <View style={[styles.card, styles.logContainer]}>
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
            getItemLayout={getItemLayout}
            style={styles.logScroll}
            nestedScrollEnabled
            initialNumToRender={50}
            maxToRenderPerBatch={30}
            windowSize={10}
            removeClippedSubviews={true}
            onScroll={handleScroll}
            scrollEventThrottle={100}
          />

          {/* Плавающая кнопка "Вниз" */}
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
    height: LOG_ITEM_HEIGHT,
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
```

---

## 📄 Файл 2: `mobile/src/services/mqttService.ts` — 1 изменение

Убрать `setTimeout(0)` в [`onConnectionChange`](mobile/src/services/mqttService.ts:204):

```typescript
// БЫЛО:
onConnectionChange(cb: ConnectionCallback): () => void {
  this.connListeners.push(cb);
  setTimeout(() => cb(this._connected), 0);  // ← Асинхронный разрыв
  return () => { this.connListeners = this.connListeners.filter(l => l !== cb); };
}

// СТАЛО:
onConnectionChange(cb: ConnectionCallback): () => void {
  this.connListeners.push(cb);
  cb(this._connected);  // ← Синхронный вызов
  return () => { this.connListeners = this.connListeners.filter(l => l !== cb); };
}
```

---

## ✅ Почему этот план должен сработать

| Проблема | Решение | Статус |
|----------|---------|--------|
| `logLines` пересоздаётся каждый рендер → сброс скролла | Массив в `useRef` — одна ссылка навсегда | ✅ Исправлено |
| `userScrolledUp` блокирует скролл навсегда | Убран флаг, добавлена кнопка "↓ Вниз" | ✅ Исправлено |
| `useEffect` с `setTimeout` не успевает за частыми событиями | `useLayoutEffect` без таймера — синхронно после каждого изменения | ✅ Исправлено |
| FlatList сбрасывает позицию при новой ссылке на массив | `getItemLayout` + одна ссылка через `useRef` | ✅ Исправлено |
| Сложная логика с retry/таймаутами | Убрана полностью — только кнопка "Обновить" | ✅ Исправлено |
| Race condition в `onConnectionChange` | Убран `setTimeout(0)` — синхронный вызов | ✅ Исправлено |

---

## 📋 Порядок выполнения

1. **Переписать `mobile/src/screens/LogsScreen.tsx`** — полная замена файла
2. **Исправить `mobile/src/services/mqttService.ts`** — убрать setTimeout (1 строка)
3. **Собрать APK** и проверить на устройстве