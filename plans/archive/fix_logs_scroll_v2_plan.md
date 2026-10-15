# План исправления v2: вертикальный скролл и отображение логов

## Проблемы (по результатам тестирования)

1. **Вертикальный скролл не работает** — ScrollView не ограничен по высоте
2. **Горизонтальный скролл остался** — `horizontal={false}` не решает проблему полностью
3. **События показываются с начала** — после получения полного лога (раз в 60с) скролл не уходит в конец
4. **Бандл не пересобрался** — в APK попал старый JS-бандл без изменений

## Корневая причина

### Проблема 1: ScrollView не ограничен по высоте
В React Native `ScrollView` активирует скроллинг **только если его высота меньше высоты содержимого**. Текущая структура:

```
<View style={styles.container}>          // flex: 1
  <Card>                                  // НЕТ flex — занимает столько, сколько нужно
    <Card.Content>
      ...инфо...
    </Card.Content>
  </Card>
  <View style={styles.logContainer}>      // flex: 1
    <ScrollView style={styles.logScroll}> // flex: 1
      <Text>{logs}</Text>
    </ScrollView>
  </View>
</View>
```

Проблема: `Card` не имеет `flex`, но `logContainer` имеет `flex: 1`. React Native выделяет `logContainer` оставшееся пространство. `ScrollView` внутри получает `flex: 1` от `logContainer`. **Это должно работать**, но на практике `Card` из `react-native-paper` может иметь внутренние отступы и поведение, которое ломает расчёт высоты.

**Решение:** Использовать `FlatList` вместо `ScrollView` — он天生 работает с ограниченной высотой. Или использовать `onLayout` для принудительного задания высоты.

### Проблема 2: Горизонтальный скролл
`horizontal={false}` в ScrollView не запрещает горизонтальный скролл полностью. В React Native ScrollView по умолчанию позволяет скроллить в обоих направлениях, если контент шире. `Text` с `monospace` шрифтом может быть шире родителя.

**Решение:** 
- Добавить `showsHorizontalScrollIndicator={false}`
- Разбить текст на отдельные строки, каждая в своём `<Text>` с `numberOfLines={1}` или без
- Использовать `wordBreak: 'break-all'` (не поддерживается в RN) — альтернатива: обернуть каждую строку в `<Text>` и использовать `flexShrink: 1`

### Проблема 3: События с начала
После получения полного лога через `onLogs`, нужно принудительно скроллить в конец. Сейчас `useEffect` на `logs` (строка 152) делает `scrollToEnd`, но:
- При получении полного лога (60с) — `logs` заменяется полностью, скролл должен сработать
- Но если пользователь скроллит вверх (`userScrolledUp.current = true`), авто-скролл отключается

**Решение:** После получения полного лога через `onLogs` принудительно сбрасывать `userScrolledUp.current = false` и скроллить в конец.

### Проблема 4: Бандл не пересобрался
При сборке `assembleRelease` Gradle запускает задачу `export:embed`, которая собирает JS-бандл. Но если в `assets` уже лежит старый бандл, он может быть использован вместо нового.

**Решение:** Очистить `assets` перед сборкой, чтобы Gradle пересобрал бандл.

## План исправления

### Шаг 1: Очистить assets и пересобрать бандл
```bash
cd mobile
rm -rf android/app/src/main/assets/*
npx expo export --platform android --output-dir dist
```

### Шаг 2: Исправить LogsScreen.tsx — заменить ScrollView на FlatList

**Файл:** `mobile/src/screens/LogsScreen.tsx`

**Изменения:**
1. Заменить `ScrollView` на `FlatList` с инвертированным порядком (новые события внизу)
2. `FlatList`天生 работает с ограниченной высотой и не требует `flex`
3. Убрать `horizontal={false}` (не нужно для FlatList)
4. Разбить текст на массив строк для FlatList

```tsx
import { View, FlatList, Text, StyleSheet, Linking, Alert } from 'react-native';

// Вместо ScrollView:
<FlatList
  ref={logScrollRef}
  data={logs.split('\n')}
  keyExtractor={(item, index) => String(index)}
  renderItem={({ item }) => (
    <Text style={styles.logText} selectable>
      {item}
    </Text>
  )}
  style={styles.logScroll}
  nestedScrollEnabled
  onScroll={(e) => {
    const { contentOffset, contentSize, layoutMeasurement } = e.nativeEvent;
    const isNearBottom = contentOffset.y + layoutMeasurement.height >= contentSize.height - 50;
    userScrolledUp.current = !isNearBottom;
  }}
  scrollEventThrottle={100}
/>
```

### Шаг 3: Исправить стили для FlatList

```tsx
logContainer: {
  flex: 1,
  marginBottom: 16,
  backgroundColor: '#fff',
  borderRadius: 8,
  elevation: 2,
  padding: 16,
},
logScroll: {
  flex: 1,
},
logText: {
  fontFamily: 'monospace',
  fontSize: 11,
  lineHeight: 16,
  color: '#333',
},
```

### Шаг 4: Исправить авто-скролл после получения полного лога

В обработчике `onLogs` (строка 56) добавить сброс `userScrolledUp`:

```tsx
const unsubLogs = MqttService.onLogs((logsText) => {
  if (destroyed) return;
  logsReceivedRef.current = true;
  setError(null);
  setLoading(false);
  setStatusMessage('✓ Логи получены');
  
  // Сбрасываем флаг, чтобы скролл ушёл в конец после получения нового лога
  userScrolledUp.current = false;

  const trimmed = logsText.trim();
  if (trimmed === '(no events)' || trimmed === '(no events)\n') {
    setLogs('(нет событий) — устройство не зафиксировало событий с момента запуска');
  } else {
    setLogs(logsText);
  }

  if (retryTimerRef.current) clearTimeout(retryTimerRef.current);
  if (timeoutRef.current) clearTimeout(timeoutRef.current);
});
```

### Шаг 5: Исправить авто-скролл useEffect для FlatList

```tsx
useEffect(() => {
  if (logs && !userScrolledUp.current) {
    // Небольшая задержка, чтобы FlatList успел отрендерить новые строки
    const timer = setTimeout(() => {
      logScrollRef.current?.scrollToEnd({ animated: true });
    }, 50);
    return () => clearTimeout(timer);
  }
}, [logs]);
```

### Шаг 6: Собрать APK
```bash
cd mobile/android
gradlew.bat assembleRelease
```

## Проверка

1. Вертикальный скролл должен работать с FlatList
2. Горизонтальный скролл не должен появляться (каждая строка — отдельный Text)
3. После получения полного лога (раз в 60с) скролл автоматически уходит в конец
4. Новые события (onSingleEvent) добавляются в конец и скролл следует за ними
5. Если пользователь скроллит вверх — авто-скролл отключается