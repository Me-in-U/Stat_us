/* ===== ESP32 + Waveshare 4.2" V2: Centered Big Date/Time (Maple 폰트 + 정각 Full Refresh) ===== */
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

// ===== 부분 업데이트 영역 (가운데 384x120) =====
// Maple44: 글자폭 44, "HH:MM:SS" 8문자(콜론 포함) * 44 = 352px <= 384px
static const UWORD CANVAS_W = 384;                     // 48의 배수(바이트 경계 정렬)
static const UWORD CANVAS_H = 120;
static const UWORD PART_X0  = (EPD_W - CANVAS_W) / 2;  // 8px 좌우 여백
static const UWORD PART_Y0  = (EPD_H - CANVAS_H) / 2;  // 90
static const UWORD PART_X1  = PART_X0 + CANVAS_W - 1;  // inclusive
static const UWORD PART_Y1  = PART_Y0 + CANVAS_H - 1;  // inclusive

// ===== 폰트 =====
#define DATE_FONT   Font20     // "YYYY-MM-DD"
#define TIME_FONT   Maple44    // "HH:MM:SS"

// ===== 버퍼 =====
static UBYTE* gCanvas = nullptr;

// ===== 정각 Full Refresh 제어 =====
static int8_t lastFullRefreshHour = -1;   // -1: 아직 수행 전

// ===== 유틸 =====
static bool getNowKST(struct tm& t) {
  return getLocalTime(&t, 2000);
}

// 가운데 정렬 + 고정 스텝(step=Width). 캔버스가 부족하면 그리지 않음.
static void drawCenteredText(UWORD canvasW, UWORD y, const char* s, const sFONT* f, UWORD bg, UWORD fg) {
  const UWORD cw = f->Width;
  const size_t n = strlen(s);
  if (n == 0) return;

  // 세로/가로 경계 체크
  if (y + f->Height > Paint.Height) return;
  const UWORD neededW = (UWORD)(cw * n);
  if (neededW > canvasW) return;

  const UWORD textW = neededW;
  UWORD x = (canvasW - textW) / 2;

  for (size_t i = 0; i < n; ++i) {
    if (x + cw > Paint.Width) break;                // 최후 안전장치
    Paint_DrawChar(x, y, s[i], (sFONT*)f, fg, bg);  // Waveshare API는 sFONT* 필요
    x += cw;                                        // 글자폭 만큼 이동 → 겹침 방지
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
    Serial.println("[WiFi] Failed (will retry).");
  }

  // NTP
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);

  // EPD 초기화
  DEV_Module_Init();
  EPD_4IN2_V2_Init();
  EPD_4IN2_V2_Clear();
  DEV_Delay_ms(400);
  EPD_4IN2_V2_Init_Fast(Seconds_1S);

  // 캔버스 메모리 확보 (1bpp)
  UWORD bufSize = ((CANVAS_W % 8 == 0) ? (CANVAS_W / 8) : (CANVAS_W / 8 + 1)) * CANVAS_H;
  gCanvas = (UBYTE*)malloc(bufSize);
  if (!gCanvas) {
    Serial.println("[EPD] canvas alloc failed");
    while (1) delay(1000);
  }

  Paint_NewImage(gCanvas, CANVAS_W, CANVAS_H, 0, WHITE);
  Paint_SelectImage(gCanvas);
  Paint_Clear(WHITE);

  // 부분 업데이트 영역 지정
  EPD_4IN2_V2_PartialDisplay(gCanvas, PART_X0, PART_Y0, PART_X1, PART_Y1);

  Serial.printf("[DBG] TIME_FONT W=%u H=%u, canvas=%ux%u, area=[%u,%u]-[%u,%u]\n",
                TIME_FONT.Width, TIME_FONT.Height,
                (unsigned)CANVAS_W, (unsigned)CANVAS_H,
                (unsigned)PART_X0, (unsigned)PART_Y0, (unsigned)PART_X1, (unsigned)PART_Y1);
}

void loop() {
  static uint32_t lastTick = 0;
  uint32_t now = millis();
  if (now - lastTick < 1000) return;  // 1Hz
  lastTick = now;

  // 현재 시각 (1회 계산 재사용)
  struct tm ti;
  bool ok_time = getNowKST(ti);

  // ===== 정각 Full Refresh: 분==0이고 초<=2일 때, 해당 시의 첫 1회만 =====
  if (ok_time && ti.tm_min == 0 && ti.tm_sec <= 2) {
    if (lastFullRefreshHour != ti.tm_hour) {
      EPD_4IN2_V2_Init();
      EPD_4IN2_V2_Clear();
      DEV_Delay_ms(200);
      EPD_4IN2_V2_Init_Fast(Seconds_1S);
      lastFullRefreshHour = (int8_t)ti.tm_hour;
      Serial.printf("[FULL] Top-of-hour refresh at %02d:00\n", ti.tm_hour);
    }
  }

  // 문자열 구성
  char dateStr[16];
  char timeStr[16];
  if (ok_time) {
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
             1900 + ti.tm_year, 1 + ti.tm_mon, ti.tm_mday);
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    snprintf(dateStr, sizeof(dateStr), "---- -- --");
    snprintf(timeStr, sizeof(timeStr), "--:--:--");
  }

  // ===== 렌더링 =====
  Paint_SelectImage(gCanvas);
  Paint_Clear(WHITE);

  const UWORD y_date = 10;
  const UWORD y_time = y_date + DATE_FONT.Height + 10;   // 10 + 20~24 + 10 ≈ 40~44
                                                         // 40~44 + Maple44.H(≈64) <= 120 OK

  drawCenteredText(CANVAS_W, y_date, dateStr, &DATE_FONT, WHITE, BLACK);
  drawCenteredText(CANVAS_W, y_time, timeStr, &TIME_FONT, WHITE, BLACK);

  // 부분 업데이트
  EPD_4IN2_V2_PartialDisplay(gCanvas, PART_X0, PART_Y0, PART_X1, PART_Y1);

  Serial.printf("[TIME] %s %s\n", dateStr, timeStr);
}
