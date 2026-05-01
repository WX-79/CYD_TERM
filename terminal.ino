#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <EEPROM.h>
#include <FS.h>
#include <time.h>
#include <Wire.h>
#include <TimeLib.h>
#include <qrcode_espi.h>

struct CalendarNote {
  int year, month, day;
  String note;
  bool hasNote;
};

#define I2C_SCL 39
#define I2C_SDA 16

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// ==================== PIN DEFINITIONS ====================
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
#define SD_CS 5
#define SOUND_PIN 26
#define EEPROM_SIZE 256

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
WebServer server(80);

// ==================== THEME SETTINGS ====================
uint16_t BG_COLOR = TFT_WHITE;
uint16_t TEXT_COLOR = TFT_BLACK;
uint16_t ACCENT_COLOR = TFT_SKYBLUE;
uint16_t BUTTON_COLOR = TFT_LIGHTGREY;
uint16_t WARNING_COLOR = TFT_RED;
uint16_t SUCCESS_COLOR = TFT_GREEN;
bool darkMode = false;

// ==================== SYSTEM SETTINGS ====================
#define MAX_HISTORY 40
#define VISIBLE_LINES 9
std::vector<String> terminalHistory;
int scrollOffset = 0;

String currentInput = "";
int kbMode = 0;
unsigned long lastCursorBlink = 0;
bool cursorVisible = true;
bool soundEnabled = true;
int brightness = 150;

// ==================== PREFIX VARIABLES ====================
String cmdPrefix = ">";
String promptPrefix = "> ";
String errorPrefix = "[ERROR] ";
String successPrefix = "[OK] ";
String infoPrefix = "[INFO] ";

// ==================== ENHANCED KEYBOARD ====================
const char* keys[4][4] = {
  { "qwertzuiop", "asdfghjkl", "yxcvbnm", "äöüß" },
  { "QWERTZUIOP", "ASDFGHJKL", "YXCVBNM", "ÄÖÜ" },
  { "1234567890", "-=[]\\;',", "./`~!@#$%", "^&*()_+" },
  { "{}|:<>?\"", "^&*()_+", "äöüß", "^&*()_+" }
};

// ==================== CHIP-8 EMULATOR ====================
// ==================== VERBESSERTER SCHIP-EMULATOR ====================
// ==================== ORIGINAL CHIP-8 EMULATOR (NUR CHIP-8) ====================
class Chip8Emulator {
private:
  uint8_t memory[4096];  // Standard 4KB für CHIP-8
  uint8_t V[16];
  uint16_t I;
  uint16_t pc;
  uint16_t stack[16];
  uint8_t sp;
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint8_t keypad[16];
  uint8_t display[64 * 32];  // Standard 64x32 Display
  bool drawFlag;
  bool running;
  unsigned long lastTimerUpdate;
  unsigned long lastKeyRelease;
  String romName;
  uint32_t cycleCount;

  // Clock Speed Control
  int clockSpeed;  // 0=slow, 1=normal, 2=fast
  unsigned long lastCycleTime;

  const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80   // F
  };

public:
  Chip8Emulator() {
    clockSpeed = 1;  // Normal Speed
    reset();
  }

  void reset() {
    memset(memory, 0, sizeof(memory));
    memset(V, 0, sizeof(V));
    memset(stack, 0, sizeof(stack));
    memset(display, 0, sizeof(display));
    memset(keypad, 0, sizeof(keypad));
    I = 0;
    pc = 0x200;
    sp = 0;
    delay_timer = 0;
    sound_timer = 0;
    drawFlag = true;
    running = true;
    cycleCount = 0;
    romName = "";

    for (int i = 0; i < 80; i++) {
      memory[i] = fontset[i];
    }

    lastTimerUpdate = millis();
    lastKeyRelease = millis();
    lastCycleTime = millis();
  }

  // Clock Speed Control
  void setClockSpeed(int speed) {
    clockSpeed = constrain(speed, 0, 2);
  }

  int getClockSpeed() {
    return clockSpeed;
  }

  String getClockSpeedName() {
    switch (clockSpeed) {
      case 0: return "SLOW";
      case 1: return "NORMAL";
      case 2: return "FAST";
      default: return "NORMAL";
    }
  }

  int getCycleDelay() {
    switch (clockSpeed) {
      case 0: return 4;  // ~250Hz
      case 1: return 2;  // ~500Hz (Standard)
      case 2: return 1;  // ~1000Hz
      default: return 2;
    }
  }

  bool loadROM(String filename) {
    reset();

    String fullPath = "/" + filename;
    Serial.println("Trying to load: " + fullPath);

    File rom = SD.open(fullPath, FILE_READ);
    if (!rom) {
      Serial.println("ERROR: Cannot open file!");
      return false;
    }

    size_t romSize = rom.size();
    Serial.print("ROM size: ");
    Serial.println(romSize);

    if (romSize > 3584) {
      Serial.println("ERROR: ROM too large! Max 3584 bytes");
      rom.close();
      return false;
    }

    memset(memory + 0x200, 0, 3584);
    size_t bytesRead = rom.read(memory + 0x200, romSize);
    rom.close();

    Serial.print("Bytes read: ");
    Serial.println(bytesRead);

    if (bytesRead == 0) {
      Serial.println("ERROR: No bytes read!");
      return false;
    }

    romName = filename;
    pc = 0x200;
    drawFlag = true;
    running = true;
    memset(display, 0, sizeof(display));

    uint16_t firstOpcode = (memory[0x200] << 8) | memory[0x201];
    Serial.print("First opcode: 0x");
    Serial.println(firstOpcode, HEX);

    Serial.println("ROM loaded successfully!");
    return true;
  }

  void keyPress(int key) {
    if (key >= 0 && key < 16) {
      keypad[key] = 1;
      lastKeyRelease = millis();
    }
  }

  void keyRelease() {
    if (millis() - lastKeyRelease > 100) {
      memset(keypad, 0, sizeof(keypad));
    }
  }

  void emulateCycle() {
    if (!running) return;

    uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];

    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t n = opcode & 0x000F;
    uint8_t kk = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;

    pc += 2;
    cycleCount++;

    switch (opcode & 0xF000) {
      case 0x0000:
        if ((opcode & 0x00FF) == 0x00E0) {
          memset(display, 0, sizeof(display));
          drawFlag = true;
        } else if ((opcode & 0x00FF) == 0x00EE) {
          if (sp > 0) {
            sp--;
            pc = stack[sp];
          }
        }
        break;

      case 0x1000:
        pc = nnn;
        break;

      case 0x2000:
        if (sp < 16) {
          stack[sp] = pc;
          sp++;
          pc = nnn;
        }
        break;

      case 0x3000:
        if (V[x] == kk) pc += 2;
        break;

      case 0x4000:
        if (V[x] != kk) pc += 2;
        break;

      case 0x5000:
        if (V[x] == V[y]) pc += 2;
        break;

      case 0x6000:
        V[x] = kk;
        break;

      case 0x7000:
        V[x] += kk;
        break;

      case 0x8000:
        switch (opcode & 0x000F) {
          case 0x0: V[x] = V[y]; break;
          case 0x1: V[x] |= V[y]; break;
          case 0x2: V[x] &= V[y]; break;
          case 0x3: V[x] ^= V[y]; break;
          case 0x4:
            {
              uint16_t sum = V[x] + V[y];
              V[0xF] = (sum > 255) ? 1 : 0;
              V[x] = sum & 0xFF;
              break;
            }
          case 0x5:
            {
              V[0xF] = (V[x] > V[y]) ? 1 : 0;
              V[x] -= V[y];
              break;
            }
          case 0x6:
            {
              V[0xF] = V[x] & 0x1;
              V[x] >>= 1;
              break;
            }
          case 0x7:
            {
              V[0xF] = (V[y] > V[x]) ? 1 : 0;
              V[x] = V[y] - V[x];
              break;
            }
          case 0xE:
            {
              V[0xF] = (V[x] >> 7) & 0x1;
              V[x] <<= 1;
              break;
            }
        }
        break;

      case 0x9000:
        if (V[x] != V[y]) pc += 2;
        break;

      case 0xA000:
        I = nnn;
        break;

      case 0xB000:
        pc = nnn + V[0];
        break;

      case 0xC000:
        V[x] = random(256) & kk;
        break;

      case 0xD000:
        {
          uint8_t xPos = V[x] & 0x3F;
          uint8_t yPos = V[y] & 0x1F;
          V[0xF] = 0;

          for (int row = 0; row < n; row++) {
            uint8_t spriteByte = memory[I + row];
            for (int col = 0; col < 8; col++) {
              if ((spriteByte & (0x80 >> col)) != 0) {
                int pixelX = (xPos + col) % 64;
                int pixelY = (yPos + row) % 32;
                int idx = pixelY * 64 + pixelX;

                if (display[idx] == 1) V[0xF] = 1;
                display[idx] ^= 1;
              }
            }
          }
          drawFlag = true;
          break;
        }

      case 0xE000:
        if ((opcode & 0x00FF) == 0x009E) {
          if (keypad[V[x] & 0x0F]) pc += 2;
        } else if ((opcode & 0x00FF) == 0x00A1) {
          if (!keypad[V[x] & 0x0F]) pc += 2;
        }
        break;

      case 0xF000:
        switch (opcode & 0x00FF) {
          case 0x07: V[x] = delay_timer; break;
          case 0x0A:
            {
              bool pressed = false;
              for (int i = 0; i < 16; i++) {
                if (keypad[i]) {
                  V[x] = i;
                  pressed = true;
                  break;
                }
              }
              if (!pressed) pc -= 2;
              break;
            }
          case 0x15: delay_timer = V[x]; break;
          case 0x18: sound_timer = V[x]; break;
          case 0x1E: I += V[x]; break;
          case 0x29: I = V[x] * 5; break;
          case 0x33:
            {
              uint8_t value = V[x];
              memory[I] = value / 100;
              memory[I + 1] = (value / 10) % 10;
              memory[I + 2] = value % 10;
              break;
            }
          case 0x55:
            {
              for (int i = 0; i <= x; i++) {
                memory[I + i] = V[i];
              }
              I += x + 1;
              break;
            }
          case 0x65:
            {
              for (int i = 0; i <= x; i++) {
                V[i] = memory[I + i];
              }
              I += x + 1;
              break;
            }
        }
        break;
    }
  }

  void updateTimers() {
    if (millis() - lastTimerUpdate >= 16) {
      if (delay_timer > 0) delay_timer--;
      if (sound_timer > 0) {
        sound_timer--;
        if (sound_timer == 0 && soundEnabled) {
          tone(SOUND_PIN, 440, 50);
        }
      }
      lastTimerUpdate = millis();
    }
  }

  void draw(TFT_eSPI& tft, int xOffset, int yOffset, int scale) {
    // Hintergrund löschen
    tft.fillRect(xOffset - 2, yOffset - 2, 64 * scale + 4, 32 * scale + 4, BG_COLOR);

    // Pixel zeichnen
    for (int y = 0; y < 32; y++) {
      for (int x = 0; x < 64; x++) {
        if (display[y * 64 + x]) {
          tft.fillRect(xOffset + x * scale, yOffset + y * scale, scale, scale, TFT_GREEN);
        }
      }
    }

    // Rahmen
    tft.drawRect(xOffset - 2, yOffset - 2, 64 * scale + 4, 32 * scale + 4, TEXT_COLOR);
    drawFlag = false;
  }

  bool needsDraw() {
    return drawFlag;
  }
  bool isRunning() {
    return running;
  }
  String getROMName() {
    return romName;
  }
  uint16_t getPC() {
    return pc;
  }
  void setRunning(bool state) {
    running = state;
  }
  uint8_t* getDisplay() {
    return display;
  }
};

// Globale Instanz
Chip8Emulator chip8;


// ==================== GLOBALE VARIABLEN ====================
String wifiSSID = "";
String wifiPassword = "";

// Game variables
int snakeX[100], snakeY[100];
int snakeLength = 3;
int foodX, foodY;
int snakeDir = 2;
bool gameRunning = false;
int score = 0;
int highScore = 0;
char board[3][3];
bool playerTurn = true;
int gameMode = 0;
std::vector<String> notes;
int selectedNote = -1;

// ==================== FORWARD DEKLARATIONEN ====================
void updateInputLine(bool force = false);
void drawKeyboard();
void drawScrollButtons();
void refreshTerminal();
String getTextInput();
void applyTheme();
void fileManager();
void settingsMenu();
void snakeGame();
void pongGame();
void ticTacToe();
void drawingApp();
void notesApp();
void todoApp();
void timerApp();
void chatApp(bool isHost);
void wifiManager();
void sysInfo();
void storageInfo();
void writeHelpFile();
void calculator();
void chip8Emulator();
void playSysSound(int type);
bool getTouch(int& x, int& y);
String handleKeyboardInput();
void printToConsole(String text, uint16_t color = -1);
void listDirectory(String path, std::vector<String>& files, std::vector<String>& dirs);
void deleteFileOrDir(String path);
void copyFileItem(String src, String dst);
void moveFileItem(String src, String dst);
String evaluateExpression(String expr);
String evaluateScientific(String expr);
String evaluateProgrammer(String expr);
long parseNumber(String num);

QRcode_eSPI qrcode(&tft);  // Korrekter Klassenname
void createQRCode(String data, int size, int xOffset, int yOffset) {
  QRcode_eSPI qrcode(&tft);
  qrcode.init();
  
  // Die create() Methode erzeugt den QR-Code automatisch in der richtigen Größe
  // Die Skalierung wird bei dieser Bibliothek nicht separat gesetzt
  qrcode.create(data);
  
  // Die display-Methode ist privat - stattdessen wird der QR-Code automatisch angezeigt
  // Der QR-Code wird nach create() automatisch auf dem Display gezeichnet
}

void qrGeneratorApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;
  
  bool running = true;
  int mode = 0;
  String qrData = "";
  bool qrGenerated = false;
  int lastMode = -1;
  bool needsRedraw = true;  // NEU: Nur bei Bedarf neu zeichnen
  
  while (running) {
    int tx, ty;
    bool touchDetected = getTouch(tx, ty);
    
    // Nur neu zeichnen wenn nötig (nicht bei jedem Loop)
    if (needsRedraw) {
      tft.fillScreen(BG_COLOR);
      
      // Header
      tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.drawCentreString("QR GENERATOR", 120, 42, 2);
      
      // ESC Button
      tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
      tft.drawCentreString("ESC", 207, 10, 2);
      
      // Modus Buttons
      const char* modes[] = {"TEXT", "URL", "WIFI", "VCARD"};
      for (int i = 0; i < 4; i++) {
        int x = 10 + i * 55;
        uint16_t color = (mode == i) ? SUCCESS_COLOR : BUTTON_COLOR;
        tft.fillRoundRect(x, 70, 50, 25, 3, color);
        tft.setTextColor(TEXT_COLOR);
        tft.drawCentreString(modes[i], x + 25, 82, 1);
      }
      
      // Input Bereich
      tft.fillRoundRect(10, 105, 220, 45, 4, BUTTON_COLOR);
      tft.setTextSize(1);
      tft.setTextColor(TFT_CYAN);
      tft.setCursor(15, 115);
      
      if (mode == 0) tft.print("Enter text (max 200 chars):");
      else if (mode == 1) tft.print("Enter URL (http://...):");
      else if (mode == 2) tft.print("SSID:PASSWORD:ENC:");
      else if (mode == 3) tft.print("Name:Tel:Email:");
      
      if (qrData != "") {
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(15, 130);
        String display = qrData;
        if (display.length() > 28) display = display.substring(0, 25) + "...";
        tft.print(display);
      } else {
        tft.setTextColor(TFT_DARKGREY);
        tft.setCursor(15, 130);
        tft.print("No data entered yet");
      }
      
      // Generate Button
      if (!qrGenerated) {
        tft.fillRoundRect(10, 160, 100, 30, 4, SUCCESS_COLOR);
        tft.drawCentreString("GENERATE", 60, 175, 1);
      }
      
      // Save Button
      if (qrGenerated && qrData != "") {
        tft.fillRoundRect(120, 160, 100, 30, 4, TFT_BLUE);
        tft.drawCentreString("SAVE", 170, 175, 1);
      }
      
      // QR Code Anzeige Bereich
      tft.drawRect(35, 200, 170, 170, TEXT_COLOR);
      
      needsRedraw = false;
    }
    
    // QR-Code separat zeichnen (ohne den Rest zu löschen)
    if (qrGenerated && qrData != "") {
      // Nur den QR-Code Bereich löschen und neu zeichnen
      tft.fillRect(36, 201, 168, 168, BG_COLOR);
      tft.drawRect(35, 200, 170, 170, TEXT_COLOR);
      
      QRcode_eSPI qrcode(&tft);
      qrcode.init();
      qrcode.create(qrData);
    } else if (!qrGenerated) {
      // Platzhalter anzeigen (nur wenn kein QR)
      tft.fillRect(36, 201, 168, 168, BG_COLOR);
      tft.drawRect(35, 200, 170, 170, TEXT_COLOR);
      tft.setTextSize(1);
      tft.setTextColor(TFT_DARKGREY);
      tft.drawCentreString("QR Code", 120, 270, 2);
      tft.drawCentreString("will appear here", 120, 290, 1);
      tft.drawCentreString("after pressing", 120, 305, 1);
      tft.drawCentreString("GENERATE", 120, 320, 1);
    }
    
    // Touch Handling
    if (touchDetected) {
      // ESC Button
      if (tx > 180 && ty < 40) {
        running = false;
      }
      // Modus Auswahl
      else if (ty > 70 && ty < 95) {
        for (int i = 0; i < 4; i++) {
          int x = 10 + i * 55;
          if (tx > x && tx < x + 50) {
            if (mode != i) {
              mode = i;
              qrData = "";
              qrGenerated = false;
              needsRedraw = true;  // Komplett neu zeichnen
              playSysSound(0);
            }
            break;
          }
        }
      }
      // GENERATE Button
      else if (!qrGenerated && tx < 110 && ty > 160 && ty < 190) {
        printToConsole(infoPrefix + "Enter data:", TFT_BLUE);
        
        String newData = "";
        if (mode == 0) {
          newData = getTextInput();
        } else if (mode == 1) {
          newData = getTextInput();
          if (!newData.startsWith("http")) newData = "http://" + newData;
        } else if (mode == 2) {
          printToConsole(infoPrefix + "Enter SSID:", TFT_BLUE);
          String ssid = getTextInput();
          printToConsole(infoPrefix + "Enter password:", TFT_BLUE);
          String pwd = getTextInput();
          newData = "WIFI:S:" + ssid + ";T:WPA;P:" + pwd + ";;";
        } else if (mode == 3) {
          printToConsole(infoPrefix + "Enter name:", TFT_BLUE);
          String name = getTextInput();
          printToConsole(infoPrefix + "Enter phone:", TFT_BLUE);
          String phone = getTextInput();
          printToConsole(infoPrefix + "Enter email:", TFT_BLUE);
          String email = getTextInput();
          newData = "BEGIN:VCARD\nVERSION:3.0\nFN:" + name + "\nTEL:" + phone + "\nEMAIL:" + email + "\nEND:VCARD";
        }
        
        if (newData != "") {
          qrData = newData;
          qrGenerated = true;
          needsRedraw = true;  // Neu zeichnen mit QR-Code
          playSysSound(1);
          printToConsole(successPrefix + "QR Code generated!", TFT_GREEN);
        }
      }
      // SAVE Button
      else if (qrGenerated && tx > 120 && ty > 160 && ty < 190 && qrData != "") {
        String filename = "/qr_" + String(millis()) + ".txt";
        File f = SD.open(filename, FILE_WRITE);
        if (f) {
          f.println(qrData);
          f.close();
          printToConsole(successPrefix + "QR data saved to " + filename, TFT_GREEN);
          playSysSound(1);
        } else {
          printToConsole(errorPrefix + "Save failed!", TFT_RED);
        }
        delay(500);
      }
      // NEU: Bildschirm antippen zum Zurücksetzen (optional)
      else if (qrGenerated && qrData != "") {
        // Nur wenn außerhalb aller Buttons
        if (!(tx > 180 && ty < 40) && 
            !(ty > 70 && ty < 95) &&
            !(ty > 160 && ty < 190)) {
          
          // Optional: Bestätigungsdialog
          tft.fillRect(0, 0, 240, 320, BG_COLOR);
          tft.fillRoundRect(30, 100, 180, 120, 8, BUTTON_COLOR);
          tft.setTextSize(1);
          tft.setTextColor(TEXT_COLOR);
          tft.drawCentreString("Create new QR code?", 120, 120, 2);
          tft.drawCentreString("Current data will be lost", 120, 140, 1);
          
          tft.fillRoundRect(50, 170, 60, 30, 4, SUCCESS_COLOR);
          tft.drawCentreString("YES", 80, 182, 2);
          tft.fillRoundRect(130, 170, 60, 30, 4, WARNING_COLOR);
          tft.drawCentreString("NO", 160, 182, 2);
          
          bool waiting = true;
          while (waiting) {
            if (getTouch(tx, ty)) {
              if (tx > 50 && tx < 110 && ty > 170 && ty < 200) {
                qrData = "";
                qrGenerated = false;
                needsRedraw = true;
                waiting = false;
                playSysSound(0);
              } else if (tx > 130 && tx < 190 && ty > 170 && ty < 200) {
                waiting = false;
                playSysSound(0);
              }
            }
            delay(50);
          }
        }
      }
    }
    
    delay(20);
  }
  
  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== KALENDER MIT NOTIZEN ====================

std::vector<CalendarNote> calendarNotes;
int currentYear = 2024;
int currentMonth = 1;
int currentDay = 1;

// ==================== KALENDER NOTIZEN SPEICHERN ====================
void saveCalendarNotes() {
  File f = SD.open("/calendar_notes.txt", FILE_WRITE);
  if (!f) {
    printToConsole(errorPrefix + "Cannot save calendar notes!", TFT_RED);
    return;
  }

  for (int i = 0; i < (int)calendarNotes.size(); i++) {
    f.print(calendarNotes[i].year);
    f.print("|");
    f.print(calendarNotes[i].month);
    f.print("|");
    f.print(calendarNotes[i].day);
    f.print("|");
    f.println(calendarNotes[i].note);
  }
  f.close();
}

// ==================== KALENDER NOTIZEN LADEN ====================
void loadCalendarNotes() {
  calendarNotes.clear();

  File f = SD.open("/calendar_notes.txt", FILE_READ);
  if (!f) return;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.replace("\r", "");

    int firstSep = line.indexOf('|');
    int secondSep = line.indexOf('|', firstSep + 1);
    int thirdSep = line.indexOf('|', secondSep + 1);

    if (firstSep > 0 && secondSep > 0 && thirdSep > 0) {
      CalendarNote note;
      note.year = line.substring(0, firstSep).toInt();
      note.month = line.substring(firstSep + 1, secondSep).toInt();
      note.day = line.substring(secondSep + 1, thirdSep).toInt();
      note.note = line.substring(thirdSep + 1);
      note.hasNote = true;
      calendarNotes.push_back(note);
    }
  }
  f.close();
}

// ==================== NOTIZ FÜR TAG HOLEN ====================
String getNoteForDate(int year, int month, int day) {
  for (int i = 0; i < (int)calendarNotes.size(); i++) {
    if (calendarNotes[i].year == year && calendarNotes[i].month == month && calendarNotes[i].day == day) {
      return calendarNotes[i].note;
    }
  }
  return "";
}

// ==================== NOTIZ HINZUFÜGEN/BEARBEITEN ====================
void addOrEditNote(int year, int month, int day) {
  String existingNote = getNoteForDate(year, month, day);

  tft.fillScreen(BG_COLOR);
  tft.setTextSize(2);
  tft.drawCentreString("NOTE FOR", 120, 45, 2);
  tft.drawCentreString(String(day) + "." + String(month) + "." + String(year), 120, 70, 2);
  tft.setTextSize(1);

  if (existingNote != "") {
    printToConsole(infoPrefix + "Edit existing note: " + existingNote, TFT_BLUE);
  } else {
    printToConsole(infoPrefix + "Add new note for " + String(day) + "." + String(month) + "." + String(year), TFT_BLUE);
  }

  printToConsole(infoPrefix + "Enter note (max 100 chars):", TFT_BLUE);
  String newNote = getTextInput();

  if (newNote != "") {
    // Prüfen ob schon eine Notiz existiert
    for (int i = 0; i < (int)calendarNotes.size(); i++) {
      if (calendarNotes[i].year == year && calendarNotes[i].month == month && calendarNotes[i].day == day) {
        calendarNotes[i].note = newNote;
        saveCalendarNotes();
        printToConsole(successPrefix + "Note updated!", TFT_GREEN);
        return;
      }
    }

    // Neue Notiz hinzufügen
    CalendarNote note;
    note.year = year;
    note.month = month;
    note.day = day;
    note.note = newNote;
    note.hasNote = true;
    calendarNotes.push_back(note);
    saveCalendarNotes();
    printToConsole(successPrefix + "Note added!", TFT_GREEN);
  }
}

// ==================== KALENDER ANZEIGEN ====================
void drawCalendar(int year, int month) {
  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  // Monat und Jahr Header
  tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

  const char* monthNames[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                               "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
  String header = String(monthNames[month - 1]) + " " + String(year);
  tft.drawCentreString(header, 120, 42, 2);

  // Navigation Buttons
  tft.fillRoundRect(10, 5, 50, 25, 4, TFT_BLUE);
  tft.drawCentreString("<", 35, 12, 2);

  tft.fillRoundRect(180, 5, 50, 25, 4, TFT_BLUE);
  tft.drawCentreString(">", 205, 12, 2);

  tft.fillRoundRect(180, 295, 55, 25, 4, WARNING_COLOR);
  tft.drawCentreString("ESC", 207, 302, 1);

  // Wochentage
  const char* weekdays[] = { "MO", "DI", "MI", "DO", "FR", "SA", "SO" };
  tft.setTextSize(1);
  tft.setTextColor(TEXT_COLOR);
  for (int i = 0; i < 7; i++) {
    tft.drawCentreString(weekdays[i], 20 + i * 32, 75, 1);
  }

  // Ersten Tag des Monats berechnen (vereinfacht)
  int firstDayOfWeek = 1;  // Annahme: 1. ist Montag

  // Anzahl Tage im Monat
  int daysInMonth;
  if (month == 2) {
    // Schaltjahr prüfen
    if ((year % 400 == 0) || (year % 4 == 0 && year % 100 != 0))
      daysInMonth = 29;
    else
      daysInMonth = 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    daysInMonth = 30;
  } else {
    daysInMonth = 31;
  }

  // Kalender zeichnen
  int startX = 10;
  int startY = 95;
  int cellW = 30;
  int cellH = 28;

  for (int day = 1; day <= daysInMonth; day++) {
    int weekIndex = (day + firstDayOfWeek - 2) % 7;
    int row = (day + firstDayOfWeek - 2) / 7;
    int x = startX + weekIndex * cellW;
    int y = startY + row * cellH;

    // Zelle hervorheben wenn aktueller Tag
    bool isToday = (day == currentDay && month == currentMonth && year == currentYear);

    // Notiz vorhanden?
    bool hasNote = (getNoteForDate(year, month, day) != "");

    if (isToday) {
      tft.fillRoundRect(x - 2, y - 2, cellW + 2, cellH + 2, 4, SUCCESS_COLOR);
      tft.setTextColor(TFT_WHITE);
    } else if (hasNote) {
      tft.fillRoundRect(x - 2, y - 2, cellW + 2, cellH + 2, 4, TFT_YELLOW);
      tft.setTextColor(TFT_BLACK);
    } else {
      tft.fillRoundRect(x - 2, y - 2, cellW + 2, cellH + 2, 4, BUTTON_COLOR);
      tft.setTextColor(TEXT_COLOR);
    }

    tft.setTextSize(2);
    tft.drawCentreString(String(day), x + cellW / 2, y + cellH / 2 - 5, 2);

    // Notiz-Indikator (kleiner Punkt)
    if (hasNote && !isToday) {
      tft.fillCircle(x + cellW - 5, y + 5, 3, TFT_RED);
    }
  }

  // Info Text
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 310);
  tft.print("Tap date for note | Yellow=has note");
}

