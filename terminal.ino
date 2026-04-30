#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <EEPROM.h>
#include <FS.h>

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
// ==================== ERWEITERTER CHIP-8 EMULATOR MIT SCHIP ====================
class Chip8Emulator {
private:
  // Erweiterter Speicher für SCHIP (64KB)
  uint8_t memory[65536];  // 64KB statt 4KB
  uint8_t V[16];
  uint16_t I;
  uint16_t pc;
  uint16_t stack[16];
  uint8_t sp;
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint8_t keypad[16];
  
  // SCHIP: Erweiterte Display-Größe (128x64 statt 64x32)
  uint8_t display[128 * 64];
  bool drawFlag;
  bool running;
  bool schipMode;  // SCHIP Modus aktiv?
  unsigned long lastTimerUpdate;
  unsigned long lastKeyRelease;
  String romName;
  uint32_t cycleCount;
  
  // SCHIP: Hi-Res Flag und Scroll-Register
  bool hiresMode;
  uint8_t scrollX, scrollY;
  
  // SCHIP: Erweiterte Fonts (8x10 für Hi-Res)
  const uint8_t schipFontset[180] = {
    // Normale CHIP-8 Fonts (4x5)
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
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
    
    // SCHIP Hi-Res Fonts (8x10)
    0x3C, 0x66, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0x66, 0x3C,  // 0
    0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E,  // 1
    0x3C, 0x66, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF,  // 2
    0x3C, 0x66, 0x03, 0x03, 0x1E, 0x03, 0x03, 0x03, 0x66, 0x3C,  // 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0x06, 0x06, 0x06,  // 4
    0x7E, 0x60, 0x60, 0x7C, 0x06, 0x03, 0x03, 0x03, 0x66, 0x3C,  // 5
    0x1C, 0x30, 0x60, 0x60, 0x7C, 0x66, 0x63, 0x63, 0x66, 0x3C,  // 6
    0x7E, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30,  // 7
    0x3C, 0x66, 0xC3, 0xC3, 0x66, 0x66, 0xC3, 0xC3, 0x66, 0x3C,  // 8
    0x3C, 0x66, 0xC3, 0xC3, 0xCF, 0x3F, 0x03, 0x03, 0x03, 0x3E   // 9
  };

public:
  Chip8Emulator() {
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
    schipMode = false;
    hiresMode = false;
    scrollX = scrollY = 0;
    cycleCount = 0;
    romName = "";

    // Lade normale CHIP-8 Fonts
    for (int i = 0; i < 80; i++) {
      memory[i] = schipFontset[i];
    }

    lastTimerUpdate = millis();
    lastKeyRelease = millis();
  }

  void enableSCHIPMode() {
    schipMode = true;
    hiresMode = true;
    
    // Lade SCHIP Hi-Res Fonts an Position 0x50
    for (int i = 0; i < 160; i++) {
      memory[0x50 + i] = schipFontset[80 + i];
    }
    
    Serial.println("Super-CHIP mode enabled!");
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

    // AUTOMATISCHE SCHIP-ERKENNUNG
    bool isSCHIP = false;
    
    // 1. Prüfe Dateiendung
    if (filename.endsWith(".sc8") || filename.endsWith(".SC8") || 
        filename.endsWith(".schip") || filename.endsWith(".S86")) {
      isSCHIP = true;
      Serial.println("SCHIP ROM detected by extension");
    }
    
    // 2. Prüfe ROM-Größe (SCHIP ROMs sind oft > 3584 Bytes)
    if (romSize > 3584 && romSize <= 65536) {
      isSCHIP = true;
      Serial.println("SCHIP ROM detected by size");
    }
    
    // 3. Prüfe Magic Bytes (optional)
    uint8_t firstBytes[4];
    rom.seek(0);
    rom.read(firstBytes, 4);
    rom.seek(0);
    
    // Einige SCHIP ROMs haben spezielle Header
    if (firstBytes[0] == 0x53 && firstBytes[1] == 0x43) { // "SC"
      isSCHIP = true;
      Serial.println("SCHIP ROM detected by magic bytes");
    }
    
    // Aktiviere SCHIP Modus wenn nötig
    if (isSCHIP) {
      enableSCHIPMode();
    }
    
    // Prüfe maximale ROM-Größe
    size_t maxROMSize = isSCHIP ? 65536 : 3584;
    if (romSize > maxROMSize) {
      Serial.print("ERROR: ROM too large for ");
      Serial.print(isSCHIP ? "SCHIP" : "CHIP-8");
      Serial.print(" mode! Max: ");
      Serial.println(maxROMSize);
      rom.close();
      return false;
    }

    // Lade ROM in den Speicher
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
    Serial.print("Mode: ");
    Serial.println(isSCHIP ? "SUPER-CHIP" : "CHIP-8");

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
  
  // SCHIP: Scroll-Funktionen
  void scrollDown(int lines) {
    int width = hiresMode ? 128 : 64;
    int height = hiresMode ? 64 : 32;
    for (int y = height - 1; y >= lines; y--) {
      memcpy(&display[y * width], &display[(y - lines) * width], width);
    }
    memset(display, 0, lines * width);
    drawFlag = true;
  }
  
  void scrollRight() {
    int width = hiresMode ? 128 : 64;
    int height = hiresMode ? 64 : 32;
    for (int y = 0; y < height; y++) {
      for (int x = width - 1; x > 3; x--) {
        display[y * width + x] = display[y * width + x - 4];
      }
      for (int x = 0; x < 4 && x < width; x++) {
        display[y * width + x] = 0;
      }
    }
    drawFlag = true;
  }
  
  void scrollLeft() {
    int width = hiresMode ? 128 : 64;
    int height = hiresMode ? 64 : 32;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width - 4; x++) {
        display[y * width + x] = display[y * width + x + 4];
      }
      for (int x = width - 4; x < width; x++) {
        display[y * width + x] = 0;
      }
    }
    drawFlag = true;
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

