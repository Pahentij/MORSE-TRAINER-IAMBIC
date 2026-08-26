Проєкт створено на основі:
https://github.com/LU6APR/ESP32_C3_MINI_MORSE_TRAINER

# MORSE TRAINER IAMBIC — LU6APR

Телеграфный тренажёр на ESP32-C3 с ямбичным ключом, аппаратным CW-вихідом, встроенным декодером азбуки Морзе, дисплеем Nokia 5110 / PCD8544 и двумя післядовательными интерфейсами: USB-Serial и Bluetooth-Serial.

Поточна зафиксированная версія исходного кода: **v41**.

## 1. Можливості

Проєкт об’єднує:

- ямбічний телеграфний ключ;
- Iambic Mode A та Mode B;
- введення DIT/DAH двома кнопками;
- регулювання швидкості 10–35 WPM з кроком 5 WPM;
- програмний sidetone;
- апаратний CW-вихід через `PIN_KEY_OUT`;
- декодування Morse у реальному часі;
- вибір мови декодування RU / EN;
- цифри та знаки пунктуації;
- додаткові підтримувані символи `_`, `+`, `&`;
- розпізнавання службових сигналів, основних Q-кодів і кодів системи 92;
- виведення службових кодів у форматі `[CODE] <Description>`;
- виведення повного тексту службових кодів у USB-Serial та Bluetooth-Serial;
- Nokia 5110 / PCD8544 84×48;
- налаштування підсвічування, контрастності та орієнтації дисплея;
- перестановку DIT і DAH;
- автоматичне або ручне очищення екрана;
- збереження налаштувань в EEPROM;
- BLE Serial на базі NimBLE та черги FreeRTOS;
- відображення таблиць Morse Codes і Service Codes безпосередньо на дисплеї.

---

## 2. Апаратна платформа

Проєкт розрахований на **ESP32-C3 Super Mini**.

Дисплей:

- Nokia 5110 / PCD8544;
- разрешение 84×48;
- програмний SPI;
- библиотека `U8g2`.

Основні бібліотеки:

```text
Arduino
EEPROM
U8g2lib
NimBLEDevice
FreeRTOS
```

---

## 3. Розпіновка

| Призначення | GPIO |
|---|---:|
| DIT | GPIO 2 |
| DAH | GPIO 3 |
| COMMAND | GPIO 0 |
| Sidetone / динамик | GPIO 1 |
| CW KEY OUT | GPIO 20 |
| Підсвічування LCD | GPIO 6 |
| LCD CLK | GPIO 4 |
| LCD DIN | GPIO 5 |
| LCD SCE | GPIO 7 |
| LCD DC | GPIO 8 |
| LCD RST | GPIO 10 |

DIT, DAH и COMMAND используют `INPUT_PULLUP`.

CW-вихід:

```text
PIN_KEY_OUT = GPIO 20
```

В активном состоянии вихід устанавливается в `HIGH`.

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

Основні параметри:

```cpp
TEXT_ROWS = 4
TEXT_COLS = 14
```

Таким чином, один екран основного текстового поля розрахований на:

```text
4 × 14 = 56 позиций
```

При стартовом екране `>READY<` ввод занимает строки 2–4, поэтому до первой ручной очистки доступно:

```text
3 × 14 = 42 позиции
```

Для відображення використовується кириличний шрифт U8g2:

```cpp
u8g2_font_5x7_t_cyrillic
u8g2_font_4x6_t_cyrillic
```

---

## 5. Рядок стану

После Splash Screen статус виведенняится в USB-Serial и Bluetooth-Serial.

Формат:

```text
15WPM Mode A RU BT
```

Вміст:

- швидкість WPM;
- Iambic Mode A/B;
- мова RU/EN;
- `BT`, якщо BLE-клієнт підключений.

На дисплее статусная строка размещена в нижней части екрана.

---

## 6. Керування ключем

### DIT

Коротке натискання/утримання DIT формує крапку:

```text
.
```

### DAH

Коротке натискання/утримання DAH формує тире:

```text
-
```

Тривалість елементів визначається швидкістю WPM.

Формулы:

```text
DIT = 1200 / WPM мс
DAH = 3 × DIT
міжсимвольний інтервал = 3 × DIT
міжслівний інтервал = 7 × DIT
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

Робота стиснення paddles відповідає Iambic A.

### Mode B

Якщо обидва paddles були затиснуті під час поточного елемента, після його завершення може бути автоматично сформований додатковий протилежний елемент.

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

Дозволяє поміняти місцями логічні функції фізичних вхідів.

```text
OFF:
фізичний DIT → DIT
фізичний DAH → DAH

ON:
фізичний DIT → DAH
фізичний DAH → DIT
```

---

## 9. Sidetone

Параметр:

```text
Beep
```

Значення:

```text
ON
OFF
```

Робоча частота sidetone:

```text
600 Hz
```

Сигнали меню використовують додаткові короткі звуки підтвердження.

---

## 10. Керування COMMAND

Кнопка `COMMAND` виконує дві основні функції.

### Тривале натискання

Порог:

```text
1000 мс
```

Тривале натискання открывает главное меню.

### Коротке натискання

У звичайному режимі очищає екран.

Перед очищенням в USB-Serial и Bluetooth-Serial выполняется перевод строки.

Після очищення відображається:

```text
>READY<
```

---

## 11. Головне меню

Головне меню містить:

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

Під час вхіду в меню вказівник встановлюється на:

```text
Back
```

Під час відкриття меню всі поточні налаштування виводяться в USB-Serial та Bluetooth-Serial.

Приклад:

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

Після зміни будь-якого параметра повний набір налаштувань повторно виводиться в Serial.

Після виходу з будь-якого підменю головне меню також виводиться в Serial/BLE Serial.

---

## 12. Навігація меню

В главном меню:

```text
DIT  → наступний пункт
DAH  → попередній пункт
COMMAND → відкрити вибраний пункт
```

В большинстве меню налаштування:

```text
DIT  → зменшення / перший варіант
DAH  → збільшення / другий варіант
COMMAND → зберегти та повернутися до головного меню
```

В `Morse codes`:

```text
DIT → наступна страница
DAH → предыдущая страница
COMMAND → повернутися до головного меню
```

---

## 13. Швидкість WPM

Діапазон:

```text
10–35 WPM
```

Крок:

```text
5 WPM
```

Циклічна зміна:

```text
10 → 15 → 20 → 25 → 30 → 35 → 10
```

---

## 14. Language

Доступні:

```text
RU
EN
```

У меню:

```text
DIT = RU
DAH = ENG
COMMAND = OK
```

Мова впливає на декодування літер і варіантів пунктуації.

Важливо: служебные сигналы, Q-коды и коды системы 92 розпізнаються незалежно від значення `Language`.

---

# 15. Декодер Morse

Декодер працює безпосередньо з часовими параметрами CW-сигналу.

Після завершення Morse-послідовності:

1. визначається тривалість кожного елемента;
2. формується двійкова Morse-послідовність;
3. спочатку перевіряється `serviceCodes[]`;
4. потім виконується звичайне декодування символу;
5. результат виводиться на дисплей і в Serial/BLE Serial.

Невідома або неприпустима послідовність повертає:

```text
*
```

---

## 16. Пріоритет розпізнавання

Порядок розпізнавання є принциповим.

Спочатку перевіряються:

```text
Service signals
Q-codes
92 Code
```

и лише після этого:

```text
звичайні Morse-символи
```

Це необхідно, оскільки частина службових кодів має послідовності, які не можна обробляти як незалежні літери.

---

# 17. Служебные сигналы, Q-коды и 92 Code

У проєкті використовується масив:

```cpp
const ServiceCode serviceCodes[] PROGMEM
```

Кожен запис містить:

```cpp
codeText
description
length
code
```

Під час розпізнавання сервісного коду:

- дисплей негайно очищається;
- на екран виводиться код та опис;
- повідомлення обрізається відповідно до розміру дисплея;
- повний текст передається в USB-Serial;
- повний текст передається в Bluetooth-Serial;
- перед сервісним кодом забезпечується переведення рядка в Serial;
- після повідомлення виконується переведення рядка.

Формат:

```text
[SOS] <Emergency call for help.>
```

---

## 18. Текущие Service Codes

### Трёхлітеренные

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

### Двухлітеренные

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

Текст выше приведён непосредственно по массиву `serviceCodes[]` поточної версии v41.

---

# 19. Страницы Service Codes в меню Morse codes

У `Morse codes` додано дві сторінки.

### Страница 7

```text
SERVICE CODES - 3 LETTER
```

На дисплеї коди розташовуються у:

```text
4 столбца × 7 строк
```

Формат:

```text
[SOS] [VVV] [QTH] [QRP]
...
```

У Serial/BLE Serial виводиться повний текст:

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

В Serial/BLE Serial виведенняится полное описание каждого кода:

```text
[KN], Others please stand by
[BK], Break.
[HH], Error.
...
[12], Do you understand?
[13], I understand.
```

Страницы переключаются кнопками DIT и DAH.

После післядней страницы выполняется переход обратно на первую.

---

# 20. Morse Codes — страницы

Меню `Morse codes` містить 9 сторінок.

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

Сторінка 1:

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

Сторінка 2:

```text
Y -.--  Z --..
```

---

## 22. RU Morse

Сторінка 1:

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

Сторінка 2:

```text
Ш ---  Ь ---
Щ ---  Э ----.
Ъ ---- Ю ---
Ы ---  Я ---
```

---

## 23. Цифри

Сторінка:

```text
0 -----     5 .....
1 .----     6 -....
2 ..---     7 --...
3 ...--     8 ---..
4 ....-     9 ----.
```

Цифры распознаются независимо от вибраного языка.

---

# 24. Пунктуация RU

Сторінка:

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

Для поточної реализации следует учитывать, что некоторые Morse-коды совпадают. В частности, согласно таблице проекта точка и запятая имеют одинаковую післядовательность, поэтому декодер не может определить исходный символ лише по Morse-післядовательности без дополнительного контекста.

---

# 25. Пунктуация EN

Сторінка:

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

EN имеет отдельную ветку декодування пунктуації.

Полные варианты для `"` и `'` використовуються в соответствии с поточної реализацией:

```text
"  .-..-.
'  .----.
```

---

# 26. Дополнительные символы

У таблиці проєкту також присутні:

```text
_  .-..-.
+  .-.-.
&  ..-.
```

Ці символи є підтримуваними елементами `morseTable`.

---

# 27. Особенности скобок

В EN використовується єдиний Morse-код:

```text
-.--.-
```

Для нього декодер повертає спеціальне значення:

```cpp
MORSE_PARENTHESES_CODE
```

Після цього виводиться:

```text
()
```

У RU відкривна та закривна дужки мають різні коди:

```text
(  -.--.
)  -.--.-
```

---

# 28. Автоматические пробелы

Після паузи:

```text
1000 мс
```

декодер додає пробіл між словами.

Межлітеренный и міжслівний інтервалы також определяются текущим WPM.

---

# 29. Очистка екрана

Параметр:

```text
CLR scrn
```

має два режими.

### Auto

Після заповнення доступного текстового поля запускається затримка:

```text
500 мс
```

Після неї екран очищається автоматично.

Звичайні символи, які надходять після заповнення екрана до моменту очищення, продовжують передаватися в Serial/BLE Serial, але не зберігаються в буфері екрана.

### Manual

Після заповнення екрана з’являється миготливий індикатор:

```text
CLR
```

Очищення виконується коротким натисканням COMMAND.

---

# 30. Service Code при заполненном екране

Службові сигнали мають особливий пріоритет.

Якщо екран заповнений звичайним текстом і потім розпізнано `ServiceCode`:

1. екран одразу очищається;
2. службовий код виводиться на чистий екран;
3. опис виводиться на екран;
4. повідомлення обрізається, якщо воно довше чотирьох рядків;
5. повний текст передається в USB-Serial;
6. повний текст передається в BLE-Serial.

Это дозволяє распознавать сервисные коды независимо от состояния обычного текстового буфера.

---

# 31. Serial

USB-Serial працює на:

```text
115200 baud
```

Основні події дублюються в Serial.

До них належать:

- Splash/status;
- вхід у меню;
- значення налаштувань;
- зміна налаштувань;
- головне меню після виходу з підменю;
- About;
- сторінки Morse Codes;
- сторінки Service Codes;
- звичайні декодовані символи;
- службові коди.