// ==================== KALENDER GUI ====================
void calendarApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  loadCalendarNotes();

  // Aktuelles Datum holen (vereinfacht - für NTP später)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    currentYear = timeinfo.tm_year + 1900;
    currentMonth = timeinfo.tm_mon + 1;
    currentDay = timeinfo.tm_mday;
  }

  bool running = true;
  int selectedDay = -1;

  while (running) {
    drawCalendar(currentYear, currentMonth);

    int tx, ty;
    if (getTouch(tx, ty)) {
      // ESC Button
      if (tx > 180 && ty > 295 && ty < 320) {
        running = false;
      }
      // Previous Month Button
      else if (tx < 60 && ty < 35) {
        currentMonth--;
        if (currentMonth < 1) {
          currentMonth = 12;
          currentYear--;
        }
        playSysSound(0);
      }
      // Next Month Button
      else if (tx > 170 && tx < 230 && ty < 35) {
        currentMonth++;
        if (currentMonth > 12) {
          currentMonth = 1;
          currentYear++;
        }
        playSysSound(0);
      }
      // Kalender Zellen
      else if (ty > 90 && ty < 290 && tx > 5 && tx < 230) {
        int cellW = 30;
        int startX = 10;
        int startY = 95;

        int col = (tx - startX) / cellW;
        int row = (ty - startY) / 28;

        if (col >= 0 && col < 7 && row >= 0 && row < 6) {
          // Ersten Tag des Monats berechnen
          int firstDayOfWeek = 1;  // Montag
          int dayNumber = (row * 7 + col) - (firstDayOfWeek - 1) + 1;

          // Anzahl Tage im Monat
          int daysInMonth;
          if (currentMonth == 2) {
            if ((currentYear % 400 == 0) || (currentYear % 4 == 0 && currentYear % 100 != 0))
              daysInMonth = 29;
            else
              daysInMonth = 28;
          } else if (currentMonth == 4 || currentMonth == 6 || currentMonth == 9 || currentMonth == 11) {
            daysInMonth = 30;
          } else {
            daysInMonth = 31;
          }

          if (dayNumber >= 1 && dayNumber <= daysInMonth) {
            selectedDay = dayNumber;
            addOrEditNote(currentYear, currentMonth, selectedDay);
            playSysSound(1);
            delay(500);
          }
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== PERIODENSYSTEM DER ELEMENTE (ALLE 118) ====================
// ==================== STRUKTUREN & FORWARD DEKLARATIONEN ====================
// Vor allen Funktionen am Anfang der Datei einfügen!

struct Element {
  int number;
  String symbol;
  String name;
  String group;
  float atomicMass;
  int period;
  int groupNumber;
  int electrons;
  int protons;
  int neutrons;
  String discoverer;
  int discoveryYear;
  String density;
  String meltingPoint;
  String boilingPoint;
};

std::vector<Element> elements;

// Forward Deklarationen
void showElementDetails(Element e);
int findElement(String query);
void drawPeriodicTable();
void periodicTableApp();
void createQRCode(String data, int size, int xOffset, int yOffset);
void qrGeneratorApp();
void calendarApp();
void saveCalendarNotes();
void loadCalendarNotes();
String getNoteForDate(int year, int month, int day);
void addOrEditNote(int year, int month, int day);
void drawCalendar(int year, int month);




Element selectedElement;

// ==================== ALLE 118 ELEMENTE LADEN ====================
void loadElements() {
  elements.clear();

  // PERIODE 1
  elements.push_back({ 1, "H", "Wasserstoff", "Nichtmetalle", 1.008, 1, 1, 1, 1, 0, "Henry Cavendish", 1766, "0.0899 g/L", "-259.1°C", "-252.9°C" });
  elements.push_back({ 2, "He", "Helium", "Edelgase", 4.003, 1, 18, 2, 2, 2, "Pierre Janssen", 1868, "0.1785 g/L", "-272.2°C", "-268.9°C" });

  // PERIODE 2
  elements.push_back({ 3, "Li", "Lithium", "Alkalimetalle", 6.941, 2, 1, 3, 3, 4, "Johan August Arfwedson", 1817, "0.534 g/cm³", "180.5°C", "1342°C" });
  elements.push_back({ 4, "Be", "Beryllium", "Erdalkalimetalle", 9.012, 2, 2, 4, 4, 5, "Louis-Nicolas Vauquelin", 1798, "1.85 g/cm³", "1287°C", "2470°C" });
  elements.push_back({ 5, "B", "Bor", "Metalloide", 10.811, 2, 13, 5, 5, 6, "Joseph Louis Gay-Lussac", 1808, "2.34 g/cm³", "2076°C", "3927°C" });
  elements.push_back({ 6, "C", "Kohlenstoff", "Nichtmetalle", 12.011, 2, 14, 6, 6, 6, "Bekannt seit Altertum", 0, "2.267 g/cm³", "3550°C", "4827°C" });
  elements.push_back({ 7, "N", "Stickstoff", "Nichtmetalle", 14.007, 2, 15, 7, 7, 7, "Daniel Rutherford", 1772, "1.251 g/L", "-210.0°C", "-195.8°C" });
  elements.push_back({ 8, "O", "Sauerstoff", "Nichtmetalle", 15.999, 2, 16, 8, 8, 8, "Carl Wilhelm Scheele", 1774, "1.429 g/L", "-218.8°C", "-183.0°C" });
  elements.push_back({ 9, "F", "Fluor", "Halogene", 18.998, 2, 17, 9, 9, 10, "Henri Moissan", 1886, "1.696 g/L", "-219.6°C", "-188.1°C" });
  elements.push_back({ 10, "Ne", "Neon", "Edelgase", 20.180, 2, 18, 10, 10, 10, "William Ramsay", 1898, "0.900 g/L", "-248.6°C", "-246.1°C" });

  // PERIODE 3
  elements.push_back({ 11, "Na", "Natrium", "Alkalimetalle", 22.990, 3, 1, 11, 11, 12, "Humphry Davy", 1807, "0.968 g/cm³", "97.8°C", "883°C" });
  elements.push_back({ 12, "Mg", "Magnesium", "Erdalkalimetalle", 24.305, 3, 2, 12, 12, 12, "Joseph Black", 1755, "1.738 g/cm³", "650°C", "1091°C" });
  elements.push_back({ 13, "Al", "Aluminium", "Metalle", 26.982, 3, 13, 13, 13, 14, "Hans Christian Ørsted", 1825, "2.70 g/cm³", "660.3°C", "2519°C" });
  elements.push_back({ 14, "Si", "Silicium", "Metalloide", 28.086, 3, 14, 14, 14, 14, "Jöns Jacob Berzelius", 1824, "2.33 g/cm³", "1414°C", "3265°C" });
  elements.push_back({ 15, "P", "Phosphor", "Nichtmetalle", 30.974, 3, 15, 15, 15, 16, "Hennig Brand", 1669, "1.82 g/cm³", "44.2°C", "280°C" });
  elements.push_back({ 16, "S", "Schwefel", "Nichtmetalle", 32.065, 3, 16, 16, 16, 16, "Bekannt seit Altertum", 0, "2.07 g/cm³", "115.2°C", "444.7°C" });
  elements.push_back({ 17, "Cl", "Chlor", "Halogene", 35.453, 3, 17, 17, 17, 18, "Carl Wilhelm Scheele", 1774, "3.214 g/L", "-101.5°C", "-34.0°C" });
  elements.push_back({ 18, "Ar", "Argon", "Edelgase", 39.948, 3, 18, 18, 18, 22, "William Ramsay", 1894, "1.784 g/L", "-189.4°C", "-185.9°C" });

  // PERIODE 4
  elements.push_back({ 19, "K", "Kalium", "Alkalimetalle", 39.098, 4, 1, 19, 19, 20, "Humphry Davy", 1807, "0.856 g/cm³", "63.5°C", "759°C" });
  elements.push_back({ 20, "Ca", "Calcium", "Erdalkalimetalle", 40.078, 4, 2, 20, 20, 20, "Humphry Davy", 1808, "1.55 g/cm³", "842°C", "1484°C" });
  elements.push_back({ 21, "Sc", "Scandium", "Übergangsmetalle", 44.956, 4, 3, 21, 21, 24, "Lars Fredrik Nilson", 1879, "2.985 g/cm³", "1541°C", "2836°C" });
  elements.push_back({ 22, "Ti", "Titan", "Übergangsmetalle", 47.867, 4, 4, 22, 22, 26, "William Gregor", 1791, "4.506 g/cm³", "1668°C", "3287°C" });
  elements.push_back({ 23, "V", "Vanadium", "Übergangsmetalle", 50.942, 4, 5, 23, 23, 28, "Andrés Manuel del Río", 1801, "6.11 g/cm³", "1910°C", "3407°C" });
  elements.push_back({ 24, "Cr", "Chrom", "Übergangsmetalle", 51.996, 4, 6, 24, 24, 28, "Louis-Nicolas Vauquelin", 1797, "7.19 g/cm³", "1907°C", "2671°C" });
  elements.push_back({ 25, "Mn", "Mangan", "Übergangsmetalle", 54.938, 4, 7, 25, 25, 30, "Johann Gottlieb Gahn", 1774, "7.21 g/cm³", "1246°C", "2061°C" });
  elements.push_back({ 26, "Fe", "Eisen", "Übergangsmetalle", 55.845, 4, 8, 26, 26, 30, "Bekannt seit Altertum", 0, "7.874 g/cm³", "1538°C", "2861°C" });
  elements.push_back({ 27, "Co", "Cobalt", "Übergangsmetalle", 58.933, 4, 9, 27, 27, 32, "Georg Brandt", 1739, "8.90 g/cm³", "1495°C", "2927°C" });
  elements.push_back({ 28, "Ni", "Nickel", "Übergangsmetalle", 58.693, 4, 10, 28, 28, 31, "Axel Fredrik Cronstedt", 1751, "8.908 g/cm³", "1455°C", "2913°C" });
  elements.push_back({ 29, "Cu", "Kupfer", "Übergangsmetalle", 63.546, 4, 11, 29, 29, 35, "Bekannt seit Altertum", 0, "8.96 g/cm³", "1085°C", "2562°C" });
  elements.push_back({ 30, "Zn", "Zink", "Metalle", 65.38, 4, 12, 30, 30, 35, "Bekannt seit Altertum", 0, "7.14 g/cm³", "419.5°C", "907°C" });
  elements.push_back({ 31, "Ga", "Gallium", "Metalle", 69.723, 4, 13, 31, 31, 39, "Paul-Émile Lecoq de Boisbaudran", 1875, "5.91 g/cm³", "29.8°C", "2400°C" });
  elements.push_back({ 32, "Ge", "Germanium", "Metalloide", 72.630, 4, 14, 32, 32, 41, "Clemens Winkler", 1886, "5.323 g/cm³", "938.3°C", "2833°C" });
  elements.push_back({ 33, "As", "Arsen", "Metalloide", 74.922, 4, 15, 33, 33, 42, "Bekannt seit Altertum", 0, "5.727 g/cm³", "817°C", "614°C" });
  elements.push_back({ 34, "Se", "Selen", "Nichtmetalle", 78.971, 4, 16, 34, 34, 45, "Jöns Jacob Berzelius", 1817, "4.81 g/cm³", "221°C", "685°C" });
  elements.push_back({ 35, "Br", "Brom", "Halogene", 79.904, 4, 17, 35, 35, 45, "Antoine Jérôme Balard", 1826, "3.102 g/cm³", "-7.2°C", "58.8°C" });
  elements.push_back({ 36, "Kr", "Krypton", "Edelgase", 83.798, 4, 18, 36, 36, 48, "William Ramsay", 1898, "3.749 g/L", "-157.4°C", "-153.4°C" });

  // PERIODE 5
  elements.push_back({ 37, "Rb", "Rubidium", "Alkalimetalle", 85.468, 5, 1, 37, 37, 48, "Robert Bunsen", 1861, "1.532 g/cm³", "39.3°C", "688°C" });
  elements.push_back({ 38, "Sr", "Strontium", "Erdalkalimetalle", 87.62, 5, 2, 38, 38, 50, "William Cruickshank", 1787, "2.64 g/cm³", "777°C", "1382°C" });
  elements.push_back({ 39, "Y", "Yttrium", "Übergangsmetalle", 88.906, 5, 3, 39, 39, 50, "Johan Gadolin", 1794, "4.472 g/cm³", "1526°C", "3336°C" });
  elements.push_back({ 40, "Zr", "Zirconium", "Übergangsmetalle", 91.224, 5, 4, 40, 40, 51, "Martin Heinrich Klaproth", 1789, "6.52 g/cm³", "1855°C", "4409°C" });
  elements.push_back({ 41, "Nb", "Niob", "Übergangsmetalle", 92.906, 5, 5, 41, 41, 52, "Charles Hatchett", 1801, "8.57 g/cm³", "2477°C", "4744°C" });
  elements.push_back({ 42, "Mo", "Molybdän", "Übergangsmetalle", 95.95, 5, 6, 42, 42, 54, "Carl Wilhelm Scheele", 1778, "10.28 g/cm³", "2623°C", "4639°C" });
  elements.push_back({ 43, "Tc", "Technetium", "Übergangsmetalle", 98.00, 5, 7, 43, 43, 55, "Carlo Perrier", 1937, "11.5 g/cm³", "2157°C", "4265°C" });
  elements.push_back({ 44, "Ru", "Ruthenium", "Übergangsmetalle", 101.07, 5, 8, 44, 44, 57, "Karl Ernst Claus", 1844, "12.45 g/cm³", "2334°C", "4150°C" });
  elements.push_back({ 45, "Rh", "Rhodium", "Übergangsmetalle", 102.91, 5, 9, 45, 45, 58, "William Hyde Wollaston", 1803, "12.41 g/cm³", "1964°C", "3695°C" });
  elements.push_back({ 46, "Pd", "Palladium", "Übergangsmetalle", 106.42, 5, 10, 46, 46, 60, "William Hyde Wollaston", 1803, "12.023 g/cm³", "1555°C", "2963°C" });
  elements.push_back({ 47, "Ag", "Silber", "Übergangsmetalle", 107.87, 5, 11, 47, 47, 61, "Bekannt seit Altertum", 0, "10.49 g/cm³", "961.8°C", "2162°C" });
  elements.push_back({ 48, "Cd", "Cadmium", "Übergangsmetalle", 112.41, 5, 12, 48, 48, 64, "Friedrich Stromeyer", 1817, "8.65 g/cm³", "321.1°C", "767°C" });
  elements.push_back({ 49, "In", "Indium", "Metalle", 114.82, 5, 13, 49, 49, 66, "Ferdinand Reich", 1863, "7.31 g/cm³", "156.6°C", "2072°C" });
  elements.push_back({ 50, "Sn", "Zinn", "Metalle", 118.71, 5, 14, 50, 50, 69, "Bekannt seit Altertum", 0, "7.31 g/cm³", "231.9°C", "2602°C" });
  elements.push_back({ 51, "Sb", "Antimon", "Metalloide", 121.76, 5, 15, 51, 51, 71, "Bekannt seit Altertum", 0, "6.68 g/cm³", "630.6°C", "1635°C" });
  elements.push_back({ 52, "Te", "Tellur", "Metalloide", 127.60, 5, 16, 52, 52, 76, "Franz Joseph Müller", 1782, "6.24 g/cm³", "449.5°C", "988°C" });
  elements.push_back({ 53, "I", "Iod", "Halogene", 126.90, 5, 17, 53, 53, 74, "Bernard Courtois", 1811, "4.93 g/cm³", "113.7°C", "184.3°C" });
  elements.push_back({ 54, "Xe", "Xenon", "Edelgase", 131.29, 5, 18, 54, 54, 77, "William Ramsay", 1898, "5.894 g/L", "-111.8°C", "-108.0°C" });

  // PERIODE 6 (Lanthanoide)
  elements.push_back({ 55, "Cs", "Cäsium", "Alkalimetalle", 132.91, 6, 1, 55, 55, 78, "Robert Bunsen", 1860, "1.93 g/cm³", "28.5°C", "671°C" });
  elements.push_back({ 56, "Ba", "Barium", "Erdalkalimetalle", 137.33, 6, 2, 56, 56, 81, "Humphry Davy", 1808, "3.51 g/cm³", "727°C", "1897°C" });
  elements.push_back({ 57, "La", "Lanthan", "Lanthanoide", 138.91, 6, 3, 57, 57, 82, "Carl Gustaf Mosander", 1839, "6.162 g/cm³", "920°C", "3464°C" });
  elements.push_back({ 58, "Ce", "Cer", "Lanthanoide", 140.12, 6, 101, 58, 58, 82, "Martin Heinrich Klaproth", 1803, "6.77 g/cm³", "798°C", "3443°C" });
  elements.push_back({ 59, "Pr", "Praseodym", "Lanthanoide", 140.91, 6, 101, 59, 59, 82, "Carl Gustaf Mosander", 1841, "6.77 g/cm³", "931°C", "3520°C" });
  elements.push_back({ 60, "Nd", "Neodym", "Lanthanoide", 144.24, 6, 101, 60, 60, 84, "Carl Gustaf Mosander", 1841, "7.01 g/cm³", "1024°C", "3074°C" });
  elements.push_back({ 61, "Pm", "Promethium", "Lanthanoide", 145.00, 6, 101, 61, 61, 84, "Jacob A. Marinsky", 1945, "7.26 g/cm³", "1042°C", "3000°C" });
  elements.push_back({ 62, "Sm", "Samarium", "Lanthanoide", 150.36, 6, 101, 62, 62, 88, "Paul-Émile Lecoq de Boisbaudran", 1879, "7.52 g/cm³", "1074°C", "1794°C" });
  elements.push_back({ 63, "Eu", "Europium", "Lanthanoide", 151.96, 6, 101, 63, 63, 89, "Eugène-Anatole Demarçay", 1901, "5.244 g/cm³", "826°C", "1529°C" });
  elements.push_back({ 64, "Gd", "Gadolinium", "Lanthanoide", 157.25, 6, 101, 64, 64, 93, "Jean Charles Galissard de Marignac", 1880, "7.90 g/cm³", "1313°C", "3273°C" });
  elements.push_back({ 65, "Tb", "Terbium", "Lanthanoide", 158.93, 6, 101, 65, 65, 94, "Carl Gustaf Mosander", 1843, "8.23 g/cm³", "1356°C", "3123°C" });
  elements.push_back({ 66, "Dy", "Dysprosium", "Lanthanoide", 162.50, 6, 101, 66, 66, 97, "Paul-Émile Lecoq de Boisbaudran", 1886, "8.54 g/cm³", "1412°C", "2567°C" });
  elements.push_back({ 67, "Ho", "Holmium", "Lanthanoide", 164.93, 6, 101, 67, 67, 98, "Per Teodor Cleve", 1879, "8.79 g/cm³", "1461°C", "2720°C" });
  elements.push_back({ 68, "Er", "Erbium", "Lanthanoide", 167.26, 6, 101, 68, 68, 99, "Carl Gustaf Mosander", 1843, "9.066 g/cm³", "1529°C", "2868°C" });
  elements.push_back({ 69, "Tm", "Thulium", "Lanthanoide", 168.93, 6, 101, 69, 69, 100, "Per Teodor Cleve", 1879, "9.32 g/cm³", "1545°C", "1950°C" });
  elements.push_back({ 70, "Yb", "Ytterbium", "Lanthanoide", 173.05, 6, 101, 70, 70, 103, "Jean Charles Galissard de Marignac", 1878, "6.90 g/cm³", "819°C", "1196°C" });
  elements.push_back({ 71, "Lu", "Lutetium", "Lanthanoide", 174.97, 6, 101, 71, 71, 104, "Georges Urbain", 1907, "9.841 g/cm³", "1663°C", "3402°C" });

  // PERIODE 6 (Fortsetzung)
  elements.push_back({ 72, "Hf", "Hafnium", "Übergangsmetalle", 178.49, 6, 4, 72, 72, 106, "Dirk Coster", 1923, "13.31 g/cm³", "2233°C", "4603°C" });
  elements.push_back({ 73, "Ta", "Tantal", "Übergangsmetalle", 180.95, 6, 5, 73, 73, 108, "Anders Gustaf Ekeberg", 1802, "16.69 g/cm³", "3017°C", "5458°C" });
  elements.push_back({ 74, "W", "Wolfram", "Übergangsmetalle", 183.84, 6, 6, 74, 74, 110, "Carl Wilhelm Scheele", 1781, "19.25 g/cm³", "3422°C", "5555°C" });
  elements.push_back({ 75, "Re", "Rhenium", "Übergangsmetalle", 186.21, 6, 7, 75, 75, 111, "Walter Noddack", 1925, "21.02 g/cm³", "3186°C", "5596°C" });
  elements.push_back({ 76, "Os", "Osmium", "Übergangsmetalle", 190.23, 6, 8, 76, 76, 114, "Smithson Tennant", 1803, "22.59 g/cm³", "3033°C", "5012°C" });
  elements.push_back({ 77, "Ir", "Iridium", "Übergangsmetalle", 192.22, 6, 9, 77, 77, 115, "Smithson Tennant", 1803, "22.56 g/cm³", "2446°C", "4428°C" });
  elements.push_back({ 78, "Pt", "Platin", "Übergangsmetalle", 195.08, 6, 10, 78, 78, 117, "Antonio de Ulloa", 1748, "21.45 g/cm³", "1768°C", "3825°C" });
  elements.push_back({ 79, "Au", "Gold", "Übergangsmetalle", 196.97, 6, 11, 79, 79, 118, "Bekannt seit Altertum", 0, "19.30 g/cm³", "1064°C", "2856°C" });
  elements.push_back({ 80, "Hg", "Quecksilber", "Metalle", 200.59, 6, 12, 80, 80, 121, "Bekannt seit Altertum", 0, "13.534 g/cm³", "-38.8°C", "356.7°C" });
  elements.push_back({ 81, "Tl", "Thallium", "Metalle", 204.38, 6, 13, 81, 81, 123, "William Crookes", 1861, "11.85 g/cm³", "304°C", "1473°C" });
  elements.push_back({ 82, "Pb", "Blei", "Metalle", 207.20, 6, 14, 82, 82, 125, "Bekannt seit Altertum", 0, "11.34 g/cm³", "327.5°C", "1749°C" });
  elements.push_back({ 83, "Bi", "Bismut", "Metalle", 208.98, 6, 15, 83, 83, 126, "Bekannt seit Altertum", 0, "9.78 g/cm³", "271.4°C", "1564°C" });
  elements.push_back({ 84, "Po", "Polonium", "Metalloide", 209.00, 6, 16, 84, 84, 125, "Marie Curie", 1898, "9.196 g/cm³", "254°C", "962°C" });
  elements.push_back({ 85, "At", "Astat", "Halogene", 210.00, 6, 17, 85, 85, 125, "Dale R. Corson", 1940, "7 g/cm³", "302°C", "337°C" });
  elements.push_back({ 86, "Rn", "Radon", "Edelgase", 222.00, 6, 18, 86, 86, 136, "Friedrich Ernst Dorn", 1900, "9.73 g/L", "-71.0°C", "-61.7°C" });

  // PERIODE 7 (Actinoide + Rest)
  elements.push_back({ 87, "Fr", "Francium", "Alkalimetalle", 223.00, 7, 1, 87, 87, 136, "Marguerite Perey", 1939, "1.87 g/cm³", "27°C", "677°C" });
  elements.push_back({ 88, "Ra", "Radium", "Erdalkalimetalle", 226.00, 7, 2, 88, 88, 138, "Marie Curie", 1898, "5.5 g/cm³", "700°C", "1737°C" });
  elements.push_back({ 89, "Ac", "Actinium", "Actinoide", 227.00, 7, 3, 89, 89, 138, "André-Louis Debierne", 1899, "10.07 g/cm³", "1050°C", "3198°C" });
  elements.push_back({ 90, "Th", "Thorium", "Actinoide", 232.04, 7, 102, 90, 90, 142, "Jöns Jacob Berzelius", 1829, "11.72 g/cm³", "1750°C", "4788°C" });
  elements.push_back({ 91, "Pa", "Protactinium", "Actinoide", 231.04, 7, 102, 91, 91, 140, "Kazimierz Fajans", 1913, "15.37 g/cm³", "1572°C", "4027°C" });
  elements.push_back({ 92, "U", "Uran", "Actinoide", 238.03, 7, 102, 92, 92, 146, "Martin Heinrich Klaproth", 1789, "19.05 g/cm³", "1135°C", "4131°C" });
  elements.push_back({ 93, "Np", "Neptunium", "Actinoide", 237.00, 7, 102, 93, 93, 144, "Edwin McMillan", 1940, "19.45 g/cm³", "644°C", "3902°C" });
  elements.push_back({ 94, "Pu", "Plutonium", "Actinoide", 244.00, 7, 102, 94, 94, 150, "Glenn T. Seaborg", 1940, "19.85 g/cm³", "639°C", "3228°C" });
  elements.push_back({ 95, "Am", "Americium", "Actinoide", 243.00, 7, 102, 95, 95, 148, "Glenn T. Seaborg", 1944, "13.69 g/cm³", "1176°C", "2011°C" });
  elements.push_back({ 96, "Cm", "Curium", "Actinoide", 247.00, 7, 102, 96, 96, 151, "Glenn T. Seaborg", 1944, "13.51 g/cm³", "1345°C", "3110°C" });
  elements.push_back({ 97, "Bk", "Berkelium", "Actinoide", 247.00, 7, 102, 97, 97, 150, "Glenn T. Seaborg", 1949, "14.78 g/cm³", "986°C", "2627°C" });
  elements.push_back({ 98, "Cf", "Californium", "Actinoide", 251.00, 7, 102, 98, 98, 153, "Stanley G. Thompson", 1950, "15.1 g/cm³", "900°C", "1470°C" });
  elements.push_back({ 99, "Es", "Einsteinium", "Actinoide", 252.00, 7, 102, 99, 99, 153, "Albert Ghiorso", 1952, "8.84 g/cm³", "860°C", "996°C" });
  elements.push_back({ 100, "Fm", "Fermium", "Actinoide", 257.00, 7, 102, 100, 100, 157, "Albert Ghiorso", 1952, "9.7 g/cm³", "1527°C", "NA" });
  elements.push_back({ 101, "Md", "Mendelevium", "Actinoide", 258.00, 7, 102, 101, 101, 157, "Albert Ghiorso", 1955, "10.3 g/cm³", "827°C", "NA" });
  elements.push_back({ 102, "No", "Nobelium", "Actinoide", 259.00, 7, 102, 102, 102, 157, "Albert Ghiorso", 1958, "9.9 g/cm³", "827°C", "NA" });
  elements.push_back({ 103, "Lr", "Lawrencium", "Actinoide", 262.00, 7, 102, 103, 103, 159, "Albert Ghiorso", 1961, "15.6 g/cm³", "1627°C", "NA" });

  // PERIODE 7 (Superschwere Elemente)
  elements.push_back({ 104, "Rf", "Rutherfordium", "Übergangsmetalle", 267.00, 7, 4, 104, 104, 163, "Albert Ghiorso", 1969, "23.2 g/cm³", "2100°C", "5500°C" });
  elements.push_back({ 105, "Db", "Dubnium", "Übergangsmetalle", 268.00, 7, 5, 105, 105, 163, "Albert Ghiorso", 1970, "29.3 g/cm³", "NA", "NA" });
  elements.push_back({ 106, "Sg", "Seaborgium", "Übergangsmetalle", 271.00, 7, 6, 106, 106, 165, "Albert Ghiorso", 1974, "35.0 g/cm³", "NA", "NA" });
  elements.push_back({ 107, "Bh", "Bohrium", "Übergangsmetalle", 270.00, 7, 7, 107, 107, 163, "Gottfried Münzenberg", 1981, "37.1 g/cm³", "NA", "NA" });
  elements.push_back({ 108, "Hs", "Hassium", "Übergangsmetalle", 277.00, 7, 8, 108, 108, 169, "Gottfried Münzenberg", 1984, "40.7 g/cm³", "NA", "NA" });
  elements.push_back({ 109, "Mt", "Meitnerium", "Übergangsmetalle", 278.00, 7, 9, 109, 109, 169, "Gottfried Münzenberg", 1982, "37.4 g/cm³", "NA", "NA" });
  elements.push_back({ 110, "Ds", "Darmstadtium", "Übergangsmetalle", 281.00, 7, 10, 110, 110, 171, "Sigurd Hofmann", 1994, "34.8 g/cm³", "NA", "NA" });
  elements.push_back({ 111, "Rg", "Roentgenium", "Übergangsmetalle", 282.00, 7, 11, 111, 111, 171, "Sigurd Hofmann", 1994, "28.7 g/cm³", "NA", "NA" });
  elements.push_back({ 112, "Cn", "Copernicium", "Übergangsmetalle", 285.00, 7, 12, 112, 112, 173, "Sigurd Hofmann", 1996, "23.7 g/cm³", "NA", "NA" });
  elements.push_back({ 113, "Nh", "Nihonium", "Metalle", 284.00, 7, 13, 113, 113, 171, "Kosuke Morita", 2004, "16 g/cm³", "NA", "NA" });
  elements.push_back({ 114, "Fl", "Flerovium", "Metalle", 289.00, 7, 14, 114, 114, 175, "Yuri Oganessian", 1999, "14 g/cm³", "NA", "NA" });
  elements.push_back({ 115, "Mc", "Moscovium", "Metalle", 288.00, 7, 15, 115, 115, 173, "Yuri Oganessian", 2004, "13.5 g/cm³", "NA", "NA" });
  elements.push_back({ 116, "Lv", "Livermorium", "Metalle", 293.00, 7, 16, 116, 116, 177, "Yuri Oganessian", 2000, "12.9 g/cm³", "NA", "NA" });
  elements.push_back({ 117, "Ts", "Tenness", "Halogene", 294.00, 7, 17, 117, 117, 177, "Yuri Oganessian", 2010, "11.4 g/cm³", "NA", "NA" });
  elements.push_back({ 118, "Og", "Oganesson", "Edelgase", 294.00, 7, 18, 118, 118, 176, "Yuri Oganessian", 2006, "7.2 g/cm³", "NA", "NA" });
}

void showElementDetails(Element e) {
  tft.fillScreen(BG_COLOR);

  // Header
  tft.fillRect(0, 35, 240, 40, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString(e.symbol + " - " + e.name, 120, 48, 2);

  // ESC Button
  tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
  tft.drawCentreString("ESC", 207, 10, 2);

  tft.setTextSize(1);
  tft.setTextColor(TEXT_COLOR);

  // Element Informationen (2 Spalten)
  int y = 85;
  int col1 = 20, col2 = 130;

  // Linke Spalte
  tft.setCursor(col1, y);
  tft.print("Ordnungszahl:");
  tft.setCursor(col1 + 100, y);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(String(e.number));

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y + 16);
  tft.print("Symbol:");
  tft.setCursor(col1 + 100, y + 16);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(e.symbol);

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y + 32);
  tft.print("Name:");
  tft.setCursor(col1 + 100, y + 32);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(e.name);

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y + 48);
  tft.print("Gruppe:");
  tft.setCursor(col1 + 100, y + 48);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(e.group);

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y + 64);
  tft.print("Periode:");
  tft.setCursor(col1 + 100, y + 64);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(String(e.period));

  // Rechte Spalte
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col2, y);
  tft.print("Atommasse:");
  tft.setCursor(col2 + 85, y);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(String(e.atomicMass, 3) + " u");

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col2, y + 16);
  tft.print("Elektronen:");
  tft.setCursor(col2 + 85, y + 16);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(String(e.electrons));

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col2, y + 32);
  tft.print("Protonen:");
  tft.setCursor(col2 + 85, y + 32);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(String(e.protons));

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col2, y + 48);
  tft.print("Neutronen:");
  tft.setCursor(col2 + 85, y + 48);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(String(e.neutrons));

  // Unterer Bereich
  int y2 = 180;
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y2);
  tft.print("Dichte:");
  tft.setCursor(col1 + 85, y2);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(e.density);

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y2 + 16);
  tft.print("Schmelzpunkt:");
  tft.setCursor(col1 + 85, y2 + 16);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(e.meltingPoint);

  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(col1, y2 + 32);
  tft.print("Siedepunkt:");
  tft.setCursor(col1 + 85, y2 + 32);
  tft.setTextColor(ACCENT_COLOR);
  tft.println(e.boilingPoint);

  // Info Box
  tft.fillRoundRect(15, 240, 210, 55, 5, BUTTON_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(25, 255);
  tft.print("📚 Entdeckung:");
  tft.setCursor(25, 270);
  tft.setTextColor(ACCENT_COLOR);
  if (e.discoveryYear > 0) {
    tft.print(e.discoverer + " (" + String(e.discoveryYear) + ")");
  } else {
    tft.print("Bekannt seit der Antike");
  }

  // Warten auf Touch
  bool waiting = true;
  while (waiting) {
    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) {
        waiting = false;
      }
    }
    delay(50);
  }
}

