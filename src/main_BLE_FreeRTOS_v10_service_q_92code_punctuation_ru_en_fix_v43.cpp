/*
  MORSE TRAINER IAMBIC — LU6APR
  PlatformIO / VS Code
  Nokia 5110 / PCD8544 version

  Based on the verified Nokia 5110 v8 logic. The keyer, COMMAND button,
  menu navigation, WPM, Iambic A/B, EEPROM, sidetone and decoder timing
  are preserved. Only the display layer and decoded character type are
  changed for the Nokia 5110.
*/

// ============================================================
// LIBRARIES
// ============================================================
#include <Arduino.h>
#include <EEPROM.h>

#include <U8g2lib.h>
#include <NimBLEDevice.h>

// ============================================================
// PIN CONFIGURATION — VERIFIED WORKING v8 PINS
// ============================================================
#define PIN_DIT         2    //Вход DIT
#define PIN_DAH         3    //Вход DAH
#define PIN_COMMAND     0    //Вход кнопка очистки экрана/входа в меню
#define PIN_SIDETONE    1    //Выход на динамик
#define PIN_KEY_OUT     20   //Выход на оптрон
#define PIN_BACKLIGHT   6    //Выход на подсветку дисплея, PWM

// Nokia 5110 backlight PWM.
// Keep this channel separate from the LEDC channel used by tone().
constexpr uint8_t BACKLIGHT_CHANNEL = 5;
constexpr uint32_t BACKLIGHT_FREQ = 5000;
constexpr uint8_t BACKLIGHT_RESOLUTION = 8;

// Nokia 5110 / PCD8544 — verified by standalone hardware test.
#define PIN_LCD_CLK     4
#define PIN_LCD_DIN     5
#define PIN_LCD_SCE     7
#define PIN_LCD_DC      8
#define PIN_LCD_RST     10

U8G2_PCD8544_84X48_F_4W_SW_SPI lcd(
  U8G2_R0,
  PIN_LCD_CLK,
  PIN_LCD_DIN,
  PIN_LCD_SCE,
  PIN_LCD_DC,
  PIN_LCD_RST
);

constexpr uint8_t TEXT_ROWS = 4;
constexpr uint8_t TEXT_COLS = 14;
constexpr uint16_t TEXT_AREA_LENGTH = TEXT_ROWS * TEXT_COLS;
constexpr uint8_t TEXT_ROW_Y[TEXT_ROWS] = {7, 17, 27, 37};
constexpr uint8_t STATUS_DIVIDER_Y = 40;
constexpr uint8_t STATUS_TEXT_Y = 47;

// Status bar layout for 84 px Nokia 5110 display.
constexpr uint8_t STATUS_X_WPM  = 0;
constexpr uint8_t STATUS_X_MODE = 18;
constexpr uint8_t STATUS_X_LANG = 72;

// ============================================================
// SETTINGS
// ============================================================
// Default display settings — change only this line when needed.
constexpr uint8_t DEFAULT_BACKLIGHT_PERCENT = 50, DEFAULT_CONTRAST_PERCENT = 65;

struct KeyerSettings {
  uint8_t speedWPM;
  bool iambicModeA;
  bool swapPaddles;
  bool sidetoneEnabled;
  uint16_t sidetoneFreq;
  bool autoClearScreen;
  uint8_t backlightPercent;
  uint8_t contrastPercent;
  bool displayRotated;
};

// ============================================================
// DEFAULT SETTINGS
// ============================================================
KeyerSettings settings = {15, true, false, true, 600, true, DEFAULT_BACKLIGHT_PERCENT, DEFAULT_CONTRAST_PERCENT, false};

// false = international Morse / Latin, true = Russian Morse / Cyrillic.
// A language selector is required because many codes are identical.
bool morseCyrillic = true;

constexpr int EEPROM_SETTINGS_ADDR = 4;
constexpr int EEPROM_LANGUAGE_ADDR = EEPROM_SETTINGS_ADDR + sizeof(KeyerSettings);

// ============================================================
// MORSE TABLE
// ============================================================
struct MorseChar {
  uint32_t character;
  uint8_t length;
  uint32_t code;
};

struct ServiceCode {
  const char* codeText;
  const char* description;
  uint8_t length;
  uint32_t code;
};

const ServiceCode serviceCodes[] PROGMEM = {
  {"SOS", "Emergency call for help.", 9, 0b000111000},
  {"KN", "Others please stand by", 5, 0b10101},
  {"BK", "Break.", 7, 0b1000101},
  {"HH", "Error.", 8, 0b00000000},
  {"SK", "End of contact.", 6, 0b000101},
  {"CL", "Closing station.", 8, 0b10100100},
  {"CQ", "Calling any station!", 8, 0b10101101},
  {"VVV", "Testing signal.", 12, 0b000100010001},
  {"QTH", "My location is...", 9, 0b110110000},
  {"QRP", "Operating with low power.", 11, 0b11010100110},
  {"QRZ", "You are being called by...", 11, 0b11010101100},
  {"QSL", "I acknowledge receipt.", 11, 0b11010010010},
  {"QSO", "Direct communication is established with...", 10, 0b1101000111},
  {"QRL", "I am busy.", 11, 0b11010101100},
  {"QRM", "Experiencing man-made interference.", 9, 0b110101011},
  {"QRN", "Troubled by atmospheric noise (static).", 9, 0b110101010},
  {"QRT", "Stopping transmission / Going off air.", 8, 0b11010101},
  {"QRO", "Increasing transmitter power.", 10, 0b1101010111},
  {"QRQ", "Send faster.", 11, 0b11010101101},
  {"QRS", "Send more slowly.", 10, 0b1101010000},
  {"QSB", "Your signals are fading.", 11, 0b11010001000},
  {"QRK", "The intelligibility of your signals is... (1-5).", 10, 0b1101010101},
  {"QSA", "The strength of your signals is... (1-5).", 10, 0b1101000010},
  {"QRV", "I am ready.", 11, 0b11010100001},
  {"QRX", "I will call you again at... / Standby.", 11, 0b11010101001},
  {"QTR", "The correct time is...", 8, 0b11011010},
  {"12", "Do you understand?", 10, 0b0111100111},
  {"13", "I understand.", 10, 0b0111100011},
  {"14", "What is the weather?", 10, 0b0111100001},
  {"15", "For your information.", 10, 0b0111100000},
  {"17", "Breaking news item", 10, 0b0111111000},
  {"18", "What are your instructions?", 10, 0b0111111100},
  {"19", "Important message.", 10, 0b0111111110},
  {"21", "Stop for meal.", 10, 0b0011101111},
  {"22", "Love and greetings.", 10, 0b0011100111},
  {"23", "Message for all.", 10, 0b0011100011},
  {"24", "Repeat this back.", 10, 0b0011100001},
  {"25", "Busy on another circuit.", 10, 0b0011100000},
  {"30", "The End.", 10, 0b0001111111},
  {"33", "Best regards (Between female operators).", 10, 0b0001100011},
  {"44", "Respects to nature. (Modern addition used in park activations)", 10, 0b0000100001},
  {"55", "Best success. / Friendly handshake.", 10, 0b0000000000},
  {"73", "Best regards.", 9, 0b110000011},
  {"88", "Love and kisses.", 10, 0b1110011100},
  {"92", "Deliver to all stations.", 10, 0b1111000111},
  {"99", "Get off this frequency).", 10, 0b1111101111},
};

constexpr size_t SERVICE_CODE_COUNT =
  sizeof(serviceCodes) / sizeof(serviceCodes[0]);

#define MORSE_PARENTHESES_CODE 0xFFFFFFFEUL

const MorseChar morseTable[] PROGMEM = {
  // Latin A-Z
  {0x0041,2,0b01},{0x0042,4,0b1000},{0x0043,4,0b1010},{0x0044,3,0b100},
  {0x0045,1,0b0},{0x0046,4,0b0010},{0x0047,3,0b110},{0x0048,4,0b0000},
  {0x0049,2,0b00},{0x004A,4,0b0111},{0x004B,3,0b101},{0x004C,4,0b0100},
  {0x004D,2,0b11},{0x004E,2,0b10},{0x004F,3,0b111},{0x0050,4,0b0110},
  {0x0051,4,0b1101},{0x0052,3,0b010},{0x0053,3,0b000},{0x0054,1,0b1},
  {0x0055,3,0b001},{0x0056,4,0b0001},{0x0057,3,0b011},{0x0058,4,0b1001},
  {0x0059,4,0b1011},{0x005A,4,0b1100},

  // Numbers
  {0x0030,5,0b11111},{0x0031,5,0b01111},{0x0032,5,0b00111},{0x0033,5,0b00011},
  {0x0034,5,0b00001},{0x0035,5,0b00000},{0x0036,5,0b10000},{0x0037,5,0b11000},
  {0x0038,5,0b11100},{0x0039,5,0b11110},

  // Punctuation — RU / EN variants according to the supplied comparison.
  // ! : RU --..-- ; EN -.-.--
  // ' : RU .----. ; EN .----
  // ; : RU -.-.-. ; EN -.-.-.
  // / : text form -..-. ; callsign short form is not distinguishable here
  // " : RU .-..-. ; EN .-..-
  // ( ) : RU has separate forms; EN uses one universal form.

  {0x002E,6,0b010101}, // .  .-.-.-
  {0x002C,6,0b110011}, // ,  --..--
  {0x003A,6,0b111000}, // :  ---...
  {0x003B,6,0b101010}, // ;  -.-.-.
  {0x003F,6,0b001100}, // ?  ..--..
  {0x0021,6,0b110011}, // !  --..--  (RU)
  {0x0021,6,0b101011}, // !  -.-.--  (EN)
  {0x002D,6,0b100001}, // -  -....-
  {0x002F,5,0b10010},  // /  -..-.
  {0x003D,5,0b10001},  // =  -...-
  {0x0022,6,0b010010}, // "  .-..-.  (RU)
  {0x0022,5,0b01001},  // "  .-..-   (EN)
  {0x0027,6,0b011110}, // '  .----.  (RU)
  {0x0027,5,0b01111},  // '  .----   (EN)
  {0x0028,5,0b10110},  // (  -.--.  (RU)
  {0x0029,6,0b101101}, // )  -.--.-  (RU)
  {0x0028,6,0b101101}, // (  -.--.-  (EN)
  {0x0029,6,0b101101}, // )  -.--.-  (EN)
  {0x0040,6,0b011010}, // @  .--.-.

  // Additional project-supported symbols retained from the previous version.
  {0x005F,6,0b100110}, // _
  {0x002B,5,0b01010},   // +
  {0x0026,5,0b01000},   // &
  // Russian Cyrillic А-Я.
  // Ё intentionally maps to the same Morse code as Е and therefore cannot
  // be distinguished during decoding; it is not included in the decoder.
  {0x0410,2,0b01},    // А .-
  {0x0411,4,0b1000},  // Б -...
  {0x0412,3,0b011},   // В .--
  {0x0413,3,0b110},   // Г --.
  {0x0414,3,0b100},   // Д -..
  {0x0415,1,0b0},     // Е .
  {0x0416,4,0b0001},  // Ж ...-
  {0x0417,4,0b1100},  // З --..
  {0x0418,2,0b00},    // И ..
  {0x0419,4,0b0111},  // Й .---
  {0x041A,3,0b101},   // К -.-
  {0x041B,4,0b0100},  // Л .-..
  {0x041C,2,0b11},    // М --
  {0x041D,2,0b10},    // Н -.
  {0x041E,3,0b111},   // О ---
  {0x041F,4,0b0110},  // П .--.
  {0x0420,3,0b010},   // Р .-.
  {0x0421,3,0b000},   // С ...
  {0x0422,1,0b1},     // Т -
  {0x0423,3,0b001},   // У ..-
  {0x0424,4,0b0010},  // Ф ..-.
  {0x0425,4,0b0000},  // Х ....
  {0x0426,4,0b1010},  // Ц -.-.
  {0x0427,4,0b1110},  // Ч ---. 
  {0x0428,4,0b1111},  // Ш ----
  {0x0429,4,0b1101},  // Щ --.-
  {0x042A,5,0b11010}, // Ъ --.-.
  {0x042B,4,0b1011},  // Ы -.--
  {0x042C,4,0b1001},  // Ь -..-
  {0x042D,5,0b00100}, // Э ..-..
  {0x042E,4,0b0011},  // Ю ..--
  {0x042F,4,0b0101}   // Я .-.-
};