Звичайні символи передаються без додавання зайвих символів.

Під час переходу на новий рядок виконується переведення рядка.

---

# 32. BLE Serial

Використовується NimBLE.

Ім’я пристрою:

```text
CW TRAINER
```

UART-подібний BLE service:

```text
Service UUID:
6E400001-B5A3-F393-E0A9-E50E24DCCA9E

RX:
6E400002-B5A3-F393-E0A9-E50E24DCCA9E

TX:
6E400003-B5A3-F393-E0A9-E50E24DCCA9E
```

Передача BLE виконується через чергу FreeRTOS.

Основна логіка Morse/keyer не блокується викликом BLE `notify()`.

Для передачі використовується черга:

```text
BLE_TX_QUEUE_LENGTH = 128
```

Кожен елемент черги містить до:

```text
20 bytes
```

BLE Serial використовується як дзеркало USB-Serial.

---

# 33. EEPROM

Налаштування зберігаються в EEPROM.

Зберігаються:

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

Якщо значення недійсні, налаштування замінюються значеннями за замовчуванням.

Значення за замовчуванням у поточному вихідному коді:

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

Підсвічування керується PWM.

Параметры:

```text
GPIO        = 6
PWM channel = 5
Frequency   = 5000 Hz
Resolution  = 8 bit
```

Користувацький діапазон:

```text
0–100%
```

Крок:

```text
5%
```

---

# 35. Контраст

Користувацький діапазон:

```text
50–100%
```

Крок:

```text
5%
```

Значення перетворюється в діапазон контрасту U8g2:

```text
100–180
```

---

# 36. Поворот дисплея

Доступні:

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

Пункт `About` виводить інформацію про проєкт:

```text
Телеграфний тренажер
Автори:
github.com/LU6APR
Павло Лузан
```

Розширена інформація виводиться в Serial/BLE Serial.

Після виходу з `About` головне меню також повторно виводиться в Serial/BLE Serial.

---

# 38. Splash Screen

Під час запуску відображається:

```text
CW TRAINER
by LU6APR
&
Pavlo Luzan
2026
```

Час відображення:

```text
3000 мс
```

Після Splash Screen рядок стану передається в USB-Serial та BLE-Serial.

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

Основні функції:

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

# 40. Основний цикл

У звичайному режимі:

```cpp
processKeyer();
processDecoder();
processCommand();
verificarAutoClear();
updateClearIndicator();
```

У режимі меню:

```cpp
processMenu();
```

Наприкінці циклу використовується коротка затримка:

```cpp
delay(2);
```

---

# 41. Сборка проекта

Проєкт призначений для PlatformIO / VS Code.

Вихідний файл поточної версії:

```text
main_BLE_FreeRTOS_v10_service_q_92code_punctuation_ru_en_fix_v41.cpp
```

Для збірки необхідно:

1. відкрити проєкт у VS Code з PlatformIO;
2. вибрати конфігурацію плати ESP32-C3;
3. встановити залежності проєкту;
4. виконати Build;
5. прошити ESP32-C3;
6. відкрити Serial Monitor на `115200 baud`.

Точний `platformio.ini` у цьому вихідному файлі не міститься, тому параметри PlatformIO, які не визначені самим вихідним кодом, тут навмисно не фіксуються.

---

# 42. Проверка після прошивки

Рекомендований порядок перевірки:

### Запуск

Перевірити:

```text
CW TRAINER
by LU6APR
&
Pavlo Luzan
2026
```

и статус в Serial.

### Ключ

Перевірити:

```text
DIT
DAH
Iambic A
Iambic B
Swap DIT&DAH
```

### Декодер

Перевірити:

```text
літеры EN
літеры RU
цифри
пунктуацию
```

### Service Codes

Перевірити:

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

Перевірити, що формат:

```text
[SOS] <Emergency call for help.>
```

появляется одновременно на дисплее и в Serial.

### Меню

Перевірити:

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

После вихіда из каждого подменю проверить восстановление главного меню в Serial.

### Morse Codes

Перевірити всі 9 сторінок:

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