int findElement(String query) {
  query.toLowerCase();
  for (int i = 0; i < (int)elements.size(); i++) {
    String name = elements[i].name;
    name.toLowerCase();
    String symbol = elements[i].symbol;
    symbol.toLowerCase();

    if (name.indexOf(query) >= 0 || symbol == query || String(elements[i].number) == query) {
      return i;
    }
  }
  return -1;
}

void drawPeriodicTable() {
  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  // Header
  tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("PERIODENSYSTEM", 120, 42, 2);

  // ESC Button
  tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
  tft.drawCentreString("ESC", 207, 10, 2);

  // Suchfeld
  tft.fillRoundRect(10, 70, 180, 28, 4, BUTTON_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.drawCentreString("🔍 SEARCH ELEMENT", 100, 84, 1);

  tft.fillRoundRect(195, 70, 35, 28, 4, SUCCESS_COLOR);
  tft.drawCentreString("GO", 212, 84, 1);

  // Perioden Auswahl Buttons
  const char* periods[] = { "1-2", "3-4", "5-6", "7" };
  for (int i = 0; i < 4; i++) {
    int x = 10 + i * 55;
    tft.fillRoundRect(x, 105, 50, 22, 3, BUTTON_COLOR);
    tft.drawCentreString(periods[i], x + 25, 116, 1);
  }

  // Elemente als Raster
  int cellW = 45;
  int cellH = 32;  // WICHTIG: cellH definieren!
  int startX = 12;
  int startY = 135;
  int perRow = 5;

  for (int i = 0; i < (int)elements.size() && i < 40; i++) {
    int row = i / perRow;
    int col = i % perRow;
    int x = startX + col * cellW;
    int y = startY + row * cellH;

    // Farbgruppe
    uint16_t color;
    if (elements[i].group == "Alkalimetalle") color = TFT_RED;
    else if (elements[i].group == "Erdalkalimetalle") color = TFT_ORANGE;
    else if (elements[i].group == "Übergangsmetalle") color = TFT_BLUE;
    else if (elements[i].group == "Halogene") color = TFT_GREEN;
    else if (elements[i].group == "Edelgase") color = TFT_CYAN;
    else if (elements[i].group == "Nichtmetalle") color = TFT_YELLOW;
    else if (elements[i].group == "Lanthanoide") color = TFT_PURPLE;
    else if (elements[i].group == "Actinoide") color = TFT_MAGENTA;
    else if (elements[i].group == "Metalloide") color = TFT_OLIVE;
    else color = BUTTON_COLOR;

    tft.fillRoundRect(x, y, cellW - 2, cellH - 2, 3, color);
    tft.drawRoundRect(x, y, cellW - 2, cellH - 2, 3, TEXT_COLOR);

    tft.setTextSize(1);
    tft.setTextColor(TEXT_COLOR);
    tft.drawCentreString(elements[i].symbol, x + 20, y + 8, 1);
    tft.setTextSize(1);
    tft.drawCentreString(String(elements[i].number), x + 20, y + 20, 1);
  }

  // Informationstext
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 310);
  tft.print("Tap element for details | 📍 " + String(elements.size()) + " elements");
}

void periodicTableApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  loadElements();
  bool running = true;

  while (running) {
    drawPeriodicTable();

    int tx, ty;
    if (getTouch(tx, ty)) {
      // ESC Button
      if (tx > 180 && ty < 40) {
        running = false;
      }
      // Search Box
      else if (ty > 70 && ty < 98) {
        if (tx < 190) {
          printToConsole(infoPrefix + "Enter element name, symbol or number:", TFT_BLUE);
          String query = getTextInput();
          if (query != "") {
            int idx = findElement(query);
            if (idx >= 0) {
              showElementDetails(elements[idx]);
            } else {
              printToConsole(errorPrefix + "Element not found: " + query, TFT_RED);
              delay(1500);
            }
            tft.fillScreen(BG_COLOR);
            drawPeriodicTable();
          }
        } else if (tx > 195) {
          printToConsole(infoPrefix + "Enter element name, symbol or number:", TFT_BLUE);
          String query = getTextInput();
          if (query != "") {
            int idx = findElement(query);
            if (idx >= 0) {
              showElementDetails(elements[idx]);
            } else {
              printToConsole(errorPrefix + "Element not found: " + query, TFT_RED);
              delay(1500);
            }
          }
        }
      }
      // Elemente Raster
      else if (ty > 132 && ty < 290) {
        int cellW = 45;
        int cellH = 32;
        int startX = 12;
        int startY = 135;
        int perRow = 5;

        int col = (tx - startX) / cellW;
        int row = (ty - startY) / cellH;
        int index = row * perRow + col;

        if (index >= 0 && index < (int)elements.size() && index < 40) {
          showElementDetails(elements[index]);
          tft.fillScreen(BG_COLOR);
          drawPeriodicTable();
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== QR CODE GENERATOR ====================



// ==================== I2C SCANNER ====================


void i2cScanner() {
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  printToConsole(infoPrefix + "I2C Scanner started", TFT_BLUE);
  printToConsole(infoPrefix + "SCL: GPIO39, SDA: GPIO16", TFT_BLUE);
  printToConsole("Scanning I2C bus...", TFT_CYAN);

  int deviceCount = 0;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      printToConsole("  Device found at 0x" + String(address, HEX), TFT_GREEN);
      deviceCount++;
    }
    delay(10);
  }

  if (deviceCount == 0) {
    printToConsole("No I2C devices found!", TFT_RED);
  } else {
    printToConsole(successPrefix + "Found " + String(deviceCount) + " device(s)", TFT_GREEN);
  }

  Wire.end();
}

// ==================== I2C SENDER ====================
void i2cSend(String addressHex, String data) {
  Wire.begin(I2C_SDA, I2C_SCL);

  // Hex-String zu Integer konvertieren
  uint8_t addr = (uint8_t)strtol(addressHex.c_str(), NULL, 16);

  printToConsole(infoPrefix + "Sending to 0x" + String(addr, HEX) + "...", TFT_BLUE);

  Wire.beginTransmission(addr);

  // Daten senden (kann als String oder Hex sein)
  if (data.startsWith("0x")) {
    // Hex-Daten senden
    String hexData = data.substring(2);
    for (int i = 0; i < hexData.length(); i += 2) {
      String byteStr = hexData.substring(i, i + 2);
      uint8_t byteVal = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
      Wire.write(byteVal);
    }
  } else {
    // Text als Bytes senden
    for (int i = 0; i < data.length(); i++) {
      Wire.write((uint8_t)data[i]);
    }
  }

  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    printToConsole(successPrefix + "Data sent successfully!", TFT_GREEN);
  } else {
    switch (error) {
      case 1:
        printToConsole(errorPrefix + "Data too long to fit in transmit buffer", TFT_RED);
        break;
      case 2:
        printToConsole(errorPrefix + "NACK on transmit of address", TFT_RED);
        break;
      case 3:
        printToConsole(errorPrefix + "NACK on transmit of data", TFT_RED);
        break;
      case 4:
        printToConsole(errorPrefix + "Other error", TFT_RED);
        break;
    }
  }
}

// ==================== I2C READER ====================
void i2cRead(String addressHex, int bytes) {
  Wire.begin(I2C_SDA, I2C_SCL);

  uint8_t addr = (uint8_t)strtol(addressHex.c_str(), NULL, 16);

  printToConsole(infoPrefix + "Reading " + String(bytes) + " bytes from 0x" + String(addr, HEX) + "...", TFT_BLUE);

  Wire.requestFrom(addr, bytes);

  String result = "Data: ";
  while (Wire.available()) {
    uint8_t val = Wire.read();
    result += "0x" + String(val, HEX) + " ";

    // Als Zeichen anzeigen wenn druckbar
    if (val >= 32 && val <= 126) {
      result += "('" + String((char)val) + "') ";
    }
  }

  printToConsole(result, TFT_GREEN);
}

// ==================== I2C REGISTER WRITE ====================
void i2cWriteReg(String addressHex, String regHex, String dataHex) {
  Wire.begin(I2C_SDA, I2C_SCL);

  uint8_t addr = (uint8_t)strtol(addressHex.c_str(), NULL, 16);
  uint8_t reg = (uint8_t)strtol(regHex.c_str(), NULL, 16);

  printToConsole(infoPrefix + "Writing to 0x" + String(addr, HEX) + " register 0x" + String(reg, HEX), TFT_BLUE);

  Wire.beginTransmission(addr);
  Wire.write(reg);

  // Daten senden
  if (dataHex.startsWith("0x")) {
    dataHex = dataHex.substring(2);
  }

  for (int i = 0; i < dataHex.length(); i += 2) {
    String byteStr = dataHex.substring(i, i + 2);
    uint8_t byteVal = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    Wire.write(byteVal);
  }

  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    printToConsole(successPrefix + "Register write successful!", TFT_GREEN);
  } else {
    printToConsole(errorPrefix + "Write failed! Error: " + String(error), TFT_RED);
  }
}

// ==================== I2C REGISTER READ ====================
void i2cReadReg(String addressHex, String regHex, int bytes) {
  Wire.begin(I2C_SDA, I2C_SCL);

  uint8_t addr = (uint8_t)strtol(addressHex.c_str(), NULL, 16);
  uint8_t reg = (uint8_t)strtol(regHex.c_str(), NULL, 16);

  printToConsole(infoPrefix + "Reading register 0x" + String(reg, HEX) + " from 0x" + String(addr, HEX), TFT_BLUE);

  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(addr, bytes);

  String result = "Register data: ";
  while (Wire.available()) {
    uint8_t val = Wire.read();
    result += "0x" + String(val, HEX) + " ";
    if (val >= 32 && val <= 126) {
      result += "('" + String((char)val) + "') ";
    }
  }

  printToConsole(result, TFT_GREEN);
}

// ==================== I2C GUI TOOL ====================
void i2cTool() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillScreen(BG_COLOR);

  bool running = true;
  int selected = 0;
  String targetAddr = "3C";  // Standard (SSD1306 OLED)

  const char* menuOptions[] = {
    "I2C SCAN",
    "SEND TEXT",
    "SEND HEX",
    "READ BYTES",
    "REGISTER WRITE",
    "REGISTER READ",
    "BACK"
  };
  int numOptions = 7;

  while (running) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Titel
    tft.setTextSize(2);
    tft.drawCentreString("I2C TOOL", 120, 45, 2);
    tft.setTextSize(1);

    // Aktuelle Adresse anzeigen
    tft.setTextColor(ACCENT_COLOR);
    tft.drawCentreString("Current Addr: 0x" + targetAddr, 120, 70, 1);

    // Zieladresse ändern Button
    tft.fillRoundRect(10, 85, 220, 25, 4, BUTTON_COLOR);
    tft.drawCentreString("CHANGE ADDRESS", 120, 97, 1);

    // Menü
    for (int i = 0; i < numOptions; i++) {
      int y = 125 + i * 28;
      if (i == selected) {
        tft.fillRoundRect(10, y - 2, 220, 24, 4, ACCENT_COLOR);
        tft.setTextColor(TFT_WHITE);
      } else {
        tft.fillRoundRect(10, y - 2, 220, 24, 4, BUTTON_COLOR);
        tft.setTextColor(TEXT_COLOR);
      }
      tft.drawCentreString(menuOptions[i], 120, y + 5, 1);
    }

    // ESC Button
    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) {
        running = false;
      } else if (ty > 85 && ty < 110) {
        // Adresse ändern
        printToConsole(infoPrefix + "Enter I2C address (hex, e.g., 3C):", TFT_BLUE);
        String newAddr = getTextInput();
        if (newAddr.length() > 0 && newAddr.length() <= 2) {
          targetAddr = newAddr;
          printToConsole(successPrefix + "Address set to 0x" + targetAddr, TFT_GREEN);
        }
        playSysSound(0);
      } else if (ty > 120 && ty < 310) {
        int idx = (ty - 120) / 28;
        if (idx >= 0 && idx < numOptions) {
          selected = idx;
          playSysSound(0);

          // Variablen außerhalb des switch deklarieren
          String textData = "";
          String hexData = "";
          String numBytes = "";
          String regAddr = "";
          String regData = "";
          String regRead = "";
          String numBytesReg = "";

          switch (selected) {
            case 0:  // I2C SCAN
              i2cScanner();
              delay(2000);
              break;

            case 1:  // SEND TEXT
              printToConsole(infoPrefix + "Enter text to send:", TFT_BLUE);
              textData = getTextInput();
              if (textData != "") {
                i2cSend(targetAddr, textData);
              }
              delay(1500);
              break;

            case 2:  // SEND HEX
              printToConsole(infoPrefix + "Enter hex data (e.g., 48656C6C6F):", TFT_BLUE);
              hexData = getTextInput();
              if (hexData != "") {
                i2cSend(targetAddr, "0x" + hexData);
              }
              delay(1500);
              break;

            case 3:  // READ BYTES
              printToConsole(infoPrefix + "Number of bytes to read:", TFT_BLUE);
              numBytes = getTextInput();
              if (numBytes != "") {
                i2cRead(targetAddr, numBytes.toInt());
              }
              delay(1500);
              break;

            case 4:  // REGISTER WRITE
              printToConsole(infoPrefix + "Register address (hex):", TFT_BLUE);
              regAddr = getTextInput();
              printToConsole(infoPrefix + "Data to write (hex):", TFT_BLUE);
              regData = getTextInput();
              if (regAddr != "" && regData != "") {
                i2cWriteReg(targetAddr, regAddr, regData);
              }
              delay(1500);
              break;

            case 5:  // REGISTER READ
              printToConsole(infoPrefix + "Register address (hex):", TFT_BLUE);
              regRead = getTextInput();
              printToConsole(infoPrefix + "Number of bytes:", TFT_BLUE);
              numBytesReg = getTextInput();
              if (regRead != "" && numBytesReg != "") {
                i2cReadReg(targetAddr, regRead, numBytesReg.toInt());
              }
              delay(1500);
              break;

            case 6:  // BACK
              running = false;
              break;
          }

          // Neuzeichnen nach der Operation
          tft.fillScreen(BG_COLOR);
          drawScrollButtons();
          drawKeyboard();
          refreshTerminal();
          updateInputLine(true);
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}
// ==================== SOUND SYSTEM ====================
void playSysSound(int type) {
  if (!soundEnabled) return;
  switch (type) {
    case 0: tone(SOUND_PIN, 2000, 10); break;
    case 1:
      tone(SOUND_PIN, 1500, 50);
      delay(60);
      tone(SOUND_PIN, 2500, 50);
      break;
    case 2:
      for (int i = 500; i < 2000; i += 100) {
        tone(SOUND_PIN, i, 20);
        delay(20);
      }
      break;
    case 3: tone(SOUND_PIN, 400, 200); break;
    case 4:
      for (int i = 0; i < 3; i++) {
        tone(SOUND_PIN, 1000, 50);
        delay(50);
      }
      break;
  }
}

// ==================== UI HELPERS ====================
void printToConsole(String text, uint16_t color) {
  if (color != -1) tft.setTextColor(color);
  else tft.setTextColor(TEXT_COLOR);

  terminalHistory.push_back(text);
  if (terminalHistory.size() > MAX_HISTORY) terminalHistory.erase(terminalHistory.begin());
  scrollOffset = 0;
  refreshTerminal();
}

void updateInputLine(bool force) {
  tft.fillRect(0, 140, 240, 28, BG_COLOR);
  tft.drawFastHLine(0, 138, 240, TEXT_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_COLOR);
  String disp = currentInput;
  if (disp.length() > 17) disp = disp.substring(disp.length() - 17);
  tft.setCursor(5, 145);
  tft.print(promptPrefix + disp);
  if (force || cursorVisible) tft.print("_");
}

void refreshTerminal() {
  tft.fillRect(0, 35, 210, 103, BG_COLOR);
  int startIdx = terminalHistory.size() - VISIBLE_LINES - scrollOffset;
  if (startIdx < 0) startIdx = 0;

  int y = 40;
  int endIdx = startIdx + VISIBLE_LINES;
  if (endIdx > (int)terminalHistory.size()) endIdx = terminalHistory.size();

  tft.setTextSize(1);
  tft.setTextColor(TEXT_COLOR);
  for (int i = startIdx; i < endIdx; i++) {
    tft.setCursor(10, y);
    tft.println(terminalHistory[i]);
    y += 11;
  }
}

void drawScrollButtons() {
  tft.fillRoundRect(215, 40, 22, 45, 4, BUTTON_COLOR);
  tft.fillRoundRect(215, 90, 22, 45, 4, BUTTON_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);
  tft.setCursor(220, 55);
  tft.print("A");
  tft.setCursor(220, 105);
  tft.print("V");
  tft.setTextSize(1);
}

bool getTouch(int& x, int& y) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = constrain(map(p.x, 200, 3800, 0, 239), 0, 239);
    y = constrain(map(p.y, 200, 3800, 0, 319), 0, 319);
    return true;
  }
  return false;
}

// ==================== THEME MANAGER ====================
void applyTheme() {
  if (darkMode) {
    BG_COLOR = TFT_BLACK;
    TEXT_COLOR = TFT_WHITE;
    ACCENT_COLOR = TFT_DARKGREEN;
    BUTTON_COLOR = TFT_DARKGREY;
    WARNING_COLOR = TFT_RED;
    SUCCESS_COLOR = TFT_GREEN;
  } else {
    BG_COLOR = TFT_WHITE;
    TEXT_COLOR = TFT_BLACK;
    ACCENT_COLOR = TFT_SKYBLUE;
    BUTTON_COLOR = TFT_LIGHTGREY;
    WARNING_COLOR = TFT_RED;
    SUCCESS_COLOR = TFT_GREEN;
  }
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== KEYBOARD ====================
void drawKeyboard() {
  tft.fillRect(0, 170, 240, 150, BG_COLOR);
  int tW = 24, tH = 35, startY = 170;

  for (int i = 0; i < 10; i++) {
    tft.fillRoundRect(i * tW, startY, tW - 1, tH - 1, 4, BUTTON_COLOR);
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);
    tft.drawCentreString(String(keys[kbMode][0][i]), i * tW + (tW / 2), startY + (tH / 2) - 7, 1);
  }

  for (int i = 0; i < 9; i++) {
    tft.fillRoundRect(12 + i * tW, startY + tH + 5, tW - 1, tH - 1, 4, BUTTON_COLOR);
    tft.drawCentreString(String(keys[kbMode][1][i]), 12 + i * tW + (tW / 2), startY + tH + 5 + (tH / 2) - 7, 1);
  }

  tft.fillRoundRect(0, startY + 2 * (tH + 5), 35, tH, 4, (kbMode == 1) ? ACCENT_COLOR : BUTTON_COLOR);
  tft.drawCentreString((kbMode == 1) ? "^" : "abc", 17, startY + 2 * (tH + 5) + (tH / 2) - 7, 1);

  for (int i = 0; i < 7; i++) {
    tft.fillRoundRect(38 + i * 24, startY + 2 * (tH + 5), tW - 1, tH - 1, 4, BUTTON_COLOR);
    tft.drawCentreString(String(keys[kbMode][2][i]), 38 + i * 24 + (tW / 2), startY + 2 * (tH + 5) + (tH / 2) - 7, 1);
  }

  tft.fillRoundRect(206, startY + 2 * (tH + 5), 34, tH, 4, BUTTON_COLOR);
  tft.drawCentreString("<", 223, startY + 2 * (tH + 5) + (tH / 2) - 7, 1);

  tft.fillRoundRect(2, startY + 3 * (tH + 5), 55, tH, 4, (kbMode == 2) ? ACCENT_COLOR : BUTTON_COLOR);
  tft.drawCentreString((kbMode == 2) ? "ABC" : "123", 30, startY + 3 * (tH + 5) + (tH / 2) - 7, 1);

  tft.fillRoundRect(62, startY + 3 * (tH + 5), 55, tH, 4, (kbMode == 3) ? ACCENT_COLOR : BUTTON_COLOR);
  tft.drawCentreString("§$%", 89, startY + 3 * (tH + 5) + (tH / 2) - 7, 1);

  tft.fillRoundRect(122, startY + 3 * (tH + 5), 45, tH, 4, BUTTON_COLOR);
  tft.drawCentreString(" ", 144, startY + 3 * (tH + 5) + (tH / 2) - 7, 1);

  tft.fillRoundRect(172, startY + 3 * (tH + 5), 65, tH, 4, SUCCESS_COLOR);
  tft.drawCentreString("OK", 204, startY + 3 * (tH + 5) + (tH / 2) - 7, 1);
}