    // SCHIP: Neue Opcodes erkennen
    if (schipMode) {
      // SCHIP: Exit (0x00FD)
      if (opcode == 0x00FD) {
        running = false;
        return;
      }
      
      // SCHIP: Scroll Down (0x00FC)
      if (opcode == 0x00FC) {
        scrollDown(1);
        return;
      }
      
      // SCHIP: Scroll Right (0x00FB)
      if (opcode == 0x00FB) {
        scrollRight();
        return;
      }
      
      // SCHIP: Scroll Left (0x00FA)
      if (opcode == 0x00FA) {
        scrollLeft();
        return;
      }
      
      // SCHIP: Hi-Res Mode (0x00FF)
      if (opcode == 0x00FF) {
        hiresMode = true;
        drawFlag = true;
        return;
      }
      
      // SCHIP: Low-Res Mode (0x00FE)
      if (opcode == 0x00FE) {
        hiresMode = false;
        drawFlag = true;
        return;
      }
      
      // SCHIP: Scroll Down by N (0x00CN)
      if ((opcode & 0xFFF0) == 0x00C0) {
        scrollDown(n);
        return;
      }
      
      // SCHIP: Load Hi-Res Font (0x00Fx) - wird durch I = nnn implementiert
    }

    switch (opcode & 0xF000) {
      case 0x0000:
        if ((opcode & 0x00FF) == 0x00E0) {
          // Clear Screen (unterstützt beide Auflösungen)
          int screenSize = hiresMode ? (128 * 64) : (64 * 32);
          memset(display, 0, screenSize);
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
          uint8_t xPos = V[x];
          uint8_t yPos = V[y];
          
          // SCHIP: Hi-Res Sprite Drawing (8x16 oder 16x16)
          int spriteHeight = n;
          int spriteWidth = schipMode && hiresMode ? 16 : 8;
          int maxX = hiresMode ? 128 : 64;
          int maxY = hiresMode ? 64 : 32;
          
          V[0xF] = 0;
          
          for (int row = 0; row < spriteHeight; row++) {
            uint16_t spriteData;
            
            if (spriteWidth == 16) {
              // 16-Bit Sprite für Hi-Res
              spriteData = (memory[I + row * 2] << 8) | memory[I + row * 2 + 1];
            } else {
              // 8-Bit Sprite für Low-Res
              spriteData = memory[I + row];
            }
            
            for (int col = 0; col < spriteWidth; col++) {
              if (spriteData & (1 << (spriteWidth - 1 - col))) {
                int pixelX = (xPos + col) % maxX;
                int pixelY = (yPos + row) % maxY;
                int idx = pixelY * maxX + pixelX;
                
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
          case 0x29: 
            // SCHIP: Hi-Res Fonts (0x50)
            if (schipMode && hiresMode) {
              I = 0x50 + (V[x] * 10);
            } else {
              I = V[x] * 5;
            }
            break;
          case 0x30: // SCHIP: Font for 8x10
            if (schipMode) {
              I = 0x50 + (V[x] * 10);
            } else {
              I = V[x] * 5;
            }
            break;
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
              // SCHIP: I verändert sich nicht automatisch
              if (!schipMode) I += x + 1;
              break;
            }
          case 0x65:
            {
              for (int i = 0; i <= x; i++) {
                V[i] = memory[I + i];
              }
              // SCHIP: I verändert sich nicht automatisch
              if (!schipMode) I += x + 1;
              break;
            }
          // SCHIP: Exit
          case 0x75:
          case 0x85:
            // Store/load registers - für SCHIP kompatibel
            break;
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
    int width = hiresMode ? 128 : 64;
    int height = hiresMode ? 64 : 32;
    int displayWidth = width * scale;
    int displayHeight = height * scale;
    
    // Hintergrund löschen
    tft.fillRect(xOffset - 2, yOffset - 2, displayWidth + 4, displayHeight + 4, BG_COLOR);
    
    // Pixel zeichnen
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        if (display[y * width + x]) {
          uint16_t color = hiresMode ? TFT_CYAN : TFT_GREEN;
          tft.fillRect(xOffset + x * scale, yOffset + y * scale, scale, scale, color);
        }
      }
    }
    
    // Rahmen
    tft.drawRect(xOffset - 2, yOffset - 2, displayWidth + 4, displayHeight + 4, TEXT_COLOR);
    
    // Hi-Res Anzeige
    if (hiresMode) {
      tft.setTextSize(1);
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(xOffset + displayWidth - 30, yOffset - 12);
      tft.print("Hi-Res");
    }
    
    drawFlag = false;
  }

  bool needsDraw() { return drawFlag; }
  bool isRunning() { return running; }
  String getROMName() { return romName; }
  bool isSCHIPMode() { return schipMode; }
  bool isHiRes() { return hiresMode; }
  uint16_t getPC() { return pc; }
  void setRunning(bool state) { running = state; }
  uint8_t* getDisplay() { return display; }
};
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

  if (ty < 170) {
    if (tx > 180 && ty < 40) {
      playSysSound(3);
      return "ESC_SIGNAL";
    }
    return "";
  }

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

// ==================== CHIP-8 EMULATOR APP ====================
void chip8Emulator() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillRect(0, 35, 240, 285, BG_COLOR);