// ============================================================
// TEXT BUFFER
// Nokia 5110: 4 text rows x 14 characters = 56 positions.
// The fifth row is reserved for WPM + Iambic status.
// ============================================================
constexpr uint16_t MAX_TEXT_LENGTH = TEXT_AREA_LENGTH;

uint32_t textBuffer[MAX_TEXT_LENGTH];
uint16_t caracteresEnPantalla = 0;
uint8_t serialDisplayRow = 0;

// True when the next Serial character starts a new line.
// This is independent of the display buffer because ServiceCode output
// may clear the display without changing the Serial stream position.
bool serialAtLineStart = true;

// At startup the first row contains >READY<.
// Input is then accepted on rows 2-4 only.
bool readyScreen = true;

// When the available text area is full, decoding is paused until
// PIN_COMMAND is pressed to confirm clearing the screen.
bool clearWaiting = false;
bool clearIndicatorVisible = true;
unsigned long clearBlinkTimer = 0;
constexpr unsigned long CLEAR_BLINK_INTERVAL_MS = 400;

// Retained for compatibility with the existing settings/state structure.
// A full ordinary-text screen is no longer cleared automatically.
bool autoClearPending = false;
unsigned long autoClearTimer = 0;
constexpr unsigned long AUTO_CLEAR_DELAY_MS = 500;

// Number of text positions available before the first manual clear:
// rows 2-4 = 3 * 14 = 42 characters.
constexpr uint16_t READY_TEXT_CAPACITY =
  (TEXT_ROWS - 1) * TEXT_COLS;

// ============================================================
// DECODER STATE
// ============================================================
uint32_t currentCode = 0;
uint8_t currentLength = 0;

unsigned long lastChangeTime = 0;
bool isKeyDown = false;
unsigned long keyDownStart = 0;

unsigned long lastKeyActivityTime = 0;
bool espacioAgregadoPorPausa = false;

constexpr unsigned long PAUSA_ESPACIO_MS = 1000;

// ============================================================
// KEYER STATE
// ============================================================
enum KeyerState {
  IDLE,
  KEY_DOWN,
  INTER_ELEMENT
};

KeyerState keyerState = IDLE;

unsigned long keyerTimer = 0;

bool txActive = false;

// Latched paddle requests for the next element.
volatile uint8_t paddleState = 0;

constexpr uint8_t DIT_MASK = 0x01;
constexpr uint8_t DAH_MASK = 0x02;

// Element currently being transmitted.
// true  = DIT
// false = DAH
bool currentElementIsDit = true;

// Set when both paddles were squeezed at some point during the
// current element. This is the information required by Iambic B
// to generate the final opposite element after release.
bool bothPaddlesSeen = false;

// ============================================================
// COMMAND BUTTON / MENU
// ============================================================
bool lastBtnState = HIGH;
unsigned long commandPressStart = 0;
bool commandLongHandled = false;
bool menuCommandArmed = false;

constexpr unsigned long COMMAND_LONG_PRESS_MS = 1000;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 35;

unsigned long lastCommandEdgeTime = 0;

enum MenuState {
  MENU_OFF,
  MENU_MAIN,
  MENU_SPEED,
  MENU_IAMBIC,
  MENU_BEEP,
  MENU_LANGUAGE,
  MENU_CLEAR,
  MENU_BACKLIGHT,
  MENU_CONTRAST,
  MENU_DISPLAY_ROT,
  MENU_SWAP_DIT_DAH,
  MENU_ABOUT,
  MENU_MORSE_CODES
};

MenuState menuState = MENU_OFF;

uint8_t menuItem = 0;
uint8_t menuScroll = 0;

// Morse codes submenu pages:
// 0 — EN page 1
// 1 — EN page 2
// 2 — RU page 1
// 3 — RU page 2
// 4 — digits
// 5 — punctuation RU
// 6 — punctuation EN
// 7 — service codes: 3-letters
// 8 — service codes: 2-letters and 92 codes.
uint8_t morseCodesPage = 0;

constexpr uint8_t MORSE_CODES_LAST_PAGE = 8;
constexpr uint8_t SERVICE_CODES_COLUMNS = 4;
constexpr uint8_t SERVICE_CODES_ROWS = 7;
constexpr uint8_t SERVICE_CODES_PER_PAGE =
  SERVICE_CODES_COLUMNS * SERVICE_CODES_ROWS;

constexpr uint8_t MENU_VISIBLE_ITEMS = 5;
constexpr uint8_t MENU_ITEM_BACK = 0;
constexpr uint8_t MENU_ITEM_SPEED = 1;
constexpr uint8_t MENU_ITEM_IAMBIC = 2;
constexpr uint8_t MENU_ITEM_BEEP = 3;
constexpr uint8_t MENU_ITEM_LANGUAGE = 4;
constexpr uint8_t MENU_ITEM_CLEAR = 5;
constexpr uint8_t MENU_ITEM_BACKLIGHT = 6;
constexpr uint8_t MENU_ITEM_CONTRAST = 7;
constexpr uint8_t MENU_ITEM_DISPLAY_ROT = 8;
constexpr uint8_t MENU_ITEM_SWAP_DIT_DAH = 9;
constexpr uint8_t MENU_ITEM_MORSE_CODES = 10;
constexpr uint8_t MENU_ITEM_ABOUT = 11;
constexpr uint8_t MENU_ITEMS = 12;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================
void updateDisplay();
void updateStatusLine();
void printStatusLineSerial();
void serialPrintlnMirror(const char* text);
void printSettingsSerial();
void printMainMenuSerial();
void clearBuffer();
void addCharToBuffer(uint32_t c);
void addSpaceToBuffer();
void verificarPausaLarga();
void verificarAutoClear();
void processDecoder();
void processKeyer();
void processCommand();
void processMenu();
void startTransmit();
void stopTransmit();
uint8_t readPaddles();
#define MORSE_PARENTHESES_CODE 0xFFFFFFFEUL
uint32_t decodeMorse(uint32_t code, uint8_t length);
int decodeServiceCode(uint32_t code, uint8_t length);
void outputServiceCode(size_t index);
void showSplash();
void showMessage(const char* line1, const char* line2 = nullptr);
void drawMenu();
void enterMenu();
void exitMenu();
void saveSettings();
void menuUp();
void menuDown();
void menuSelect();
void menuBeep(bool selectBeep = false);
void drawBeepMenu();
void drawLanguageMenu();
void drawClearMenu();
void drawBacklightMenu();
void drawContrastMenu();
void drawDisplayRotMenu();
void drawSwapDitDahMenu();
void drawAboutMenu();
void drawMorseCodesMenu();
void morseCodesNextPage();
void printMorseCodesPageSerial();
void morseCodesPrevPage();
void applyDisplayRotation();
void updateClearIndicator();
void setReadyScreen();
void applyBacklight();
void applyContrast();
void outputDecodedCharacter(uint32_t c);

// ============================================================
// TIMING
// ============================================================
unsigned long getDitDuration() {
  return 1200UL / settings.speedWPM;
}

unsigned long getDahDuration() {
  return getDitDuration() * 3;
}

unsigned long getInterLetterGap() {
  return getDitDuration() * 3;
}

unsigned long getInterWordGap() {
  return getDitDuration() * 7;
}

// ============================================================
// NOKIA 5110 DISPLAY HELPERS
// ============================================================
void drawCodepoint(uint32_t codepoint, uint8_t x, uint8_t baseline) {
  char utf8[5] = {0};

  if (codepoint <= 0x7F) {
    utf8[0] = (char)codepoint;
  }
  else if (codepoint <= 0x7FF) {
    utf8[0] = (char)(0xC0 | (codepoint >> 6));
    utf8[1] = (char)(0x80 | (codepoint & 0x3F));
  }
  else {
    utf8[0] = (char)(0xE0 | (codepoint >> 12));
    utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    utf8[2] = (char)(0x80 | (codepoint & 0x3F));
  }

  lcd.drawUTF8(x, baseline, utf8);
}

void showMessage(const char* line1, const char* line2) {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  if (line1 != nullptr)
    lcd.drawStr(0, 7, line1);

  if (line2 != nullptr)
    lcd.drawStr(0, 20, line2);

  lcd.sendBuffer();
  delay(800);
}

// ============================================================
// DISPLAY
// ============================================================
extern volatile bool bleConnected;
void updateDisplay();

void printStatusLineSerial() {
  char line[40];
  snprintf(line, sizeof(line), "%dWPM %s %s %s",
           settings.speedWPM,
           settings.iambicModeA ? "Mode A" : "Mode B",
           morseCyrillic ? "RU" : "EN",
           bleConnected ? "BT" : "");
  serialPrintlnMirror(line);
}

void printSettingsAfterChange() {
  printSettingsSerial();
}

void printMainMenuSerial() {
  serialPrintlnMirror("");
  serialPrintlnMirror("MENU");
  serialPrintlnMirror("Back");

  char line[64];

  snprintf(line, sizeof(line), "Speed, WPM: %d", settings.speedWPM);
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Iambic mode: Mode %c",
           settings.iambicModeA ? 'A' : 'B');
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Beep: %s",
           settings.sidetoneEnabled ? "ON" : "OFF");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Language: %s",
           morseCyrillic ? "RU" : "ENG");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "CLR scrn: %s",
           settings.autoClearScreen ? "Auto" : "Manual");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Backlight: %d%%",
           settings.backlightPercent);
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Contrast: %d%%",
           settings.contrastPercent);
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Display rot.: %s",
           settings.displayRotated ? "DOWN" : "UP");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Swap DIT&DAH: %s",
           settings.swapPaddles ? "ON" : "OFF");
  serialPrintlnMirror(line);

  serialPrintlnMirror("Morse codes");
  serialPrintlnMirror("About");
  serialPrintlnMirror("");
}

