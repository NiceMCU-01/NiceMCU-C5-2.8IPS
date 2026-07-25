#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <SD.h>

// ==========================================
// 引脚定义 (请再次核对你的实际接线)
// ==========================================
#define LED_PIN     27   // WS2812 数据线引脚
#define NUM_LEDS    1    // WS2812 灯珠数量

#define SPK         26   // 蜂鸣器
#define KEY         28   // 按键

// I2C 触摸引脚
#define CTP_SCL     23
#define CTP_SDA     24
#define CTP_INT     1
#define CTP_RST     -1 

// SPI 屏幕引脚
#define TFT_DC      4
#define TFT_CS      5
#define TFT_CLK     6
#define TFT_MOSI    7
#define TFT_RST     -1      
#define TFT_BL      25      

// SPI SD卡引脚 (重点检查这几根线是否插紧)
#define TF_CD       13     // CS (片选)
#define TF_CMD      10     // MOSI (主出从入)
#define TF_CLK      9      // SCK (时钟)
#define TF_DAT      8      // MISO (主入从出)

SPIClass tftSPI(FSPI); 
Adafruit_ST7789 tft = Adafruit_ST7789(&tftSPI, TFT_CS, TFT_DC, TFT_RST);

Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define CST816D_ADDR  0x15
#define PWM_FREQ   5000
#define PWM_RES    8       

volatile bool rgbEnabled = true;
bool lastKeyState = HIGH;
unsigned long lastKeyDebounce = 0;
#define DEBOUNCE_MS 50
static float rgbHue = 0.0f;
static uint32_t lastTftUpdate = 0;
struct TouchPoint { bool touched; uint16_t x, y; uint8_t gesture; };

constexpr uint16_t UI_NAVY = 0x0339;
constexpr uint16_t UI_BLUE = 0x2C1D;
constexpr uint16_t UI_BG = 0xF7BF;
constexpr uint16_t UI_TEXT = 0x2187;
constexpr uint16_t UI_MUTED = 0x6B90;
constexpr uint16_t UI_LINE = 0xD69A;

// HSV 转 RGB
void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  float c = v * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  float r1, g1, b1;
  if      (h < 60)  { r1=c; g1=x; b1=0; }
  else if (h < 120) { r1=x; g1=c; b1=0; }
  else if (h < 180) { r1=0; g1=c; b1=x; }
  else if (h < 240) { r1=0; g1=x; b1=c; }
  else if (h < 300) { r1=x; g1=0; b1=c; }
  else              { r1=c; g1=0; b1=x; }
  r = (uint8_t)((r1 + m) * 255);
  g = (uint8_t)((g1 + m) * 255);
  b = (uint8_t)((b1 + m) * 255);
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  for(int i=0; i<NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void rgbOff() {
  pixels.clear();
  pixels.show();
}

TouchPoint ctpRead() {
  TouchPoint tp = {false, 0, 0, 0};
  Wire.beginTransmission(CST816D_ADDR);
  Wire.write(0x01);  
  if (Wire.endTransmission(false) != 0) return tp;
  Wire.requestFrom(CST816D_ADDR, 6);
  if (Wire.available() < 6) return tp;
  tp.gesture = Wire.read();          
  uint8_t fingers = Wire.read();     
  uint8_t xH     = Wire.read();      
  uint8_t xL     = Wire.read();      
  uint8_t yH     = Wire.read();      
  uint8_t yL     = Wire.read();      
  tp.x = ((xH & 0x0F) << 8) | xL;
  tp.y = ((yH & 0x0F) << 8) | yL;
  tp.touched = (fingers > 0);
  return tp;
}

void playTone(uint16_t freq, uint16_t durationMs) {
  ledcWriteTone(SPK, freq); 
  delay(durationMs);
  ledcWriteTone(SPK, 0); 
}

void playStartupMelody() {
  uint16_t notes[] = {262, 330, 392, 523};
  for (int i = 0; i < 4; i++) {
    playTone(notes[i], 120);
    delay(20);
  }
}

void sdListRoot() {
  Serial.println("\n── MicroSD contents (root) ──");
  File root = SD.open("/");
  if (!root) {
    Serial.println("  [failed to open root]");
    return;
  }
  bool empty = true;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    empty = false;
    Serial.printf("  %s  %s  %lu bytes\n",
      entry.isDirectory() ? "[DIR] " : "[FILE]",
      entry.name(),
      entry.isDirectory() ? 0UL : (unsigned long)entry.size());
    entry.close();
  }
  if (empty) Serial.println("  (empty)");
  root.close();
  Serial.println("─────────────────────────────");
}

void tftDrawColorBars() {
  int w = tft.width();
  int h = tft.height();
  int barW = w / 7;
  uint16_t colors[] = {
    ST77XX_RED, ST77XX_YELLOW, ST77XX_GREEN,
    ST77XX_CYAN, ST77XX_BLUE, ST77XX_MAGENTA, ST77XX_WHITE
  };
  for (int i = 0; i < 7; i++) {
    tft.fillRect(i * barW, 0, barW, h, colors[i]);
  }
}

void tftPrintCentered(const char *text, int16_t baselineY) {
  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds(text, 0, baselineY, &x1, &y1, &textW, &textH);
  tft.setCursor((tft.width() - textW) / 2 - x1, baselineY);
  tft.print(text);
}