  // ROMs sammeln (unterstützt jetzt beide Formate)
  std::vector<String> roms;
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    String name = String(entry.name());
    // Unterstützte Formate: .ch8, .CH8, .c8, .bin, .sc8, .SC8, .schip, .s86
    if (name.endsWith(".ch8") || name.endsWith(".CH8") || 
        name.endsWith(".c8") || name.endsWith(".bin") ||
        name.endsWith(".sc8") || name.endsWith(".SC8") ||
        name.endsWith(".schip") || name.endsWith(".s86")) {
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
  int maxVisible = 8;
  bool running = true;
  bool emulating = false;

  while (running && !emulating) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Titel
    tft.setTextSize(2);
    tft.drawCentreString("CHIP-8 / SCHIP EMULATOR", 120, 45, 2);
    tft.setTextSize(1);

    // ROM Count anzeigen
    char countStr[64];
    int ch8Count = 0, sc8Count = 0;
    for (String r : roms) {
      if (r.endsWith(".sc8") || r.endsWith(".SC8") || 
          r.endsWith(".schip") || r.endsWith(".s86")) {
        sc8Count++;
      } else {
        ch8Count++;
      }
    }
    sprintf(countStr, "ROMs: %d CHIP-8 | %d SCHIP", ch8Count, sc8Count);
    tft.drawCentreString(countStr, 120, 70, 1);

    // Keine ROMs gefunden
    if (roms.size() == 0) {
      tft.drawCentreString("No ROMs found on SD card!", 120, 100, 1);
      tft.drawCentreString("Place .ch8 or .sc8 files in /", 120, 115, 1);
    } else {
      // ROM Liste anzeigen mit Scroll
      int startIdx = scrollOffset;
      int endIdx = min(startIdx + maxVisible, (int)roms.size());

      for (int i = startIdx; i < endIdx; i++) {
        int y = 95 + (i - startIdx) * 18;
        
        // Typ-Erkennung für Anzeige
        bool isSCHIP = (roms[i].endsWith(".sc8") || roms[i].endsWith(".SC8") || 
                        roms[i].endsWith(".schip") || roms[i].endsWith(".s86"));
        
        // Ausgewählte ROM hervorheben
        if (i == selected) {
          tft.fillRoundRect(20, y - 2, 200, 16, 3, ACCENT_COLOR);
          tft.setTextColor(TFT_WHITE);
        } else {
          tft.setTextColor(TEXT_COLOR);
        }

        // ROM Name mit Typ-Anzeige
        tft.setCursor(25, y);
        char lineStr[64];
        sprintf(lineStr, "%s%-22s", isSCHIP ? "[S] " : "[8] ", roms[i].substring(0, 22).c_str());
        tft.println(lineStr);
      }

      // Scroll Indikatoren
      if (scrollOffset > 0) {
        tft.fillTriangle(120, 88, 130, 82, 110, 82, ACCENT_COLOR);
      }
      if (scrollOffset + maxVisible < (int)roms.size()) {
        tft.fillTriangle(120, 230, 130, 236, 110, 236, ACCENT_COLOR);
      }
    }

    // Buttons
    tft.fillRoundRect(30, 260, 80, 35, 4, SUCCESS_COLOR);
    tft.drawCentreString("LOAD", 70, 272, 1);
    tft.fillRoundRect(130, 260, 80, 35, 4, WARNING_COLOR);
    tft.drawCentreString("BACK", 170, 272, 1);
    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    // Scroll Buttons (Touch)
    tft.fillTriangle(215, 100, 235, 90, 235, 110, BUTTON_COLOR);
    tft.fillTriangle(215, 140, 235, 130, 235, 150, BUTTON_COLOR);

    int tx, ty;
    if (getTouch(tx, ty)) {
      // ESC
      if (tx > 180 && ty < 40) {
        running = false;
      }
      // Scroll Up
      else if (tx > 210 && ty > 90 && ty < 115 && scrollOffset > 0) {
        scrollOffset--;
        if (selected >= scrollOffset + maxVisible) {
          selected = scrollOffset + maxVisible - 1;
        }
        playSysSound(0);
        delay(100);
      }
      // Scroll Down
      else if (tx > 210 && ty > 130 && ty < 155 && scrollOffset + maxVisible < (int)roms.size()) {
        scrollOffset++;
        if (selected < scrollOffset) {
          selected = scrollOffset;
        }
        playSysSound(0);
        delay(100);
      }
      // LOAD Button
      else if (ty > 260 && ty < 295 && tx < 110 && roms.size() > 0) {
        if (selected >= 0 && selected < (int)roms.size()) {
          if (chip8.loadROM(roms[selected])) {
            printToConsole(successPrefix + "Loaded: " + roms[selected] + 
                          (chip8.isSCHIPMode() ? " (SCHIP Mode)" : " (CHIP-8 Mode)"), TFT_GREEN);
            emulating = true;
          } else {
            printToConsole(errorPrefix + "Failed to load ROM", TFT_RED);
            delay(500);
          }
        }
      }
      // BACK Button
      else if (ty > 260 && ty < 295 && tx > 110) {
        running = false;
      }
      // ROM Auswahl durch Touch auf Liste
      else if (ty > 90 && ty < 230 && roms.size() > 0) {
        int idx = scrollOffset + ((ty - 90) / 18);
        if (idx >= 0 && idx < (int)roms.size()) {
          selected = idx;
          playSysSound(0);
          delay(50);
        }
      }
    }
    delay(20);
  }