void printSettingsSerial() {
  serialPrintlnMirror("");
  serialPrintlnMirror("SETTINGS");

  char line[64];

  snprintf(line, sizeof(line), "Speed, WPM: %d", settings.speedWPM);
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Iambic mode: Mode %c",
           settings.iambicModeA ? 'A' : 'B');
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Beep: %s",
           settings.sidetoneEnabled ? "ON" : "OFF");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Language: %s",
           morseCyrillic ? "RU" : "EN");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "CLR scrn: %s",
           settings.autoClearScreen ? "Auto" : "Manual");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Backlight: %d%%",
           settings.backlightPercent);
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Contrast: %d%%",
           settings.contrastPercent);
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Display rot.: %s",
           settings.displayRotated ? "DOWN" : "UP");
  serialPrintlnMirror(line);

  snprintf(line, sizeof(line), "Swap DIT&DAH: %s",
           settings.swapPaddles ? "ON" : "OFF");
  serialPrintlnMirror(line);

  serialPrintlnMirror("About");
  serialPrintlnMirror("");
}

void updateStatusLine() {
  // Compact status row:
  // 15WPM  Mode B  RU  BT
  // BT is shown only while a BLE client is connected.
  lcd.setFont(u8g2_font_4x6_t_cyrillic);

  // Thin separator line above the status row.
  lcd.drawHLine(0, STATUS_DIVIDER_Y, 84);

  char wpmText[8];
  snprintf(wpmText, sizeof(wpmText), "%dWPM", settings.speedWPM);
  lcd.drawStr(0, STATUS_TEXT_Y, wpmText);

  const char* modeText = settings.iambicModeA ? "Mode A" : "Mode B";
  lcd.drawStr(22, STATUS_TEXT_Y, modeText);

  lcd.drawStr(52, STATUS_TEXT_Y, morseCyrillic ? "RU" : "EN");

  if (bleConnected)
    lcd.drawStr(68, STATUS_TEXT_Y, "BT");
}

void drawReadyText() {
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  // 7 characters × 6 pixels = 42 pixels.
 
  lcd.drawStr(25, TEXT_ROW_Y[0], ">READY<");
}

void drawClearIndicator() {
  if (!clearWaiting || !clearIndicatorVisible)
    return;

  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  // 3 characters × 6 pixels = 18 pixels.
  // Right aligned on an 84-pixel display.
  lcd.drawStr(66, STATUS_TEXT_Y, "CLR");
}

void updateDisplay() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  if (readyScreen) {
    drawReadyText();

    // Input starts on the second row.
    for (uint16_t i = 0; i < caracteresEnPantalla && i < READY_TEXT_CAPACITY; i++) {
      uint8_t row = 1;
      uint8_t col = 0;

      for (uint16_t j = 0; j < i; j++) {
        if (textBuffer[j] == '\n') {
          row++;
          col = 0;
        } else {
          col++;
          if (col >= TEXT_COLS) {
            col = 0;
            row++;
          }
        }
      }

      if (textBuffer[i] != '\n' && row < 4) {
        drawCodepoint(
          textBuffer[i],
          col * 6,
          TEXT_ROW_Y[row]
        );
      }
    }
  }
  else {
    // After the first manual clear, input starts on the first row.
    for (uint16_t i = 0; i < caracteresEnPantalla && i < MAX_TEXT_LENGTH; i++) {
      uint8_t row = 0;
      uint8_t col = 0;

      for (uint16_t j = 0; j < i; j++) {
        if (textBuffer[j] == '\n') {
          row++;
          col = 0;
        } else {
          col++;
          if (col >= TEXT_COLS) {
            col = 0;
            row++;
          }
        }
      }

      if (textBuffer[i] != '\n' && row < 4) {
        drawCodepoint(
          textBuffer[i],
          col * 6,
          TEXT_ROW_Y[row]
        );
      }
    }
  }

  if (clearWaiting)
    drawClearIndicator();
  else
    updateStatusLine();

  lcd.sendBuffer();
}

void updateClearIndicator() {
  if (!clearWaiting)
    return;

  unsigned long now = millis();

  if ((now - clearBlinkTimer) >= CLEAR_BLINK_INTERVAL_MS) {
    clearBlinkTimer = now;
    clearIndicatorVisible = !clearIndicatorVisible;
    updateDisplay();
  }
}

void setReadyScreen() {
  readyScreen = true;
  clearWaiting = false;
  clearIndicatorVisible = true;
  clearBlinkTimer = millis();
  caracteresEnPantalla = 0;
  memset(textBuffer, 0, sizeof(textBuffer));
  updateDisplay();
}

// ============================================================
// TEXT BUFFER
// ============================================================
void bleQueueBytes(const uint8_t* data, size_t length);
void bleQueuePrintln(const char* text);

void serialPrintlnMirror(const char* text) {
  Serial.println(text);
  bleQueuePrintln(text);
}

void serialPrintlnMirror() {
  Serial.println();
  bleQueuePrintln(nullptr);
  serialAtLineStart = true;
}

void serialPrintCharacter(uint32_t c) {
  uint8_t bytes[3];
  size_t length = 0;

  if (c <= 0x7F) {
    bytes[0] = (uint8_t)c;
    length = 1;
  } else if (c <= 0x7FF) {
    bytes[0] = (uint8_t)(0xC0 | (c >> 6));
    bytes[1] = (uint8_t)(0x80 | (c & 0x3F));
    length = 2;
  } else {
    bytes[0] = (uint8_t)(0xE0 | (c >> 12));
    bytes[1] = (uint8_t)(0x80 | ((c >> 6) & 0x3F));
    bytes[2] = (uint8_t)(0x80 | (c & 0x3F));
    length = 3;
  }

  // Existing USB-Serial output.
  Serial.write(bytes, length);
  serialAtLineStart = false;

  // Non-blocking copy to the BLE FreeRTOS queue.
  // BLE notify() itself is executed only by bleSerialTask().
  bleQueueBytes(bytes, length);
}

void addCharToBuffer(uint32_t c) {

  uint16_t capacity = readyScreen
                      ? READY_TEXT_CAPACITY
                      : MAX_TEXT_LENGTH;

  // If the display is already full, keep sending decoded characters to
  // Serial, but never store them for later display.
  if (caracteresEnPantalla >= capacity) {
    serialPrintCharacter(c);
    return;
  }

  // Determine the actual display position of the character BEFORE adding it.
  // row/col therefore describe the cell occupied by c.
  uint8_t row = readyScreen ? 1 : 0;
  uint8_t col = 0;

  for (uint16_t i = 0; i < caracteresEnPantalla; i++) {
    if (textBuffer[i] == '\n') {
      row++;
      col = 0;
    }
    else {
      col++;
      if (col >= TEXT_COLS) {
        col = 0;
        row++;
      }
    }
  }

  // Store the character.
  textBuffer[caracteresEnPantalla++] = c;

  // Update the display first.
  updateDisplay();

  // Send exactly the same character to USB/BLE Serial.
  serialPrintCharacter(c);

  // Serial line wrapping is tied to the actual display cell:
  // - explicit newline -> newline immediately;
  // - last column of a display row -> newline immediately AFTER the
  //   character that occupies that cell.
  if (c == '\n' || col == (TEXT_COLS - 1)) {
    serialPrintlnMirror();
    serialDisplayRow++;
  }

  // Determine whether the newly added character filled the last visible
  // display cell.
  bool displayFull = false;

  if (c != '\n') {
    uint8_t displayRow = row;
    uint8_t displayCol = col;

    if (displayCol == (TEXT_COLS - 1)) {
      // Current character occupies the last column. If this is the last
      // available display row, the screen is now full.
      if (displayRow >= (TEXT_ROWS - 1))
        displayFull = true;
    }
  }

  if (caracteresEnPantalla >= capacity)
    displayFull = true;

  if (displayFull && settings.autoClearScreen && !autoClearPending) {
    autoClearPending = true;
    autoClearTimer = millis();
  }
  else if (displayFull && !settings.autoClearScreen) {
    clearWaiting = true;
    clearIndicatorVisible = true;
    clearBlinkTimer = millis();
  }
}

void addSpaceToBuffer() {

  if (clearWaiting || caracteresEnPantalla == 0)
    return;

  uint16_t capacity = readyScreen
                      ? READY_TEXT_CAPACITY
                      : MAX_TEXT_LENGTH;

  if (caracteresEnPantalla >= capacity)
    return;

  // Do not create a second consecutive space.
  // Also do not put an automatic space at the start of a new display row.
  if (textBuffer[caracteresEnPantalla - 1] == ' ')
    return;

  if ((caracteresEnPantalla % TEXT_COLS) == 0)
    return;

  addCharToBuffer(' ');
}

void clearBuffer() {

  caracteresEnPantalla = 0;
  serialDisplayRow = 0;
  memset(textBuffer, 0, sizeof(textBuffer));

  readyScreen = false;
  clearWaiting = false;
  autoClearPending = false;
  autoClearTimer = 0;
  clearIndicatorVisible = true;
  clearBlinkTimer = 0;

  updateDisplay();
}

// ============================================================
// MORSE DECODER
// ============================================================
int decodeServiceCode(uint32_t code, uint8_t length) {
  // Service signals, Q-codes and 92 Code are independent of Language.
  for (size_t i = 0; i < SERVICE_CODE_COUNT; i++) {
    ServiceCode sc;
    memcpy_P(&sc, &serviceCodes[i], sizeof(sc));

    if (sc.length == length && sc.code == code)
      return (int)i;
  }

  return -1;
}

void outputServiceCode(size_t index) {
  if (index >= SERVICE_CODE_COUNT)
    return;

  ServiceCode sc;
  memcpy_P(&sc, &serviceCodes[index], sizeof(sc));

  char codeText[8];
  char description[128];

  strncpy_P(codeText, sc.codeText, sizeof(codeText) - 1);
  codeText[sizeof(codeText) - 1] = '\0';

  strncpy_P(description, sc.description, sizeof(description) - 1);
  description[sizeof(description) - 1] = '\0';

  // A ServiceCode always starts on a new Serial line.
  // Do not add an extra blank line if the stream is already at line start.
  if (!serialAtLineStart)
    serialPrintlnMirror();

  // Complete description is always sent to USB-Serial and BLE-Serial.
  char line[144];
  snprintf(line, sizeof(line), "[%s] <%s>", codeText, description);

  // Start the display from a clean screen for every service/procedural code.
  clearBuffer();

  // The display has four text rows. The message is rendered from the
  // beginning and is truncated after the available 4 x TEXT_COLS cells.
  // The complete message remains available through both serial outputs.
  uint16_t lineLength = strlen(line);
  uint16_t displayLength = lineLength;

  const uint16_t maxDisplayChars = MAX_TEXT_LENGTH;
  if (displayLength > maxDisplayChars)
    displayLength = maxDisplayChars;

  for (uint16_t i = 0; i < displayLength; i++) {
    if (caracteresEnPantalla >= maxDisplayChars)
      break;

    textBuffer[caracteresEnPantalla++] = (uint8_t)line[i];
  }

  // Keep the existing explicit line-break semantics after the
  // service/procedural message.
  if (caracteresEnPantalla < maxDisplayChars)
    textBuffer[caracteresEnPantalla++] = '\n';

  updateDisplay();

  // Serial output is emitted after the corresponding display update.
  serialPrintlnMirror(line);
}

