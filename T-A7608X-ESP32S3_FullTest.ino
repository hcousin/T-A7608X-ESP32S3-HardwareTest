/*
 * LilyGO T-A7608X-ESP32S3 -- Full Hardware Test Sketch
 * ------------------------------------------------------
 * Board silkscreen: "A7608-ESP32S3  2023-08-24  V1.0"
 * MCU:    Espressif ESP32-S3-WROOM-1
 * Modem:  SIMCom A7608E-H (LTE Cat-1 + built-in GNSS)
 *
 * This sketch exercises every major peripheral on the board in one run:
 *   1. Modem power-on sequence + AT communication
 *   2. SIM card detection (IMEI / IMSI)
 *   3. Network registration + signal quality
 *   4. LTE data connection + HTTP GET
 *   5. GNSS fix via the modem's built-in GNSS engine (needs the GNSS antenna)
 *   6. Battery voltage via ADC
 *   7. microSD card read/write (TF card slot)
 *
 * Requirements:
 *   - Arduino core for ESP32 (esp32 by Espressif Systems), board:
 *       "ESP32S3 Dev Module" (or "UM ESP32-S3" family), USB CDC on boot: enabled
 *   - TinyGSM library, LilyGO fork with A7608 support:
 *       https://github.com/lewisxhe/TinyGSM
 *     (the mainline TinyGSM master branch does NOT support A7608 well --
 *      use the fork above, install as a ZIP library)
 *   - A SIM card inserted, the LTE antenna connected to the modem's main
 *     antenna connector, and the GNSS antenna connected to the "GNSS" u.FL
 *     connector.
 *
 * Pin mapping is taken from LilyGO's official
 * Xinyuan-LilyGO/LilyGo-Modem-Series repository (utilities.h,
 * LILYGO_T_A7608X_S3 board definition).
 */

#define TINY_GSM_MODEM_A7608
#define TINY_GSM_RX_BUFFER 1024

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// ---------------- Pin mapping: T-A7608X-ESP32S3 V1.0 ----------------
#define MODEM_BAUDRATE      115200
#define MODEM_DTR_PIN       7
#define MODEM_TX_PIN        17
#define MODEM_RX_PIN        18
#define BOARD_PWRKEY_PIN    15
#define BOARD_BAT_ADC_PIN   4
#define MODEM_RING_PIN      6
#define MODEM_RESET_PIN     16
#define MODEM_RESET_LEVEL   LOW

#define BOARD_MISO_PIN      47
#define BOARD_MOSI_PIN      14
#define BOARD_SCK_PIN       21
#define BOARD_SD_CS_PIN     13

#define SerialAT Serial1

// ---------------- APN: adjust to your SIM provider! ----------------
const char apn[]      = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);

// ---------------------------------------------------------------------
void printHeader(const char *title) {
  Serial.println();
  Serial.println(F("=================================================="));
  Serial.print(F("  "));
  Serial.println(title);
  Serial.println(F("=================================================="));
}

void modemPowerOn() {
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  pinMode(MODEM_RESET_PIN, OUTPUT);
  pinMode(MODEM_DTR_PIN, OUTPUT);

  digitalWrite(MODEM_DTR_PIN, LOW); // LOW = keep modem awake, no sleep

  // Hardware reset pulse
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  delay(2600);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

  // PWRKEY pulse to boot the modem
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(200);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
}

bool testModemAT() {
  printHeader("1) MODEM / AT COMMUNICATION TEST");
  Serial.println(F("Waiting for modem to respond to AT..."));
  int retry = 0;
  while (!modem.testAT(1000)) {
    Serial.print('.');
    if (retry++ > 20) {
      Serial.println(F("\n[FAIL] Modem did not respond. Check wiring/power."));
      return false;
    }
  }
  Serial.println();
  Serial.println(F("[OK] Modem responds to AT."));
  Serial.print(F("Modem info: "));
  Serial.println(modem.getModemInfo());
  return true;
}

void testSimCard() {
  printHeader("2) SIM CARD TEST");
  SimStatus sim = modem.getSimStatus();
  switch (sim) {
    case SIM_READY:
      Serial.println(F("[OK] SIM card ready."));
      break;
    case SIM_LOCKED:
      Serial.println(F("[WARN] SIM card locked (PIN required)."));
      break;
    default:
      Serial.println(F("[FAIL] No SIM card detected / error."));
      break;
  }
  Serial.print(F("IMEI: ")); Serial.println(modem.getIMEI());
  Serial.print(F("IMSI: ")); Serial.println(modem.getIMSI());
}