  // ==================== EMULATION (angepasst für SCHIP) ====================
  if (emulating) {
    tft.fillScreen(BG_COLOR);
    
    // Automatische Skalierung basierend auf Modus
    int scale;
    int xOffset, yOffset;
    
    if (chip8.isHiRes()) {
      // Hi-Res Modus: 128x64 Display
      scale = 1;  // 1:1 Skalierung (128x64)
      xOffset = (240 - 128) / 2;
      yOffset = 45;
    } else {
      // Low-Res Modus: 64x32 Display
      scale = 3;  // 3x Skalierung (192x96)
      xOffset = (240 - 192) / 2;
      yOffset = 45;
    }
    
    // ESC Button
    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    // ROM Name und Modus anzeigen
    tft.setTextColor(ACCENT_COLOR);
    tft.setTextSize(1);
    tft.setCursor(5, 8);
    String romName = chip8.getROMName();
    if (romName.length() > 18) romName = romName.substring(0, 15) + "...";
    tft.print(romName);
    
    tft.setCursor(5, 20);
    tft.print(chip8.isSCHIPMode() ? "SCHIP" : "CHIP-8");
    if (chip8.isHiRes()) tft.print(" (Hi-Res)");

    // CHIP-8/SCHIP Keypad Layout
    const char* keyNames[4][4] = {
      { "1", "2", "3", "C" },
      { "4", "5", "6", "D" },
      { "7", "8", "9", "E" },
      { "A", "0", "B", "F" }
    };

    // Tastenbereiche speichern
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
    tft.drawRect(xOffset - 2, yOffset - 2, 
                 (chip8.isHiRes() ? 128 : 64 * scale) + 4, 
                 (chip8.isHiRes() ? 64 : 32 * scale) + 4, TEXT_COLOR);

    // Emulations-Variablen
    unsigned long lastCycle = millis();
    unsigned long lastDraw = millis();
    unsigned long lastStats = millis();
    int cycles = 0;
    int frames = 0;

    // Key mapping
    int keyMap[4][4] = {
      { 0x1, 0x2, 0x3, 0xC },
      { 0x4, 0x5, 0x6, 0xD },
      { 0x7, 0x8, 0x9, 0xE },
      { 0xA, 0x0, 0xB, 0xF }
    };

    int lastPressedKey = -1;
    unsigned long lastKeyPressTime = 0;

    while (emulating && chip8.isRunning()) {
      int tx, ty;
      bool touchDetected = getTouch(tx, ty);

      // ESC prüfen
      if (touchDetected && tx > 180 && ty < 40) {
        emulating = false;
        break;
      }

      // Touch Erkennung
      int currentKey = -1;

      if (touchDetected) {
        for (int ky = 0; ky < 4; ky++) {
          for (int kx = 0; kx < 4; kx++) {
            if (tx >= keyRegions[ky][kx][0] && tx <= keyRegions[ky][kx][2] && 
                ty >= keyRegions[ky][kx][1] && ty <= keyRegions[ky][kx][3]) {
              currentKey = keyMap[ky][kx];
              break;
            }
          }
          if (currentKey != -1) break;
        }
      }

      // Taste verarbeiten
      if (currentKey != -1 && currentKey != lastPressedKey) {
        if (lastPressedKey != -1) {
          chip8.keyRelease();
        }
        chip8.keyPress(currentKey);
        lastPressedKey = currentKey;
        lastKeyPressTime = millis();
        playSysSound(0);
      } else if (currentKey == -1 && lastPressedKey != -1) {
        chip8.keyRelease();
        lastPressedKey = -1;
      }

      // Timers updaten
      chip8.updateTimers();

      // Cycles ausführen (SCHIP benötigt etwas mehr Leistung)
      int cycleDelay = chip8.isSCHIPMode() ? 1 : 2;
      if (millis() - lastCycle >= cycleDelay) {
        chip8.emulateCycle();
        lastCycle = millis();
        cycles++;
      }

      // Display zeichnen (~60Hz)
      if (millis() - lastDraw >= 16) {
        if (chip8.needsDraw()) {
          chip8.draw(tft, xOffset, yOffset, scale);
        }
        lastDraw = millis();
        frames++;
      }

      // Stats anzeigen
      if (millis() - lastStats >= 1000) {
        tft.fillRect(5, 32, 80, 10, BG_COLOR);
        tft.setCursor(5, 32);
        tft.setTextColor(TFT_YELLOW);
        tft.printf("CPS:%d", cycles);
        
        tft.fillRect(80, 32, 60, 10, BG_COLOR);
        tft.setCursor(80, 32);
        tft.setTextColor(SUCCESS_COLOR);
        tft.printf("FPS:%d", frames);
        
        if (chip8.isSCHIPMode()) {
          tft.fillRect(140, 32, 40, 10, BG_COLOR);
          tft.setCursor(140, 32);
          tft.setTextColor(TFT_CYAN);
          tft.print("SCHIP");
        }

        cycles = 0;
        frames = 0;
        lastStats = millis();
      }

      delay(1);
    }

    // Aufräumen
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
  if(f) {
    f.println("╔══════════════════════════════════════════╗");
    f.println("║        CYD TERMINAL OS v3.0             ║");
    f.println("║    ESP32-WROOM Command Reference        ║");
    f.println("╚══════════════════════════════════════════╝");
    f.println("");
    f.println("USAGE: <command> [arguments] [options]");
    f.println("       help [category] - Detailed help");
    f.println("       man <command>   - Manual page");
    f.println("");
    f.println("┌─────────────────────────────────────────┐");
    f.println("│ QUICK REFERENCE                         │");
    f.println("├─────────────────────────────────────────┤");
    f.println("│ help, ?, man    - This help system      │");
    f.println("│ cls, clear      - Clear terminal        │");
    f.println("│ reboot          - Restart system        │");
    f.println("│ date, uptime    - Time information      │");
    f.println("│ free, ps        - System resources      │");
    f.println("│ neofetch        - System info banner    │");
    f.println("└─────────────────────────────────────────┘");
    f.println("");
    f.println("AVAILABLE CATEGORIES:");
    f.println("  help system    - System commands");
    f.println("  help files     - File operations");
    f.println("  help net       - Network tools");
    f.println("  help games     - Games & Entertainment");
    f.println("  help apps      - Applications");
    f.println("  help dev       - Developer tools");
    f.println("  help tips      - Tips & Tricks");
    f.println("  help all       - Complete reference");
    f.close();
  }
  
  // System Hilfe
  SD.remove("/help_sys.txt");
  f = SD.open("/help_sys.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ SYSTEM COMMANDS ══════════");
    f.println("");
    f.println("cls, clear");
    f.println("  Clears the terminal screen");
    f.println("  Usage: cls");
    f.println("");
    f.println("reboot, restart");
    f.println("  Restarts the CYD OS");
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
    f.println("settings");
    f.println("  Open settings menu (GUI)");
    f.println("  Usage: settings");
    f.close();
  }
  