uint32_t decodeMorse(uint32_t code, uint8_t length) {

  // Service signals / Q-codes / 92 Code are handled before this function.
  // Language selects the alphabet for letters.
  // Punctuation is checked BEFORE digits because some EN punctuation
  // codes are identical to numeric Morse codes:
  //   '  .----  == digit 1
  //   "  .-..-  is a five-element punctuation code
  //   !  --..-- == comma in RU
  //
  // Therefore punctuation must have priority over numbers.

  // ----------------------------------------------------------
  // Punctuation and additional supported symbols.
  // RU/EN variants are selected by morseCyrillic.
  // ----------------------------------------------------------

  if (!morseCyrillic) {

    // EN apostrophe:
    // .----.  (six elements)
    // .----   (five elements, abbreviated form)
    if (length == 6 && code == 0b011110) {
      return 0x0027; // '
    }

    // EN quotation mark:
    // .-..-.  (six elements)
    // .-..-   (five elements, abbreviated form)
    if (length == 6 && code == 0b010010) {
      return 0x0022; // "
    }

    // EN exclamation mark: -.-.--
    if (length == 6 && code == 0b101011) {
      return 0x0021; // !
    }

    // EN comma: --..--
    if (length == 6 && code == 0b110011) {
      return 0x002C; // ,
    }

    // EN parentheses use one universal code: -.--.-
    if (length == 6 && code == 0b101101) {
      return MORSE_PARENTHESES_CODE;
    }
  }
  else {

    // RU punctuation — exact mapping supplied by the user.
    // .  .-.-.-
    // ,  .-.-.-   (same code as period)
    // !  --..--
    // ?  ..--..
    // :  ---...
    // ;  -.-.-.
    // -  -....-
    // /  -..-.
    // "  .-..-.
    // '  .----.
    // (  -.--.
    // )  -.--.-
    // =  -...-
    // @  .--.-.
    // +  .-.-.

    if (length == 6 && code == 0b010101) {
      return 0x002E; // .
    }

    // RU comma intentionally has the same Morse code as period.
    // The decoder cannot distinguish these two symbols by Morse code alone.
    // The shared sequence is decoded as a period.

    if (length == 6 && code == 0b110011) {
      return 0x0021; // !
    }

    if (length == 6 && code == 0b001100) {
      return 0x003F; // ?
    }

    if (length == 6 && code == 0b111000) {
      return 0x003A; // :
    }

    if (length == 6 && code == 0b101010) {
      return 0x003B; // ;
    }

    if (length == 6 && code == 0b100001) {
      return 0x002D; // -
    }

    if (length == 5 && code == 0b10010) {
      return 0x002F; // /
    }

    if (length == 6 && code == 0b010010) {
      return 0x0022; // "
    }

    if (length == 6 && code == 0b011110) {
      return 0x0027; // '
    }

    if (length == 5 && code == 0b10110) {
      return 0x0028; // (
    }

    if (length == 6 && code == 0b101101) {
      return 0x0029; // )
    }

    if (length == 5 && code == 0b10001) {
      return 0x003D; // =
    }

    if (length == 6 && code == 0b011010) {
      return 0x0040; // @
    }

    if (length == 5 && code == 0b01010) {
      return 0x002B; // +
    }
  }

  for (size_t i = 0;
       i < sizeof(morseTable) / sizeof(morseTable[0]);
       i++) {

    MorseChar mc;
    memcpy_P(&mc, &morseTable[i], sizeof(mc));

    if (morseCyrillic) {
      // All RU punctuation is decoded explicitly above.
      // Do not let generic morseTable entries override it.
      if (mc.character == 0x0021 || mc.character == 0x0022 ||
          mc.character == 0x0027 || mc.character == 0x0028 ||
          mc.character == 0x0029 || mc.character == 0x002C ||
          mc.character == 0x002D || mc.character == 0x002E ||
          mc.character == 0x002F || mc.character == 0x003A ||
          mc.character == 0x003B || mc.character == 0x003D ||
          mc.character == 0x003F || mc.character == 0x0040) {
        continue;
      }
    }

    if (morseCyrillic) {
      // RU punctuation is decoded explicitly above.
      if (mc.character == 0x0021 || mc.character == 0x0022 ||
          mc.character == 0x0027 || mc.character == 0x0028 ||
          mc.character == 0x0029 || mc.character == 0x002B ||
          mc.character == 0x002D || mc.character == 0x002E ||
          mc.character == 0x002F || mc.character == 0x003A ||
          mc.character == 0x003B || mc.character == 0x003D ||
          mc.character == 0x003F || mc.character == 0x0040) {
        continue;
      }
    }

    bool isSupportedSymbol =
      mc.character == 0x0021 || // !
      mc.character == 0x0022 || // "
      mc.character == 0x0026 || // &
      mc.character == 0x0027 || // '
      mc.character == 0x0028 || // (
      mc.character == 0x0029 || // )
      mc.character == 0x002B || // +
      mc.character == 0x002C || // ,
      mc.character == 0x002D || // -
      mc.character == 0x002E || // .
      mc.character == 0x002F || // /
      mc.character == 0x003A || // :
      mc.character == 0x003B || // ;
      mc.character == 0x003D || // =
      mc.character == 0x003F || // ?
      mc.character == 0x0040 || // @
      mc.character == 0x005F;   // _

    if (!isSupportedSymbol)
      continue;

    if (morseCyrillic &&
        (mc.character == 0x002C || mc.character == 0x002E)) {
      continue;
    }

    if (mc.character == 0x0021) {
      // RU comma uses --..--; do not decode this code as '!'.
      if (morseCyrillic)
        continue;
      if (mc.code != 0b101011)
        continue;
    }

    if (mc.character == 0x0027) {
      if (mc.length != 6 || mc.code != 0b011110)
        continue;
    }

    if (mc.character == 0x0022) {
      if (morseCyrillic && mc.code != 0b010010)
        continue;
      if (!morseCyrillic && mc.code != 0b01001)
        continue;
    }

    if (mc.character == 0x0028 || mc.character == 0x0029) {
      if (morseCyrillic) {
        if (mc.character == 0x0028 && mc.code != 0b10110)
          continue;
        if (mc.character == 0x0029 && mc.code != 0b101101)
          continue;
      } else {
        if (mc.code != 0b101101)
          continue;
      }
    }

    if (mc.length == length &&
        mc.code == code) {
      return mc.character;
    }
  }

  // ----------------------------------------------------------
  // Letters: selected language only.
  // ----------------------------------------------------------
  if (morseCyrillic) {

    for (size_t i = 0;
         i < sizeof(morseTable) / sizeof(morseTable[0]);
         i++) {

      MorseChar mc;
      memcpy_P(&mc, &morseTable[i], sizeof(mc));

      if (mc.character >= 0x0410 &&
          mc.character <= 0x042F &&
          mc.length == length &&
          mc.code == code) {

        return mc.character;
      }
    }

  } else {

    for (size_t i = 0;
         i < sizeof(morseTable) / sizeof(morseTable[0]);
         i++) {

      MorseChar mc;
      memcpy_P(&mc, &morseTable[i], sizeof(mc));

      if (mc.character >= 0x0041 &&
          mc.character <= 0x005A &&
          mc.length == length &&
          mc.code == code) {

        return mc.character;
      }
    }
  }

  // ----------------------------------------------------------
  // Numbers 0-9: valid in BOTH EN and RU.
  // ----------------------------------------------------------
  for (size_t i = 0;
       i < sizeof(morseTable) / sizeof(morseTable[0]);
       i++) {

    MorseChar mc;
    memcpy_P(&mc, &morseTable[i], sizeof(mc));

    if (mc.character >= 0x0030 &&
        mc.character <= 0x0039 &&
        mc.length == length &&
        mc.code == code) {

      return mc.character;
    }
  }

  // Unknown / invalid Morse sequence.
  return '*';
}

// ============================================================
// DECODED CHARACTER OUTPUT
// ============================================================
void outputDecodedCharacter(uint32_t c) {

  if (c == MORSE_PARENTHESES_CODE) {
    // EN uses one Morse code for the paired parentheses.
    // Represent it as the two visible characters "()" on output.
    addCharToBuffer('(');
    addCharToBuffer(')');
    return;
  }

  if (c != 0)
    addCharToBuffer(c);
}

// ============================================================
// LONG PAUSE → SPACE
// ============================================================
void verificarPausaLarga() {

  if (!txActive && !isKeyDown) {

    if ((millis() - lastKeyActivityTime) >= PAUSA_ESPACIO_MS &&
        !espacioAgregadoPorPausa) {

      addSpaceToBuffer();
      espacioAgregadoPorPausa = true;
    }
  }
  else {
    espacioAgregadoPorPausa = false;
  }
}

// ============================================================
// AUTO CLEAR
// ============================================================
void verificarAutoClear() {

  if (!settings.autoClearScreen || !autoClearPending)
    return;

  if ((millis() - autoClearTimer) >= AUTO_CLEAR_DELAY_MS) {
    clearBuffer();
  }
}

