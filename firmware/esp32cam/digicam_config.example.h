#pragma once

// Copy this file to digicam_config.h in your private sketch folder.
// Keep digicam_config.h local; do not post it or add it to shared code.
const char *DIGICAM_WIFI_SSID = "YOUR_WIFI_NAME";
const char *DIGICAM_WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Replace with your laptop's Wi-Fi IPv4 address from ipconfig.
// Example format only: http://192.168.1.25:5000/api/captures
const char *DIGICAM_UPLOAD_URL = "http://YOUR_LAPTOP_IP:5000/api/captures";
