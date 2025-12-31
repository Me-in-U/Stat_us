#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "secrets.h"

// Display resolution
#define EPD_WIDTH 800
#define EPD_HEIGHT 480

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 9 * 3600; // Korea Standard Time (UTC+9)
const int   daylightOffset_sec = 0;

// Image buffer
UBYTE *BlackImage;

// Forward declarations
void drawCalendar(struct tm *timeinfo);
void fetchEvents(int year, int month, int daysInMonth, DynamicJsonDocument *doc);
void drawEventsForDay(int year, int month, int day, int x, int y, int w, int h, DynamicJsonDocument *doc);

void setup() {
  Serial.begin(115200);
  
  // Initialize Display
  printf("EPD_7IN5_V2_test Demo\r\n");
  DEV_Module_Init();

  printf("e-Paper Init and Clear...\r\n");
  EPD_7IN5_V2_Init();
  EPD_7IN5_V2_Clear();
  DEV_Delay_ms(500);

  // Create a new image cache
  UWORD Imagesize = ((EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1)) * EPD_HEIGHT;
  if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
      printf("Failed to apply for black memory...\r\n");
      while(1);
  }
  printf("Paint_NewImage\r\n");
  Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 0, WHITE);
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  // Draw Calendar
  drawCalendar(&timeinfo);

  // Display the frame
  EPD_7IN5_V2_Display(BlackImage);
  DEV_Delay_ms(2000);

  // Sleep
  printf("Goto Sleep...\r\n");
  EPD_7IN5_V2_Sleep();
  free(BlackImage);
  BlackImage = NULL;
}

void loop() {
  // Nothing to do here
  delay(100000);
}

void fetchEvents(int year, int month, int daysInMonth, DynamicJsonDocument *doc) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Skip SSL verification
  HTTPClient http;

  // 1. Get List of Calendars
  String listUrl = String(ha_base_url) + "/api/calendars";
  Serial.println("Fetching Calendars List: " + listUrl);
  
  http.begin(client, listUrl);
  http.addHeader("Authorization", String("Bearer ") + ha_token);
  http.addHeader("Content-Type", "application/json");
  
  int code = http.GET();
  if (code != 200) {
      Serial.printf("Failed to fetch calendar list: %d\n", code);
      if (code > 0) Serial.println(http.getString());
      http.end();
      return;
  }
  
  String listPayload = http.getString();
  http.end();

  // Parse List
  DynamicJsonDocument listDoc(8192);
  DeserializationError listError = deserializeJson(listDoc, listPayload);
  if (listError) {
    Serial.print("List deserializeJson() failed: ");
    Serial.println(listError.f_str());
    return;
  }
  
  JsonArray calendars = listDoc.as<JsonArray>();
  JsonArray allEvents = doc->to<JsonArray>(); // Initialize main doc as array

  // 2. Iterate and Fetch Events
  for (JsonObject cal : calendars) {
      const char* entity_id = cal["entity_id"];
      if (!entity_id) continue;

      String eventsUrl = String(ha_base_url) + "/api/calendars/" + entity_id;
      char query[128];
      sprintf(query, "?start=%04d-%02d-01T00:00:00&end=%04d-%02d-%02dT23:59:59", year, month, year, month, daysInMonth);
      eventsUrl += query;

      Serial.print("Fetching events for: ");
      Serial.println(entity_id);
      Serial.println(eventsUrl);
      
      http.begin(client, eventsUrl);
      http.addHeader("Authorization", String("Bearer ") + ha_token);
      http.addHeader("Content-Type", "application/json");
      
      int eventCode = http.GET();
      if (eventCode == 200) {
          String eventPayload = http.getString();
          // Use a temporary doc for parsing each calendar's events
          DynamicJsonDocument tempDoc(16384); 
          DeserializationError err = deserializeJson(tempDoc, eventPayload);
          if (!err) {
              JsonArray events = tempDoc.as<JsonArray>();
              for (JsonVariant v : events) {
                  // Deep copy event to main doc
                  allEvents.add(v);
              }
              Serial.printf("  Found %d events\n", events.size());
          } else {
              Serial.print("  JSON Error: ");
              Serial.println(err.f_str());
          }
      } else {
          Serial.printf("  Failed: %d\n", eventCode);
      }
      http.end();
  }
  Serial.printf("Total events fetched: %d\n", allEvents.size());
}