// ============================================================
// CW DECODER
// ============================================================
void processDecoder() {

  // Decoding continues even when the display text area is full.
  // Ordinary characters are then sent only to Serial, while a recognized
  // service/Q/92 code is allowed to clear the display and is shown
  // immediately by outputServiceCode().
  bool currentKeyState = digitalRead(PIN_KEY_OUT);
  unsigned long now = millis();

  if (currentKeyState && !isKeyDown) {

    isKeyDown = true;
    keyDownStart = now;
    lastKeyActivityTime = now;

    if (currentLength > 0 &&
        (now - lastChangeTime) > getInterLetterGap()) {

      int serviceIndex = decodeServiceCode(currentCode, currentLength);

      if (serviceIndex >= 0) {
        outputServiceCode((size_t)serviceIndex);

        // A ServiceCode terminates the preceding text line. Prevent the
        // long-pause handler from inserting an extra leading space before
        // the first character entered after the ServiceCode.
        lastKeyActivityTime = now;
        espacioAgregadoPorPausa = true;
      }
      else {
        uint32_t c = decodeMorse(currentCode, currentLength);

        if (c != 0) {
          outputDecodedCharacter(c);
        }
      }

      currentCode = 0;
      currentLength = 0;
    }

  }
  else if (!currentKeyState && isKeyDown) {

    isKeyDown = false;

    unsigned long duration = now - keyDownStart;
    unsigned long ditDur = getDitDuration();

    lastKeyActivityTime = now;

    if (duration <= ditDur * 1.5) {
      currentCode = (currentCode << 1) | 0;
    }
    else if (duration <= ditDur * 3.5) {
      currentCode = (currentCode << 1) | 1;
    }
    else {
      currentCode = 0;
      currentLength = 0;
    }

    if (duration <= ditDur * 3.5) {
      currentLength++;

      if (currentLength > 12)
        currentLength = 12;
    }

    lastChangeTime = now;

  }
  else if (!isKeyDown && currentLength > 0) {

    if ((now - lastChangeTime) >= getInterLetterGap()) {

      int serviceIndex = decodeServiceCode(currentCode, currentLength);

      if (serviceIndex >= 0) {
        outputServiceCode((size_t)serviceIndex);

        // A ServiceCode terminates the preceding text line. Prevent the
        // long-pause handler from inserting an extra leading space before
        // the first character entered after the ServiceCode.
        lastKeyActivityTime = now;
        espacioAgregadoPorPausa = true;
      }
      else {
        uint32_t c = decodeMorse(currentCode, currentLength);

        if (c != 0) {
          outputDecodedCharacter(c);
        }
      }

      currentCode = 0;
      currentLength = 0;
      lastChangeTime = now;
    }

    if ((now - lastChangeTime) >= getInterWordGap()) {

      addSpaceToBuffer();

      lastChangeTime = now;
    }
  }

  verificarPausaLarga();
}

// ============================================================
// PADDLES
// ============================================================
uint8_t readPaddles() {

  uint8_t result = 0;

  if (digitalRead(PIN_DIT) == LOW)
    result |= DIT_MASK;

  if (digitalRead(PIN_DAH) == LOW)
    result |= DAH_MASK;

  if (result) {
    lastKeyActivityTime = millis();
    espacioAgregadoPorPausa = false;
  }

  if (settings.swapPaddles) {

    uint8_t swapped = 0;

    // Physical DIT becomes logical DAH.
    if (result & DIT_MASK)
      swapped |= DAH_MASK;

    // Physical DAH becomes logical DIT.
    if (result & DAH_MASK)
      swapped |= DIT_MASK;

    return swapped;
  }

  return result;
}

// ============================================================
// TRANSMITTER
// ============================================================
void startTransmit() {

  if (!txActive) {

    txActive = true;

    digitalWrite(PIN_KEY_OUT, HIGH);

    if (settings.sidetoneEnabled)
      tone(PIN_SIDETONE, settings.sidetoneFreq);
  }
}

void stopTransmit() {

  if (txActive) {

    txActive = false;

    digitalWrite(PIN_KEY_OUT, LOW);

    if (settings.sidetoneEnabled)
      noTone(PIN_SIDETONE);
  }
}

// ============================================================
// IAMBIC KEYER — Mode A / Mode B state machine
// ============================================================
void processKeyer() {

  const unsigned long now = millis();
  const uint8_t paddles = readPaddles();

  switch (keyerState) {

    // ----------------------------------------------------------
    // IDLE
    // ----------------------------------------------------------
    case IDLE: {

      paddleState = 0;
      bothPaddlesSeen = false;

      if (paddles & DIT_MASK) {

        // Preserve the existing v12 priority when both paddles
        // are detected simultaneously: DIT starts first.
        currentElementIsDit = true;
        keyerState = KEY_DOWN;

        startTransmit();
        keyerTimer = now + getDitDuration();

        if ((paddles & DIT_MASK) && (paddles & DAH_MASK))
          bothPaddlesSeen = true;

      }
      else if (paddles & DAH_MASK) {

        currentElementIsDit = false;
        keyerState = KEY_DOWN;

        startTransmit();
        keyerTimer = now + getDahDuration();
      }

      break;
    }

    // ----------------------------------------------------------
    // KEY DOWN
    // ----------------------------------------------------------
    case KEY_DOWN: {

      // A squeeze during the current element is remembered.
      if ((paddles & DIT_MASK) && (paddles & DAH_MASK))
        bothPaddlesSeen = true;

      if (now >= keyerTimer) {

        stopTransmit();

        // One dit time is the inter-element space.
        keyerTimer = now + getDitDuration();
        keyerState = INTER_ELEMENT;
      }

      break;
    }

    // ----------------------------------------------------------
    // INTER ELEMENT
    // ----------------------------------------------------------
    case INTER_ELEMENT: {

      if (now < keyerTimer)
        break;

      const bool ditPressed = (paddles & DIT_MASK) != 0;
      const bool dahPressed = (paddles & DAH_MASK) != 0;
      const bool bothPressed = ditPressed && dahPressed;

      bool sendNext = false;
      bool nextIsDit = currentElementIsDit;

      if (bothPressed) {

        // Iambic squeeze: alternate DIT / DAH.
        sendNext = true;
        nextIsDit = !currentElementIsDit;

      }
      else if (currentElementIsDit && dahPressed) {

        // Opposite paddle is held.
        sendNext = true;
        nextIsDit = false;

      }
      else if (!currentElementIsDit && ditPressed) {

        // Opposite paddle is held.
        sendNext = true;
        nextIsDit = true;

      }
      else if (!settings.iambicModeA && bothPaddlesSeen) {

        // Iambic Mode B:
        // if both paddles had been squeezed during the current
        // element and both are now released, send one additional
        // element opposite to the one just transmitted.
        sendNext = true;
        nextIsDit = !currentElementIsDit;
      }

      if (sendNext) {

        currentElementIsDit = nextIsDit;
        bothPaddlesSeen = false;

        startTransmit();

        keyerTimer = now +
          (currentElementIsDit
            ? getDitDuration()
            : getDahDuration());

        keyerState = KEY_DOWN;

      }
      else {

        // No paddle request remains.
        paddleState = 0;
        bothPaddlesSeen = false;
        keyerState = IDLE;
      }

      break;
    }
  }
}

// ============================================================
// DISPLAY HARDWARE CONTROL
// ============================================================
void applyBacklight() {
  // ESP32-C3 LEDC PWM, 8-bit duty cycle.
  uint32_t duty = ((uint32_t)settings.backlightPercent * 255UL) / 100UL;
  ledcWrite(BACKLIGHT_CHANNEL, duty);
}

void applyContrast() {
  // 50..100% user range maps linearly to U8g2 100..180.
  uint8_t contrast =
    100 + ((uint16_t)(settings.contrastPercent - 50) * 80 / 50);

  lcd.setContrast(contrast);
}

// ============================================================
// MENU
// ============================================================
void saveSettings() {
  EEPROM.begin(EEPROM_LANGUAGE_ADDR + 1);
  EEPROM.put(EEPROM_SETTINGS_ADDR, settings);
  EEPROM.put(EEPROM_LANGUAGE_ADDR, (uint8_t)(morseCyrillic ? 1 : 0));
  EEPROM.commit();
  EEPROM.end();
}

void menuBeep(bool selectBeep) {
  if (!settings.sidetoneEnabled)
    return;

  tone(PIN_SIDETONE, selectBeep ? 1200 : 800);
  delay(selectBeep ? 45 : 30);
  noTone(PIN_SIDETONE);
}

void drawLanguageMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);
  lcd.drawStr(0, 8, "Language");
  lcd.drawStr(0, 20, morseCyrillic ? "Current: RU" : "Current: ENG");
  lcd.drawStr(0, 32, "DIT=RU  DAH=ENG");
  lcd.drawStr(0, 44, "COMMAND=OK");
  lcd.sendBuffer();
}

void drawClearMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "CLR scrn");

  if (settings.autoClearScreen)
    lcd.drawStr(65, 20, "Auto");    // right-aligned
  else
    lcd.drawStr(48, 20, "Manual");  // right-aligned

  lcd.drawStr(0, 32, "DIT=MAN DAH=AUTO");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void drawBacklightMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "Backlight");

  char value[8];
  snprintf(value, sizeof(value), "%d%%", settings.backlightPercent);
  lcd.drawStr(60, 20, value);

  lcd.drawStr(0, 32, "DIT=DOWN DAH=UP");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void drawContrastMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "Contrast");

  char value[8];
  snprintf(value, sizeof(value), "%d%%", settings.contrastPercent);
  lcd.drawStr(60, 20, value);

  lcd.drawStr(0, 32, "DIT=DOWN DAH=UP");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void drawDisplayRotMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "Display rot.");
  lcd.drawStr(80, 20, settings.displayRotated ? "↓" : "↑");

  lcd.drawStr(0, 32, "DIT=DOWN DAH=UP");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void drawSwapDitDahMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);
  lcd.drawStr(0, 8, "Swap DIT & DAH");
  lcd.drawStr(0, 20, settings.swapPaddles ? "Current: ON" : "Current: OFF");
  lcd.drawStr(0, 32, "DIT=DOWN DAH=UP");
  lcd.drawStr(0, 44, "COMMAND=OK");
  lcd.sendBuffer();
}

void printAboutSerial() {
  serialPrintlnMirror("ОПИС ПРОЕКТУ");
  serialPrintlnMirror("Даний проект - це ямбiчний телеграфний ключ");
  serialPrintlnMirror("iз вбудованим декодером азбуки Морзе на бaзi");
  serialPrintlnMirror(" мiкpoкoнтpoлepa ESP32-C3 Super Mini.");
  serialPrintlnMirror("Пpиcтpiй дозволяє:");
  serialPrintlnMirror("* Передавати азбуку Морзе за допомогою ямбiчнoгo ключа");
  serialPrintlnMirror("  (два вaжeлi: точка та тире)");
  serialPrintlnMirror("* Автоматично декодувати код Морзе");
  serialPrintlnMirror("* Преремикати ввiд cимвoлiв RU/EN");
  serialPrintlnMirror("* Biдoбpaжaти декодований текст на eкpaнi (4 рядки),");
  serialPrintlnMirror("  передавати текст через USB-serial та BLE-serial");
  serialPrintlnMirror("* Регулювати швидкicть пepeдaчi (cлiв за хвилину)");
  serialPrintlnMirror("* Автоматично встановлювати iнтepвaли пicля 1-секундної паузи");
  serialPrintlnMirror("* Автоматично aбo вручну з пiдтвepджeнням очищати екран пicля");
  serialPrintlnMirror("  завершення пepeдaчi 4 pядкiв");
  serialPrintlnMirror("* Очищати екран вручну коротким натисканням кнопки");
  serialPrintlnMirror("* Комутувати за допомогою оптрона CW вxiд трансивера");
  serialPrintlnMirror("");
  serialPrintlnMirror("Проект створено на бaзi");
  serialPrintlnMirror("https://github.com/LU6APR/ESP32_C3_MINI_MORSE_TRAINER");
  serialPrintlnMirror("");
  serialPrintlnMirror("Moдифiкaцiя коду:");
  serialPrintlnMirror("Павло Лузан");
  serialPrintlnMirror("pahentij@gmail.com");
  serialPrintlnMirror("");
  serialPrintlnMirror("2026");
  serialPrintlnMirror("");
}

void drawAboutMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_4x6_t_cyrillic);

  lcd.drawStr(0, 6,   "~~~~~~~~~~~~~~~~~~~~~");
  lcd.drawUTF8(3, 9,  "Телеграфний тренажер");
  lcd.drawUTF8(0, 17, "Автори:");
  lcd.drawUTF8(1, 23, "<-> github.com/LU6APR");
  lcd.drawUTF8(1, 29, "<->       Павло Лузан");
//  lcd.drawUTF8(0, 36, "");
  lcd.drawUTF8(0, 39, "Бiльшe - y Serial LOG");
  lcd.drawUTF8(1, 45, "Pahentij (c)     2026");
  lcd.drawStr(0, 51,  "~~~~~~~~~~~~~~~~~~~~~");

  lcd.sendBuffer();
}

void applyDisplayRotation() {
  lcd.setDisplayRotation(
    settings.displayRotated ? U8G2_R2 : U8G2_R0
  );
}



void printMorseCodesPageSerial() {
  switch (morseCodesPage) {

    case 0:
      serialPrintlnMirror("MORSE CODES - EN");
      serialPrintlnMirror("A .-    I ..    Q --.-");
      serialPrintlnMirror("B -...  J .---  R .-.");
      serialPrintlnMirror("C -.-.  K -.-   S ...");
      serialPrintlnMirror("D -..   L .-..  T -");
      serialPrintlnMirror("E .     M --    U ..-");
      serialPrintlnMirror("F ..-.  N -.    V ...-");
      serialPrintlnMirror("G --.   O ---   W .--");
      serialPrintlnMirror("H ....  P .--.  X -..-");
      serialPrintlnMirror("");
      break;

    case 1:
      serialPrintlnMirror("MORSE CODES - EN");
      serialPrintlnMirror("Y -.--  Z --..");
      serialPrintlnMirror("");
      break;

    case 2:
      serialPrintlnMirror("MORSE CODES - RU");
      serialPrintlnMirror("А .-    И ..   Р .-.");
      serialPrintlnMirror("Б -...  Й .--- С ...");
      serialPrintlnMirror("В .--   К -.-  Т -");
      serialPrintlnMirror("Г --.   Л .-.. У ..-");
      serialPrintlnMirror("Д -..   М --   Ф ..-.");
      serialPrintlnMirror("Е .     Н -.   Х ....");
      serialPrintlnMirror("Ж ...-  О ---  Ц -.-.");
      serialPrintlnMirror("З --..  П .--. Ч ---.");
      serialPrintlnMirror("");
      break;

    case 3:
      serialPrintlnMirror("MORSE CODES - RU");
      serialPrintlnMirror("Ш ----  Э ..-..");
      serialPrintlnMirror("Щ --.-  Ю ..--");
      serialPrintlnMirror("ЪЬ -..- Я .-.-");
      serialPrintlnMirror("Ы -.--  ");
      serialPrintlnMirror("");
      break;

    case 4:
      serialPrintlnMirror("MORSE CODES - DIGITS");
      serialPrintlnMirror("0 -----     5 .....");
      serialPrintlnMirror("1 .----     6 -....");
      serialPrintlnMirror("2 ..---     7 --...");
      serialPrintlnMirror("3 ...--     8 ---..");
      serialPrintlnMirror("4 ....-     9 ----.");
      serialPrintlnMirror("");
      break;

    case 5:
      serialPrintlnMirror("MORSE CODES - PUNCTUATION RU");
      serialPrintlnMirror("'.' ......   ',' .-.-.-");
      serialPrintlnMirror("':' ---...   ';' -.-.-.");
      serialPrintlnMirror("'?' ..--..   '!' --..--");
      serialPrintlnMirror("'\"' .-..-.   '\'' .----.");
      serialPrintlnMirror("'(' -.--.    ')' -.--.-");
      serialPrintlnMirror("'/' -..-.    '-' -....-");
      serialPrintlnMirror("'=' -...-    '@' .--.-.");
      serialPrintlnMirror("");
      break;

    case 6:
      serialPrintlnMirror("MORSE CODES - PUNCTUATION EN");
      serialPrintlnMirror("'.' .-.-.-   ',' --..--");
      serialPrintlnMirror("':' ---...   ';' -.-.-.");
      serialPrintlnMirror("'?' ..--..   '!' -.-.--");
      serialPrintlnMirror("'\"' .-..-.   '\'' .----.");
      serialPrintlnMirror("'(' -.--.-   ')' -.--.-");
      serialPrintlnMirror("'/' -..-.    '-' -....-");
      serialPrintlnMirror("'=' -...-    '@' .--.-.");
      serialPrintlnMirror("");
      break;

    case 7: {
      serialPrintlnMirror("SERVICE CODES - 3 LETTERS");
      char line[96];
      uint8_t column = 0;

      for (size_t i = 0; i < SERVICE_CODE_COUNT; i++) {
        ServiceCode sc;
        memcpy_P(&sc, &serviceCodes[i], sizeof(sc));

        char codeText[8];
        strncpy_P(codeText, sc.codeText, sizeof(codeText) - 1);
        codeText[sizeof(codeText) - 1] = '\0';

        if (strlen(codeText) != 3)
          continue;


        strncpy_P(codeText, sc.codeText, sizeof(codeText) - 1);
        codeText[sizeof(codeText) - 1] = '\0';

        char description[64];
        strncpy_P(description, sc.description, sizeof(description) - 1);
        description[sizeof(description) - 1] = '\0';

        char fullLine[96];
        snprintf(fullLine, sizeof(fullLine), "[%s], %s", codeText, description);
        serialPrintlnMirror(fullLine);

        column++;
      }

      serialPrintlnMirror("");
      break;
    }

    case 8: {
      serialPrintlnMirror("SERVICE CODES - 2 LETTERS / 92");

      for (size_t i = 0; i < SERVICE_CODE_COUNT; i++) {
        ServiceCode sc;
        memcpy_P(&sc, &serviceCodes[i], sizeof(sc));

        char codeText[8];
        strncpy_P(codeText, sc.codeText, sizeof(codeText) - 1);
        codeText[sizeof(codeText) - 1] = '\0';

        if (strlen(codeText) != 2)
          continue;


        codeText[sizeof(codeText) - 1] = '\0';

        char description[64];
        strncpy_P(description, sc.description, sizeof(description) - 1);
        description[sizeof(description) - 1] = '\0';

        char fullLine[96];
        snprintf(fullLine, sizeof(fullLine), "[%s], %s", codeText, description);
        serialPrintlnMirror(fullLine);
      }

      serialPrintlnMirror("");
      break;
    }
  }
}

void drawMorseCodesMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_4x6_t_cyrillic);

  switch (morseCodesPage) {

    case 0: {
      const char* lines[] = {
        "A .-    I ..   Q --.-",
        "B -...  J .--- R .-.",
        "C -.-.  K -.-  S ...",
        "D -..   L .-.. T -",
        "E .     M --   U ..-",
        "F ..-.  N -.   V ...-",
        "G --.   O ---  W .--",
        "H ....  P .--. X -..-"
      };
      for (uint8_t i = 0; i < 8; i++)
        lcd.drawStr(0, 6 + i * 6, lines[i]);
      break;
    }

    case 1:
      lcd.drawStr(0, 6, "Y -.--  Z --..");
      break;

    case 2: {
      const char* lines[] = {
        "А .-    И ..   Р .-.",
        "Б -...  Й .--- С ...",
        "В .--   К -.-  Т -",
        "Г --.   Л .-.. У ..-",
        "Д -..   М --   Ф ..-.",
        "Е .     Н -.   Х ....",
        "Ж ...-  О ---  Ц -.-.",
        "З --..  П .--. Ч ---."
      };
      for (uint8_t i = 0; i < 8; i++)
        lcd.drawUTF8(0, 6 + i * 6, lines[i]);
      break;
    }

    case 3: {
      const char* lines[] = {
        "Ш ----   Э ..-..",
        "Щ --.-   Ю ..--",
        "ЪЬ -..-  Я .-.-",
        "Ы -.--   "
      };
      for (uint8_t i = 0; i < 4; i++)
        lcd.drawUTF8(0, 6 + i * 6, lines[i]);
      break;
    }

    case 4: {
      const char* lines[] = {
        "0 -----     5 .....",
        "1 .----     6 -....",
        "2 ..---     7 --...",
        "3 ...--     8 ---..",
        "4 ....-     9 ----."
      };
      for (uint8_t i = 0; i < 5; i++)
        lcd.drawStr(0, 6 + i * 6, lines[i]);
      break;
    }

    case 5: {
      const char* lines[] = {
        "PUNCTUATION RU",
        ". ......   , .-.-.-",
        ": ---...   ; -.-.-.",
        "? ..--..   ! --..--",
        "\" .-..-.   ' .----.",
        "( -.--.    ) -.--.-",
        "/ -..-.    - -....-",
        "= -...-    @ .--.-."
      };
      for (uint8_t i = 0; i < 8; i++)
        lcd.drawStr(0, 6 + i * 6, lines[i]);
      break;
    }

    case 6: {
      const char* lines[] = {
        "PUNCTUATION EN",
        ". .-.-.-   , --..--",
        ": ---...   ; -.-.-.",
        "? ..--..   ! -.-.--",
        "\" .-..-.   ' .----.",
        "( -.--.-   ) -.--.-",
        "/ -..-.    - -....-",
        "= -...-    @ .--.-."
      };
      for (uint8_t i = 0; i < 8; i++)
        lcd.drawStr(0, 6 + i * 6, lines[i]);
      break;
    }

    case 7:
    case 8: {
      uint8_t wantedLength = (morseCodesPage == 7) ? 3 : 2;
      uint8_t column = 0;
      uint8_t row = 0;

      for (size_t i = 0; i < SERVICE_CODE_COUNT; i++) {
        ServiceCode sc;
        memcpy_P(&sc, &serviceCodes[i], sizeof(sc));

        char codeText[8];
        strncpy_P(codeText, sc.codeText, sizeof(codeText) - 1);
        codeText[sizeof(codeText) - 1] = '\0';

        if (strlen(codeText) != wantedLength)
          continue;


        strncpy_P(codeText, sc.codeText, sizeof(codeText) - 1);
        codeText[sizeof(codeText) - 1] = '\0';

        char item[8];
        snprintf(item, sizeof(item), "[%s]", codeText);

        // Service codes are displayed in four columns and seven rows.
        lcd.drawStr(column * 21, 6 + row * 6, item);

        column++;
        if (column >= SERVICE_CODES_COLUMNS) {
          column = 0;
          row++;
        }

        if (row >= SERVICE_CODES_ROWS)
          break;
      }
      break;
    }
  }

  lcd.sendBuffer();
}

void morseCodesNextPage() {
  if (morseCodesPage < MORSE_CODES_LAST_PAGE)
    morseCodesPage++;
  else
    morseCodesPage = 0;

  menuBeep();
  drawMorseCodesMenu();
  printMorseCodesPageSerial();
}

