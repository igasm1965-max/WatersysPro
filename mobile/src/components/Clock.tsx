/**
 * @file Clock.tsx
 * @brief Часы/дата с собственным таймером.
 *
 * Отдельный компонент, чтобы секундный setInterval перерисовывал только сам
 * индикатор времени, а не весь DashboardScreen.
 */
import React, { useEffect, useState } from 'react';
import { View, Text, StyleSheet } from 'react-native';

function formatDate(d: Date): string {
  const days = ['Вс', 'Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб'];
  const months = ['янв', 'фев', 'мар', 'апр', 'май', 'июн', 'июл', 'авг', 'сен', 'окт', 'ноя', 'дек'];
  return `${days[d.getDay()]}, ${d.getDate()} ${months[d.getMonth()]} ${d.getFullYear()}`;
}

function formatTime(d: Date): string {
  const h = d.getHours().toString().padStart(2, '0');
  const m = d.getMinutes().toString().padStart(2, '0');
  const s = d.getSeconds().toString().padStart(2, '0');
  return `${h}:${m}:${s}`;
}

export default function Clock() {
  const [now, setNow] = useState(() => new Date());

  useEffect(() => {
    const id = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(id);
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.time}>{formatTime(now)}</Text>
      <Text style={styles.date}>{formatDate(now)}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { alignItems: 'center', paddingVertical: 8, paddingHorizontal: 16 },
  time: {
    fontSize: 28,
    fontWeight: '300',
    fontFamily: 'monospace',
    color: '#1e293b',
    letterSpacing: 2,
  },
  date: { fontSize: 14, color: '#64748b', marginTop: 2 },
});