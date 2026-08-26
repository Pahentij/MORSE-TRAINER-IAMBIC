Проект создан на бaзе

https://github.com/LU6APR/ESP32_C3_MINI_MORSE_TRAINER

# MORSE TRAINER IAMBIC — LU6APR

Телеграфный тренажёр на ESP32-C3 с ямбичным ключом, аппаратным CW-выходом, встроенным декодером азбуки Морзе, дисплеем Nokia 5110 / PCD8544 и двумя последовательными интерфейсами: USB-Serial и Bluetooth-Serial.

Текущая зафиксированная версия исходного кода: **v41**.

## 1. Возможности

Проект объединяет:

- ямбичный телеграфный ключ;
- Iambic Mode A и Mode B;
- ввод DIT/DAH двумя кнопками;
- регулировку скорости 10–35 WPM с шагом 5 WPM;
- программный sidetone;
- аппаратный CW-выход через `PIN_KEY_OUT`;
- декодирование Morse в режиме реального времени;
- выбор языка декодирования RU / EN;
- цифры и знаки пунктуации;
- дополнительные поддерживаемые символы `_`, `+`, `&`;
- распознавание служебных сигналов, основных Q-кодов и кодов системы 92;
- вывод служебных кодов в формате `[CODE] <Description>`;
- вывод полного текста служебных кодов в USB-Serial и Bluetooth-Serial;
- Nokia 5110 / PCD8544 84×48;
- настройку подсветки, контрастности и ориентации дисплея;
- перестановку DIT и DAH;
- автоматическую или ручную очистку экрана;
- сохранение настроек в EEPROM;
- BLE Serial на базе NimBLE и FreeRTOS-очереди;
- отображение таблиц Morse Codes и Service Codes непосредственно на дисплее.

---

## 2. Аппаратная платформа

Проект рассчитан на **ESP32-C3 Super Mini**.

Дисплей:

- Nokia 5110 / PCD8544;
- разрешение 84×48;
- программный SPI;
- библиотека `U8g2`.

Основные библиотеки:

```text
Arduino
EEPROM
U8g2lib
NimBLEDevice
FreeRTOS
```

---

## 3. Распиновка

| Назначение | GPIO |
|---|---:|
| DIT | GPIO 2 |
| DAH | GPIO 3 |
| COMMAND | GPIO 0 |
| Sidetone / динамик | GPIO 1 |
| CW KEY OUT | GPIO 20 |
| Подсветка LCD | GPIO 6 |
| LCD CLK | GPIO 4 |
| LCD DIN | GPIO 5 |
| LCD SCE | GPIO 7 |
| LCD DC | GPIO 8 |
| LCD RST | GPIO 10 |

DIT, DAH и COMMAND используют `INPUT_PULLUP`.

CW-выход:

```text
PIN_KEY_OUT = GPIO 20
```

В активном состоянии выход устанавливается в `HIGH`.

---

## 4. Дисплей

Используется:

```cpp
U8G2_PCD8544_84X48_F_4W_SW_SPI
```

Конфигурация:

```text
84 × 48 пикселей
4 текстовых строки основного текстового поля
статусная строка в нижней части
```

Основные параметры:

```cpp
TEXT_ROWS = 4
TEXT_COLS = 14
```

Таким образом, один экран основного текстового поля рассчитан на:

```text
4 × 14 = 56 позиций
```

При стартовом экране `>READY<` ввод занимает строки 2–4, поэтому до первой ручной очистки доступно:

```text
3 × 14 = 42 позиции
```

Для отображения используется кириллический шрифт U8g2:

```cpp
u8g2_font_5x7_t_cyrillic
u8g2_font_4x6_t_cyrillic
```

---

## 5. Статусная строка

После Splash Screen статус выводится в USB-Serial и Bluetooth-Serial.

Формат:

```text
15WPM Mode A RU BT
```

Содержимое:

