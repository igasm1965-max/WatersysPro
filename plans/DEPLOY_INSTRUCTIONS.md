# Инструкция по деплою на VPS и сборке APK

## Предварительные требования

- **VPS** с Ubuntu/Debian, root-доступ по SSH
- **Аккаунт на [expo.dev](https://expo.dev)** (для облачной сборки APK)
- **Git** установлен локально

---

## Часть 1: Деплой backend на VPS

### Шаг 1: Подключитесь к серверу

```bash
ssh root@<IP-вашего-VPS>
```

### Шаг 2: Установите Git и Docker

```bash
# Обновить пакеты
apt update && apt upgrade -y

# Git
apt install -y git

# Docker (официальный скрипт)
curl -fsSL https://get.docker.com | sh
```

### Шаг 3: Склонируйте репозиторий

```bash
cd /opt
git clone <URL-вашего-репозитория> watersys
cd watersys
```

> Если репозиторий уже есть — обновите код:
> ```bash
> cd /opt/watersys
> git pull origin main
> ```

### Шаг 4: Настройте .env.cloud

```bash
cp .env.cloud.example .env.cloud
nano .env.cloud
```

**Обязательно смените следующие значения:**

| Переменная | Описание | Рекомендация |
|-----------|----------|--------------|
| `REMOTE_ADMIN_TOKEN` | Токен для удалённого управления | Сгенерируйте случайную строку: `openssl rand -hex 32` |
| `JWT_SECRET` | Секрет для JWT-токенов | Сгенерируйте случайную строку: `openssl rand -hex 32` |
| `POSTGRES_PASSWORD` | Пароль для PostgreSQL | Надёжный пароль, минимум 16 символов |

Остальные переменные можно оставить по умолчанию.

**Важно:** `PGHOST=postgres` — это имя сервиса в docker-compose, менять не нужно.

### Шаг 5: Запустите backend

```bash
docker compose -f docker-compose.cloud.yml up -d --build
```

### Шаг 6: Проверьте, что всё работает

```bash
# Статус контейнеров
docker compose -f docker-compose.cloud.yml ps

# Логи backend
docker compose -f docker-compose.cloud.yml logs -f backend

# Проверка API (на сервере)
curl http://localhost:3000/api/health
```

Ожидаемый ответ:
```json
{"status":"ok","uptime":123,"db":"connected","mqtt":"connected"}
```

### Шаг 7: Откройте порт 3000 в firewall

```bash
# Если используете ufw
ufw allow 3000

# Если используете iptables
iptables -A INPUT -p tcp --dport 3000 -j ACCEPT

# Если используете DigitalOcean / Vultr / др. — откройте порт в панели управления
```

### Шаг 8: Проверьте API извне

С локальной машины (не с VPS):

```bash
curl http://<IP-сервера>:3000/api/health
curl http://<IP-сервера>:3000/api/devices
curl http://<IP-сервера>:3000/api/device-logs-db/watersystem
```

---

## Часть 2: Сборка APK мобильного приложения

### Вариант A: Облачная сборка через EAS (рекомендуется)

**Требуется:** аккаунт на [expo.dev](https://expo.dev) (бесплатный).

```bash
# Перейти в директорию mobile
cd mobile

# Установить зависимости
npm install

# Войти в Expo (откроется браузер для авторизации)
npx eas login

# Собрать APK
npx eas build -p android --profile production
```

После завершения сборки EAS выдаст ссылку на скачивание APK.

### Вариант B: Локальная сборка (требуется Android SDK)

```bash
cd mobile
npm install
npx eas build -p android --local --profile production
```

**Требования:**
- Установлен Android SDK (через Android Studio)
- Настроена переменная `ANDROID_HOME`
- Приняты лицензии Android SDK

### Вариант C: Быстрое тестирование через Expo Go

```bash
cd mobile
npx expo start
```

Отсканируйте QR-код приложением **Expo Go** на телефоне.

**Ограничения:** Expo Go не поддерживает все нативные модули. Если приложение использует кастомные native модули — этот вариант не подойдёт.

---

## Часть 3: Настройка мобильного приложения

После установки APK на телефон:

1. Откройте приложение → **Settings** (шестерёнка)
2. В поле **Адрес сервера** укажите: `http://<IP-вашего-VPS>:3000`
   - **Важно:** IP вашего VPS, а не `192.168.0.103:3000`
3. Нажмите **Сохранить**
4. Перейдите в **Логи** → выберите **MQTT** → **Загрузить логи**

### Проверка всех режимов

| Режим | Описание | Когда работает |
|-------|----------|----------------|
| **MQTT** | Логи через MQTT → Backend → API | Всегда, если устройство онлайн |
| **RAM** | Прямое подключение к ESP32 (локальная сеть) | Телефон в одной Wi-Fi сети с ESP32 |
| **SD** | Прямое подключение к ESP32, лог с SD-карты | Телефон в одной Wi-Fi сети с ESP32 |

---

## Часть 4: Обслуживание

### Просмотр логов backend

```bash
# На VPS
docker compose -f docker-compose.cloud.yml logs -f backend
```

### Перезапуск backend

```bash
docker compose -f docker-compose.cloud.yml restart backend
```

### Обновление backend (после git pull)

```bash
cd /opt/watersys
git pull origin main
docker compose -f docker-compose.cloud.yml up -d --build
```

### Бэкап базы данных

```bash
docker exec watersys-postgres-1 pg_dump -U postgres watersys > backup_$(date +%Y%m%d).sql
```

---

## Диагностика проблем

### Backend не запускается

```bash
# Проверить логи
docker compose -f docker-compose.cloud.yml logs backend

# Проверить, что порт не занят
ss -tlnp | grep 3000

# Пересобрать с нуля
docker compose -f docker-compose.cloud.yml down
docker compose -f docker-compose.cloud.yml up -d --build
```

### Приложение не подключается к серверу

1. Проверьте, что сервер доступен из интернета:
   ```bash
   curl http://<IP-сервера>:3000/api/health
   ```
2. Проверьте firewall на VPS (порт 3000)
3. Проверьте адрес сервера в настройках приложения
4. Проверьте логи backend:
   ```bash
   docker compose -f docker-compose.cloud.yml logs -f backend
   ```

### Логи не загружаются (ошибка "Network Error")

1. Убедитесь, что адрес сервера в приложении указывает на VPS, а не на `192.168.x.x`
2. Проверьте endpoint логов:
   ```bash
   curl http://<IP-сервера>:3000/api/device-logs-db/watersystem
   ```
3. Если ответ `404` — устройство ещё не опубликовало логи через MQTT (подождите 1-2 мин)
4. Если ответ приходит — проблема в мобильном приложении (переустановите APK)

---

## Структура проекта (для справки)

```
/opt/watersys/
├── backend/                    # Node.js backend
│   ├── src/
│   │   ├── server.js           # Express сервер (основной)
│   │   ├── db.js               # PostgreSQL + in-memory
│   │   └── httpClient.js       # HTTP клиент для ESP32
│   ├── Dockerfile
│   └── package.json
├── mobile/                     # React Native (Expo)
│   ├── src/
│   │   ├── screens/
│   │   │   ├── LogsScreen.tsx  # Экран логов
│   │   │   ├── SettingsScreen.tsx
│   │   │   └── DashboardScreen.tsx
│   │   └── services/
│   │       └── api.ts          # API клиент (axios)
│   ├── app.json
│   └── eas.json
├── docker-compose.cloud.yml    # Docker compose для VPS
├── .env.cloud.example          # Шаблон переменных окружения
└── deploy.sh                   # Скрипт деплоя