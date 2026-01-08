/* ===== ESP32 + Waveshare 4.2" V2: Clock + Today's Schedule (Korean) ===== */
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "secrets.h" // Provides ssid, password, ha_base_url, ha_token

// ===== Config =====
static const long   GMT_OFFSET_SEC = 9 * 3600; // KST
static const int    DST_OFFSET_SEC = 0;
static const char*  NTP_SERVER     = "pool.ntp.org";

// ===== Display Layout (400x300) =====
static const UWORD EPD_W = EPD_4IN2_V2_WIDTH;   // 400
static const UWORD EPD_H = EPD_4IN2_V2_HEIGHT;  // 300

// We use a single full-screen buffer to avoid partial update artifacts/disappearing regions
static UBYTE* gCanvas = nullptr;

// Clock Logic
static const UWORD CLOCK_Y = 15;
static const UWORD SCH_Y = 130;  // Schedule starts

// Fonts
#define DATE_FONT   Font20KR
#define TIME_FONT   Maple44
#define HEADER_FONT Font20KR
#define ITEM_FONT   Font16KR

// State
static int8_t lastFullRefreshHour = -1;
static int8_t lastFullRefreshMin  = -1;
static long   lastScheduleUpdate  = 0;
String        todayScheduleCache = ""; 

// Utility
static bool getNowKST(struct tm& t) {
  return getLocalTime(&t, 2000);
}

