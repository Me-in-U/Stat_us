/*
  ESP32 + 2.0" TFT SPI (ST7789) 통합 테스트
  배선(D라벨 기준)  ->  ESP32 GPIO
    SCLK = D18      ->  GPIO18
    MOSI = D23      ->  GPIO23
    CS   = D5       ->  GPIO5     (보드에서 CS가 GND 고정이면 아래에서 CS 미사용 생성자 사용)
    DC   = D2       ->  GPIO2
    RST  = D4       ->  GPIO4
    BL   = D15      ->  GPIO15    (백라이트 PWM, 없으면 3.3V 고정)

  기본 해상도 240x320. 240x240 패널이면 아래 #define PANEL_240x240 주석 해제.
  SPI 40 MHz. 색 반전되면 invertDisplay(true) 한 줄만 바꿔보기.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ====== 패널 해상도 선택 ======
// #define PANEL_240x240   // 240x240 패널이면 이 줄 주석 해제

#ifdef PANEL_240x240
const uint16_t TFT_W = 240;
const uint16_t TFT_H = 240;
#else
const uint16_t TFT_W = 240;
const uint16_t TFT_H = 320;
#endif

// ====== 핀 매핑 (D라벨 -> GPIO) ======
#define TFT_SCLK 18  // D18
#define TFT_MOSI 23  // D23
#define TFT_CS 5     // D5  (CS가 GND로 고정된 보드면 아래의 생성자 대안 주석 참고)
#define TFT_DC 2     // D2
#define TFT_RST 4    // D4
#define TFT_BL 15    // D15 (없으면 -1로 두고 관련 코드 주석처리)

// ====== 디스플레이 인스턴스 ======
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
// CS가 GND에 하드고정된 보드라면 위 줄 대신 아래 2-인자 생성자 사용:
// Adafruit_ST7789 tft(TFT_DC, TFT_RST);

void backlightSetup(uint8_t brightness /*0~255*/) {
  if (TFT_BL < 0) return;
  ledcSetup(0, 5000, 8);  // 채널0, 5 kHz, 8비트
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, brightness);  // 예: 200 ≈ 78% 밝기
}

void basicDrawTest() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  // 헤더 바
  tft.fillRect(0, 0, TFT_W, 22, ST77XX_RED);
  tft.setCursor(6, 4);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print(F("ST7789 "));
  tft.print(TFT_W);
  tft.print("x");
  tft.print(TFT_H);

  // 대각선/테두리
  tft.drawLine(0, 0, TFT_W - 1, TFT_H - 1, ST77XX_GREEN);
  tft.drawLine(0, TFT_H - 1, TFT_W - 1, 0, ST77XX_GREEN);
  tft.drawRect(0, 0, TFT_W, TFT_H, ST77XX_WHITE);

  // 컬러 바
  const uint16_t bars[] = {
    ST77XX_BLUE, ST77XX_CYAN, ST77XX_GREEN, ST77XX_YELLOW, ST77XX_ORANGE,
    ST77XX_RED, ST77XX_MAGENTA, ST77XX_WHITE
  };
  int nb = sizeof(bars) / sizeof(bars[0]);
  int bh = (TFT_H - 40) / nb;
  for (int i = 0; i < nb; i++) {
    tft.fillRect(10, 30 + i * bh, TFT_W - 20, bh - 2, bars[i]);
  }

  // 텍스트 폰트 크기 예시
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(10, TFT_H - 28);
  tft.setTextSize(2);
  tft.print(F("Rotation="));
  tft.print(tft.getRotation());
}

void fillFpsTest(uint16_t rounds /*예: 60*/) {
  // 컬러 순환 풀스크린 fill로 대충 FPS 감 잡기
  const uint16_t colors[] = {
    ST77XX_BLACK, ST77XX_BLUE, ST77XX_RED, ST77XX_GREEN,
    ST77XX_CYAN, ST77XX_MAGENTA, ST77XX_YELLOW, ST77XX_WHITE
  };
  int nc = sizeof(colors) / sizeof(colors[0]);

  uint32_t t0 = millis();
  for (uint16_t i = 0; i < rounds; i++) {
    tft.fillScreen(colors[i % nc]);
  }
  uint32_t dt = millis() - t0;
  // 결과 출력: 60회 기준으로 총 소요 ms와 대략 FPS
  float fps = (rounds * 1000.0f) / dt;
  Serial.print(F("[FillFps] rounds="));
  Serial.print(rounds);
  Serial.print(F(", time(ms)="));
  Serial.print(dt);
  Serial.print(F(", approx FPS="));
  Serial.println(fps);

  // 결과 화면에 표시
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print(F("Fill "));
  tft.print(rounds);
  tft.print(F(" frames"));
  tft.setCursor(10, 32);
  tft.print(F("Time "));
  tft.print(dt);
  tft.print(F(" ms"));
  tft.setCursor(10, 54);
  tft.print(F("FPS  "));
  tft.print(fps, 1);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // 백라이트 0~255
  backlightSetup(200);

  // SPI 시작: SCLK=18, MISO(없음=-1), MOSI=23, SS=CS
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

#ifdef PANEL_240x240
  tft.init(240, 240);
#else
  tft.init(240, 320);
#endif
  tft.setSPISpeed(40000000);  // 40 MHz
  tft.setRotation(1);         // 0~3 바꿔보며 최적 조합 찾기
  // 색이 뒤집히면 다음 줄을 true로:
  // tft.invertDisplay(true);

  basicDrawTest();
  delay(1000);

  // 간단 성능 체크(정량 예시): 60프레임 컬러필
  fillFpsTest(60);

  // 도형 드로우 간단 루프 예시 (선/원/사각형)
  tft.fillScreen(ST77XX_BLACK);
  for (int i = 0; i < min(TFT_W, TFT_H) / 2; i += 6) {
    tft.drawCircle(TFT_W / 2, TFT_H / 2, i, ST77XX_CYAN);
  }
  delay(500);
  for (int i = 0; i < TFT_W; i += 6) {
    tft.drawFastVLine(i, 0, TFT_H, ST77XX_YELLOW);
  }
  delay(500);
  for (int i = 0; i < TFT_H; i += 6) {
    tft.drawFastHLine(0, i, TFT_W, ST77XX_ORANGE);
  }
}

void loop() {
  // 회전 및 반전 토글 데모(각 2초)
  static uint32_t last = 0;
  static int mode = 0;
  if (millis() - last > 2000) {
    last = millis();
    mode++;
    if (mode % 2 == 0) {
      tft.invertDisplay(false);
    } else {
      tft.invertDisplay(true);
    }
    tft.setRotation((tft.getRotation() + 1) & 3);
    basicDrawTest();
  }
}
