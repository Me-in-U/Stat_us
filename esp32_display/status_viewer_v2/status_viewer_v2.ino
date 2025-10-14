/*
  ESP32 + ST7789(Adafruit) + HTTP GET(JSON) 상태 뷰어 (임시 하드코딩 버전)
  - WiFi SSID/PW, API Key, 백엔드 URL은 하드코딩합니다. (추후 AP 모드/설정 저장으로 대체)
  - 백엔드: GET /api/status/latest/by-key  (x-api-key 헤더)
  - 응답 래핑: { code, isSuccess, message, result: {...스냅샷...} }

  배선(D라벨 기준)  ->  ESP32 GPIO
    SCLK = D18      ->  GPIO18
    MOSI = D23      ->  GPIO23
    CS   = D5       ->  GPIO5
    DC   = D2       ->  GPIO2
    RST  = D4       ->  GPIO4
    BL   = D15      ->  GPIO15 (PWM)
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ArduinoJson.h>

// ====== 하드코딩 설정 ======
const char* WIFI_SSID = ""; // WiFi SSID
const char* WIFI_PW   = ""; // WiFi PW
const char* API_KEY   = "";  // 웹에서 발급한 API 키
const char* BACKEND_URL = "http://192.168.0.2:8080/api/status/latest/by-key"; // 네트워크 환경에 맞게 수정

// 패널 해상도 선택
// #define PANEL_240x240
#ifdef PANEL_240x240
const uint16_t TFT_W = 240;
const uint16_t TFT_H = 240;
#else
const uint16_t TFT_W = 240;
const uint16_t TFT_H = 320;
#endif

// 핀 매핑
#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4
#define TFT_BL 15

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ====== 테마(Customize here) ======
// 필요에 맞게 색상을 변경하세요. (Adafruit ST77XX 표준 색상 또는 16-bit RGB565 값)
uint16_t THEME_BG                 = ST77XX_BLACK;
uint16_t THEME_HEADER_BG          = ST77XX_BLUE;
uint16_t THEME_HEADER_FG          = ST77XX_WHITE;
uint16_t THEME_LABEL_FG           = ST77XX_YELLOW;
uint16_t THEME_VALUE_FG           = ST77XX_WHITE;
uint16_t THEME_KEYBAR_ACTIVE_BG   = ST77XX_GREEN;
uint16_t THEME_KEYBAR_IDLE_BG     = ST77XX_ORANGE;
uint16_t THEME_KEYBAR_FG          = ST77XX_WHITE;

// ====== 디스플레이 설정 ======
// 필요 시 0/1/2/3으로 바꿔보세요. 회전 불일치로 화면이 안 보일 때 유용합니다.
#ifndef TFT_ROTATION
#define TFT_ROTATION 1
#endif

// 디버그 프레임/스플래시를 잠시 켜서 화면이 보이는지 확인
bool DEBUG_FRAME = true; // 문제가 해결되면 false로 바꾸세요.

// ====== 뷰 상태(변경 감지용) ======
struct ViewState {
  String ts;
  String lang;
  String br;
  String file;
  long idleS = -1;
  long actS  = -1;
  long keys  = -1;
  int8_t idleFlag = -1; // -1 unknown, 0 active, 1 idle
  bool drawn = false;
} gPrev;

// 시간 파싱 및 부분 갱신용 이전 값 저장
struct PrevTime {
  int y = -1, m = -1, d = -1;
  int hh = -1, mm = -1, ss = -1, ms = -1;
  bool hasMs = false;
  bool init = false;
} gPrevTime;

void backlightSetup(uint8_t brightness /*0~255*/) {
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, brightness);
}

void drawHeader(const char* title) {
  tft.fillRect(0, 0, tft.width(), 20, THEME_HEADER_BG);
  tft.setCursor(6, 4);
  tft.setTextColor(THEME_HEADER_FG);
  tft.setTextSize(2);
  tft.print(title);
}