  // Datei Hilfe
  SD.remove("/help_files.txt");
  f = SD.open("/help_files.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ FILE OPERATIONS ══════════");
    f.println("");
    f.println("ls [path]");
    f.println("  List directory contents");
    f.println("  Usage: ls /folder");
    f.println("  Flags: -l (detailed view soon)");
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
    f.println("files, fm");
    f.println("  Open file manager (GUI)");
    f.println("  Usage: files");
    f.close();
  }
  
  // Netzwerk Hilfe
  SD.remove("/help_net.txt");
  f = SD.open("/help_net.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ NETWORK TOOLS ══════════");
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
    f.println("chat");
    f.println("  Start chat application");
    f.println("  Usage: chat");
    f.println("");
    f.println("scan");
    f.println("  Scan for WiFi networks");
    f.println("  Usage: scan (coming soon)");
    f.close();
  }
  
  // Spiele Hilfe
  SD.remove("/help_games.txt");
  f = SD.open("/help_games.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ GAMES & FUN ══════════");
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
    f.println("dice <sides>");
    f.println("  Roll a dice with N sides");
    f.println("  Usage: dice 20");
    f.println("");
    f.println("random [max]");
    f.println("  Generate random number 0-999");
    f.println("  Usage: random 100");
    f.close();
  }
  
  // Apps Hilfe
  SD.remove("/help_apps.txt");
  f = SD.open("/help_apps.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ APPLICATIONS ══════════");
    f.println("");
    f.println("chip8");
    f.println("  CHIP-8 game emulator");
    f.println("  Place .ch8 ROMs on SD card root");
    f.println("  Usage: chip8");
    f.println("  Controls: Touch keypad on screen");
    f.println("");
    f.println("calc");
    f.println("  Scientific calculator");
    f.println("  Modes: Standard, Scientific, Programmer");
    f.println("  Usage: calc");
    f.println("");
    f.println("draw, paint");
    f.println("  Drawing application");
    f.println("  Features: 32 colors, RGB mixer,");
    f.println("            8 tools, 6 brush sizes");
    f.println("  Usage: draw");
    f.println("  Tools: Brush, Line, Rect, Circle,");
    f.println("         Fill, Eraser, Picker, Text");
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
    f.println("");
    f.println("clock");
    f.println("  Digital clock display");
    f.println("  Usage: clock (coming soon)");
    f.close();
  }
  
  // Developer Tools
  SD.remove("/help_dev.txt");
  f = SD.open("/help_dev.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ DEVELOPER TOOLS ══════════");
    f.println("");
    f.println("echo <text>");
    f.println("  Print text to terminal");
    f.println("  Variables: $TIME, $FREE");
    f.println("  Usage: echo Hello World");
    f.println("");
    f.println("eval <expression>");
    f.println("  Evaluate math expression");
    f.println("  Usage: eval 2+2");
    f.println("");
    f.println("colors");
    f.println("  List available color names");
    f.println("  Usage: colors");
    f.println("");
    f.println("heap");
    f.println("  Show heap fragmentation");
    f.println("  Usage: heap (coming soon)");
    f.println("");
    f.println("i2cscan");
    f.println("  Scan I2C bus for devices");
    f.println("  Usage: i2cscan (coming soon)");
    f.println("");
    f.println("benchmark");
    f.println("  Run CPU benchmark test");
    f.println("  Usage: benchmark (coming soon)");
    f.close();
  }
  
  // Tips & Tricks
  SD.remove("/help_tips.txt");
  f = SD.open("/help_tips.txt", FILE_WRITE);
  if(f) {
    f.println("══════════ TIPS & TRICKS ══════════");
    f.println("");
    f.println("KEYBOARD SHORTCUTS:");
    f.println("  Keyboard button ^  = Shift");
    f.println("  Keyboard button 123 = Numbers");
    f.println("  Keyboard button §$% = Special chars");
    f.println("  OK button = Enter/Execute");
    f.println("");
    f.println("TERMINAL FEATURES:");
    f.println("  Scroll: Use ▲▼ buttons");
    f.println("  Backspace: < button on keyboard");
    f.println("  ESC: Top-right corner");
    f.println("");
    f.println("DRAWING EDITOR TIPS:");
    f.println("  [>] = Switch to RGB mixer");
    f.println("  [<] = Back to palette");
    f.println("  Click color to select");
    f.println("  Click circle for brush size");
    f.println("  SET button = Apply custom RGB");
    f.println("");
    f.println("CHIP-8 EMULATOR TIPS:");
    f.println("  Place ROMs in root of SD card");
    f.println("  Supported: .ch8, .CH8, .c8, .bin");
    f.println("  Scroll: Touch ▲▼ arrows");
    f.println("  Keypad: 1-9, A-F on screen");
    f.println("");
    f.println("CALCULATOR TIPS:");
    f.println("  STD = Standard (+, -, *, /)");
    f.println("  SCI = Scientific (sin, cos, sqrt)");
    f.println("  PROG = Programmer (AND, OR, XOR)");
    f.println("");
    f.println("SD CARD TIPS:");
    f.println("  Format: FAT32");
    f.println("  Max size: 32GB tested");
    f.println("  Auto-mounts on boot");
    f.println("");
    f.println("TROUBLESHOOTING:");
    f.println("  Screen freezes: Press RST button");
    f.println("  SD not detected: Check format");
    f.println("  ROM won't load: Max 3584 bytes");
    f.println("  WiFi issues: Reboot and retry");
    f.close();
  }
  
  printToConsole(successPrefix + "Help system ready!", TFT_GREEN);
  printToConsole(infoPrefix + "Type 'help' for overview", TFT_BLUE);
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

  drawScrollButtons();
  drawKeyboard();
  updateInputLine(true);
  printToConsole(successPrefix + "CYD OS v3.0 Ready!", TFT_NAVY);
  printToConsole(infoPrefix + "Type 'help' for commands", TFT_BLUE);
  printToConsole(infoPrefix + "Type 'chip8' for Game Boy style emulator", TFT_GREEN);
  printToConsole(infoPrefix + "Place .ch8 ROMs on SD card", TFT_GREEN);
  randomSeed(analogRead(34));
}