String handleKeyboardInput() {
  int tx, ty;
  if (!getTouch(tx, ty)) return "";

  if (tx > 210 && ty > 40 && ty < 135) {
    if (ty < 85) {
      if (scrollOffset < (int)terminalHistory.size() - VISIBLE_LINES) scrollOffset++;
    } else {
      if (scrollOffset > 0) scrollOffset--;
    }
    refreshTerminal();
    delay(100);
    return "";
  }

  if (ty < 40 && tx > 180) {
    playSysSound(3);
    return "ESC_SIGNAL";
  }

  if (ty < 170) return "";

  playSysSound(0);
  String res = "";

  if (ty < 205) {
    char c = keys[kbMode][0][tx / 24];
    if (c != '\0' && tx / 24 < 10) currentInput += c;
    if (kbMode == 1) {
      kbMode = 0;
      drawKeyboard();
    }
  } else if (ty < 245) {
    int idx = (tx - 12) / 24;
    if (idx >= 0 && idx < 9) {
      char c = keys[kbMode][1][idx];
      if (c != '\0') currentInput += c;
      if (kbMode == 1) {
        kbMode = 0;
        drawKeyboard();
      }
    }
  } else if (ty < 285) {
    if (tx < 38) {
      kbMode = (kbMode == 0) ? 1 : 0;
      drawKeyboard();
    } else if (tx > 205) {
      if (currentInput.length() > 0) currentInput.remove(currentInput.length() - 1);
    } else {
      int idx = (tx - 38) / 24;
      if (idx >= 0 && idx < 7) {
        char c = keys[kbMode][2][idx];
        if (c != '\0') currentInput += c;
        if (kbMode == 1) {
          kbMode = 0;
          drawKeyboard();
        }
      }
    }
  } else {
    if (tx < 55) {
      kbMode = (kbMode == 2) ? 0 : 2;
      drawKeyboard();
    } else if (tx >= 55 && tx < 115) {
      kbMode = (kbMode == 3) ? 0 : 3;
      drawKeyboard();
    } else if (tx >= 115 && tx < 165) {
      currentInput += " ";
    } else if (tx >= 165) {
      res = currentInput;
      currentInput = "";
      playSysSound(1);
    }
  }
  updateInputLine(true);
  delay(150);
  return res;
}

String getTextInput() {
  String input = "";
  bool typing = true;
  while (typing) {
    String res = handleKeyboardInput();
    if (res == "ESC_SIGNAL") {
      typing = false;
      input = "";
    } else if (res != "") {
      input = res;
      typing = false;
    }
    delay(10);
  }
  return input;
}

// ==================== FILE MANAGER ====================
void listDirectory(String path, std::vector<String>& files, std::vector<String>& dirs) {
  files.clear();
  dirs.clear();

  File root = SD.open(path);
  if (!root) return;
  if (!root.isDirectory()) return;

  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    if (file.isDirectory()) {
      dirs.push_back(name + "/");
    } else {
      files.push_back(name);
    }
    file = root.openNextFile();
  }
  root.close();
}

void deleteFileOrDir(String path) {
  File f = SD.open(path);
  if (!f) {
    printToConsole(errorPrefix + "Not found: " + path, TFT_RED);
    return;
  }

  if (f.isDirectory()) {
    f.close();
    if (SD.rmdir(path)) {
      printToConsole(successPrefix + "Deleted directory: " + path, TFT_GREEN);
    } else {
      printToConsole(errorPrefix + "Cannot delete (not empty?): " + path, TFT_RED);
    }
  } else {
    f.close();
    if (SD.remove(path)) {
      printToConsole(successPrefix + "Deleted file: " + path, TFT_GREEN);
    } else {
      printToConsole(errorPrefix + "Delete failed: " + path, TFT_RED);
    }
  }
}

void copyFileItem(String src, String dst) {
  File srcFile = SD.open(src, FILE_READ);
  if (!srcFile) {
    printToConsole(errorPrefix + "Source not found", TFT_RED);
    return;
  }

  File dstFile = SD.open(dst, FILE_WRITE);
  if (!dstFile) {
    printToConsole(errorPrefix + "Cannot create destination", TFT_RED);
    srcFile.close();
    return;
  }

  uint8_t buffer[512];
  size_t bytes;
  while ((bytes = srcFile.read(buffer, sizeof(buffer))) > 0) {
    dstFile.write(buffer, bytes);
  }
  srcFile.close();
  dstFile.close();
  printToConsole(successPrefix + "Copied: " + src + " -> " + dst, TFT_GREEN);
}

void moveFileItem(String src, String dst) {
  if (SD.rename(src, dst)) {
    printToConsole(successPrefix + "Moved: " + src + " -> " + dst, TFT_GREEN);
  } else {
    printToConsole(errorPrefix + "Move failed", TFT_RED);
  }
}

void fileManager() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  String currentPath = "/";
  std::vector<String> files, dirs;
  int selected = 0;
  int listStart = 0;
  int maxVisible = 7;
  bool running = true;

  while (running) {
    listDirectory(currentPath, files, dirs);

    tft.fillRect(0, 35, 240, 135, BG_COLOR);

    tft.fillRoundRect(5, 38, 230, 18, 3, ACCENT_COLOR);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(8, 43);
    String shortPath = currentPath;
    if (shortPath.length() > 28) shortPath = "..." + shortPath.substring(shortPath.length() - 25);
    tft.print(shortPath);

    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(1);

    int yPos = 60;
    int itemCount = dirs.size() + files.size();

    for (int i = listStart; i < min(listStart + maxVisible, itemCount); i++) {
      String item;
      if (i < (int)dirs.size()) {
        item = "[DIR] " + dirs[i];
      } else {
        item = "[FILE] " + files[i - dirs.size()];
      }

      if (i == selected) {
        tft.fillRoundRect(5, yPos - 2, 230, 16, 3, ACCENT_COLOR);
        tft.setTextColor(TFT_WHITE);
      } else {
        tft.setTextColor(TEXT_COLOR);
      }

      tft.setCursor(10, yPos);
      tft.print(item.substring(0, 30));
      yPos += 18;
    }

    tft.fillRoundRect(5, 195, 55, 22, 3, SUCCESS_COLOR);
    tft.drawCentreString("OPEN", 32, 200, 1);
    tft.fillRoundRect(65, 195, 55, 22, 3, WARNING_COLOR);
    tft.drawCentreString("DEL", 92, 200, 1);
    tft.fillRoundRect(125, 195, 55, 22, 3, TFT_BLUE);
    tft.drawCentreString("NEW", 152, 200, 1);
    tft.fillRoundRect(185, 195, 50, 22, 3, TFT_PURPLE);
    tft.drawCentreString("DIR", 210, 200, 1);

    tft.fillRoundRect(5, 222, 55, 22, 3, TFT_CYAN);
    tft.drawCentreString("COPY", 32, 227, 1);
    tft.fillRoundRect(65, 222, 55, 22, 3, TFT_YELLOW);
    tft.drawCentreString("MOVE", 92, 227, 1);
    tft.fillRoundRect(125, 222, 55, 22, 3, ACCENT_COLOR);
    tft.drawCentreString("REN", 152, 227, 1);
    tft.fillRoundRect(185, 222, 50, 22, 3, WARNING_COLOR);
    tft.drawCentreString("ESC", 210, 227, 1);

    tft.fillTriangle(215, 60, 235, 50, 235, 70, BUTTON_COLOR);
    tft.fillTriangle(215, 85, 235, 75, 235, 95, BUTTON_COLOR);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 185 && ty > 222 && ty < 244) {
        running = false;
      } else if (tx > 210 && ty > 50 && ty < 70 && listStart > 0) {
        listStart--;
        if (selected >= listStart + maxVisible) selected = listStart + maxVisible - 1;
        playSysSound(0);
      } else if (tx > 210 && ty > 75 && ty < 95 && listStart < itemCount - maxVisible) {
        listStart++;
        if (selected < listStart) selected = listStart;
        playSysSound(0);
      } else if (ty > 55 && ty < 190) {
        int idx = listStart + ((ty - 55) / 18);
        if (idx >= 0 && idx < itemCount) {
          selected = idx;
          playSysSound(0);
        }
      } else if (ty > 195 && ty < 217) {
        if (tx < 60 && selected < itemCount) {
          if (selected < (int)dirs.size()) {
            if (currentPath == "/") currentPath = "/" + dirs[selected];
            else currentPath = currentPath + "/" + dirs[selected];
            currentPath.replace("//", "/");
            selected = 0;
            listStart = 0;
          } else {
            String filename = files[selected - dirs.size()];
            String fullPath = currentPath + "/" + filename;
            File f = SD.open(fullPath, FILE_READ);
            if (f) {
              printToConsole(infoPrefix + "File: " + filename, TFT_BLUE);
              String content = "";
              while (f.available()) {
                char c = f.read();
                content += c;
                if (content.length() > 400) {
                  printToConsole(content);
                  content = "";
                  delay(10);
                }
              }
              if (content.length() > 0) printToConsole(content);
              f.close();
            }
          }
        } else if (tx >= 60 && tx < 120 && selected < itemCount) {
          if (selected < (int)dirs.size()) {
            deleteFileOrDir(currentPath + "/" + dirs[selected]);
          } else {
            deleteFileOrDir(currentPath + "/" + files[selected - dirs.size()]);
          }
        } else if (tx >= 120 && tx < 180) {
          printToConsole(infoPrefix + "New filename:", TFT_BLUE);
          String newFile = getTextInput();
          if (newFile != "") {
            File f = SD.open(currentPath + "/" + newFile, FILE_WRITE);
            if (f) {
              f.close();
              printToConsole(successPrefix + "File created", TFT_GREEN);
            }
          }
        } else if (tx >= 180 && selected < itemCount) {
          printToConsole(infoPrefix + "Directory name:", TFT_BLUE);
          String newDir = getTextInput();
          if (newDir != "") {
            if (SD.mkdir(currentPath + "/" + newDir)) {
              printToConsole(successPrefix + "Directory created", TFT_GREEN);
            } else {
              printToConsole(errorPrefix + "Creation failed", TFT_RED);
            }
          }
        }
      } else if (ty > 222 && ty < 244) {
        if (tx < 60 && selected < itemCount) {
          String selectedPath;
          if (selected < (int)dirs.size()) {
            selectedPath = currentPath + "/" + dirs[selected];
          } else {
            selectedPath = currentPath + "/" + files[selected - dirs.size()];
          }
          printToConsole(infoPrefix + "Copy to path:", TFT_BLUE);
          String destPath = getTextInput();
          if (destPath != "") {
            String destFull = destPath + "/" + selectedPath.substring(selectedPath.lastIndexOf('/') + 1);
            copyFileItem(selectedPath, destFull);
          }
        } else if (tx >= 60 && tx < 120 && selected < itemCount) {
          String selectedPath;
          if (selected < (int)dirs.size()) {
            selectedPath = currentPath + "/" + dirs[selected];
          } else {
            selectedPath = currentPath + "/" + files[selected - dirs.size()];
          }
          printToConsole(infoPrefix + "Move to path:", TFT_BLUE);
          String destPath = getTextInput();
          if (destPath != "") {
            String destFull = destPath + "/" + selectedPath.substring(selectedPath.lastIndexOf('/') + 1);
            moveFileItem(selectedPath, destFull);
          }
        } else if (tx >= 120 && tx < 180 && selected < itemCount) {
          String oldName;
          if (selected < (int)dirs.size()) {
            oldName = dirs[selected];
          } else {
            oldName = files[selected - dirs.size()];
          }
          printToConsole(infoPrefix + "New name for " + oldName + ":", TFT_BLUE);
          String newName = getTextInput();
          if (newName != "") {
            moveFileItem(currentPath + "/" + oldName, currentPath + "/" + newName);
          }
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== WIFI MANAGER ====================
void wifiManager() {
  printToConsole(infoPrefix + "WiFi Manager", TFT_BLUE);
  printToConsole(infoPrefix + "Enter SSID:", TFT_BLUE);
  String ssid = getTextInput();
  if (ssid != "") {
    printToConsole(infoPrefix + "Enter Password:", TFT_BLUE);
    String password = getTextInput();
    printToConsole(infoPrefix + "Connecting to " + ssid + "...", TFT_BLUE);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(1000);
      attempts++;
      printToConsole(".", TFT_GREEN);
    }

    if (WiFi.status() == WL_CONNECTED) {
      printToConsole(successPrefix + "Connected! IP: " + WiFi.localIP().toString(), TFT_GREEN);
      wifiSSID = ssid;
      wifiPassword = password;
      EEPROM.put(150, wifiSSID);
      EEPROM.put(200, wifiPassword);
      EEPROM.commit();
      initNTP();
    } else {
      printToConsole(errorPrefix + "Connection failed!", TFT_RED);
    }
  }
}

// ==================== SYSTEM INFO ====================
void sysInfo() {
  printToConsole(infoPrefix + "=== SYSTEM INFO ===", TFT_BLUE);
  printToConsole(cmdPrefix + "CPU: ESP32 @ 240MHz");
  printToConsole(cmdPrefix + "RAM: " + String(ESP.getFreeHeap() / 1024) + " KB free");
  printToConsole(cmdPrefix + "Flash: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  printToConsole(cmdPrefix + "Theme: " + String(darkMode ? "Dark" : "Light"));
  printToConsole(cmdPrefix + "Sound: " + String(soundEnabled ? "ON" : "OFF"));
}

void storageInfo() {
  if (!SD.begin(SD_CS)) {
    printToConsole(errorPrefix + "SD Card not found!", TFT_RED);
    return;
  }
  printToConsole(infoPrefix + "SD Card: " + String(SD.cardSize() / 1024 / 1024) + " MB total", TFT_GREEN);
}

// ==================== CALCULATOR ====================
String evaluateExpression(String expr) {
  int a, b;
  char op;
  if (sscanf(expr.c_str(), "%d%c%d", &a, &op, &b) == 3) {
    switch (op) {
      case '+': return String(a + b);
      case '-': return String(a - b);
      case '*': return String(a * b);
      case '/':
        if (b != 0) return String(a / b);
        else return "Error";
      default: return "Error";
    }
  }
  return "Error";
}

String evaluateScientific(String expr) {
  if (expr.startsWith("sqrt(") && expr.endsWith(")")) {
    double val = expr.substring(5, expr.length() - 1).toDouble();
    return String(sqrt(val));
  } else if (expr.startsWith("cbrt(") && expr.endsWith(")")) {
    double val = expr.substring(5, expr.length() - 1).toDouble();
    return String(cbrt(val));
  } else if (expr.startsWith("sin(") && expr.endsWith(")")) {
    double val = expr.substring(4, expr.length() - 1).toDouble();
    return String(sin(val * PI / 180.0));
  } else if (expr.startsWith("cos(") && expr.endsWith(")")) {
    double val = expr.substring(4, expr.length() - 1).toDouble();
    return String(cos(val * PI / 180.0));
  } else if (expr.startsWith("tan(") && expr.endsWith(")")) {
    double val = expr.substring(4, expr.length() - 1).toDouble();
    return String(tan(val * PI / 180.0));
  } else if (expr.startsWith("log10(") && expr.endsWith(")")) {
    double val = expr.substring(6, expr.length() - 1).toDouble();
    return String(log10(val));
  } else if (expr.startsWith("ln(") && expr.endsWith(")")) {
    double val = expr.substring(3, expr.length() - 1).toDouble();
    return String(log(val));
  } else if (expr.startsWith("exp(") && expr.endsWith(")")) {
    double val = expr.substring(4, expr.length() - 1).toDouble();
    return String(exp(val));
  } else if (expr.startsWith("abs(") && expr.endsWith(")")) {
    double val = expr.substring(4, expr.length() - 1).toDouble();
    return String(abs(val));
  } else if (expr.indexOf('^') > 0) {
    int pos = expr.indexOf('^');
    double base = expr.substring(0, pos).toDouble();
    double expo = expr.substring(pos + 1).toDouble();
    return String(pow(base, expo));
  }
  return evaluateExpression(expr);
}

String evaluateProgrammer(String expr) {
  expr.replace(" ", "");
  if (expr.indexOf('&') > 0) {
    int pos = expr.indexOf('&');
    long l = parseNumber(expr.substring(0, pos));
    long r = parseNumber(expr.substring(pos + 1));
    return String(l & r);
  } else if (expr.indexOf('|') > 0) {
    int pos = expr.indexOf('|');
    long l = parseNumber(expr.substring(0, pos));
    long r = parseNumber(expr.substring(pos + 1));
    return String(l | r);
  } else if (expr.indexOf('^') > 0 && !expr.startsWith("0x")) {
    int pos = expr.indexOf('^');
    long l = parseNumber(expr.substring(0, pos));
    long r = parseNumber(expr.substring(pos + 1));
    return String(l ^ r);
  } else if (expr.indexOf("<<") > 0) {
    int pos = expr.indexOf("<<");
    long l = parseNumber(expr.substring(0, pos));
    long r = parseNumber(expr.substring(pos + 2));
    return String(l << r);
  } else if (expr.indexOf(">>") > 0) {
    int pos = expr.indexOf(">>");
    long l = parseNumber(expr.substring(0, pos));
    long r = parseNumber(expr.substring(pos + 2));
    return String(l >> r);
  } else {
    return String(parseNumber(expr));
  }
}

long parseNumber(String num) {
  num.trim();
  if (num.startsWith("0b") || num.startsWith("0B")) {
    return strtol(num.substring(2).c_str(), NULL, 2);
  } else if (num.startsWith("0x") || num.startsWith("0X")) {
    return strtol(num.substring(2).c_str(), NULL, 16);
  } else {
    return num.toInt();
  }
}

void calculator() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);
  String expression = "";
  String result = "";
  int calcMode = 0;
  double memory = 0;

  const char* sciButtons[6][5] = {
    { "sin", "cos", "tan", "log", "ln" },
    { "asin", "acos", "atan", "sqrt", "cbrt" },
    { "x²", "x³", "xʸ", "eˣ", "10ˣ" },
    { "π", "e", "!", "1/x", "|x|" },
    { "M+", "M-", "MR", "MC", "x√y" },
    { "(", ")", "EXP", "MOD", "=" }
  };

  bool running = true;
  int lastMode = 0;

  while (running) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    tft.fillRoundRect(10, 40, 70, 25, 4, calcMode == 0 ? SUCCESS_COLOR : BUTTON_COLOR);
    tft.drawCentreString("STD", 45, 48, 1);
    tft.fillRoundRect(85, 40, 70, 25, 4, calcMode == 1 ? SUCCESS_COLOR : BUTTON_COLOR);
    tft.drawCentreString("SCI", 120, 48, 1);
    tft.fillRoundRect(160, 40, 70, 25, 4, calcMode == 2 ? SUCCESS_COLOR : BUTTON_COLOR);
    tft.drawCentreString("PROG", 195, 48, 1);

    tft.fillRect(10, 70, 220, 50, BG_COLOR);
    tft.drawRect(10, 70, 220, 50, TEXT_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 80);
    tft.setTextColor(TFT_BLUE);
    tft.print("Expr: ");
    tft.setTextColor(TEXT_COLOR);
    String dispExpr = expression;
    if (dispExpr.length() > 28) dispExpr = "..." + dispExpr.substring(dispExpr.length() - 25);
    tft.println(dispExpr);
    tft.setCursor(15, 100);
    tft.setTextColor(TFT_RED);
    tft.print("Ans: ");
    tft.setTextColor(SUCCESS_COLOR);
    tft.print(result);

    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    if (calcMode == 0) {
      const char* stdButtons[4][4] = {
        { "7", "8", "9", "+" },
        { "4", "5", "6", "-" },
        { "1", "2", "3", "*" },
        { "C", "0", "=", "/" }
      };
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          tft.fillRoundRect(10 + j * 55, 130 + i * 35, 50, 30, 4, BUTTON_COLOR);
          tft.drawCentreString(stdButtons[i][j], 35 + j * 55, 140 + i * 35, 2);
        }
      }
    } else if (calcMode == 1) {
      for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 5; j++) {
          int x = 5 + j * 46;
          int y = 130 + i * 22;
          tft.fillRoundRect(x, y, 44, 20, 3, BUTTON_COLOR);
          tft.setTextSize(1);
          tft.drawCentreString(sciButtons[i][j], x + 22, y + 11, 1);
        }
      }
    } else if (calcMode == 2) {
      const char* progButtons[5][5] = {
        { "AND", "OR", "XOR", "NOT", "<<" },
        { ">>", "BIN", "HEX", "DEC", "CLR" },
        { "0b", "0x", "~", "&", "|" },
        { "=", "(", ")", "A", "B" },
        { "C", "D", "E", "F", "BACK" }
      };
      for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
          int x = 5 + j * 46;
          int y = 130 + i * 22;
          tft.fillRoundRect(x, y, 44, 20, 3, BUTTON_COLOR);
          tft.setTextSize(1);
          tft.drawCentreString(progButtons[i][j], x + 22, y + 11, 1);
        }
      }
    }

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) running = false;
      else if (ty > 40 && ty < 65) {
        if (tx > 10 && tx < 80) calcMode = 0;
        else if (tx > 85 && tx < 155) calcMode = 1;
        else if (tx > 160 && tx < 230) calcMode = 2;
        if (lastMode != calcMode) {
          expression = "";
          result = "";
          lastMode = calcMode;
        }
        playSysSound(0);
      } else if (ty > 125 && ty < 280) {
        String btn = "";

        if (calcMode == 0) {
          int row = (ty - 125) / 35;
          int col = (tx - 10) / 55;
          if (row >= 0 && row < 4 && col >= 0 && col < 4) {
            const char* stdButtons[4][4] = { { "7", "8", "9", "+" }, { "4", "5", "6", "-" }, { "1", "2", "3", "*" }, { "C", "0", "=", "/" } };
            btn = stdButtons[row][col];
          }
        } else if (calcMode == 1) {
          int row = (ty - 125) / 22;
          int col = (tx - 5) / 46;
          if (row >= 0 && row < 6 && col >= 0 && col < 5) {
            btn = sciButtons[row][col];
          }
        } else if (calcMode == 2) {
          int row = (ty - 125) / 22;
          int col = (tx - 5) / 46;
          if (row >= 0 && row < 5 && col >= 0 && col < 5) {
            const char* progButtons[5][5] = {
              { "AND", "OR", "XOR", "NOT", "<<" },
              { ">>", "BIN", "HEX", "DEC", "CLR" },
              { "0b", "0x", "~", "&", "|" },
              { "=", "(", ")", "A", "B" },
              { "C", "D", "E", "F", "BACK" }
            };
            btn = progButtons[row][col];
          }
        }

        if (btn != "") {
          playSysSound(0);

          if (calcMode == 0) {
            if (btn == "C") {
              expression = "";
              result = "";
            } else if (btn == "=") {
              result = evaluateExpression(expression);
            } else {
              expression += btn;
            }
          } else if (calcMode == 1) {
            if (btn == "CLR" || btn == "C") {
              expression = "";
              result = "";
            } else if (btn == "=") {
              result = evaluateScientific(expression);
            } else if (btn == "π") {
              expression += "3.14159265359";
            } else if (btn == "e") {
              expression += "2.71828182846";
            } else if (btn == "M+") {
              memory = result.toDouble();
            } else if (btn == "M-") {
              memory -= result.toDouble();
            } else if (btn == "MR") {
              expression += String(memory);
            } else if (btn == "MC") {
              memory = 0;
            } else if (btn == "x²") {
              expression = "(" + expression + ")^2";
            } else if (btn == "x³") {
              expression = "(" + expression + ")^3";
            } else if (btn == "sqrt") {
              expression = "sqrt(" + expression + ")";
            } else if (btn == "cbrt") {
              expression = "cbrt(" + expression + ")";
            } else if (btn == "sin") {
              expression = "sin(" + expression + ")";
            } else if (btn == "cos") {
              expression = "cos(" + expression + ")";
            } else if (btn == "tan") {
              expression = "tan(" + expression + ")";
            } else if (btn == "asin") {
              expression = "asin(" + expression + ")";
            } else if (btn == "acos") {
              expression = "acos(" + expression + ")";
            } else if (btn == "atan") {
              expression = "atan(" + expression + ")";
            } else if (btn == "log") {
              expression = "log10(" + expression + ")";
            } else if (btn == "ln") {
              expression = "ln(" + expression + ")";
            } else if (btn == "eˣ") {
              expression = "exp(" + expression + ")";
            } else if (btn == "10ˣ") {
              expression = "pow10(" + expression + ")";
            } else if (btn == "!") {
              expression = "fact(" + expression + ")";
            } else if (btn == "1/x") {
              expression = "1/(" + expression + ")";
            } else if (btn == "|x|") {
              expression = "abs(" + expression + ")";
            } else {
              expression += btn;
            }
          } else if (calcMode == 2) {
            if (btn == "CLR") {
              expression = "";
              result = "";
            } else if (btn == "=") {
              result = evaluateProgrammer(expression);
            } else if (btn == "BIN") {
              long num = result.toInt();
              char binStr[33];
              itoa(num, binStr, 2);
              expression = String(binStr);
              result = expression;
            } else if (btn == "HEX") {
              long num = result.toInt();
              char hexStr[17];
              itoa(num, hexStr, 16);
              expression = String(hexStr);
              result = expression;
            } else if (btn == "DEC") {
              long num = parseNumber(expression);
              expression = String(num);
              result = expression;
            } else if (btn == "BACK" && expression.length() > 0) {
              expression.remove(expression.length() - 1);
            } else {
              expression += btn;
            }
          }
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== SETTINGS MENU ====================
void settingsMenu() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  int selected = 0;
  const char* options[] = { "Sound", "Brightness", "Dark Mode", "Prefixes", "File Manager", "WiFi", "About", "Back" };
  int numOptions = 8;
  bool running = true;

  while (running) {
    tft.fillRect(0, 35, 240, 200, BG_COLOR);

    for (int i = 0; i < numOptions; i++) {
      if (i == selected) tft.fillRoundRect(20, 40 + i * 35, 200, 30, 4, ACCENT_COLOR);
      else tft.fillRoundRect(20, 40 + i * 35, 200, 30, 4, BUTTON_COLOR);
      tft.setTextSize(2);
      tft.drawCentreString(options[i], 120, 52 + i * 35, 2);
    }

    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    tft.fillTriangle(215, 50, 235, 40, 235, 60, BUTTON_COLOR);
    tft.fillTriangle(215, 90, 235, 80, 235, 100, BUTTON_COLOR);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) running = false;
      else if (tx > 210 && ty > 40 && ty < 60 && selected > 0) selected--;
      else if (tx > 210 && ty > 80 && ty < 100 && selected < numOptions - 1) selected++;

      for (int i = 0; i < numOptions; i++) {
        if (ty > 35 + i * 35 && ty < 70 + i * 35) {
          selected = i;
          playSysSound(0);

          if (i == 0) {
            soundEnabled = !soundEnabled;
            printToConsole(successPrefix + "Sound: " + String(soundEnabled ? "ON" : "OFF"));
            EEPROM.write(0, soundEnabled);
            EEPROM.commit();
          } else if (i == 1) {
            brightness = (brightness + 50) % 256;
            analogWrite(SOUND_PIN, brightness);
            printToConsole(successPrefix + "Brightness: " + String(brightness));
          } else if (i == 2) {
            darkMode = !darkMode;
            applyTheme();
            printToConsole(successPrefix + "Dark Mode: " + String(darkMode ? "ON" : "OFF"));
            EEPROM.write(1, darkMode);
            EEPROM.commit();
          } else if (i == 3) {
            printToConsole(infoPrefix + "Cmd Prefix (" + cmdPrefix + "):", TFT_BLUE);
            String newCmd = getTextInput();
            if (newCmd != "") cmdPrefix = newCmd;
            printToConsole(infoPrefix + "Prompt Prefix (" + promptPrefix + "):", TFT_BLUE);
            String newPrompt = getTextInput();
            if (newPrompt != "") promptPrefix = newPrompt;
            EEPROM.put(10, cmdPrefix);
            EEPROM.put(50, promptPrefix);
            EEPROM.commit();
          } else if (i == 4) {
            fileManager();
          } else if (i == 5) {
            wifiManager();
          } else if (i == 6) {
            printToConsole(infoPrefix + "CYD OS v3.0 - Full Feature", TFT_GREEN);
            printToConsole(infoPrefix + "CHIP-8 Emulator ready!", TFT_BLUE);
            printToConsole(infoPrefix + "Dark/Light Mode available", TFT_BLUE);
          } else if (i == 7) {
            running = false;
          }

          delay(300);
          tft.fillRect(0, 35, 240, 200, BG_COLOR);
          break;
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void chip8Emulator() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  // ROMs sammeln (nur .ch8, .CH8, .c8, .bin)
  std::vector<String> roms;
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    String name = String(entry.name());
    if (name.endsWith(".ch8") || name.endsWith(".CH8") || name.endsWith(".c8") || name.endsWith(".bin")) {
      roms.push_back(name);
    }
    entry.close();
  }
  root.close();

  // Sortiere ROMs alphabetisch
  for (int i = 0; i < (int)roms.size() - 1; i++) {
    for (int j = i + 1; j < (int)roms.size(); j++) {
      if (roms[i] > roms[j]) {
        String temp = roms[i];
        roms[i] = roms[j];
        roms[j] = temp;
      }
    }
  }

  int selected = 0;
  int scrollOffset = 0;
  int maxVisible = 7;
  bool running = true;
  bool emulating = false;

  while (running && !emulating) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Titel
    tft.setTextSize(2);
    tft.drawCentreString("CHIP-8 EMULATOR", 120, 45, 2);
    tft.setTextSize(1);

    // ROM Count anzeigen
    char countStr[32];
    sprintf(countStr, "ROMs found: %d", roms.size());
    tft.drawCentreString(countStr, 120, 68, 1);

    // Keine ROMs gefunden
    if (roms.size() == 0) {
      tft.drawCentreString("No ROMs found on SD card!", 120, 100, 1);
      tft.drawCentreString("Place .ch8 files in /", 120, 115, 1);
    } else {
      // ROM Liste anzeigen mit Scroll
      int startIdx = scrollOffset;
      int endIdx = min(startIdx + maxVisible, (int)roms.size());

      for (int i = startIdx; i < endIdx; i++) {
        int y = 90 + (i - startIdx) * 20;

        // Ausgewählte ROM hervorheben
        if (i == selected) {
          tft.fillRoundRect(20, y - 2, 200, 18, 3, ACCENT_COLOR);
          tft.setTextColor(TFT_WHITE);
        } else {
          tft.setTextColor(TEXT_COLOR);
        }

        // ROM Name anzeigen
        tft.setCursor(25, y);
        char lineStr[64];
        sprintf(lineStr, "%3d. %s", i + 1, roms[i].substring(0, 24).c_str());
        tft.println(lineStr);
      }

      // Scroll Indikatoren
      if (scrollOffset > 0) {
        tft.fillTriangle(120, 85, 130, 79, 110, 79, ACCENT_COLOR);
      }
      if (scrollOffset + maxVisible < (int)roms.size()) {
        tft.fillTriangle(120, 235, 130, 241, 110, 241, ACCENT_COLOR);
      }
    }

    // Buttons
    tft.fillRoundRect(10, 260, 70, 30, 4, SUCCESS_COLOR);
    tft.drawCentreString("LOAD", 45, 272, 1);

    tft.fillRoundRect(85, 260, 70, 30, 4, WARNING_COLOR);
    tft.drawCentreString("BACK", 120, 272, 1);

    tft.fillRoundRect(160, 260, 70, 30, 4, ACCENT_COLOR);
    tft.drawCentreString("SPEED", 195, 272, 1);

    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    // Scroll Buttons
    tft.fillTriangle(215, 105, 235, 95, 235, 115, BUTTON_COLOR);
    tft.fillTriangle(215, 145, 235, 135, 235, 155, BUTTON_COLOR);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) {
        running = false;
      } else if (tx > 210 && ty > 95 && ty < 120 && scrollOffset > 0) {
        scrollOffset--;
        if (selected >= scrollOffset + maxVisible) {
          selected = scrollOffset + maxVisible - 1;
        }
        playSysSound(0);
        delay(100);
      } else if (tx > 210 && ty > 135 && ty < 160 && scrollOffset + maxVisible < (int)roms.size()) {
        scrollOffset++;
        if (selected < scrollOffset) {
          selected = scrollOffset;
        }
        playSysSound(0);
        delay(100);
      } else if (ty > 260 && ty < 290) {
        if (tx < 80 && roms.size() > 0) {  // LOAD
          if (selected >= 0 && selected < (int)roms.size()) {
            if (chip8.loadROM(roms[selected])) {
              printToConsole(successPrefix + "Loaded: " + roms[selected], TFT_GREEN);
              emulating = true;
            } else {
              printToConsole(errorPrefix + "Failed to load ROM", TFT_RED);
              delay(500);
            }
          }
        } else if (tx >= 80 && tx < 155) {  // BACK
          running = false;
        } else if (tx >= 155) {  // SPEED Button
          int newSpeed = (chip8.getClockSpeed() + 1) % 3;
          chip8.setClockSpeed(newSpeed);
          printToConsole(infoPrefix + "Speed: " + chip8.getClockSpeedName(), TFT_BLUE);
          playSysSound(0);
          delay(200);
        }
      } else if (ty > 85 && ty < 250 && roms.size() > 0) {
        int idx = scrollOffset + ((ty - 85) / 20);
        if (idx >= 0 && idx < (int)roms.size()) {
          selected = idx;
          playSysSound(0);
          delay(50);
        }
      }
    }
    delay(20);
  }

  // ==================== EMULATION ====================
  if (emulating) {
    tft.fillScreen(BG_COLOR);

    int scale = 3;
    int xOffset = (240 - 64 * scale) / 2;
    int yOffset = 45;

    // Buttons
    tft.fillRoundRect(5, 5, 50, 25, 4, SUCCESS_COLOR);
    tft.drawCentreString("SPD", 30, 12, 1);

    tft.fillRoundRect(60, 5, 50, 25, 4, TFT_CYAN);
    tft.drawCentreString("RST", 85, 12, 1);

    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    // ROM Name und Speed anzeigen
    tft.setTextColor(ACCENT_COLOR);
    tft.setTextSize(1);
    tft.setCursor(115, 12);
    String romName = chip8.getROMName();
    if (romName.length() > 18) romName = romName.substring(0, 15) + "...";
    tft.print(romName);

    tft.setCursor(115, 24);
    tft.print("CHIP-8 | ");
    tft.print(chip8.getClockSpeedName());

    // CHIP-8 Keypad Layout
    const char* keyNames[4][4] = {
      { "1", "2", "3", "C" },
      { "4", "5", "6", "D" },
      { "7", "8", "9", "E" },
      { "A", "0", "B", "F" }
    };

    int keyRegions[4][4][4];
    for (int ky = 0; ky < 4; ky++) {
      for (int kx = 0; kx < 4; kx++) {
        int x = 20 + kx * 45;
        int y = 200 + ky * 24;
        int w = 42;
        int h = 22;
        keyRegions[ky][kx][0] = x;
        keyRegions[ky][kx][1] = y;
        keyRegions[ky][kx][2] = x + w;
        keyRegions[ky][kx][3] = y + h;
        tft.fillRoundRect(x, y, w, h, 3, BUTTON_COLOR);
        tft.drawCentreString(keyNames[ky][kx], x + w / 2, y + h / 2 - 4, 2);
      }
    }

    // Display Rahmen
    tft.drawRect(xOffset - 2, yOffset - 2, 64 * scale + 4, 32 * scale + 4, TEXT_COLOR);

    unsigned long lastCycle = millis();
    unsigned long lastDraw = millis();
    unsigned long lastStats = millis();
    int cycles = 0;
    int frames = 0;

    int keyMap[4][4] = {
      { 0x1, 0x2, 0x3, 0xC },
      { 0x4, 0x5, 0x6, 0xD },
      { 0x7, 0x8, 0x9, 0xE },
      { 0xA, 0x0, 0xB, 0xF }
    };

    int lastPressedKey = -1;

    while (emulating && chip8.isRunning()) {
      int tx, ty;
      bool touchDetected = getTouch(tx, ty);

      // Button Handling
      if (touchDetected) {
        if (tx < 55 && ty < 30) {  // SPEED Button
          int newSpeed = (chip8.getClockSpeed() + 1) % 3;
          chip8.setClockSpeed(newSpeed);
          tft.fillRect(115, 24, 80, 10, BG_COLOR);
          tft.setCursor(115, 24);
          tft.print("CHIP-8 | ");
          tft.print(chip8.getClockSpeedName());
          playSysSound(0);
          delay(200);
        } else if (tx > 55 && tx < 110 && ty < 30) {  // RESET Button
          chip8.reset();
          if (chip8.loadROM(chip8.getROMName())) {
            printToConsole(infoPrefix + "Reset", TFT_BLUE);
          }
          playSysSound(1);
          delay(200);
        } else if (tx > 180 && ty < 40) {  // ESC
          emulating = false;
          break;
        }
      }

      // Touch Keypad
      int currentKey = -1;
      if (touchDetected) {
        for (int ky = 0; ky < 4; ky++) {
          for (int kx = 0; kx < 4; kx++) {
            if (tx >= keyRegions[ky][kx][0] && tx <= keyRegions[ky][kx][2] && ty >= keyRegions[ky][kx][1] && ty <= keyRegions[ky][kx][3]) {
              currentKey = keyMap[ky][kx];
              break;
            }
          }
          if (currentKey != -1) break;
        }
      }

      if (currentKey != -1 && currentKey != lastPressedKey) {
        if (lastPressedKey != -1) chip8.keyRelease();
        chip8.keyPress(currentKey);
        lastPressedKey = currentKey;
      } else if (currentKey == -1 && lastPressedKey != -1) {
        chip8.keyRelease();
        lastPressedKey = -1;
      }

      chip8.updateTimers();

      // Clock Speed gesteuerte Emulation
      int cycleDelay = chip8.getCycleDelay();
      if (millis() - lastCycle >= cycleDelay) {
        chip8.emulateCycle();
        lastCycle = millis();
        cycles++;
      }

      // 60 FPS Draw
      if (millis() - lastDraw >= 16) {
        if (chip8.needsDraw()) {
          chip8.draw(tft, xOffset, yOffset, scale);
        }
        lastDraw = millis();
        frames++;
      }

      // Stats anzeigen
      if (millis() - lastStats >= 1000) {
        tft.fillRect(5, 32, 100, 10, BG_COLOR);
        tft.setCursor(5, 32);
        tft.setTextColor(TFT_YELLOW);
        tft.printf("CPS:%d FPS:%d", cycles, frames);
        cycles = 0;
        frames = 0;
        lastStats = millis();
      }

      delay(1);
    }

    chip8.keyRelease();
    noTone(SOUND_PIN);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void snakeGame() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

#define MAX_SNAKE 100
  int snakeX[MAX_SNAKE], snakeY[MAX_SNAKE];
  int snakeLength = 3;
  int snakeDir = 2;  // 0=up,1=right,2=down,3=left
  int foodX, foodY;
  int score = 0;
  int highScore = 0;
  bool gameRunning = true;

  // Init snake
  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = 10 - i;
    snakeY[i] = 10;
  }

  // Generate first food
  randomSeed(millis());
  do {
    foodX = random(2, 22);
    foodY = random(2, 26);
  } while (foodX == snakeX[0] && foodY == snakeY[0]);

  unsigned long lastMove = 0;
  int moveDelay = 200;

  while (gameRunning) {
    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) gameRunning = false;
      else if (ty > 170) {
        if (tx < 80) snakeDir = 3;        // left
        else if (tx < 160) snakeDir = 2;  // down
        else if (tx < 240) snakeDir = 1;  // right
      } else if (ty < 140) {
        if (tx < 80) snakeDir = 0;  // up
      }
    }

    if (millis() - lastMove > moveDelay) {
      lastMove = millis();

      // Calculate new head position
      int newX = snakeX[0];
      int newY = snakeY[0];
      switch (snakeDir) {
        case 0: newY--; break;
        case 1: newX++; break;
        case 2: newY++; break;
        case 3: newX--; break;
      }

      // Wall collision
      if (newX < 0 || newX > 23 || newY < 0 || newY > 27) {
        gameRunning = false;
        break;
      }

      // Self collision
      for (int i = 1; i < snakeLength; i++) {
        if (newX == snakeX[i] && newY == snakeY[i]) {
          gameRunning = false;
          break;
        }
      }

      // Move snake
      for (int i = snakeLength; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
      }
      snakeX[0] = newX;
      snakeY[0] = newY;

      // Food collision
      if (newX == foodX && newY == foodY) {
        snakeLength++;
        score++;
        if (score > highScore) highScore = score;
        moveDelay = max(50, moveDelay - 5);

        do {
          foodX = random(2, 22);
          foodY = random(2, 26);
        } while (false);
      }

      // Draw game area
      tft.fillRect(0, 35, 240, 285, BG_COLOR);

      // Draw food
      tft.fillCircle(foodX * 10 + 5, foodY * 10 + 40, 4, TFT_RED);

      // Draw snake
      for (int i = 0; i < snakeLength; i++) {
        uint16_t color = (i == 0) ? TFT_GREEN : TFT_DARKGREEN;
        tft.fillRect(snakeX[i] * 10, snakeY[i] * 10 + 35, 9, 9, color);
      }

      // Draw score
      tft.setTextColor(TEXT_COLOR);
      tft.setTextSize(1);
      tft.setCursor(5, 5);
      tft.printf("Score: %d  High: %d", score, highScore);

      // Draw ESC button
      tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
      tft.drawCentreString("ESC", 207, 10, 2);
    }
    delay(10);
  }

  printToConsole(errorPrefix + "Game Over! Score: " + String(score), TFT_RED);

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void pongGame() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  int ballX = 120, ballY = 150;
  int ballDX = 2, ballDY = 2;
  int paddleY = 150;
  int aiPaddleY = 150;
  int playerScore = 0, aiScore = 0;
  bool gameActive = true;

  while (gameActive) {
    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) gameActive = false;
      paddleY = constrain(ty - 20, 50, 270);
    }

    // AI movement
    if (ballY > aiPaddleY + 20) aiPaddleY += 2;
    else if (ballY < aiPaddleY + 20) aiPaddleY -= 2;
    aiPaddleY = constrain(aiPaddleY, 50, 270);

    // Ball movement
    ballX += ballDX;
    ballY += ballDY;

    // Wall collision
    if (ballY < 40 || ballY > 300) ballDY = -ballDY;

    // Paddle collisions
    if (ballX < 20 && ballY > paddleY && ballY < paddleY + 50) {
      ballDX = -ballDX;
      ballDX += (ballY - (paddleY + 25)) / 10;
      playSysSound(0);
    }
    if (ballX > 220 && ballY > aiPaddleY && ballY < aiPaddleY + 50) {
      ballDX = -ballDX;
      playSysSound(0);
    }

    // Scoring
    if (ballX < 0) {
      aiScore++;
      ballX = 120;
      ballY = 150;
      ballDX = 2;
      ballDY = 2;
      playSysSound(4);
      delay(500);
    }
    if (ballX > 240) {
      playerScore++;
      ballX = 120;
      ballY = 150;
      ballDX = -2;
      ballDY = 2;
      playSysSound(4);
      delay(500);
    }

    // Draw
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Paddles
    tft.fillRect(5, paddleY, 5, 50, TFT_BLUE);
    tft.fillRect(230, aiPaddleY, 5, 50, TFT_RED);

    // Ball
    tft.fillCircle(ballX, ballY, 4, TEXT_COLOR);

    // Score
    tft.setTextSize(2);
    tft.setTextColor(TEXT_COLOR);
    tft.setCursor(100, 10);
    tft.printf("%d : %d", playerScore, aiScore);

    // ESC button
    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    delay(16);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

