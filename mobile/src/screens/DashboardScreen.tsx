import React, { useCallback, useEffect, useRef, useState } from 'react';
import { View, ScrollView, RefreshControl, Text, Alert } from 'react-native';
import { Card, Title, Paragraph, Button, ActivityIndicator, Chip } from 'react-native-paper';
import AsyncStorage from '@react-native-async-storage/async-storage';
import MqttService from '../services/mqttService';
import { useFocusEffect } from '@react-navigation/native';

export default function DashboardScreen() {
  const [status, setStatus] = useState<any | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [mqttConnected, setMqttConnected] = useState(false);
  const prevEmergencyRef = useRef<boolean>(false);

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



  // Alert when emergency becomes active
  useEffect(() => {
    const isActive = !!status?.emergency?.active;
    if (isActive && !prevEmergencyRef.current) {
      Alert.alert('АВАРИЯ', status?.emergency?.message || 'Аварийный режим активирован', [{ text: 'OK' }]);
    }
    prevEmergencyRef.current = isActive;
  }, [status?.emergency?.active]);

  // Connect to MQTT on mount, disconnect on unmount
  useEffect(() => {
    let unsubTelemetry: (() => void) | null = null;
    let unsubConn: (() => void) | null = null;

    unsubConn = MqttService.onConnectionChange(setMqttConnected);

    const timeout = setTimeout(() => {
      setLoading(false);
    }, 10000);

    MqttService.connect()
      .then(() => {
        setMqttConnected(true);
        setLoading(false);
        unsubTelemetry = MqttService.onTelemetry((data) => {
          setStatus(data);
        });
      })
      .catch((err) => {
        console.warn('[MQTT] connect error', err);
        setLoading(false);
      });

    return () => {
      clearTimeout(timeout);
      if (unsubTelemetry) unsubTelemetry();
      if (unsubConn) unsubConn();
      MqttService.disconnect();
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
    setStatus((prev: any) => ({
      ...prev,
      relays: { ...(prev?.relays || {}), [target]: next ? 1 : 0 },
    }));
    MqttService.publish(target, next ? 'on' : 'off');
  };

  const setControlMode = (mode: 'manual' | 'auto') => {
    if (!mqttConnected) { alert('Нет подключения к MQTT брокеру.'); return; }
    MqttService.publish('set_mode', mode);
    setStatus((prev: any) => ({ ...(prev || {}), control_mode: mode }));
  };

  const resetEmergency = () => {
    if (!mqttConnected) { alert('Нет подключения к MQTT брокеру.'); return; }
    MqttService.publish('reset_emergency', '1');
  };

  if (loading) return (
    <View style={{flex:1,justifyContent:'center',alignItems:'center'}}>
      <ActivityIndicator size="large" />
      <Paragraph style={{marginTop:12}}>Подключение к MQTT...</Paragraph>
    </View>
  );
  if (!status) return (
    <View style={{flex:1,justifyContent:'center',alignItems:'center',paddingHorizontal:24}}>
      <Title style={{textAlign:'center'}}>Нет данных</Title>
      <Paragraph style={{textAlign:'center',marginTop:8}}>
        {mqttConnected ? 'Ожидание телеметрии от устройства...' : 'Нет подключения к MQTT брокеру m1.wqtt.ru'}
      </Paragraph>
    </View>
  );

  return (
    <ScrollView refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />} style={{flex:1}}>
      {/* MQTT status bar */}
      <View style={{flexDirection:'row', alignItems:'center', paddingHorizontal:16, paddingTop:10, paddingBottom:4}}>
        <View style={{width:10, height:10, borderRadius:5, backgroundColor: mqttConnected ? '#22c55e' : '#ef4444', marginRight:6}} />
        <Text style={{fontSize:12, color: mqttConnected ? '#15803d' : '#b91c1c'}}>
          {mqttConnected ? 'MQTT: подключено (wqtt.ru)' : 'MQTT: нет связи...'}
        </Text>
      </View>
      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Текущее состояние</Title>
          <Paragraph>ID устройства: {status.device_id || status.ip || '-'}</Paragraph>
          <Paragraph>IP устройства: {status.ip || '-'}</Paragraph>
          <View style={{flexDirection:'row',justifyContent:'space-between',marginTop:16}}>
            <View><Paragraph>Бак 1</Paragraph><Title>{status.tank1 ?? '-'}%</Title></View>
            <View><Paragraph>Бак 2</Paragraph><Title>{status.tank2 ?? '-'}%</Title></View>
            <View><Paragraph>Uptime</Paragraph><Title>{status.uptime ?? '-'} c</Title></View>
          </View>
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Таймеры</Title>
          <View style={{flexDirection:'row', marginTop:10}}>
            {/* Timer rows */}
            <View style={{flex:1}}>
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
            <View style={{flexDirection:'row', marginLeft:14, alignItems:'flex-start'}}>
              {([
                {label:'Б1', val: status?.tank1, color:'#1565c0'},
                {label:'Б2', val: status?.tank2, color:'#2e7d32'},
              ] as {label:string; val:any; color:string}[]).map(({label, val, color}) => {
                const raw = Math.min(100, Math.max(0, Number(val) || 0));
                const fillPct = 100 - raw;
                return (
                  <View key={label} style={{alignItems:'center', marginHorizontal:4}}>
                    <Text style={{fontSize:10, color:'#555', marginBottom:2}}>{val ?? '-'}%</Text>
                    <View style={{width:28, height:110, backgroundColor:'#e0e0e0', borderRadius:4, borderWidth:1, borderColor:'#9e9e9e', overflow:'hidden', justifyContent:'flex-end'}}>
                      <View style={{width:'100%', height:`${fillPct}%` as any, backgroundColor: color, borderRadius:3}} />
                    </View>
                    <Text style={{fontSize:11, color:'#333', marginTop:3, fontWeight:'bold'}}>{label}</Text>
                  </View>
                );
              })}
            </View>
          </View>
          <Paragraph style={{marginTop:8, fontSize:11, color:'#aaa', textAlign:'center'}}>
            {status?.state || '—'}{status?.emergency?.active ? ' ⚠ АВАРИЯ' : ''}
          </Paragraph>
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Режим управления</Title>
          <View style={{flexDirection:'row',justifyContent:'space-between'}}>
            <Button
              mode={status.control_mode === 'manual' ? 'contained' : 'outlined'}
              onPress={() => setControlMode('manual')}
              style={{width:'48%'}}
            >
              Ручной
            </Button>
            <Button
              mode={status.control_mode === 'auto' ? 'contained' : 'outlined'}
              onPress={() => setControlMode('auto')}
              style={{width:'48%'}}
            >
              Автомат
            </Button>
          </View>
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Управление реле</Title>
          <Paragraph style={{marginBottom:12}}>Кнопки как в веб-интерфейсе (toggle ON/OFF).</Paragraph>
          <View style={{flexDirection:'row',flexWrap:'wrap',justifyContent:'space-between'}}>
            {relayTargets.map((target) => {
              const isOn = !!status?.relays?.[target];
              const manualMode = status?.control_mode === 'manual';
              const btnColor = isOn ? '#2e7d32' : (manualMode ? '#c62828' : '#757575');
              return (
                <Button
                  key={target}
                  mode="contained"
                  onPress={() => toggleRelay(target)}
                  buttonColor={btnColor}
                  textColor="#ffffff"
                  style={{marginBottom:10, width:'48%', opacity: manualMode ? 1 : 0.85}}
                >
                  {target}: {isOn ? 'ON' : 'OFF'}
                </Button>
              );
            })}
          </View>
        </Card.Content>
      </Card>

      <Card style={{margin:16, backgroundColor: status?.emergency?.active ? '#ffebee' : '#ffffff'}}>
        <Card.Content>
          <Title style={status?.emergency?.active ? {color: '#c62828'} : undefined}>
            {status?.emergency?.active ? '⚠ АВАРИЯ' : 'Авария'}
          </Title>
          <Paragraph style={{marginBottom:12, color: status?.emergency?.active ? '#c62828' : undefined, fontWeight: status?.emergency?.active ? 'bold' : 'normal'}}>
            {status?.emergency?.active
              ? `Активна: ${status?.emergency?.message || 'без сообщения'}`
              : 'Авария не активна'}
          </Paragraph>
          {status?.emergency?.active && (
            <Button mode="contained" buttonColor="#c62828" textColor="#ffffff" onPress={resetEmergency}>Сброс аварии</Button>
          )}
        </Card.Content>
      </Card>
    </ScrollView>
  );
}