void morseCodesPrevPage() {
  if (morseCodesPage > 0)
    morseCodesPage--;
  else
    morseCodesPage = MORSE_CODES_LAST_PAGE;

  menuBeep();
  drawMorseCodesMenu();
  printMorseCodesPageSerial();
}

void drawMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  // MENU centered on the first line.
  lcd.drawStr(30, 7, "MENU");

  const char* items[MENU_ITEMS] = {
    "Back",
    "Speed, WPM",
    "Iambic mode",
    "Beep",
    "Language",
    "CLR scrn",
    "Backlight",
    "Contrast",
    "Display rot.",
    "Swap DIT&DAH",
    "Morse codes",
    "About"
  };

  // Five rows are visible below the MENU title.
  // Scroll automatically to keep the selected item visible.
  if (menuItem < menuScroll)
    menuScroll = menuItem;

  if (menuItem >= menuScroll + MENU_VISIBLE_ITEMS)
    menuScroll = menuItem - MENU_VISIBLE_ITEMS + 1;

  for (uint8_t row = 0; row < MENU_VISIBLE_ITEMS; row++) {
    uint8_t i = menuScroll + row;
    if (i >= MENU_ITEMS)
      break;

    uint8_t y = 15 + row * 8;

    lcd.setCursor(0, y);
    lcd.print(i == menuItem ? ">" : " ");
    lcd.print(items[i]);

    if (i == MENU_ITEM_SPEED) {
      char speedText[5];
      snprintf(speedText, sizeof(speedText), "%d", settings.speedWPM);
      lcd.drawStr(74, y, speedText);
    }

    if (i == MENU_ITEM_IAMBIC) {
      lcd.drawStr(80, y, settings.iambicModeA ? "A" : "B");
    }

    if (i == MENU_ITEM_BEEP) {
      if (settings.sidetoneEnabled)
      lcd.drawStr(75, y, "ON");
    else
      lcd.drawStr(70, y, "OFF");
    }

    if (i == MENU_ITEM_LANGUAGE) {
      if (morseCyrillic)
        lcd.drawStr(74, y, "RU");
      else
        lcd.drawStr(70, y, "ENG");
    }

    if (i == MENU_ITEM_CLEAR) {
      if (settings.autoClearScreen)
        lcd.drawStr(65, y, "Auto");
      else
        lcd.drawStr(54, y, "Manual");
    }

    if (i == MENU_ITEM_BACKLIGHT) {
      char value[8];
      snprintf(value, sizeof(value), "%d%%", settings.backlightPercent);
      lcd.drawStr(66, y, value);
    }

    if (i == MENU_ITEM_CONTRAST) {
      char value[8];
      snprintf(value, sizeof(value), "%d%%", settings.contrastPercent);
      lcd.drawStr(66, y, value);
    }

    if (i == MENU_ITEM_DISPLAY_ROT) {
      lcd.drawStr(80, y, settings.displayRotated ? "↓" : "↑");
    }

    if (i == MENU_ITEM_SWAP_DIT_DAH) {
      if (settings.swapPaddles)
      lcd.drawStr(75, y, "ON");
    else
      lcd.drawStr(70, y, "OFF");
    }
  }

  lcd.sendBuffer();
}

void drawSpeedMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "Speed, WPM");

  char current[24];
  snprintf(current, sizeof(current), "Current: %d", settings.speedWPM);
  lcd.drawStr(0, 22, current);

  lcd.drawStr(0, 32, "DIT=DOWN DAH=UP");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void drawIambicMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "Iambic mode");
  lcd.drawStr(0, 20, settings.iambicModeA ? "Current: Mode A" : "Current: Mode B");
  lcd.drawStr(0, 32, "DIT=DOWN DAH=UP");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void drawBeepMenu() {
  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(0, 8, "Beep");
  lcd.drawStr(0, 20, settings.sidetoneEnabled ? "Current: ON" : "Current: OFF");
  lcd.drawStr(0, 32, "DIT=OFF DAH=ON");
  lcd.drawStr(0, 44, "COMMAND=OK");

  lcd.sendBuffer();
}

void enterMenu() {
  menuState = MENU_MAIN;
  menuItem = MENU_ITEM_BACK;
  menuScroll = 0;
  menuCommandArmed = false;

  txActive = false;
  digitalWrite(PIN_KEY_OUT, LOW);

  if (settings.sidetoneEnabled)
    noTone(PIN_SIDETONE);

  keyerState = IDLE;
  paddleState = 0;

  // Same confirmation sound as DIT/DAH menu navigation.
  menuBeep();

  currentCode = 0;
  currentLength = 0;
  isKeyDown = false;

  drawMenu();

  // Mirror all settings values to USB-Serial and BLE.
  printSettingsSerial();
}

void exitMenu() {
  saveSettings();

  menuState = MENU_OFF;

  clearBuffer();

  lastKeyActivityTime = millis();
  espacioAgregadoPorPausa = false;

  updateDisplay();
}

void menuUp() {
  if (menuState == MENU_MAIN) {
    if (menuItem == 0) menuItem = MENU_ITEMS - 1;
    else menuItem--;
    menuBeep();
    drawMenu();
  }
  else if (menuState == MENU_SPEED) {
    settings.speedWPM += 5;
    if (settings.speedWPM > 35) settings.speedWPM = 10;
    printSettingsAfterChange();
    menuBeep();
    drawSpeedMenu();
  }
  else if (menuState == MENU_IAMBIC) {
    settings.iambicModeA = true;
    printSettingsAfterChange();
    menuBeep();
    drawIambicMenu();
  }
  else if (menuState == MENU_BEEP) {
    settings.sidetoneEnabled = true;
    printSettingsAfterChange();
    menuBeep();
    drawBeepMenu();
  }
  else if (menuState == MENU_LANGUAGE) {
    morseCyrillic = true;
    printSettingsAfterChange();
    menuBeep();
    drawLanguageMenu();
  }
  else if (menuState == MENU_CLEAR) {
    settings.autoClearScreen = true;
    printSettingsAfterChange();
    menuBeep();
    drawClearMenu();
  }
  else if (menuState == MENU_BACKLIGHT) {
    settings.backlightPercent += 5;
    if (settings.backlightPercent > 100)
      settings.backlightPercent = 0;
    applyBacklight();
    printSettingsAfterChange();
    menuBeep();
    drawBacklightMenu();
  }
  else if (menuState == MENU_CONTRAST) {
    settings.contrastPercent += 5;
    if (settings.contrastPercent > 100)
      settings.contrastPercent = 50;
    applyContrast();
    printSettingsAfterChange();
    menuBeep();
    drawContrastMenu();
  }
  else if (menuState == MENU_DISPLAY_ROT) {
    settings.displayRotated = !settings.displayRotated;
    applyDisplayRotation();
    printSettingsAfterChange();
    menuBeep();
    drawDisplayRotMenu();
  }
  else if (menuState == MENU_SWAP_DIT_DAH) {
    settings.swapPaddles = true;
    printSettingsAfterChange();
    menuBeep();
    drawSwapDitDahMenu();
  }
  else if (menuState == MENU_MORSE_CODES) {
    morseCodesPrevPage();
  }
}

void menuDown() {
  if (menuState == MENU_MAIN) {
    menuItem = (menuItem + 1) % MENU_ITEMS;
    menuBeep();
    drawMenu();
  }
  else if (menuState == MENU_SPEED) {
    if (settings.speedWPM <= 10) settings.speedWPM = 35;
    else settings.speedWPM -= 5;
    printSettingsAfterChange();
    menuBeep();
    drawSpeedMenu();
  }
  else if (menuState == MENU_IAMBIC) {
    settings.iambicModeA = false;
    printSettingsAfterChange();
    menuBeep();
    drawIambicMenu();
  }
  else if (menuState == MENU_BEEP) {
    settings.sidetoneEnabled = false;
    printSettingsAfterChange();
    drawBeepMenu();
  }
  else if (menuState == MENU_LANGUAGE) {
    morseCyrillic = false;
    printSettingsAfterChange();
    menuBeep();
    drawLanguageMenu();
  }
  else if (menuState == MENU_CLEAR) {
    settings.autoClearScreen = false;
    printSettingsAfterChange();
    menuBeep();
    drawClearMenu();
  }
  else if (menuState == MENU_BACKLIGHT) {
    if (settings.backlightPercent == 0)
      settings.backlightPercent = 100;
    else
      settings.backlightPercent -= 5;
    applyBacklight();
    printSettingsAfterChange();
    menuBeep();
    drawBacklightMenu();
  }
  else if (menuState == MENU_CONTRAST) {
    if (settings.contrastPercent <= 50)
      settings.contrastPercent = 100;
    else
      settings.contrastPercent -= 5;
    applyContrast();
    printSettingsAfterChange();
    menuBeep();
    drawContrastMenu();
  }
  else if (menuState == MENU_DISPLAY_ROT) {
    settings.displayRotated = !settings.displayRotated;
    applyDisplayRotation();
    printSettingsAfterChange();
    menuBeep();
    drawDisplayRotMenu();
  }
  else if (menuState == MENU_SWAP_DIT_DAH) {
    settings.swapPaddles = false;
    printSettingsAfterChange();
    menuBeep();
    drawSwapDitDahMenu();
  }
  else if (menuState == MENU_MORSE_CODES) {
    morseCodesNextPage();
  }
}

void menuSelect() {
  menuBeep(true);

  if (menuState == MENU_MAIN) {
    switch (menuItem) {
      case MENU_ITEM_SPEED:
        menuState = MENU_SPEED;
        drawSpeedMenu();
        break;
      case MENU_ITEM_IAMBIC:
        menuState = MENU_IAMBIC;
        drawIambicMenu();
        break;
      case MENU_ITEM_BEEP:
        menuState = MENU_BEEP;
        drawBeepMenu();
        break;
      case MENU_ITEM_LANGUAGE:
        menuState = MENU_LANGUAGE;
        drawLanguageMenu();
        break;
      case MENU_ITEM_CLEAR:
        menuState = MENU_CLEAR;
        drawClearMenu();
        break;
      case MENU_ITEM_BACKLIGHT:
        menuState = MENU_BACKLIGHT;
        drawBacklightMenu();
        break;
      case MENU_ITEM_CONTRAST:
        menuState = MENU_CONTRAST;
        drawContrastMenu();
        break;
      case MENU_ITEM_DISPLAY_ROT:
        menuState = MENU_DISPLAY_ROT;
        drawDisplayRotMenu();
        break;
      case MENU_ITEM_SWAP_DIT_DAH:
        menuState = MENU_SWAP_DIT_DAH;
        drawSwapDitDahMenu();
        break;
      case MENU_ITEM_ABOUT:
        menuState = MENU_ABOUT;
        printAboutSerial();
        drawAboutMenu();
        break;
      case MENU_ITEM_MORSE_CODES:
        menuState = MENU_MORSE_CODES;
        morseCodesPage = 0;
        drawMorseCodesMenu();
        printMorseCodesPageSerial();
        break;
      case MENU_ITEM_BACK:
        exitMenu();
        break;
    }
    return;
  }

  if (menuState == MENU_SPEED ||
      menuState == MENU_IAMBIC ||
      menuState == MENU_BEEP ||
      menuState == MENU_LANGUAGE ||
      menuState == MENU_CLEAR ||
      menuState == MENU_BACKLIGHT ||
      menuState == MENU_CONTRAST ||
      menuState == MENU_DISPLAY_ROT ||
      menuState == MENU_SWAP_DIT_DAH ||
      menuState == MENU_ABOUT ||
      menuState == MENU_MORSE_CODES) {

    MenuState previousMenuState = menuState;

    saveSettings();
    menuState = MENU_MAIN;
    drawMenu();

    // Restore the main menu in USB-Serial and Bluetooth-Serial after leaving any submenu.
    if (previousMenuState != MENU_MAIN)
      printMainMenuSerial();
  }
}