char tttBoard[3][3];


bool checkWin(char c) {
  for (int i = 0; i < 3; i++)
    if (tttBoard[i][0] == c && tttBoard[i][1] == c && tttBoard[i][2] == c) return true;
  for (int i = 0; i < 3; i++)
    if (tttBoard[0][i] == c && tttBoard[1][i] == c && tttBoard[2][i] == c) return true;
  if (tttBoard[0][0] == c && tttBoard[1][1] == c && tttBoard[2][2] == c) return true;
  if (tttBoard[0][2] == c && tttBoard[1][1] == c && tttBoard[2][0] == c) return true;
  return false;
}

bool checkDraw() {
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (tttBoard[i][j] == ' ') return false;
  return true;
}

void computerMove() {
  // Try to win
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (tttBoard[i][j] == ' ') {
        tttBoard[i][j] = 'O';
        if (checkWin('O')) return;
        tttBoard[i][j] = ' ';
      }
    }
  }

  // Try to block player
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (tttBoard[i][j] == ' ') {
        tttBoard[i][j] = 'X';
        if (checkWin('X')) {
          tttBoard[i][j] = 'O';
          return;
        }
        tttBoard[i][j] = ' ';
      }
    }
  }

  // Take center
  if (tttBoard[1][1] == ' ') {
    tttBoard[1][1] = 'O';
    return;
  }

  // Take corners
  int corners[4][2] = { { 0, 0 }, { 0, 2 }, { 2, 0 }, { 2, 2 } };
  for (int i = 0; i < 4; i++) {
    if (tttBoard[corners[i][0]][corners[i][1]] == ' ') {
      tttBoard[corners[i][0]][corners[i][1]] = 'O';
      return;
    }
  }

  // Take any empty
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (tttBoard[i][j] == ' ') {
        tttBoard[i][j] = 'O';
        return;
      }
    }
  }
}