// ====== 고정 레이아웃 1회 렌더 ======
// 라벨은 고정으로 그리고, 값 영역만 이후 업데이트 시 부분 갱신
const int X_LABEL = 6;
const int X_VALUE = 90;
const int H_2X = 20;  // size=2 텍스트 라인 높이
const int H_1X = 16;  // size=1 텍스트 라인 높이
const int CHAR_W_2X = 12; // 기본 글꼴 문자 폭(6) * size(2)
const int Y_TIME   = 26;
const int Y_TIME2  = Y_TIME + H_2X; // Time 값을 두 줄로(YYYY-MM-DD, HH:MM:SS.mmm)
const int Y_LANG   = 46 + H_2X;     // Time 2번째 줄 공간만큼 아래로 이동
const int Y_BRANCH = 66 + H_2X;
const int Y_IDLE   = 86 + H_2X;
const int Y_ACTIVE = 106 + H_2X;
const int Y_FILE_L = 126 + H_2X;
const int Y_FILE   = 146 + H_2X;

void drawLabel2x(int y, const char* label) {
  tft.setTextColor(THEME_LABEL_FG, THEME_BG);
  tft.setTextSize(2);
  tft.setCursor(X_LABEL, y);
  tft.print(label);
}

void drawLayoutOnce() {
  tft.fillScreen(THEME_BG);
  if (DEBUG_FRAME) {
    tft.drawRect(0, 0, tft.width()-1, tft.height()-1, ST77XX_RED);
  }
  drawHeader("Stat-us");
  drawLabel2x(Y_TIME,   "Time:");
  // 값 라인은 update 단계에서 렌더 (YYYY-MM-DD / HH:MM:SS.mmm)
  drawLabel2x(Y_LANG,   "Lang:");
  drawLabel2x(Y_BRANCH, "Branch:");
  drawLabel2x(Y_IDLE,   "Idle:");
  drawLabel2x(Y_ACTIVE, "Active:");
  drawLabel2x(Y_FILE_L, "File:");
  // 하단 키 바 바탕
  tft.fillRect(0, tft.height()-20, tft.width(), 20, THEME_KEYBAR_ACTIVE_BG);
  // 초기 키 바 텍스트도 표기
  tft.setCursor(6, tft.height()-18);
  tft.setTextColor(THEME_KEYBAR_FG, THEME_KEYBAR_ACTIVE_BG);
  tft.setTextSize(2);
  tft.print("Keys: 0");
}

void clearValueArea(int y, int h) {
  tft.fillRect(X_VALUE, y, tft.width() - X_VALUE - 6, h, THEME_BG);
}

void printValue2x(int y, const String& v) {
  tft.setTextColor(THEME_VALUE_FG, THEME_BG);
  tft.setTextSize(2);
  tft.setCursor(X_VALUE, y);
  tft.print(v);
}

void printFile1x(const String& path) {
  // 최대 3줄 출력: 1행은 라벨 옆에서 시작, 넘치면 2행(Y_FILE), 더 넘치면 3행(Y_FILE+H_2X)에서 이어서 출력
  const int labelLen = 5; // "File:" 길이
  int xStart1 = X_LABEL + (labelLen * CHAR_W_2X) + (1 * CHAR_W_2X); // 라벨 뒤 한 글자 간격
  int max1 = (tft.width() - xStart1 - 2) / CHAR_W_2X;
  if (max1 < 0) max1 = 0;
  int max2 = (tft.width() - X_VALUE - 2) / CHAR_W_2X;
  if (max2 < 0) max2 = 0;
  int max3 = max2;
  int y3 = Y_FILE + H_2X;

  // 해당 영역 클리어
  tft.fillRect(xStart1, Y_FILE_L, tft.width() - xStart1 - 2, H_2X, THEME_BG);
  tft.fillRect(X_VALUE,  Y_FILE,   tft.width() - X_VALUE  - 2, H_2X, THEME_BG);
  if (y3 + H_2X <= tft.height() - 20) { // 키 바와 겹치지 않도록 간단 가드
    tft.fillRect(X_VALUE,  y3,   tft.width() - X_VALUE  - 2, H_2X, THEME_BG);
  }

  tft.setTextColor(THEME_VALUE_FG, THEME_BG);
  tft.setTextSize(2);
  tft.setTextWrap(false);

  // 첫 줄
  tft.setCursor(xStart1, Y_FILE_L);
  String line1 = path;
  String line2 = "";
  String line3 = "";
  if ((int)path.length() > max1) {
    line1 = path.substring(0, max1);
    String rest = path.substring(max1);
    if ((int)rest.length() > max2) {
      line2 = rest.substring(0, max2);
      line3 = rest.substring(max2);
    } else {
      line2 = rest;
    }
  }
  tft.print(line1);

  // 둘째 줄 (있으면)
  if (line2.length() > 0) {
    // 둘째 줄도 화면 폭에 맞게 자르기
    if ((int)line2.length() > max2) line2 = line2.substring(0, max2);
    tft.setCursor(X_VALUE, Y_FILE);
    tft.print(line2);
  }

  // 셋째 줄 (있으면)
  if (line3.length() > 0 && y3 + H_2X <= tft.height() - 20) {
    if ((int)line3.length() > max3) line3 = line3.substring(0, max3);
    tft.setCursor(X_VALUE, y3);
    tft.print(line3);
  }
}

