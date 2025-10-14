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

void backlightSetup(uint8_t brightness /*0~255*/) {
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, brightness);
}

void drawHeader(const char* title) {
  tft.fillRect(0, 0, TFT_W, 20, ST77XX_BLUE);
  tft.setCursor(6, 4);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print(title);
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

void drawSnapshot(const JsonObject& snap) {
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Stat-us");
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  int y = 26;
  auto line = [&](const char* k, const String& v) {
    tft.setCursor(6, y);
    tft.print(k);
    tft.print(": ");
    tft.print(v);
    y += 20;
  };

  String ts   = safeStr(snap["timestamp"]);
  String lang = safeStr(snap["languageId"]);
  String file = safeStr(snap["filePath"]);
  String br   = safeStr(snap["branch"]);
  long idleMs = safeNum(snap["idleForMs"], 0);
  long actMs  = safeNum(snap["sessionActiveMs"], 0);
  long ks     = safeNum(snap["keystrokes"], 0);

  line("Time", ts);
  line("Lang", lang);
  line("Branch", br);
  line("Idle", String(idleMs/1000) + "s");
  line("Active", String(actMs/1000) + "s");

  // 파일 경로는 길 수 있어 한 줄 더 사용
  tft.setCursor(6, y); tft.print("File:"); y += 20;
  tft.setCursor(6, y); tft.setTextSize(1); tft.print(file); tft.setTextSize(2); y += 16;

  // 하단 상태
  tft.fillRect(0, TFT_H-20, TFT_W, 20, ST77XX_BLACK);
  tft.setCursor(6, TFT_H-18);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print("Keys: "); tft.print(ks);
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
  tft.setRotation(1);
  backlightSetup(200);

  tft.fillScreen(ST77XX_BLACK);
  drawHeader("WiFi...");
  tft.setCursor(6, 26);
  tft.setTextColor(ST77XX_WHITE);
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
    tft.fillScreen(ST77XX_BLACK);
    drawHeader("Ready");
    tft.setCursor(6, 26);
    tft.print("IP: "); tft.print(WiFi.localIP());
  } else {
    tft.fillScreen(ST77XX_BLACK);
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
        drawSnapshot(res.as<JsonObject>());
      } else {
        tft.fillScreen(ST77XX_BLACK);
        drawHeader("No Data");
      }
    } else {
      drawHttpError(code, body);
    }
  }
}
