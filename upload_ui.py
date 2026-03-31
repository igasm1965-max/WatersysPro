#!/usr/bin/env python3
"""
Загрузка файлов веб-интерфейса на SD-карту ESP32 через HTTP.
Использование:
    python upload_ui.py [IP] [token]
Пример:
    python upload_ui.py 192.168.0.108 A7b9K3mN4pQrT6vX2zY8

Файлы берутся из папки "закинуть на сд/"
"""
import os
import sys
import urllib.request

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(ROOT, "закинуть на сд")
FILES = ["index.html", "logfilter.html"]

def upload_file(ip, token, filename):
    path = os.path.join(SRC_DIR, filename)
    if not os.path.exists(path):
        print(f"  ПРОПУСК: {path} не найден")
        return False

    url = f"http://{ip}/api/sd_upload?token={token}"
    boundary = b"----WatersysBoundary"
    with open(path, "rb") as f:
        file_data = f.read()

    body = (
        b"--" + boundary + b"\r\n"
        b'Content-Disposition: form-data; name="file"; filename="' + filename.encode() + b'"\r\n'
        b"Content-Type: text/html\r\n\r\n"
        + file_data
        + b"\r\n--" + boundary + b"--\r\n"
    )

    req = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={"Content-Type": f"multipart/form-data; boundary={boundary.decode()}"}
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            resp = r.read().decode()
            print(f"  {filename}: {resp}")
            return True
    except Exception as e:
        print(f"  ОШИБКА {filename}: {e}")
        return False

def main():
    ip = sys.argv[1] if len(sys.argv) > 1 else input("IP адрес ESP32 [192.168.0.108]: ").strip() or "192.168.0.108"
    token = sys.argv[2] if len(sys.argv) > 2 else input("Admin token: ").strip()

    if not token:
        print("Токен не указан. Выход.")
        sys.exit(1)

    print(f"\nЗагрузка файлов на ESP32 ({ip})...")
    ok = 0
    for f in FILES:
        print(f"Загрузка {f}...")
        if upload_file(ip, token, f):
            ok += 1

    print(f"\nГотово: {ok}/{len(FILES)} файлов загружено.")
    if ok == len(FILES):
        print("Обновите страницу браузера: http://" + ip)

if __name__ == "__main__":
    main()