String safeStr(const JsonVariant& v) {
  if (v.is<const char*>()) return String(v.as<const char*>());
  if (v.is<String>()) return v.as<String>();
  return String("");
}

long safeNum(const JsonVariant& v, long d=0) {
  if (v.is<long>()) return v.as<long>();
  if (v.is<int>()) return (long)v.as<int>();
  if (v.is<float>()) return (long)v.as<float>();
  if (v.is<double>()) return (long)v.as<double>();
  if (v.is<const char*>()) return atol(v.as<const char*>());
  return d;
}

// ====== Time 문자열 파싱(가능한 ISO8601 형태 가정: YYYY-MM-DDThh:mm:ss[.SSS][Z|+09:00]) ======
bool parseTimestamp(const String& ts, int& y, int& m, int& d, int& hh, int& mm, int& ss, int& ms, bool& hasMs) {
  y = m = d = hh = mm = ss = ms = -1; hasMs = false;
  if (ts.length() < 10) return false;
  // 날짜 부분
  int p1 = ts.indexOf('-');
  if (p1 < 0) return false;
  int p2 = ts.indexOf('-', p1 + 1);
  if (p2 < 0) return false;
  int tSep = ts.indexOf('T', p2 + 1);
  if (tSep < 0) tSep = ts.indexOf(' ', p2 + 1); // 공백 구분 허용
  if (tSep < 0) return false;
  y = ts.substring(0, p1).toInt();
  m = ts.substring(p1 + 1, p2).toInt();
  d = ts.substring(p2 + 1, tSep).toInt();
  // 시간 부분
  int c1 = ts.indexOf(':', tSep + 1);
  if (c1 < 0) return false;
  int c2 = ts.indexOf(':', c1 + 1);
  if (c2 < 0) return false;
  hh = ts.substring(tSep + 1, c1).toInt();
  mm = ts.substring(c1 + 1, c2).toInt();
  // 초와 ms
  int dot = ts.indexOf('.', c2 + 1);
  int end = ts.length();
  // 초 끝 경계(Z, +, 공백 등) 검색
  int zPos = ts.indexOf('Z', c2 + 1);
  int plusPos = ts.indexOf('+', c2 + 1);
  int minusPos = ts.indexOf('-', c2 + 1); // 타임존 -hh:mm 구분자일 수 있어 주의
  if (dot >= 0) end = dot; else {
    if (zPos >= 0) end = min(end, zPos);
    if (plusPos >= 0) end = min(end, plusPos);
    if (minusPos >= 0) end = min(end, minusPos);
  }
  if (end <= c2 + 1) return false;
  ss = ts.substring(c2 + 1, end).toInt();
  if (dot >= 0) {
    // 밀리초 최대 3자리만 사용
    int msEnd = end;
    // dot 이후부터 숫자만 취합
    int i = dot + 1;
    String msStr = "";
    while (i < (int)ts.length() && isDigit(ts[i]) && (int)msStr.length() < 3) {
      msStr += ts[i++];
    }
    while ((int)msStr.length() < 3) msStr += '0'; // 2자리/1자리면 3자리로 패딩
    ms = msStr.toInt();
    hasMs = true;
  } else {
    hasMs = false;
    ms = 0;
  }
  return true;
}

