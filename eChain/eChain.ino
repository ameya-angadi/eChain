/*
 * Project Name: eChain: A Fun Digital Key Chain
 * Designed For: XIAO ePaper Display Board EE05 By Seeed Studio
 *
 * License: CC BY-NC 4.0
 * This project is licensed under the Creative Commons Attribution-NonCommercial 
 * 4.0 International License. You are free to use, modify, and share this software 
 * for non-commercial purposes, as long as you provide appropriate credit to the 
 * original author and indicate if changes were made. 
 * For full legal details, see <https://creativecommons.org/licenses/by-nc/4.0/>.
 *
 * Copyright (C) 2026  Ameya Angadi
 *
 * Code Created And Maintained By: Ameya Angadi
 * Last Modified On: September 26, 2026
 * Version: 1.0.0
 *
 * Support my projects by purchasing hardware through the following affiliate links:
 *
 * 2.9" Quadruple Color (BWRY) ePaper Display - https://www.seeedstudio.com/2-9-Quadruple-Color-ePaper-Display-with-128x296-Pixels-p-5783.html?sensecap_affiliate=JI84v1k&referring_service=link
 *
 * XIAO ePaper Display Board EE05 - https://www.seeedstudio.com/XIAO-ePaper-Display-Board-EE05-p-6755.html?sensecap_affiliate=JI84v1k&referring_service=link
 *
 */

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h> 
#include "TFT_eSPI.h"
#include "driver/rtc_io.h" 

#include "eChain_Image.h" 
#include "contact_icon.h"
#include "setup_icon.h"

void countStoredImages();
String getFileNameByIndex(int index);
void triggerBuzzer(int times);
void goToSleep(uint64_t sleepTime_ms, bool enableTimer);
void drawImage(int index);
void drawContactCard();
int getBatteryPercentage();

const int BTN2_PIN = D2; 
const int BTN3_PIN = D9; 
const int BUZZER_PIN = D13; 
const int BATTERY_PIN = A0; 
const int ADC_EN_PIN = D12;

#define BUTTON_PRESSED LOW 

#ifdef EPAPER_ENABLE
EPaper epaper;
#endif

RTC_DATA_ATTR int currentImageIndex = 0;
RTC_DATA_ATTR bool isBuzzerEnabled = true;
RTC_DATA_ATTR uint64_t slideShowInterval_ms = 1800000; // Default 30 mins
RTC_DATA_ATTR bool isContactMode = false; 
int totalImages = 0;

const float CALIBRATION_FACTOR = 0.968;

// --- NETWORK CONFIGURATION ---
const char* ssid = "eChain Setup";
const char* password = "img@1234";
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
File uploadFile;
unsigned long apStartTime = 0;
const unsigned long AP_TIMEOUT = 300000; // 5 minutes

