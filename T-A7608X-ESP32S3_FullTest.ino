/*
 * LilyGO T-A7608X-ESP32S3 -- Vollständiger Hardware-Testsketch
 * ------------------------------------------------------
 * Aufdruck auf der Platine: "A7608-ESP32S3  2023-08-24  V1.0"
 * MCU:    Espressif ESP32-S3-WROOM-1
 * Modem:  SIMCom A7608E-H (LTE Cat-1 + eingebautes GNSS)
 *
 * Dieser Sketch testet in einem Durchlauf alle wichtigen Peripheriekomponenten
 * des Boards:
 *   1. Einschaltsequenz des Modems + AT-Kommunikation
 *   2. Erkennung der SIM-Karte (IMEI / IMSI)
 *   3. Netzanmeldung + Signalqualität
 *   4. LTE-Datenverbindung + HTTP-GET
 *   5. GNSS-Fix über die eingebaute GNSS-Engine des Modems (benötigt die GNSS-Antenne)
 *   6. Reverse-Geocoding: aus den GPS-Koordinaten per Nominatim/OpenStreetMap (HTTPS)
 *      eine Adresse ermitteln
 *   7. Batteriespannung über den ADC
 *   8. Lesen/Schreiben auf der microSD-Karte (TF-Kartenslot)
 *
 * Voraussetzungen:
 *   - Arduino-Core für ESP32 (esp32 by Espressif Systems), Board:
 *       "ESP32S3 Dev Module" (bzw. Familie "UM ESP32-S3"), USB CDC on boot: aktiviert
 *   - TinyGSM-Bibliothek, LilyGO-Fork mit A7608-Unterstützung:
 *       https://github.com/lewisxhe/TinyGSM-fork
 *     (das offizielle TinyGSM (vshymanskyy/TinyGSM) unterstützt A7608 NICHT
 *      -- diese Bibliothek muss zuerst deinstalliert/entfernt werden, sonst
 *      bricht die Kompilierung mit "#error Please define GSM modem model" ab.
 *      Danach den obigen Fork als ZIP-Bibliothek installieren.)
 *   - Eine eingelegte SIM-Karte, die LTE-Antenne am Hauptantennenanschluss
 *     des Modems, sowie die GNSS-Antenne am u.FL-Anschluss "GNSS".
 *
 * Die Pin-Belegung stammt aus LilyGOs offiziellem Repository
 * Xinyuan-LilyGO/LilyGo-Modem-Series (utilities.h,
 * Board-Definition LILYGO_T_A7608X_S3).
 *
 * Arduino-IDE Board-Einstellungen (Werkzeuge-Menü):
 *   Board:                    "ESP32S3 Dev Module"
 *   Modul:                    ESP32-S3-WROOM-1 (N16R8) -> 16 MB Flash, 8 MB PSRAM
 *   USB CDC On Boot:          "Enabled"   (nötig für Serial-Ausgabe über den USB-C-Port)
 *   CPU Frequency:            "240MHz (WiFi)"
 *   Flash Mode:                "QIO 80MHz"
 *   Flash Size:               "16MB (128Mb)"
 *   PSRAM:                    "OPI PSRAM"
 *   Partition Scheme:         "16M Flash (3MB APP/9.9MB FATFS)"
 *   Upload Mode:              "UART0 / Hardware CDC"
 *   Upload Speed:             "921600"
 *   USB Mode:                 "Hardware CDC and JTAG"
 *   Core Debug Level:         "None"
 *
 * Hinweis: Wird das Board über eine externe 5V-Versorgung statt über
 * USB-C betrieben, muss "USB CDC On Boot" auf "Disabled" gesetzt werden,
 * da das Board sonst beim Start auf eine USB-Verbindung wartet.
 */

#define TINY_GSM_MODEM_A7608
#define TINY_GSM_RX_BUFFER 1024

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>

// ---------------- Pin-Belegung: T-A7608X-ESP32S3 V1.0 ----------------
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

