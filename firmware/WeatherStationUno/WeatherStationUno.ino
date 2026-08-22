#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <SoftwareSerial.h>
#include "secrets.h"

/*
  GSM Weather Monitoring Station
  Board: Arduino Uno
  Sensors: AHT20 + BMP280 (I2C)
  Display: 16x2 I2C LCD
  Modem: SIM800L on pins D10/D11

  Copy secrets.example.h to secrets.h and configure it before uploading.

  Required libraries:
    LiquidCrystal_I2C
    Adafruit AHTX0
    Adafruit BMP280 Library
    Adafruit Unified Sensor
*/

// ---------------- Pin and device configuration ----------------
static const uint8_t SIM800_RX_PIN = 10; // Uno receives from SIM800L TX
static const uint8_t SIM800_TX_PIN = 11; // Uno transmits to SIM800L RX
static const uint8_t LCD_ADDRESS = 0x27;
static const uint8_t LCD_COLUMNS = 16;
static const uint8_t LCD_ROWS = 2;

// ---------------- Station and network configuration ----------------
static const char STATION_ID[] = "WS_ABU_001";
static const char APN[] = "web.gprs.mtnnigeria.net";
static const char APN_USER[] = "";
static const char APN_PASSWORD[] = "";

static const char EDGE_FUNCTION_URL[] = WEATHER_EDGE_FUNCTION_URL;
static const char STATION_API_KEY[] = WEATHER_STATION_API_KEY;

// ---------------- Timing ----------------
static const unsigned long SENSOR_INTERVAL_MS = 10000UL;
static const unsigned long LCD_PAGE_INTERVAL_MS = 3000UL;
static const unsigned long UPLOAD_INTERVAL_MS = 300000UL; // 5 minutes
static const unsigned long RETRY_INTERVAL_MS = 60000UL;
static const unsigned long MODEM_RESPONSE_TIMEOUT_MS = 10000UL;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
SoftwareSerial sim800(SIM800_RX_PIN, SIM800_TX_PIN);

struct WeatherReading {
  float temperatureC;
  float humidityPercent;
  float pressureHpa;
  float bmpTemperatureC;
  float altitudeM;
  int signalRssi;
  bool valid;
};

WeatherReading reading = {NAN, NAN, NAN, NAN, NAN, 99, false};

bool ahtOnline = false;
bool bmpOnline = false;
bool modemOnline = false;
bool networkRegistered = false;
bool lastUploadOk = false;
int lastHttpStatus = 0;
uint8_t lcdPage = 0;
unsigned long lastSensorReadMs = 0;
unsigned long lastLcdPageMs = 0;
unsigned long lastUploadAttemptMs = 0;
unsigned long lastSuccessfulUploadMs = 0;

void clearModemInput() {
  while (sim800.available()) sim800.read();
}

bool waitForModemText(const char *expected, unsigned long timeoutMs) {
  char response[220];
  size_t index = 0;
  response[0] = '\0';
  unsigned long started = millis();

  while (millis() - started < timeoutMs) {
    while (sim800.available()) {
      char c = (char)sim800.read();
      Serial.write(c);
      if (index < sizeof(response) - 1) {
        response[index++] = c;
        response[index] = '\0';
      }
      if (strstr(response, expected) != NULL) return true;
      if (strstr(response, "ERROR") != NULL) return false;
    }
  }
  return false;
}

bool sendAT(const __FlashStringHelper *command,
            const char *expected = "OK",
            unsigned long timeoutMs = MODEM_RESPONSE_TIMEOUT_MS) {
  clearModemInput();
  Serial.print(F("\n[MODEM TX] "));
  Serial.println(command);
  sim800.println(command);
  return waitForModemText(expected, timeoutMs);
}

bool sendATBuffer(const char *command,
                  const char *expected = "OK",
                  unsigned long timeoutMs = MODEM_RESPONSE_TIMEOUT_MS) {
  clearModemInput();
  Serial.print(F("\n[MODEM TX] "));
  Serial.println(command);
  sim800.println(command);
  return waitForModemText(expected, timeoutMs);
}

bool sendATSecretBuffer(const char *command,
                        const char *expected = "OK",
                        unsigned long timeoutMs = MODEM_RESPONSE_TIMEOUT_MS) {
  clearModemInput();
  Serial.println(F("\n[MODEM TX] <redacted station authentication>"));
  sim800.println(command);
  return waitForModemText(expected, timeoutMs);
}

void printPadded(const char *text) {
  uint8_t count = 0;
  while (*text && count < LCD_COLUMNS) {
    lcd.print(*text++);
    count++;
  }
  while (count++ < LCD_COLUMNS) lcd.print(' ');
}