- скорость WPM;
- Iambic Mode A/B;
- язык RU/EN;
- `BT`, если BLE-клиент подключён.

На дисплее статусная строка размещена в нижней части экрана.

---

## 6. Управление ключом

### DIT

Короткое нажатие/удержание DIT формирует точку:

```text
.
```

### DAH

Короткое нажатие/удержание DAH формирует тире:

```text
-
```

Длительность элементов определяется скоростью WPM.

Формулы:

```text
DIT = 1200 / WPM мс
DAH = 3 × DIT
межсимвольный интервал = 3 × DIT
межсловный интервал = 7 × DIT
```

Например, при 15 WPM:

```text
DIT = 1200 / 15 = 80 мс
DAH = 240 мс
```

---

## 7. Iambic A / B

В меню `Iambic mode` доступны:

```text
Mode A
Mode B
```

### Mode A

Работа сжатия paddles соответствует Iambic A.

### Mode B

Если оба paddles были зажаты во время текущего элемента, после его завершения может быть автоматически сформирован дополнительный противоположный элемент.

Переключение:

```text
DIT = Mode A
DAH = Mode B
```

---

## 8. Swap DIT & DAH

Параметр:

```text
Swap DIT&DAH
```

Позволяет поменять логические функции физических входов местами.

```text
OFF:
физический DIT → DIT
физический DAH → DAH

ON:
физический DIT → DAH
физический DAH → DIT
```

---

## 9. Sidetone

Параметр:

```text
Beep
```

Значения:

```text
ON
OFF
```

Рабочая частота sidetone:

```text
600 Hz
```

Сигналы меню используют дополнительные короткие звуки подтверждения.

---

## 10. Управление COMMAND

Кнопка `COMMAND` выполняет две основные функции.

### Долгое нажатие

Порог:

```text
1000 мс
```

Долгое нажатие открывает главное меню.

### Короткое нажатие

В обычном режиме очищает экран.

Перед очисткой в USB-Serial и Bluetooth-Serial выполняется перевод строки.

После очистки отображается:

```text
>READY<
```

---

## 11. Главное меню

Главное меню содержит:

```text
Back
Speed, WPM
Iambic mode
Beep
Language
CLR scrn
Backlight
Contrast
Display rot.
Swap DIT&DAH
Morse codes
About
```

При входе в меню указатель устанавливается на:

```text
Back
```

При открытии меню все текущие настройки выводятся в USB-Serial и Bluetooth-Serial.

Пример:

```text
SETTINGS
Speed, WPM: 15
Iambic mode: Mode B
Beep: ON
Language: EN
CLR scrn: Auto
Backlight: 55%
Contrast: 70%
Display rot.: UP
Swap DIT&DAH: ON
About
```

При изменении любого параметра его полный набор настроек повторно выводится в Serial.

После выхода из любого подменю главное меню также выводится в Serial/BLE Serial.

---

## 12. Навигация по меню

В главном меню:

```text
DIT  → следующий пункт
DAH  → предыдущий пункт
COMMAND → открыть выбранный пункт
```

В большинстве меню настройки:

```text
DIT  → уменьшение / первый вариант
DAH  → увеличение / второй вариант
COMMAND → сохранить и вернуться в главное меню
```

В `Morse codes`:

```text
DIT → следующая страница
DAH → предыдущая страница
COMMAND → вернуться в главное меню
```

---

## 13. Скорость WPM

Диапазон:

```text
10–35 WPM
```

Шаг:

```text
5 WPM
```

Циклическое изменение:

```text
10 → 15 → 20 → 25 → 30 → 35 → 10
```

---

## 14. Language

Доступны:

```text
RU
EN
```

В меню:

```text
DIT = RU
DAH = ENG
COMMAND = OK
```

Язык влияет на декодирование букв и вариантов пунктуации.

Важно: служебные сигналы, Q-коды и коды системы 92 распознаются независимо от значения `Language`.

---

