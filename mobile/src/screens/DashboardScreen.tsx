import React, { useEffect, useState } from 'react';
import { View, ScrollView, RefreshControl } from 'react-native';
import { Card, Title, Paragraph, Button, Chip, ActivityIndicator } from 'react-native-paper';
import { VictoryLine, VictoryChart, VictoryTheme } from 'victory-native';
import ApiService from '../services/api';
import WebSocketService from '../services/websocket';

export default function DashboardScreen() {
  const [status, setStatus] = useState<any | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [historyData, setHistoryData] = useState<any[]>([]);
  const deviceId = 'esp32_001';

  useEffect(() => {
    loadData();
    WebSocketService.connect();
    const unsub = WebSocketService.onStatusUpdate((s) => {
      if (s.deviceId === deviceId) setStatus(s.payload);
    });
    return () => {
      unsub();
      WebSocketService.disconnect();
    };
  }, []);

  const loadData = async () => {
    try {
      const [st, hist] = await Promise.all([ApiService.getStatus(deviceId), ApiService.getHistory(deviceId, 7)]);
      setStatus(st.payload || st);
      setHistoryData(hist || []);
    } catch (e) {
      console.warn(e);
    } finally { setLoading(false); setRefreshing(false); }
  };

  const onRefresh = () => { setRefreshing(true); loadData(); };
  const handleFlush = async () => { await ApiService.sendCommand(deviceId, { command: 'start_flush' }); };

  if (loading) return (<View style={{flex:1,justifyContent:'center',alignItems:'center'}}><ActivityIndicator size="large" /></View>);
  if (!status) return (<View style={{flex:1,justifyContent:'center',alignItems:'center'}}><Title>No data</Title><Button onPress={loadData}>Retry</Button></View>);

  return (
    <ScrollView refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} />} style={{flex:1}}>
      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Текущее состояние</Title>
          <View style={{flexDirection:'row',justifyContent:'space-between',marginTop:16}}>
            <View><Paragraph>Давление</Paragraph><Title>{status.pressure ?? '-' } bar</Title></View>
            <View><Paragraph>Расход</Paragraph><Title>{status.flowRate ?? '-'} l/min</Title></View>
            <View><Paragraph>Ресурс фильтра</Paragraph><Title>{status.filterLifeLeft ?? '-'}%</Title></View>
          </View>
          <View style={{flexDirection:'row',marginTop:16}}>
            <Chip icon={status.valveState === 'flushing' ? 'refresh' : 'check'} style={{marginRight:8}}>{status.valveState}</Chip>
            <Chip icon="water" style={{marginRight:8}}>Соль: {status.saltLevel ?? '-' }%</Chip>
          </View>
        </Card.Content>
      </Card>

      <Card style={{margin:16}}>
        <Card.Content>
          <Title>Давление за 7 дней</Title>
          <VictoryChart theme={VictoryTheme.material}>
            <VictoryLine data={historyData} x="timestamp" y="pressure" style={{data:{stroke:'#2196F3'}}} />
          </VictoryChart>
        </Card.Content>
      </Card>

      {status.alerts && status.alerts.length > 0 && (
        <Card style={{margin:16, backgroundColor:'#ffebee'}}>
          <Card.Content>
            <Title style={{color:'#c62828'}}>⚠️ Требуется внимание</Title>
            {status.alerts.map((alert:any) => (
              <View key={alert.id} style={{marginVertical:8}}>
                <Paragraph style={{fontWeight:'bold'}}>{alert.message}</Paragraph>
                <Paragraph style={{fontSize:12}}>{new Date(alert.timestamp).toLocaleString()}</Paragraph>
                <Button mode="text" onPress={() => ApiService.sendCommand(deviceId, { command: 'acknowledge_alert', parameters: { alertId: alert.id } })}>Понятно</Button>
              </View>
            ))}
          </Card.Content>
        </Card>
      )}

      <Card style={{margin:16}}>
        <Card.Content>
          <Button mode="contained" onPress={handleFlush} disabled={status.valveState !== 'idle'}>Запустить промывку</Button>
        </Card.Content>
      </Card>
    </ScrollView>
  );
}