void drawEventsForDay(int year, int month, int day, int x, int y, int w, int h, DynamicJsonDocument *doc) {
  int eventY = y + 25;
  JsonArray events = doc->as<JsonArray>();
  
  for (JsonObject event : events) {
    const char* summary = event["summary"];
    const char* start = event["start"]["date"]; // All day
    const char* startDateTime = event["start"]["dateTime"]; // Timed

    String eventDateStr;
    if (start) {
      eventDateStr = String(start);
    } else if (startDateTime) {
      eventDateStr = String(startDateTime).substring(0, 10);
    }

    int eYear = eventDateStr.substring(0, 4).toInt();
    int eMonth = eventDateStr.substring(5, 7).toInt();
    int eDay = eventDateStr.substring(8, 10).toInt();

    if (eYear == year && eMonth == month && eDay == day) {
      // Draw event
      // Truncate summary to fit width dynamically
      // Font12KR: ASCII ~9px (0.75), KR 12px
      int asciiW = 9;
      int krW = 12;
      int availableW = w - 5; // Increased padding (4px left + 1px right)
      
      char buffer[128]; 
      int currentW = 0;
      int srcIdx = 0;
      int dstIdx = 0;
      
      while (summary[srcIdx] != '\0') {
          unsigned char c = (unsigned char)summary[srcIdx];
          int len = 1;
          int charW = asciiW;
          
          if (c == ' ' || c == '[' || c == ']') charW = asciiW / 2; // Narrow chars
          
          if (c >= 0xC0) { // Multi-byte
             charW = krW;
             if (c >= 0xF0) len = 4;
             else if (c >= 0xE0) len = 3;
             else len = 2;
          }
          
          if (currentW + charW > availableW) break;
          if (dstIdx + len >= 127) break; // Buffer safety
          
          for(int k=0; k<len; k++) {
              buffer[dstIdx++] = summary[srcIdx++];
          }
          currentW += charW;
      }
      buffer[dstIdx] = '\0';
      
      // Use CN function for Korean support
      // Note: Paint_DrawString_CN handles UTF-8 if the font table is generated correctly
      Paint_DrawString_CN(x + 4, eventY, buffer, &Font12KR, BLACK, WHITE);
      eventY += 12; // Font height
      if (eventY > y + h - 12) break; // No more space
    }
  }
}

void drawCalendar(struct tm *timeinfo) {
  int year = timeinfo->tm_year + 1900;
  int month = timeinfo->tm_mon + 1; // 1-12
  
  // Draw Days of Week (Korean)
  const char* days[] = {"일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일"};
  int cellWidth = EPD_WIDTH / 7;
  int headerHeight = 30; // Reduced header height (only for day names)
  int cellHeight = (EPD_HEIGHT - headerHeight) / 6; // Max 6 rows

  for (int i = 0; i < 7; i++) {
    // Center the text
    // Font20KR width is 20. "일요일" is 3 chars -> 60px width.
    int textWidth = 20 * 3;
    int x = i * cellWidth + (cellWidth - textWidth) / 2;
    Paint_DrawString_CN(x, 5, days[i], &Font20KR, BLACK, WHITE);
  }

  // Calculate first day of month
  struct tm firstDay = *timeinfo;
  firstDay.tm_mday = 1;
  mktime(&firstDay);
  int startDayOfWeek = firstDay.tm_wday; // 0=Sun, 6=Sat

  // Calculate days in month
  int daysInMonth;
  if (month == 2) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
      daysInMonth = 29;
    else
      daysInMonth = 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    daysInMonth = 30;
  } else {
    daysInMonth = 31;
  }

  // Fetch Events
  // Allocate on heap to avoid stack overflow
  // Increased size to hold events from multiple calendars
  DynamicJsonDocument *doc = new DynamicJsonDocument(65536); // 64KB
  fetchEvents(year, month, daysInMonth, doc);

  // Draw Grid and Days
  int currentDay = 1;
  for (int row = 0; row < 6; row++) {
    for (int col = 0; col < 7; col++) {
      if (row == 0 && col < startDayOfWeek) {
        continue;
      }
      if (currentDay > daysInMonth) {
        break;
      }

      int x = col * cellWidth;
      int y = headerHeight + row * cellHeight;

      // Draw Horizontal Lines (Top and Bottom)
      Paint_DrawLine(x, y, x + cellWidth, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
      Paint_DrawLine(x, y + cellHeight, x + cellWidth, y + cellHeight, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

      // Draw Vertical Lines
      // Left border: Skip for Sunday (col 0)
      if (col > 0) {
        Paint_DrawLine(x, y, x, y + cellHeight, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
      }
      // Right border: Skip for Saturday (col 6)
      if (col < 6) {
        Paint_DrawLine(x + cellWidth, y, x + cellWidth, y + cellHeight, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
      }

      // Draw Day Number (Using Korean Font for style)
      char dayStr[4];
      sprintf(dayStr, "%d", currentDay);
      Paint_DrawString_CN(x + 3, y + 3, dayStr, &Font20KR, BLACK, WHITE);

      // Draw Events for this day
      drawEventsForDay(year, month, currentDay, x, y, cellWidth, cellHeight, doc);

      currentDay++;
    }
  }
  
  delete doc;
}
