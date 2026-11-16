# DIGICAM hardware notes

## Verified setup

- AI Thinker ESP32-CAM
- OV2640 camera module
- ESP32-CAM-MB USB programmer
- Windows laptop and local Wi-Fi

The existing Espressif CameraWebServer firmware has been compiled and uploaded, the camera has joined Wi-Fi, and its live camera page opens in a browser. The current stream is slow; performance tuning is deferred.

## AI Thinker camera pin map

The official Espressif CameraWebServer mapping for `CAMERA_MODEL_AI_THINKER` assigns the following signals. This list is included to prevent accidentally treating a camera signal as a spare shutter pin.

| Signal | ESP32 GPIO |
| --- | ---: |
| Camera power-down | 32 |
| Camera XCLK | 0 |
| SCCB data / clock | 26 / 27 |
| Camera Y2–Y9 data | 5, 18, 19, 21, 36, 39, 34, 35 |
| Camera VSYNC / HREF / PCLK | 25 / 23 / 22 |
| Flash LED | 4 |

Source: [Espressif Arduino-ESP32 CameraWebServer `camera_pins.h`](https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/camera_pins.h).

## Shutter status

No GPIO has been assigned to a physical shutter. The board's camera wiring, flash LED, boot behavior, programmer board, and the exact physical breakout must be checked before wiring a button. The first upload test uses the Arduino Serial Monitor command `c`, so it does not require a new GPIO connection.

## Storage and sensors

The project does not use a microSD card. The laptop receiver stores images under `backend/captures/YYYY/MM/`. No environmental sensor has been selected or connected yet; environmental measurements remain future work.
