# T-A7608X-ESP32S3 Full Hardware Test

Arduino-Testsketch für das LilyGO-Board **T-A7608X-ESP32S3**
(Aufdruck: `A7608-ESP32S3 2023-08-24 V1.0`) mit einem SIMCom-**A7608E-H**-
LTE-Cat-1-Modem (mit eingebautem GNSS) und einem ESP32-S3-WROOM-1-MCU.

Der Sketch testet der Reihe nach:

1. Einschaltsequenz des Modems + AT-Kommunikation
2. Erkennung der SIM-Karte (IMEI / IMSI)
3. Netzanmeldung + Signalqualität
4. LTE-Datenverbindung + HTTP-GET (`httpbin.org/ip`)
5. GNSS-Fix über die eingebaute GNSS-Engine des Modems
6. Batteriespannung über den ADC
7. Lesen/Schreiben auf der microSD-Karte (TF-Kartenslot)

## Hardware-Vorbereitung

- SIM-Karte in den SIM-Slot einlegen.
- Die **LTE-Antenne** (Full Band LTE, 698–960/1710–2690 MHz) am
  Hauptantennenanschluss des Modems anschließen.
- Die **GNSS-Antenne** am mit `GNSS` beschrifteten u.FL-Anschluss anschließen.
- Optional eine microSD-Karte einlegen.

## Einrichtung in der Arduino IDE

1. Das Boardpaket **esp32 by Espressif Systems** installieren (Board Manager URL:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
2. Die **TinyGSM**-Bibliothek installieren — dabei den LilyGO-Fork mit
   A7608-Unterstützung verwenden, **nicht** das offizielle TinyGSM:
   https://github.com/lewisxhe/TinyGSM-fork
   (Sketch → Include Library → Add .ZIP Library…). Falls bereits die
   Standardbibliothek `vshymanskyy/TinyGSM` installiert ist, muss sie zuerst
   entfernt werden (Ordner `libraries/TinyGSM` löschen) — sonst schlägt die
   Kompilierung mit `#error "Please define GSM modem model"` fehl, da diese
   Bibliothek A7608 nicht kennt.
3. Unter **Werkzeuge** (Tools) die folgenden Board-Parameter setzen (siehe
   Tabelle unten).
4. `T-A7608X-ESP32S3_FullTest.ino` öffnen, die Konstante `apn` an deinen
   SIM-Anbieter anpassen und über den USB-C-Port des Boards hochladen.
5. Den Seriellen Monitor mit 115200 Baud öffnen, um die Testergebnisse
   zu sehen.

### Arduino-IDE Board-Einstellungen (Werkzeuge-Menü)

Das Board verwendet ein ESP32-S3-WROOM-1 (N16R8): 16 MB Flash, 8 MB PSRAM
(Octal SPI). Es gibt keine eigene LilyGO-Board-Definition für dieses Modul —
als Board wird das generische **„ESP32S3 Dev Module“** verwendet und die
folgenden Parameter manuell gesetzt:

| Parameter               | Wert                                      |
|--------------------------|-------------------------------------------|
| Board                    | `ESP32S3 Dev Module`                      |
| USB CDC On Boot          | `Enabled` *(siehe Hinweis unten)*         |
| CPU Frequency            | `240MHz (WiFi)`                           |
| Flash Mode               | `QIO 80MHz`                               |
| Flash Size               | `16MB (128Mb)`                            |
| PSRAM                    | `OPI PSRAM`                               |
| Partition Scheme         | `16M Flash (3MB APP/9.9MB FATFS)`         |
| Upload Mode              | `UART0 / Hardware CDC`                    |
| Upload Speed             | `921600`                                  |
| USB Mode                 | `Hardware CDC and JTAG`                   |
| Core Debug Level         | `None`                                    |

**Hinweis zu USB CDC On Boot:** Wird das Board über USB-C betrieben und
programmiert, muss diese Option auf `Enabled` stehen, damit `Serial`
(Ausgaben, Serieller Monitor) über den USB-C-Port funktioniert. Wird das
Board stattdessen über eine externe 5V-Stromversorgung betrieben, sollte
die Option auf `Disabled` gesetzt werden — sonst wartet das Board beim
Start auf eine USB-Verbindung.

## Pin-Belegung

Entnommen aus LilyGOs offiziellem Repository
[LilyGo-Modem-Series](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series)
(`utilities.h`, Board-Definition `LILYGO_T_A7608X_S3`):

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

## Hinweise

- Für einen GNSS-Fix wird freie Sicht zum Himmel benötigt; bei einem
  Kaltstart kann das bis zu einigen Minuten dauern.
- Bei der Batteriespannungsmessung wird von einem 1:2-Spannungsteiler
  vor dem ADC-Pin ausgegangen, wie er auf diesem Board verbaut ist.