// ============================================================
// MENU BUTTON HANDLING
// ============================================================
void processMenu() {
  bool ditPressed = digitalRead(PIN_DIT) == LOW;
  bool dahPressed = digitalRead(PIN_DAH) == LOW;
  bool commandPressed = digitalRead(PIN_COMMAND) == LOW;

  static bool lastDit = false;
  static bool lastDah = false;
  static bool lastCommand = false;

  unsigned long now = millis();

  // The long COMMAND press used to enter the menu must be released
  // before COMMAND can act as SELECT.
  if (!commandPressed) {
    menuCommandArmed = true;
  }

  if (ditPressed && !lastDit && (now - lastCommandEdgeTime) >= BUTTON_DEBOUNCE_MS) {
    menuDown();
    lastCommandEdgeTime = now;
  }

  if (dahPressed && !lastDah && (now - lastCommandEdgeTime) >= BUTTON_DEBOUNCE_MS) {
    menuUp();
    lastCommandEdgeTime = now;
  }

  if (commandPressed &&
      !lastCommand &&
      menuCommandArmed &&
      (now - lastCommandEdgeTime) >= BUTTON_DEBOUNCE_MS) {

    menuSelect();

    // Require release before another SELECT.
    menuCommandArmed = false;
    lastCommandEdgeTime = now;
  }

  lastDit = ditPressed;
  lastDah = dahPressed;
  lastCommand = commandPressed;
}

// ============================================================
// COMMAND BUTTON
// ============================================================
void processCommand() {

  bool btnState = digitalRead(PIN_COMMAND);
  unsigned long now = millis();

  // Detect press.
  if (lastBtnState == HIGH && btnState == LOW) {

    if ((now - lastCommandEdgeTime) >= BUTTON_DEBOUNCE_MS) {
      commandPressStart = now;
      commandLongHandled = false;
      lastCommandEdgeTime = now;
    }
  }

  // Long press -> enter menu.
  if (btnState == LOW &&
      commandPressStart != 0 &&
      !commandLongHandled &&
      (now - commandPressStart) >= COMMAND_LONG_PRESS_MS) {

    commandLongHandled = true;
    enterMenu();
  }

  // Detect release.
  if (lastBtnState == LOW && btnState == HIGH) {

    if ((now - lastCommandEdgeTime) >= BUTTON_DEBOUNCE_MS) {

      // Short press:
      // - when CLR is blinking: confirm screen clear;
      // - otherwise: keep the existing manual clear behavior.
      if (!commandLongHandled &&
          commandPressStart != 0 &&
          (now - commandPressStart) < COMMAND_LONG_PRESS_MS) {

        // Manual screen clear starts a new Serial/BLE line.
        serialPrintlnMirror();

        clearBuffer();

        if (settings.sidetoneEnabled) {
          tone(PIN_SIDETONE, 1000);
          delay(60);
          noTone(PIN_SIDETONE);
        }
      }

      commandPressStart = 0;
      commandLongHandled = false;
      lastCommandEdgeTime = now;
    }
  }

  lastBtnState = btnState;
}

// ============================================================
// SPLASH SCREEN
// ============================================================
void showSplash() {

  lcd.clearBuffer();
  lcd.setFont(u8g2_font_5x7_t_cyrillic);

  lcd.drawStr(19, 7, "CW TRAINER");
  lcd.drawStr(22, 16, "by LU6APR");
  lcd.drawStr(40, 24, "&");
  lcd.drawStr(15, 32, "Pavlo Luzan");
  lcd.drawStr(32, 44, "2026");
  lcd.sendBuffer();
  delay(3000);

  // Mirror the status line after the splash screen.
  printStatusLineSerial();
}

// ============================================================
// SETUP
// ============================================================

// ============================================================
// BLE SERIAL - FREERTOS QUEUE + DEDICATED TASK
// ============================================================
#define BLE_SERIAL_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_SERIAL_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_SERIAL_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* bleTxCharacteristic = nullptr;
volatile bool bleConnected = false;

QueueHandle_t bleTxQueue = nullptr;
TaskHandle_t bleTaskHandle = nullptr;

constexpr UBaseType_t BLE_TX_QUEUE_LENGTH = 128;

struct BleTxItem {
  uint8_t data[20];
  uint8_t length;
};

class BleServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    bleConnected = true;
    updateDisplay();
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    bleConnected = false;
    updateDisplay();

    if (bleTxQueue != nullptr) {
      BleTxItem item;
      while (xQueueReceive(bleTxQueue, &item, 0) == pdTRUE) {
      }
    }

    NimBLEDevice::startAdvertising();
  }
};

class BleRxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic,
               NimBLEConnInfo& connInfo) override {
    // RX intentionally does not affect the existing Morse/keyer logic.
  }
};

void bleQueueBytes(const uint8_t* data, size_t length) {
  if (!bleConnected || bleTxQueue == nullptr || data == nullptr || length == 0)
    return;

  while (length > 0) {
    BleTxItem item;
    item.length = (length > sizeof(item.data)) ? sizeof(item.data) : length;
    memcpy(item.data, data, item.length);

    // Never wait for BLE from the Morse processing path.
    if (xQueueSend(bleTxQueue, &item, 0) != pdTRUE)
      return;

    data += item.length;
    length -= item.length;
  }
}

void bleQueuePrintln(const char* text) {
  if (!bleConnected)
    return;

  if (text != nullptr)
    bleQueueBytes((const uint8_t*)text, strlen(text));

  const uint8_t eol[] = {'\r', '\n'};
  bleQueueBytes(eol, sizeof(eol));
}

void bleSerialTask(void* parameter) {
  BleTxItem item;

  for (;;) {
    if (xQueueReceive(bleTxQueue, &item, portMAX_DELAY) == pdTRUE) {

      // BLE may temporarily reject a notification while the controller
      // is busy. Keep the queued item and retry from this dedicated task.
      while (bleConnected && bleTxCharacteristic != nullptr) {

        bleTxCharacteristic->setValue(item.data, item.length);

        if (bleTxCharacteristic->notify()) {
          // Yield after every successful notification.
          vTaskDelay(pdMS_TO_TICKS(5));
          break;
        }

        // Do not block the Morse/keyer path; only this BLE task waits.
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
  }
}

void bleSerialBegin() {
  bleTxQueue = xQueueCreate(BLE_TX_QUEUE_LENGTH, sizeof(BleTxItem));

  if (bleTxQueue == nullptr)
    return;

  NimBLEDevice::init("CW TRAINER");

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new BleServerCallbacks());

  NimBLEService* service =
      bleServer->createService(BLE_SERIAL_SERVICE_UUID);

  bleTxCharacteristic =
      service->createCharacteristic(
          BLE_SERIAL_TX_UUID,
          NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic* rx =
      service->createCharacteristic(
          BLE_SERIAL_RX_UUID,
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_NR);

  rx->setCallbacks(new BleRxCallbacks());

  bleServer->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();

  // Keep the device name in the primary advertising packet.
  // The UART service remains available through GATT after connection.
  advertising->setName("CW TRAINER");
  advertising->enableScanResponse(false);

  bool started = advertising->start();

  Serial.print("BLE advertising: ");
  Serial.println(started ? "OK" : "FAILED");

  Serial.print("BLE advertising active: ");
  Serial.println(advertising->isAdvertising() ? "YES" : "NO");

  xTaskCreate(
      bleSerialTask,
      "BLE_SERIAL",
      4096,
      nullptr,
      1,
      &bleTaskHandle
  );
}

void setup() {

  pinMode(PIN_DIT, INPUT_PULLUP);
  pinMode(PIN_DAH, INPUT_PULLUP);
  pinMode(PIN_COMMAND, INPUT_PULLUP);

  pinMode(PIN_SIDETONE, OUTPUT);

  pinMode(PIN_BACKLIGHT, OUTPUT);

  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
  ledcAttachPin(PIN_BACKLIGHT, BACKLIGHT_CHANNEL);

  pinMode(PIN_KEY_OUT, OUTPUT);
  digitalWrite(PIN_KEY_OUT, LOW);

  Serial.begin(115200);
  bleSerialBegin();

  serialPrintlnMirror("CW TRAINER");
  serialPrintlnMirror("by LU6APR");
  serialPrintlnMirror("& Pavlo Luzan");
  serialPrintlnMirror("2026");

  delay(100);

  // Load settings from EEPROM.
  EEPROM.begin(EEPROM_LANGUAGE_ADDR + 1);

  EEPROM.get(EEPROM_SETTINGS_ADDR, settings);

  if (settings.speedWPM < 5 || settings.speedWPM > 50 ||
      settings.backlightPercent > 100 ||
      settings.contrastPercent < 50 || settings.contrastPercent > 100 ||
      settings.displayRotated > 1) {
    settings = {15, true, false, true, 600, true, DEFAULT_BACKLIGHT_PERCENT, DEFAULT_CONTRAST_PERCENT, false};
  }

  uint8_t storedLanguage = 0xFF;
  EEPROM.get(EEPROM_LANGUAGE_ADDR, storedLanguage);

  if (storedLanguage == 0 || storedLanguage == 1)
    morseCyrillic = (storedLanguage == 1);
  else
    morseCyrillic = true;

  EEPROM.end();

  // Nokia 5110 / PCD8544 initialization.
  // Pin mapping was verified separately with a standalone display test.
  lcd.begin();
  applyDisplayRotation();
  applyContrast();
  applyBacklight();

  showSplash();

  lastKeyActivityTime = millis();

  lastBtnState = digitalRead(PIN_COMMAND);
  commandPressStart = 0;
  commandLongHandled = false;

  setReadyScreen();

  if (settings.sidetoneEnabled) {

    tone(PIN_SIDETONE, 600);
    delay(200);
    noTone(PIN_SIDETONE);
  }
}

// ============================================================
// LOOP
// ============================================================
void loop() {

  if (menuState != MENU_OFF) {

    processMenu();

  }
  else {

    processKeyer();
    processDecoder();
    processCommand();
    verificarAutoClear();
    updateClearIndicator();
  }

  delay(2);
}