void ticTacToe() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  // Init board
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      tttBoard[i][j] = ' ';

  playerTurn = true;
  bool gameActive = true;
  String message = "Your turn (X)";

  // Draw grid
  tft.drawLine(80, 80, 80, 240, TEXT_COLOR);
  tft.drawLine(160, 80, 160, 240, TEXT_COLOR);
  tft.drawLine(10, 140, 230, 140, TEXT_COLOR);
  tft.drawLine(10, 200, 230, 200, TEXT_COLOR);

  while (gameActive) {
    // Draw board
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        if (tttBoard[i][j] != ' ') {
          tft.setTextSize(4);
          tft.setTextColor(TEXT_COLOR);
          tft.drawCentreString(String(tttBoard[i][j]), i * 80 + 40, j * 60 + 100, 4);
        }
      }
    }

    // Draw message
    tft.fillRect(10, 250, 220, 30, BG_COLOR);
    tft.setTextSize(1);
    tft.setTextColor(ACCENT_COLOR);
    tft.setCursor(10, 260);
    tft.print(message);

    // ESC button
    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) {
        gameActive = false;
      }

      if (playerTurn && tx < 240 && ty > 80 && ty < 240) {
        int x = tx / 80;
        int y = (ty - 80) / 60;

        if (x >= 0 && x < 3 && y >= 0 && y < 3 && tttBoard[x][y] == ' ') {
          tttBoard[x][y] = 'X';
          tft.drawCentreString("X", x * 80 + 40, y * 60 + 100, 4);
          playerTurn = false;

          if (checkWin('X')) {
            message = "You win!";
            gameActive = false;
            printToConsole(successPrefix + "You won at Tic Tac Toe!", TFT_GREEN);
          } else if (checkDraw()) {
            message = "Draw!";
            gameActive = false;
            printToConsole(infoPrefix + "Tic Tac Toe - Draw!", TFT_YELLOW);
          } else {
            message = "Computer thinking...";
            delay(500);
            computerMove();
            playerTurn = true;

            if (checkWin('O')) {
              message = "Computer wins!";
              gameActive = false;
              printToConsole(errorPrefix + "Computer won!", TFT_RED);
            } else if (checkDraw()) {
              message = "Draw!";
              gameActive = false;
            } else {
              message = "Your turn (X)";
            }
          }
          playSysSound(0);
        }
      }
    }
    delay(50);
  }

  delay(1500);

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}
void drawingApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, TFT_WHITE);

  // Erweiterte Farbpalette (32 Farben)
  uint16_t colors[32] = {
    TFT_BLACK, TFT_NAVY, TFT_DARKGREEN, TFT_DARKCYAN,
    TFT_MAROON, TFT_PURPLE, TFT_OLIVE, TFT_LIGHTGREY,
    TFT_DARKGREY, TFT_BLUE, TFT_GREEN, TFT_CYAN,
    TFT_RED, TFT_MAGENTA, TFT_YELLOW, TFT_WHITE,
    TFT_ORANGE, TFT_GREENYELLOW, TFT_PINK, TFT_BROWN,
    TFT_GOLD, TFT_SILVER, TFT_SKYBLUE, TFT_VIOLET,
    0xFD20, 0xAFE5, 0xAAA0, 0xFC10,
    0x07E0, 0x001F, 0xF800, 0x07FF
  };

  String toolNames[] = { "BRUSH", "LINE", "RECT", "CIRCLE", "FILL", "ERASER", "PICKER", "TEXT" };
  int numTools = 8;

  uint16_t currentColor = TFT_BLACK;
  int brushSize = 2;
  int currentTool = 0;  // 0=brush, 1=line, 2=rect, 3=circle, 4=fill, 5=eraser, 6=picker, 7=text
  bool drawing = true;
  int startX = -1, startY = -1;
  bool isDrawing = false;
  int page = 0;  // 0=colors, 1=RGB mixer
  uint8_t customR = 0, customG = 0, customB = 0;

  // UI Layout Konstanten
  const int TOP_BAR = 60;
  const int BOTTOM_BAR = 280;

  while (drawing) {
    // === TOP BAR: Tools + ESC ===
    tft.fillRect(0, 35, 240, 25, TFT_DARKGREY);
    tft.fillRoundRect(5, 37, 20, 20, 2, WARNING_COLOR);
    tft.drawCentreString("X", 15, 42, 2);

    // Tools als Buttons
    for (int i = 0; i < numTools; i++) {
      int tx = 35 + i * 25;
      uint16_t bg = (i == currentTool) ? ACCENT_COLOR : TFT_LIGHTGREY;
      tft.fillRoundRect(tx, 37, 23, 20, 2, bg);
      tft.drawCentreString(String(toolNames[i][0]), tx + 12, 42, 1);
    }

    // === FARBPALETTE (2 Seiten) ===
    if (page == 0) {
      // Seite 1: Vordefinierte Farben
      tft.fillRect(0, 60, 240, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.drawCentreString("PALETTE [>] for RGB Mixer", 120, 64, 1);

      for (int i = 0; i < 16; i++) {
        int x = (i % 8) * 30;
        int y = 80 + (i / 8) * 25;
        tft.fillRect(x, y, 29, 24, colors[i]);
        if (colors[i] == currentColor) {
          tft.drawRect(x - 1, y - 1, 31, 26, TFT_WHITE);
          tft.drawRect(x, y, 29, 24, TFT_BLACK);
        }
      }
    } else {
      // Seite 2: RGB Mixer + restliche Farben
      tft.fillRect(0, 60, 240, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("RGB MIXER [<] for Palette", 120, 64, 1);

      // RGB Slider
      tft.setTextColor(TEXT_COLOR);
      tft.drawCentreString("R:" + String(customR), 30, 85, 1);
      tft.drawCentreString("G:" + String(customG), 100, 85, 1);
      tft.drawCentreString("B:" + String(customB), 170, 85, 1);

      // R Slider
      tft.fillRect(10, 95, 60, 10, TFT_RED);
      tft.fillRect(10, 95, map(customR, 0, 255, 0, 60), 10, TFT_WHITE);
      tft.drawRect(10, 95, 60, 10, TFT_BLACK);

      // G Slider
      tft.fillRect(80, 95, 60, 10, TFT_GREEN);
      tft.fillRect(80, 95, map(customG, 0, 255, 0, 60), 10, TFT_WHITE);
      tft.drawRect(80, 95, 60, 10, TFT_BLACK);

      // B Slider
      tft.fillRect(150, 95, 60, 10, TFT_BLUE);
      tft.fillRect(150, 95, map(customB, 0, 255, 0, 60), 10, TFT_WHITE);
      tft.drawRect(150, 95, 60, 10, TFT_BLACK);

      // Vorschau
      uint16_t previewColor = tft.color565(customR, customG, customB);
      tft.fillRect(80, 110, 80, 25, previewColor);
      tft.drawRect(80, 110, 80, 25, TFT_BLACK);
      tft.drawCentreString("CURRENT", 120, 117, 1);
      if (previewColor == TFT_BLACK) tft.setTextColor(TFT_WHITE);
      else tft.setTextColor(TFT_BLACK);
      tft.drawCentreString("CURRENT", 120, 117, 1);
      tft.setTextColor(TEXT_COLOR);

      // OK Button für RGB
      tft.fillRoundRect(165, 110, 50, 25, 3, SUCCESS_COLOR);
      tft.drawCentreString("SET", 190, 117, 1);

      // Restliche 16 Farben
      for (int i = 0; i < 16; i++) {
        int x = (i % 8) * 30;
        int y = 140 + (i / 8) * 25;
        tft.fillRect(x, y, 29, 24, colors[i + 16]);
        if (colors[i + 16] == currentColor) {
          tft.drawRect(x - 1, y - 1, 31, 26, TFT_WHITE);
          tft.drawRect(x, y, 29, 24, TFT_BLACK);
        }
      }
    }

    // === BOTTOM BAR: Pinselgröße ===
    tft.fillRect(0, BOTTOM_BAR, 240, 40, TFT_DARKGREY);

    // Pinselgrößen als Kreise
    for (int i = 1; i <= 6; i++) {
      int x = 10 + i * 35;
      int y = 300;
      uint16_t bg = (i == brushSize) ? ACCENT_COLOR : TFT_WHITE;
      tft.fillCircle(x, y, i + 2, bg);
      if (i == brushSize) tft.drawCircle(x, y, i + 3, TFT_BLACK);
    }

    // Clear Button
    tft.fillRoundRect(5, 280, 40, 18, 2, TFT_RED);
    tft.drawCentreString("CLR", 25, 284, 1);

    // Aktuelle Farbe anzeigen
    tft.fillRect(200, 280, 35, 35, currentColor);
    tft.drawRect(199, 279, 37, 37, TFT_BLACK);

    // === TOUCH HANDLING ===
    int tx, ty;
    if (getTouch(tx, ty)) {
      // ESC Button
      if (tx < 25 && ty > 35 && ty < 60) {
        drawing = false;
      }

      // Tool Auswahl
      else if (ty > 35 && ty < 60 && tx >= 30) {
        int toolIdx = (tx - 35) / 25;
        if (toolIdx >= 0 && toolIdx < numTools) {
          currentTool = toolIdx;
          playSysSound(0);
        }
      }

      // Palette umschalten
      else if (ty > 60 && ty < 80) {
        if (tx > 120) page = 1;
        else page = 0;
      }

      // Zurück zur Palette
      else if (page == 1 && ty > 60 && ty < 80 && tx < 120) {
        page = 0;
      }

      // Farbauswahl Seite 0
      else if (page == 0 && ty >= 80 && ty < 130 && tx < 240) {
        int colorIdx = (tx / 30) + ((ty - 80) / 25) * 8;
        if (colorIdx >= 0 && colorIdx < 16) {
          currentColor = colors[colorIdx];
          playSysSound(0);
        }
      }

      // RGB Slider
      else if (page == 1 && ty >= 95 && ty < 105) {
        if (tx > 10 && tx < 70) {
          customR = map(tx, 10, 70, 0, 255);
        } else if (tx > 80 && tx < 140) {
          customG = map(tx, 80, 140, 0, 255);
        } else if (tx > 150 && tx < 210) {
          customB = map(tx, 150, 210, 0, 255);
        }
      }

      // RGB SET Button
      else if (page == 1 && ty >= 110 && ty < 135 && tx > 165) {
        currentColor = tft.color565(customR, customG, customB);
        playSysSound(1);
      }

      // Farbauswahl Seite 1 (zweite Hälfte)
      else if (page == 1 && ty >= 140 && ty < 190 && tx < 240) {
        int colorIdx = (tx / 30) + ((ty - 140) / 25) * 8 + 16;
        if (colorIdx >= 16 && colorIdx < 32) {
          currentColor = colors[colorIdx];
          playSysSound(0);
        }
      }

      // Pinselgröße
      else if (ty > BOTTOM_BAR && ty < 320 && tx > 30 && tx < 230) {
        for (int i = 1; i <= 6; i++) {
          int x = 10 + i * 35;
          if (abs(tx - x) < 10) {
            brushSize = i;
            playSysSound(0);
            break;
          }
        }
      }

      // Clear Button
      else if (tx < 50 && ty > BOTTOM_BAR && ty < 298) {
        tft.fillRect(0, 60, 240, 220, TFT_WHITE);
        playSysSound(3);
      }

      // === ZEICHENBEREICH ===
      else if (ty > (page == 0 ? 130 : 190) && ty < BOTTOM_BAR && tx < 240) {
        int drawY = ty;

        if (currentTool == 0) {  // Brush
          uint16_t drawColor = (currentTool == 5) ? TFT_WHITE : currentColor;
          tft.fillCircle(tx, drawY, brushSize, drawColor);
        } else if (currentTool == 1) {  // Line
          if (!isDrawing) {
            startX = tx;
            startY = drawY;
            isDrawing = true;
          } else {
            tft.drawLine(startX, startY, tx, drawY, currentColor);
            isDrawing = false;
          }
        } else if (currentTool == 2) {  // Rect
          if (!isDrawing) {
            startX = tx;
            startY = drawY;
            isDrawing = true;
          } else {
            tft.drawRect(min(startX, tx), min(startY, drawY),
                         abs(tx - startX), abs(drawY - startY), currentColor);
            isDrawing = false;
          }
        } else if (currentTool == 3) {  // Circle
          if (!isDrawing) {
            startX = tx;
            startY = drawY;
            isDrawing = true;
          } else {
            int radius = sqrt(pow(tx - startX, 2) + pow(drawY - startY, 2));
            tft.drawCircle(startX, startY, radius, currentColor);
            isDrawing = false;
          }
        } else if (currentTool == 4) {  // Fill (Flood Fill - einfach)
          tft.fillCircle(tx, drawY, brushSize * 3, currentColor);
        } else if (currentTool == 5) {  // Eraser
          tft.fillCircle(tx, drawY, brushSize + 2, TFT_WHITE);
        } else if (currentTool == 6) {  // Color Picker
          uint16_t pickedColor = tft.readPixel(tx, drawY);
          if (pickedColor != TFT_BLACK || pickedColor != TFT_WHITE) {
            currentColor = pickedColor;
            // Update custom RGB
            customR = (pickedColor >> 11) & 0x1F;
            customR = (customR * 255) / 31;
            customG = (pickedColor >> 5) & 0x3F;
            customG = (customG * 255) / 63;
            customB = pickedColor & 0x1F;
            customB = (customB * 255) / 31;
            playSysSound(0);
          }
        } else if (currentTool == 7) {  // Text
          // Einfacher Text-Modus: Tippe auf Position für Buchstabe
          // In Vollversion: kleines Text-Eingabefeld
          tft.setCursor(tx, drawY);
          tft.setTextColor(currentColor);
          tft.setTextSize(brushSize);
          tft.print("A");
        }
      }
    }
    delay(15);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}



void loadNotes() {
  notes.clear();
  File f = SD.open("/notes.txt", FILE_READ);
  if (f) {
    while (f.available()) {
      notes.push_back(f.readStringUntil('\n'));
    }
    f.close();
  }
}

void saveNotes() {
  File f = SD.open("/notes.txt", FILE_WRITE);
  if (f) {
    for (String note : notes) {
      f.println(note);
    }
    f.close();
  }
}

void notesApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);
  loadNotes();
  int selected = -1;
  bool running = true;

  while (running) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Draw notes list
    tft.setTextSize(1);
    tft.setTextColor(TEXT_COLOR);
    for (int i = 0; i < min(10, (int)notes.size()); i++) {
      int y = 45 + i * 22;
      if (i == selected) {
        tft.fillRoundRect(10, y - 2, 220, 20, 3, ACCENT_COLOR);
        tft.setTextColor(TFT_WHITE);
      } else {
        tft.fillRoundRect(10, y - 2, 220, 20, 3, BUTTON_COLOR);
        tft.setTextColor(TEXT_COLOR);
      }
      tft.setCursor(15, y);
      String display = notes[i];
      if (display.length() > 27) display = display.substring(0, 24) + "...";
      tft.print(display);
    }

    if (notes.size() == 0) {
      tft.drawCentreString("No notes. Press NEW to create", 120, 100, 1);
    }

    // Buttons
    tft.fillRoundRect(10, 270, 70, 25, 4, SUCCESS_COLOR);
    tft.drawCentreString("NEW", 45, 280, 1);
    tft.fillRoundRect(85, 270, 70, 25, 4, WARNING_COLOR);
    tft.drawCentreString("DEL", 120, 280, 1);
    tft.fillRoundRect(160, 270, 70, 25, 4, TFT_BLUE);
    tft.drawCentreString("EDIT", 195, 280, 1);

    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) {
        running = false;
      } else if (ty > 270 && ty < 295) {
        if (tx < 80) {  // NEW
          printToConsole(infoPrefix + "Enter note text:", TFT_BLUE);
          String newNote = getTextInput();
          if (newNote != "") {
            notes.push_back(newNote);
            saveNotes();
            printToConsole(successPrefix + "Note saved", TFT_GREEN);
          }
        } else if (tx < 155 && selected >= 0) {  // DEL
          notes.erase(notes.begin() + selected);
          selected = -1;
          saveNotes();
          printToConsole(successPrefix + "Note deleted", TFT_GREEN);
        } else if (tx >= 155 && selected >= 0) {  // EDIT
          printToConsole(infoPrefix + "Edit note (current: " + notes[selected] + ")", TFT_BLUE);
          String editedNote = getTextInput();
          if (editedNote != "") {
            notes[selected] = editedNote;
            saveNotes();
            printToConsole(successPrefix + "Note updated", TFT_GREEN);
          }
        }
      } else if (ty > 40 && ty < 260) {
        selected = (ty - 40) / 22;
        if (selected >= (int)notes.size()) selected = -1;
        playSysSound(0);
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void todoApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  std::vector<String> todos;
  std::vector<bool> done;

  // Load todos
  File f = SD.open("/todos.txt", FILE_READ);
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.startsWith("[D]")) {
        done.push_back(true);
        todos.push_back(line.substring(3));
      } else if (line.length() > 0) {
        done.push_back(false);
        todos.push_back(line);
      }
    }
    f.close();
  }

  bool running = true;
  int selected = -1;

  while (running) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    tft.setTextSize(1);
    tft.setTextColor(TEXT_COLOR);

    for (int i = 0; i < min(8, (int)todos.size()); i++) {
      int y = 45 + i * 25;
      if (i == selected) {
        tft.fillRoundRect(10, y - 2, 220, 22, 3, ACCENT_COLOR);
        tft.setTextColor(TFT_WHITE);
      } else {
        tft.fillRoundRect(10, y - 2, 220, 22, 3, BUTTON_COLOR);
        tft.setTextColor(TEXT_COLOR);
      }

      String prefix = done[i] ? "✓ " : "☐ ";
      String display = prefix + todos[i];
      if (display.length() > 27) display = display.substring(0, 24) + "...";
      tft.setCursor(15, y);
      tft.print(display);
    }

    if (todos.size() == 0) {
      tft.drawCentreString("No tasks. Press ADD to create", 120, 100, 1);
    }

    // Buttons
    tft.fillRoundRect(10, 265, 55, 25, 4, SUCCESS_COLOR);
    tft.drawCentreString("ADD", 37, 274, 1);
    tft.fillRoundRect(70, 265, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("DEL", 97, 274, 1);
    tft.fillRoundRect(130, 265, 55, 25, 4, TFT_BLUE);
    tft.drawCentreString("TOGGLE", 157, 274, 1);
    tft.fillRoundRect(190, 265, 45, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 212, 274, 1);

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 190 && ty > 265 && ty < 290) {
        running = false;
      } else if (ty > 265 && ty < 290) {
        if (tx < 65) {  // ADD
          printToConsole(infoPrefix + "New todo:", TFT_BLUE);
          String newTodo = getTextInput();
          if (newTodo != "") {
            todos.push_back(newTodo);
            done.push_back(false);
          }
        } else if (tx < 125 && selected >= 0) {  // DEL
          todos.erase(todos.begin() + selected);
          done.erase(done.begin() + selected);
          selected = -1;
        } else if (tx < 185 && selected >= 0) {  // TOGGLE
          done[selected] = !done[selected];
        }
      } else if (ty > 40 && ty < 250) {
        selected = (ty - 40) / 25;
        if (selected >= (int)todos.size()) selected = -1;
        playSysSound(0);
      }
    }
    delay(50);
  }

  // Save todos
  f = SD.open("/todos.txt", FILE_WRITE);
  if (f) {
    for (int i = 0; i < (int)todos.size(); i++) {
      if (done[i]) f.println("[D]" + todos[i]);
      else f.println(todos[i]);
    }
    f.close();
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void timerApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  int minutes = 0, seconds = 0;
  int setMinutes = 0, setSeconds = 0;
  bool running = false;
  bool setting = true;
  unsigned long startTime = 0;
  int selectedField = 0;  // 0=minutes,1=seconds

  while (true) {
    tft.fillRect(0, 35, 240, 200, BG_COLOR);

    // Timer display
    tft.setTextSize(4);
    tft.setTextColor(TEXT_COLOR);
    String timeStr = String(setMinutes) + ":" + (setSeconds < 10 ? "0" : "") + String(setSeconds);
    tft.drawCentreString(timeStr, 120, 80, 4);

    // Status
    tft.setTextSize(1);
    tft.setTextColor(ACCENT_COLOR);
    tft.drawCentreString(running ? "RUNNING..." : (setting ? "SET TIME" : "PAUSED"), 120, 140, 2);

    // Highlight selected field
    if (setting && !running) {
      if (selectedField == 0) {
        tft.drawRect(40, 70, 80, 50, ACCENT_COLOR);
      } else {
        tft.drawRect(120, 70, 80, 50, ACCENT_COLOR);
      }
    }

    // Buttons
    if (setting && !running) {
      tft.fillRoundRect(20, 200, 60, 35, 4, BUTTON_COLOR);
      tft.drawCentreString("+", 50, 212, 2);
      tft.fillRoundRect(90, 200, 60, 35, 4, BUTTON_COLOR);
      tft.drawCentreString("-", 120, 212, 2);
      tft.fillRoundRect(160, 200, 60, 35, 4, SUCCESS_COLOR);
      tft.drawCentreString("SET", 190, 212, 2);
    } else {
      tft.fillRoundRect(20, 200, 60, 35, 4, SUCCESS_COLOR);
      tft.drawCentreString(running ? "PAUSE" : "START", 50, 212, 2);
      tft.fillRoundRect(90, 200, 60, 35, 4, WARNING_COLOR);
      tft.drawCentreString("RESET", 120, 212, 2);
      tft.fillRoundRect(160, 200, 60, 35, 4, TFT_BLUE);
      tft.drawCentreString("EDIT", 190, 212, 2);
    }

    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    // Timer logic
    if (running) {
      unsigned long elapsed = (millis() - startTime) / 1000;
      int totalSeconds = setMinutes * 60 + setSeconds;
      int remaining = totalSeconds - elapsed;

      if (remaining <= 0) {
        running = false;
        setting = true;
        playSysSound(4);
        printToConsole(successPrefix + "Timer finished!", TFT_GREEN);
        tone(SOUND_PIN, 2000, 500);
        delay(500);
        tone(SOUND_PIN, 1500, 500);
      } else {
        minutes = remaining / 60;
        seconds = remaining % 60;
        setMinutes = minutes;
        setSeconds = seconds;
      }
    }

    int tx, ty;
    if (getTouch(tx, ty)) {
      if (tx > 180 && ty < 40) break;

      if (setting && !running) {
        // + / - buttons
        if (ty > 200 && ty < 235) {
          if (tx < 80) {  // PLUS
            if (selectedField == 0) {
              setMinutes = (setMinutes + 1) % 100;
            } else {
              setSeconds = (setSeconds + 1) % 60;
            }
            playSysSound(0);
          } else if (tx < 150) {  // MINUS
            if (selectedField == 0) {
              setMinutes = (setMinutes - 1 + 100) % 100;
            } else {
              setSeconds = (setSeconds - 1 + 60) % 60;
            }
            playSysSound(0);
          } else if (tx >= 150) {  // SET
            running = true;
            setting = false;
            startTime = millis();
            playSysSound(1);
          }
        } else if (ty > 60 && ty < 130) {
          selectedField = (tx > 100) ? 1 : 0;
          playSysSound(0);
        }
      } else {
        if (ty > 200 && ty < 235) {
          if (tx < 80) {  // START/PAUSE
            if (running) {
              running = false;
              setting = false;
            } else {
              running = true;
              startTime = millis();
            }
            playSysSound(0);
          } else if (tx < 150) {  // RESET
            running = false;
            setting = true;
            minutes = 0;
            seconds = 0;
            setMinutes = 0;
            setSeconds = 0;
            playSysSound(3);
          } else if (tx >= 150) {  // EDIT
            running = false;
            setting = true;
            playSysSound(0);
          }
        }
      }
    }
    delay(50);
  }

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void chatApp(bool isHost) {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);
  tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
  tft.drawCentreString("ESC", 207, 10, 2);

  if (isHost) {
    printToConsole(infoPrefix + "Starting chat server...", TFT_BLUE);
    WiFi.softAP("CYD_CHAT", "12345678");
    server.on("/send", []() {
      if (server.hasArg("msg")) {
        String msg = server.arg("msg");
        printToConsole("[Guest] " + msg, TFT_GREEN);
        server.send(200, "text/plain", "OK");
      }
    });
    server.begin();
    printToConsole(successPrefix + "Host mode active! SSID: CYD_CHAT", TFT_GREEN);
    printToConsole(infoPrefix + "Others can connect and send messages", TFT_BLUE);
  } else {
    printToConsole(infoPrefix + "Connecting to CYD_CHAT...", TFT_BLUE);
    WiFi.begin("CYD_CHAT", "12345678");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 50) {
      delay(500);
      attempts++;
      tft.fillCircle(200 + (attempts % 3) * 5, 50, 2, SUCCESS_COLOR);
    }
    if (WiFi.status() == WL_CONNECTED) {
      printToConsole(successPrefix + "Connected! You can now chat", TFT_GREEN);
    } else {
      printToConsole(errorPrefix + "Connection failed!", TFT_RED);
      delay(2000);
      kbMode = oldKbMode;
      currentInput = oldInput;
      tft.fillScreen(BG_COLOR);
      drawKeyboard();
      drawScrollButtons();
      refreshTerminal();
      updateInputLine(true);
      return;
    }
  }

  drawKeyboard();
  bool running = true;
  unsigned long lastCheck = millis();

  while (running) {
    if (isHost) {
      server.handleClient();
    } else {
      // Client mode - check for messages (simplified)
      if (millis() - lastCheck > 2000) {
        lastCheck = millis();
        // In a real implementation, you would poll for messages
      }
    }

    String msg = handleKeyboardInput();
    if (msg == "ESC_SIGNAL") {
      running = false;
    } else if (msg != "") {
      if (isHost) {
        printToConsole("[Me] " + msg, TFT_CYAN);
      } else {
        printToConsole("[Me] " + msg, TFT_CYAN);
        // Send message to host
        if (WiFi.status() == WL_CONNECTED) {
          WiFiClient client;
          if (client.connect(IPAddress(192, 168, 4, 1), 80)) {
            client.print("GET /send?msg=" + msg + " HTTP/1.1\r\n");
            client.print("Host: 192.168.4.1\r\n");
            client.print("Connection: close\r\n\r\n");
            client.stop();
          } else {
            printToConsole(errorPrefix + "Connection lost!", TFT_RED);
          }
        }
      }
    }
    delay(10);
  }

  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