String pad2(int v) { char buf[4]; snprintf(buf, sizeof(buf), "%02d", v); return String(buf); }
String pad3(int v) { char buf[5]; snprintf(buf, sizeof(buf), "%03d", v); return String(buf); }

void drawKeysBar(long ks) {
  uint16_t barBg = (gPrev.idleFlag == 1) ? THEME_KEYBAR_IDLE_BG : THEME_KEYBAR_ACTIVE_BG;
  tft.fillRect(0, tft.height()-20, tft.width(), 20, barBg);
  tft.setCursor(6, tft.height()-18);
  tft.setTextColor(THEME_KEYBAR_FG, barBg);
  tft.setTextSize(2);
  tft.print("Keys: "); tft.print(ks);
}

void clearSegment(int x, int y, int chars) {
  tft.fillRect(x, y, chars * CHAR_W_2X, H_2X, THEME_BG);
}

void drawDateFull(int y, int yy, int mm, int dd) {
  // YYYY-MM-DD 전체 그리기
  clearValueArea(y, H_2X);
  tft.setTextColor(THEME_VALUE_FG, THEME_BG);
  tft.setTextSize(2);
  int x = X_VALUE;
  tft.setCursor(x, y); tft.print(String(yy)); x += 4 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print('-');       x += 1 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print(pad2(mm));  x += 2 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print('-');       x += 1 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print(pad2(dd));
}

void drawTimeFull(int y, int hh, int mm, int ss, int ms, bool hasMs) {
  clearValueArea(y, H_2X);
  tft.setTextColor(THEME_VALUE_FG, THEME_BG);
  tft.setTextSize(2);
  int x = X_VALUE;
  tft.setCursor(x, y); tft.print(pad2(hh)); x += 2 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print(':');      x += 1 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print(pad2(mm)); x += 2 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print(':');      x += 1 * CHAR_W_2X;
  tft.setCursor(x, y); tft.print(pad2(ss)); x += 2 * CHAR_W_2X;
  if (hasMs) {
    tft.setCursor(x, y); tft.print('.');    x += 1 * CHAR_W_2X;
    tft.setCursor(x, y); tft.print(pad3(ms));
  }
}

void updateDateParts(int yy, int mm, int dd) {
  // x 위치 계산
  int xYYYY = X_VALUE;
  int xMM   = X_VALUE + (4 + 1) * CHAR_W_2X; // YYYY(4) + '-'(1)
  int xDD   = X_VALUE + (4 + 1 + 2 + 1) * CHAR_W_2X; // YYYY + '-' + MM(2) + '-'
  if (!gPrevTime.init) {
    drawDateFull(Y_TIME, yy, mm, dd);
  } else {
    if (yy != gPrevTime.y) { clearSegment(xYYYY, Y_TIME, 4); tft.setCursor(xYYYY, Y_TIME); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(String(yy)); }
    if (mm != gPrevTime.m) { clearSegment(xMM,   Y_TIME, 2); tft.setCursor(xMM,   Y_TIME); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(pad2(mm)); }
    if (dd != gPrevTime.d) { clearSegment(xDD,   Y_TIME, 2); tft.setCursor(xDD,   Y_TIME); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(pad2(dd)); }
  }
  gPrevTime.y = yy; gPrevTime.m = mm; gPrevTime.d = dd;
}

