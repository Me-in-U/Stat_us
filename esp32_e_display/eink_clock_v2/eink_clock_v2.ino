/* ===== ESP32 + Waveshare 4.2" V2: Centered Big Date/Time (Maple 폰트 + 정각/30분 Full Refresh) ===== */
#include <WiFi.h>
#include "time.h"

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"

// ===== WiFi =====
const char* WIFI_SSID = "E108";
const char* WIFI_PW   = "08080808";

// ===== NTP (KST = UTC+9) =====
static const long   GMT_OFFSET_SEC = 9 * 3600;
static const int    DST_OFFSET_SEC = 0;
static const char*  NTP_SERVER     = "pool.ntp.org";

// ===== ePaper 해상도 =====
static const UWORD EPD_W = EPD_4IN2_V2_WIDTH;   // 400
static const UWORD EPD_H = EPD_4IN2_V2_HEIGHT;  // 300

// ===== 부분 업데이트 영역 (가운데 384x120) =====
static const UWORD CANVAS_W = 384;
static const UWORD CANVAS_H = 120;
static const UWORD PART_X0  = (EPD_W - CANVAS_W) / 2;
static const UWORD PART_Y0  = (EPD_H - CANVAS_H) / 2;
static const UWORD PART_X1  = PART_X0 + CANVAS_W - 1;
static const UWORD PART_Y1  = PART_Y0 + CANVAS_H - 1;

// ===== 폰트 =====
#define DATE_FONT   Font20
#define TIME_FONT   Maple44

// ===== 버퍼 =====
static UBYTE* gCanvas = nullptr;

// ===== 30분마다 Full Refresh 제어 =====
static int8_t lastFullRefreshHour = -1;
static int8_t lastFullRefreshMin = -1;

// ===== 유틸 =====
static bool getNowKST(struct tm& t) {
  return getLocalTime(&t, 2000);
}

// 가운데 정렬
static void drawCenteredText(UWORD canvasW, UWORD y, const char* s, const sFONT* f, UWORD bg, UWORD fg) {
  const UWORD cw = f->Width;
  const size_t n = strlen(s);
  if (n == 0) return;

  if (y + f->Height > Paint.Height) return;
  const UWORD neededW = (UWORD)(cw * n);
  if (neededW > canvasW) return;

  const UWORD textW = neededW;
  UWORD x = (canvasW - textW) / 2;

  for (size_t i = 0; i < n; ++i) {
    if (x + cw > Paint.Width) break;
    Paint_DrawChar(x, y, s[i], (sFONT*)f, fg, bg);
    x += cw;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Wi-Fi
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
    Serial.println("[WiFi] Failed");
  }

  // NTP
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

  Serial.printf("[INIT] Canvas=%ux%u, Area=[%u,%u]-[%u,%u]\n",
                CANVAS_W, CANVAS_H, PART_X0, PART_Y0, PART_X1, PART_Y1);
}

void loop() {
  static uint32_t lastTick = 0;
  uint32_t now = millis();
  if (now - lastTick < 1000) return;
  lastTick = now;

  // WiFi 상태 확인
  bool wifi_connected = (WiFi.status() == WL_CONNECTED);

  struct tm ti;
  bool ok_time = getNowKST(ti);

  // 정각/30분 Full Refresh
  bool isHourMark = (ok_time && ti.tm_min == 0 && ti.tm_sec <= 2);
  bool isHalfMark = (ok_time && ti.tm_min == 30 && ti.tm_sec <= 2);
  
  if (isHourMark && (lastFullRefreshHour != ti.tm_hour || lastFullRefreshMin != 0)) {
    EPD_4IN2_V2_Init();
    EPD_4IN2_V2_Clear();
    DEV_Delay_ms(200);
    EPD_4IN2_V2_Init_Fast(Seconds_1S);
    lastFullRefreshHour = (int8_t)ti.tm_hour;
    lastFullRefreshMin = 0;
    Serial.printf("[REFRESH] %02d:00\n", ti.tm_hour);
  } else if (isHalfMark && lastFullRefreshMin != 30) {
    EPD_4IN2_V2_Init();
    EPD_4IN2_V2_Clear();
    DEV_Delay_ms(200);
    EPD_4IN2_V2_Init_Fast(Seconds_1S);
    lastFullRefreshMin = 30;
    Serial.printf("[REFRESH] %02d:30\n", ti.tm_hour);
  }

  // 문자열 구성
  char dateStr[20], timeStr[20];
  if (!wifi_connected) {
    snprintf(dateStr, sizeof(dateStr), "WIFI");
    snprintf(timeStr, sizeof(timeStr), "CONNECTING");
  } else if (ok_time) {
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
             1900 + ti.tm_year, 1 + ti.tm_mon, ti.tm_mday);
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    snprintf(dateStr, sizeof(dateStr), "TIME");
    snprintf(timeStr, sizeof(timeStr), "SYNCING");
  }

  // 부분 업데이트
  Paint_SelectImage(gCanvas);
  Paint_Clear(WHITE);
  
  const UWORD y_date = 10;
  const UWORD y_time = y_date + DATE_FONT.Height + 10;

  // 상태 메시지는 Font20, 시간은 Maple44 사용
  if (!wifi_connected || !ok_time) {
    drawCenteredText(CANVAS_W, y_date, dateStr, &DATE_FONT, WHITE, BLACK);
    drawCenteredText(CANVAS_W, y_time, timeStr, &DATE_FONT, WHITE, BLACK);
  } else {
    drawCenteredText(CANVAS_W, y_date, dateStr, &DATE_FONT, WHITE, BLACK);
    drawCenteredText(CANVAS_W, y_time, timeStr, &TIME_FONT, WHITE, BLACK);
  }

  EPD_4IN2_V2_PartialDisplay(gCanvas, PART_X0, PART_Y0, PART_X1, PART_Y1);

  if (!wifi_connected) {
    Serial.println("[STATUS] WiFi 연결중...");
  } else {
    Serial.printf("[TIME] %s %s\n", dateStr, timeStr);
  }
}
