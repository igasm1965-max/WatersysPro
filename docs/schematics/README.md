# WaterSysPro — Электросхемы

Набор графических электрических схем (SVG) для проекта WaterSysPro. Все схемы созданы на основе распиновки из [`include/config.h`](../../include/config.h:191).

## 📋 Список схем

| Схема | Описание |
|---|---|
| [`full_system.svg`](full_system.svg) | **Полная электросхема системы** — ESP32 Dev Module в центре, все подключенные модули вокруг, линии соединений с подписями GPIO |
| [`pinout_table.svg`](pinout_table.svg) | **Таблица распиновки GPIO** — полный список всех используемых пинов ESP32 с назначением |
| [`relays.svg`](relays.svg) | **Релейный модуль (6 каналов)** — подключение реле насосов, аэрации, озонатора, фильтра и обратной промывки |
| [`sensors_ultrasonic.svg`](sensors_ultrasonic.svg) | **Ультразвуковые датчики HC-SR04** — два датчика для баков 1 и 2, делитель напряжения для ECHO |
| [`i2c_bus.svg`](i2c_bus.svg) | **I2C шина** — LCD 20x4 дисплей (адрес 0x27) и RTC DS3231 (адрес 0x68) |
| [`encoder.svg`](encoder.svg) | **Энкодер KY-040** — подключение энкодера с кнопкой, подтягивающие резисторы |
| [`sd_card.svg`](sd_card.svg) | **SD-карта (SPI mode)** — microSD карта в SPI режиме, распиновка и предупреждения по питанию |

## 🎨 Цветовая кодировка

- 🔴 **Красный** — линии питания (VCC)
- ⚫ **Черный** — линии GND
- 🔵 **Синий** — сигнальные линии
- 🟢 **Зеленый** — I2C линии (SDA/SCL)
- 🟠 **Оранжевый** — SPI линии (SD-карта)

## 🔧 Используемые GPIO

| GPIO | Назначение | Подключение |
|------|-----------|-------------|
| GPIO2 | Built-in LED | Встроенный светодиод ESP32 |
| GPIO4 | SD SCK | SD Card SPI Clock |
| GPIO5 | HC-SR04 #2 TRIG | УЗ датчик 2 (бак 2) триггер |
| GPIO12 | PUMP2 | Реле насоса 2 (фильтрация) |
| GPIO13 | PUMP1 | Реле насоса 1 (наполнение) |
| GPIO14 | AERATION | Реле аэрации |
| GPIO16 | HC-SR04 #1 TRIG | УЗ датчик 1 (бак 1) триггер |
| GPIO17 | HC-SR04 #1 ECHO | УЗ датчик 1 (бак 1) эхо |
| GPIO18 | HC-SR04 #2 ECHO | УЗ датчик 2 (бак 2) эхо |
| GPIO19 | SD MISO | SD Card SPI MISO (DO) |
| GPIO21 | I2C SDA | LCD (0x27) + RTC DS3231 (0x68) |
| GPIO22 | I2C SCL | LCD (0x27) + RTC DS3231 (0x68) |
| GPIO23 | SD MOSI | SD Card SPI MOSI (DI) |
| GPIO25 | BACKWASH | Реле обратной промывки |
| GPIO26 | FILTER | Реле фильтра |
| GPIO27 | OZONE | Реле озонатора |
| GPIO32 | Encoder SW | Кнопка энкодера |
| GPIO33 | SD CS | SD Card SPI Chip Select |
| GPIO34 | Encoder DT | Энкодер DT (инвертирован) |
| GPIO35 | Encoder CLK | Энкодер CLK (инвертирован) |
| GPIO39 | SD CD | SD Card Detect (опционально) |

## 📄 Формат

Все схемы в формате **SVG** (Scalable Vector Graphics):
- Можно открыть в любом современном браузере
- Можно встроить в Markdown-документацию
- Можно редактировать в векторных редакторах (Inkscape, Illustrator)
- Можно конвертировать в PNG/PDF

## 🔗 Ссылки

- Исходный код распиновки: [`include/config.h`](../../include/config.h:191)
- План создания схем: [`plans/create_electrical_schematics_plan.md`](../../plans/create_electrical_schematics_plan.md)
- Документация SD-карты: [`docs/SD_INTEGRATION.md`](../../docs/SD_INTEGRATION.md:20)

---

*WaterSysPro v1.3.5 — Схемы созданы на основе актуальной распиновки из config.h*