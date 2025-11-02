/* ===== ESP32 + Waveshare 4.2" V2: Centered Big Date/Time (Font24 max) ===== */
#include <WiFi.h>
#include "time.h"

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"

// ===== WiFi =====
const char* WIFI_SSID = "iptime Mesh";
const char* WIFI_PW   = "0553213247";

// ===== NTP (KST = UTC+9) =====
static const long   GMT_OFFSET_SEC = 9 * 3600;
static const int    DST_OFFSET_SEC = 0;
static const char*  NTP_SERVER     = "pool.ntp.org";

// ===== ePaper 해상도 =====
static const UWORD EPD_W = EPD_4IN2_V2_WIDTH;   // 400
static const UWORD EPD_H = EPD_4IN2_V2_HEIGHT;  // 300

// ===== 부분 업데이트 영역 (중앙 280x100) =====
static const UWORD CANVAS_W = 280;
static const UWORD CANVAS_H = 100;
static const UWORD PART_X0  = (EPD_W - CANVAS_W) / 2;   // 60
static const UWORD PART_Y0  = (EPD_H - CANVAS_H) / 2;   // 100
static const UWORD PART_X1  = PART_X0 + CANVAS_W;       // 340
static const UWORD PART_Y1  = PART_Y0 + CANVAS_H;       // 200

// ===== 폰트 =====
#define DATE_FONT   Font20   // "YYYY-MM-DD"
#define TIME_FONT   Font24   // "HH:MM:SS"

// ===== 버퍼 =====
static UBYTE* gCanvas = nullptr;

// ===== 잔상 방지 주기(초) =====
static const uint16_t FULL_REFRESH_PERIOD_SEC = 60;
static uint32_t lastFullRefreshSec = 0;

// ===== 유틸 =====
static bool getNowKST(struct tm& t) {
  return getLocalTime(&t, 2000);
}

// 가운데 정렬 출력 함수 (BG, FG 순서)
static void drawCenteredText(UWORD canvasW, UWORD y, const char* s, sFONT* f, UWORD bg, UWORD fg) {
  size_t n = strlen(s);
  UWORD textW = n * f->Width;
  UWORD x = (canvasW > textW) ? (canvasW - textW) / 2 : 0;
  Paint_DrawString_EN(x, y, s, f, bg, fg);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Wi-Fi 연결
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  uint32_t t0 = millis();
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250); Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] OK: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Failed (will retry).");
  }

  // NTP 설정
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);

  // EPD 초기화
  DEV_Module_Init();
  EPD_4IN2_V2_Init();
  EPD_4IN2_V2_Clear();
  DEV_Delay_ms(400);
  EPD_4IN2_V2_Init_Fast(Seconds_1S);

  // 캔버스 메모리 확보
  UWORD bufSize = ((CANVAS_W % 8 == 0) ? (CANVAS_W / 8) : (CANVAS_W / 8 + 1)) * CANVAS_H;
  gCanvas = (UBYTE*)malloc(bufSize);
  if (!gCanvas) {
    Serial.println("[EPD] canvas alloc failed");
    while (1) delay(1000);
  }

  Paint_NewImage(gCanvas, CANVAS_W, CANVAS_H, 0, WHITE);
  Paint_SelectImage(gCanvas);
  Paint_Clear(WHITE);
  EPD_4IN2_V2_PartialDisplay(gCanvas, PART_X0, PART_Y0, PART_X1, PART_Y1);

  lastFullRefreshSec = millis() / 1000;
}

void loop() {
  static uint32_t lastTick = 0;
  uint32_t now = millis();
  if (now - lastTick < 1000) return;
  lastTick = now;

  // 잔상 방지용 풀 리프레시
  if ((now/1000 - lastFullRefreshSec) >= FULL_REFRESH_PERIOD_SEC) {
    EPD_4IN2_V2_Init();
    EPD_4IN2_V2_Clear();
    DEV_Delay_ms(200);
    EPD_4IN2_V2_Init_Fast(Seconds_1S);
    lastFullRefreshSec = now / 1000;
  }

  // 현재 시각
  struct tm ti;
  bool ok = getNowKST(ti);

  char dateStr[16];
  char timeStr[16];
  if (ok) {
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
             1900 + ti.tm_year, 1 + ti.tm_mon, ti.tm_mday);
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    snprintf(dateStr, sizeof(dateStr), "---- -- --");
    snprintf(timeStr, sizeof(timeStr), "--:--:--");
  }

  // 렌더링
  Paint_SelectImage(gCanvas);
  Paint_Clear(WHITE);

  const UWORD y_date = 10;
  const UWORD y_time = y_date + DATE_FONT.Height + 10;

  drawCenteredText(CANVAS_W, y_date, dateStr, &DATE_FONT, WHITE, BLACK);
  drawCenteredText(CANVAS_W, y_time, timeStr, &TIME_FONT, WHITE, BLACK);

  EPD_4IN2_V2_PartialDisplay(gCanvas, PART_X0, PART_Y0, PART_X1, PART_Y1);

  Serial.printf("[TIME] %s %s\n", dateStr, timeStr);
}