// --- HTML / WEB UI ---
const char* index_html = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>eChain | Control Center</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, system-ui, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
    h2, h3 { color: #ffffff; border-bottom: 1px solid #333; padding-bottom: 10px; margin-top: 0; }
    .card { background: #1e1e1e; padding: 20px; margin-bottom: 20px; border-radius: 8px; border: 1px solid #2a2a2a; }
    .accent { color: #ff6600; }
    
    .flex-row { display: flex; gap: 15px; align-items: stretch; margin-top: 10px; }
    .flex-col { display: flex; flex-direction: column; gap: 8px; flex-grow: 1; }
    
    input[type=file], input[type=text], input[type=number] { width: 100%; box-sizing: border-box; background: #2a2a2a; border: 1px solid #333; color: #fff; padding: 12px; border-radius: 4px; margin: 0; }
    
    button { background-color: #ff6600; color: white; padding: 12px 24px; border: none; border-radius: 4px; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.2s; white-space: nowrap; }
    button:hover { background-color: #e65c00; }
    
    .btn-action { padding: 8px 16px; height: 42px; align-self: flex-end; }
    .btn-secondary { background-color: #555; padding: 8px 16px; font-size: 14px; margin-bottom: 10px;}
    .btn-secondary:hover { background-color: #444; }
    .btn-flip { padding: 6px 12px; font-size: 14px; width: auto; margin-right: 10px; }
    .btn-exit { background-color: #d32f2f; padding: 8px 16px; font-size: 14px; margin-left: 15px;}
    .btn-exit:hover { background-color: #b71c1c; }
    
    .delete-btn { background-color: #d32f2f; padding: 6px 12px; width: auto; font-size: 14px; }
    .delete-btn:hover { background-color: #b71c1c; }
    
    .btn-icon { background-color: #444; padding: 6px 10px; font-size: 14px; margin-right: 5px; }
    .btn-icon:hover { background-color: #666; }
    
    .instructions { font-size: 14px; color: #aaa; line-height: 1.5; }
    .checkbox-container { display: flex; align-items: center; margin-bottom: 8px; }
    .checkbox-container input { width: auto; margin-right: 10px; }
    .status { font-weight: bold; margin-top: 10px; display: block; min-height: 20px; }
    
    .image-item { display: flex; justify-content: space-between; align-items: center; background: #2a2a2a; padding: 10px; margin-bottom: 8px; border-radius: 4px; }
    .image-item-left { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; flex-grow: 1; }
    .image-item-right { display: flex; align-items: center; margin-left: 10px; }
    .img-name { margin-right: 10px; word-break: break-all; font-size: 15px; }
    
    .storage-header { display: flex; justify-content: space-between; align-items: center; background: #1e1e1e; padding: 15px 20px; border-radius: 8px; border: 1px solid #2a2a2a; margin-bottom: 20px; }
    .progress-bg { flex-grow: 1; background: #111; height: 12px; border-radius: 6px; margin: 0 15px; overflow: hidden; }
    .progress-fill { width: 0%; height: 100%; background: #ff6600; transition: width 0.3s ease; }

    /* MOBILE RESPONSIVE TWEAKS */
    @media (max-width: 768px) {
      .flex-row { flex-direction: column; }
      .btn-action { align-self: stretch; width: 100%; height: auto; }
      .storage-header { flex-direction: column; text-align: center; gap: 12px; }
      .btn-exit { margin-left: 0; width: 100%; }
      .progress-bg { width: 100%; margin: 5px 0; }
      
      .image-item { flex-direction: column; align-items: stretch; gap: 10px; }
      .image-item-left { flex-direction: column; align-items: flex-start; }
      .image-item-right { margin-left: 0; justify-content: flex-end; width: 100%; }
      .img-name { margin-bottom: 5px; }
    }
  </style>
</head>
<body>
  
  <div class="storage-header">
    <h2 style="margin:0; border:none; padding:0;"><span class="accent">eChain</span></h2>
    <div class="progress-bg"><div id="storage-bar" class="progress-fill"></div></div>
    <span id="storage-text" style="font-size: 14px; color: #aaa; white-space: nowrap;">Loading...</span>
    <button class="btn-exit" onclick="exitSetup()">Exit & Reboot</button>
  </div>

  <div class="card">
    <h3>1. Instructions</h3>
    <div class="instructions">
      <p><b>Step 1:</b> Go to <a href="https://sensecraft.seeed.cc/hmi/tools/dither" target="_blank" class="accent">SenseCraft Dither Tool</a>.</p>
      <p><b>Step 2:</b> Upload image.<br/>Set params: <i>Select E4 Four-Color 4bpp, set Width and height as (128 and 296) or (296 and 128)</i>.<br/>Select appropriate Dither Algorithm and play with Gamma Correction, Vibrance, Clarity, Darkness and use preview option to see the results.</p>
      <p><b>Step 3:</b> Generate Header (.h) file and upload it below.</p>
    </div>
  </div>

  <div class="card">
    <h3>2. Manage Images</h3>
    <p class="instructions" style="margin-top: 0;">Use the Move Up and Move Down buttons to set the priority order for your slideshow.</p>
    <div id="image-list" style="margin-bottom: 15px;">Loading...</div>
    <div style="display:flex; flex-wrap:wrap; gap:10px;">
      <button class="btn-secondary" onclick="deleteSelected()">Delete Selected</button>
      <button class="btn-secondary" style="background-color: #d32f2f;" onclick="deleteAll()">Delete All Images</button>
    </div>
    <hr style="border-color: #333; margin: 20px 0;">
    
    <div class="flex-row">
      <div class="flex-col">
        <input type="file" id="fileInput" accept=".h" multiple>
      </div>
      <button class="btn-action" onclick="uploadImages()">Upload New</button>
    </div>
    <span id="upload-status" class="status"></span>
  </div>

  <div class="card">
    <h3>3. Digital Business Card</h3>
    <div class="flex-row">
      <div class="flex-col">
        <input type="text" id="contact-name" placeholder="Full Name (Max 15)" maxlength="15">
        <input type="text" id="contact-phone" placeholder="Phone Number (Max 15)" maxlength="15">
        <input type="text" id="contact-email" placeholder="Email Address (Max 30)" maxlength="30">
        <input type="text" id="contact-custom" placeholder="Custom Message (Optional, Max 30)" maxlength="30">
      </div>
      <button class="btn-action" onclick="saveContact()">Update Contact</button>
    </div>
    <span id="contact-status" class="status"></span>
  </div>

  <div class="card">
    <h3>4. Device Settings</h3>
    <div class="flex-row">
      <div class="flex-col">
        <label style="color:#aaa; font-size:14px;">Slideshow Timer (Minutes)</label>
        <input type="number" id="timer-input" value="30" min="5">
        <div class="checkbox-container">
          <input type="checkbox" id="buzzer-toggle" checked>
          <label style="color:#aaa; font-size:14px;">Enable Buzzer</label>
        </div>
      </div>
      <button class="btn-action" onclick="saveSettings()">Save Settings</button>
    </div>
    <span id="settings-status" class="status"></span>
  </div>

  <script>
    let existingImageNames = [];

    const showStatus = (id, message, isError = false) => {
      const el = document.getElementById(id);
      el.style.color = isError ? '#d32f2f' : '#4CAF50';
      el.innerText = message;
      setTimeout(() => { if (el.innerText === message) el.innerText = ""; }, 5000);
    };

    const loadStorage = async () => {
      const res = await fetch('/storage');
      const data = await res.json();
      const pct = data.total > 0 ? Math.round((data.used / data.total) * 100) : 0;
      const freeKB = Math.round((data.total - data.used) / 1024);
      document.getElementById('storage-text').innerText = `${pct}% Used (${freeKB}KB Free) | Battery: ${data.battery}%`;
      document.getElementById('storage-bar').style.width = `${pct}%`;
    };

    const exitSetup = async () => {
      if(!confirm("Exit Setup Mode and reboot into Slideshow?")) return;
      await fetch('/exit', { method: 'POST' });
      document.body.innerHTML = "<h2 style='text-align:center; margin-top:50px;'>Rebooting...<br>You can now close this page.</h2>";
    };

    const loadSettings = async () => {
      try {
        const res = await fetch('/get_settings');
        if (res.ok) {
          const data = await res.json();
          if (data.timer) document.getElementById('timer-input').value = data.timer;
          if (data.buzzer !== undefined) document.getElementById('buzzer-toggle').checked = (data.buzzer == 1);
        }
      } catch (e) {}
    };

    const loadContact = async () => {
      try {
        const res = await fetch('/get_contact');
        if (res.ok) {
          const data = await res.json();
          if(data.name) document.getElementById('contact-name').value = data.name;
          if(data.phone) document.getElementById('contact-phone').value = data.phone;
          if(data.email) document.getElementById('contact-email').value = data.email;
          if(data.custom) document.getElementById('contact-custom').value = data.custom;
        }
      } catch (e) {}
    };

    const loadImages = async () => {
      const res = await fetch('/list_images');
      const data = await res.json();
      existingImageNames = data.map(i => i.name);
      
      const list = document.getElementById('image-list');
      list.innerHTML = '';
      
      if(data.length === 0) {
        list.innerHTML = '<span style="color:#aaa;">No custom images uploaded yet.</span>';
      } else {
        data.forEach((item, idx) => {
          const isFlipped = item.flip;
          const bgColor = isFlipped ? '#ff6600' : '#555';
          const btnText = isFlipped ? 'Flipped' : 'Flip';
          
          list.innerHTML += `
            <div class="image-item">
              <div class="image-item-left">
                <input type="checkbox" class="img-cb" value="${item.name}">
                <span class="img-name">${idx + 1}. ${item.name.replace('.bin', '.h')}</span>
                <div>
                  <button class="btn-icon" onclick="moveUp(${idx})">Move Up</button>
                  <button class="btn-icon" onclick="moveDown(${idx})">Move Down</button>
                </div>
              </div>
              <div class="image-item-right">
                <button class="btn-flip" style="background-color: ${bgColor};" onclick="toggleFlip('${item.name}', ${!isFlipped})">${btnText}</button>
                <button class="delete-btn" onclick="deleteImages(['${item.name}'])">X</button>
              </div>
            </div>`;
        });
      }
      loadStorage(); 
    };

    const moveUp = async (idx) => {
      if (idx === 0) return;
      const temp = existingImageNames[idx - 1];
      existingImageNames[idx - 1] = existingImageNames[idx];
      existingImageNames[idx] = temp;
      await saveOrder();
    };

    const moveDown = async (idx) => {
      if (idx === existingImageNames.length - 1) return;
      const temp = existingImageNames[idx + 1];
      existingImageNames[idx + 1] = existingImageNames[idx];
      existingImageNames[idx] = temp;
      await saveOrder();
    };

    const saveOrder = async () => {
      await fetch('/save_order', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(existingImageNames)
      });
      loadImages();
    };

    const toggleFlip = async (imgName, state) => {
      await fetch('/set_flip', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `img=${imgName}&state=${state ? '1' : '0'}`
      });
      loadImages();
    };

    const deleteImages = async (filenames) => {
      if(!confirm(`Are you sure you want to permanently delete ${filenames.length} image(s)?`)) return;
      try {
        const res = await fetch('/delete_images', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(filenames)
        });
        if (!res.ok) throw new Error();
        await loadImages(); 
        showStatus('upload-status', "Deleted Successfully!");
      } catch (e) {
        showStatus('upload-status', "Failed to delete images.", true);
      }
    };

    const deleteSelected = async () => {
      const cbs = document.querySelectorAll('.img-cb:checked');
      const toDelete = Array.from(cbs).map(cb => cb.value);
      if(toDelete.length > 0) deleteImages(toDelete);
    };

    const deleteAll = async () => {
      if(!confirm("Are you sure you want to delete ALL images? This cannot be undone.")) return;
      try {
        const res = await fetch('/delete_all_images', { method: 'POST' });
        if (!res.ok) throw new Error();
        await loadImages();
        showStatus('upload-status', "All images deleted.");
      } catch (e) {
        showStatus('upload-status', "Failed to delete images.", true);
      }
    };

    window.onload = () => { loadImages(); loadContact(); loadSettings(); };

    const uploadImages = async () => {
      try {
        const files = document.getElementById('fileInput').files;
        if (files.length === 0) return showStatus('upload-status', "Please select at least one .h file.", true);

        for (let i = 0; i < files.length; i++) {
          const safeName = files[i].name.replace('.h', '.bin');
          if (existingImageNames.includes(safeName)) {
            return showStatus('upload-status', `Error: "${files[i].name}" already exists.`, true);
          }
        }

        for (let i = 0; i < files.length; i++) {
          showStatus('upload-status', `Uploading ${i + 1} of ${files.length}...`);
          const text = await files[i].text();
          
          let w = 128, h = 296;
          let wMatch = text.match(/width.*?(\d+)/i);
          let hMatch = text.match(/height.*?(\d+)/i);
          if (wMatch) w = parseInt(wMatch[1]);
          if (hMatch) h = parseInt(hMatch[1]);
          if (!wMatch && !hMatch) {
             let idx296 = text.indexOf("296");
             let idx128 = text.indexOf("128");
             if (idx296 !== -1 && idx128 !== -1) {
                 if (idx296 < idx128) { w = 296; h = 128; }
             }
          }
          
          const hexArray = text.match(/0x[0-9A-Fa-f]{1,2}/g);
          if (!hexArray) continue;
          
          const byteArray = new Uint8Array(hexArray.length);
          for (let j = 0; j < hexArray.length; j++) byteArray[j] = parseInt(hexArray[j], 16);

          const safeName = files[i].name.replace('.h', '.bin');
          const formData = new FormData();
          formData.append('data', new Blob([byteArray]), safeName);

          const res = await fetch(`/upload?name=${safeName}&w=${w}&h=${h}`, { method: 'POST', body: formData });
          if (!res.ok) throw new Error();
        }
        
        showStatus('upload-status', "Upload Complete!");
        document.getElementById('fileInput').value = "";
        await loadImages();
      } catch (e) {
        showStatus('upload-status', "Upload Failed! Try again.", true);
      }
    };

    const saveContact = async () => {
      try {
        const name = encodeURIComponent(document.getElementById('contact-name').value);
        const phone = encodeURIComponent(document.getElementById('contact-phone').value);
        const email = encodeURIComponent(document.getElementById('contact-email').value);
        const custom = encodeURIComponent(document.getElementById('contact-custom').value);
        
        const res = await fetch('/contact', { 
          method: 'POST', 
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: `name=${name}&phone=${phone}&email=${email}&custom=${custom}`
        });
        if (!res.ok) throw new Error();
        showStatus('contact-status', "Contact saved successfully!");
        loadStorage();
      } catch (e) {
        showStatus('contact-status', "Failed to save contact.", true);
      }
    };

    const saveSettings = async () => {
      try {
        let timerVal = parseInt(document.getElementById('timer-input').value);
        if (timerVal < 5 || isNaN(timerVal)) {
          timerVal = 5;
          document.getElementById('timer-input').value = 5;
          showStatus('settings-status', "Minimum timer is 5 minutes.", true);
        }
        
        const timer = encodeURIComponent(timerVal);
        const buzzer = document.getElementById('buzzer-toggle').checked ? "1" : "0";
        
        const res = await fetch('/settings', { 
          method: 'POST', 
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: `timer=${timer}&buzzer=${buzzer}`
        });
        if (!res.ok) throw new Error();
        showStatus('settings-status', "Settings saved successfully!");
        loadStorage();
      } catch (e) {
        showStatus('settings-status', "Failed to save settings.", true);
      }
    };
  </script>
</body>
</html>
)rawliteral";

float readBatteryVoltage() {
  digitalWrite(ADC_EN_PIN, HIGH);
  delay(10);
  
  long sum = 0;
  for(int i = 0; i < 30; i++) {
    sum += analogRead(BATTERY_PIN);
    delayMicroseconds(100);
  }
  digitalWrite(ADC_EN_PIN, LOW);
  
  float adc_avg = sum / 30.0;
  float voltage = (adc_avg / 4095.0) * 3.6 * 2.0 * CALIBRATION_FACTOR;
  return voltage;
}

int calculateBatteryPercentage(float voltage) {
  int percentage = (voltage - 3.2) * 100.0 / (4.2 - 3.2);
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;
  return percentage;
}

int getBatteryPercentage() {
  return calculateBatteryPercentage(readBatteryVoltage());
}

String getFileNameByIndex(int index) {
  int currentIndex = 0;
  
  if (LittleFS.exists("/order.json")) {
    File f = LittleFS.open("/order.json", "r");
    JsonDocument doc;
    if (!deserializeJson(doc, f)) {
      JsonArray arr = doc.as<JsonArray>();
      for (JsonVariant v : arr) {
        String fname = v.as<String>();
        if (LittleFS.exists("/" + fname)) {
          if (currentIndex == index) {
            f.close();
            return "/" + fname;
          }
          currentIndex++;
        }
      }
    }
    f.close();
  }
  
  File root = LittleFS.open("/");
  if (!root) return "";
  
  File file = root.openNextFile();
  while(file) {
    String fname = String(file.name());
    if(fname.startsWith("/")) fname = fname.substring(1); 
    
    if(!file.isDirectory() && fname.endsWith(".bin")) {
      if(currentIndex == index) {
        file.close();
        root.close();
        return "/" + fname;
      }
      currentIndex++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return "";
}

void countStoredImages() {
  totalImages = 0;
  File root = LittleFS.open("/");
  if (!root) return;
  
  File file = root.openNextFile();
  while(file) {
    String fname = String(file.name());
    if(fname.startsWith("/")) fname = fname.substring(1);
    
    if(!file.isDirectory() && fname.endsWith(".bin")) totalImages++;
    file.close();
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("Found %d images in LittleFS.\n", totalImages);
}

void triggerBuzzer(int times) {
  if (isBuzzerEnabled) {
    for (int i = 0; i < times; i++) {
      tone(BUZZER_PIN, 4000, 100); 
      delay(150);
    }
  }
}

void goToSleep(uint64_t sleepTime_ms, bool enableTimer) {
  epaper.sleep();
  WiFi.mode(WIFI_OFF);
  
  if (BUTTON_PRESSED == LOW) {
    rtc_gpio_pullup_en((gpio_num_t)BTN2_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN2_PIN);
    rtc_gpio_pullup_en((gpio_num_t)BTN3_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN3_PIN);
  }

  uint64_t wake_mask = (1ULL << BTN2_PIN) | (1ULL << BTN3_PIN);
  esp_sleep_enable_ext1_wakeup(wake_mask, (BUTTON_PRESSED == LOW) ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
  
  if (enableTimer) {
    esp_sleep_enable_timer_wakeup(sleepTime_ms * 1000ULL);
  }
  
  Serial.println("Entering Deep Sleep...");
  esp_deep_sleep_start();
}

void drawImage(int index) {
  epaper.begin();
  int rotation = 0; 
  
  if (totalImages > 0) {
    String filename = getFileNameByIndex(index);
    if (filename != "") {
      
      String pureName = filename;
      if (pureName.startsWith("/")) pureName = pureName.substring(1);
      
      int imgW = 128;
      int imgH = 296;
      
      if (LittleFS.exists("/meta.json")) {
        File fm = LittleFS.open("/meta.json", "r");
        JsonDocument meta;
        if (!deserializeJson(meta, fm)) {
          if (meta.containsKey(pureName)) {
            imgW = meta[pureName]["w"] | 128;
            imgH = meta[pureName]["h"] | 296;
          }
        }
        fm.close();
      }

      bool isFlipped = false; 
      if (LittleFS.exists("/flips.json")) {
        File f = LittleFS.open("/flips.json", "r");
        JsonDocument doc;
        if (!deserializeJson(doc, f)) {
          if (doc.containsKey(pureName)) {
            isFlipped = doc[pureName];
          }
        }
        f.close();
      }

      if (imgW == 296 && imgH == 128) rotation = 1; 
      else rotation = 0; 
      
      rotation = (rotation + 2) % 4;

      if (isFlipped) rotation = (rotation + 2) % 4; 

      epaper.setRotation(rotation);
      epaper.fillScreen(TFT_WHITE);
      
      File file = LittleFS.open(filename, "r");
      if(file) {
        uint8_t *imgBuffer = (uint8_t *)malloc(file.size());
        if (imgBuffer) {
          file.read(imgBuffer, file.size());
          epaper.pushImage(0, 0, imgW, imgH, (uint16_t *)imgBuffer); 
          free(imgBuffer);
        }
        file.close();
      }
    }
  } else {
    epaper.setRotation(3);
    epaper.fillScreen(TFT_WHITE);
    epaper.pushImage(0, 0, 296, 128, (uint16_t *)eChain_Image);
  }
  
  epaper.update();
}

void drawContactCard() {
  epaper.begin();
  epaper.setRotation(3); 
  epaper.fillScreen(TFT_WHITE);
  
  String name = "No Name Set";
  String phone = "No Phone Set";
  String email = "No Email Set";
  String custom = "";

  File file = LittleFS.open("/contact.json", "r");
  if (file) {
    JsonDocument doc; 
    if (!deserializeJson(doc, file)) {
      name = doc["name"].as<String>();
      phone = doc["phone"].as<String>();
      email = doc["email"].as<String>();
      custom = doc["custom"].as<String>();
    }
    file.close();
  }

  epaper.setTextColor(TFT_BLACK);
  epaper.setTextSize(1);
  epaper.drawString("-------------------------------------------------", 0, 0, 2);
  epaper.drawString("-------------------------------------------------", 0, 115, 2);

  epaper.pushImage(10, 32, 64, 64, (uint16_t *)contact_icon);

  epaper.setTextSize(2);
  epaper.setTextColor(TFT_RED);
  epaper.drawString(name, 85, 20, 2);
  
  epaper.setTextSize(1);
  epaper.setTextColor(TFT_BLACK);
  epaper.drawString(phone, 85, 55, 2);
  epaper.drawString(email, 85, 75, 2); 
  
  if (custom != "") {
    epaper.drawString(custom, 85, 95, 2); 
  }
  
  epaper.update();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ADC_EN_PIN, OUTPUT);
  digitalWrite(ADC_EN_PIN, LOW);
  
  if (BUTTON_PRESSED == LOW) {
    pinMode(BTN2_PIN, INPUT_PULLUP);
    pinMode(BTN3_PIN, INPUT_PULLUP);
  } else {
    pinMode(BTN2_PIN, INPUT_PULLDOWN);
    pinMode(BTN3_PIN, INPUT_PULLDOWN);
  }

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  countStoredImages();

  File file = LittleFS.open("/settings.json", "r");
  if (file) {
    JsonDocument doc;
    if (!deserializeJson(doc, file)) {
      slideShowInterval_ms = doc["timer"].as<int>() * 60000ULL;
      isBuzzerEnabled = doc["buzzer"].as<int>() == 1;
    }
    file.close();
  }

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();

  bool setupModeRequested = false;
  bool btn2_latched = false;
  bool btn3_latched = false;

  unsigned long bootTime = millis();
  while (millis() - bootTime < 750) {
    if (digitalRead(BTN2_PIN) == BUTTON_PRESSED) btn2_latched = true;
    if (digitalRead(BTN3_PIN) == BUTTON_PRESSED) btn3_latched = true;

    if (digitalRead(BTN2_PIN) == BUTTON_PRESSED && digitalRead(BTN3_PIN) == BUTTON_PRESSED) {
      setupModeRequested = true;
      break;
    }
    delay(10);
  }

  bool btn2_trigger = btn2_latched || (wakeup_pin_mask & (1ULL << BTN2_PIN));
  bool btn3_trigger = btn3_latched || (wakeup_pin_mask & (1ULL << BTN3_PIN));

  if (setupModeRequested) {
    isContactMode = false; 
    triggerBuzzer(2); 
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid, password);
    apStartTime = millis();

    int batPct = getBatteryPercentage();

    epaper.begin();
    epaper.setRotation(3); 
    epaper.fillScreen(TFT_WHITE);
    
    epaper.setTextColor(TFT_BLACK);
    epaper.setTextSize(1);
    epaper.drawString("-------------------------------------------------", 0, 0, 2);
    epaper.drawString("-------------------------------------------------", 0, 115, 2);
    
    epaper.pushImage(10, 32, 64, 64, (uint16_t *)setup_icon);
    
    epaper.setTextSize(2);
    epaper.setTextColor(TFT_RED);
    epaper.drawString("SETUP MODE", 85, 15, 2);
    
    epaper.setTextSize(1);
    epaper.setTextColor(TFT_BLACK);
    epaper.drawString("SSID: eChain Setup", 85, 45, 2);
    epaper.drawString("PASS: img@1234", 85, 65, 2);
    epaper.drawString("IP: 192.168.4.1", 85, 85, 2);
    
    epaper.setTextColor(TFT_RED);
    epaper.drawString("BATTERY: " + String(batPct) + "%", 85, 105, 2);
    epaper.update();

    server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
    
    server.on("/storage", HTTP_GET, []() {
      String json = "{\"total\":" + String(LittleFS.totalBytes()) + ",\"used\":" + String(LittleFS.usedBytes()) + ",\"battery\":" + String(getBatteryPercentage()) + "}";
      server.send(200, "application/json", json);
      apStartTime = millis();
    });

    server.on("/exit", HTTP_POST, []() {
      server.send(200, "text/plain", "Rebooting");
      delay(500);
      ESP.restart();
    });
    
    server.on("/get_contact", HTTP_GET, []() {
      if (LittleFS.exists("/contact.json")) {
        File f = LittleFS.open("/contact.json", "r");
        server.streamFile(f, "application/json");
        f.close();
      } else {
        server.send(200, "application/json", "{}");
      }
      apStartTime = millis();
    });
    
    server.on("/get_settings", HTTP_GET, []() {
      if (LittleFS.exists("/settings.json")) {
        File f = LittleFS.open("/settings.json", "r");
        server.streamFile(f, "application/json");
        f.close();
      } else {
        server.send(200, "application/json", "{}");
      }
      apStartTime = millis();
    });

    server.on("/list_images", HTTP_GET, []() {
      JsonDocument flips;
      if (LittleFS.exists("/flips.json")) {
        File ff = LittleFS.open("/flips.json", "r");
        deserializeJson(flips, ff);
        ff.close();
      }
      
      JsonDocument orderDoc;
      JsonArray orderArr;
      if (LittleFS.exists("/order.json")) {
         File fo = LittleFS.open("/order.json", "r");
         deserializeJson(orderDoc, fo);
         orderArr = orderDoc.as<JsonArray>();
         fo.close();
      }

      String json = "[";
      bool first = true;

      for (JsonVariant v : orderArr) {
         String fname = v.as<String>();
         if (LittleFS.exists("/" + fname)) {
            if(!first) json += ",";
            bool isFlipped = false; 
            if (flips.containsKey(fname)) {
                isFlipped = flips[fname];
            }
            json += "{\"name\":\"" + fname + "\",\"flip\":" + (isFlipped ? "true" : "false") + "}";
            first = false;
         }
      }

      File root = LittleFS.open("/");
      if(root) {
        File f = root.openNextFile();
        while(f) {
          String fname = String(f.name());
          if(fname.startsWith("/")) fname = fname.substring(1);
          if(!f.isDirectory() && fname.endsWith(".bin")) {
            bool found = false;
            for (JsonVariant v : orderArr) {
               if (v.as<String>() == fname) { found = true; break; }
            }
            if (!found) {
              if(!first) json += ",";
              bool isFlipped = false; 
              if (flips.containsKey(fname)) {
                  isFlipped = flips[fname];
              }
              json += "{\"name\":\"" + fname + "\",\"flip\":" + (isFlipped ? "true" : "false") + "}";
              first = false;
            }
          }
          f.close();
          f = root.openNextFile();
        }
        root.close();
      }
      json += "]";
      server.send(200, "application/json", json);
      apStartTime = millis();
    });
    
    server.on("/save_order", HTTP_POST, []() {
      File f = LittleFS.open("/order.json", "w");
      if(f) {
        f.print(server.arg("plain"));
        f.close();
      }
      server.send(200, "text/plain", "OK");
      apStartTime = millis();
    });

    server.on("/set_flip", HTTP_POST, []() {
      String img = server.arg("img");
      bool state = (server.arg("state") == "1");

      JsonDocument flips;
      if (LittleFS.exists("/flips.json")) {
        File ff = LittleFS.open("/flips.json", "r");
        deserializeJson(flips, ff);
        ff.close();
      }
      flips[img] = state;
      File fw = LittleFS.open("/flips.json", "w");
      if (fw) { serializeJson(flips, fw); fw.close(); }
      
      server.send(200, "text/plain", "OK");
      apStartTime = millis();
    });

    server.on("/delete_images", HTTP_POST, []() {
      JsonDocument doc;
      deserializeJson(doc, server.arg("plain"));
      JsonArray arr = doc.as<JsonArray>();
      
      JsonDocument orderDoc;
      JsonArray newOrder = orderDoc.to<JsonArray>();
      if (LittleFS.exists("/order.json")) {
        File f = LittleFS.open("/order.json", "r");
        JsonDocument oldOrder;
        if (!deserializeJson(oldOrder, f)) {
          for (JsonVariant v : oldOrder.as<JsonArray>()) {
             bool toDelete = false;
             for (JsonVariant dv : arr) {
                if (dv.as<String>() == v.as<String>()) toDelete = true;
             }
             if (!toDelete) newOrder.add(v.as<String>());
          }
        }
        f.close();
      }
      File fw = LittleFS.open("/order.json", "w");
      if (fw) { serializeJson(orderDoc, fw); fw.close(); }

      for(JsonVariant v : arr) {
        LittleFS.remove("/" + v.as<String>());
      }
      countStoredImages();
      server.send(200, "text/plain", "Deleted");
      triggerBuzzer(2);
      apStartTime = millis();
    });

    server.on("/delete_all_images", HTTP_POST, []() {
      bool deletedAny = true;
      while(deletedAny) {
        deletedAny = false;
        File root = LittleFS.open("/");
        if(root) {
          File f = root.openNextFile();
          while(f) {
            String fname = String(f.name());
            if(fname.startsWith("/")) fname = fname.substring(1);
            bool isBin = !f.isDirectory() && fname.endsWith(".bin");
            f.close();
            
            if(isBin) {
              LittleFS.remove("/" + fname);
              deletedAny = true;
              break; 
            }
            f = root.openNextFile();
          }
          root.close();
        }
      }
      LittleFS.remove("/flips.json");
      LittleFS.remove("/order.json");
      LittleFS.remove("/meta.json");
      countStoredImages();
      server.send(200, "text/plain", "Deleted All");
      triggerBuzzer(2);
      apStartTime = millis();
    });

    server.on("/upload", HTTP_POST, []() {
      String w = server.arg("w");
      String h = server.arg("h");
      String safeName = server.arg("name");
      
      if (w != "" && h != "" && safeName != "") {
        JsonDocument meta;
        if (LittleFS.exists("/meta.json")) {
          File f = LittleFS.open("/meta.json", "r");
          deserializeJson(meta, f);
          f.close();
        }
        meta[safeName]["w"] = w.toInt();
        meta[safeName]["h"] = h.toInt();
        File fw = LittleFS.open("/meta.json", "w");
        if (fw) { serializeJson(meta, fw); fw.close(); }
      }
      
      JsonDocument newOrder;
      JsonArray nArr = newOrder.to<JsonArray>();
      if (LittleFS.exists("/order.json")) {
         File f = LittleFS.open("/order.json", "r");
         JsonDocument old;
         if(!deserializeJson(old, f)) {
            for (JsonVariant v : old.as<JsonArray>()) {
               if (v.as<String>() != safeName) nArr.add(v.as<String>()); 
            }
         }
         f.close();
      }
      nArr.add(safeName);
      File fwo = LittleFS.open("/order.json", "w");
      if (fwo) { serializeJson(newOrder, fwo); fwo.close(); }

      server.send(200, "text/plain", "Chunk OK");
      apStartTime = millis(); 
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        uploadFile = LittleFS.open("/" + upload.filename, "w");
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
          uploadFile.close();
          countStoredImages();
          triggerBuzzer(2); 
        }
      }
    });

    server.on("/contact", HTTP_POST, []() {
      JsonDocument doc;
      doc["name"] = server.arg("name");
      doc["phone"] = server.arg("phone");
      doc["email"] = server.arg("email");
      doc["custom"] = server.arg("custom");
      File f = LittleFS.open("/contact.json", "w");
      if(f) { serializeJson(doc, f); f.close(); }
      server.send(200, "text/plain", "OK");
      apStartTime = millis();
      triggerBuzzer(2);
    });

    server.on("/settings", HTTP_POST, []() {
      JsonDocument doc;
      int timerVal = server.arg("timer").toInt();
      if (timerVal < 5) timerVal = 5; 
      doc["timer"] = String(timerVal);
      doc["buzzer"] = server.arg("buzzer");
      File f = LittleFS.open("/settings.json", "w");
      if(f) { serializeJson(doc, f); f.close(); }
      slideShowInterval_ms = timerVal * 60000ULL;
      isBuzzerEnabled = server.arg("buzzer").toInt() == 1;
      server.send(200, "text/plain", "OK");
      apStartTime = millis();
      triggerBuzzer(2);
    });

    server.begin();
    return; 
  }

  if (btn2_trigger) {
    triggerBuzzer(1); 
    isContactMode = !isContactMode; 
    
    if (isContactMode) {
      drawContactCard();
      goToSleep(slideShowInterval_ms, false); 
    } else {
      drawImage(currentImageIndex);
      goToSleep(slideShowInterval_ms, true); 
    }
  } 
  else if (btn3_trigger) {
    triggerBuzzer(1); 
    isContactMode = false; 
    if (totalImages > 1) {
      currentImageIndex++;
      if (currentImageIndex >= totalImages) currentImageIndex = 0;
    }
    drawImage(currentImageIndex);
    goToSleep(slideShowInterval_ms, true);
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    isContactMode = false; 
    if (totalImages > 1) {
      currentImageIndex++;
      if (currentImageIndex >= totalImages) currentImageIndex = 0;
    }
    drawImage(currentImageIndex);
    goToSleep(slideShowInterval_ms, true);
  }
  
  drawImage(currentImageIndex);
  goToSleep(slideShowInterval_ms, true);
}

void loop() {
  server.handleClient();
  if (millis() - apStartTime > AP_TIMEOUT) {
    triggerBuzzer(3);
    ESP.restart(); 
  }
}