void showBootMessage(const __FlashStringHelper *line1,
                     const __FlashStringHelper *line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

bool initializeSensors() {
  Serial.println(F("[INIT] Starting I2C sensors"));
  ahtOnline = aht.begin();

  bmpOnline = bmp.begin(0x76);
  if (!bmpOnline) bmpOnline = bmp.begin(0x77);

  if (bmpOnline) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }

  Serial.print(F("[AHT20] "));
  Serial.println(ahtOnline ? F("ONLINE") : F("FAILED"));
  Serial.print(F("[BMP280] "));
  Serial.println(bmpOnline ? F("ONLINE") : F("FAILED"));
  return ahtOnline && bmpOnline;
}

bool isNetworkRegistered() {
  clearModemInput();
  sim800.println(F("AT+CREG?"));

  char response[80];
  size_t index = 0;
  response[0] = '\0';
  unsigned long started = millis();

  while (millis() - started < 4000UL) {
    while (sim800.available()) {
      char c = (char)sim800.read();
      Serial.write(c);
      if (index < sizeof(response) - 1) {
        response[index++] = c;
        response[index] = '\0';
      }
    }
  }
  return strstr(response, ",1") != NULL || strstr(response, ",5") != NULL;
}

int readSignalStrength() {
  clearModemInput();
  sim800.println(F("AT+CSQ"));

  char response[80];
  size_t index = 0;
  response[0] = '\0';
  unsigned long started = millis();

  while (millis() - started < 3000UL) {
    while (sim800.available()) {
      char c = (char)sim800.read();
      Serial.write(c);
      if (index < sizeof(response) - 1) {
        response[index++] = c;
        response[index] = '\0';
      }
    }
  }

  char *position = strstr(response, "+CSQ:");
  if (!position) return 99;
  int rssi = atoi(position + 5);
  return (rssi >= 0 && rssi <= 31) ? rssi : 99;
}

bool initializeModem() {
  Serial.println(F("\n[INIT] Starting SIM800L"));
  showBootMessage(F("Starting GSM..."), F("Please wait"));

  bool responded = false;
  for (uint8_t attempt = 0; attempt < 5 && !responded; attempt++) {
    responded = sendAT(F("AT"), "OK", 3000UL);
    if (!responded) delay(1000);
  }
  if (!responded) return false;

  sendAT(F("ATE0"));
  sendAT(F("AT+CPIN?"), "READY", 5000UL);
  networkRegistered = isNetworkRegistered();
  reading.signalRssi = readSignalStrength();

  Serial.print(F("[NETWORK] Registered: "));
  Serial.println(networkRegistered ? F("YES") : F("NO"));
  Serial.print(F("[NETWORK] RSSI: "));
  Serial.println(reading.signalRssi);
  return true;
}

void readSensors() {
  if (!ahtOnline || !bmpOnline) {
    reading.valid = false;
    return;
  }

  sensors_event_t humidityEvent;
  sensors_event_t temperatureEvent;
  aht.getEvent(&humidityEvent, &temperatureEvent);

  reading.temperatureC = temperatureEvent.temperature;
  reading.humidityPercent = humidityEvent.relative_humidity;
  reading.bmpTemperatureC = bmp.readTemperature();
  reading.pressureHpa = bmp.readPressure() / 100.0F;
  reading.altitudeM = bmp.readAltitude(1013.25F);

  reading.valid =
    !isnan(reading.temperatureC) &&
    !isnan(reading.humidityPercent) &&
    !isnan(reading.pressureHpa) &&
    reading.temperatureC > -40.0F && reading.temperatureC < 85.0F &&
    reading.humidityPercent >= 0.0F && reading.humidityPercent <= 100.0F &&
    reading.pressureHpa > 300.0F && reading.pressureHpa < 1100.0F;

  Serial.println(F("\n========== WEATHER READING =========="));
  Serial.print(F("Station:        ")); Serial.println(STATION_ID);
  Serial.print(F("AHT20 Temp:     ")); Serial.print(reading.temperatureC, 2); Serial.println(F(" C"));
  Serial.print(F("Humidity:       ")); Serial.print(reading.humidityPercent, 2); Serial.println(F(" %"));
  Serial.print(F("BMP280 Temp:    ")); Serial.print(reading.bmpTemperatureC, 2); Serial.println(F(" C"));
  Serial.print(F("Pressure:       ")); Serial.print(reading.pressureHpa, 2); Serial.println(F(" hPa"));
  Serial.print(F("Est. Altitude:  ")); Serial.print(reading.altitudeM, 2); Serial.println(F(" m"));
  Serial.print(F("Reading valid:  ")); Serial.println(reading.valid ? F("YES") : F("NO"));
  Serial.println(F("====================================="));
}