# 15. Декодер Morse

Декодер работает непосредственно с временными параметрами CW-сигнала.

При завершении Morse-последовательности:

1. определяется длительность каждого элемента;
2. формируется двоичная Morse-последовательность;
3. сначала проверяется `serviceCodes[]`;
4. затем выполняется обычное декодирование символа;
5. результат выводится на дисплей и в Serial/BLE Serial.

Неизвестная или недопустимая последовательность возвращает:

```text
*
```

---

## 16. Приоритет распознавания

Порядок распознавания принципиален.

Сначала проверяются:

```text
Service signals
Q-codes
92 Code
```

и только после этого:

```text
обычные Morse-символы
```

Это необходимо, поскольку часть служебных кодов имеет последовательности, которые нельзя обрабатывать как независимые буквы.

---

# 17. Служебные сигналы, Q-коды и 92 Code

В проекте используется массив:

```cpp
const ServiceCode serviceCodes[] PROGMEM
```

Каждая запись содержит:

```cpp
codeText
description
length
code
```

При распознавании сервисного кода:

- дисплей немедленно очищается;
- на экран выводится код и описание;
- сообщение обрезается по размеру дисплея;
- полный текст передаётся в USB-Serial;
- полный текст передаётся в Bluetooth-Serial;
- перед сервисным кодом обеспечивается перевод строки в Serial;
- после сообщения выполняется перевод строки.

Формат:

```text
[SOS] <Emergency call for help.>
```

---

## 18. Текущие Service Codes

### Трёхбуквенные

```text
[SOS]  Emergency call for help.
[VVV]  Testing signal.
[QTH]  My location is...
[QRP]  Operating with low power.
[QRZ]  You are being called by...
[QSL]  I acknowledge receipt.
[QSO]  Direct communication is established with...
[QRL]  I am busy.
[QRM]  Experiencing man-made interference.
[QRN]  Troubled by atmospheric noise (static).
[QRT]  Stopping transmission / Going off air.
[QRO]  Increasing transmitter power.
[QRQ]  Send faster.
[QRS]  Send more slowly.
[QSB]  Your signals are fading.
[QRK]  The intelligibility of your signals is... (1-5).
[QSA]  The strength of your signals is... (1-5).
[QRV]  I am ready.
[QRX]  I will call you again at... / Standby.
[QTR]  The correct time is...
```

### Двухбуквенные

```text
[KN]  Others please stand by
[BK]  Break.
[HH]  Error.
[SK]  End of contact.
[CL]  Closing station.
[CQ]  Calling any station!
```

### Коды системы 92

```text
[12] Do you understand?
[13] I understand.
[14] What is the weather?
[15] For your information.
[17] Breaking news item
[18] What are your instructions?
[19] Important message.
[21] Stop for meal.
[22] Love and greetings.
[23] Message for all.
[24] Repeat this back.
[25] Busy on another circuit.
[30] The End.
[33] Best regards (Between female operators).
[44] Respects to nature. (Modern addition used in park activations)
[55] Best success. / Friendly handshake.
[73] Best regards.
[88] Love and kisses.
[92] Deliver to all stations.
[99] Get off this frequency).
```

Текст выше приведён непосредственно по массиву `serviceCodes[]` текущей версии v41.

---

# 19. Страницы Service Codes в меню Morse codes

В `Morse codes` добавлены две страницы.

### Страница 7

```text
SERVICE CODES - 3 LETTER
```

На дисплее коды располагаются в:

```text
4 столбца × 7 строк
```

Формат:

```text
[SOS] [VVV] [QTH] [QRP]
...
```

В Serial/BLE Serial выводится полный текст:

```text
[SOS], Emergency call for help.
[VVV], Testing signal.
[QTH], My location is...
```

### Страница 8

```text
SERVICE CODES - 2 LETTER / 92
```

На дисплее:

```text
[KN] [BK] [HH] [SK]
...
[12] [13] [14] [15]
...
```

