/**
 * @file DashboardScreen.tsx
 * @brief Главный экран с телеметрией устройства (v1.3.6)
 *
 * Изменения в версии 1.3.6:
 * - Добавлено сохранение IP устройства в глобальный контекст (AppContext)
 * - IP используется на LogsScreen для кнопки "Открыть веб-интерфейс"
 *
 * @see plans/logs_screen_redesign_plan.md
 */

import React, { useEffect, useRef, useState } from 'react';
import { View, ScrollView, RefreshControl, Text, Alert, StyleSheet } from 'react-native';
import { Card, Title, Paragraph, Button, ActivityIndicator, Chip } from 'react-native-paper';
import MqttService from '../services/mqttService';
import { useAppContext } from '../context/AppContext';
import { Telemetry } from '../types';
import { colors, spacing } from '../theme';
import Clock from '../components/Clock';

export default function DashboardScreen() {
  const [status, setStatus] = useState<Telemetry | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [mqttConnected, setMqttConnected] = useState(false);
  // Baseline для Alert аварии: null = первое полученное значение (не показываем),
  // далее Alert только на фронте false→true.
  const prevEmergencyRef = useRef<boolean | null>(null);
  const { setDeviceIp } = useAppContext();

  const relayTargets = ['pump1', 'pump2', 'aeration', 'ozone', 'filter', 'backwash'];

  const toInt = (v: any) => {
    const n = typeof v === 'number' ? v : parseInt(String(v || 0), 10);
    if (!Number.isFinite(n) || Number.isNaN(n)) return 0;
    return Math.max(0, n);
  };

  const fmt2 = (n: any) => {
    const v = toInt(n);
    return v < 10 ? `0${v}` : String(v);
  };

  const fmtHMS = (sec: any) => {
    const s = toInt(sec);
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const ss = s % 60;
    return `${fmt2(h)}:${fmt2(m)}:${fmt2(ss)}`;
  };

  // Format days+HH:MM:SS for long intervals (Wash period up to 7 days)
  const fmtDHMS = (sec: any) => {
    const s = toInt(sec);
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    const m = Math.floor((s % 3600) / 60);
    const ss = s % 60;
    if (d > 0) return `${d}д ${fmt2(h)}:${fmt2(m)}:${fmt2(ss)}`;
    return `${fmt2(h)}:${fmt2(m)}:${fmt2(ss)}`;
  };



  // Alert when emergency becomes active (только фронт false→true, без срабатывания
  // при первом полученном значении и при повторном монтировании экрана)
  useEffect(() => {
    const isActive = !!status?.emergency?.active;
    const prev = prevEmergencyRef.current;
    if (prev === false && isActive) {
      Alert.alert('АВАРИЯ', status?.emergency?.message || 'Аварийный режим активирован', [{ text: 'OK' }]);
    }
    prevEmergencyRef.current = isActive;
  }, [status?.emergency?.active]);

  // Подписки на события MQTT (само подключение централизовано в App.tsx)
  useEffect(() => {
    let unsubTelemetry: (() => void) | null = null;
    let unsubConn: (() => void) | null = null;

    unsubConn = MqttService.onConnectionChange(setMqttConnected);
    unsubTelemetry = MqttService.onTelemetry((data) => {
      setStatus(data);
      // v1.3.6: Сохраняем IP устройства в глобальный контекст для LogsScreen
      // IP используется для кнопки "Открыть веб-интерфейс" на экране логов
      // Поле ip приходит в MQTT телеметрии от ESP32
      const ip = data?.ip || data?.device_ip || null;
      if (ip) {
        setDeviceIp(ip);
      }
    });

    const timeout = setTimeout(() => {
      setLoading(false);
    }, 10000);

    return () => {
      clearTimeout(timeout);
      if (unsubTelemetry) unsubTelemetry();
      if (unsubConn) unsubConn();
      // MQTT не отключаем — он нужен для LogsScreen и других экранов
    };
  }, []);

  const onRefresh = () => {
    setRefreshing(true);
    // Telemetry arrives automatically; just clear the spinner after a moment
    setTimeout(() => setRefreshing(false), 1500);
  };

  const toggleRelay = (target: string) => {
    if (status?.control_mode !== 'manual') {
      alert('Реле можно переключать только в ручном режиме.');
      return;
    }
    if (!mqttConnected) {
      alert('Нет подключения к MQTT брокеру.');
      return;
    }
    const next = !status?.relays?.[target];
    // Optimistic UI
    setStatus((prev) => ({
      ...(prev ?? {}),
      relays: { ...(prev?.relays || {}), [target]: next ? 1 : 0 },
    }) as Telemetry);
    MqttService.publish(target, next ? 'on' : 'off');
  };

  const setControlMode = (mode: 'manual' | 'auto') => {
    if (!mqttConnected) { alert('Нет подключения к MQTT брокеру.'); return; }
    MqttService.publish('set_mode', mode);
    setStatus((prev) => ({ ...(prev || {}), control_mode: mode }) as Telemetry);
  };

  const resetEmergency = () => {
    if (!mqttConnected) { alert('Нет подключения к MQTT брокеру.'); return; }
    MqttService.publish('reset_emergency', '1');
  };

  if (loading) return (
    <View style={styles.centered}>
      <ActivityIndicator size="large" />
      <Paragraph style={styles.topGap}>Подключение к MQTT...</Paragraph>
    </View>
  );
  if (!status) return (
    <View style={[styles.centered, styles.centeredWide]}>
      <Title style={styles.centerAlign}>Нет данных</Title>
      <Paragraph style={[styles.centerAlign, styles.topGap]}>
        {mqttConnected ? 'Ожидание телеметрии от устройства...' : 'Нет подключения к MQTT брокеру m1.wqtt.ru'}
      </Paragraph>
    </View>
  );

  return (
    <ScrollView refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />} style={styles.screen}>
      {/* MQTT status bar */}
      <View style={styles.statusBar}>
        <View style={[styles.statusDot, { backgroundColor: mqttConnected ? colors.ok : colors.dangerLight }]} />
        <Text style={[styles.statusText, { color: mqttConnected ? colors.okDark : colors.dangerDark }]}>
          {mqttConnected ? 'MQTT: подключено (wqtt.ru)' : 'MQTT: нет связи...'}
        </Text>
      </View>

      {/* Date and time display — обновляется отдельным компонентом, чтобы
          секундный таймер не перерисовывал весь экран */}
      <Clock />

      <Card style={styles.card}>
        <Card.Content>
          <Title>Текущее состояние</Title>
          <Paragraph>ID устройства: {status.device_id || status.ip || '-'}</Paragraph>
          <Paragraph>IP устройства: {status.ip || '-'}</Paragraph>
          <View style={styles.metricsRow}>
            <View><Paragraph>Бак 1</Paragraph><Title>{status.tank1 ?? '-'}%</Title></View>
            <View><Paragraph>Бак 2</Paragraph><Title>{status.tank2 ?? '-'}%</Title></View>
            <View><Paragraph>Uptime</Paragraph><Title>{status.uptime ?? '-'} c</Title></View>
          </View>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Таймеры</Title>
          <View style={styles.timerLayout}>
            {/* Timer rows */}
            <View style={styles.timerCol}>
              {([
                {label:'Озонирование', key:'ozonation', stateKey:'OZONATION'},
                {label:'Аэрация',      key:'aeration',  stateKey:'AERATION'},
                {label:'Отстаивание', key:'settling',   stateKey:'SETTLING'},
              ] as {label:string; key:string; stateKey:string}[]).map(({label, key, stateKey}) => {
                const active = String(status?.state||'').toUpperCase() === stateKey;
                const timeStr = fmtHMS(status?.timers?.[key]);
                return (
                  <View key={key} style={{flexDirection:'row', alignItems:'center', marginBottom:14}}>
                    <View style={{width:8, height:8, borderRadius:4, backgroundColor: active ? '#43a047' : '#e0e0e0', marginRight:8}} />
                    <Text style={{flex:1, fontSize:13, color: active ? '#1b5e20' : '#757575', fontWeight: active ? '700' : '400'}}>{label}</Text>
                    <Text style={{fontFamily:'monospace', fontSize:18, fontWeight:'700', color: active ? '#2e7d32' : '#9e9e9e'}}>{timeStr}</Text>
                  </View>
                );
              })}
              {/* Filter remaining row */}
              {(() => {
                const fl = status?.flags || {};
                const backwashActive = String(status?.state||'').toUpperCase() === 'BACKWASH' || !!fl.backwashInProgress;
                let timeLabel: string;
                let dotColor: string;
                let textColor: string;
                let valColor: string;
                if (backwashActive) {
                  timeLabel = fmtHMS(status?.timers?.backwash);
                  dotColor = '#1565c0'; textColor = '#0d47a1'; valColor = '#1565c0';
                } else if (fl.filterCleaningNeeded) {
                  timeLabel = 'ПРОМЫВКА!';
                  dotColor = '#c62828'; textColor = '#c62828'; valColor = '#c62828';
                } else {
                  timeLabel = fmtDHMS(status?.timers?.filterRemaining);
                  dotColor = '#e0e0e0'; textColor = '#757575'; valColor = '#9e9e9e';
                }
                return (
                  <View style={{flexDirection:'row', alignItems:'center', marginBottom:4}}>
                    <View style={{width:8, height:8, borderRadius:4, backgroundColor: dotColor, marginRight:8}} />
                    <Text style={{flex:1, fontSize:13, color: textColor, fontWeight: (backwashActive || fl.filterCleaningNeeded) ? '700' : '400'}}>Wash period</Text>
                    <Text style={{fontFamily:'monospace', fontSize:18, fontWeight:'700', color: valColor}}>{timeLabel}</Text>
                  </View>
                );
              })()}
            </View>

            {/* Vertical tank indicators */}
            <View style={styles.tankGroup}>
              {([
                {label:'Б1', val: status?.tank1, color: colors.info},
                {label:'Б2', val: status?.tank2, color: colors.success},
              ] as {label:string; val:number|undefined; color:string}[]).map(({label, val, color}) => {
                const raw = Math.min(100, Math.max(0, Number(val) || 0));
                const fillPct = 100 - raw;
                return (
                  <View key={label} style={styles.tankItem}>
                    <Text style={styles.tankValue}>{val ?? '-'}%</Text>
                    <View style={styles.tankShell}>
                      <View style={[styles.tankFill, { height: `${fillPct}%` as any, backgroundColor: color }]} />
                    </View>
                    <Text style={styles.tankLabel}>{label}</Text>
                  </View>
                );
              })}
            </View>
          </View>
          <Paragraph style={styles.stateNote}>
            {status?.state || '—'}{status?.emergency?.active ? ' ⚠ АВАРИЯ' : ''}
          </Paragraph>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Режим управления</Title>
          <View style={styles.splitRow}>
            <Button
              mode={status.control_mode === 'manual' ? 'contained' : 'outlined'}
              onPress={() => setControlMode('manual')}
              style={styles.halfWidth}
            >
              Ручной
            </Button>
            <Button
              mode={status.control_mode === 'auto' ? 'contained' : 'outlined'}
              onPress={() => setControlMode('auto')}
              style={styles.halfWidth}
            >
              Автомат
            </Button>
          </View>
        </Card.Content>
      </Card>

      <Card style={styles.card}>
        <Card.Content>
          <Title>Управление реле</Title>
          <Paragraph style={styles.bottomGap}>Кнопки как в веб-интерфейсе (toggle ON/OFF).</Paragraph>
          <View style={styles.relayGrid}>
            {relayTargets.map((target) => {
              const isOn = !!status?.relays?.[target];
              const manualMode = status?.control_mode === 'manual';
              const btnColor = isOn ? colors.success : (manualMode ? colors.danger : colors.textLight);
              return (
                <Button
                  key={target}
                  mode="contained"
                  onPress={() => toggleRelay(target)}
                  buttonColor={btnColor}
                  textColor={colors.white}
                  style={[styles.relayButton, { opacity: manualMode ? 1 : 0.85 }]}
                >
                  {target}: {isOn ? 'ON' : 'OFF'}
                </Button>
              );
            })}
          </View>
        </Card.Content>
      </Card>

      <Card style={[styles.card, status?.emergency?.active && styles.emergencyCard]}>
        <Card.Content>
          <Title style={status?.emergency?.active ? { color: colors.danger } : undefined}>
            {status?.emergency?.active ? '⚠ АВАРИЯ' : 'Авария'}
          </Title>
          <Paragraph style={[
            styles.bottomGap,
            status?.emergency?.active ? { color: colors.danger, fontWeight: 'bold' } : undefined,
          ]}>
            {status?.emergency?.active
              ? `Активна: ${status?.emergency?.message || 'без сообщения'}`
              : 'Авария не активна'}
          </Paragraph>
          {status?.emergency?.active && (
            <Button mode="contained" buttonColor={colors.danger} textColor={colors.white} onPress={resetEmergency}>Сброс аварии</Button>
          )}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: colors.background },
  centered: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  centeredWide: { paddingHorizontal: 24 },
  centerAlign: { textAlign: 'center' },
  topGap: { marginTop: 12 },
  bottomGap: { marginBottom: 12 },
  statusBar: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: spacing.md,
    paddingTop: 10,
    paddingBottom: 4,
  },
  statusDot: { width: 10, height: 10, borderRadius: 5, marginRight: 6 },
  statusText: { fontSize: 12 },
  card: { margin: spacing.md },
  metricsRow: { flexDirection: 'row', justifyContent: 'space-between', marginTop: spacing.md },
  timerLayout: { flexDirection: 'row', marginTop: 10 },
  timerCol: { flex: 1 },
  tankGroup: { flexDirection: 'row', marginLeft: 14, alignItems: 'flex-start' },
  tankItem: { alignItems: 'center', marginHorizontal: 4 },
  tankValue: { fontSize: 10, color: '#555', marginBottom: 2 },
  tankShell: {
    width: 28,
    height: 110,
    backgroundColor: colors.border,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: colors.textMuted,
    overflow: 'hidden',
    justifyContent: 'flex-end',
  },
  tankFill: { width: '100%', borderRadius: 3 },
  tankLabel: { fontSize: 11, color: colors.textPrimary, marginTop: 3, fontWeight: 'bold' },
  stateNote: { marginTop: 8, fontSize: 11, color: colors.textFaint, textAlign: 'center' },
  splitRow: { flexDirection: 'row', justifyContent: 'space-between' },
  halfWidth: { width: '48%' },
  relayGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  relayButton: { marginBottom: 10, width: '48%' },
  emergencyCard: { backgroundColor: colors.dangerBg },
});