void updateTimeParts(int hh, int mm, int ss, int ms, bool hasMs) {
  // hasMs 토글 변화 시 전체 라인 재그리기 (길이가 달라짐)
  if (!gPrevTime.init || hasMs != gPrevTime.hasMs) {
    drawTimeFull(Y_TIME2, hh, mm, ss, ms, hasMs);
    gPrevTime.hh = hh; gPrevTime.mm = mm; gPrevTime.ss = ss; gPrevTime.ms = ms; gPrevTime.hasMs = hasMs;
    return;
  }
  // x 위치 계산
  int xHH = X_VALUE;
  int xMM = X_VALUE + (2 + 1) * CHAR_W_2X; // HH + ':'
  int xSS = X_VALUE + (2 + 1 + 2 + 1) * CHAR_W_2X; // HH + ':' + MM + ':'
  int xMS = X_VALUE + (2 + 1 + 2 + 1 + 2 + 1) * CHAR_W_2X; // HH:MM:SS + '.'
  if (hh != gPrevTime.hh) { clearSegment(xHH, Y_TIME2, 2); tft.setCursor(xHH, Y_TIME2); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(pad2(hh)); }
  if (mm != gPrevTime.mm) { clearSegment(xMM, Y_TIME2, 2); tft.setCursor(xMM, Y_TIME2); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(pad2(mm)); }
  if (ss != gPrevTime.ss) { clearSegment(xSS, Y_TIME2, 2); tft.setCursor(xSS, Y_TIME2); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(pad2(ss)); }
  if (hasMs && ms != gPrevTime.ms) { clearSegment(xMS, Y_TIME2, 3); tft.setCursor(xMS, Y_TIME2); tft.setTextSize(2); tft.setTextColor(THEME_VALUE_FG, THEME_BG); tft.print(pad3(ms)); }
  gPrevTime.hh = hh; gPrevTime.mm = mm; gPrevTime.ss = ss; gPrevTime.ms = ms; gPrevTime.hasMs = hasMs;
}

