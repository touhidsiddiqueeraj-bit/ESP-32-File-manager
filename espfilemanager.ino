#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FFat.h>
#include <SD.h>
#include <SPI.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

// --- WiFi Credentials ---
const char* ssid = "YOUR_HOME_WIFI";
const char* password = "YOUR_WIFI_PASSWORD";

// --- AP Fallback Credentials ---
const char* ap_ssid = "ESP32-S3-Drive";
const char* ap_password = "12345678";

// --- Server & DNS ---
WebServer server(80);
DNSServer dnsServer;

// --- Storage Management ---
bool useInternalFS = true;
FS* currentFS = &FFat;
File uploadFile;

bool internalReady = false;
bool externalReady = false;

// Global buffer for TAR streaming to prevent stack overflow
uint8_t tar_buf[512];

// SD Card Pins (Adjust to your specific ESP32-S3 board wiring)
#define SD_MISO 12
#define SD_MOSI 11
#define SD_SCK 10
#define SD_CS 13

// --- HTML & CSS (Light Mode File Manager UI - Mobile Fixed) ---
const char MAIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32-S3 File Manager</title>
    <style>
        :root { --primary: #2563eb; --bg: #f8fafc; --card-bg: #ffffff; --text: #1e293b; --border: #e2e8f0; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 20px; }
        .header { display: flex; justify-content: space-between; align-items: center; max-width: 800px; margin: 0 auto 20px auto; }
        .container { max-width: 800px; margin: 0 auto; background: var(--card-bg); border-radius: 12px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); padding: 20px; }
        .toolbar { display: flex; gap: 10px; margin-bottom: 20px; align-items: center; flex-wrap: wrap; }
        .btn { background: var(--primary); color: white; border: none; padding: 10px 16px; border-radius: 6px; cursor: pointer; text-decoration: none; font-size: 14px; font-weight: 500; transition: 0.2s; text-align: center; }
        .btn:hover { background: #1d4ed8; }
        .btn-secondary { background: #e2e8f0; color: var(--text); }
        .btn-secondary:hover { background: #cbd5e1; }
        .storage-tabs { display: flex; gap: 5px; margin-bottom: 15px; border-bottom: 2px solid var(--border); }
        .tab { padding: 10px 20px; cursor: pointer; border-bottom: 2px solid transparent; margin-bottom: -2px; font-weight: 500; color: #64748b; }
        .tab.active { color: var(--primary); border-bottom-color: var(--primary); }
        .table-wrap { overflow-x: auto; }
        table { width: 100%; border-collapse: collapse; min-width: 400px; }
        th, td { text-align: left; padding: 12px 8px; border-bottom: 1px solid var(--border); }
        th { color: #64748b; font-size: 12px; text-transform: uppercase; letter-spacing: 0.05em; }
        tr:hover { background-color: #f1f5f9; }
        .breadcrumb a { color: var(--primary); text-decoration: none; word-break: break-all; }
        .upload-form { margin-top: 20px; padding-top: 20px; border-top: 1px solid var(--border); display: flex; gap: 10px; flex-wrap: wrap; }
        input[type="file"] { flex: 1 1 100%; padding: 10px; border: 1px dashed #cbd5e1; border-radius: 6px; }
        .status { margin-top: 10px; font-size: 14px; color: #64748b; word-break: break-all; }
        
        /* Mobile Responsive Tweaks */
        @media (max-width: 600px) {
            body { padding: 0; }
            .container { border-radius: 0; padding: 15px; }
            .header { padding: 0 15px; margin-top: 15px; }
            .btn { padding: 10px 12px; font-size: 13px; flex: 1; }
            .toolbar .btn { flex: 1; }
            th, td { padding: 8px 4px; font-size: 14px; }
            .storage-tabs .tab { padding: 10px; font-size: 14px; flex: 1; text-align: center; }
        }
    </style>
</head>
<body>
    <div class="header">
        <h2>ESP32-S3 Storage</h2>
    </div>
    <div class="container">
        <div class="storage-tabs">
            <div class="tab active" id="tab-internal" onclick="switchStorage('internal', event)">Internal Storage</div>
            <div class="tab" id="tab-external" onclick="switchStorage('external', event)">External Storage (SD)</div>
        </div>
        
        <div class="breadcrumb" id="breadcrumb">
            <a href="#" onclick="navigate('/')">Root</a>
        </div>

        <div class="toolbar">
            <button class="btn btn-secondary" onclick="navigate(upDir())">⬆ Up</button>
            <button class="btn btn-secondary" onclick="loadFiles(currentPath)">🔄 Refresh</button>
            <button class="btn" onclick="downloadAll()">⬇ Download All (.tar)</button>
        </div>

        <div class="table-wrap">
            <table>
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Size</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody id="fileList"></tbody>
            </table>
        </div>

        <div class="upload-form">
            <input type="file" id="fileInput" multiple>
            <button class="btn" style="flex: 0 0 auto;" onclick="uploadFiles()">Upload</button>
        </div>
        <div class="status" id="statusText"></div>
    </div>

    <script>
        let currentPath = "/";
        let currentStorage = "internal";

        function upDir() {
            if (currentPath === "/") return "/";
            let parts = currentPath.split('/').filter(p => p !== '');
            parts.pop();
            return '/' + parts.join('/') + (parts.length > 0 ? '/' : '');
        }

        function navigate(path) {
            if (path === "..") path = upDir();
            currentPath = path;
            loadFiles(currentPath);
        }

        function switchStorage(type, e) {
            currentStorage = type;
            currentPath = "/";
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            e.target.classList.add('active');
            loadFiles("/");
        }

        function loadFiles(path) {
            currentPath = path;
            fetch(`/list?path=${encodeURIComponent(path)}&storage=${currentStorage}`)
                .then(res => res.json())
                .then(data => {
                    const list = document.getElementById('fileList');
                    list.innerHTML = '';
                    document.getElementById('breadcrumb').innerHTML = `<a href="#" onclick="navigate('/')">Root</a> ${path.split('/').filter(p=>p).map((p, i, arr) => ` / <a href="#" onclick="navigate('/${arr.slice(0,i+1).join('/')}/')">${p}</a>`).join('')}`;
                    
                    data.forEach(file => {
                        const isDir = file.type === "dir";
                        const icon = isDir ? "📁" : (file.name.match(/\.(jpg|jpeg|png|gif)$/i) ? "🖼️" : "📄");
                        const row = `<tr>
                            <td style="cursor:pointer;" onclick="${isDir ? `navigate('${path}${file.name}/')` : ''}">${icon} ${file.name}</td>
                            <td>${isDir ? '-' : formatSize(file.size)}</td>
                            <td>
                                ${!isDir ? `<a href="/download?path=${encodeURIComponent(path + file.name)}&storage=${currentStorage}" class="btn btn-secondary">Get</a>` : ''}
                                <button class="btn btn-secondary" onclick="deleteFile('${path}${file.name}', '${file.type}')">Del</button>
                            </td>
                        </tr>`;
                        list.innerHTML += row;
                    });
                });
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / 1048576).toFixed(1) + ' MB';
        }

        function downloadAll() {
            window.location.href = `/downloadAll?path=${encodeURIComponent(currentPath)}&storage=${currentStorage}`;
        }

        function uploadFiles() {
            const input = document.getElementById('fileInput');
            if (!input.files.length) return;
            document.getElementById('statusText').innerText = "Uploading...";
            
            const formData = new FormData();
            for (let i = 0; i < input.files.length; i++) {
                formData.append('files[]', input.files[i], input.files[i].name);
            }

            fetch(`/upload?path=${encodeURIComponent(currentPath)}&storage=${currentStorage}`, {
                method: 'POST',
                body: formData
            }).then(res => res.text())
              .then(data => {
                  document.getElementById('statusText').innerText = data;
                  input.value = '';
                  loadFiles(currentPath);
              }).catch(err => {
                  document.getElementById('statusText').innerText = "Upload failed!";
              });
        }

        function deleteFile(path, type) {
            if (!confirm(`Delete ${path}?`)) return;
            fetch('/delete', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `path=${encodeURIComponent(path)}&type=${type}&storage=${currentStorage}`
            }).then(res => res.text())
              .then(data => {
                  loadFiles(currentPath);
              });
        }

        loadFiles("/");
    </script>
</body>
</html>
)rawliteral";

// --- Helper Functions ---
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg")) return "image/jpeg";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".txt")) return "text/plain";
  return "application/octet-stream";
}

void selectStorage(String storage) {
  if (storage == "external") {
    useInternalFS = false;
    currentFS = &SD;
  } else {
    useInternalFS = true;
    currentFS = &FFat;
  }
}

// --- TAR Streaming Logic (For Download All) ---
void writeTarHeader(WiFiClient &client, String name, size_t size, bool isDir) {
    char header[512];
    memset(header, 0, 512);
    
    if (name.startsWith("/")) name = name.substring(1);
    if (isDir && !name.endsWith("/")) name += "/";
    
    strncpy(header, name.c_str(), 100);
    strncpy(header + 100, "0000644 ", 8);
    strncpy(header + 108, "0000000 ", 8);
    strncpy(header + 116, "0000000 ", 8);
    
    char sizeStr[12];
    snprintf(sizeStr, 12, "%011o", size);
    strncpy(header + 124, sizeStr, 11);
    header[135] = ' '; // Space after size
    
    strncpy(header + 136, "00000000000", 11);
    header[147] = ' '; // Space after mtime
    
    memset(header + 148, ' ', 8); // Initialize checksum as spaces
    header[156] = isDir ? '5' : '0'; // Type flag (5=dir, 0=file)
    
    // Calculate checksum
    int checksum = 0;
    for (int i = 0; i < 512; i++) checksum += header[i];
    
    char chkStr[8];
    snprintf(chkStr, 8, "%06o", checksum);
    chkStr[6] = '\0';
    chkStr[7] = ' ';
    memcpy(header + 148, chkStr, 8);
    
    client.write((uint8_t*)header, 512);
}

void streamDirectoryTar(WiFiClient &client, String path) {
    if (!path.endsWith("/")) path += "/";
    File root = currentFS->open(path);
    if (!root || !root.isDirectory()) return;
    
    File file = root.openNextFile();
    while (file) {
        if (!client.connected()) break;
        
        String fullPath = path + file.name();
        if (file.isDirectory()) {
            writeTarHeader(client, fullPath, 0, true);
            streamDirectoryTar(client, fullPath);
        } else {
            writeTarHeader(client, fullPath, file.size(), false);
            size_t bytesRead;
            while ((bytesRead = file.read(tar_buf, 512)) > 0) {
                if (!client.connected()) break;
                client.write(tar_buf, bytesRead);
                if (bytesRead < 512) {
                    // Pad the last block to 512 bytes
                    memset(tar_buf, 0, 512);
                    client.write(tar_buf, 512 - bytesRead);
                }
                vTaskDelay(1); // Yield to prevent Watchdog Timer crash
            }
        }
        file = root.openNextFile();
        vTaskDelay(1); // Yield between files
    }
    root.close();
}

// --- Web Server Handlers ---
void handleRoot() {
  server.send_P(200, "text/html", MAIN_HTML);
}

void handleList() {
  String path = server.arg("path");
  String storage = server.arg("storage");
  selectStorage(storage);

  String json = "[";
  File root = currentFS->open(path);
  if (!root || !root.isDirectory()) {
    server.send(200, "application/json", "[]");
    return;
  }

  File file = root.openNextFile();
  bool first = true;
  while (file) {
    if (!first) json += ",";
    first = false;
    
    String name = String(file.name());
    name = name.substring(name.lastIndexOf('/') + 1);
    if(name == "") name = String(file.name());

    json += "{\"name\":\"" + name + "\",\"size\":" + String(file.size()) + ",\"type\":\"" + (file.isDirectory() ? "dir" : "file") + "\"}";
    file = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleDownload() {
  String path = server.arg("path");
  String storage = server.arg("storage");
  selectStorage(storage);

  if (currentFS->exists(path)) {
    File file = currentFS->open(path, "r");
    String contentType = getContentType(path);
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

void handleDownloadAll() {
  String path = server.arg("path");
  String storage = server.arg("storage");
  selectStorage(storage);

  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: application/x-tar\r\n");
  client.print("Content-Disposition: attachment; filename=\"esp32_storage.tar\"\r\n");
  client.print("Connection: close\r\n\r\n");
  
  streamDirectoryTar(client, path);
  
  // End of TAR file (two 512-byte zero blocks)
  uint8_t eof[1024];
  memset(eof, 0, 1024);
  client.write(eof, 1024);
  client.stop();
}

void handleDelete() {
  String path = server.arg("path");
  String type = server.arg("type");
  String storage = server.arg("storage");
  selectStorage(storage);

  bool success = false;
  if (type == "dir") {
    success = currentFS->rmdir(path);
  } else {
    success = currentFS->remove(path);
  }

  if (success) server.send(200, "text/plain", "Deleted");
  else server.send(500, "text/plain", "Delete failed");
}

void handleUpload() {
  String path = server.arg("path");
  String storage = server.arg("storage");
  selectStorage(storage);

  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (filename.length() == 0) return;

    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith("/")) path += "/";
    
    if (!filename.startsWith("/")) {
      filename = path + filename;
    }
    
    Serial.printf("[Core 0] Upload Start: %s\n", filename.c_str());
    uploadFile = currentFS->open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[Core 0] Upload End: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
    }
  }
}

void handleUploadComplete() {
  server.send(200, "text/plain", "Upload complete!");
}

// --- FreeRTOS Task for Core 0 (Network/IoT) ---
void networkTask(void * pvParameters) {
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("[Core 0] Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[Core 0] Failed. Starting Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("[Core 0] AP IP address: ");
    Serial.println(WiFi.softAPIP());
    dnsServer.start(53, "*", WiFi.softAPIP());
  } else {
    Serial.println("\n[Core 0] Connected to WiFi!");
    Serial.print("[Core 0] IP address: ");
    Serial.println(WiFi.localIP());
  }

  if (!MDNS.begin("esp32s3-drive")) {
    Serial.println("[Core 0] Error setting up MDNS responder!");
  } else {
    Serial.println("[Core 0] mDNS responder started.");
  }

  // Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/list", HTTP_GET, handleList);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/downloadAll", HTTP_GET, handleDownloadAll); // TAR Download Route
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUpload);

  server.onNotFound([](){
    if (WiFi.getMode() == WIFI_AP) {
      server.sendHeader("Location", "http://esp32s3-drive.local/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();
  Serial.println("[Core 0] HTTP server started");

  // Infinite loop for handling clients
  for (;;) {
    server.handleClient();
    if (WiFi.getMode() == WIFI_AP) {
      dnsServer.processNextRequest();
    }
    vTaskDelay(1); // Yield to watchdog
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 

  // 1. Initialize Internal Storage (FFat)
  if (FFat.begin(false)) {
    internalReady = true;
    Serial.println("[Core 1] Internal FATFS Mounted successfully");
  } else {
    internalReady = false;
    Serial.println("[Core 1] Internal FATFS Mount Failed!");
  }

  // 2. Initialize External Storage (SD Card)
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS)) {
    externalReady = true;
    Serial.println("[Core 1] SD Card Mounted successfully");
  } else {
    externalReady = false;
    Serial.println("[Core 1] SD Card Mount Failed");
  }

  // 3. Start Network Task on Core 0
  xTaskCreatePinnedToCore(
    networkTask,   // Task function
    "NetworkTask", // Name
    10000,         // Stack size
    NULL,          // Parameters
    1,             // Priority
    NULL,          // Task handle
    0              // Pin to Core 0
  );
}

void loop() {
  // This loop runs on Core 1. 
  // We use it for background file processing (e.g., monitoring storage space)
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 15000) { // Every 15 seconds
    lastCheck = millis();
    if (internalReady) {
      Serial.printf("[Core 1] Internal FS Free: %llu bytes\n", FFat.totalBytes() - FFat.usedBytes());
    }
  }
  delay(10);
}
