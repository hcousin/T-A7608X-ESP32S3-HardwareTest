# T-A7608X-ESP32S3 Full Hardware Test

Arduino test sketch for the LilyGO **T-A7608X-ESP32S3** board
(silkscreen: `A7608-ESP32S3 2023-08-24 V1.0`) with a SIMCom **A7608E-H**
LTE Cat-1 modem (with built-in GNSS) and an ESP32-S3-WROOM-1 MCU.

The sketch tests, in order:

1. Modem power-on sequence + AT communication
2. SIM card detection (IMEI / IMSI)
3. Network registration + signal quality
4. LTE data connection + HTTP GET (`httpbin.org/ip`)
5. GNSS fix via the modem's built-in GNSS engine
6. Battery voltage via ADC
7. microSD card read/write (TF card slot)

## Hardware setup

- Insert a SIM card into the SIM slot.
- Connect the **LTE antenna** (Full Band LTE, 698–960/1710–2690 MHz) to the
  modem's main antenna connector.
- Connect the **GNSS antenna** to the u.FL connector labeled `GNSS`.
- Optionally insert a microSD card.

## Arduino IDE setup

1. Install the **esp32 by Espressif Systems** board package.
2. Select board **ESP32S3 Dev Module** (USB CDC On Boot: *Enabled*).
3. Install the **TinyGSM** library — use the LilyGO fork with A7608
   support, not mainline TinyGSM:
   https://github.com/lewisxhe/TinyGSM
4. Open `T-A7608X-ESP32S3_FullTest.ino`, adjust the `apn` constant to
   match your SIM provider, and upload via the board's USB-C port.
5. Open the Serial Monitor at 115200 baud to see the test results.

## Pin mapping

Taken from LilyGO's official
[LilyGo-Modem-Series](https://github.com/Xinyuan-LilyGO/LilyGo-Modem-Series)
repository (`utilities.h`, `LILYGO_T_A7608X_S3` board definition):

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

## Notes

- GNSS fix acquisition needs an open sky view and can take up to a
  couple of minutes on a cold start.
- The battery voltage reading assumes a 1:2 voltage divider in front
  of the ADC pin, as used on this board.