void updateFromSnapshot(const JsonObject& snap) {
  // 최초 1회 레이아웃
  if (!gPrev.drawn) {
    drawLayoutOnce();
    gPrev.drawn = true;
  }

  String ts   = safeStr(snap["timestamp"]);
  String lang = safeStr(snap["languageId"]);
  String file = safeStr(snap["filePath"]);
  String wsRoot = safeStr(snap["workspaceRoot"]);
  String br   = safeStr(snap["branch"]);
  long idleS  = safeNum(snap["idleForMs"], 0) / 1000;
  long actS   = safeNum(snap["sessionActiveMs"], 0) / 1000;
  long ks     = safeNum(snap["keystrokes"], 0);
  bool isIdle = false;
  if (snap["isIdle"].is<bool>()) {
    isIdle = snap["isIdle"].as<bool>();
  }

  // Time (두 줄, 부분 갱신) - KST(+9)로 변환, 소수초 미표시
  if (ts != gPrev.ts) {
    int yy, mm, dd, hh, mi, ss, ms; bool hasMs;
    if (parseTimestamp(ts, yy, mm, dd, hh, mi, ss, ms, hasMs)) {
      // 타임존 오프셋 계산 (Z=0, +HH:MM/-HH:MM). 기본 0
      int tSep = ts.indexOf('T'); if (tSep < 0) tSep = ts.indexOf(' ');
      int tzMin = 0; bool tzFound = false;
      if (tSep >= 0) {
        int zPos = ts.indexOf('Z', tSep + 1);
        int pPos = ts.indexOf('+', tSep + 1);
        int mPos = ts.indexOf('-', tSep + 1);
        if (zPos >= 0) { tzMin = 0; tzFound = true; }
        else if (pPos >= 0) {
          int hhOff = ts.substring(pPos + 1, pPos + 3).toInt();
          int mmOff = 0; if (ts.length() > pPos + 5 && ts.charAt(pPos + 3) == ':') mmOff = ts.substring(pPos + 4, pPos + 6).toInt();
          tzMin = hhOff * 60 + mmOff; tzFound = true;
        } else if (mPos >= 0) {
          int hhOff = ts.substring(mPos + 1, mPos + 3).toInt();
          int mmOff = 0; if (ts.length() > mPos + 5 && ts.charAt(mPos + 3) == ':') mmOff = ts.substring(mPos + 4, mPos + 6).toInt();
          tzMin = -(hhOff * 60 + mmOff); tzFound = true;
        }
      }
      // ts는 로컬(+오프셋) 표현. UTC = 로컬 - 오프셋
      auto daysInMonth = [](int y, int m){
        static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int d = mdays[m-1];
        bool leap = ((y%4==0 && y%100!=0) || (y%400==0));
        if (m==2 && leap) d = 29;
        return d;
      };
      auto addMinutes = [&](int &Y,int &M,int &D,int &H,int &I,long delta){
        long total = (long)I + delta;
        long carryH = floor((double)total / 60.0);
        I = (int)(total % 60); if (I < 0) { I += 60; carryH -= 1; }
        long Htot = (long)H + carryH;
        long carryD = floor((double)Htot / 24.0);
        H = (int)(Htot % 24); if (H < 0) { H += 24; carryD -= 1; }
        long Dt = (long)D + carryD;
        // 일 단위 조정
        while (Dt < 1) {
          M -= 1; if (M < 1) { M = 12; Y -= 1; }
          Dt += daysInMonth(Y, M);
        }
        while (true) {
          int dim = daysInMonth(Y, M);
          if (Dt <= dim) break;
          Dt -= dim; M += 1; if (M > 12) { M = 1; Y += 1; }
        }
        D = (int)Dt;
      };
      // UTC로 이동 후 KST(+540분)
      if (tzFound && tzMin != 0) addMinutes(yy, mm, dd, hh, mi, -tzMin);
      addMinutes(yy, mm, dd, hh, mi, 9 * 60);
      // 날짜/시간 갱신 (ms는 표시 안 함)
      updateDateParts(yy, mm, dd);
      updateTimeParts(hh, mi, ss, 0, false);
      gPrevTime.init = true;
    } else {
      // 파싱 실패 시 첫 줄에 원문 출력, 둘째 줄은 클리어
      clearValueArea(Y_TIME, H_2X);
      printValue2x(Y_TIME, ts);
      clearValueArea(Y_TIME2, H_2X);
      gPrevTime.init = false;
    }
    gPrev.ts = ts;
  }
  // Lang
  if (lang != gPrev.lang) {
    clearValueArea(Y_LANG, H_2X);
    printValue2x(Y_LANG, lang);
    gPrev.lang = lang;
  }
  // Branch
  if (br != gPrev.br) {
    clearValueArea(Y_BRANCH, H_2X);
    printValue2x(Y_BRANCH, br);
    gPrev.br = br;
  }
  // Idle (seconds + state label)
  if (idleS != gPrev.idleS || (int8_t)(isIdle ? 1 : 0) != gPrev.idleFlag) {
    int8_t prevFlag = gPrev.idleFlag;
    clearValueArea(Y_IDLE, H_2X);
    String state = isIdle ? "(IDLE)" : "(ACTIVE)";
    printValue2x(Y_IDLE, String(idleS) + "s " + state);
    gPrev.idleS = idleS;
    gPrev.idleFlag = isIdle ? 1 : 0;
    // idle 상태가 바뀌었을 때 키 바 색도 즉시 반영
    if (gPrev.drawn && prevFlag != gPrev.idleFlag) {
      drawKeysBar(ks);
    }
  }
  // Active (seconds)
  if (actS != gPrev.actS) {
    clearValueArea(Y_ACTIVE, H_2X);
    printValue2x(Y_ACTIVE, String(actS) + "s");
    gPrev.actS = actS;
  }
  // File (workspaceRoot 기준 상대 경로: <rootFolder>\...)
  auto normBackslashes = [](String s){ s.replace('/', '\\'); return s; };
  auto baseName = [](const String& s){ int p = s.lastIndexOf('\\'); return (p>=0? s.substring(p+1): s); };
  String dispPath = file;
  if (wsRoot.length() > 0 && file.length() > 0) {
    String r0 = normBackslashes(wsRoot);
    String p0 = normBackslashes(file);
    String rL = r0; rL.toLowerCase();
    String pL = p0; pL.toLowerCase();
    if (pL.startsWith(rL)) {
      String base = baseName(r0);
      String rem = p0.substring(r0.length());
      if (rem.length() > 0 && rem.charAt(0) != '\\') rem = String('\\') + rem;
      dispPath = base + rem;
    } else {
      String base = baseName(r0);
      String pat = String('\\') + base; pat.toLowerCase(); pat += '\\';
      int idx = pL.indexOf(pat);
      if (idx >= 0) dispPath = p0.substring(idx + 1); // 앞의 '\' 제외
      else dispPath = p0;
    }
  }
  if (dispPath != gPrev.file) {
    printFile1x(dispPath);
    gPrev.file = dispPath;
  }

  // Keys (하단 바)
  if (ks != gPrev.keys) {
    drawKeysBar(ks);
    gPrev.keys = ks;
  }
}