void tftDrawMainScreen() {
  tft.fillScreen(UI_BG);
  tft.setTextWrap(false);
  tft.setTextSize(1);

  tft.fillRect(0, 0, tft.width(), 72, UI_NAVY);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(16, 30);
  tft.print("ESP32-C5");

  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(16, 57);
  tft.print("ESP32_C5_2.8IPS");

  tft.fillRoundRect(14, 88, 212, 144, 12, ST77XX_WHITE);
  tft.drawRoundRect(14, 88, 212, 144, 12, UI_LINE);

  tft.setTextColor(UI_MUTED);
  tftPrintCentered("TOUCH POSITION", 117);

  tft.setTextColor(UI_MUTED);
  tftPrintCentered("Touch the screen to test", 210);

  tft.drawFastHLine(18, 274, tft.width() - 36, UI_LINE);
  tft.setTextColor(UI_MUTED);
  tftPrintCentered("GitHub  NiceMCU-01", 302);
}

void tftDrawStatus(TouchPoint &tp) {
  char statusText[24];

  if (tp.touched) {
    snprintf(statusText, sizeof(statusText), "X %3d   Y %3d", tp.x, tp.y);
  } else {
    snprintf(statusText, sizeof(statusText), "X  --   Y  --");
  }

  tft.fillRect(24, 132, 192, 54, ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(UI_BLUE);
  tftPrintCentered(statusText, 169);
}

void setup() {
  Serial.begin(115200);
  delay(200); 
  Serial.println("\n\n=== ESP32-C5 Board Self-Test ===");

  pinMode(KEY, INPUT_PULLUP);
  pinMode(TFT_BL, OUTPUT);
  pinMode(CTP_INT, OUTPUT); 

  pixels.begin();
  pixels.setBrightness(50);
  rgbOff();

  ledcAttach(SPK, 2000, PWM_RES); 

  if (CTP_RST != -1) {
    pinMode(CTP_RST, OUTPUT);
    digitalWrite(CTP_RST, 0);  
    delay(20);
    digitalWrite(CTP_RST, 1); 
    delay(50);
  }

  Wire.begin(CTP_SDA, CTP_SCL);
  Wire.setClock(400000);
  Wire.beginTransmission(CST816D_ADDR);
  uint8_t ctpErr = Wire.endTransmission();
  if (ctpErr == 0) {
    Serial.println("[CTP] CST816D found OK");
  } else {
    Serial.printf("[CTP] not found (err=%d)\n", ctpErr);
  }
  pinMode(CTP_INT, INPUT);

  tftSPI.begin(TFT_CLK, -1, TFT_MOSI, TFT_CS);  
  tft.init(240, 320); 
  tft.setRotation(0);
  Serial.println("[TFT] ST7789 initialised");

  tftDrawMainScreen();
  TouchPoint initialTouch = {false, 0, 0, 0};
  tftDrawStatus(initialTouch);
  digitalWrite(TFT_BL, HIGH);

  uint8_t initialR, initialG, initialB;
  hsvToRgb(rgbHue, 1.0f, 0.6f, initialR, initialG, initialB);
  setRGB(initialR, initialG, initialB);

  Serial.println("[SPK] Play");
  playStartupMelody();
  Serial.println("[SPK] Done");

  // ==========================================
  // 【终极优化】SD 卡初始化逻辑
  // ==========================================
  Serial.println("[SD] Initializing SD card...");
  
  // 1. 增加长延时，等待 SD 卡模块上电稳定
  delay(1000); 

  // 2. 直接使用底层 SPI.begin 初始化，绕过 SPIClass 对象，这是最稳妥的方式
  SPI.begin(TF_CLK, TF_DAT, TF_CMD, TF_CD);
  
  // 3. 尝试挂载 SD 卡，并强制指定 SPI 频率为 400kHz (SD卡初始化的标准安全频率)
  if (SD.begin(TF_CD, SPI, 400000)) {
    Serial.printf("[SD] Card OK — type: %s, size: %llu MB\n",
      SD.cardType() == CARD_SDHC ? "SDHC" : "SD",
      SD.cardSize() / (1024ULL * 1024ULL));
    sdListRoot();
  } else {
    Serial.println("[SD] Mount failed! Please check:");
    Serial.println("  1. Wiring (CS=13, MOSI=10, MISO=8, SCK=9)");
    Serial.println("  2. SD Card format (Must be FAT32)");
    Serial.println("  3. SD Card capacity (Try <= 32GB)");
  }

  Serial.println("[SYS] Setup complete — entering loop");
}

void loop() {
  uint32_t now = millis();
  bool keyNow = digitalRead(KEY);
  if (keyNow == LOW && lastKeyState == HIGH && (now - lastKeyDebounce) > DEBOUNCE_MS) {
    rgbEnabled = !rgbEnabled;
    lastKeyDebounce = now;
    Serial.printf("[KEY] RGB effect %s\n", rgbEnabled ? "ON" : "OFF");
    if (!rgbEnabled) rgbOff();
  }
  lastKeyState = keyNow;

  if (rgbEnabled) {
    uint8_t r, g, b;
    hsvToRgb(rgbHue, 1.0f, 0.6f, r, g, b);
    setRGB(r, g, b);
    rgbHue += 1.0f;
    if (rgbHue >= 360.0f) rgbHue = 0.0f;
  }

  TouchPoint tp = ctpRead();
  if (tp.touched) {
    Serial.printf("[CTP] Touch x=%d y=%d gesture=0x%02X\n", tp.x, tp.y, tp.gesture);
  }

  if (now - lastTftUpdate > 200) {
    tftDrawStatus(tp);
    lastTftUpdate = now;
  }

  delay(10); 
}