// Fetch Events Logic (Same as before)
void fetchTodaySchedule(int year, int month, int day, String &output) {
  if (WiFi.status() != WL_CONNECTED) {
    output = "WiFi Disconnected";
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // 1. Get List of Calendars
  String listUrl = String(ha_base_url) + "/api/calendars";
  
  http.begin(client, listUrl);
  http.addHeader("Authorization", String("Bearer ") + ha_token);
  http.addHeader("Content-Type", "application/json");
  
  int code = http.GET();
  if (code != 200) {
      http.end();
      return;
  }
  
  String listPayload = http.getString();
  http.end();

  DynamicJsonDocument listDoc(8192);
  deserializeJson(listDoc, listPayload);
  
  JsonArray calendars = listDoc.as<JsonArray>();
  DynamicJsonDocument eventsDoc(16384);
  JsonArray allEvents = eventsDoc.to<JsonArray>();

  // 2. Fetch Events
  for (JsonObject cal : calendars) {
      const char* entity_id = cal["entity_id"];
      if (!entity_id) continue;

      String eventsUrl = String(ha_base_url) + "/api/calendars/" + entity_id;
      char query[128];
      sprintf(query, "?start=%04d-%02d-%02dT00:00:00&end=%04d-%02d-%02dT23:59:59", year, month, day, year, month, day);
      eventsUrl += query;

      http.begin(client, eventsUrl);
      http.addHeader("Authorization", String("Bearer ") + ha_token);
      
      if (http.GET() == 200) {
          DynamicJsonDocument tempDoc(4096); 
          deserializeJson(tempDoc, http.getString());
          JsonArray events = tempDoc.as<JsonArray>();
          for (JsonVariant v : events) {
              allEvents.add(v);
          }
      }
      http.end();
  }
  
  // 3. Output
  output = "";
  if (allEvents.size() == 0) {
      output = "일정이 없습니다.";
  } else {
      for (JsonObject event : allEvents) {
          const char* summary = event["summary"];
          if (!summary) continue;
          String sumStr = String(summary);
          if (sumStr.endsWith("님의 생일")) continue;

          String timePrefix = ""; 
          if (event["start"]["dateTime"]) {
             String sDt = event["start"]["dateTime"];
             String eDt = event["end"]["dateTime"]; // usually exists if start has dateTime
             
             String sTime = (sDt.length() >= 16) ? sDt.substring(11, 16) : "00:00";
             String eTime = (eDt.length() >= 16) ? eDt.substring(11, 16) : "00:00";
             
             timePrefix = "[" + sTime + " ~ " + eTime + "] ";
          } else {
             timePrefix = "[종일] ";
          }
          output += timePrefix + sumStr + "\n";
      }
      if (output.length() == 0) output = "일정이 없습니다.";
  }
}

// Draw Schedule to Global Canvas
void drawScheduleToCanvas(String text) {
  // Clear Schedule Area (White Rectangle)
  // X: 0-400, Y: SCH_Y onwards
  Paint_DrawRectangle(0, SCH_Y, EPD_W, EPD_H, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

  // Draw Header
  Paint_DrawString_CN(10, SCH_Y, "오늘의 일정", &HEADER_FONT, BLACK, WHITE);
  Paint_DrawLine(10, SCH_Y + 25, EPD_W - 10, SCH_Y + 25, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

  // Draw Items
  int x = 10;
  int y = SCH_Y + 40; 
  int lineH = 28; // 16px font, 28px spacing

  int sIdx = 0;
  const char* str = text.c_str();
  
  while (str[sIdx] != '\0' && y < EPD_H - lineH) {
      int eIdx = sIdx;
      while(str[eIdx] != '\n' && str[eIdx] != '\0') eIdx++;
      String line = text.substring(sIdx, eIdx);
      
      Paint_DrawString_CN(x, y, line.c_str(), &ITEM_FONT, BLACK, WHITE);
      
      y += lineH;
      sIdx = eIdx;
      if (str[sIdx] == '\n') sIdx++;
  }
}

// Manual helper to draw time with Maple44
void drawTimeManual(int x, int y, const char* str) {
  for (int i=0; str[i] != '\0'; i++) {
     char c = str[i];
     if (c == ':') {
        Paint_DrawChar(x, y, ':', (sFONT*)&Maple44, BLACK, WHITE);
        x += 20; // Colon advance
     } else {
        Paint_DrawChar(x, y, c, (sFONT*)&Maple44, BLACK, WHITE);
        x += 35; // Digit advance (Reduced from 38 to make it tighter if needed, or keeping 36-38? Photo showed overlapping next char? No, Photo showed "32" close to ":". 
                 // Previous code: Colon adv 20. Digit adv 38. 
                 // If : is at X, next digit at X+20. 
                 // In photo, "3" is very close to ":". 20 is too small.
                 // Increase Colon Advance to 25.
     }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Init EPD
  DEV_Module_Init();
  EPD_4IN2_V2_Init();
  EPD_4IN2_V2_Clear();
  DEV_Delay_ms(500);

  // Alloc Full Buffer
  UWORD imagesize = ((EPD_W % 8 == 0)? (EPD_W / 8 ): (EPD_W / 8 + 1)) * EPD_H;
  gCanvas = (UBYTE*)malloc(imagesize);
  if (!gCanvas) {
      Serial.println("Buffer Alloc Failed");
      return;
  }

  // Init Buffer
  Paint_NewImage(gCanvas, EPD_W, EPD_H, 0, WHITE);
  Paint_SelectImage(gCanvas);
  Paint_Clear(WHITE);

  // WiFi
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500); Serial.print("."); retry++;
  }
  
  // NTP
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);

  // Init for Partial Update
  EPD_4IN2_V2_Init_Fast(Seconds_1S);
}

void loop() {
  static uint32_t lastTick = 0;
  if (millis() - lastTick < 1000) return;
  lastTick = millis();

  struct tm ti;
  if (!getNowKST(ti)) return;

  // 1. Refresh Schedule Data
  bool scheduleDataChanged = false;
  // Update on start or every 30 mins (sync with full refresh)
  if ((todayScheduleCache == "") || ((ti.tm_min == 0 || ti.tm_min == 30) && ti.tm_sec == 0)) {
      String newSchedule;
      fetchTodaySchedule(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, newSchedule);
      if (newSchedule != todayScheduleCache) {
          todayScheduleCache = newSchedule;
          scheduleDataChanged = true;
          lastScheduleUpdate = millis();
      }
  }

  // 2. Full Refresh Check (Every 30 mins)
  bool didFullRefresh = false;
  if ((ti.tm_min == 0 || ti.tm_min == 30) && ti.tm_sec == 0) {
      if (lastFullRefreshHour != ti.tm_hour || lastFullRefreshMin != ti.tm_min) {
          EPD_4IN2_V2_Init();
          EPD_4IN2_V2_Clear();
          EPD_4IN2_V2_Init_Fast(Seconds_1S); // Back to fast mode
          
          lastFullRefreshHour = ti.tm_hour;
          lastFullRefreshMin = ti.tm_min;
          didFullRefresh = true;
          
          // Force redraw of everything to buffer (buffer might be stale relative to screen clearing, though buffer content is valid)
      }
  }

  // 3. Draw to Global Canvas
  Paint_SelectImage(gCanvas);
  
  // Clear Clock Area only
  Paint_DrawRectangle(0, 0, EPD_W, SCH_Y - 1, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

  // Draw Date
  char dateStr[64];
  // Change format to YYYY년 MM월 DD일
  snprintf(dateStr, sizeof(dateStr), "%04d년 %02d월 %02d일", 1900 + ti.tm_year, 1 + ti.tm_mon, ti.tm_mday);
  
  // Center Date
  // Font20KR
  // "2026년 01월 08일"
  // Digits/Space: 2026 01 08 -> 8 chars. (Space is half now 10px? Font20? ASCIIW=15 -> 7.5?)
  // ASCII Width (Font20) is usually 15. Spaces: 2. 
  // Chars: '년', '월', '일' -> 3 chars * 20px = 60px.
  // Digits: 8 * 15px = 120px. 
  // Spaces: 2 * 7px = 14px.
  // Total ~ 194px.
  // Center: (384 - 194) / 2 = 95.
  Paint_DrawString_CN(95, 5, dateStr, &DATE_FONT, BLACK, WHITE);

  // Draw Time
  char timeStr[20];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
  
  // Center Manually
  // 6 digits * 40 = 240. 2 colons * 30 = 60. Total 300.
  // (400 - 300) / 2 = 50.
  int timeX = 50;
  for (int i=0; timeStr[i] != '\0'; i++) {
     if (timeStr[i] == ':') {
        Paint_DrawChar(timeX, 40, ':', (sFONT*)&Maple44, BLACK, WHITE);
        timeX += 30; // Increased spacing
     } else {
        Paint_DrawChar(timeX, 40, timeStr[i], (sFONT*)&Maple44, BLACK, WHITE);
        timeX += 40; // Increased spacing
     }
  }

  // Update Schedule in Buffer if needed or Full Refresh happened
  if (scheduleDataChanged || didFullRefresh || todayScheduleCache != "") {
      // Just redraw schedule every time to be safe? 
      // No, strictly only if changed. But if Full Refresh cleared screen, we must redraw schedule pixels to screen.
      // The buffer 'gCanvas' RETAINS the schedule pixels from previous loop.
      // So if (didFullRefresh), 'gCanvas' still has schedule. We just send it.
      // BUT if 'scheduleDataChanged', we must clear and rewrite schedule area in 'gCanvas'.
      if (scheduleDataChanged) {
         drawScheduleToCanvas(todayScheduleCache);
      }
  }

  // 4. Update Screen (Partial Update of FULL frame)
  // This sends the whole 400x300 buffer using Partial Update command.
  // This ensures NO part of the screen is "forgotten" or "cleared" by the controller.
  EPD_4IN2_V2_PartialDisplay(gCanvas, 0, 0, EPD_W, EPD_H);
}

