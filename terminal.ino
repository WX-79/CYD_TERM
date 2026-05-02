#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <EEPROM.h>
#include <time.h>
#include <Wire.h>
#include <TimeLib.h>
#include <qrcode_espi.h>

#define MAX_SAVED_WIFIS 5
#define WIFI_EEPROM_START 150
#define WIFI_EEPROM_SIZE 256

struct SavedWiFi {
  String ssid;
  String password;
  bool valid;
};

SavedWiFi savedNetworks[MAX_SAVED_WIFIS];


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
#define MAX_HISTORY 200
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
void drawingApp();
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

// ==================== WIFI DATEIVERWALTUNG MIT PASSWORT ====================
// ==================== CAESAR VERSCHLÜSSELUNG (BIDIREKTIONAL) ====================

// Verschlüsselung mit symmetrischer Caesar-Methode


// Passwort speichern (verschlüsselt)


// ==================== WEB SERVER DATEIVERWALTUNG ====================

WebServer fileServer(8080);  // Port 8080 für Dateiverwaltung
bool fileServerRunning = false;
String adminPassword = "";
bool isAuthenticated = false;
unsigned long authTimeout = 0;

// HTML Login Seite
String getLoginPage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>CYD File Manager Login</title>";
  html += "<style>";
  html += "body{font-family:Arial;background:#1a1a2e;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}";
  html += ".login{background:#16213e;padding:40px;border-radius:10px;box-shadow:0 0 20px rgba(0,0,0,0.5)}";
  html += "h2{color:#0f3460;text-align:center}";
  html += "input{width:100%;padding:12px;margin:10px 0;border:none;border-radius:5px;background:#0f3460;color:#fff}";
  html += "button{width:100%;padding:12px;background:#e94560;border:none;border-radius:5px;color:#fff;cursor:pointer}";
  html += "button:hover{background:#ff6b6b}";
  html += "</style></head><body>";
  html += "<div class='login'><h2>🔐 CYD File Manager</h2>";
  html += "<form method='POST' action='/login'>";
  html += "<input type='password' name='password' placeholder='Admin Password' autofocus>";
  html += "<button type='submit'>Login</button>";
  html += "</form></div></body></html>";
  return html;
}

// HTML File Manager Hauptseite
String getFileManagerPage(String currentPath = "/") {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>CYD File Manager</title>";
  html += "<style>";
  html += "*{box-sizing:border-box}";
  html += "body{font-family:'Segoe UI',Arial;background:#0a0a0a;margin:0;padding:20px;color:#eee}";
  html += ".container{max-width:1200px;margin:0 auto}";
  html += ".header{background:#1e1e2e;padding:15px;border-radius:10px;margin-bottom:20px}";
  html += ".header h1{color:#e94560;margin:0}";
  html += ".path{background:#2a2a3a;padding:10px;border-radius:5px;margin:10px 0;word-break:break-all}";
  html += ".tools{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:20px}";
  html += ".btn{background:#3a3a4a;padding:10px 20px;border:none;border-radius:5px;color:#fff;cursor:pointer;text-decoration:none;display:inline-block}";
  html += ".btn-primary{background:#e94560}";
  html += ".btn-success{background:#27ae60}";
  html += ".btn-danger{background:#c0392b}";
  html += ".btn-warning{background:#f39c12}";
  html += ".btn-info{background:#3498db}";
  html += ".file-list{background:#1e1e2e;border-radius:10px;overflow:hidden}";
  html += ".file-item{display:flex;justify-content:space-between;align-items:center;padding:12px 15px;border-bottom:1px solid #2a2a3a}";
  html += ".file-item:hover{background:#2a2a3a}";
  html += ".file-name{flex:2;word-break:break-all}";
  html += ".file-size{flex:0.5;text-align:right;color:#888}";
  html += ".file-actions{flex:1;display:flex;gap:5px;justify-content:flex-end}";
  html += ".file-actions button{background:#3a3a4a;border:none;padding:5px 10px;border-radius:3px;color:#fff;cursor:pointer}";
  html += ".dir{color:#3498db;font-weight:bold}";
  html += ".upload-area{border:2px dashed #3a3a4a;border-radius:10px;padding:20px;text-align:center;margin-top:20px}";
  html += ".status{position:fixed;bottom:20px;right:20px;background:#27ae60;padding:10px 20px;border-radius:5px;display:none}";
  html += "input,select{padding:10px;margin:5px;border-radius:5px;border:none;background:#2a2a3a;color:#fff}";
  html += "@media (max-width:768px){.file-item{flex-wrap:wrap}.file-actions{margin-top:10px;width:100%}}";
  html += "</style>";
  html += "<script>";
  html += "function showStatus(msg,isError){";
  html += "var s=document.getElementById('status');s.innerHTML=msg;s.style.backgroundColor=isError?'#c0392b':'#27ae60';s.style.display='block';";
  html += "setTimeout(function(){s.style.display='none';},3000);}";
  html += "function renameItem(oldName){";
  html += "var newName=prompt('New name:',oldName.split('/').pop());";
  html += "if(newName){fetch('/rename?old='+encodeURIComponent(oldName)+'&new='+encodeURIComponent(newName)).then(r=>r.text()).then(t=>{showStatus(t);location.reload();});}}";
  html += "function moveItem(path){";
  html += "var dest=prompt('Destination path (e.g., /folder/):','/');";
  html += "if(dest){fetch('/move?src='+encodeURIComponent(path)+'&dst='+encodeURIComponent(dest)).then(r=>r.text()).then(t=>{showStatus(t);location.reload();});}}";
  html += "function copyItem(path){";
  html += "var dest=prompt('Destination path (e.g., /folder/):','/');";
  html += "if(dest){fetch('/copy?src='+encodeURIComponent(path)+'&dst='+encodeURIComponent(dest)).then(r=>r.text()).then(t=>{showStatus(t);location.reload();});}}";
  html += "function deleteItem(path){";
  html += "if(confirm('Delete '+path+'?')){fetch('/delete?path='+encodeURIComponent(path)).then(r=>r.text()).then(t=>{showStatus(t);location.reload();});}}";
  html += "function createFolder(){";
  html += "var name=prompt('Folder name:');";
  html += "if(name){fetch('/mkdir?name='+encodeURIComponent(name)).then(r=>r.text()).then(t=>{showStatus(t);location.reload();});}}";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>📁 CYD File Manager</h1>";
  html += "<div class='path'>📍 Current: " + currentPath + "</div>";
  html += "</div>";

  // Tools
  html += "<div class='tools'>";
  html += "<a href='/logout' class='btn btn-danger'>🚪 Logout</a>";
  html += "<button class='btn btn-warning' onclick='createFolder()'>📁 New Folder</button>";
  html += "<button class='btn btn-info' onclick=\"window.location.href='/upload'\">📤 Upload File</button>";
  html += "<button class='btn' onclick=\"window.location.reload()\">🔄 Refresh</button>";
  html += "</div>";

  // Dateiliste
  html += "<div class='file-list'>";

  // Parent directory Link
  if (currentPath != "/") {
    String parentPath = currentPath.substring(0, currentPath.lastIndexOf('/'));
    if (parentPath == "") parentPath = "/";
    html += "<div class='file-item'>";
    html += "<div class='file-name'><a href='/?path=" + urlEncode(parentPath) + "' style='color:#e94560;text-decoration:none;'>📂 .. (Parent)</a></div>";
    html += "<div class='file-size'></div>";
    html += "<div class='file-actions'></div>";
    html += "</div>";
  }

  // Dateien und Ordner auflisten
  std::vector<String> dirs, files;
  listDirectory(currentPath, files, dirs);

  // Ordner zuerst
  for (String dir : dirs) {
    String fullPath = currentPath;
    if (fullPath == "/") fullPath = "/" + dir;
    else fullPath = currentPath + "/" + dir;
    fullPath.replace("//", "/");

    html += "<div class='file-item'>";
    html += "<div class='file-name dir'>📁 <a href='/?path=" + urlEncode(fullPath) + "' style='color:#3498db;text-decoration:none;'>" + dir + "</a></div>";
    html += "<div class='file-size'></div>";
    html += "<div class='file-actions'>";
    html += "<button onclick='renameItem(\"" + fullPath + "\")'>✏️</button>";
    html += "<button onclick='moveItem(\"" + fullPath + "\")'>📦</button>";
    html += "<button onclick='copyItem(\"" + fullPath + "\")'>📋</button>";
    html += "<button onclick='deleteItem(\"" + fullPath + "\")' style='background:#c0392b'>🗑️</button>";
    html += "</div></div>";
  }

  // Dateien danach
  for (String file : files) {
    String fullPath = currentPath;
    if (fullPath == "/") fullPath = "/" + file;
    else fullPath = currentPath + "/" + file;
    fullPath.replace("//", "/");

    File f = SD.open(fullPath);
    String fileSize = "";
    if (f) {
      int sz = f.size();
      if (sz < 1024) fileSize = String(sz) + " B";
      else if (sz < 1024 * 1024) fileSize = String(sz / 1024) + " KB";
      else fileSize = String(sz / (1024 * 1024)) + " MB";
      f.close();
    }

    html += "<div class='file-item'>";
    html += "<div class='file-name'>📄 " + file + "</div>";
    html += "<div class='file-size'>" + fileSize + "</div>";
    html += "<div class='file-actions'>";
    html += "<button onclick=\"window.location.href='/download?file=" + urlEncode(fullPath) + "'\">⬇️</button>";
    html += "<button onclick='renameItem(\"" + fullPath + "\")'>✏️</button>";
    html += "<button onclick='moveItem(\"" + fullPath + "\")'>📦</button>";
    html += "<button onclick='copyItem(\"" + fullPath + "\")'>📋</button>";
    html += "<button onclick='deleteItem(\"" + fullPath + "\")' style='background:#c0392b'>🗑️</button>";
    html += "</div></div>";
  }

  if (dirs.size() == 0 && files.size() == 0) {
    html += "<div style='text-align:center;padding:40px;color:#888'>📂 Empty directory</div>";
  }

  html += "</div>";

  // Upload Bereich
  html += "<div class='upload-area'>";
  html += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
  html += "<input type='file' name='file' required>";
  html += "<input type='hidden' name='path' value='" + currentPath + "'>";
  html += "<button type='submit' class='btn btn-success'>📤 Upload to current folder</button>";
  html += "</form>";
  html += "</div>";

  html += "<div id='status' class='status'></div>";
  html += "</div></body></html>";

  return html;
}