В Serial/BLE Serial выводится полное описание каждого кода:

```text
[KN], Others please stand by
[BK], Break.
[HH], Error.
...
[12], Do you understand?
[13], I understand.
```

Страницы переключаются кнопками DIT и DAH.

После последней страницы выполняется переход обратно на первую.

---

# 20. Morse Codes — страницы

Меню `Morse codes` содержит 9 страниц.

```text
0 — MORSE CODES - EN
1 — MORSE CODES - EN, продолжение
2 — MORSE CODES - RU
3 — MORSE CODES - RU, продолжение
4 — MORSE CODES - DIGITS
5 — MORSE CODES - PUNCTUATION RU
6 — MORSE CODES - PUNCTUATION EN
7 — SERVICE CODES - 3 LETTER
8 — SERVICE CODES - 2 LETTER / 92
```

---

## 21. EN Morse

Страница 1:

```text
A .-    I ..    Q --.-
B -...  J .--.  R .-.
C -.-.  K -.-   S ...
D -..   L .-..  T -
E .     M --    U ..-
F ..-.  N -.    V ...-
G --.   O ---   W .--
H ....  P .--.  X -..-
```

Страница 2:

```text
Y -.--  Z --..
```

---

## 22. RU Morse

Страница 1:

```text
А .-    И -.   Р --.
Б ---.  Й ---  С --.
В --    К --   Т -
Г --.   Л ---. У --
Д --.   М --   Ф ---.
Е .     Н -.   Х ---.
Ж ---   О --   Ц ---.
З ---.  П ---. Ч ---.
```

Страница 2:

```text
Ш ---  Ь ---
Щ ---  Э ----.
Ъ ---- Ю ---
Ы ---  Я ---
```

---

## 23. Цифры

Страница:

```text
0 -----     5 .....
1 .----     6 -....
2 ..---     7 --...
3 ...--     8 ---..
4 ....-     9 ----.
```

Цифры распознаются независимо от выбранного языка.

---

# 24. Пунктуация RU

Страница:

```text
PUNCTUATION RU
'.' ......   ',' .-.-.-
':' ---...   ';' -.-.-.
'?' ..--..   '!' --..--
'"' .-..-.   ''' .----.
'(' -.--.    ')' -.--.-
'/' -..-.    '-' -....-
'=' -...-    '@' .--.-.
```

RU-пунктуация выбирается отдельной веткой декодера.

Для текущей реализации следует учитывать, что некоторые Morse-коды совпадают. В частности, согласно таблице проекта точка и запятая имеют одинаковую последовательность, поэтому декодер не может определить исходный символ только по Morse-последовательности без дополнительного контекста.

---

# 25. Пунктуация EN

Страница:

```text
PUNCTUATION EN
'.' .-.-.-   ',' --..--
':' ---...   ';' -.-.-.
'?' ..--..   '!' -.-.--
'"' .-..-.   ''' .----.
'(' -.--.-   ')' -.--.-
'/' -..-.    '-' -....-
'=' -...-    '@' .--.-.
```

EN имеет отдельную ветку декодирования пунктуации.

Полные варианты для `"` и `'` используются в соответствии с текущей реализацией:

```text
"  .-..-.
'  .----.
```

---

# 26. Дополнительные символы

В таблице проекта также присутствуют:

```text
_  .-..-.
+  .-.-.
&  ..-.
```

Эти символы являются поддерживаемыми элементами `morseTable`.

---

# 27. Особенности скобок

В EN используется единый Morse-код:

```text
-.--.-
```

Для него декодер возвращает специальное значение:

```cpp
MORSE_PARENTHESES_CODE
```

После чего выводит:

```text
()
```

В RU открывающая и закрывающая скобки имеют разные коды:

```text
(  -.--.
)  -.--.-
```

---

# 28. Автоматические пробелы

После паузы:

```text
1000 мс
```