// ---------------- APN: bitte an deinen SIM-Anbieter anpassen! ----------------
const char apn[]      = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem);

// Vom GNSS-Test befuellt, vom Reverse-Geocoding-Test genutzt
bool  gpsFixValid = false;
float gpsLat = 0, gpsLon = 0;

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
// Kleine Helfer fuer direkte, rohe AT-Kommunikation ueber SerialAT.
// Wird fuer den nativen HTTPS-Client des Modems benutzt (AT+HTTP...),
// da diese Befehle unabhaengig von der TinyGSM-Socket-Abstraktion sind
// und auf jeder A76xx-Firmware funktionieren.
void atSend(const String &cmd) {
  while (SerialAT.available()) SerialAT.read(); // Eingangspuffer leeren
  SerialAT.print(F("AT"));
  SerialAT.println(cmd);
}

String atRead(unsigned long timeoutMs, const char *token = "OK") {
  String resp;
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (SerialAT.available()) {
      resp += (char)SerialAT.read();
      t0 = millis();
    }
    if (resp.indexOf(token) >= 0) break;
  }
  return resp;
}

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

  digitalWrite(MODEM_DTR_PIN, LOW); // LOW = Modem bleibt wach, kein Sleep

  // Hardware-Reset-Impuls
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  delay(2600);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

  // PWRKEY-Impuls, um das Modem zu starten
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

  Serial.println(F("Requesting http://api.ipify.org/ ..."));
  if (client.connect("api.ipify.org", 80)) {
    client.println(F("GET / HTTP/1.1"));
    client.println(F("Host: api.ipify.org"));
    client.println(F("Connection: close"));
    client.println();

    String statusLine = client.readStringUntil('\n');
    Serial.print(F("HTTP-Status: ")); Serial.println(statusLine);

    // Restliche Header ueberspringen bis zur Leerzeile
    String line;
    do {
      line = client.readStringUntil('\n');
    } while (line.length() > 1 && client.connected());

    // Body (die oeffentliche IP-Adresse) auslesen
    String body;
    unsigned long t0 = millis();
    while (client.connected() && millis() - t0 < 10000L) {
      while (client.available()) {
        body += (char)client.read();
        t0 = millis();
      }
    }
    client.stop();

    if (statusLine.indexOf("200") > 0) {
      Serial.print(F("[OK] Oeffentliche IP laut api.ipify.org: ")); Serial.println(body);
    } else {
      Serial.print(F("[WARN] Unerwarteter HTTP-Status. Antwort: ")); Serial.println(body);
    }
    Serial.println(F("[OK] HTTP request finished."));
  } else {
    Serial.println(F("[FAIL] Could not connect to api.ipify.org."));
  }
  modem.gprsDisconnect();
}