bool configureGprs() {
  if (!modemOnline) return false;

  networkRegistered = isNetworkRegistered();
  if (!networkRegistered) {
    Serial.println(F("[GPRS] Network not registered"));
    return false;
  }

  char command[96];
  sendAT(F("AT+SAPBR=0,1"), "OK", 5000UL); // Ignore failure if already closed.
  if (!sendAT(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""))) return false;

  snprintf(command, sizeof(command), "AT+SAPBR=3,1,\"APN\",\"%s\"", APN);
  if (!sendATBuffer(command)) return false;

  if (strlen(APN_USER) > 0) {
    snprintf(command, sizeof(command), "AT+SAPBR=3,1,\"USER\",\"%s\"", APN_USER);
    if (!sendATBuffer(command)) return false;
  }
  if (strlen(APN_PASSWORD) > 0) {
    snprintf(command, sizeof(command), "AT+SAPBR=3,1,\"PWD\",\"%s\"", APN_PASSWORD);
    if (!sendATBuffer(command)) return false;
  }

  if (!sendAT(F("AT+SAPBR=1,1"), "OK", 30000UL)) return false;
  return sendAT(F("AT+SAPBR=2,1"), "+SAPBR:", 10000UL);
}

bool uploadReading() {
  if (!reading.valid) {
    Serial.println(F("[UPLOAD] Skipped: invalid sensor reading"));
    return false;
  }
  if (strstr(EDGE_FUNCTION_URL, "YOUR_PROJECT_REF") != NULL ||
      strstr(STATION_API_KEY, "REPLACE_WITH") != NULL) {
    Serial.println(F("[UPLOAD] Configure EDGE_FUNCTION_URL and STATION_API_KEY first"));
    return false;
  }

  showBootMessage(F("Uploading data"), F("MTN network..."));
  reading.signalRssi = readSignalStrength();

  if (!configureGprs()) {
    Serial.println(F("[UPLOAD] GPRS configuration failed"));
    return false;
  }

  char json[230];
  int jsonLength = snprintf(
    json, sizeof(json),
    "{\"station_id\":\"%s\",\"temperature_c\":%.2f,\"humidity_percent\":%.2f,"
    "\"pressure_hpa\":%.2f,\"bmp_temperature_c\":%.2f,\"altitude_m\":%.2f,"
    "\"signal_rssi\":%d}",
    STATION_ID,
    reading.temperatureC,
    reading.humidityPercent,
    reading.pressureHpa,
    reading.bmpTemperatureC,
    reading.altitudeM,
    reading.signalRssi
  );

  if (jsonLength <= 0 || jsonLength >= (int)sizeof(json)) {
    Serial.println(F("[UPLOAD] JSON buffer overflow prevented"));
    return false;
  }

  sendAT(F("AT+HTTPTERM"), "OK", 3000UL); // Safe cleanup from any previous session.
  if (!sendAT(F("AT+HTTPINIT"))) return false;
  if (!sendAT(F("AT+HTTPPARA=\"CID\",1"))) return false;

  char command[190];
  snprintf(command, sizeof(command), "AT+HTTPPARA=\"URL\",\"%s\"", EDGE_FUNCTION_URL);
  if (!sendATBuffer(command)) return false;

  if (!sendAT(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""))) return false;

  // The Supabase Edge Function checks this station-specific secret.
  snprintf(command, sizeof(command),
           "AT+HTTPPARA=\"USERDATA\",\"x-station-key: %s\"",
           STATION_API_KEY);
  if (!sendATSecretBuffer(command)) return false;

  // Supabase endpoints require HTTPS. This command needs SIM800 firmware with SSL support.
  if (!sendAT(F("AT+HTTPSSL=1"))) {
    Serial.println(F("[UPLOAD] SIM800 firmware does not support HTTP SSL"));
    sendAT(F("AT+HTTPTERM"));
    return false;
  }

  snprintf(command, sizeof(command), "AT+HTTPDATA=%d,15000", jsonLength);
  if (!sendATBuffer(command, "DOWNLOAD", 5000UL)) return false;
  sim800.print(json);
  if (!waitForModemText("OK", 20000UL)) return false;

  clearModemInput();
  sim800.println(F("AT+HTTPACTION=1"));
  lastHttpStatus = 0;
  char actionResponse[96];
  size_t actionIndex = 0;
  actionResponse[0] = '\0';
  unsigned long actionStarted = millis();
  while (millis() - actionStarted < 60000UL) {
    while (sim800.available()) {
      char c = (char)sim800.read();
      Serial.write(c);
      if (actionIndex < sizeof(actionResponse) - 1) {
        actionResponse[actionIndex++] = c;
        actionResponse[actionIndex] = '\0';
      }
    }
    char *action = strstr(actionResponse, "+HTTPACTION:");
    if (action != NULL) {
      int method = 0;
      int responseLength = 0;
      if (sscanf(action, "+HTTPACTION: %d,%d,%d", &method, &lastHttpStatus,
                 &responseLength) == 3) break;
    }
  }
  bool actionReceived = lastHttpStatus > 0;

  // Request the response body for diagnostics, then close cleanly.
  if (actionReceived) sendAT(F("AT+HTTPREAD"), "OK", 15000UL);
  sendAT(F("AT+HTTPTERM"));
  sendAT(F("AT+SAPBR=0,1"), "OK", 10000UL);

  bool accepted = lastHttpStatus == 200 || lastHttpStatus == 201;
  Serial.print(F("[UPLOAD] HTTP status: "));
  Serial.println(lastHttpStatus);
  return actionReceived && accepted;
}

