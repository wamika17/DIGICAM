# ESP32-CAM firmware

`DIGICAM_CameraWebServer.ino` keeps the CameraWebServer setup and adds a Serial Monitor command (`c`) to capture a JPEG and POST it to the laptop receiver. It is prepared firmware; compile and hardware upload still need to be verified.

## Use a copy of the working CameraWebServer sketch

1. In Arduino IDE, open your working CameraWebServer sketch and choose **Sketch → Show Sketch Folder**.
2. Copy the complete sketch folder in File Explorer and name the copy `DIGICAM_CameraWebServer`.
3. In the copied folder, rename its main `.ino` to `DIGICAM_CameraWebServer.ino`.
4. Replace that copied main file with the `.ino` in this repository.
5. Keep the copy's `board_config.h`, `camera_pins.h`, `app_httpd.cpp`, `camera_index.h`, and any other example files. Confirm `board_config.h` still selects `CAMERA_MODEL_AI_THINKER`.
6. Copy `digicam_config.example.h` from this repository into the copied sketch folder as `digicam_config.h`. Edit the local copy with the Wi-Fi network and laptop address. Keep this private file out of Git and chat.

The original working sketch remains untouched and can be reopened if needed.

## Run the laptop receiver

In a separate PowerShell window, from the repository root:

```powershell
ipconfig
```

Use the IPv4 address under the active Wi-Fi adapter in `digicam_config.h`, with port `5000` and path `/api/captures`. In another PowerShell window, run the receiver from the repository root:

```powershell
.\.venv\Scripts\python.exe backend\app.py
```

If `.venv` does not exist, create it and install dependencies using the root README first. Keep the receiver running. The ESP32-CAM and laptop must share the same Wi-Fi network. Allow Python on the private/home network if Windows Firewall asks.

## Compile and upload

1. Open the copied `DIGICAM_CameraWebServer.ino` in Arduino IDE.
2. Select the same ESP32 board and port that worked for CameraWebServer. Keep the same board and partition settings.
3. Click **Verify** and resolve any compile error before uploading.
4. Click **Upload**. Use the same upload procedure that worked for the original sketch.
5. Open **Tools → Serial Monitor**, select **115200 baud** and **No line ending**.
6. Wait for `Camera web server started.` Type `c` and press Enter.

## Expected result

Serial Monitor should show `Upload complete. The photo should now appear in the DIGICAM gallery.` The receiver logs the saved capture; the dashboard should show it within its next refresh. The timestamp currently represents laptop receive time.

If upload fails, check that Flask is running, the configured laptop address is current, and both devices use the same local network. Do not connect a physical shutter button yet; GPIO selection and wiring are a separate hardware step.