void testGNSS() {
  printHeader("5) GNSS / GPS TEST");
  Serial.println(F("Enabling built-in GNSS engine..."));
  int retry = 0;
  while (!modem.enableGPS()) {
    Serial.print('.');
    if (retry++ > 15) {
      Serial.println(F("\n[WARN] Could not enable GNSS (check firmware / module variant)."));
      return;
    }
    delay(1000);
  }
  Serial.println();
  Serial.println(F("[OK] GNSS enabled."));
  modem.setGPSBaud(115200);

  Serial.println(F("Waiting for a fix (up to 5 min, needs open sky view -- window/roof usually NOT enough)..."));
  Serial.println(F("Raw AT+CGNSSINFO output every 5s (fixMode,GPS-sat,GLONASS-sat,BEIDOU-sat,lat,N/S,lon,E/W,date,time,...):"));
  uint8_t fixMode = 0;
  float   lat = 0, lon = 0, speed = 0, alt = 0, accuracy = 0;
  int     vsat = 0, usat = 0, year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  unsigned long t0 = millis();
  unsigned long lastPrint = 0;
  bool fixed = false;
  while (millis() - t0 < 300000L) {
    if (modem.getGPS(&fixMode, &lat, &lon, &speed, &alt, &vsat, &usat, &accuracy,
                      &year, &month, &day, &hour, &minute, &second)) {
      fixed = true;
      break;
    }
    // Rohdaten alle 5s zur Diagnose ausgeben (zeigt, ob ueberhaupt Satelliten sichtbar sind)
    if (millis() - lastPrint > 5000) {
      lastPrint = millis();
      Serial.print(F("  [")); Serial.print((millis() - t0) / 1000); Serial.print(F("s] "));
      Serial.println(modem.getGPSraw());
    }
    delay(200);
  }
  Serial.println();
  if (fixed) {
    gpsFixValid = true;
    gpsLat = lat;
    gpsLon = lon;
    Serial.print(F("[OK] Fix acquired -> Lat: ")); Serial.print(lat, 6);
    Serial.print(F("  Lon: ")); Serial.println(lon, 6);
    Serial.print(F("Satelliten sichtbar: ")); Serial.print(vsat);
    Serial.print(F("  Genauigkeit: ")); Serial.println(accuracy);
    Serial.printf("UTC-Zeit: %04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);
  } else {
    Serial.println(F("[WARN] No fix within timeout."));
    Serial.println(F("  -> Wenn die Rohdaten oben komplett leer waren (nur Kommas), sieht das Modul"));
    Serial.println(F("     GAR KEINE Satelliten: bitte mit freier Sicht zum Himmel im Freien testen"));
    Serial.println(F("     (Fenster/Dach reichen meist nicht) und ein paar Minuten laenger warten."));
    Serial.println(F("  -> Wenn Satellitenzahlen > 0 auftauchten, aber kein Fix: einfach laenger warten,"));
    Serial.println(F("     ein Kaltstart kann bis zu ~12 Minuten dauern."));
  }
  modem.disableGPS();
}

void testReverseGeocode() {
  printHeader("6) REVERSE GEOCODING (Adresse zu GPS-Koordinaten)");
  if (!gpsFixValid) {
    Serial.println(F("[SKIP] Kein gueltiger GPS-Fix aus Test 5 vorhanden, ueberspringe."));
    return;
  }

  Serial.print(F("Verbinde erneut mit APN '")); Serial.print(apn); Serial.println(F("'..."));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(F("[FAIL] Datenverbindung fehlgeschlagen."));
    return;
  }

  char url[200];
  snprintf(url, sizeof(url),
           "https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=%.6f&lon=%.6f&zoom=18&addressdetails=1",
           gpsLat, gpsLon);

  Serial.print(F("Frage Adresse ab fuer (")); Serial.print(gpsLat, 6);
  Serial.print(F(", ")); Serial.print(gpsLon, 6); Serial.println(F(")..."));

  // Der native HTTPS-Client des Modems wird per AT-Befehlen angesteuert
  // (SIMCom-Standardbefehlssatz, unabhaengig von TinyGSM-Socket-Klassen):
  //   AT+HTTPINIT -> AT+HTTPPARA -> AT+HTTPACTION -> AT+HTTPREAD -> AT+HTTPTERM
  atSend("+HTTPTERM"); atRead(2000);  // evtl. offene Vorgaenger-Session schliessen

  atSend("+HTTPINIT");
  if (atRead(5000).indexOf("OK") < 0) {
    Serial.println(F("[FAIL] AT+HTTPINIT fehlgeschlagen."));
    modem.gprsDisconnect();
    return;
  }

  // SSL-Kontext 0 konfigurieren: TLS aktivieren, KEINE Serverzertifikatspruefung
  // (kein CA-Upload noetig) und -- der Kernpunkt -- SNI aktivieren. Ohne SNI
  // schickt das Modem beim TLS-Handshake keinen Hostnamen mit, viele Server
  // (u.a. Nominatim) antworten dann mit "421 Misdirected Request".
  atSend("+CSSLCFG=\"sslversion\",0,4"); // 4 = alle TLS-Versionen erlauben
  atRead(2000);
  atSend("+CSSLCFG=\"authmode\",0,0");   // 0 = keine Serverzertifikatspruefung
  atRead(2000);
  atSend("+CSSLCFG=\"enableSNI\",0,1");  // SNI aktivieren -- behebt den 421-Fehler
  atRead(2000);
  atSend("+HTTPPARA=\"SSLCFG\",0");      // HTTP-Client an SSL-Kontext 0 binden
  atRead(2000);

  atSend("+HTTPPARA=\"CID\",1");
  atRead(2000);

  atSend(String("+HTTPPARA=\"URL\",\"") + url + "\"");
  atRead(2000);

  // Nominatim-Nutzungsrichtlinie verlangt einen aussagekraeftigen User-Agent
  atSend("+HTTPPARA=\"USERDATA\",\"User-Agent: T-A7608X-ESP32S3-HardwareTest/1.0\\r\\n\"");
  atRead(2000); // nicht kritisch, falls diese Firmware USERDATA nicht kennt

  Serial.println(F("Sende HTTPS GET (kann einige Sekunden dauern)..."));
  atSend("+HTTPACTION=0");
  atRead(2000); // bestaetigt nur die Entgegennahme des Befehls (sofortiges "OK")

  // Die eigentliche Antwort kommt asynchron als "+HTTPACTION: 0,<status>,<len>"
  String action = atRead(30000, "+HTTPACTION:");
  int statusCode = 0, dataLen = 0;
  int idxTag = action.indexOf("+HTTPACTION:");
  if (idxTag >= 0) {
    String tail = action.substring(idxTag);
    int c1 = tail.indexOf(',');
    int c2 = tail.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      statusCode = tail.substring(c1 + 1, c2).toInt();
      int c3 = tail.indexOf('\r', c2);
      dataLen    = tail.substring(c2 + 1, c3 > 0 ? c3 : tail.length()).toInt();
    }
  }

  if (statusCode == 200 && dataLen > 0) {
    Serial.print(F("[OK] HTTP-Status 200, ")); Serial.print(dataLen); Serial.println(F(" Bytes. Lese Inhalt..."));
    atSend(String("+HTTPREAD=0,") + dataLen);
    String body = atRead(15000, "OK");

    int idx = body.indexOf("\"display_name\":\"");
    if (idx >= 0) {
      int start = idx + strlen("\"display_name\":\"");
      int end   = body.indexOf('"', start);
      Serial.print(F("[OK] Adresse: ")); Serial.println(body.substring(start, end));
    } else {
      Serial.println(F("[WARN] Konnte 'display_name' nicht in der Antwort finden."));
      Serial.print(F("Antwort (gekuerzt, erste 300 Zeichen): "));
      Serial.println(body.substring(0, 300));
    }
  } else {
    Serial.print(F("[WARN] Unerwarteter HTTP-Status: ")); Serial.println(statusCode);
  }

  atSend("+HTTPTERM"); atRead(2000);
  modem.gprsDisconnect();
}

void testBattery() {
  printHeader("7) BATTERY / ADC TEST");
  analogReadResolution(12);
  int raw = analogRead(BOARD_BAT_ADC_PIN);
  // Dieses Board hat einen 1:2-Spannungsteiler vor dem ADC-Pin
  float voltage = (raw / 4095.0f) * 3.3f * 2.0f;
  Serial.print(F("Raw ADC: "));    Serial.println(raw);
  Serial.print(F("Battery ~= ")); Serial.print(voltage, 2); Serial.println(F(" V"));
}

void testSDCard() {
  printHeader("8) microSD CARD TEST");
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
    testReverseGeocode();
  }

  testBattery();
  testSDCard();

  printHeader("ALL TESTS FINISHED");
}

void loop() {
  // Alle Tests laufen einmalig in setup(); hier gibt es nichts zu tun.
}