bool testNetwork() {
  printHeader("3) NETWORK REGISTRATION TEST");
  Serial.println(F("Waiting for network registration (up to 60s)..."));
  if (!modem.waitForNetwork(60000L)) {
    Serial.println(F("[FAIL] Network registration failed."));
    return false;
  }
  Serial.println(F("[OK] Registered on network."));
  Serial.print(F("Operator: "));       Serial.println(modem.getOperator());
  Serial.print(F("Signal (0-31): "));  Serial.println(modem.getSignalQuality());
  return true;
}

void testGprsAndHttp() {
  printHeader("4) LTE DATA + HTTP GET TEST");
  Serial.print(F("Connecting to APN '")); Serial.print(apn); Serial.println(F("'..."));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(F("[FAIL] Data connect failed."));
    return;
  }
  Serial.println(F("[OK] Data connection established."));
  Serial.print(F("Local IP: ")); Serial.println(modem.getLocalIP());

  Serial.println(F("Requesting http://httpbin.org/ip ..."));
  if (client.connect("httpbin.org", 80)) {
    client.println(F("GET /ip HTTP/1.1"));
    client.println(F("Host: httpbin.org"));
    client.println(F("Connection: close"));
    client.println();

    unsigned long t0 = millis();
    while (client.connected() && millis() - t0 < 10000L) {
      while (client.available()) {
        Serial.write(client.read());
        t0 = millis();
      }
    }
    client.stop();
    Serial.println(F("\n[OK] HTTP request finished."));
  } else {
    Serial.println(F("[FAIL] Could not connect to httpbin.org."));
  }
  modem.gprsDisconnect();
}

void testGNSS() {
  printHeader("5) GNSS / GPS TEST");
  Serial.println(F("Powering on built-in GNSS engine..."));
  modem.sendAT("+CGNSSPWR=1");
  modem.waitResponse(10000L);

  Serial.println(F("Waiting for a fix (up to 120s, needs open sky view)..."));
  float lat = 0, lon = 0;
  unsigned long t0 = millis();
  bool fixed = false;
  while (millis() - t0 < 120000L) {
    if (modem.getGPS(&lat, &lon)) {
      fixed = true;
      break;
    }
    Serial.print('.');
    delay(2000);
  }
  Serial.println();
  if (fixed) {
    Serial.print(F("[OK] Fix acquired -> Lat: ")); Serial.print(lat, 6);
    Serial.print(F("  Lon: ")); Serial.println(lon, 6);
  } else {
    Serial.println(F("[WARN] No fix within timeout (normal indoors/without sky view)."));
  }
  modem.sendAT("+CGNSSPWR=0");
  modem.waitResponse(5000L);
}

void testBattery() {
  printHeader("6) BATTERY / ADC TEST");
  analogReadResolution(12);
  int raw = analogRead(BOARD_BAT_ADC_PIN);
  // This board has a 1:2 voltage divider in front of the ADC pin
  float voltage = (raw / 4095.0f) * 3.3f * 2.0f;
  Serial.print(F("Raw ADC: "));    Serial.println(raw);
  Serial.print(F("Battery ~= ")); Serial.print(voltage, 2); Serial.println(F(" V"));
}

void testSDCard() {
  printHeader("7) microSD CARD TEST");
  SPIClass sdSPI(HSPI);
  sdSPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN, BOARD_SD_CS_PIN);
  if (!SD.begin(BOARD_SD_CS_PIN, sdSPI)) {
    Serial.println(F("[WARN] No SD card detected (or init failed)."));
    return;
  }
  uint64_t cardSizeMB = SD.cardSize() / (1024 * 1024);
  Serial.print(F("[OK] Card size: ")); Serial.print((uint32_t)cardSizeMB); Serial.println(F(" MB"));

  File f = SD.open("/test.txt", FILE_WRITE);
  if (f) {
    f.println(F("Hello from T-A7608X-ESP32S3 test sketch!"));
    f.close();
    Serial.println(F("[OK] Write test passed."));
  } else {
    Serial.println(F("[FAIL] Could not open file for writing."));
    return;
  }

  f = SD.open("/test.txt");
  if (f) {
    Serial.print(F("File content: "));
    while (f.available()) Serial.write(f.read());
    f.close();
    Serial.println(F("[OK] Read test passed."));
  } else {
    Serial.println(F("[FAIL] Could not open file for reading."));
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("\n\n=== LilyGO T-A7608X-ESP32S3 Full Hardware Test ==="));

  modemPowerOn();
  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(3000);

  if (testModemAT()) {
    testSimCard();
    if (testNetwork()) {
      testGprsAndHttp();
    }
    testGNSS();
  }

  testBattery();
  testSDCard();

  printHeader("ALL TESTS FINISHED");
}

void loop() {
  // All tests run once in setup(); nothing to do here.
}