void writeHelpFile() {
  printToConsole(infoPrefix + "Creating help files...", TFT_BLUE);

  // Haupt-Hilfe
  SD.remove("/help.txt");
  File f = SD.open("/help.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════╗");
    f.println("║              CYT TERMINAL OS v3.0                        ║");
    f.println("║              ESP32-WROOM Command Reference               ║");
    f.println("╚══════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("╔══════════════════════════════════════════════════════════╗");
    f.println("║ QUICK REFERENCE                                          ║");
    f.println("╠══════════════════════════════════════════════════════════╣");
    f.println("║ help, ?        - This help system                        ║");
    f.println("║ man <cmd>      - Manual page for command                 ║");
    f.println("║ cls, clear     - Clear terminal                          ║");
    f.println("║ reboot         - Restart system                          ║");
    f.println("║ date, uptime   - Time information                        ║");
    f.println("║ free, ps       - System resources                        ║");
    f.println("║ neofetch       - System info banner                      ║");
    f.println("║ darkmode       - Toggle dark/light mode                  ║");
    f.println("║ sound          - Toggle system sounds                    ║");
    f.println("║ colors         - List available colors                   ║");
    f.println("╚══════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("AVAILABLE CATEGORIES:");
    f.println("  help system    - System commands");
    f.println("  help files     - File operations");
    f.println("  help net       - Network tools");
    f.println("  help games     - Games & Entertainment");
    f.println("  help apps      - Applications");
    f.println("  help i2c       - I2C bus tools");
    f.println("  help dev       - Developer tools");
    f.println("  help tips      - Tips & Tricks");
    f.println("  help all       - Complete reference");
    f.close();
  }

  // System Hilfe
  SD.remove("/help_sys.txt");
  f = SD.open("/help_sys.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("SYSTEM COMMANDS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("cls, clear");
    f.println("  Clears the terminal screen completely");
    f.println("  Usage: cls");
    f.println("");
    f.println("reboot, restart");
    f.println("  Restarts the CYT OS");
    f.println("  Usage: reboot");
    f.println("");
    f.println("date");
    f.println("  Shows seconds since boot");
    f.println("  Usage: date");
    f.println("");
    f.println("uptime");
    f.println("  Shows system uptime in HH:MM:SS");
    f.println("  Usage: uptime");
    f.println("");
    f.println("free");
    f.println("  Displays RAM and Flash usage");
    f.println("  Usage: free");
    f.println("");
    f.println("ps");
    f.println("  Lists running processes");
    f.println("  Usage: ps");
    f.println("");
    f.println("neofetch");
    f.println("  Shows ASCII art system info");
    f.println("  Usage: neofetch");
    f.println("");
    f.println("sysinfo, stats");
    f.println("  Detailed hardware information");
    f.println("  Usage: sysinfo");
    f.println("");
    f.println("sd, storage");
    f.println("  SD card status and capacity");
    f.println("  Usage: sd");
    f.println("");
    f.println("darkmode, theme");
    f.println("  Toggle dark/light mode");
    f.println("  Usage: darkmode");
    f.println("");
    f.println("sound");
    f.println("  Toggle system sounds on/off");
    f.println("  Usage: sound");
    f.println("");
    f.println("colors");
    f.println("  List all available color names");
    f.println("  Usage: colors");
    f.println("");
    f.println("settings");
    f.println("  Open settings menu (GUI)");
    f.println("  Usage: settings");
    f.close();
  }

  // Datei Hilfe
  SD.remove("/help_files.txt");
  f = SD.open("/help_files.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("FILE OPERATIONS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("ls [path]");
    f.println("  List directory contents");
    f.println("  Usage: ls /folder");
    f.println("");
    f.println("dir [path]");
    f.println("  Alias for ls");
    f.println("  Usage: dir /");
    f.println("");
    f.println("cat <file>");
    f.println("  Display file contents");
    f.println("  Usage: cat /readme.txt");
    f.println("");
    f.println("head [-n<lines>] <file>");
    f.println("  Show first N lines (default 10)");
    f.println("  Usage: head -n5 /log.txt");
    f.println("");
    f.println("tail [-n<lines>] <file>");
    f.println("  Show last N lines (default 10)");
    f.println("  Usage: tail -n20 /history.txt");
    f.println("");
    f.println("grep <pattern> <file>");
    f.println("  Search for pattern in file");
    f.println("  Usage: grep 'error' /log.txt");
    f.println("");
    f.println("rm <file/dir>");
    f.println("  Delete file or directory");
    f.println("  Usage: rm /oldfile.txt");
    f.println("");
    f.println("touch <file>");
    f.println("  Create empty file");
    f.println("  Usage: touch /newfile.txt");
    f.println("");
    f.println("mkdir <dir>");
    f.println("  Create directory");
    f.println("  Usage: mkdir /newfolder");
    f.println("");
    f.println("edit, editor, nano");
    f.println("  Open text editor");
    f.println("  Usage: edit myfile.txt");
    f.println("");
    f.println("files, fm");
    f.println("  Open file manager (GUI)");
    f.println("  Usage: files");
    f.close();
  }

  // Netzwerk Hilfe
  SD.remove("/help_net.txt");
  f = SD.open("/help_net.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("NETWORK TOOLS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("ifconfig, ip");
    f.println("  Show network configuration");
    f.println("  Usage: ifconfig");
    f.println("");
    f.println("ping <host>");
    f.println("  Test network connectivity");
    f.println("  Usage: ping google.com");
    f.println("");
    f.println("wifi");
    f.println("  WiFi connection manager");
    f.println("  Usage: wifi");
    f.println("");
    f.println("wget <url>");
    f.println("  Download file from internet");
    f.println("  Usage: wget example.com/file.txt");
    f.println("");
    f.println("clock, time, zeit");
    f.println("  Show current time via NTP");
    f.println("  Usage: clock");
    f.println("");
    f.println("chat");
    f.println("  Start chat application");
    f.println("  Usage: chat");
    f.close();
  }

  // Spiele Hilfe
  SD.remove("/help_games.txt");
  f = SD.open("/help_games.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("GAMES & FUN");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("snake");
    f.println("  Classic snake game");
    f.println("  Controls: Touch left=left, right=right,");
    f.println("            top=up, bottom=down");
    f.println("  Usage: snake");
    f.println("");
    f.println("pong");
    f.println("  Pong game vs AI");
    f.println("  Controls: Touch to move paddle");
    f.println("  Usage: pong");
    f.println("");
    f.println("tictac, ttt");
    f.println("  Tic-Tac-Toe vs AI");
    f.println("  Usage: tictac");
    f.println("");
    f.println("chip8, chip-8");
    f.println("  CHIP-8 game emulator");
    f.println("  Place .ch8 ROMs on SD card");
    f.println("  Usage: chip8");
    f.println("");
    f.println("dice <sides>");
    f.println("  Roll a dice with N sides");
    f.println("  Usage: dice 20");
    f.println("");
    f.println("random [max]");
    f.println("  Generate random number");
    f.println("  Usage: random 100");
    f.close();
  }

  // Apps Hilfe
  SD.remove("/help_apps.txt");
  f = SD.open("/help_apps.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("APPLICATIONS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("calc, rechner");
    f.println("  Scientific calculator");
    f.println("  Modes: Standard, Scientific, Programmer");
    f.println("  Usage: calc");
    f.println("");
    f.println("draw, paint");
    f.println("  Drawing application");
    f.println("  Features: 32 colors, RGB mixer,");
    f.println("            8 tools, 6 brush sizes");
    f.println("  Usage: draw");
    f.println("");
    f.println("notes");
    f.println("  Note-taking app");
    f.println("  Usage: notes");
    f.println("");
    f.println("todo");
    f.println("  To-do list manager");
    f.println("  Usage: todo");
    f.println("");
    f.println("timer");
    f.println("  Countdown timer");
    f.println("  Usage: timer");
    f.close();
  }

  // I2C Hilfe
  SD.remove("/help_i2c.txt");
  f = SD.open("/help_i2c.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("I2C BUS TOOLS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("i2c, i2cscan");
    f.println("  Scan I2C bus for devices");
    f.println("  Usage: i2cscan");
    f.println("");
    f.println("i2ctool");
    f.println("  Open graphical I2C tool");
    f.println("  Usage: i2ctool");
    f.println("");
    f.println("i2csend <addr> <data>");
    f.println("  Send data to I2C device");
    f.println("  Usage: i2csend 3C Hello");
    f.println("  Usage: i2csend 3C 0x48656C6C6F");
    f.println("");
    f.println("i2cread <addr> <bytes>");
    f.println("  Read bytes from I2C device");
    f.println("  Usage: i2cread 3C 10");
    f.println("");
    f.println("i2cwrite <addr> <reg> <data>");
    f.println("  Write to I2C register");
    f.println("  Usage: i2cwrite 3C 00 01");
    f.println("");
    f.println("I2C Pins: SDA=GPIO16, SCL=GPIO39");
    f.close();
  }

  // Developer Tools
  SD.remove("/help_dev.txt");
  f = SD.open("/help_dev.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("DEVELOPER TOOLS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("echo <text>");
    f.println("  Print text to terminal");
    f.println("  Variables: $TIME, $FREE, $UPTIME");
    f.println("  Usage: echo Hello World");
    f.println("");
    f.println("eval <expression>");
    f.println("  Evaluate math expression");
    f.println("  Usage: eval 2+2");
    f.println("");
    f.println("delay, sleep <ms>");
    f.println("  Wait for milliseconds");
    f.println("  Usage: delay 1000");
    f.close();
  }

  // Tips & Tricks
  SD.remove("/help_tips.txt");
  f = SD.open("/help_tips.txt", FILE_WRITE);
  if (f) {
    f.println("═══════════════════════════════════════════════════════════");
    f.println("TIPS & TRICKS");
    f.println("═══════════════════════════════════════════════════════════");
    f.println("");
    f.println("KEYBOARD SHORTCUTS:");
    f.println("  ^ button     = Shift (uppercase)");
    f.println("  123 button   = Numbers");
    f.println("  §$% button   = Special characters");
    f.println("  OK button    = Enter/Execute");
    f.println("  < button     = Backspace");
    f.println("");
    f.println("TERMINAL FEATURES:");
    f.println("  Scroll up/down: Use ▲▼ buttons on right");
    f.println("  ESC: Top-right corner");
    f.println("  Commands are case-insensitive");
    f.println("");
    f.println("CHIP-8 EMULATOR:");
    f.println("  Place .ch8 ROMs in root of SD card");
    f.println("  Keypad: 1-9, A-F on screen");
    f.println("  SPEED button changes emulation speed");
    f.println("");
    f.println("TROUBLESHOOTING:");
    f.println("  Screen freezes: Press RST button");
    f.println("  SD not detected: Check FAT32 format");
    f.println("  Touch not working: Check wiring");
    f.println("  I2C issues: Run 'i2cscan' first");
    f.close();
  }

  printToConsole(successPrefix + "Help system ready!", TFT_GREEN);
  printToConsole(infoPrefix + "Type 'help' for overview", TFT_BLUE);
}


// ==================== TEXT EDITOR MIT DIREKTER TASTATUR ====================
class TextEditor {
private:
  std::vector<String> lines;
  int cursorX = 0, cursorY = 0;
  int scrollX = 0, scrollY = 0;
  int viewWidth = 35;
  int viewHeight = 8;
  String filename;
  bool unsavedChanges = false;
  bool cursorVisible = true;
  unsigned long lastCursorBlink = 0;
  bool shiftPressed = false;
  bool capsLock = false;

  void drawEditor() {
    // Editor Bereich (oben, über der Tastatur)
    tft.fillRect(0, 35, 240, 130, BG_COLOR);

    // Titelzeile
    tft.fillRect(0, 35, 240, 20, ACCENT_COLOR);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, 40);
    tft.print("EDITOR: " + filename);
    if (unsavedChanges) tft.print(" *");

    tft.setCursor(160, 40);
    tft.print("L:" + String(cursorY + 1) + " C:" + String(cursorX));

    if (capsLock) {
      tft.setCursor(210, 40);
      tft.print("CAPS");
    }

    // Trennlinie
    tft.drawFastHLine(0, 55, 240, TEXT_COLOR);

    // Text anzeigen mit Scroll
    tft.setTextSize(1);

    for (int i = scrollY; i < min(scrollY + viewHeight, (int)lines.size()); i++) {
      int y = 65 + (i - scrollY) * 14;
      String displayLine = lines[i];

      // Horizontal scrollen
      if (scrollX > 0 && scrollX < (int)displayLine.length()) {
        displayLine = displayLine.substring(scrollX);
      }
      if (displayLine.length() > viewWidth) {
        displayLine = displayLine.substring(0, viewWidth);
      }

      // Zeilennummer
      tft.setTextColor(TFT_DARKGREY);
      tft.setCursor(5, y);
      tft.printf("%3d", i);

      // Cursor position berechnen
      if (i == cursorY) {
        int cursorPixelX = 45;
        for (int ci = 0; ci < cursorX - scrollX; ci++) {
          if (ci < (int)displayLine.length()) {
            cursorPixelX += tft.textWidth(String(displayLine[ci]));
          } else {
            cursorPixelX += 6;
          }
        }
        if (cursorVisible && cursorX >= scrollX && cursorX <= scrollX + viewWidth) {
          tft.drawFastVLine(cursorPixelX, y - 2, 12, TEXT_COLOR);
        }
      }

      // Text zeichnen
      tft.setTextColor(TEXT_COLOR, BG_COLOR);
      tft.setCursor(45, y);
      tft.print(displayLine);
    }

    // Editor Rahmen
    tft.drawRect(2, 62, 236, viewHeight * 14 + 5, TEXT_COLOR);

    // Scroll Buttons
    drawScrollButtons();
  }

  void drawScrollButtons() {
    // Scroll Up/Down/Left/Right Buttons rechts
    tft.fillRoundRect(215, 65, 20, 20, 4, BUTTON_COLOR);
    tft.drawCentreString("▲", 225, 70, 1);

    tft.fillRoundRect(215, 90, 20, 20, 4, BUTTON_COLOR);
    tft.drawCentreString("▼", 225, 95, 1);

    tft.fillRoundRect(215, 115, 20, 20, 4, BUTTON_COLOR);
    tft.drawCentreString("◀", 225, 120, 1);

    tft.fillRoundRect(215, 140, 20, 20, 4, BUTTON_COLOR);
    tft.drawCentreString("▶", 225, 145, 1);
  }

  void moveCursorUp() {
    if (cursorY > 0) {
      cursorY--;
      int newLen = lines[cursorY].length();
      if (cursorX > newLen) cursorX = newLen;
      if (cursorY < scrollY) scrollY = cursorY;
    }
  }

  void moveCursorDown() {
    if (cursorY < (int)lines.size() - 1) {
      cursorY++;
      int newLen = lines[cursorY].length();
      if (cursorX > newLen) cursorX = newLen;
      if (cursorY >= scrollY + viewHeight) scrollY = cursorY - viewHeight + 1;
    }
  }

  void moveCursorLeft() {
    if (cursorX > 0) {
      cursorX--;
      if (cursorX < scrollX) scrollX = cursorX;
    } else if (cursorY > 0) {
      cursorY--;
      cursorX = lines[cursorY].length();
      if (cursorY < scrollY) scrollY = cursorY;
      if (cursorX - scrollX > viewWidth) scrollX = cursorX - viewWidth + 1;
    }
  }

  void moveCursorRight() {
    if (cursorX < (int)lines[cursorY].length()) {
      cursorX++;
      if (cursorX >= scrollX + viewWidth) scrollX = cursorX - viewWidth + 1;
    } else if (cursorY < (int)lines.size() - 1) {
      cursorY++;
      cursorX = 0;
      if (cursorY >= scrollY + viewHeight) scrollY = cursorY - viewHeight + 1;
      scrollX = 0;
    }
  }

public:
  TextEditor() {
    lines.push_back("");
    cursorX = 0;
    cursorY = 0;
    scrollX = 0;
    scrollY = 0;
    unsavedChanges = false;
    shiftPressed = false;
    capsLock = false;
  }

  bool loadFile(String fname) {
    filename = fname;
    lines.clear();

    File f = SD.open("/" + filename, FILE_READ);
    if (f) {
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.replace("\r", "");
        lines.push_back(line);
      }
      f.close();
      if (lines.size() == 0) lines.push_back("");
      unsavedChanges = false;
      return true;
    } else {
      lines.push_back("");
      unsavedChanges = true;
      return false;
    }
  }

  void saveFile() {
    File f = SD.open("/" + filename, FILE_WRITE);
    if (f) {
      for (int i = 0; i < (int)lines.size(); i++) {
        f.println(lines[i]);
      }
      f.close();
      unsavedChanges = false;
      printToConsole(successPrefix + "Saved: " + filename, TFT_GREEN);
      playSysSound(1);
    } else {
      printToConsole(errorPrefix + "Cannot save file!", TFT_RED);
    }
  }

  void insertChar(char c) {
    // Großbuchstaben bei Shift oder CapsLock
    if (shiftPressed || capsLock) {
      if (c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
      }
    }

    String oldLine = lines[cursorY];
    String newLine = oldLine.substring(0, cursorX) + String(c) + oldLine.substring(cursorX);
    lines[cursorY] = newLine;
    cursorX++;
    unsavedChanges = true;

    if (cursorX >= scrollX + viewWidth) scrollX = cursorX - viewWidth + 1;
  }

  void deleteChar() {
    if (cursorX > 0) {
      String oldLine = lines[cursorY];
      String newLine = oldLine.substring(0, cursorX - 1) + oldLine.substring(cursorX);
      lines[cursorY] = newLine;
      cursorX--;
      unsavedChanges = true;
      if (cursorX < scrollX) scrollX = cursorX;
    } else if (cursorY > 0) {
      cursorX = lines[cursorY - 1].length();
      lines[cursorY - 1] += lines[cursorY];
      lines.erase(lines.begin() + cursorY);
      cursorY--;
      unsavedChanges = true;
      if (cursorY < scrollY) scrollY = cursorY;
      if (cursorX - scrollX > viewWidth) scrollX = cursorX - viewWidth + 1;
    }
  }

  void newLine() {
    String currentLine = lines[cursorY];
    String newLine = currentLine.substring(cursorX);
    lines[cursorY] = currentLine.substring(0, cursorX);
    lines.insert(lines.begin() + cursorY + 1, newLine);
    cursorY++;
    cursorX = 0;
    unsavedChanges = true;

    if (cursorY >= scrollY + viewHeight) scrollY = cursorY - viewHeight + 1;
    scrollX = 0;
  }

  void handleTouch(int tx, int ty) {
    // Scroll Up
    if (tx > 215 && tx < 235 && ty > 65 && ty < 85) {
      moveCursorUp();
      playSysSound(0);
    }
    // Scroll Down
    else if (tx > 215 && tx < 235 && ty > 90 && ty < 110) {
      moveCursorDown();
      playSysSound(0);
    }
    // Scroll Left
    else if (tx > 215 && tx < 235 && ty > 115 && ty < 135) {
      moveCursorLeft();
      playSysSound(0);
    }
    // Scroll Right
    else if (tx > 215 && tx < 235 && ty > 140 && ty < 160) {
      moveCursorRight();
      playSysSound(0);
    }
    // Textbereich - Cursor positionieren (optional)
    else if (tx < 210 && ty > 65 && ty < 170) {
      int line = scrollY + (ty - 65) / 14;
      if (line >= 0 && line < (int)lines.size()) {
        cursorY = line;
        int charPos = (tx - 45) / 7;
        if (charPos < 0) charPos = 0;
        if (charPos > (int)lines[cursorY].length()) charPos = lines[cursorY].length();
        cursorX = charPos + scrollX;
        if (cursorX > (int)lines[cursorY].length()) cursorX = lines[cursorY].length();
        playSysSound(0);
      }
    }
  }

  void run() {
    int oldKbMode = kbMode;
    String oldInput = currentInput;

    tft.fillScreen(BG_COLOR);
    drawEditor();
    drawKeyboard();  // Tastatur wird unten gezeichnet

    bool running = true;
    lastCursorBlink = millis();
    cursorVisible = true;

    while (running) {
      // Cursor blinken
      if (millis() - lastCursorBlink > 500) {
        cursorVisible = !cursorVisible;
        lastCursorBlink = millis();
        drawEditor();
      }

      int tx, ty;
      if (getTouch(tx, ty)) {
        // ESC Button (oben rechts)
        if (tx > 180 && ty < 35) {
          running = false;
        }
        // Speichern Button (oben links)
        else if (tx > 5 && tx < 55 && ty < 35) {
          if (filename != "") saveFile();
          delay(200);
        }
        // Editor Bereich
        else if (ty < 170) {
          handleTouch(tx, ty);
          drawEditor();
        }
        delay(100);
      }

      // DIREKTE Tastatureingabe - kein Buffer!
      String key = getDirectKeyPress();
      if (key != "") {
        if (key == "ESC_SIGNAL") {
          running = false;
        } else if (key == "ENTER") {
          newLine();
          drawEditor();
        } else if (key == "BACKSPACE") {
          deleteChar();
          drawEditor();
        } else if (key == "SPACE") {
          insertChar(' ');
          drawEditor();
        } else if (key == "SHIFT") {
          shiftPressed = !shiftPressed;
          drawKeyboard();
          drawEditor();
        } else if (key == "CAPS") {
          capsLock = !capsLock;
          drawKeyboard();
          drawEditor();
        } else if (key == "TAB") {
          insertChar(' ');
          insertChar(' ');
          insertChar(' ');
          insertChar(' ');
          drawEditor();
        } else if (key.length() == 1) {
          insertChar(key[0]);
          drawEditor();
        }
      }

      updateInputLine(false);
      delay(10);
    }

    if (unsavedChanges && filename != "") {
      printToConsole(infoPrefix + "Unsaved changes in " + filename, TFT_YELLOW);
    }

    // Zum Terminal zurück
    tft.fillScreen(BG_COLOR);
    drawKeyboard();
    drawScrollButtons();
    refreshTerminal();
    updateInputLine(true);

    kbMode = oldKbMode;
    currentInput = oldInput;
  }

  // Direkte Tastaturerkennung ohne Buffer
  String getDirectKeyPress() {
    int tx, ty;
    if (!getTouch(tx, ty)) return "";

    // ESC Button
    if (ty < 40 && tx > 180) {
      playSysSound(3);
      delay(200);
      return "ESC_SIGNAL";
    }

    // Speichern Button
    if (ty < 35 && tx > 5 && tx < 55) {
      playSysSound(0);
      delay(200);
      return "SAVE";
    }

    if (ty < 170) return "";

    playSysSound(0);

    // Erste Zeile
    if (ty >= 170 && ty < 205) {
      int keyIndex = tx / 24;
      if (keyIndex >= 0 && keyIndex < 10) {
        char c = keys[kbMode][0][keyIndex];
        if (c != '\0') return String(c);
      }
    }
    // Zweite Zeile
    else if (ty >= 205 && ty < 245) {
      int keyIndex = (tx - 12) / 24;
      if (keyIndex >= 0 && keyIndex < 9) {
        char c = keys[kbMode][1][keyIndex];
        if (c != '\0') return String(c);
      }
    }
    // Dritte Zeile
    else if (ty >= 245 && ty < 285) {
      if (tx < 38) {
        return "SHIFT";
      } else if (tx > 205) {
        return "BACKSPACE";
      } else {
        int keyIndex = (tx - 38) / 24;
        if (keyIndex >= 0 && keyIndex < 7) {
          char c = keys[kbMode][2][keyIndex];
          if (c != '\0') return String(c);
        }
      }
    }
    // Vierte Zeile
    else if (ty >= 285) {
      if (tx < 55) {
        return "CAPS";
      } else if (tx >= 55 && tx < 115) {
        return "SPECIAL";
      } else if (tx >= 115 && tx < 165) {
        return "SPACE";
      } else if (tx >= 165) {
        return "ENTER";
      }
    }

    return "";
  }
};

void textEditor() {
  TextEditor editor;

  tft.fillScreen(BG_COLOR);
  printToConsole(infoPrefix + "Enter filename to edit:", TFT_BLUE);
  printToConsole(infoPrefix + "(or press ESC to cancel)", TFT_BLUE);

  String fname = getTextInput();

  if (fname != "") {
    editor.loadFile(fname);
    editor.run();
  }

  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== MAIN SETUP ====================
void setup() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 10);
  tft.println("CYD OS BOOTING...");

  pinMode(SOUND_PIN, OUTPUT);
  playSysSound(2);

  EEPROM.begin(EEPROM_SIZE);
  soundEnabled = EEPROM.read(0);
  if (soundEnabled != 0 && soundEnabled != 1) soundEnabled = true;

  darkMode = EEPROM.read(1);
  if (darkMode != 0 && darkMode != 1) darkMode = false;

  EEPROM.get(10, cmdPrefix);
  EEPROM.get(50, promptPrefix);
  if (cmdPrefix.length() == 0 || cmdPrefix[0] == 0xFF) cmdPrefix = ">";
  if (promptPrefix.length() == 0 || promptPrefix[0] == 0xFF) promptPrefix = "> ";

  applyTheme();

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);
  tft.println("Touch OK");

  tft.println("SD init...");
  if (SD.begin(SD_CS)) {
    tft.println("SD OK");
    delay(500);
    writeHelpFile();
  } else {
    tft.println("SD FAILED!");
    delay(1000);
  }

  // KEINE I2C INITIALISIERUNG HIER!
  // Wire.begin wird nur in den I2C Funktionen selbst aufgerufen

  drawScrollButtons();
  drawKeyboard();
  updateInputLine(true);
  printToConsole(successPrefix + "CYD OS v3.0 Ready!", TFT_NAVY);
  printToConsole(infoPrefix + "Type 'help' for commands", TFT_BLUE);
  printToConsole(infoPrefix + "Type 'chip8' for Game Boy style emulator", TFT_GREEN);
  printToConsole(infoPrefix + "Type 'i2cscan' to find I2C devices", TFT_CYAN);
  randomSeed(analogRead(34));
}

void clearScreen() {
  terminalHistory.clear();
  scrollOffset = 0;
  tft.fillScreen(BG_COLOR);
  drawScrollButtons();
  drawKeyboard();
  refreshTerminal();
  updateInputLine(true);
}

void initNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printToConsole(infoPrefix + "NTP initialized", TFT_BLUE);
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time sync failed";
  }
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S - %d.%m.%Y", &timeinfo);
  return String(timeStr);
}

void showClock() {
  if (WiFi.status() != WL_CONNECTED) {
    printToConsole(errorPrefix + "WiFi not connected! Use 'wifi' first.", TFT_RED);
    return;
  }
  printToConsole(infoPrefix + "Fetching time from NTP...", TFT_BLUE);
  initNTP();
  int attempts = 0;
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
  }
  if (attempts < 10) {
    printToConsole(successPrefix + "Current time: " + getCurrentTime(), TFT_GREEN);
  } else {
    printToConsole(errorPrefix + "Time sync failed!", TFT_RED);
  }
}

// ==================== WIFI SCANNER FUNKTION ====================
void wifiScanner() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillScreen(BG_COLOR);

  // GUI Header
  tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("WiFi SCANNER", 120, 42, 2);

  // Status zeigen
  tft.setTextSize(1);
  tft.drawCentreString("Scanning networks...", 120, 75, 1);
  drawScrollButtons();

  // ESC Button
  tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
  tft.drawCentreString("ESC", 207, 10, 2);

  // Scan starten
  int n = WiFi.scanNetworks();

  if (n == 0) {
    tft.drawCentreString("No networks found!", 120, 100, 2);
  } else {
    // Ergebnisse anzeigen
    tft.fillRect(0, 60, 240, 260, BG_COLOR);
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(10, 65);
    tft.printf("Found %d networks:", n);

    int maxVisible = 12;
    int scrollOffset = 0;
    int selected = -1;
    bool running = true;
    unsigned long lastScroll = 0;

    while (running) {
      // Netzwerke mit Scroll anzeigen
      tft.fillRect(0, 80, 240, 200, BG_COLOR);

      for (int i = scrollOffset; i < min(scrollOffset + maxVisible, n); i++) {
        int y = 85 + (i - scrollOffset) * 18;

        // Auswahl hervorheben
        if (i == selected) {
          tft.fillRoundRect(5, y - 2, 230, 16, 3, ACCENT_COLOR);
          tft.setTextColor(TFT_WHITE);
        } else {
          tft.setTextColor(TEXT_COLOR);
        }

        // Signalstärke als Balken
        int rssi = WiFi.RSSI(i);
        int barCount = constrain(map(rssi, -90, -30, 1, 4), 1, 4);
        String bars = "";
        for (int b = 0; b < barCount; b++) bars += "█";
        for (int b = barCount; b < 4; b++) bars += "░";

        // Verschlüsselung anzeigen
        String enc = "";
        switch (WiFi.encryptionType(i)) {
          case WIFI_AUTH_OPEN: enc = "🔓"; break;
          case WIFI_AUTH_WEP: enc = "WEP"; break;
          case WIFI_AUTH_WPA_PSK: enc = "WPA"; break;
          case WIFI_AUTH_WPA2_PSK: enc = "WPA2"; break;
          case WIFI_AUTH_WPA_WPA2_PSK: enc = "WPA/WPA2"; break;
          default: enc = "🔒"; break;
        }

        // Netzwerk anzeigen
        tft.setCursor(10, y);
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<Hidden SSID>";
        if (ssid.length() > 18) ssid = ssid.substring(0, 15) + "...";

        tft.printf("%2d. %s", i + 1, ssid.c_str());
        tft.setCursor(180, y);
        tft.printf("%s %3ddBm", bars.c_str(), rssi);
        tft.setCursor(220, y);
        tft.print(enc);
      }

      // Scroll Indikatoren
      if (scrollOffset > 0) {
        tft.fillTriangle(120, 82, 130, 76, 110, 76, ACCENT_COLOR);
      }
      if (scrollOffset + maxVisible < n) {
        tft.fillTriangle(120, 275, 130, 281, 110, 281, ACCENT_COLOR);
      }

      // Buttons
      tft.fillRoundRect(10, 295, 70, 25, 4, SUCCESS_COLOR);
      tft.drawCentreString("CONNECT", 45, 302, 1);

      tft.fillRoundRect(85, 295, 70, 25, 4, TFT_BLUE);
      tft.drawCentreString("RESCAN", 120, 302, 1);

      tft.fillRoundRect(160, 295, 70, 25, 4, WARNING_COLOR);
      tft.drawCentreString("BACK", 195, 302, 1);

      // Touch Handling
      int tx, ty;
      if (getTouch(tx, ty)) {
        // ESC Button oben rechts
        if (tx > 180 && ty < 40) {
          running = false;
        }
        // Scroll Up
        else if (tx > 110 && tx < 130 && ty > 76 && ty < 82 && scrollOffset > 0) {
          scrollOffset--;
          playSysSound(0);
          delay(100);
        }
        // Scroll Down
        else if (tx > 110 && tx < 130 && ty > 275 && ty < 281 && scrollOffset + maxVisible < n) {
          scrollOffset++;
          playSysSound(0);
          delay(100);
        }
        // Netzwerk auswählen (Tippen im Bereich)
        else if (ty > 80 && ty < 280) {
          int idx = scrollOffset + ((ty - 80) / 18);
          if (idx >= 0 && idx < n) {
            selected = idx;
            playSysSound(0);
            delay(100);
          }
        }
        // CONNECT Button
        else if (ty > 295 && ty < 320) {
          if (tx < 80 && selected >= 0) {
            // Verbindung zum ausgewählten Netzwerk
            String ssid = WiFi.SSID(selected);
            String encryption = (WiFi.encryptionType(selected) == WIFI_AUTH_OPEN) ? "open" : "secure";

            printToConsole(infoPrefix + "Connecting to: " + ssid, TFT_BLUE);

            if (WiFi.encryptionType(selected) == WIFI_AUTH_OPEN) {
              // Offenes Netzwerk
              WiFi.begin(ssid.c_str());
            } else {
              // Passwort abfragen
              printToConsole(infoPrefix + "Enter password:", TFT_BLUE);
              String pwd = getTextInput();
              if (pwd != "") {
                WiFi.begin(ssid.c_str(), pwd.c_str());
              } else {
                printToConsole(errorPrefix + "Password required!", TFT_RED);
                continue;
              }
            }

            // Verbindungsstatus anzeigen
            tft.fillRect(0, 35, 240, 50, BG_COLOR);
            tft.setTextColor(TFT_CYAN);
            tft.drawCentreString("Connecting to " + ssid, 120, 45, 1);
            tft.drawCentreString("Please wait...", 120, 60, 1);

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
              delay(500);
              attempts++;
              tft.fillCircle(120 + (attempts % 5) * 10, 75, 2, SUCCESS_COLOR);
            }

            if (WiFi.status() == WL_CONNECTED) {
              printToConsole(successPrefix + "Connected! IP: " + WiFi.localIP().toString(), TFT_GREEN);
              playSysSound(1);

              // Credentials speichern
              EEPROM.put(150, ssid);
              EEPROM.put(200, WiFi.psk());
              EEPROM.commit();

              delay(2000);
            } else {
              printToConsole(errorPrefix + "Connection failed!", TFT_RED);
              playSysSound(3);
              delay(1500);
            }

            // Neu zeichnen
            tft.fillScreen(BG_COLOR);
            tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
            tft.drawCentreString("WiFi SCANNER", 120, 42, 2);
            drawScrollButtons();
            tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
            tft.drawCentreString("ESC", 207, 10, 2);
          }
        }
        // RESCAN Button
        else if (tx > 85 && tx < 155 && ty > 295 && ty < 320) {
          printToConsole(infoPrefix + "Rescanning...", TFT_BLUE);
          WiFi.scanDelete();
          n = WiFi.scanNetworks();
          selected = -1;
          scrollOffset = 0;
          playSysSound(0);
          delay(200);
        }
        // BACK Button
        else if (tx > 160 && ty > 295 && ty < 320) {
          running = false;
        }
      }

      delay(20);
    }
  }

  WiFi.scanDelete();

  kbMode = oldKbMode;
  currentInput = oldInput;
  tft.fillScreen(BG_COLOR);
  drawKeyboard();
  drawScrollButtons();
  refreshTerminal();
  updateInputLine(true);
}

