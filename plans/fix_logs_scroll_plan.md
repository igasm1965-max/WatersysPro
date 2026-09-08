# План исправления: вертикальный скролл логов в мобильном приложении

## Проблема

На экране логов (`LogsScreen.tsx`) текст логов не прокручивается вертикально — работает только горизонтальный скролл. Новые события добавляются (счётчик строк растёт), но они не видны, так как `ScrollView` не скроллится вниз.

## Корневая причина

В React Native `ScrollView` **обязательно должен иметь ограниченную высоту**, чтобы скроллинг работал. Если высота `ScrollView` не ограничена, он растягивается по содержимому, и скроллинг не активируется.

В текущей реализации:

```
<Card style={[styles.card, styles.logCard]}>          // flex: 1
  <Card.Content style={styles.logCardContent}>         // flex: 1
    <ScrollView style={styles.logScroll}>              // flex: 1
      <Text style={styles.logText}>{logs}</Text>
    </ScrollView>
  </Card.Content>
</Card>
```

Проблема в том, что:
1. `Card` из `react-native-paper` **не передаёт `flex`** своим дочерним элементам корректно — у него есть внутренние отступы и своя вёрстка.
2. `Card.Content` — это просто `View` с `padding`, который тоже не ограничивает высоту.
3. В результате `ScrollView` получает неограниченную высоту, растягивается под весь текст, и скроллинг не работает.

Дополнительно: **горизонтальный скролл** появляется, потому что `Text` с `fontFamily: 'monospace'` может быть шире контейнера, а `flexWrap: 'wrap'` не срабатывает, если у текста нет ограничения по ширине.

## План исправления

### Шаг 1: Изменить структуру компонента — заменить Card на View с фиксированной высотой

**Файл:** `mobile/src/screens/LogsScreen.tsx`

**Что изменить:**
- Заменить внешний `Card` (строки 298-327) на обычный `View` с явно заданной высотой через `flex`.
- Убрать `Card.Content` — использовать `View` напрямую.
- `ScrollView` должен быть внутри `View` с **фиксированной/ограниченной высотой**.

**Конкретные изменения:**

```tsx
{logs !== null && (
  <View style={[styles.card, styles.logContainer]}>
    <View style={styles.logHeader}>
      <Title>События</Title>
      <Text style={styles.logLineCount}>
        {logs.split('\n').length} строк
      </Text>
    </View>
    <Text style={styles.autoUpdateHint}>
      Логи обновляются автоматически в реальном времени
    </Text>
    <ScrollView
      ref={logScrollRef}
      style={styles.logScroll}
      nestedScrollEnabled
      onScroll={(e) => {
        const { contentOffset, contentSize, layoutMeasurement } = e.nativeEvent;
        const isNearBottom = contentOffset.y + layoutMeasurement.height >= contentSize.height - 50;
        userScrolledUp.current = !isNearBottom;
      }}
      scrollEventThrottle={100}
    >
      <Text style={styles.logText} selectable>
        {logs}
      </Text>
    </ScrollView>
  </View>
)}
```

### Шаг 2: Исправить стили

**Файл:** `mobile/src/screens/LogsScreen.tsx`

**Что изменить в стилях:**

1. **Удалить** `logCard` и `logCardContent` — они больше не нужны.
2. **Добавить** `logContainer` — контейнер с ограниченной высотой:

```tsx
logContainer: {
  flex: 1,
  margin: 16,
  marginBottom: 16,
  backgroundColor: '#fff',
  borderRadius: 8,
  elevation: 2,
  padding: 16,
},
```

3. **Изменить `logScroll`** — убрать `flex: 1`, добавить явную высоту через `flexGrow`:

```tsx
logScroll: {
  flex: 1,
  minHeight: 200,  // Минимальная высота, чтобы ScrollView точно был ограничен
},
```

4. **Исправить `logText`** — убрать `flexWrap: 'wrap'` (он не работает в ScrollView), добавить явную ширину:

```tsx
logText: {
  fontFamily: 'monospace',
  fontSize: 11,
  lineHeight: 16,
  color: '#333',
},
```

### Шаг 3: Исправить авто-скролл к новым событиям

**Файл:** `mobile/src/screens/LogsScreen.tsx`

**Что изменить:**

Текущий код (строки 152-159) использует `scrollToEnd`, но с задержкой 100ms, что может не срабатывать при быстром добавлении событий.

```tsx
// Авто-скролл к последним событиям
useEffect(() => {
  if (logs && !userScrolledUp.current) {
    // Используем requestAnimationFrame вместо setTimeout для более точного момента
    // когда ScrollView уже отрендерил новый контент
    const raf = requestAnimationFrame(() => {
      logScrollRef.current?.scrollToEnd({ animated: true });
    });
    return () => cancelAnimationFrame(raf);
  }
}, [logs]);
```

### Шаг 4: Исправить горизонтальный скролл (если останется)

Если после шагов 1-3 горизонтальный скролл всё ещё появляется (из-за длинных строк логов), добавить:

```tsx
<ScrollView
  ...
  horizontal={false}  // Явно запрещаем горизонтальный скролл
>
  <Text style={styles.logText} selectable>
    {logs}
  </Text>
</ScrollView>
```

И в стили `logText` добавить:

```tsx
logText: {
  ...
  // Принудительный перенос длинных строк
  // wordBreak: 'break-all' не поддерживается в RN, используем альтернативу
},
```

В React Native для переноса длинных строк в `Text` нужно использовать:

```tsx
<Text style={styles.logText} selectable>
  {logs}
</Text>
```

В React Native `Text` автоматически переносит строки, если у него есть ограничение по ширине. Если строка не переносится, можно обернуть каждую строку в отдельный `<Text>`:

```tsx
<ScrollView ...>
  {logs.split('\n').map((line, i) => (
    <Text key={i} style={styles.logText} selectable>
      {line}
      {'\n'}
    </Text>
  ))}
</ScrollView>
```

Это гарантирует перенос каждой строки независимо.

## Итоговый список изменений в LogsScreen.tsx

| № | Строки | Изменение |
|---|--------|-----------|
| 1 | 298-327 | Заменить `Card` + `Card.Content` на `View` с ограниченной высотой |
| 2 | 332-356 | Обновить стили: убрать `logCard`, `logCardContent`, добавить `logContainer`, исправить `logScroll` и `logText` |
| 3 | 152-159 | Заменить `setTimeout` на `requestAnimationFrame` в авто-скролле |
| 4 | 310-324 | Добавить `horizontal={false}` в ScrollView |
| 5 | 321-323 | Разбить текст на отдельные строки для гарантированного переноса |

## Проверка

После внесения изменений:
1. Вертикальный скролл должен работать — новые строки логов будут видны
2. Авто-скролл к новым событиям должен работать плавно
3. Горизонтальный скролл не должен появляться
4. Счётчик строк должен продолжать корректно отображаться