void updateLcd() {
  char line1[17];
  char line2[17];

  if (!reading.valid) {
    printPadded("Sensor error");
    lcd.setCursor(0, 1);
    printPadded("Check wiring");
    return;
  }

  switch (lcdPage) {
    case 0:
      dtostrf(reading.temperatureC, 4, 1, line1);
      dtostrf(reading.humidityPercent, 4, 1, line2);
      {
        char row1[17];
        char row2[17];
        snprintf(row1, sizeof(row1), "Temp:%s C", line1);
        snprintf(row2, sizeof(row2), "Humidity:%s%%", line2);
        lcd.setCursor(0, 0); printPadded(row1);
        lcd.setCursor(0, 1); printPadded(row2);
      }
      break;

    case 1:
      dtostrf(reading.pressureHpa, 6, 1, line1);
      dtostrf(reading.altitudeM, 5, 0, line2);
      {
        char row1[17];
        char row2[17];
        snprintf(row1, sizeof(row1), "P:%shPa", line1);
        snprintf(row2, sizeof(row2), "Altitude:%sm", line2);
        lcd.setCursor(0, 0); printPadded(row1);
        lcd.setCursor(0, 1); printPadded(row2);
      }
      break;

    default:
      snprintf(line1, sizeof(line1), "MTN RSSI:%d", reading.signalRssi);
      lcd.setCursor(0, 0); printPadded(line1);
      lcd.setCursor(0, 1);
      if (!modemOnline) printPadded("Modem offline");
      else if (!networkRegistered) printPadded("No network");
      else if (lastUploadOk) printPadded("Cloud: synced");
      else printPadded("Cloud: pending");
      break;
  }
}

void setup() {
  Serial.begin(9600);
  sim800.begin(9600);
  Wire.begin();

  lcd.init();
  lcd.backlight();
  showBootMessage(F("Weather Station"), F("Starting..."));

  Serial.println(F("\n====================================="));
  Serial.println(F(" GSM WEATHER MONITORING STATION"));
  Serial.println(F(" Board: Arduino Uno"));
  Serial.println(F(" Network: MTN Nigeria / SIM800L"));
  Serial.println(F("====================================="));

  bool sensorsReady = initializeSensors();
  showBootMessage(sensorsReady ? F("Sensors online") : F("Sensor failure"),
                  F("Starting GSM"));
  delay(1200);

  modemOnline = initializeModem();
  showBootMessage(modemOnline ? F("GSM online") : F("GSM offline"),
                  F("System ready"));
  delay(1200);

  readSensors();
  updateLcd();
  unsigned long now = millis();
  lastSensorReadMs = now;
  lastLcdPageMs = now;
  // Delay initial cloud attempt slightly so the station becomes responsive first.
  lastUploadAttemptMs = now - UPLOAD_INTERVAL_MS + 15000UL;
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorReadMs >= SENSOR_INTERVAL_MS) {
    lastSensorReadMs = now;
    readSensors();
  }

  if (now - lastLcdPageMs >= LCD_PAGE_INTERVAL_MS) {
    lastLcdPageMs = now;
    lcdPage = (lcdPage + 1) % 3;
    updateLcd();
  }

  unsigned long requiredInterval = lastUploadOk ? UPLOAD_INTERVAL_MS : RETRY_INTERVAL_MS;
  if (now - lastUploadAttemptMs >= requiredInterval) {
    lastUploadAttemptMs = now;

    if (!modemOnline) modemOnline = initializeModem();
    lastUploadOk = modemOnline && uploadReading();

    if (lastUploadOk) {
      lastSuccessfulUploadMs = millis();
      Serial.println(F("[UPLOAD] Request completed; verify HTTP 200/201 above"));
    } else {
      Serial.println(F("[UPLOAD] Failed; retry scheduled in 60 seconds"));
    }
    updateLcd();
  }

  // Manual AT-command bridge: type commands in Serial Monitor if needed.
  if (Serial.available()) sim800.write(Serial.read());
  if (sim800.available()) Serial.write(sim800.read());
}