декодер добавляет пробел между словами.

Межбуквенный и межсловный интервалы также определяются текущим WPM.

---

# 29. Очистка экрана

Параметр:

```text
CLR scrn
```

имеет два режима.

### Auto

После заполнения доступного текстового поля запускается задержка:

```text
500 мс
```

После неё экран очищается автоматически.

Обычные символы, которые поступают после заполнения экрана до момента очистки, продолжают передаваться в Serial/BLE Serial, но не сохраняются в экранном буфере.

### Manual

После заполнения экрана появляется мигающий индикатор:

```text
CLR
```

Очистка выполняется коротким нажатием COMMAND.

---

# 30. Service Code при заполненном экране

Служебные сигналы имеют особый приоритет.

Если экран заполнен обычным текстом и затем распознан `ServiceCode`:

1. экран сразу очищается;
2. служебный код выводится на чистый экран;
3. описание выводится на экран;
4. сообщение обрезается, если оно длиннее четырёх строк;
5. полный текст передаётся в USB-Serial;
6. полный текст передаётся в BLE-Serial.

Это позволяет распознавать сервисные коды независимо от состояния обычного текстового буфера.

---

# 31. Serial

USB-Serial работает на:

```text
115200 baud
```

Основные события дублируются в Serial.

К ним относятся:

- Splash/status;
- вход в меню;
- значения настроек;
- изменение настроек;
- главное меню после выхода из подменю;
- About;
- страницы Morse Codes;
- страницы Service Codes;
- обычные декодированные символы;
- служебные коды.

Обычные символы передаются без добавления лишних символов.

При переходе на новую строку выполняется перевод строки.

---

# 32. BLE Serial

Используется NimBLE.

Имя устройства:

```text
CW TRAINER
```

UART-подобный BLE service:

```text
Service UUID:
6E400001-B5A3-F393-E0A9-E50E24DCCA9E

RX:
6E400002-B5A3-F393-E0A9-E50E24DCCA9E

TX:
6E400003-B5A3-F393-E0A9-E50E24DCCA9E
```

Передача BLE выполняется через FreeRTOS queue.

Основная логика Morse/keyer не блокируется вызовом BLE `notify()`.

Для передачи используется очередь:

```text
BLE_TX_QUEUE_LENGTH = 128
```

Каждый элемент очереди содержит до:

```text
20 bytes
```

BLE Serial используется как зеркало USB-Serial.

---

# 33. EEPROM

Настройки сохраняются в EEPROM.

Сохраняются:

```text
speedWPM
iambicModeA
swapPaddles
sidetoneEnabled
sidetoneFreq
autoClearScreen
backlightPercent
contrastPercent
displayRotated
language
```

Адреса:

```cpp
EEPROM_SETTINGS_ADDR = 4
EEPROM_LANGUAGE_ADDR = EEPROM_SETTINGS_ADDR + sizeof(KeyerSettings)
```

При недействительных значениях настройки заменяются значениями по умолчанию.

Значения по умолчанию в текущем исходнике:

```text
Speed WPM       = 15
Iambic          = Mode A
Swap DIT&DAH    = OFF
Beep            = ON
Sidetone        = 600 Hz
CLR scrn        = Auto
Backlight       = 100%
Contrast        = 50%
Display rot.    = UP
Language        = RU
```

---

# 34. Подсветка

Подсветка управляется PWM.

Параметры:

```text
GPIO        = 6
PWM channel = 5
Frequency   = 5000 Hz
Resolution  = 8 bit
```

Пользовательский диапазон:

```text
0–100%
```

Шаг:

```text
5%
```

---

# 35. Контраст

Пользовательский диапазон:

```text
50–100%
```

Шаг:

```text
5%
```

Значение преобразуется в диапазон контраста U8g2:

```text
100–180
```

---

# 36. Поворот дисплея

Доступны:

```text
UP
DOWN
```

Используются:

```cpp
U8G2_R0
U8G2_R2
```

---

# 37. About

Пункт `About` выводит информацию о проекте:

```text
Телеграфний тренажер
Автори:
github.com/LU6APR
Павло Лузан
```

Расширенная информация выводится в Serial/BLE Serial.

После выхода из `About` главное меню также повторно выводится в Serial/BLE Serial.

---

# 38. Splash Screen

При запуске отображается:

```text
CW TRAINER
by LU6APR
&
Pavlo Luzan
2026
```

Время отображения:

```text
3000 мс
```

После Splash Screen статусная строка передаётся в USB-Serial и BLE-Serial.

---

# 39. Структура программной логики

Основные подсистемы исходника:

```text
LIBRARIES
PIN CONFIGURATION
SETTINGS
MORSE TABLE
TEXT BUFFER
DECODER STATE
KEYER STATE
COMMAND BUTTON / MENU
TIMING
DISPLAY
TEXT BUFFER
MORSE DECODER
DECODED CHARACTER OUTPUT
LONG PAUSE
AUTO CLEAR
CW DECODER
PADDLES
TRANSMITTER
IAMBIC KEYER
DISPLAY HARDWARE CONTROL
MENU
MENU BUTTON HANDLING
COMMAND BUTTON
SPLASH SCREEN
BLE SERIAL
SETUP
LOOP
```

Главные функции:

```cpp
processKeyer()
processDecoder()
processCommand()
processMenu()

decodeMorse()
decodeServiceCode()
outputDecodedCharacter()
outputServiceCode()

drawMenu()
drawMorseCodesMenu()
printMorseCodesPageSerial()

serialPrintlnMirror()
printStatusLineSerial()
printSettingsSerial()
printMainMenuSerial()
```

---

# 40. Основной цикл

В обычном режиме:

```cpp
processKeyer();
processDecoder();
processCommand();
verificarAutoClear();
updateClearIndicator();
```

В режиме меню:

```cpp
processMenu();
```

В конце цикла используется короткая задержка:

```cpp
delay(2);
```

---

# 41. Сборка проекта

Проект предназначен для PlatformIO / VS Code.

Исходный файл текущей версии:

```text
main_BLE_FreeRTOS_v10_service_q_92code_punctuation_ru_en_fix_v41.cpp
```

Для сборки необходимо:

1. открыть проект в VS Code с PlatformIO;
2. выбрать конфигурацию платы ESP32-C3;
3. установить зависимости проекта;
4. выполнить Build;
5. прошить ESP32-C3;
6. открыть Serial Monitor на `115200 baud`.

Точный `platformio.ini` в данном исходном файле не содержится, поэтому параметры PlatformIO, которые не определены самим исходником, здесь намеренно не фиксируются.

---

# 42. Проверка после прошивки

Рекомендуемый порядок проверки:

### Запуск

Проверить:

```text
CW TRAINER
by LU6APR
&
Pavlo Luzan
2026
```

и статус в Serial.

### Ключ

Проверить:

```text
DIT
DAH
Iambic A
Iambic B
Swap DIT&DAH
```

### Декодер

Проверить:

```text
буквы EN
буквы RU
цифры
пунктуацию
```

### Service Codes

Проверить:

```text
SOS
QSL
QTH
VVV
KN
BK
12
92
99
```

Проверить, что формат:

```text
[SOS] <Emergency call for help.>
```

появляется одновременно на дисплее и в Serial.

### Меню

Проверить:

```text
Speed
Iambic
Beep
Language
CLR scrn
Backlight
Contrast
Display rot.
Swap DIT&DAH
Morse codes
About
```

После выхода из каждого подменю проверить восстановление главного меню в Serial.

### Morse Codes

Проверить все 9 страниц:

```text
EN × 2
RU × 2
Digits
Punctuation RU
Punctuation EN
Service Codes 3-letter
Service Codes 2-letter / 92
```

---
