#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Keep the existing AI Thinker board_config.h from CameraWebServer.
#include "board_config.h"
// Local-only file: copy digicam_config.example.h to digicam_config.h and edit it.
#include "digicam_config.h"

void startCameraServer();
void setupLedFlash();

static unsigned long lastWifiRetry = 0;

static bool waitForWiFi(unsigned long timeoutMs) {
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

static void captureAndUpload() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi is disconnected; trying to reconnect before capture...");
    WiFi.reconnect();
    if (!waitForWiFi(15000)) {
      Serial.println("Upload skipped: Wi-Fi is still disconnected. Press c to retry.");
      return;
    }
    Serial.print("Wi-Fi reconnected. Laptop receiver: ");
    Serial.println(DIGICAM_UPLOAD_URL);
  }

  Serial.println("Capturing JPEG...");
  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == nullptr) {
    Serial.println("Capture failed: camera returned no frame.");
    return;
  }
  if (frame->format != PIXFORMAT_JPEG) {
    Serial.println("Capture failed: camera frame is not JPEG.");
    esp_camera_fb_return(frame);
    return;
  }

  Serial.printf("Captured %u bytes; uploading to laptop...\n", frame->len);
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(DIGICAM_UPLOAD_URL)) {
    Serial.println("Upload failed: could not open the laptop receiver URL.");
    esp_camera_fb_return(frame);
    return;
  }

  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Filename", "digicam.jpg");
  const int status = http.POST(frame->buf, frame->len);
  const String response = status > 0 ? http.getString() : String();
  http.end();
  esp_camera_fb_return(frame);

  if (status == 201) {
    Serial.println("Upload complete. The photo should now appear in the DIGICAM gallery.");
  } else if (status > 0) {
    Serial.printf("Laptop replied with HTTP %d: %s\n", status, response.c_str());
  } else {
    Serial.printf("Upload failed (HTTPClient error %d). Check the laptop server, Wi-Fi, and laptop IP.\n", status);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("DIGICAM camera starting...");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  const esp_err_t cameraError = esp_camera_init(&config);
  if (cameraError != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x. The original CameraWebServer firmware is unchanged.\n", cameraError);
    return;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor->id.PID == OV3660_PID) {
    sensor->set_vflip(sensor, 1);
    sensor->set_brightness(sensor, 1);
    sensor->set_saturation(sensor, -2);
  }
  // Keep the existing CameraWebServer preview responsive at its current size.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    sensor->set_framesize(sensor, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  sensor->set_vflip(sensor, 1);
  sensor->set_hmirror(sensor, 1);
#endif
#if defined(CAMERA_MODEL_ESP32S3_EYE)
  sensor->set_vflip(sensor, 1);
#endif
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(DIGICAM_WIFI_SSID, DIGICAM_WIFI_PASSWORD);
  WiFi.setSleep(false);
  Serial.print("Connecting to Wi-Fi");
  if (waitForWiFi(20000)) {
    Serial.println("Wi-Fi connected.");
    Serial.print("Camera IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi did not connect yet; background retries will continue.");
  }

  // Keep the existing CameraWebServer preview in this copied sketch.
  startCameraServer();
  Serial.println("Camera web server started.");
  Serial.println("Press lowercase c in Serial Monitor to capture and upload a photo.");
  Serial.println("Use 115200 baud and set the line ending to 'No line ending'.");
}

void loop() {
  if (Serial.available() > 0) {
    const char command = Serial.read();
    if (command == 'c' || command == 'C') {
      captureAndUpload();
    }
  }

  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiRetry >= 10000) {
    lastWifiRetry = millis();
    Serial.println("Wi-Fi disconnected; retrying...");
    WiFi.reconnect();
  }
  delay(10);
}
