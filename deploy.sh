#!/bin/bash
# Скрипт деплоя WatersysPro backend на VPS
# Запускать на сервере после клонирования репозитория

set -e

# 1. Установить Docker если не установлен
if ! command -v docker &> /dev/null; then
  curl -fsSL https://get.docker.com | sh
  usermod -aG docker $USER
  echo "Docker установлен. Перезайдите в сессию (newgrp docker) и запустите скрипт снова."
  exit 0
fi

# 2. Установить docker-compose plugin если не установлен
if ! docker compose version &> /dev/null; then
  apt-get install -y docker-compose-plugin
fi

# 3. Создать .env.cloud из примера если не существует
if [ ! -f .env.cloud ]; then
  cp .env.cloud.example .env.cloud
  echo ""
  echo "ВНИМАНИЕ: создан файл .env.cloud"
  echo "Отредактируйте его перед запуском:"
  echo "  nano .env.cloud"
  echo ""
  echo "Обязательно смените:"
  echo "  REMOTE_ADMIN_TOKEN=  (любой случайный токен)"
  echo "  JWT_SECRET=          (любая случайная строка)"
  echo "  POSTGRES_PASSWORD=   (надёжный пароль)"
  exit 0
fi

# 4. Запустить сервисы
docker compose -f docker-compose.cloud.yml pull
docker compose -f docker-compose.cloud.yml up -d --build

echo ""
echo "Backend запущен на порту 3000."
echo "Проверить логи: docker compose -f docker-compose.cloud.yml logs -f backend"