void drawHttpError(int code, const String& body) {
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("HTTP ERR");
  tft.setCursor(6, 26);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.printf("Code %d", code);
  tft.setTextSize(1);
  tft.setCursor(6, 48);
  String line = body.substring(0, min((size_t)120, body.length()));
  tft.print(line);
}

int fetchLatest(JsonDocument& doc, String& respBody) {
  if (WiFi.status() != WL_CONNECTED) return -1;
  HTTPClient http;
  http.setTimeout(8000);
  http.begin(BACKEND_URL);
  http.addHeader("x-api-key", API_KEY);
  int code = http.GET();
  respBody = http.getString();
  http.end();
  Serial.printf("HTTP %d, %u bytes\n", code, (unsigned)respBody.length());
  if (code != 200) return code;
  DeserializationError err = deserializeJson(doc, respBody);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return -2;
  }
  return 200;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Display init
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
#ifdef PANEL_240x240
  tft.init(240, 240);
#else
  tft.init(240, 320);
#endif
  tft.setSPISpeed(40000000);
  tft.setRotation(TFT_ROTATION);
  backlightSetup(200);

  tft.fillScreen(THEME_BG);
  if (DEBUG_FRAME) {
    tft.drawRect(0, 0, tft.width()-1, tft.height()-1, ST77XX_RED);
    // 중앙 스플래시 텍스트
    tft.setTextColor(ST77XX_WHITE, THEME_BG);
    tft.setTextSize(2);
    int cx = (tft.width()/2) - (6*2*3); // 대략 6px*size*문자수 로 중앙 근사
    int cy = (tft.height()/2) - 10;
    tft.setCursor(max(0, cx), max(0, cy));
    tft.print("BOOT");
  }
  drawHeader("WiFi...");
  tft.setCursor(6, 26);
  tft.setTextColor(THEME_VALUE_FG);
  tft.setTextSize(2);
  tft.print("Connecting "); tft.print(WIFI_SSID);

  // WiFi connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(300);
    tft.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    drawLayoutOnce();
    // Ready 메시지는 Time 라인에 잠시 표시
    clearValueArea(Y_TIME, H_2X);
    printValue2x(Y_TIME, String("IP ") + WiFi.localIP().toString());
    clearValueArea(Y_TIME2, H_2X);
  } else {
    tft.fillScreen(THEME_BG);
    drawHeader("WiFi FAIL");
    tft.setCursor(6, 26);
    tft.print("Check SSID/PW");
  }
}

uint32_t lastPoll = 0;
const uint32_t POLL_MS = 5000; // 5초마다 갱신

void loop() {
  if (WiFi.status() == WL_CONNECTED && millis() - lastPoll > POLL_MS) {
    lastPoll = millis();
    StaticJsonDocument<2048> doc; // result 필드 안 스냅샷을 고려한 여유 버퍼
    String body;
    int code = fetchLatest(doc, body);
    if (code == 200) {
      // 래핑된 구조에서 result만 추출
      JsonVariant res = doc["result"];
      if (res.is<JsonObject>()) {
        updateFromSnapshot(res.as<JsonObject>());
      } else {
        // 값 영역만 No Data 표시 (Time 라인 활용)
        clearValueArea(Y_TIME, H_2X);
        printValue2x(Y_TIME, "No Data");
      }
    } else {
      drawHttpError(code, body);
    }
  }
}