// ==================== MAIN LOOP ====================
void loop() {
  // Cursor blinken lassen
  if (millis() - lastCursorBlink > 500) {
    cursorVisible = !cursorVisible; 
    lastCursorBlink = millis(); 
    updateInputLine(false);
  }

  String cmd = handleKeyboardInput();
  if(cmd != "" && cmd != "ESC_SIGNAL") {
    String lowCmd = cmd; 
    lowCmd.toLowerCase();
    
    // ==================== HILFE-SYSTEM ====================
    if(lowCmd == "help" || lowCmd == "?") {
      writeHelpFile();
      File f = SD.open("/help.txt");
      if(f) {
        while(f.available()) {
          String line = f.readStringUntil('\n');
          line.replace("\r", "");
          if(line.startsWith("╔") || line.startsWith("║") || line.startsWith("╚")) {
            printToConsole(line, TFT_CYAN);
          } else if(line.startsWith("AVAILABLE") || line.startsWith("┌") || 
                    line.startsWith("├") || line.startsWith("└") || line.startsWith("│")) {
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
    else if(lowCmd.startsWith("help ")) {
      String category = lowCmd.substring(5);
      String helpFile = "/help_";
      
      if(category == "system" || category == "sys") helpFile += "sys.txt";
      else if(category == "files" || category == "file") helpFile += "files.txt";
      else if(category == "net" || category == "network" || category == "wifi") helpFile += "net.txt";
      else if(category == "games" || category == "game" || category == "fun") helpFile += "games.txt";
      else if(category == "apps" || category == "app" || category == "applications") helpFile += "apps.txt";
      else if(category == "dev" || category == "developer" || category == "programming") helpFile += "dev.txt";
      else if(category == "tips" || category == "tricks" || category == "tutorial") helpFile += "tips.txt";
      else if(category == "all" || category == "full" || category == "complete") {
        String helpFiles[] = {"help.txt", "help_sys.txt", "help_files.txt", "help_net.txt", 
                              "help_games.txt", "help_apps.txt", "help_dev.txt", "help_tips.txt"};
        for(String hf : helpFiles) {
          printToConsole("");
          printToConsole("════════════════════════════════", TFT_CYAN);
          File f = SD.open("/" + hf);
          if(f) {
            while(f.available()) {
              String line = f.readStringUntil('\n');
              line.replace("\r", "");
              printToConsole(line);
            }
            f.close();
          }
          delay(100);
        }
        helpFile = "";
      }
      else {
        printToConsole(errorPrefix + "Unknown category: " + category, TFT_RED);
        printToConsole(infoPrefix + "Try: system, files, net, games, apps, dev, tips, all", TFT_BLUE);
        helpFile = "";
      }
      
      if(helpFile != "") {
        File f = SD.open(helpFile);
        if(f) {
          while(f.available()) {
            String line = f.readStringUntil('\n');
            line.replace("\r", "");
            if(line.startsWith("══════")) printToConsole(line, TFT_CYAN);
            else if(line.indexOf("Usage:") >= 0) printToConsole(line, TFT_GREEN);
            else printToConsole(line);
          }
          f.close();
        } else {
          printToConsole(errorPrefix + "Help file not found!", TFT_RED);
        }
      }
    }
    
    // Man-Page System
    else if(lowCmd.startsWith("man ")) {
      String searchTerm = lowCmd.substring(4);
      String helpFiles[] = {"help_sys.txt", "help_files.txt", "help_net.txt", 
                            "help_games.txt", "help_apps.txt", "help_dev.txt"};
      bool found = false;
      
      for(String hf : helpFiles) {
        File f = SD.open("/" + hf);
        if(f) {
          bool inSection = false;
          String section = "";
          while(f.available()) {
            String line = f.readStringUntil('\n');
            line.replace("\r", "");
            
            if(line.startsWith(searchTerm) || line.indexOf(searchTerm + " ") >= 0) {
              if(!found) {
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
      
      if(!found) {
        printToConsole(errorPrefix + "No manual entry for: " + searchTerm, TFT_RED);
        printToConsole(infoPrefix + "Try 'help' for available commands", TFT_BLUE);
      }
    }
    
    // ==================== SYSTEM ====================
    else if(lowCmd == "cls" || lowCmd == "clear") {
      terminalHistory.clear();
      scrollOffset = 0;
      refreshTerminal();
      updateInputLine(true);
    }
    
    else if(lowCmd == "reboot" || lowCmd == "restart") {
      printToConsole("Rebooting...", TFT_RED);
      delay(500);
      ESP.restart();
    }
    
    else if(lowCmd == "date") {
      unsigned long seconds = millis() / 1000;
      int days = seconds / 86400;
      int hours = (seconds % 86400) / 3600;
      int mins = (seconds % 3600) / 60;
      int secs = seconds % 60;
      char buf[50];
      sprintf(buf, "Boot time: %d days, %02d:%02d:%02d", days, hours, mins, secs);
      printToConsole(buf);
    }
    
    else if(lowCmd == "uptime") {
      unsigned long uptime = millis() / 1000;
      int hours = uptime / 3600;
      int mins = (uptime % 3600) / 60;
      int secs = uptime % 60;
      char buf[40];
      sprintf(buf, "Uptime: %02d:%02d:%02d", hours, mins, secs);
      printToConsole(buf, TFT_GREEN);
    }
    
    else if(lowCmd == "free") {
      printToConsole("Memory Information:", TFT_CYAN);
      printToConsole("  RAM Free:  " + String(ESP.getFreeHeap()) + " bytes");
      printToConsole("  Heap Total: " + String(ESP.getHeapSize()) + " bytes");
      printToConsole("  Min Free:  " + String(ESP.getMinFreeHeap()) + " bytes");
      printToConsole("  Flash:     " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
    }
    
    else if(lowCmd == "ps") {
      printToConsole("PID  STATE     COMMAND", TFT_CYAN);
      printToConsole("────────────────────────────");
      printToConsole("1    running   terminal");
      printToConsole("2    running   touchdriver");
      printToConsole("3    sleeping  idle");
      printToConsole("4    waiting   watchdog");
    }
    
    else if(lowCmd == "neofetch") {
      printToConsole("", TFT_CYAN);
      printToConsole("      _____  _____  ____   ", TFT_CYAN);
      printToConsole("     / ____|/ ____|/ __ \\ ", TFT_CYAN);
      printToConsole("    | |    | (___ | |  | |", TFT_CYAN);
      printToConsole("    | |     \\___ \\| |  | |", TFT_CYAN);
      printToConsole("    | |____ ____) | |__| |", TFT_CYAN);
      printToConsole("     \\_____|_____/ \\____/ ", TFT_CYAN);
      printToConsole("");
      printToConsole("OS: CYD Terminal OS v3.0", TFT_GREEN);
      printToConsole("Host: ESP32-WROOM @ 240MHz", TFT_GREEN);
      printToConsole("RAM: " + String(ESP.getFreeHeap()/1024) + "KB / " + String(ESP.getHeapSize()/1024) + "KB", TFT_GREEN);
      printToConsole("Theme: " + String(darkMode ? "Dark" : "Light"), TFT_GREEN);
      printToConsole("Sound: " + String(soundEnabled ? "ON" : "OFF"), TFT_GREEN);
    }
    
    else if(lowCmd == "sysinfo" || lowCmd == "stats") {
      sysInfo();
    }
    
    else if(lowCmd == "sd" || lowCmd == "storage") {
      storageInfo();
    }
    
    else if(lowCmd == "darkmode" || lowCmd == "theme") {
      darkMode = !darkMode;
      applyTheme();
      printToConsole(successPrefix + "Dark Mode: " + String(darkMode ? "ON" : "OFF"), TFT_GREEN);
      EEPROM.write(1, darkMode);
      EEPROM.commit();
    }
    
    else if(lowCmd == "sound") {
      soundEnabled = !soundEnabled;
      printToConsole(successPrefix + "Sound: " + String(soundEnabled ? "ON" : "OFF"), TFT_GREEN);
      if(soundEnabled) playSysSound(1);
      EEPROM.write(0, soundEnabled);
      EEPROM.commit();
    }
    
    else if(lowCmd == "colors") {
      printToConsole("Available Colors:", TFT_CYAN);
      printToConsole("black, navy, dkgreen, dkcyan");
      printToConsole("maroon, purple, olive, grey");
      printToConsole("dkgrey, blue, green, cyan");
      printToConsole("red, magenta, yellow, white");
      printToConsole("orange, lime, pink, brown");
      printToConsole("gold, silver, sky, violet");
      printToConsole("coral, mint, sand, salmon");
    }
    
    // ==================== ECHO ====================
    else if(lowCmd.startsWith("echo ")) {
      String msg = cmd.substring(5);
      msg.replace("$TIME", String(millis()));
      msg.replace("$FREE", String(ESP.getFreeHeap()));
      msg.replace("$UPTIME", String(millis()/1000));
      printToConsole(msg);
    }
    
    // ==================== DATEIOPERATIONEN ====================
    else if(lowCmd == "ls" || lowCmd == "dir") {
      std::vector<String> f, d;
      listDirectory("/", f, d);
      printToConsole("Directory: /", TFT_CYAN);
      for(String dir : d) printToConsole("  [DIR]  " + dir, TFT_GREEN);
      for(String file : f) printToConsole("  [FILE] " + file);
      printToConsole(String(d.size()) + " dirs, " + String(f.size()) + " files");
    }
    
    else if(lowCmd.startsWith("ls ")) {
      String path = lowCmd.substring(3);
      std::vector<String> f, d;
      listDirectory(path, f, d);
      printToConsole("Directory: " + path, TFT_CYAN);
      for(String dir : d) printToConsole("  [DIR]  " + dir, TFT_GREEN);
      for(String file : f) printToConsole("  [FILE] " + file);
      printToConsole(String(d.size()) + " dirs, " + String(f.size()) + " files");
    }
    
    else if(lowCmd.startsWith("cat ")) {
      String filename = lowCmd.substring(4);
      File f = SD.open(filename);
      if(f) {
        printToConsole("File: " + filename, TFT_CYAN);
        int lines = 0;
        while(f.available() && lines < 20) {
          String line = f.readStringUntil('\n');
          line.replace("\r", "");
          printToConsole(line);
          lines++;
        }
        if(f.available()) printToConsole("... (use head/tail for large files)");
        f.close();
      } else {
        printToConsole(errorPrefix + "File not found: " + filename, TFT_RED);
      }
    }
    
    else if(lowCmd.startsWith("head ")) {
      int lines = 10;
      String rest = lowCmd.substring(5);
      String filename;
      
      if(rest.startsWith("-n")) {
        int spacePos = rest.indexOf(' ');
        lines = rest.substring(2, spacePos).toInt();
        filename = rest.substring(spacePos + 1);
      } else {
        filename = rest;
      }
      
      File f = SD.open(filename);
      if(f) {
        printToConsole("First " + String(lines) + " lines of " + filename + ":", TFT_CYAN);
        for(int i=0; i<lines && f.available(); i++) {
          printToConsole(f.readStringUntil('\n'));
        }
        f.close();
      } else {
        printToConsole(errorPrefix + "File not found", TFT_RED);
      }
    }
    
    else if(lowCmd.startsWith("tail ")) {
      int lines = 10;
      String rest = lowCmd.substring(5);
      String filename = rest;
      
      if(rest.startsWith("-n")) {
        int spacePos = rest.indexOf(' ');
        lines = rest.substring(2, spacePos).toInt();
        filename = rest.substring(spacePos + 1);
      }
      
      std::vector<String> allLines;
      File f = SD.open(filename);
      if(f) {
        while(f.available()) allLines.push_back(f.readStringUntil('\n'));
        f.close();
        
        printToConsole("Last " + String(lines) + " lines of " + filename + ":", TFT_CYAN);
        int start = max(0, (int)allLines.size() - lines);
        for(int i=start; i<allLines.size(); i++) {
          printToConsole(allLines[i]);
        }
      } else {
        printToConsole(errorPrefix + "File not found", TFT_RED);
      }
    }
    
    else if(lowCmd.startsWith("grep ")) {
      String rest = lowCmd.substring(5);
      int firstQuote = rest.indexOf('"');
      int secondQuote = rest.indexOf('"', firstQuote + 1);
      
      if(firstQuote >= 0 && secondQuote >= 0) {
        String pattern = rest.substring(firstQuote + 1, secondQuote);
        String filename = rest.substring(secondQuote + 2);
        
        File f = SD.open(filename);
        if(f) {
          printToConsole("Searching for '" + pattern + "' in " + filename + ":", TFT_CYAN);
          int matches = 0;
          while(f.available()) {
            String line = f.readStringUntil('\n');
            if(line.indexOf(pattern) >= 0) {
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
    
    else if(lowCmd.startsWith("rm ")) {
      String path = lowCmd.substring(3);
      deleteFileOrDir(path);
    }
    
    else if(lowCmd.startsWith("touch ")) {
      String filename = lowCmd.substring(6);
      File f = SD.open(filename, FILE_WRITE);
      if(f) {
        f.println("");
        f.close();
        printToConsole(successPrefix + "Created: " + filename, TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Cannot create file", TFT_RED);
      }
    }
    
    else if(lowCmd.startsWith("mkdir ")) {
      String dirname = lowCmd.substring(6);
      if(SD.mkdir(dirname)) {
        printToConsole(successPrefix + "Created directory: " + dirname, TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Cannot create directory", TFT_RED);
      }
    }
    
    // ==================== NETZWERK ====================
    else if(lowCmd == "ifconfig" || lowCmd == "ip") {
      printToConsole("Network Configuration:", TFT_CYAN);
      if(WiFi.status() == WL_CONNECTED) {
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
    
    else if(lowCmd.startsWith("ping ")) {
      String host = lowCmd.substring(5);
      printToConsole("PING " + host + ":", TFT_CYAN);
      WiFiClient client;
      int success = 0;
      
      for(int i=0; i<4; i++) {
        unsigned long start = micros();
        if(client.connect(host.c_str(), 80)) {
          unsigned long respTime = (micros() - start) / 1000;
          printToConsole("  64 bytes: time=" + String(respTime) + "ms");
          client.stop();
          success++;
        } else {
          printToConsole("  Request timeout", TFT_RED);
        }
        if(i < 3) delay(1000);
      }
      printToConsole("--- " + host + " ping statistics ---");
      printToConsole(String(success) + " packets received, " + String(100 - success*25) + "% loss");
    }
    
    else if(lowCmd.startsWith("wget ")) {
      String url = lowCmd.substring(5);
      printToConsole(infoPrefix + "Downloading " + url + "...", TFT_BLUE);
      
      if(WiFi.status() != WL_CONNECTED) {
        printToConsole(errorPrefix + "WiFi not connected!", TFT_RED);
      } else {
        WiFiClient client;
        String host = url;
        String path = "/";
        
        if(host.indexOf("http://") == 0) host = host.substring(7);
        if(host.indexOf('/') > 0) {
          path = host.substring(host.indexOf('/'));
          host = host.substring(0, host.indexOf('/'));
        }
        
        if(client.connect(host.c_str(), 80)) {
          client.print("GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n");
          
          String filename = "download.txt";
          if(path.lastIndexOf('/') >= 0 && path.lastIndexOf('/') < path.length() - 1) {
            filename = path.substring(path.lastIndexOf('/') + 1);
          }
          
          File f = SD.open("/" + filename, FILE_WRITE);
          bool headersDone = false;
          
          while(client.connected()) {
            if(client.available()) {
              String line = client.readStringUntil('\n');
              if(line == "\r") {
                headersDone = true;
              } else if(headersDone && f) {
                f.print(line);
              }
            }
          }
          client.stop();
          if(f) f.close();
          printToConsole(successPrefix + "Downloaded to /" + filename, TFT_GREEN);
        } else {
          printToConsole(errorPrefix + "Connection failed!", TFT_RED);
        }
      }
    }
    
    else if(lowCmd == "wifi") {
      wifiManager();
    }
    
    // ==================== SPIELE & UNTERHALTUNG ====================
    else if(lowCmd == "random") {
      randomSeed(millis());
      printToConsole("Random: " + String(random(1000)), TFT_GREEN);
    }
    
    else if(lowCmd.startsWith("dice ")) {
      int sides = lowCmd.substring(5).toInt();
      if(sides > 0 && sides <= 100) {
        printToConsole("🎲 Rolling d" + String(sides) + "... " + String(random(1, sides + 1)), TFT_GREEN);
        playSysSound(0);
      } else {
        printToConsole(errorPrefix + "Use: dice <1-100>", TFT_RED);
      }
    }
    
    else if(lowCmd == "snake") snakeGame();
    else if(lowCmd == "pong") pongGame();
    else if(lowCmd == "tictac" || lowCmd == "ttt") ticTacToe();
    
    // ==================== APPLIKATIONEN ====================
    else if(lowCmd == "chip8" || lowCmd == "chip-8") {
      chip8Emulator();
    }
    
    else if(lowCmd == "calc" || lowCmd == "rechner") {
      calculator();
    }
    
    else if(lowCmd == "files" || lowCmd == "filemanager" || lowCmd == "fm") {
      fileManager();
    }
    
    else if(lowCmd == "draw" || lowCmd == "paint") {
      drawingApp();
    }
    
    else if(lowCmd == "notes") {
      notesApp();
    }
    
    else if(lowCmd == "todo") {
      todoApp();
    }
    
    else if(lowCmd == "timer") {
      timerApp();
    }
    
    else if(lowCmd == "chat") {
      chatApp(false);
    }
    
    else if(lowCmd == "settings" || lowCmd == "einstellungen") {
      settingsMenu();
    }
    
    // ==================== ENTWICKLER-TOOLS ====================
    else if(lowCmd.startsWith("eval ")) {
      String expr = lowCmd.substring(5);
      String result = evaluateExpression(expr);
      if(result != "Error") {
        printToConsole(expr + " = " + result, TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Cannot evaluate: " + expr, TFT_RED);
      }
    }
    
    else if(lowCmd.startsWith("delay ") || lowCmd.startsWith("sleep ")) {
      int ms = lowCmd.substring(lowCmd.indexOf(' ') + 1).toInt();
      if(ms > 0 && ms <= 10000) {
        printToConsole(infoPrefix + "Waiting " + String(ms) + "ms...", TFT_BLUE);
        delay(ms);
        printToConsole(successPrefix + "Done", TFT_GREEN);
      } else {
        printToConsole(errorPrefix + "Use: delay <1-10000>", TFT_RED);
      }
    }
    
    // ==================== UNBEKANNTER BEFEHL ====================
    else {
      // Versuche als Datei zu öffnen für cat-like Verhalten
      File f = SD.open(cmd);
      if(f) {
        printToConsole("File: " + cmd, TFT_CYAN);
        int lines = 0;
        while(f.available() && lines < 30) {
          printToConsole(f.readStringUntil('\n'));
          lines++;
        }
        if(f.available()) printToConsole("... (truncated, use 'cat' for full view)");
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