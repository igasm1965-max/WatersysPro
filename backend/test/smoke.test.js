/*
  Smoke-тесты backend WaterSysPro.

  Использует встроенный тест-раннер Node.js (node:test). Не требует
  реального MQTT-брокера или PostgreSQL: импортирует Express-app из
  server.js и проверяет базовые endpoint'ы через fetch.

  Запуск:
    npm test
    npm run test:smoke
*/
const { test } = require('node:test');
const assert = require('node:assert');

// Отключаем внешние зависимости ДО импорта server.js, чтобы тесты
// не подключались к реальному MQTT-брокеру и не зависели от PostgreSQL.
// dotenv не перезаписывает уже установленные переменные окружения.
process.env.MQTT_BROKER = '';
process.env.JWT_SECRET = 'test-jwt-secret';

// Импортируем app (сервер НЕ стартует при импорте — защищено require.main).
const { app, server } = require('../src/server');

let baseUrl = '';

test.before(() => {
  return new Promise((resolve) => {
    server.listen(0, () => {
      const { port } = server.address();
      baseUrl = `http://127.0.0.1:${port}`;
      resolve();
    });
  });
});

test.after(() => {
  return new Promise((resolve) => server.close(resolve));
});

test('GET /api/health возвращает ok:true', async () => {
  const res = await fetch(`${baseUrl}/api/health`);
  assert.strictEqual(res.status, 200);
  const body = await res.json();
  assert.strictEqual(body.ok, true);
  assert.ok('dbReady' in body);
});

test('GET /metrics возвращает текстовые метрики Prometheus', async () => {
  const res = await fetch(`${baseUrl}/metrics`);
  assert.strictEqual(res.status, 200);
  const contentType = res.headers.get('content-type') || '';
  assert.ok(contentType.includes('text/plain'));
  const text = await res.text();
  assert.ok(text.includes('watersys_uptime_seconds'));
  assert.ok(text.includes('watersys_http_requests_total'));
});

test('GET /metrics содержит counter запросов', async () => {
  await fetch(`${baseUrl}/api/health`);
  const res = await fetch(`${baseUrl}/metrics`);
  const text = await res.text();
  const line = text.split('\n').find((l) => l.startsWith('watersys_http_requests_total '));
  assert.ok(line, 'counter должен присутствовать');
  const val = Number(line.split(' ')[1]);
  assert.ok(val >= 1);
});

test('POST /api/auth/login без БД возвращает 503 (login отключён)', async () => {
  // В тестовом окружении БД недоступна → ожидаем 503, а не крэш.
  const res = await fetch(`${baseUrl}/api/auth/login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email: 'x@y.z', password: 'p' }),
  });
  assert.ok([400, 401, 503].includes(res.status), `ожидался 400/401/503, получен ${res.status}`);
});

test('GET несуществующий путь отдаёт 404', async () => {
  const res = await fetch(`${baseUrl}/no-such-route-12345`);
  assert.strictEqual(res.status, 404);
});