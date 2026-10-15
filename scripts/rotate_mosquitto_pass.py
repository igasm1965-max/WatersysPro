"""
Временный скрипт для ротации пароля MQTT-брокера Mosquitto.
Генерирует надёжный случайный пароль для пользователя `esp32` и применяет его
через `mosquitto_passwd -b` внутри контейнера eclipse-mosquitto (docker compose).

После успешного выполнения выводит НОВЫЙ пароль — его нужно прописать:
  1) в .env backend (MQTT_PASS)
  2) в настройках MQTT на устройстве (NVS)
Этот файл можно удалить после использования.
"""

import secrets
import string
import subprocess
import sys

USER = "esp32"
PW_FILE = "/mosquitto/config/passwd"

# Надёжный пароль: ~26 символов, буквы разного регистра + цифры.
# (Без спецсимволов, чтобы не создавать проблем при использовании в URL/env/NVS.)
alphabet = string.ascii_letters + string.digits
new_password = "".join(secrets.choice(alphabet) for _ in range(26))

cmd = [
    "docker", "compose", "run", "--rm", "mosquitto",
    "mosquitto_passwd", "-b", PW_FILE, USER, new_password,
]

print(">>> Running:", " ".join(cmd))
proc = subprocess.run(cmd, capture_output=True, text=True)

if proc.returncode != 0:
    print("ERROR: mosquitto_passwd failed.", file=sys.stderr)
    print(proc.stdout, file=sys.stderr)
    print(proc.stderr, file=sys.stderr)
    sys.exit(proc.returncode)

print("OK: password file updated.")
print("=" * 60)
print(f"NEW_MQTT_USER={USER}")
print(f"NEW_MQTT_PASS={new_password}")
print("=" * 60)
print("Apply this password to backend (.env MQTT_PASS) and the device (NVS MQTT password).")