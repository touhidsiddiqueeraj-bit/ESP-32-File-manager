📂 ESP32-S3 Wireless File Manager
Turn your ESP32-S3 into a fully functional wireless flash drive and SD card reader. This project provides a clean, light-mode web interface to upload, download, manage, and bulk-backup files on your ESP32-S3's internal flash and external SD card directly from your phone or computer.

ESP32-S3 File ManagerLicenseUI

✨ Features
IoT WiFi Management: Update your WiFi SSID and password directly from the web interface. Credentials are saved permanently to Non-Volatile Storage (NVS). If the ESP32 loses power, it remembers your network.
Dual Storage Support: Seamlessly switch between Internal Storage (FATFS/LittleFS) and External Storage (SD Card) via tabs, just like a modern phone.
Smart WiFi Connectivity: Automatically connects to your saved WiFi. If it fails, it instantly falls back to Access Point (AP) mode with a Captive Portal—just connect to the ESP32 WiFi and the file manager pops up automatically.
mDNS Support: Access the interface easily via http://esp32s3-drive.local without hunting for IP addresses.
Mobile-First Responsive UI: A clean, Apple/Android-style light-mode file manager that scales perfectly on mobile screens, tablets, and desktops.
File Operations: Upload multiple files, download files, delete files, and navigate directories.
Bulk "Download All" Feature: Wirelessly backs up your entire flash/SD card contents by dynamically generating and streaming a .tar archive on the fly.
Dual-Core Processing: Utilizes FreeRTOS to pin network/IoT tasks to Core 0, leaving Core 1 free for file processing and system monitoring, preventing crashes and ensuring smooth transfers.
No External Libraries Required: Built entirely on the standard ESP32 Arduino Core libraries.
🛠 Hardware Requirements
ESP32-S3 Development Board (Tested with 16MB Flash).
MicroSD Card Module (Optional, for external storage).
MicroSD Card (Formatted to FAT32).
SD Card Wiring (ESP32-S3)
If you are using an external SD card, wire it to your ESP32-S3 as follows (you can change these pins in the code):

SD Card Module	ESP32-S3 Pin
MISO	GPIO 12
MOSI	GPIO 11
SCK	GPIO 10
CS	GPIO 13
VCC	3.3V (or 5V depending on your module)
GND	GND
💻 Software Setup
1. Prerequisites
Arduino IDE installed.
ESP32 Board Manager installed in Arduino IDE (Version 3.x recommended).
2. Configure the Code
Open the .ino file. The first time you upload the code, you can put your default WiFi credentials in the variables at the top:

String ssid = "YOUR_HOME_WIFI";       // Default WiFi nameString password = "YOUR_WIFI_PASSWORD"; // Default WiFi password
Note: Once you change your WiFi via the web interface, these lines are ignored and the new credentials are used.

3. Arduino IDE Settings (Crucial)
Before uploading, configure your IDE settings to prevent erasing your existing data:

Board: ESP32S3 Dev Module
Partition Scheme: 16MB Flash (2MB APP/12.5MB FATFS) (or whatever matches your specific board)
Erase All Flash Before Sketch Upload: Disabled (Unless you want to wipe the drive)
4. Upload
Upload the code to your board. Open the Serial Monitor at 115200 baud to view the IP address and connection status.

🚀 Usage Guide
Connect to WiFi:
Home WiFi Mode: Ensure your computer/phone is on the same WiFi network as the ESP32. Open a browser and go to http://esp32s3-drive.local.
AP Mode: If away from home, connect to the ESP32-S3-Drive WiFi network using the password 12345678. A captive portal should automatically appear. If not, navigate to 192.168.4.1.
Manage Files: Use the toolbar to upload, download, and delete files.
Switch Storage: Click the "Internal Storage" or "External Storage" tabs to switch between the ESP32's built-in flash and the SD card.
Backup Everything: Click the ⬇ Download All (.tar) button to download the current directory as a .tar file. You can extract this file using WinRAR, 7-Zip, or the default tar command on Mac/Linux.
Update WiFi (IoT): Click the ⚙️ WiFi button in the top right corner. Enter your new SSID and Password, then click "Save & Reboot". The ESP32 will save the credentials, reboot, and connect to your new network.
⚙️ How It Works (Under the Hood)
Network & Web Server: Uses the standard WebServer.h library. HTTP routing and file streaming are handled entirely asynchronously on Core 0 to prevent the Watchdog Timer (WDT) from triggering resets during heavy network transfers.
NVS IoT Configuration: WiFi credentials are stored using the Preferences.h library, which writes directly to the ESP32's Non-Volatile Storage. This ensures settings persist across reboots and power losses.
Dynamic TAR Streaming: Instead of using complex ZIP libraries (which require heavy RAM), the "Download All" feature calculates file sizes and TAR headers dynamically, streaming the data block-by-block directly to the HTTP client.
Storage Abstraction: Uses the FS interface pointer, allowing the web server code to remain identical whether the user is interacting with FFat (Internal) or SD (External).
📝 License
This project is licensed under the MIT License - see the LICENSE file for details. Feel free to modify, distribute, and use it in your own projects!

If you found this project helpful, please consider giving the repository a ⭐ on GitHub!