// ==================== VOLLSTÄNDIGE LOOP() OHNE HUE ====================
void loop() {
  // Cursor blinken lassen
  if (millis() - lastCursorBlink > 500) {
    cursorVisible = !cursorVisible;
    lastCursorBlink = millis();
    updateInputLine(false);
  }

  String cmd = handleKeyboardInput();
  if (cmd != "" && cmd != "ESC_SIGNAL") {
    String lowCmd = cmd;
    lowCmd.toLowerCase();

    // ==================== HILFE-SYSTEM ====================
    if (lowCmd == "help" || lowCmd == "?") {
      writeHelpFile();
      File f = SD.open("/help.txt");
      if (f) {
        while (f.available()) {
          String line = f.readStringUntil('\n');
          line.replace("\r", "");
          if (line.startsWith("╔") || line.startsWith("║") || line.startsWith("╚")) {
            printToConsole(line, TFT_CYAN);
          } else if (line.startsWith("AVAILABLE") || line.startsWith("┌") || line.startsWith("├") || line.startsWith("└") || line.startsWith("│")) {
            printToConsole(line, TFT_YELLOW);
          } else {
            printToConsole(line);
          }
        }
        f.close();
      } else {
        printToConsole(errorPrefix + "Help file missing!", TFT_RED);
      }
    }

    // Kategorie-Hilfe
    else if (lowCmd.startsWith("help ")) {
      String category = lowCmd.substring(5);
      String helpFile = "/help_";

      if (category == "system" || category == "sys") helpFile += "sys.txt";
      else if (category == "files" || category == "file") helpFile += "files.txt";
      else if (category == "net" || category == "network" || category == "wifi") helpFile += "net.txt";
      else if (category == "games" || category == "game" || category == "fun") helpFile += "games.txt";
      else if (category == "apps" || category == "app" || category == "applications") helpFile += "apps.txt";
      else if (category == "i2c" || category == "i2cbus") helpFile += "i2c.txt";
      else if (category == "dev" || category == "developer" || category == "programming") helpFile += "dev.txt";
      else if (category == "tips" || category == "tricks" || category == "tutorial") helpFile += "tips.txt";
      else if (category == "all" || category == "full" || category == "complete") {
        String helpFiles[] = { "help.txt", "help_sys.txt", "help_files.txt", "help_net.txt",
                               "help_games.txt", "help_apps.txt", "help_i2c.txt", "help_dev.txt", "help_tips.txt" };
        for (String hf : helpFiles) {
          printToConsole("");
          printToConsole("════════════════════════════════", TFT_CYAN);
          File f = SD.open("/" + hf);
          if (f) {
            while (f.available()) {
              String line = f.readStringUntil('\n');
              line.replace("\r", "");
              printToConsole(line);
            }
            f.close();
          }
          delay(100);
        }
        helpFile = "";
      } else {
        printToConsole(errorPrefix + "Unknown category: " + category, TFT_RED);
        printToConsole(infoPrefix + "Try: system, files, net, games, apps, i2c, dev, tips, all", TFT_BLUE);
        helpFile = "";
      }

      if (helpFile != "") {
        File f = SD.open(helpFile);
        if (f) {
          while (f.available()) {
            String line = f.readStringUntil('\n');
            line.replace("\r", "");
            if (line.startsWith("══════")) printToConsole(line, TFT_CYAN);
            else if (line.indexOf("Usage:") >= 0) printToConsole(line, TFT_GREEN);
            else printToConsole(line);
          }
          f.close();
        } else {
          printToConsole(errorPrefix + "Help file not found!", TFT_RED);
        }
      }
    }

    // Man-Page System
    else if (lowCmd.startsWith("man ")) {
      String searchTerm = lowCmd.substring(4);
      String helpFiles[] = { "help_sys.txt", "help_files.txt", "help_net.txt",
                             "help_games.txt", "help_apps.txt", "help_i2c.txt", "help_dev.txt" };
      bool found = false;

      for (String hf : helpFiles) {
        File f = SD.open("/" + hf);
        if (f) {
          while (f.available()) {
            String line = f.readStringUntil('\n');
            line.replace("\r", "");

            if (line.startsWith(searchTerm) || line.indexOf(searchTerm + " ") >= 0) {
              if (!found) {
                printToConsole("");
                printToConsole("MANUAL: " + searchTerm, TFT_CYAN);
                printToConsole("═══════════════════════════════", TFT_CYAN);
              }
              found = true;
              printToConsole(line, TFT_GREEN);
            }
          }
          f.close();
        }
      }

      if (!found) {
        printToConsole(errorPrefix + "No manual entry for: " + searchTerm, TFT_RED);
        printToConsole(infoPrefix + "Try 'help' for available commands", TFT_BLUE);
      }
    }

    // ==================== SYSTEM BEFEHLE ====================
    else if (lowCmd == "cls" || lowCmd == "clear") {
      clearScreen();
    }

    else if (lowCmd == "reboot" || lowCmd == "restart") {
      printToConsole("Rebooting...", TFT_RED);
      playSysSound(2);
      delay(500);
      ESP.restart();
    }

    else if (lowCmd == "date") {
      unsigned long seconds = millis() / 1000;
      int days = seconds / 86400;
      int hours = (seconds % 86400) / 3600;
      int mins = (seconds % 3600) / 60;
      int secs = seconds % 60;
      char buf[50];
      sprintf(buf, "Boot time: %d days, %02d:%02d:%02d", days, hours, mins, secs);
      printToConsole(buf);
    }

    else if (lowCmd == "uptime") {
      unsigned long uptime = millis() / 1000;
      int hours = uptime / 3600;
      int mins = (uptime % 3600) / 60;
      int secs = uptime % 60;
      char buf[40];
      sprintf(buf, "Uptime: %02d:%02d:%02d", hours, mins, secs);
      printToConsole(buf, TFT_GREEN);
    }

    else if (lowCmd == "free") {
      printToConsole("Memory Information:", TFT_CYAN);
      printToConsole("  RAM Free:  " + String(ESP.getFreeHeap()) + " bytes");
      printToConsole("  Heap Total: " + String(ESP.getHeapSize()) + " bytes");
      printToConsole("  Min Free:  " + String(ESP.getMinFreeHeap()) + " bytes");
      printToConsole("  Flash:     " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
    }

    else if (lowCmd == "ps") {
      printToConsole("PID  STATE     COMMAND", TFT_CYAN);
      printToConsole("────────────────────────────");
      printToConsole("1    running   terminal");
      printToConsole("2    running   touchdriver");
      printToConsole("3    sleeping  idle");
      printToConsole("4    waiting   watchdog");
    }

    else if (lowCmd == "neofetch") {
      printToConsole("", TFT_CYAN);
      printToConsole("   CCCCC   YY   YY   TTTTTTT", TFT_YELLOW);
      printToConsole("  CC   CC   YY YY       TT  ", TFT_YELLOW);
      printToConsole("  CC         YYY        TT  ", TFT_YELLOW);
      printToConsole("  CC         YY         TT  ", TFT_YELLOW);
      printToConsole("  CC   CC   YY          TT  ", TFT_YELLOW);
      printToConsole("   CCCCC   YY           TT  ", TFT_YELLOW);
      printToConsole("");
      printToConsole("OS: CYD Terminal OS v3.0", TFT_GREEN);
      printToConsole("Host: ESP32-WROOM @ 240MHz", TFT_GREEN);
      printToConsole("RAM: " + String(ESP.getFreeHeap() / 1024) + "KB / " + String(ESP.getHeapSize() / 1024) + "KB", TFT_GREEN);
      printToConsole("Theme: " + String(darkMode ? "Dark" : "Light"), TFT_GREEN);
      printToConsole("Sound: " + String(soundEnabled ? "ON" : "OFF"), TFT_GREEN);
    }

    else if (lowCmd == "sysinfo" || lowCmd == "stats") {
      sysInfo();
    }

    else if (lowCmd == "sd" || lowCmd == "storage") {
      storageInfo();
    }

    else if (lowCmd == "darkmode" || lowCmd == "theme") {
      darkMode = !darkMode;
      applyTheme();
      printToConsole(successPrefix + "Dark Mode: " + String(darkMode ? "ON" : "OFF"), TFT_GREEN);
      EEPROM.write(1, darkMode);
      EEPROM.commit();
    }

    else if (lowCmd == "sound") {
      soundEnabled = !soundEnabled;
      printToConsole(successPrefix + "Sound: " + String(soundEnabled ? "ON" : "OFF"), TFT_GREEN);
      if (soundEnabled) playSysSound(1);
      EEPROM.write(0, soundEnabled);
      EEPROM.commit();
    }

    else if (lowCmd == "colors") {
      printToConsole("Available Colors:", TFT_CYAN);
      printToConsole("black, navy, dkgreen, dkcyan");
      printToConsole("maroon, purple, olive, grey");
      printToConsole("dkgrey, blue, green, cyan");
      printToConsole("red, magenta, yellow, white");
      printToConsole("orange, lime, pink, brown");
      printToConsole("gold, silver, sky, violet");
      printToConsole("coral, mint, sand, salmon");
    }

    // ==================== CLOCK / ZEIT ====================
    else if (lowCmd == "clock" || lowCmd == "time" || lowCmd == "zeit") {
      showClock();
    }

    // ==================== ECHO ====================
    else if (lowCmd.startsWith("echo ")) {
      String msg = cmd.substring(5);
      msg.replace("$TIME", String(millis()));
      msg.replace("$FREE", String(ESP.getFreeHeap()));
      msg.replace("$UPTIME", String(millis() / 1000));
      printToConsole(msg);
    }

    // ==================== DATEIOPERATIONEN ====================
    else if (lowCmd == "ls" || lowCmd == "dir") {
      std::vector<String> f, d;
      listDirectory("/", f, d);
      printToConsole("Directory: /", TFT_CYAN);
      for (String dir : d) printToConsole("  [DIR]  " + dir, TFT_GREEN);
      for (String file : f) printToConsole("  [FILE] " + file);
      printToConsole(String(d.size()) + " dirs, " + String(f.size()) + " files");
    }

    else if (lowCmd.startsWith("ls ")) {
      String path = lowCmd.substring(3);
      std::vector<String> f, d;
      listDirectory(path, f, d);
      printToConsole("Directory: " + path, TFT_CYAN);
      for (String dir : d) printToConsole("  [DIR]  " + dir, TFT_GREEN);
      for (String file : f) printToConsole("  [FILE] " + file);
      printToConsole(String(d.size()) + " dirs, " + String(f.size()) + " files");
    }

    else if (lowCmd.startsWith("cat ")) {
      String filename = lowCmd.substring(4);
      File f = SD.open(filename);
      if (f) {
        printToConsole("File: " + filename, TFT_CYAN);
        int lines = 0;
        while (f.available() && lines < 20) {
          String line = f.readStringUntil('\n');
          line.replace("\r", "");
          printToConsole(line);
          lines++;
        }
        if (f.available()) printToConsole("... (use head/tail for large files)");
        f.close();
      } else {
        printToConsole(errorPrefix + "File not found: " + filename, TFT_RED);
      }
    }

    else if (lowCmd.startsWith("head ")) {
      int lines = 10;
      String rest = lowCmd.substring(5);
      String filename;

      if (rest.startsWith("-n")) {
        int spacePos = rest.indexOf(' ');
        lines = rest.substring(2, spacePos).toInt();
        filename = rest.substring(spacePos + 1);
      } else {
        filename = rest;
      }

      File f = SD.open(filename);
      if (f) {
        printToConsole("First " + String(lines) + " lines of " + filename + ":", TFT_CYAN);
        for (int i = 0; i < lines && f.available(); i++) {
          printToConsole(f.readStringUntil('\n'));
        }
        f.close();
      } else {
        printToConsole(errorPrefix + "File not found", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("tail ")) {
      int lines = 10;
      String rest = lowCmd.substring(5);
      String filename = rest;

      if (rest.startsWith("-n")) {
        int spacePos = rest.indexOf(' ');
        lines = rest.substring(2, spacePos).toInt();
        filename = rest.substring(spacePos + 1);
      }

      std::vector<String> allLines;
      File f = SD.open(filename);
      if (f) {
        while (f.available()) allLines.push_back(f.readStringUntil('\n'));
        f.close();

        printToConsole("Last " + String(lines) + " lines of " + filename + ":", TFT_CYAN);
        int start = max(0, (int)allLines.size() - lines);
        for (int i = start; i < allLines.size(); i++) {
          printToConsole(allLines[i]);
        }
      } else {
        printToConsole(errorPrefix + "File not found", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("grep ")) {
      String rest = lowCmd.substring(5);
      int firstQuote = rest.indexOf('"');
      int secondQuote = rest.indexOf('"', firstQuote + 1);

      if (firstQuote >= 0 && secondQuote >= 0) {
        String pattern = rest.substring(firstQuote + 1, secondQuote);
        String filename = rest.substring(secondQuote + 2);

        File f = SD.open(filename);
        if (f) {
          printToConsole("Searching for '" + pattern + "' in " + filename + ":", TFT_CYAN);
          int matches = 0;
          while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.indexOf(pattern) >= 0) {
              printToConsole(line);
              matches++;
            }
          }
          printToConsole(String(matches) + " matches found");
          f.close();
        } else {
          printToConsole(errorPrefix + "File not found", TFT_RED);
        }
      } else {
        printToConsole(errorPrefix + "Usage: grep \"pattern\" filename", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("rm ")) {
      String path = lowCmd.substring(3);
      deleteFileOrDir(path);
    }

    else if (lowCmd.startsWith("touch ")) {
      String filename = lowCmd.substring(6);
      File f = SD.open(filename, FILE_WRITE);
      if (f) {
        f.println("");
        f.close();
        printToConsole(successPrefix + "Created: " + filename, TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Cannot create file", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("mkdir ")) {
      String dirname = lowCmd.substring(6);
      if (SD.mkdir(dirname)) {
        printToConsole(successPrefix + "Created directory: " + dirname, TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Cannot create directory", TFT_RED);
      }
    }

    // ==================== NETZWERK & WIFI ====================
    else if (lowCmd == "ifconfig" || lowCmd == "ip") {
      printToConsole("Network Configuration:", TFT_CYAN);
      if (WiFi.status() == WL_CONNECTED) {
        printToConsole("  Status:  Connected", TFT_GREEN);
        printToConsole("  IP:      " + WiFi.localIP().toString());
        printToConsole("  MAC:     " + WiFi.macAddress());
        printToConsole("  RSSI:    " + String(WiFi.RSSI()) + " dBm");
        printToConsole("  Gateway: " + WiFi.gatewayIP().toString());
        printToConsole("  DNS:     " + WiFi.dnsIP().toString());
      } else {
        printToConsole("  Status: Disconnected", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("ping ")) {
      String host = lowCmd.substring(5);
      printToConsole("PING " + host + ":", TFT_CYAN);
      WiFiClient client;
      int success = 0;

      for (int i = 0; i < 4; i++) {
        unsigned long start = micros();
        if (client.connect(host.c_str(), 80)) {
          unsigned long respTime = (micros() - start) / 1000;
          printToConsole("  64 bytes: time=" + String(respTime) + "ms");
          client.stop();
          success++;
        } else {
          printToConsole("  Request timeout", TFT_RED);
        }
        if (i < 3) delay(1000);
      }
      printToConsole("--- " + host + " ping statistics ---");
      printToConsole(String(success) + " packets received, " + String(100 - success * 25) + "% loss");
    }

    else if (lowCmd.startsWith("wget ")) {
      String url = lowCmd.substring(5);
      printToConsole(infoPrefix + "Downloading " + url + "...", TFT_BLUE);

      if (WiFi.status() != WL_CONNECTED) {
        printToConsole(errorPrefix + "WiFi not connected!", TFT_RED);
      } else {
        WiFiClient client;
        String host = url;
        String path = "/";

        if (host.indexOf("http://") == 0) host = host.substring(7);
        if (host.indexOf('/') > 0) {
          path = host.substring(host.indexOf('/'));
          host = host.substring(0, host.indexOf('/'));
        }

        if (client.connect(host.c_str(), 80)) {
          client.print("GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n");

          String filename = "download.txt";
          if (path.lastIndexOf('/') >= 0 && path.lastIndexOf('/') < path.length() - 1) {
            filename = path.substring(path.lastIndexOf('/') + 1);
          }

          File f = SD.open("/" + filename, FILE_WRITE);
          bool headersDone = false;

          while (client.connected()) {
            if (client.available()) {
              String line = client.readStringUntil('\n');
              if (line == "\r") {
                headersDone = true;
              } else if (headersDone && f) {
                f.print(line);
              }
            }
          }
          client.stop();
          if (f) f.close();
          printToConsole(successPrefix + "Downloaded to /" + filename, TFT_GREEN);
        } else {
          printToConsole(errorPrefix + "Connection failed!", TFT_RED);
        }
      }
    }

    else if (lowCmd == "wifi") {
      wifiManager();
    }

    // ==================== WIFI SCANNER BEFEHLE ====================
    else if (lowCmd == "scan" || lowCmd == "wifi-scan" || lowCmd == "networks") {
      wifiScanner();
    }

    else if (lowCmd == "wifistatus" || lowCmd == "wifi-status") {
      printWiFiStatus();
    }

    else if (lowCmd == "wifioff" || lowCmd == "wifi-off") {
      WiFi.disconnect(true);
      printToConsole(infoPrefix + "WiFi disconnected", TFT_YELLOW);
    }

    else if (lowCmd == "wifiinfo") {
      if (WiFi.status() == WL_CONNECTED) {
        printToConsole("Connected to: " + WiFi.SSID(), TFT_GREEN);
        printToConsole("Signal: " + String(WiFi.RSSI()) + " dBm", TFT_GREEN);
        printToConsole("IP: " + WiFi.localIP().toString(), TFT_GREEN);
        printToConsole("MAC: " + WiFi.macAddress(), TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Not connected", TFT_RED);
      }
    }

    else if (lowCmd == "autoconnect") {
      autoConnectWiFi();
    }

    // ==================== SPIELE & UNTERHALTUNG ====================
    else if (lowCmd == "random") {
      randomSeed(millis());
      printToConsole("Random: " + String(random(1000)), TFT_GREEN);
    }

    else if (lowCmd.startsWith("dice ")) {
      int sides = lowCmd.substring(5).toInt();
      if (sides > 0 && sides <= 100) {
        printToConsole("🎲 Rolling d" + String(sides) + "... " + String(random(1, sides + 1)), TFT_GREEN);
        playSysSound(0);
      } else {
        printToConsole(errorPrefix + "Use: dice <1-100>", TFT_RED);
      }
    }

    else if (lowCmd == "snake") {
      snakeGame();
    }

    else if (lowCmd == "pong") {
      pongGame();
    }

    else if (lowCmd == "tictac" || lowCmd == "ttt") {
      ticTacToe();
    }

    // ==================== APPLIKATIONEN ====================
    else if (lowCmd == "chip8" || lowCmd == "chip-8") {
      chip8Emulator();
    }

    else if (lowCmd == "calc" || lowCmd == "rechner") {
      calculator();
    }

    else if (lowCmd == "files" || lowCmd == "filemanager" || lowCmd == "fm") {
      fileManager();
    }

    else if (lowCmd == "draw" || lowCmd == "paint") {
      drawingApp();
    }

    else if (lowCmd == "notes") {
      notesApp();
    }

    else if (lowCmd == "todo") {
      todoApp();
    }

    else if (lowCmd == "timer") {
      timerApp();
    }

    else if (lowCmd == "chat") {
      chatApp(false);
    }

    else if (lowCmd == "settings" || lowCmd == "einstellungen") {
      settingsMenu();
    }

    else if (lowCmd == "edit" || lowCmd == "editor" || lowCmd == "nano") {
      textEditor();
    }

    // ==================== NEUE APPS ====================
    else if (lowCmd == "calendar" || lowCmd == "kalender") {
      calendarApp();
    }

    else if (lowCmd == "periodic" || lowCmd == "ptable" || lowCmd == "elements" || lowCmd == "periodensystem") {
      periodicTableApp();
    }

    else if (lowCmd == "qr" || lowCmd == "qrcode") {
      qrGeneratorApp();
    }

    // ==================== I2C BEFEHLE ====================
    else if (lowCmd == "i2c" || lowCmd == "i2cscan") {
      i2cScanner();
    }

    else if (lowCmd == "i2ctool") {
      i2cTool();
    }

    else if (lowCmd.startsWith("i2csend ")) {
      String rest = lowCmd.substring(8);
      int spacePos = rest.indexOf(' ');
      if (spacePos > 0) {
        String addr = rest.substring(0, spacePos);
        String data = rest.substring(spacePos + 1);
        i2cSend(addr, data);
      } else {
        printToConsole(errorPrefix + "Usage: i2csend <addr> <data>", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("i2cread ")) {
      String rest = lowCmd.substring(8);
      int spacePos = rest.indexOf(' ');
      if (spacePos > 0) {
        String addr = rest.substring(0, spacePos);
        String bytes = rest.substring(spacePos + 1);
        i2cRead(addr, bytes.toInt());
      } else {
        printToConsole(errorPrefix + "Usage: i2cread <addr> <bytes>", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("i2cwrite ")) {
      String rest = lowCmd.substring(9);
      int firstSpace = rest.indexOf(' ');
      int secondSpace = rest.indexOf(' ', firstSpace + 1);
      if (firstSpace > 0 && secondSpace > 0) {
        String addr = rest.substring(0, firstSpace);
        String reg = rest.substring(firstSpace + 1, secondSpace);
        String data = rest.substring(secondSpace + 1);
        i2cWriteReg(addr, reg, data);
      } else {
        printToConsole(errorPrefix + "Usage: i2cwrite <addr> <reg> <data>", TFT_RED);
      }
    }

    // ==================== ENTWICKLER-TOOLS ====================
    else if (lowCmd.startsWith("eval ")) {
      String expr = lowCmd.substring(5);
      String result = evaluateExpression(expr);
      if (result != "Error") {
        printToConsole(expr + " = " + result, TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Cannot evaluate: " + expr, TFT_RED);
      }
    }

    else if (lowCmd.startsWith("delay ") || lowCmd.startsWith("sleep ")) {
      int ms = lowCmd.substring(lowCmd.indexOf(' ') + 1).toInt();
      if (ms > 0 && ms <= 10000) {
        printToConsole(infoPrefix + "Waiting " + String(ms) + "ms...", TFT_BLUE);
        delay(ms);
        printToConsole(successPrefix + "Done", TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Use: delay <1-10000>", TFT_RED);
      }
    }

    // ==================== FILE AUSFÜHREN (Falls Dateiname) ====================
    else {
      File f = SD.open(cmd);
      if (f) {
        printToConsole("File: " + cmd, TFT_CYAN);
        int lines = 0;
        while (f.available() && lines < 30) {
          printToConsole(f.readStringUntil('\n'));
          lines++;
        }
        if (f.available()) printToConsole("... (truncated, use 'cat' for full view)");
        f.close();
      } else {
        printToConsole(errorPrefix + "Command not found: " + cmd, TFT_RED);
        printToConsole(infoPrefix + "Type 'help' for available commands", TFT_BLUE);
        playSysSound(3);
      }
    }

    updateInputLine(false);
  }
}

// ==================== AUTO-CONNECT FUNKTION ====================
void autoConnectWiFi() {
  // Gespeicherte Credentials laden
  String savedSSID = "";
  String savedPWD = "";

  for (int i = 0; i < 32; i++) {
    char c = EEPROM.read(150 + i);
    if (c == 0xFF) break;
    if (c != 0) savedSSID += c;
  }

  for (int i = 0; i < 64; i++) {
    char c = EEPROM.read(200 + i);
    if (c == 0xFF) break;
    if (c != 0) savedPWD += c;
  }

  if (savedSSID.length() > 0 && savedSSID[0] != 0xFF && savedSSID.length() < 32) {
    printToConsole(infoPrefix + "Auto-connecting to " + savedSSID, TFT_BLUE);
    WiFi.begin(savedSSID.c_str(), savedPWD.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      printToConsole(successPrefix + "Auto-connected! IP: " + WiFi.localIP().toString(), TFT_GREEN);
      initNTP();
    } else {
      printToConsole(errorPrefix + "Auto-connect failed", TFT_RED);
    }
  } else {
    printToConsole(infoPrefix + "No saved network found. Use 'wifi' or 'scan' first.", TFT_BLUE);
  }
}

// ==================== WIFI STATUS DETAILS ====================
void printWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    printToConsole("╔════════════════════════════════╗", TFT_CYAN);
    printToConsole("║        WIFI STATUS             ║", TFT_CYAN);
    printToConsole("╠════════════════════════════════╣", TFT_CYAN);
    printToConsole("║ SSID:      " + WiFi.SSID(), TFT_GREEN);
    printToConsole("║ Signal:    " + String(WiFi.RSSI()) + " dBm",
                   WiFi.RSSI() > -50 ? TFT_GREEN : (WiFi.RSSI() > -70 ? TFT_YELLOW : TFT_RED));
    printToConsole("║ IP:        " + WiFi.localIP().toString(), TFT_GREEN);
    printToConsole("║ Gateway:   " + WiFi.gatewayIP().toString(), TFT_GREEN);
    printToConsole("║ Subnet:    " + WiFi.subnetMask().toString(), TFT_GREEN);
    printToConsole("║ DNS:       " + WiFi.dnsIP().toString(), TFT_GREEN);
    printToConsole("║ MAC:       " + WiFi.macAddress(), TFT_GREEN);

    // Signalstärke als Balken
    int rssi = WiFi.RSSI();
    int quality = constrain(map(rssi, -90, -30, 0, 100), 0, 100);
    String bar = "";
    for (int i = 0; i < quality / 10; i++) bar += "█";
    for (int i = quality / 10; i < 10; i++) bar += "░";
    printToConsole("║ Quality:   " + bar + " " + String(quality) + "%", TFT_GREEN);

    printToConsole("╚════════════════════════════════╝", TFT_CYAN);
  } else {
    printToConsole(errorPrefix + "WiFi not connected!", TFT_RED);
    printToConsole(infoPrefix + "Use 'scan' to find networks or 'wifi' to connect", TFT_BLUE);
  }
}