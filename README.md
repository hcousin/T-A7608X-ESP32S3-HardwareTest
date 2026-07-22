# T-A7608X-ESP32S3 Full Hardware Test

Arduino-Testsketch für das LilyGO-Board **T-A7608X-ESP32S3**
(Aufdruck: `A7608-ESP32S3 2023-08-24 V1.0`) mit einem SIMCom-**A7608E-H**-
LTE-Cat-1-Modem (mit eingebautem GNSS) und einem ESP32-S3-WROOM-1-MCU
(N16R8: 16 MB Flash, 8 MB PSRAM).

Referenzen:
- Board/Pinbelegung: [LilyGo-Modem-Series](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series) (`utilities.h`, `LILYGO_T_A7608X_S3`)
- GSM-Bibliothek: [TinyGSM-fork (lewisxhe)](https://github.com/lewisxhe/TinyGSM-fork)
- Modem-Befehlssatz: SIMCom A76XX-Serie AT-Command-Manual (SIMCom-Website)
- Reverse-Geocoding-Dienst: [Nominatim](https://nominatim.openstreetmap.org) (OpenStreetMap)

## Was der Sketch testet

| # | Test | Kurzbeschreibung |
|---|------|-------------------|
| 1 | Modem / AT | Power-on-Sequenz, wartet auf AT-Antwort, liest Modem-Info |
| 2 | SIM-Karte | SIM-Status, IMEI, IMSI |
| 3 | Netzanmeldung | `waitForNetwork()` (Standard-Timeout 60 s, im Sketch anpassbar), Operator, Signalqualität |
| 4 | LTE-Daten + HTTP | APN-Verbindung, HTTP-GET gegen `api.ipify.org` (liefert die öffentliche IP) |
| 5 | GNSS | Eingebaute GNSS-Engine des Modems, Fix-Timeout 5 min, Live-Rohdaten (`AT+CGNSSINFO`) alle 5 s |
| 6 | Reverse-Geocoding | Adresse zu den GPS-Koordinaten aus Test 5, per Nominatim/OpenStreetMap über HTTPS |
| 7 | Batterie | Spannung über den ADC (1:2-Spannungsteiler auf dem Board) |
| 8 | microSD | Lese-/Schreibtest auf der TF-Karte |

## Hardware-Vorbereitung

- SIM-Karte in den SIM-Slot einlegen.
- Die **LTE-Antenne** (Full Band LTE, 698–960/1710–2690 MHz) am Hauptantennenanschluss des Modems anschließen.
- Die **GNSS-Antenne** am mit `GNSS` beschrifteten u.FL-Anschluss anschließen.
- Optional eine microSD-Karte (FAT32) einlegen — sonst meldet Test 8 erwartungsgemäß "No SD card detected".

## Einrichtung in der Arduino IDE

1. Boardpaket **esp32 by Espressif Systems** installieren (Board Manager URL:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
2. **TinyGSM-fork** (nicht das offizielle `vshymanskyy/TinyGSM`!) als ZIP installieren:
   https://github.com/lewisxhe/TinyGSM-fork → Code → Download ZIP → in Arduino IDE
   **Sketch → Include Library → Add .ZIP Library…**. Ist bereits die Standardbibliothek
   installiert, vorher den Ordner `libraries/TinyGSM` löschen (siehe Troubleshooting).
3. Board-Parameter gemäß Tabelle unten setzen.
4. `T-A7608X-ESP32S3_FullTest.ino` öffnen, Konstante `apn` an deinen SIM-Anbieter
   anpassen, über den USB-C-Port hochladen.
5. Seriellen Monitor mit 115200 Baud öffnen.

### Board-Parameter (Werkzeuge-Menü)

Für dieses Modul gibt es keine eigene LilyGO-Board-Definition — als Board wird
das generische **„ESP32S3 Dev Module“** verwendet, mit folgenden Werten:

| Parameter         | Wert                                |
|-------------------|--------------------------------------|
| Board             | `ESP32S3 Dev Module`                 |
| USB CDC On Boot   | `Enabled` bei USB-Betrieb, `Disabled` bei externer 5V-Versorgung (siehe Troubleshooting) |
| CPU Frequency     | `240MHz (WiFi)`                      |
| Flash Mode        | `QIO 80MHz`                          |
| Flash Size        | `16MB (128Mb)`                       |
| PSRAM             | `OPI PSRAM`                          |
| Partition Scheme  | `16M Flash (3MB APP/9.9MB FATFS)`    |
| Upload Mode       | `UART0 / Hardware CDC`               |
| Upload Speed      | `921600`                             |
| USB Mode          | `Hardware CDC and JTAG`              |
| Core Debug Level  | `None`                               |

## Pin-Belegung

| Signal            | GPIO |
|-------------------|------|
| MODEM_TX_PIN      | 17   |
| MODEM_RX_PIN      | 18   |
| MODEM_DTR_PIN     | 7    |
| MODEM_RESET_PIN   | 16   |
| MODEM_RING_PIN    | 6    |
| BOARD_PWRKEY_PIN  | 15   |
| BOARD_BAT_ADC_PIN | 4    |
| BOARD_SCK_PIN     | 21   |
| BOARD_MOSI_PIN    | 14   |
| BOARD_MISO_PIN    | 47   |
| BOARD_SD_CS_PIN   | 13   |

## Troubleshooting

**`#error "Please define GSM modem model"` beim Kompilieren**
Es ist die offizielle `vshymanskyy/TinyGSM`-Bibliothek installiert, die A7608
nicht kennt. Ordner `libraries/TinyGSM` löschen und stattdessen den
[TinyGSM-fork](https://github.com/lewisxhe/TinyGSM-fork) installieren (siehe oben).

**`no matching function for call to 'getGPS(float*, float*)'`**
Der TinyGSM-fork verwendet für A76xx-Module die erweiterte `getGPS()`-Signatur mit
14 Parametern (`enableGPS()` / `getGPS(&fixMode, &lat, &lon, &speed, &alt, &vsat,
&usat, &accuracy, &year, &month, &day, &hour, &minute, &second)` / `disableGPS()`).
Der Sketch nutzt bereits diese Variante.

**GNSS-Test: dauerhaft nur Kommas (`,,,,,,,,`) in den Live-Rohdaten**
Kein Satellitenempfang — meist fehlender Himmelsblick. Fenster oder Dach reichen
in der Regel nicht; im Freien mit offener Sicht testen. Erste Zahlen bei
Satellitenanzahl, aber noch kein Fix → einfach weiter warten, ein Kaltstart
(kein gespeicherter Almanach) kann bis zu ca. 10–12 Minuten dauern.

**Reverse-Geocoding: `421 Misdirected Request`**
Ohne aktiviertes SNI (Server Name Indication) weiß der Server beim TLS-Handshake
nicht, welches Zertifikat er ausliefern soll — betrifft u. a. Nominatim, das
hinter einem Reverse-Proxy mit mehreren virtuellen Hosts läuft. Behoben durch
`AT+CSSLCFG="enableSNI",0,1` vor dem Request (im Sketch bereits enthalten).

**Reverse-Geocoding: Antwort bricht mitten im JSON ab / `display_name` nicht gefunden**
`AT+HTTPREAD` sendet zuerst ein sofortiges `OK` als reine Befehlsbestätigung,
die eigentlichen Daten folgen asynchron danach. Die AT-Lesefunktion im Sketch
wartet deshalb nach dem ersten Fund eines Tokens zusätzlich auf eine kurze
Ruhephase (150 ms ohne neue Bytes), bevor sie abbricht — und der UART-Empfangs-
puffer ist auf 2048 Byte vergrößert (ESP32-Standard: 256 Byte).

**„Multiple libraries were found for SD.h“**
Nur eine Warnung, kein Fehler. Kommt von einer zusätzlichen SD-Bibliothek unter
`Arduino15/libraries/SD`, die mit der im ESP32-Boardpaket enthaltenen kollidiert.
Kann bei Bedarf gelöscht werden, damit automatisch die Core-Version verwendet wird.

**Kein serielles Signal über USB-C**
`USB CDC On Boot` muss auf `Enabled` stehen, wenn das Board über USB-C betrieben
und der Serielle Monitor genutzt wird. Bei externer 5V-Versorgung ohne USB-Host
sollte die Option auf `Disabled` stehen, sonst wartet das Board beim Start auf
eine USB-Verbindung.

## Sonstige Hinweise

- Nominatims Nutzungsrichtlinie erlaubt max. 1 Anfrage pro Sekunde — für
  gelegentliche Einzeltests im Sketch unkritisch. Bei häufigeren Wiederholungen
  bitte einen eigenen Nominatim-Server oder einen kommerziellen Geocoding-Dienst
  verwenden.
- Reverse-Geocoding läuft nur, wenn Test 5 zuvor einen GPS-Fix liefern konnte.