// URL Encoder für Pfade
String urlEncode(String str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str[i];
    if (c == ' ') encoded += "%20";
    else if (c == '/') encoded += "/";
    else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) encoded += c;
    else if (c == '.') encoded += ".";
    else if (c == '-') encoded += "-";
    else if (c == '_') encoded += "_";
    else {
      char hex[4];
      sprintf(hex, "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  return encoded;
}

// ==================== WEB ROUTES ====================

void handleFileManagerRoot() {
  if (!checkAuth()) return;

  String path = "/";
  if (server.hasArg("path")) {
    path = server.arg("path");
  }

  String html = getFileManagerPage(path);
  server.send(200, "text/html", html);
}

void handleLogin() {
  if (server.hasArg("password")) {
    String pwd = server.arg("password");
    if (pwd == adminPassword) {
      isAuthenticated = true;
      authTimeout = millis() + 3600000;  // 1 Stunde gültig
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.send(200, "text/html", getLoginPage());
}

void handleLogout() {
  isAuthenticated = false;
  server.sendHeader("Location", "/login", true);
  server.send(302, "text/plain", "");
}

void handleDownload() {
  if (!checkAuth()) return;

  if (server.hasArg("file")) {
    String filepath = server.arg("file");
    if (SD.exists(filepath)) {
      File file = SD.open(filepath, FILE_READ);
      if (file) {
        server.streamFile(file, "application/octet-stream");
        file.close();
        return;
      }
    }
  }
  server.send(404, "text/plain", "File not found");
}

void handleUpload() {
  if (!checkAuth()) return;

  HTTPUpload& upload = server.upload();
  static File uploadFile;
  static String uploadPath;

  if (upload.status == UPLOAD_FILE_START) {
    String destPath = "/";
    if (server.hasArg("path")) {
      destPath = server.arg("path");
    }
    String filename = upload.filename;
    uploadPath = destPath;
    if (uploadPath == "/") uploadPath = "/" + filename;
    else uploadPath = uploadPath + "/" + filename;
    uploadPath.replace("//", "/");

    uploadFile = SD.open(uploadPath, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE && uploadFile) {
    uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END && uploadFile) {
    uploadFile.close();
    server.sendHeader("Location", "/?path=" + urlEncode(uploadPath.substring(0, uploadPath.lastIndexOf('/'))), true);
    server.send(302, "text/plain", "");
    return;
  }

  if (!uploadFile && upload.status != UPLOAD_FILE_END) {
    server.send(500, "text/plain", "Upload failed");
  }
}

void handleDelete() {
  if (!checkAuth()) return;

  if (server.hasArg("path")) {
    String path = server.arg("path");
    deleteFileOrDir(path);
    server.send(200, "text/plain", "Deleted: " + path);
  } else {
    server.send(400, "text/plain", "Missing path");
  }
}

void handleRename() {
  if (!checkAuth()) return;

  if (server.hasArg("old") && server.hasArg("new")) {
    String oldPath = server.arg("old");
    String newName = server.arg("new");
    String newPath = oldPath.substring(0, oldPath.lastIndexOf('/') + 1) + newName;

    if (SD.rename(oldPath, newPath)) {
      server.send(200, "text/plain", "Renamed to " + newName);
    } else {
      server.send(500, "text/plain", "Rename failed");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleMove() {
  if (!checkAuth()) return;

  if (server.hasArg("src") && server.hasArg("dst")) {
    String src = server.arg("src");
    String dstDir = server.arg("dst");
    String filename = src.substring(src.lastIndexOf('/') + 1);
    String dst = dstDir;
    if (dst.endsWith("/")) dst += filename;
    else dst += "/" + filename;
    dst.replace("//", "/");

    if (SD.rename(src, dst)) {
      server.send(200, "text/plain", "Moved to " + dst);
    } else {
      server.send(500, "text/plain", "Move failed");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleCopy() {
  if (!checkAuth()) return;

  if (server.hasArg("src") && server.hasArg("dst")) {
    String src = server.arg("src");
    String dstDir = server.arg("dst");
    String filename = src.substring(src.lastIndexOf('/') + 1);
    String dst = dstDir;
    if (dst.endsWith("/")) dst += filename;
    else dst += "/" + filename;
    dst.replace("//", "/");

    copyFileItem(src, dst);
    server.send(200, "text/plain", "Copied to " + dst);
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleMkdir() {
  if (!checkAuth()) return;

  String currentPath = "/";
  if (server.hasArg("path")) {
    currentPath = server.arg("path");
  }

  if (server.hasArg("name")) {
    String dirName = server.arg("name");
    String fullPath = currentPath;
    if (fullPath == "/") fullPath = "/" + dirName;
    else fullPath = currentPath + "/" + dirName;
    fullPath.replace("//", "/");

    if (SD.mkdir(fullPath)) {
      server.send(200, "text/plain", "Created folder: " + dirName);
    } else {
      server.send(500, "text/plain", "Failed to create folder");
    }
  } else {
    server.send(400, "text/plain", "Missing folder name");
  }
}

bool checkAuth() {
  if (!isAuthenticated || (authTimeout > 0 && millis() > authTimeout)) {
    isAuthenticated = false;
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "");
    return false;
  }
  return true;
}

// ==================== START FILE SERVER ====================
void startFileServer() {
  if (fileServerRunning) {
    printToConsole(infoPrefix + "File server already running on port 8080", TFT_YELLOW);
    return;
  }

  // Passwort laden
  adminPassword = loadAdminPassword();
  if (adminPassword == "") {
    printToConsole(errorPrefix + "No admin password found in /psw_admin.txt!", TFT_RED);
    printToConsole(infoPrefix + "Creating default password: admin123", TFT_BLUE);
    adminPassword = "admin123";
    saveAdminPassword(adminPassword);
    printToConsole(successPrefix + "Default password created: admin123", TFT_GREEN);
  }

  // Falls noch kein Passwort da ist, Default setzen
  if (adminPassword.length() == 0) {
    adminPassword = "admin123";
    saveAdminPassword(adminPassword);
  }

  printToConsole(infoPrefix + "Admin password loaded (encrypted)", TFT_GREEN);

  // Routes registrieren
  fileServer.on("/", handleFileManagerRoot);
  fileServer.on("/login", HTTP_GET, handleLogin);
  fileServer.on("/login", HTTP_POST, handleLogin);
  fileServer.on("/logout", handleLogout);
  fileServer.on("/download", handleDownload);
  fileServer.on("/delete", handleDelete);
  fileServer.on("/rename", handleRename);
  fileServer.on("/move", handleMove);
  fileServer.on("/copy", handleCopy);
  fileServer.on("/mkdir", handleMkdir);
  fileServer.on("/upload", HTTP_POST, handleUpload, handleUpload);

  fileServer.begin();
  fileServerRunning = true;

  printToConsole(successPrefix + "File Server started on port 8080", TFT_GREEN);
  printToConsole(infoPrefix + "Access via: http://" + WiFi.localIP().toString() + ":8080", TFT_BLUE);
  printToConsole(infoPrefix + "Password: " + adminPassword, TFT_YELLOW);
}

void stopFileServer() {
  if (!fileServerRunning) return;
  fileServer.stop();
  fileServerRunning = false;
  printToConsole(infoPrefix + "File server stopped", TFT_YELLOW);
}

void handleFileServer() {
  if (fileServerRunning) {
    fileServer.handleClient();
  }
}



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

  int mode = 0;
  String qrData = "";
  bool qrGenerated = false;

  while (true) {
    int tx, ty;
    bool touchDetected = getTouch(tx, ty);

    // Vor der QR-Generierung
    if (!qrGenerated) {
      tft.fillScreen(BG_COLOR);

      // UI Elemente wie gehabt...
      tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
      tft.drawCentreString("QR GENERATOR", 120, 42, 2);
      tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
      tft.drawCentreString("ESC", 207, 10, 2);

      // Modus Buttons
      const char* modes[] = { "TEXT", "URL", "WIFI", "VCARD" };
      for (int i = 0; i < 4; i++) {
        int x = 10 + i * 55;
        uint16_t color = (mode == i) ? SUCCESS_COLOR : BUTTON_COLOR;
        tft.fillRoundRect(x, 70, 50, 25, 3, color);
        tft.drawCentreString(modes[i], x + 25, 82, 1);
      }

      tft.fillRoundRect(10, 105, 220, 45, 4, BUTTON_COLOR);
      tft.setCursor(15, 115);
      tft.print(mode == 0 ? "Enter text:" : mode == 1 ? "Enter URL:"
                                          : mode == 2 ? "SSID:PWD:"
                                                      : "Name:Tel:Email:");

      tft.fillRoundRect(10, 160, 100, 30, 4, SUCCESS_COLOR);
      tft.drawCentreString("GENERATE", 60, 175, 1);
      tft.drawRect(35, 200, 170, 170, TEXT_COLOR);

      if (touchDetected) {
        if (tx > 180 && ty < 40) break;

        for (int i = 0; i < 4; i++) {
          int x = 10 + i * 55;
          if (ty > 70 && ty < 95 && tx > x && tx < x + 50) {
            mode = i;
            qrData = "";
            playSysSound(0);
          }
        }

        if (tx < 110 && ty > 160 && ty < 190) {
          printToConsole(infoPrefix + "Enter data:", TFT_BLUE);

          if (mode == 0) qrData = getTextInput();
          else if (mode == 1) {
            qrData = getTextInput();
            if (!qrData.startsWith("http")) qrData = "http://" + qrData;
          } else if (mode == 2) {
            printToConsole(infoPrefix + "Enter SSID:", TFT_BLUE);
            String ssid = getTextInput();
            printToConsole(infoPrefix + "Enter password:", TFT_BLUE);
            String pwd = getTextInput();
            qrData = "WIFI:S:" + ssid + ";T:WPA;P:" + pwd + ";;";
          } else if (mode == 3) {
            printToConsole(infoPrefix + "Enter name:", TFT_BLUE);
            String name = getTextInput();
            printToConsole(infoPrefix + "Enter phone:", TFT_BLUE);
            String phone = getTextInput();
            printToConsole(infoPrefix + "Enter email:", TFT_BLUE);
            String email = getTextInput();
            qrData = "BEGIN:VCARD\nVERSION:3.0\nFN:" + name + "\nTEL:" + phone + "\nEMAIL:" + email + "\nEND:VCARD";
          }

          if (qrData != "") {
            qrGenerated = true;
            playSysSound(1);
          }
        }
      }
    }

    // QR-Code wird EINMAL generiert und angezeigt
    else {
      // Bildschirm komplett löschen
      tft.fillScreen(BG_COLOR);

      // Header
      tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.drawCentreString("QR CODE", 120, 42, 2);

      // ESC Button (einzige Möglichkeit)
      tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
      tft.drawCentreString("ESC", 207, 10, 2);

      // QR-Code Bereich
      tft.drawRect(35, 80, 170, 170, TEXT_COLOR);

      // QR-Code NUR EINMAL hier generieren!
      QRcode_eSPI qrcode(&tft);
      qrcode.init();
      qrcode.create(qrData);

      // Jetzt nur noch auf ESC warten - keine weiteren Aktionen
      while (true) {
        if (getTouch(tx, ty)) {
          if (tx > 180 && ty < 40) {
            break;  // ESC gedrückt - App beenden
          }
          // Alle anderen Berührungen werden komplett ignoriert!
        }
        delay(20);
      }

      break;  // Aus der Hauptschleife
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


// Forward Deklarationen
void createQRCode(String data, int size, int xOffset, int yOffset);
void qrGeneratorApp();







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

// ==================== VERBESSERTE HELP-FUNKTION ====================

void writeHelpFile() {
  printToConsole(infoPrefix + "Creating enhanced help system...", TFT_BLUE);

  // ==================== HAUPT-HILFE (Übersicht) ====================
  SD.remove("/help.txt");
  File f = SD.open("/help.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║                    CYD TERMINAL OS v3.1                              ║");
    f.println("║                    ESP32-WROOM Command Reference                     ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ QUICK REFERENCE - Most Important Commands                           │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│ help, ?          - This help system                                 │");
    f.println("│ help <category>  - Category help (system, files, net, games, etc)   │");
    f.println("│ man <cmd>        - Manual page for specific command                 │");
    f.println("│ cls, clear       - Clear terminal screen                            │");
    f.println("│ reboot           - Restart system                                   │");
    f.println("│ darkmode         - Toggle dark/light mode                           │");
    f.println("│ sound            - Toggle system sounds                             │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ COMMAND CATEGORIES (use 'help <category>')                          │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│ system    - System commands (reboot, info, settings)                │");
    f.println("│ files     - File operations (ls, cat, rm, mkdir)                    │");
    f.println("│ net       - Network & WiFi (scan, connect, fileserver)              │");
    f.println("│ games     - Games (snake, pong, tictac, chip8)                      │");
    f.println("│ apps      - Applications (calc, draw, todo, timer)                  │");
    f.println("│ i2c       - I2C bus tools (scanner, reader, writer)                 │");
    f.println("│ dev       - Developer tools (echo, eval, delay)                     │");
    f.println("│ qr        - QR Code Generator                                      │");
    f.println("│ help      - This help system                                        │");
    f.println("│ tips      - Tips & tricks                                           │");
    f.println("│ all       - Complete command reference                              │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ TIPS                                                               │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│ • Touch keyboard: scroll with ▲▼ buttons on right                   │");
    f.println("│ • ESC button: top-right corner to exit apps                         │");
    f.println("│ • Commands are case-insensitive                                     │");
    f.println("│ • Use TAB for auto-completion (coming soon)                         │");
    f.println("│ • File server: http://<IP>:8080 after 'fileserver'                  │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== SYSTEM CATEGORY ====================
  SD.remove("/help_system.txt");
  f = SD.open("/help_system.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ SYSTEM COMMANDS                                                      ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ BASIC SYSTEM COMMANDS                                               │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ cls, clear                                                          │");
    f.println("│   Clears the terminal screen completely                             │");
    f.println("│   Usage: cls                                                        │");
    f.println("│                                                                     │");
    f.println("│ reboot, restart                                                     │");
    f.println("│   Restarts the CYD OS                                               │");
    f.println("│   Usage: reboot                                                     │");
    f.println("│                                                                     │");
    f.println("│ darkmode, theme                                                     │");
    f.println("│   Toggle between dark and light theme                               │");
    f.println("│   Usage: darkmode                                                   │");
    f.println("│                                                                     │");
    f.println("│ sound                                                               │");
    f.println("│   Toggle system sounds on/off                                       │");
    f.println("│   Usage: sound                                                      │");
    f.println("│                                                                     │");
    f.println("│ settings                                                            │");
    f.println("│   Open graphical settings menu                                      │");
    f.println("│   Usage: settings                                                   │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ SYSTEM INFORMATION                                                  │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ sysinfo, stats                                                      │");
    f.println("│   Displays detailed hardware information                            │");
    f.println("│   Usage: sysinfo                                                    │");
    f.println("│                                                                     │");
    f.println("│ neofetch                                                            │");
    f.println("│   Shows ASCII art system info banner                                │");
    f.println("│   Usage: neofetch                                                   │");
    f.println("│                                                                     │");
    f.println("│ free                                                                │");
    f.println("│   Shows RAM and Flash memory usage                                  │");
    f.println("│   Usage: free                                                       │");
    f.println("│                                                                     │");
    f.println("│ ps                                                                  │");
    f.println("│   Lists running processes                                           │");
    f.println("│   Usage: ps                                                         │");
    f.println("│                                                                     │");
    f.println("│ sd, storage                                                         │");
    f.println("│   SD card status and capacity                                       │");
    f.println("│   Usage: sd                                                         │");
    f.println("│                                                                     │");
    f.println("│ date                                                                │");
    f.println("│   Shows time since boot                                             │");
    f.println("│   Usage: date                                                       │");
    f.println("│                                                                     │");
    f.println("│ uptime                                                              │");
    f.println("│   Shows system uptime in HH:MM:SS                                   │");
    f.println("│   Usage: uptime                                                     │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== FILES CATEGORY ====================
  SD.remove("/help_files.txt");
  f = SD.open("/help_files.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ FILE OPERATIONS                                                      ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ DIRECTORY NAVIGATION                                                │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ ls [path]                                                           │");
    f.println("│   List directory contents (files and folders)                       │");
    f.println("│   Examples: ls, ls /, ls /folder                                    │");
    f.println("│                                                                     │");
    f.println("│ dir [path]                                                          │");
    f.println("│   Alias for ls                                                      │");
    f.println("│   Usage: dir                                                        │");
    f.println("│                                                                     │");
    f.println("│ cd <path>                                                           │");
    f.println("│   Change directory (for file operations)                            │");
    f.println("│   Usage: cd /folder                                                 │");
    f.println("│                                                                     │");
    f.println("│ pwd                                                                 │");
    f.println("│   Print working directory                                           │");
    f.println("│   Usage: pwd                                                        │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ FILE VIEWING                                                        │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ cat <file>                                                          │");
    f.println("│   Display entire file contents (use for small files)                │");
    f.println("│   Usage: cat /readme.txt                                            │");
    f.println("│                                                                     │");
    f.println("│ head [-n<lines>] <file>                                             │");
    f.println("│   Show first N lines (default 10)                                   │");
    f.println("│   Examples: head log.txt, head -n20 log.txt                         │");
    f.println("│                                                                     │");
    f.println("│ tail [-n<lines>] <file>                                             │");
    f.println("│   Show last N lines (default 10)                                    │");
    f.println("│   Examples: tail log.txt, tail -n50 log.txt                         │");
    f.println("│                                                                     │");
    f.println("│ grep <pattern> <file>                                               │");
    f.println("│   Search for pattern in file (case-sensitive)                       │");
    f.println("│   Usage: grep 'error' /log.txt                                      │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ FILE CREATION & DELETION                                            │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ touch <file>                                                        │");
    f.println("│   Create an empty file                                              │");
    f.println("│   Usage: touch /newfile.txt                                         │");
    f.println("│                                                                     │");
    f.println("│ mkdir <dir>                                                         │");
    f.println("│   Create a directory                                                │");
    f.println("│   Usage: mkdir /newfolder                                           │");
    f.println("│                                                                     │");
    f.println("│ rm <file/dir>                                                       │");
    f.println("│   Delete file or empty directory                                    │");
    f.println("│   Usage: rm /oldfile.txt                                            │");
    f.println("│                                                                     │");
    f.println("│ rmdir <dir>                                                         │");
    f.println("│   Delete empty directory                                            │");
    f.println("│   Usage: rmdir /emptyfolder                                         │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ FILE EDITING                                                        │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ edit, editor, nano <file>                                           │");
    f.println("│   Open graphical text editor with on-screen keyboard                │");
    f.println("│   Features: cursor movement, line numbers, copy/paste support      │");
    f.println("│   Usage: edit myfile.txt                                            │");
    f.println("│                                                                     │");
    f.println("│ files, fm                                                           │");
    f.println("│   Open graphical file manager (GUI)                                 │");
    f.println("│   Features: browse, open, delete, rename, copy, move               │");
    f.println("│   Usage: files                                                      │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== NETWORK CATEGORY ====================
  SD.remove("/help_net.txt");
  f = SD.open("/help_net.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ NETWORK & WIFI TOOLS                                                 ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ WIFI CONNECTION MANAGEMENT                                           │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ wifi, wifimanager                                                   │");
    f.println("│   Open graphical WiFi manager                                       │");
    f.println("│   Features: scan, connect, save networks                            │");
    f.println("│   Usage: wifi                                                       │");
    f.println("│                                                                     │");
    f.println("│ scan, wifi-scan                                                     │");
    f.println("│   Scan for available WiFi networks                                  │");
    f.println("│   Shows: SSID, signal strength, encryption                          │");
    f.println("│   Usage: scan                                                       │");
    f.println("│                                                                     │");
    f.println("│ wifi auto, autoconnect                                              │");
    f.println("│   Automatically connect to best saved network                       │");
    f.println("│   Usage: wifi auto                                                  │");
    f.println("│                                                                     │");
    f.println("│ wifi add <SSID> <PASSWORD>                                          │");
    f.println("│   Connect and save a WiFi network                                   │");
    f.println("│   Usage: wifi add MyWiFi mypassword                                 │");
    f.println("│                                                                     │");
    f.println("│ wifi list, savedwifi                                                │");
    f.println("│   List all saved WiFi networks                                      │");
    f.println("│   Usage: savedwifi                                                  │");
    f.println("│                                                                     │");
    f.println("│ wifi del <SSID>                                                     │");
    f.println("│   Delete a saved WiFi network                                       │");
    f.println("│   Usage: wifi del MyWiFi                                            │");
    f.println("│                                                                     │");
    f.println("│ wifi clear                                                          │");
    f.println("│   Delete ALL saved WiFi networks                                    │");
    f.println("│   Usage: wifi clear                                                 │");
    f.println("│                                                                     │");
    f.println("│ wifi on / wifi off                                                  │");
    f.println("│   Enable or disable WiFi radio                                      │");
    f.println("│   Usage: wifi on                                                    │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ NETWORK INFORMATION                                                 │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ ifconfig, ip                                                        │");
    f.println("│   Show network configuration (IP, MAC, gateway, DNS)                │");
    f.println("│   Usage: ifconfig                                                   │");
    f.println("│                                                                     │");
    f.println("│ wifistatus, wifi-status                                             │");
    f.println("│   Detailed WiFi connection status with signal graph                 │");
    f.println("│   Usage: wifistatus                                                 │");
    f.println("│                                                                     │");
    f.println("│ wifiscan                                                            │");
    f.println("│   Graphical WiFi scanner with connection option                     │");
    f.println("│   Usage: wifiscan                                                   │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ FILE SERVER                                                         │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ fileserver, fs                                                      │");
    f.println("│   Start web-based file server on port 8080                          │");
    f.println("│   Access via: http://<IP>:8080                                      │");
    f.println("│   Features: browse, upload, download, delete, rename               │");
    f.println("│   Usage: fileserver                                                 │");
    f.println("│                                                                     │");
    f.println("│ fsstop, stopfs                                                      │");
    f.println("│   Stop the file server                                              │");
    f.println("│   Usage: fsstop                                                     │");
    f.println("│                                                                     │");
    f.println("│ fsstatus                                                            │");
    f.println("│   Check if file server is running                                   │");
    f.println("│   Usage: fsstatus                                                   │");
    f.println("│                                                                     │");
    f.println("│ setpwd <password>                                                   │");
    f.println("│   Change file server admin password                                 │");
    f.println("│   Usage: setpwd newpassword123                                      │");
    f.println("│                                                                     │");
    f.println("│ adpsw, adminpass                                                    │");
    f.println("│   Graphical admin password manager                                  │");
    f.println("│   Usage: adpsw                                                      │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ NETWORK UTILITIES                                                   │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ ping <host>                                                         │");
    f.println("│   Test network connectivity to a host                               │");
    f.println("│   Usage: ping google.com                                            │");
    f.println("│                                                                     │");
    f.println("│ wget <url>                                                          │");
    f.println("│   Download a file from the internet                                 │");
    f.println("│   Usage: wget example.com/file.txt                                  │");
    f.println("│                                                                     │");
    f.println("│ clock, time, zeit                                                   │");
    f.println("│   Show current date and time (requires WiFi)                        │");
    f.println("│   Usage: clock                                                      │");
    f.println("│                                                                     │");
    f.println("│ chat                                                                │");
    f.println("│   Simple chat application (host or client mode)                     │");
    f.println("│   Usage: chat                                                       │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== GAMES CATEGORY ====================
  SD.remove("/help_games.txt");
  f = SD.open("/help_games.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ GAMES & ENTERTAINMENT                                                ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ CLASSIC GAMES                                                       │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ snake                                                               │");
    f.println("│   Classic Snake game - eat food, grow, avoid walls                  │");
    f.println("│   Controls: Touch top/left/right/bottom of screen                   │");
    f.println("│   Usage: snake                                                      │");
    f.println("│                                                                     │");
    f.println("│ pong                                                                │");
    f.println("│   Pong game vs AI - move paddle to hit ball                         │");
    f.println("│   Controls: Touch anywhere to move paddle                           │");
    f.println("│   Usage: pong                                                       │");
    f.println("│                                                                     │");
    f.println("│ tictac, ttt                                                         │");
    f.println("│   Tic-Tac-Toe vs AI opponent                                        │");
    f.println("│   Controls: Touch grid cells to place X                             │");
    f.println("│   Usage: tictac                                                     │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ CHIP-8 EMULATOR                                                     │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ chip8, chip-8                                                       │");
    f.println("│   CHIP-8 game emulator - play classic games!                        │");
    f.println("│                                                                     │");
    f.println("│   Setup:                                                            │");
    f.println("│     1. Copy .ch8 ROM files to SD card root                          │");
    f.println("│     2. Run 'chip8' to see ROM list                                  │");
    f.println("│     3. Select ROM and press LOAD                                    │");
    f.println("│                                                                     │");
    f.println("│   Controls:                                                         │");
    f.println("│     Touch keypad on screen (1-F) for game input                     │");
    f.println("│     SPEED button - change emulation speed                           │");
    f.println("│     RST button - reset emulator                                     │");
    f.println("│     ESC - exit emulator                                             │");
    f.println("│                                                                     │");
    f.println("│   Supported ROMs: .ch8, .CH8, .c8, .bin                             │");
    f.println("│   Max ROM size: 3584 bytes                                          │");
    f.println("│                                                                     │");
    f.println("│   Popular ROMs (download online):                                   │");
    f.println("│     - PONG, SPACE INVADERS, TETRIS                                  │");
    f.println("│     - BREAKOUT, MAZE, BLINKY                                        │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ RANDOM FUN                                                          │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ dice <sides>                                                        │");
    f.println("│   Roll a dice with N sides (1-100)                                  │");
    f.println("│   Examples: dice 6, dice 20, dice 100                               │");
    f.println("│                                                                     │");
    f.println("│ random [max]                                                        │");
    f.println("│   Generate a random number (0-max, default 1000)                    │");
    f.println("│   Examples: random, random 100                                      │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== APPS CATEGORY ====================
  SD.remove("/help_apps.txt");
  f = SD.open("/help_apps.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ APPLICATIONS                                                         ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ CALCULATOR                                                          │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ calc, rechner                                                       │");
    f.println("│   Scientific calculator with 3 modes                                │");
    f.println("│                                                                     │");
    f.println("│   Modes:                                                            │");
    f.println("│     STD - Basic arithmetic (+, -, *, /)                             │");
    f.println("│     SCI - Scientific functions (sin, cos, tan, log, sqrt, etc)      │");
    f.println("│     PROG - Programmer mode (AND, OR, XOR, SHIFT, BIN, HEX, DEC)     │");
    f.println("│                                                                     │");
    f.println("│   Features:                                                         │");
    f.println("│     Memory (M+, M-, MR, MC)                                         │");
    f.println("│     Constants (π, e)                                                │");
    f.println("│     Power functions (x², x³, xʸ, eˣ, 10ˣ)                          │");
    f.println("│                                                                     │");
    f.println("│   Usage: calc                                                       │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ DRAWING APP                                                         │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ draw, paint                                                         │");
    f.println("│   Full-featured drawing application                                 │");
    f.println("│                                                                     │");
    f.println("│   Tools:                                                            │");
    f.println("│     BRUSH, LINE, RECT, CIRCLE, FILL, ERASER, PICKER, TEXT           │");
    f.println("│                                                                     │");
    f.println("│   Colors:                                                           │");
    f.println("│     32 predefined colors + RGB mixer                                │");
    f.println("│                                                                     │");
    f.println("│   Brush sizes: 1-6 pixels                                           │");
    f.println("│                                                                     │");
    f.println("│   Usage: draw                                                       │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ QR CODE GENERATOR                                                   │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ qr, qrcode                                                          │");
    f.println("│   Generate QR codes from text, URLs, WiFi, or vCards                │");
    f.println("│                                                                     │");
    f.println("│   Modes:                                                            │");
    f.println("│     TEXT - Any text message                                         │");
    f.println("│     URL - Website link (auto-adds http://)                          │");
    f.println("│     WIFI - WiFi credentials (SSID:PASSWORD)                         │");
    f.println("│     VCARD - Contact info (Name:Tel:Email)                           │");
    f.println("│                                                                     │");
    f.println("│   Usage: qr                                                         │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ PRODUCTIVITY APPS                                                   │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ todo                                                                │");
    f.println("│   To-do list manager with save/load                                 │");
    f.println("│   Features: add, delete, toggle done                                │");
    f.println("│   Usage: todo                                                       │");
    f.println("│                                                                     │");
    f.println("│ timer                                                               │");
    f.println("│   Countdown timer with start/pause/reset                            │");
    f.println("│   Usage: timer                                                      │");
    f.println("│                                                                     │");
    f.println("│ notes                                                               │");
    f.println("│   Simple note-taking app (placeholder)                              │");
    f.println("│   Usage: notes                                                      │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ TEXT EDITOR                                                         │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ edit, editor, nano <filename>                                       │");
    f.println("│   Full-featured text editor with on-screen keyboard                 │");
    f.println("│                                                                     │");
    f.println("│   Features:                                                         │");
    f.println("│     - Cursor movement (arrow buttons)                               │");
    f.println("│     - Line numbers                                                  │");
    f.println("│     - Insert/delete characters                                      │");
    f.println("│     - Save/Load files                                               │");
    f.println("│     - Visual feedback                                               │");
    f.println("│                                                                     │");
    f.println("│   Usage: edit myfile.txt                                            │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== I2C CATEGORY ====================
  SD.remove("/help_i2c.txt");
  f = SD.open("/help_i2c.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ I2C BUS TOOLS                                                        ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ HARDWARE INFO                                                       │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   I2C Pins: SDA = GPIO16, SCL = GPIO39                              │");
    f.println("│   Voltage: 3.3V (use level shifters for 5V devices)                 │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ SCANNING & DISCOVERY                                                │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ i2c, i2cscan                                                        │");
    f.println("│   Scan I2C bus for connected devices                                │");
    f.println("│   Shows all addresses (0x01-0x7F) that respond                      │");
    f.println("│   Usage: i2cscan                                                    │");
    f.println("│                                                                     │");
    f.println("│ i2ctool                                                             │");
    f.println("│   Graphical I2C tool with menu interface                            │");
    f.println("│   Features: scan, send, read, register operations                   │");
    f.println("│   Usage: i2ctool                                                    │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ DATA TRANSFER                                                       │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ i2csend <addr> <data>                                               │");
    f.println("│   Send data to I2C device                                           │");
    f.println("│   Data can be text or hex (0x prefix)                               │");
    f.println("│   Examples:                                                         │");
    f.println("│     i2csend 3C Hello World                                          │");
    f.println("│     i2csend 3C 0x48656C6C6F                                         │");
    f.println("│                                                                     │");
    f.println("│ i2cread <addr> <bytes>                                              │");
    f.println("│   Read specified bytes from I2C device                              │");
    f.println("│   Usage: i2cread 3C 10                                              │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ REGISTER OPERATIONS                                                 │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ i2cwrite <addr> <reg> <data>                                        │");
    f.println("│   Write data to a specific register                                 │");
    f.println("│   Usage: i2cwrite 3C 00 01                                          │");
    f.println("│                                                                     │");
    f.println("│ i2cregread <addr> <reg> <bytes>                                     │");
    f.println("│   Read bytes from a specific register                               │");
    f.println("│   Usage: i2cregread 3C 00 5                                         │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ COMMON I2C DEVICES                                                  │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   0x27 / 0x3F   - LCD 1602/2004 (PCF8574)                           │");
    f.println("│   0x3C / 0x3D   - OLED Display (SSD1306)                            │");
    f.println("│   0x68          - RTC DS3231 / MPU6050                              │");
    f.println("│   0x57          - EEPROM 24Cxx                                      │");
    f.println("│   0x40          - PCA9685 PWM servo driver                          │");
    f.println("│   0x76 / 0x77   - BME280 temperature/pressure sensor                │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== DEVELOPER CATEGORY ====================
  SD.remove("/help_dev.txt");
  f = SD.open("/help_dev.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ DEVELOPER TOOLS                                                      ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ TEXT OUTPUT                                                         │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ echo <text>                                                         │");
    f.println("│   Print text to terminal                                            │");
    f.println("│   Variables: $TIME, $FREE, $UPTIME, $HEAP                          │");
    f.println("│   Examples:                                                         │");
    f.println("│     echo Hello World                                                 │");
    f.println("│     echo Free RAM: $FREE bytes                                      │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ MATH & EVALUATION                                                   │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ eval <expression>                                                   │");
    f.println("│   Evaluate mathematical expression                                  │");
    f.println("│   Supports: + - * / ^ ( ) sqrt() sin() cos() tan()                  │");
    f.println("│   Examples:                                                         │");
    f.println("│     eval 2+2                                                        │");
    f.println("│     eval sqrt(16)                                                   │");
    f.println("│     eval sin(30)                                                    │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ UTILITIES                                                           │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│ delay, sleep <ms>                                                   │");
    f.println("│   Wait for specified milliseconds (max 10000ms)                     │");
    f.println("│   Usage: delay 1000                                                 │");
    f.println("│                                                                     │");
    f.println("│ colors                                                              │");
    f.println("│   List all available TFT color constants                            │");
    f.println("│   Usage: colors                                                     │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  // ==================== TIPS CATEGORY ====================
  SD.remove("/help_tips.txt");
  f = SD.open("/help_tips.txt", FILE_WRITE);
  if (f) {
    f.println("╔══════════════════════════════════════════════════════════════════════╗");
    f.println("║ TIPS & TRICKS                                                        ║");
    f.println("╚══════════════════════════════════════════════════════════════════════╝");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ TOUCH KEYBOARD SHORTCUTS                                            │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   ^ button      - Shift (uppercase letters)                         │");
    f.println("│   123 button    - Numbers and basic symbols                         │");
    f.println("│   §$% button    - Special characters and symbols                    │");
    f.println("│   OK button     - Enter/Execute command                             │");
    f.println("│   < button      - Backspace / Delete                                │");
    f.println("│   SPACE bar     - Space character                                   │");
    f.println("│                                                                     │");
    f.println("│   ▲ button (right) - Scroll terminal up                             │");
    f.println("│   ▼ button (right) - Scroll terminal down                           │");
    f.println("│   ESC button (top-right) - Exit current app                         │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ TERMINAL FEATURES                                                   │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   • Commands are case-insensitive (HELP = help)                     │");
    f.println("│   • Press ESC at any time to return to terminal                     │");
    f.println("│   • Scroll through command history using ▲▼ buttons                 │");
    f.println("│   • Terminal shows last 40 lines                                    │");
    f.println("│   • File paths can use / or start with /                            │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ CHIP-8 EMULATOR TIPS                                                │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   • Place .ch8 ROM files in SD card root                            │");
    f.println("│   • Some ROMs require different speed settings                      │");
    f.println("│   • Use SPEED button to adjust emulation speed                      │");
    f.println("│   • RST button resets the emulator                                  │");
    f.println("│   • Keypad layout mimics original CHIP-8                            │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ WIFI & NETWORK TIPS                                                 │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   • Use 'scan' to find networks before connecting                   │");
    f.println("│   • Saved networks persist after reboot                             │");
    f.println("│   • File server requires active WiFi connection                     │");
    f.println("│   • Default admin password: admin123 (change with 'setpwd')         │");
    f.println("│   • Use 'wifi auto' to auto-connect to best saved network           │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.println("");
    f.println("┌─────────────────────────────────────────────────────────────────────┐");
    f.println("│ TROUBLESHOOTING                                                     │");
    f.println("├─────────────────────────────────────────────────────────────────────┤");
    f.println("│                                                                     │");
    f.println("│   Issue: Screen freezes or unresponsive                             │");
    f.println("│   Fix: Press RST button on ESP32 board                              │");
    f.println("│                                                                     │");
    f.println("│   Issue: SD card not detected                                       │");
    f.println("│   Fix: Check FAT32 format, re-insert card, check wiring             │");
    f.println("│                                                                     │");
    f.println("│   Issue: Touch not responding                                       │");
    f.println("│   Fix: Check XPT2046 connections, reset device                     │");
    f.println("│                                                                     │");
    f.println("│   Issue: I2C device not found                                       │");
    f.println("│   Fix: Run 'i2cscan' first, check wiring and pull-up resistors      │");
    f.println("│                                                                     │");
    f.println("│   Issue: Cannot connect to WiFi                                     │");
    f.println("│   Fix: Check password, signal strength, try 'scan' first            │");
    f.println("│                                                                     │");
    f.println("└─────────────────────────────────────────────────────────────────────┘");
    f.close();
  }

  printToConsole(successPrefix + "Enhanced help system created!", TFT_GREEN);
  printToConsole(infoPrefix + "Categories: system, files, net, games, apps, i2c, dev, tips, all", TFT_BLUE);
}

// ==================== IMPROVED MAN COMMAND ====================
// Ersetze in der loop() die man-Befehl Handler

// Füge diese Funktion vor der loop() ein:
void manualPage(String command) {
  command.toLowerCase();

  // Define command manuals
  struct CommandManual {
    String cmd;
    String title;
    String syntax;
    String description;
    String examples;
    String notes;
  };

  std::vector<CommandManual> manuals = {
    { "ls", "List Directory Contents",
      "ls [path]",
      "Lists all files and directories in the specified path.\nIf no path is given, lists the current directory.",
      "  ls           - List current directory\n  ls /         - List root directory\n  ls /folder   - List specific folder",
      "Directories are shown with [DIR] prefix.\nFiles are shown with [FILE] prefix." },

    { "cat", "Display File Contents",
      "cat <filename>",
      "Displays the contents of a text file on the terminal.\nShows first 20 lines for large files.",
      "  cat readme.txt     - Show contents of readme.txt\n  cat /config/settings.txt",
      "Use head/tail for large files.\nBinary files may display garbled text." },

    { "grep", "Search File for Pattern",
      "grep \"pattern\" <filename>",
      "Searches the specified file for lines containing the pattern.",
      "  grep \"error\" log.txt     - Find error lines\n  grep \"192.168\" network.log",
      "Pattern is case-sensitive.\nUse quotes for patterns with spaces." },

    { "edit", "Text Editor",
      "edit <filename>",
      "Opens the graphical text editor with on-screen keyboard.\nCreate new file or edit existing one.",
      "  edit mynotes.txt    - Edit or create mynotes.txt\n  edit /config/settings.cfg",
      "Save with top-left button.\nExit with ESC button." },

    { "scan", "WiFi Scanner",
      "scan",
      "Scans for available WiFi networks and displays them with signal strength.\nTouch a network to connect.",
      "  scan",
      "Shows encryption type and signal bars.\nTouch network to connect." },

    { "chip8", "CHIP-8 Emulator",
      "chip8",
      "Starts the CHIP-8 game emulator.\nRequires .ch8 ROM files on SD card.",
      "  chip8",
      "Place ROMs in SD root.\nUse SPEED button for different speeds." },

    { "calc", "Calculator",
      "calc",
      "Opens scientific calculator with Standard, Scientific, and Programmer modes.",
      "  calc",
      "Touch mode buttons to switch.\nHas memory functions (M+, M-, MR, MC)." },

    { "draw", "Drawing Application",
      "draw",
      "Opens full-featured drawing app with 32 colors and 8 tools.",
      "  draw",
      "Select color from palette or RGB mixer.\nUse brush, line, rect, circle, fill, eraser, picker, or text tools." },

    { "qr", "QR Code Generator",
      "qr",
      "Create QR codes for text, URLs, WiFi credentials, or vCards.",
      "  qr",
      "Select mode (TEXT/URL/WIFI/VCARD), enter data, generate QR code." },

    { "fileserver", "Web File Server",
      "fileserver",
      "Starts web-based file manager accessible via browser.\nPort: 8080",
      "  fileserver",
      "Access at http://<ESP32_IP>:8080\nDefault password: admin123\nUse setpwd to change password." },

    { "i2cscan", "I2C Bus Scanner",
      "i2cscan",
      "Scans I2C bus for connected devices and shows their addresses.",
      "  i2cscan",
      "I2C pins: SDA=GPIO16, SCL=GPIO39\nUse 3.3V devices or level shifters." },

    { "snake", "Snake Game",
      "snake",
      "Classic snake game - eat food, grow longer, avoid walls and yourself.",
      "  snake",
      "Controls: Touch top=up, bottom=down, left=left, right=right." },

    { "pong", "Pong Game",
      "pong",
      "Pong game vs AI opponent - hit the ball with your paddle.",
      "  pong",
      "Controls: Touch anywhere to move paddle vertically." },

    { "tictac", "Tic-Tac-Toe",
      "tictac",
      "Tic-Tac-Toe game against AI opponent.",
      "  tictac",
      "Controls: Touch grid cells to place X." }
  };

  bool found = false;

  for (const auto& manual : manuals) {
    if (manual.cmd == command) {
      found = true;

      printToConsole("");
      printToConsole("╔══════════════════════════════════════════════════════════════╗", TFT_CYAN);
      printToConsole("║ MANUAL: " + manual.title + String(52 - manual.title.length(), ' ') + "║", TFT_CYAN);
      printToConsole("╚══════════════════════════════════════════════════════════════╝", TFT_CYAN);
      printToConsole("");

      printToConsole("SYNTAX", TFT_YELLOW);
      printToConsole("  " + manual.syntax, TFT_GREEN);
      printToConsole("");

      printToConsole("DESCRIPTION", TFT_YELLOW);
      printToConsole("  " + manual.description, TFT_WHITE);
      printToConsole("");

      printToConsole("EXAMPLES", TFT_YELLOW);
      // Split examples by newline
      String examples = manual.examples;
      int start = 0;
      int end = examples.indexOf('\n');
      while (end >= 0) {
        printToConsole(examples.substring(start, end), TFT_GREEN);
        start = end + 1;
        end = examples.indexOf('\n', start);
      }
      if (start < examples.length()) {
        printToConsole(examples.substring(start), TFT_GREEN);
      }
      printToConsole("");

      if (manual.notes.length() > 0) {
        printToConsole("NOTES", TFT_YELLOW);
        String notes = manual.notes;
        start = 0;
        end = notes.indexOf('\n');
        while (end >= 0) {
          printToConsole("  " + notes.substring(start, end), TFT_BLUE);
          start = end + 1;
          end = notes.indexOf('\n', start);
        }
        if (start < notes.length()) {
          printToConsole("  " + notes.substring(start), TFT_BLUE);
        }
        printToConsole("");
      }

      break;
    }
  }

  if (!found) {
    printToConsole(errorPrefix + "No manual entry for: " + command, TFT_RED);
    printToConsole(infoPrefix + "Try 'help' to see available commands", TFT_BLUE);
  }
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
// ==================== VOLLSTÄNDIGE LOOP() MIT ALLEN BEFEHLEN ====================

unsigned long lastFileServerCheck = 0;

void loop() {
  // Cursor blinken lassen
  if (millis() - lastCursorBlink > 500) {
    cursorVisible = !cursorVisible;
    lastCursorBlink = millis();
    updateInputLine(false);
  }

  // File Server regelmäßig bedienen (wenn aktiv)
  if (fileServerRunning && millis() - lastFileServerCheck > 10) {
    handleFileServer();
    lastFileServerCheck = millis();
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

    else if (lowCmd.startsWith("man ")) {
      String searchTerm = lowCmd.substring(4);
      manualPage(searchTerm);
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
      if (fileServerRunning) printToConsole("5    running   fileserver");
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
      if (WiFi.status() == WL_CONNECTED) {
        printToConsole("WiFi: Connected to " + WiFi.SSID(), TFT_GREEN);
        printToConsole("IP: " + WiFi.localIP().toString(), TFT_GREEN);
      }
      if (fileServerRunning) {
        printToConsole("File Server: Running on port 8080", TFT_GREEN);
      }
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

    // ==================== NETZWERK & WIFI (ERWEITERT) ====================
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

    // ==================== ERWEITERTE WIFI BEFEHLE ====================
    else if (lowCmd == "wifi") {
      wifiManagerEnhanced();
    }

    else if (lowCmd == "wifi auto" || lowCmd == "autoconnect") {
      autoConnectWiFi();
    }

    else if (lowCmd == "wifi list" || lowCmd == "savedwifi") {
      listSavedWiFi();
    }

    else if (lowCmd.startsWith("wifi add ")) {
      String rest = lowCmd.substring(8);
      int spacePos = rest.indexOf(' ');
      if (spacePos > 0) {
        String ssid = rest.substring(0, spacePos);
        String pwd = rest.substring(spacePos + 1);
        connectAndSaveWiFi(ssid, pwd);
      } else {
        printToConsole(errorPrefix + "Usage: wifi add <SSID> <PASSWORD>", TFT_RED);
      }
    }

    else if (lowCmd.startsWith("wifi del ")) {
      String ssid = lowCmd.substring(9);
      deleteSavedWiFi(ssid);
    }

    else if (lowCmd == "wifi clear") {
      printToConsole(infoPrefix + "Delete ALL saved WiFis? (y/n)", TFT_YELLOW);
      String confirm = getTextInput();
      if (confirm == "y" || confirm == "yes") {
        for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
          savedNetworks[i].valid = false;
        }
        saveSavedWiFi();
        WiFi.disconnect();
        printToConsole(successPrefix + "All saved WiFis cleared!", TFT_GREEN);
      } else {
        printToConsole(infoPrefix + "Cancelled", TFT_BLUE);
      }
    }

    else if (lowCmd == "wifi off" || lowCmd == "wifioff") {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      printToConsole(infoPrefix + "WiFi turned off", TFT_YELLOW);
    }

    else if (lowCmd == "wifi on") {
      WiFi.mode(WIFI_STA);
      printToConsole(infoPrefix + "WiFi turned on. Use 'wifi auto' or 'wifi' to connect", TFT_GREEN);
    }

    else if (lowCmd == "scan" || lowCmd == "wifi-scan" || lowCmd == "networks") {
      wifiScanner();
    }

    else if (lowCmd == "wifistatus" || lowCmd == "wifi-status") {
      printWiFiStatus();
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

    // ==================== FILE SERVER BEFEHLE ====================
    else if (lowCmd == "fileserver" || lowCmd == "fs") {
      if (WiFi.status() != WL_CONNECTED) {
        printToConsole(errorPrefix + "WiFi not connected! Use 'wifi' or 'wifi auto' first.", TFT_RED);
      } else {
        if (!fileServerRunning) {
          startFileServer();
        } else {
          printToConsole(infoPrefix + "File server already running at: http://" + WiFi.localIP().toString() + ":8080", TFT_BLUE);
        }
      }
    }

    else if (lowCmd == "fsstop" || lowCmd == "stopfs") {
      stopFileServer();
    }

    else if (lowCmd == "fsstatus") {
      if (fileServerRunning) {
        printToConsole(successPrefix + "File server running at: http://" + WiFi.localIP().toString() + ":8080", TFT_GREEN);
        printToConsole(infoPrefix + "Password: " + adminPassword, TFT_YELLOW);
      } else {
        printToConsole(infoPrefix + "File server not running. Use 'fileserver' to start", TFT_YELLOW);
      }
    }

    else if (lowCmd.startsWith("setpwd ")) {
      String newPwd = lowCmd.substring(7);
      if (newPwd.length() >= 4) {
        if (saveAdminPassword(newPwd)) {
          adminPassword = newPwd;
          printToConsole(successPrefix + "Password changed! New password: " + newPwd, TFT_GREEN);
        } else {
          printToConsole(errorPrefix + "Failed to save password", TFT_RED);
        }
      } else {
        printToConsole(errorPrefix + "Password must be at least 4 characters", TFT_RED);
      }
    }

    else if (lowCmd == "showpwd") {
      printToConsole(errorPrefix + "Current admin password: " + adminPassword, TFT_YELLOW);
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
        // Versuche wissenschaftlichen Taschenrechner
        result = evaluateScientific(expr);
        if (result != "Error") {
          printToConsole(expr + " = " + result, TFT_GREEN);
        } else {
          printToConsole(errorPrefix + "Cannot evaluate: " + expr, TFT_RED);
        }
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
      // Prüfen ob es eine Datei ist
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
      }
      // Prüfen ob es ein Ordner ist
      else if (SD.exists(cmd) && !cmd.endsWith(".txt") && !cmd.endsWith(".ch8")) {
        std::vector<String> f, d;
        listDirectory(cmd, f, d);
        printToConsole("Directory: " + cmd, TFT_CYAN);
        for (String dir : d) printToConsole("  [DIR]  " + dir, TFT_GREEN);
        for (String file : f) printToConsole("  [FILE] " + file);
      } else if (lowCmd == "adpsw" || lowCmd == "adminpass" || lowCmd == "passwd") {
        adminPasswordApp();
      } else {
        printToConsole(errorPrefix + "Command not found: " + cmd, TFT_RED);
        printToConsole(infoPrefix + "Type 'help' for available commands", TFT_BLUE);
        playSysSound(3);
      }
    }

    updateInputLine(false);
  }
  // File Server regelmäßig bedienen
  handleFileServer();
}

// ==================== ERWEITERTE WIFI AUTO-CONNECT FUNKTION ====================
// ==================== MIT SPEICHERVERWALTUNG ====================



// Gespeicherte WLANs aus EEPROM laden
void loadSavedWiFi() {
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    int addr = WIFI_EEPROM_START + (i * 64);

    // SSID lesen
    String ssid = "";
    for (int j = 0; j < 32; j++) {
      char c = EEPROM.read(addr + j);
      if (c == 0xFF || c == 0x00) break;
      if (c != 0) ssid += c;
    }

    // Passwort lesen
    String pwd = "";
    for (int j = 0; j < 32; j++) {
      char c = EEPROM.read(addr + 32 + j);
      if (c == 0xFF || c == 0x00) break;
      if (c != 0) pwd += c;
    }

    if (ssid.length() > 0 && ssid[0] != 0xFF) {
      savedNetworks[i].ssid = ssid;
      savedNetworks[i].password = pwd;
      savedNetworks[i].valid = true;
    } else {
      savedNetworks[i].valid = false;
    }
  }
}

// Gespeicherte WLANs in EEPROM speichern
void saveSavedWiFi() {
  // EEPROM Bereich löschen (mit 0xFF füllen)
  for (int i = 0; i < WIFI_EEPROM_SIZE; i++) {
    EEPROM.write(WIFI_EEPROM_START + i, 0xFF);
  }

  // WLANs speichern
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    if (savedNetworks[i].valid) {
      int addr = WIFI_EEPROM_START + (i * 64);

      // SSID schreiben
      for (int j = 0; j < savedNetworks[i].ssid.length() && j < 32; j++) {
        EEPROM.write(addr + j, savedNetworks[i].ssid[j]);
      }
      EEPROM.write(addr + savedNetworks[i].ssid.length(), 0x00);

      // Passwort schreiben
      for (int j = 0; j < savedNetworks[i].password.length() && j < 32; j++) {
        EEPROM.write(addr + 32 + j, savedNetworks[i].password[j]);
      }
      EEPROM.write(addr + 32 + savedNetworks[i].password.length(), 0x00);
    }
  }
  EEPROM.commit();
}

// Gespeichertes WLAN hinzufügen oder aktualisieren
void addSavedWiFi(String ssid, String password) {
  // Prüfen ob bereits vorhanden
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    if (savedNetworks[i].valid && savedNetworks[i].ssid == ssid) {
      savedNetworks[i].password = password;
      saveSavedWiFi();
      printToConsole(successPrefix + "Updated WiFi: " + ssid, TFT_GREEN);
      return;
    }
  }

  // Neuen Eintrag finden
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    if (!savedNetworks[i].valid) {
      savedNetworks[i].ssid = ssid;
      savedNetworks[i].password = password;
      savedNetworks[i].valid = true;
      saveSavedWiFi();
      printToConsole(successPrefix + "Added WiFi: " + ssid, TFT_GREEN);
      return;
    }
  }

  printToConsole(errorPrefix + "Max saved WiFis reached (" + String(MAX_SAVED_WIFIS) + ")", TFT_RED);
}

// Gespeichertes WLAN löschen
void deleteSavedWiFi(String ssid) {
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    if (savedNetworks[i].valid && savedNetworks[i].ssid == ssid) {
      savedNetworks[i].valid = false;
      savedNetworks[i].ssid = "";
      savedNetworks[i].password = "";
      saveSavedWiFi();
      printToConsole(successPrefix + "Deleted WiFi: " + ssid, TFT_GREEN);
      return;
    }
  }
  printToConsole(errorPrefix + "WiFi not found: " + ssid, TFT_RED);
}

// Alle gespeicherten WLANs anzeigen
void listSavedWiFi() {
  printToConsole("╔══════════════════════════════════════╗", TFT_CYAN);
  printToConsole("║     SAVED WIFI NETWORKS              ║", TFT_CYAN);
  printToConsole("╠══════════════════════════════════════╣", TFT_CYAN);

  int count = 0;
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    if (savedNetworks[i].valid) {
      count++;
      String pwdDisplay = savedNetworks[i].password;
      if (pwdDisplay.length() > 0) {
        pwdDisplay = String(pwdDisplay[0]) + "***" + pwdDisplay.substring(pwdDisplay.length() - 1);
      }
      printToConsole("║ " + String(i + 1) + ". " + savedNetworks[i].ssid, TFT_GREEN);
      printToConsole("║    PWD: " + pwdDisplay, TFT_YELLOW);
    }
  }

  if (count == 0) {
    printToConsole("║   No saved WiFis found               ║", TFT_RED);
  }

  printToConsole("╚══════════════════════════════════════╝", TFT_CYAN);
  printToConsole(infoPrefix + String(count) + " network(s) saved (" + String(MAX_SAVED_WIFIS - count) + " slots free)", TFT_BLUE);
}

// Auto-Connect zu bestem verfügbaren WLAN
void autoConnectWiFi() {
  loadSavedWiFi();

  // Prüfen ob WLANs gespeichert sind
  bool hasAny = false;
  for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
    if (savedNetworks[i].valid) {
      hasAny = true;
      break;
    }
  }

  if (!hasAny) {
    printToConsole(infoPrefix + "No saved networks. Use 'wifi add <SSID> <PWD>' or 'scan' to add.", TFT_BLUE);
    return;
  }

  printToConsole(infoPrefix + "Scanning for available networks...", TFT_BLUE);

  // Scan durchführen
  int n = WiFi.scanNetworks();
  if (n == 0) {
    printToConsole(errorPrefix + "No networks found!", TFT_RED);
    return;
  }

  // Bestes Netzwerk aus gespeicherten finden
  int bestIndex = -1;
  int bestRSSI = -1000;
  String bestSSID = "";

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    // Prüfen ob in gespeicherten vorhanden
    for (int j = 0; j < MAX_SAVED_WIFIS; j++) {
      if (savedNetworks[j].valid && savedNetworks[j].ssid == ssid) {
        if (rssi > bestRSSI) {
          bestRSSI = rssi;
          bestIndex = j;
          bestSSID = ssid;
        }
        break;
      }
    }
  }

  WiFi.scanDelete();

  if (bestIndex >= 0) {
    printToConsole(infoPrefix + "Connecting to " + bestSSID + " (signal: " + String(bestRSSI) + "dBm)", TFT_BLUE);
    WiFi.begin(savedNetworks[bestIndex].ssid.c_str(), savedNetworks[bestIndex].password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      attempts++;
      if (attempts % 5 == 0) {
        printToConsole(".", TFT_GREEN);
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      printToConsole("");
      printToConsole(successPrefix + "Auto-connected to: " + bestSSID, TFT_GREEN);
      printToConsole(infoPrefix + "IP: " + WiFi.localIP().toString(), TFT_BLUE);
      initNTP();
    } else {
      printToConsole("");
      printToConsole(errorPrefix + "Connection failed!", TFT_RED);
    }
  } else {
    printToConsole(errorPrefix + "No saved networks found in range!", TFT_RED);
  }
}

// WLAN mit Passwort verbinden und speichern
void connectAndSaveWiFi(String ssid, String password) {
  printToConsole(infoPrefix + "Connecting to " + ssid + "...", TFT_BLUE);
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
    if (attempts % 5 == 0) {
      printToConsole(".", TFT_GREEN);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    printToConsole("");
    printToConsole(successPrefix + "Connected to: " + ssid, TFT_GREEN);
    printToConsole(infoPrefix + "IP: " + WiFi.localIP().toString(), TFT_BLUE);

    // In gespeicherte Liste aufnehmen
    addSavedWiFi(ssid, password);
    initNTP();
  } else {
    printToConsole("");
    printToConsole(errorPrefix + "Connection failed!", TFT_RED);
  }
}

// ==================== WIFI MANAGER MIT SPEICHERVERWALTUNG ====================

void wifiManagerEnhanced() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  loadSavedWiFi();

  tft.fillScreen(BG_COLOR);
  bool running = true;
  int selected = 0;
  int scrollOffset = 0;
  int maxVisible = 8;

  while (running) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Header
    tft.fillRect(0, 35, 240, 30, ACCENT_COLOR);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawCentreString("WIFI MANAGER", 120, 42, 2);

    // Buttons
    tft.fillRoundRect(5, 5, 55, 25, 4, SUCCESS_COLOR);
    tft.drawCentreString("SCAN", 32, 12, 1);

    tft.fillRoundRect(65, 5, 55, 25, 4, TFT_BLUE);
    tft.drawCentreString("ADD", 92, 12, 1);

    tft.fillRoundRect(125, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("DEL", 152, 12, 1);

    tft.fillRoundRect(185, 5, 50, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 210, 12, 1);

    // Gespeicherte Netzwerke anzeigen
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(10, 70);
    tft.print("Saved Networks:");

    int y = 85;
    int visibleCount = 0;

    for (int i = 0; i < MAX_SAVED_WIFIS && visibleCount < maxVisible; i++) {
      if (savedNetworks[i].valid) {
        if (selected == i) {
          tft.fillRoundRect(5, y - 2, 230, 20, 3, ACCENT_COLOR);
          tft.setTextColor(TFT_WHITE);
        } else {
          tft.setTextColor(TEXT_COLOR);
        }

        tft.setCursor(10, y);
        tft.print(String(visibleCount + 1) + ". " + savedNetworks[i].ssid);

        // Signalstärke anzeigen (falls verbunden)
        if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == savedNetworks[i].ssid) {
          tft.setTextColor(TFT_GREEN);
          tft.setCursor(180, y);
          tft.print("● CONNECTED");
        }

        y += 22;
        visibleCount++;
      }
    }

    if (visibleCount == 0) {
      tft.setTextColor(TFT_RED);
      tft.setCursor(10, 100);
      tft.print("  No saved networks");
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(10, 120);
      tft.print("  Use SCAN to find networks");
      tft.setCursor(10, 135);
      tft.print("  or ADD to manually enter");
    }

    // Status anzeigen
    tft.setTextSize(1);
    if (WiFi.status() == WL_CONNECTED) {
      tft.setTextColor(TFT_GREEN);
      tft.setCursor(10, 300);
      tft.print("WiFi: " + WiFi.SSID() + " (" + String(WiFi.RSSI()) + "dBm)");
    } else {
      tft.setTextColor(TFT_RED);
      tft.setCursor(10, 300);
      tft.print("WiFi: Disconnected");
    }

    int tx, ty;
    if (getTouch(tx, ty)) {
      // ESC Button
      if (tx > 185 && ty < 35) {
        running = false;
      }
      // SCAN Button
      else if (tx < 60 && ty < 35) {
        playSysSound(0);
        wifiScanner();
        loadSavedWiFi();  // Neu laden nach Scan
        tft.fillScreen(BG_COLOR);
      }
      // ADD Button
      else if (tx > 60 && tx < 120 && ty < 35) {
        playSysSound(0);
        printToConsole(infoPrefix + "Enter SSID:", TFT_BLUE);
        String ssid = getTextInput();
        if (ssid != "") {
          printToConsole(infoPrefix + "Enter Password (or leave empty for open network):", TFT_BLUE);
          String pwd = getTextInput();
          connectAndSaveWiFi(ssid, pwd);
          loadSavedWiFi();
        }
        tft.fillScreen(BG_COLOR);
      }
      // DEL Button
      else if (tx > 120 && tx < 180 && ty < 35) {
        playSysSound(0);
        if (visibleCount > 0) {
          // Aktuell ausgewähltes löschen
          int deleteIndex = -1;
          int counter = 0;
          for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
            if (savedNetworks[i].valid) {
              if (counter == selected) {
                deleteIndex = i;
                break;
              }
              counter++;
            }
          }
          if (deleteIndex >= 0) {
            if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == savedNetworks[deleteIndex].ssid) {
              WiFi.disconnect();
            }
            deleteSavedWiFi(savedNetworks[deleteIndex].ssid);
            loadSavedWiFi();
            selected = 0;
          }
        }
        tft.fillScreen(BG_COLOR);
      }
      // Netzwerk auswählen
      else if (ty > 80 && ty < 260 && visibleCount > 0) {
        int idx = (ty - 80) / 22;
        if (idx >= 0 && idx < visibleCount) {
          // Index in echten Array-Index umwandeln
          int realIdx = -1;
          int counter = 0;
          for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
            if (savedNetworks[i].valid) {
              if (counter == idx) {
                realIdx = i;
                break;
              }
              counter++;
            }
          }
          if (realIdx >= 0) {
            selected = realIdx;
            playSysSound(0);

            // Verbinden zu ausgewähltem Netzwerk
            printToConsole(infoPrefix + "Connecting to " + savedNetworks[realIdx].ssid + "...", TFT_BLUE);
            WiFi.begin(savedNetworks[realIdx].ssid.c_str(), savedNetworks[realIdx].password.c_str());

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
              delay(500);
              attempts++;
              tft.fillCircle(120 + (attempts % 5) * 10, 280, 2, SUCCESS_COLOR);
            }

            if (WiFi.status() == WL_CONNECTED) {
              printToConsole(successPrefix + "Connected! IP: " + WiFi.localIP().toString(), TFT_GREEN);
              initNTP();
            } else {
              printToConsole(errorPrefix + "Connection failed!", TFT_RED);
            }

            delay(1000);
            tft.fillScreen(BG_COLOR);
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


// Hilfsfunktion für Bestätigung
bool confirmAction(String message) {
  printToConsole(message + " (y/n):", TFT_YELLOW);

  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    int tx, ty;
    if (getTouch(tx, ty)) {
      // Hier könnte man Touch-Buttons für Yes/No implementieren
      // Für jetzt: Einfach Yes annehmen
      return true;
    }

    // Tastatureingabe abfragen
    String input = handleKeyboardInput();
    if (input != "") {
      input.toLowerCase();
      if (input == "y" || input == "yes") return true;
      if (input == "n" || input == "no") return false;
    }
    delay(50);
  }
  return false;
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

// ==================== ADMIN PASSWORT MANAGER APP ====================
// ==================== BEFEHL: adpsw ====================

void adminPasswordApp() {
  int oldKbMode = kbMode;
  String oldInput = currentInput;

  tft.fillScreen(BG_COLOR);

  bool running = true;
  String currentPwd = adminPassword;
  String newPwd = "";
  String confirmPwd = "";
  int step = 0;  // 0=zeige aktuell, 1=neues Passwort, 2=bestätigen

  // Caesar Verschlüsselungsparameter
  const int SHIFT_RIGHT = 12;
  const int SHIFT_LEFT = 14;

  while (running) {
    tft.fillRect(0, 35, 240, 285, BG_COLOR);

    // Header
    tft.fillRect(0, 35, 240, 35, ACCENT_COLOR);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawCentreString("ADMIN PASSWORD", 120, 42, 2);
    tft.setTextSize(1);
    tft.drawCentreString("File Server Access", 120, 62, 1);

    // ESC Button
    tft.fillRoundRect(180, 5, 55, 25, 4, WARNING_COLOR);
    tft.drawCentreString("ESC", 207, 10, 2);

    // Passwort Anzeige Bereich
    tft.fillRoundRect(10, 85, 220, 120, 5, BUTTON_COLOR);
    tft.drawRect(10, 85, 220, 120, TEXT_COLOR);

    tft.setTextSize(1);

    // Aktuelles Passwort anzeigen (maskiert)
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(20, 105);
    tft.print("Current Password:");
    tft.setTextColor(TEXT_COLOR);
    tft.setCursor(20, 120);
    String masked = "";
    for (int i = 0; i < currentPwd.length(); i++) {
      masked += "•";
    }
    tft.print(masked + " (" + String(currentPwd.length()) + " chars)");

    // Neues Passwort eingeben
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(20, 150);
    tft.print("New Password:");
    tft.setTextColor(TEXT_COLOR);
    tft.setCursor(20, 165);

    if (step == 1) {
      // Eingabefeld für neues Passwort
      tft.fillRoundRect(18, 160, 204, 20, 3, TFT_DARKGREY);
      tft.setCursor(22, 165);
      String displayPwd = newPwd;
      if (displayPwd.length() > 25) displayPwd = displayPwd.substring(0, 22) + "...";
      tft.print(displayPwd);
      if ((millis() / 500) % 2 == 0) {
        tft.print("_");
      }
    } else if (step == 2) {
      // Bestätigungsfeld
      tft.fillRoundRect(18, 160, 204, 20, 3, TFT_DARKGREY);
      tft.setCursor(22, 165);
      String displayPwd = confirmPwd;
      if (displayPwd.length() > 25) displayPwd = displayPwd.substring(0, 22) + "...";
      tft.print(displayPwd);
      if ((millis() / 500) % 2 == 0) {
        tft.print("_");
      }
    } else {
      tft.print("Click 'Change' to set new password");
    }

    // Buttons
    tft.fillRoundRect(10, 220, 70, 35, 4, SUCCESS_COLOR);
    tft.drawCentreString("CHANGE", 45, 232, 1);

    tft.fillRoundRect(85, 220, 70, 35, 4, TFT_BLUE);
    tft.drawCentreString("SHOW", 120, 232, 1);

    tft.fillRoundRect(160, 220, 70, 35, 4, TFT_PURPLE);
    tft.drawCentreString("GEN", 195, 232, 1);

    // Status/Info Bereich
    tft.fillRoundRect(10, 265, 220, 50, 5, TFT_DARKGREY);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(20, 278);
    tft.print("📌 Password must be 4-20 chars");
    tft.setCursor(20, 293);
    tft.print("🔐 Encrypted with Caesar cipher");

    int tx, ty;
    if (getTouch(tx, ty)) {
      // ESC Button
      if (tx > 180 && ty < 40) {
        if (step > 0) {
          // Zurücksetzen bei Abbruch
          step = 0;
          newPwd = "";
          confirmPwd = "";
          tft.fillScreen(BG_COLOR);
        } else {
          running = false;
        }
        playSysSound(0);
      }
      // CHANGE Button
      else if (tx > 10 && tx < 80 && ty > 220 && ty < 255) {
        playSysSound(0);
        if (step == 0) {
          step = 1;
          newPwd = "";
          tft.fillScreen(BG_COLOR);
        } else if (step == 1) {
          step = 2;
          tft.fillScreen(BG_COLOR);
        } else if (step == 2) {
          // Passwort ändern
          if (newPwd == confirmPwd && newPwd.length() >= 4 && newPwd.length() <= 20) {
            if (saveAdminPassword(newPwd)) {
              adminPassword = newPwd;
              currentPwd = newPwd;
              printToConsole(successPrefix + "Password changed successfully!", TFT_GREEN);
              printToConsole(infoPrefix + "New password: " + newPwd, TFT_YELLOW);
              playSysSound(1);
              step = 0;
              newPwd = "";
              confirmPwd = "";
            } else {
              printToConsole(errorPrefix + "Failed to save password!", TFT_RED);
              playSysSound(3);
            }
          } else if (newPwd != confirmPwd) {
            printToConsole(errorPrefix + "Passwords do not match!", TFT_RED);
            playSysSound(3);
            step = 1;
            newPwd = "";
          } else if (newPwd.length() < 4) {
            printToConsole(errorPrefix + "Password too short! Min 4 chars", TFT_RED);
            playSysSound(3);
          } else if (newPwd.length() > 20) {
            printToConsole(errorPrefix + "Password too long! Max 20 chars", TFT_RED);
            playSysSound(3);
          }
          tft.fillScreen(BG_COLOR);
        }
      }
      // SHOW Button (aktuelles Passwort anzeigen)
      else if (tx > 85 && tx < 155 && ty > 220 && ty < 255) {
        playSysSound(0);
        printToConsole(errorPrefix + "Current admin password: " + adminPassword, TFT_YELLOW);
        printToConsole(infoPrefix + "Password is stored encrypted on SD card", TFT_BLUE);
        delay(1500);
      }
      // GEN Button (Passwort generieren)
      else if (tx > 160 && tx < 230 && ty > 220 && ty < 255) {
        playSysSound(0);
        // Sicheres Passwort generieren
        const char* chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%";
        String generated = "";
        randomSeed(millis());
        for (int i = 0; i < 10; i++) {
          generated += chars[random(strlen(chars))];
        }

        if (step == 1) {
          newPwd = generated;
        } else if (step == 2) {
          confirmPwd = generated;
        } else {
          // Im Hauptmenü: Neues Passwort vorschlagen
          step = 1;
          newPwd = generated;
        }
        printToConsole(infoPrefix + "Generated password: " + generated, TFT_GREEN);
        tft.fillScreen(BG_COLOR);
      }
    }

    // Tastatureingabe für Passwortfelder
    if (step == 1 || step == 2) {
      String input = handleKeyboardInput();
      if (input != "" && input != "ESC_SIGNAL") {
        if (step == 1) {
          newPwd = input;
          if (newPwd.length() > 20) newPwd = newPwd.substring(0, 20);
        } else if (step == 2) {
          confirmPwd = input;
          if (confirmPwd.length() > 20) confirmPwd = confirmPwd.substring(0, 20);
        }
        tft.fillScreen(BG_COLOR);
        playSysSound(0);
      } else if (input == "ESC_SIGNAL") {
        step = 0;
        newPwd = "";
        confirmPwd = "";
        tft.fillScreen(BG_COLOR);
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

// ==================== VERBESSERTE saveAdminPassword FUNKTION ====================
// Ersetze die alte Funktion mit dieser:

bool saveAdminPassword(String password) {
  // Passwort mit Caesar verschlüsseln
  String encrypted = caesarEncrypt(password, 12, 14);

  File pwdFile = SD.open("/psw_admin.txt", FILE_WRITE);
  if (!pwdFile) {
    Serial.println("ERROR: Cannot open psw_admin.txt for writing");
    return false;
  }

  pwdFile.print(encrypted);
  pwdFile.close();

  Serial.println("Password saved encrypted to /psw_admin.txt");
  return true;
}

// ==================== VERBESSERTE loadAdminPassword FUNKTION ====================
// Ersetze die alte Funktion mit dieser:

String loadAdminPassword() {
  File pwdFile = SD.open("/psw_admin.txt", FILE_READ);
  if (!pwdFile) {
    Serial.println("WARNING: psw_admin.txt not found, creating default");
    // Default Passwort erstellen
    String defaultPwd = "admin123";
    saveAdminPassword(defaultPwd);
    return defaultPwd;
  }

  String encryptedPwd = "";
  while (pwdFile.available()) {
    encryptedPwd += (char)pwdFile.read();
  }
  pwdFile.close();

  encryptedPwd.trim();

  // Entschlüsseln
  String decrypted = caesarDecrypt(encryptedPwd, 12, 14);

  Serial.print("Loaded password (encrypted): ");
  Serial.println(encryptedPwd);
  Serial.print("Decrypted: ");
  Serial.println(decrypted);

  return decrypted;
}

// ==================== CAESAR FUNKTIONEN (korrigiert) ====================

String caesarEncrypt(String input, int shiftRight, int shiftLeft) {
  String output = "";
  int len = input.length();
  int halfLen = (len + 1) / 2;  // Aufgerundete Mitte

  for (int i = 0; i < len; i++) {
    char c = input[i];
    int shift;

    // Erste Hälfte: shift nach rechts (positiv)
    if (i < halfLen) {
      shift = shiftRight;
    }
    // Zweite Hälfte: shift nach links (negativ)
    else {
      shift = -shiftLeft;
    }

    // Buchstaben verschieben
    if (c >= 'A' && c <= 'Z') {
      c = ((c - 'A' + shift) % 26 + 26) % 26 + 'A';
    } else if (c >= 'a' && c <= 'z') {
      c = ((c - 'a' + shift) % 26 + 26) % 26 + 'a';
    }
    // Zahlen und Sonderzeichen bleiben unverändert
    else {
      // Bleibt wie ist
    }

    output += c;
  }
  return output;
}

String caesarDecrypt(String input, int shiftRight, int shiftLeft) {
  // Entschlüsselung = gleiche Methode mit umgekehrten Shifts
  String output = "";
  int len = input.length();
  int halfLen = (len + 1) / 2;

  for (int i = 0; i < len; i++) {
    char c = input[i];
    int shift;

    if (i < halfLen) {
      shift = -shiftRight;  // Rückwärts
    } else {
      shift = shiftLeft;  // Rückwärts (weil original -shiftLeft war)
    }

    if (c >= 'A' && c <= 'Z') {
      c = ((c - 'A' + shift) % 26 + 26) % 26 + 'A';
    } else if (c >= 'a' && c <= 'z') {
      c = ((c - 'a' + shift) % 26 + 26) % 26 + 'a';
    }

    output += c;
  }
  return output;
}
