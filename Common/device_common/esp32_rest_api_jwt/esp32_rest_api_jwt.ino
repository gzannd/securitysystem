#include <WiFi.h>
#include <AsyncTCP.h>
#include <AsyncJson.h> 
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "LittleFS.h"
#include <mbedtls/md.h>
#include "base64.hpp"
#include <mbedtls/aes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <ArduinoMqttClient.h>
#include <ezButton.h>
#include <RTClib.h>
#include "esp_timer.h"

#define CONFIG_DOC_SIZE 1000
#define MAX_LOG_FILE_SIZE 10000

#define VERY_SLOW_BLINK_OFF_INTERVAL 5000
#define VERY_SLOW_BLINK_ON_INTERVAL 50

#define SLOW_BLINK_OFF_INTERVAL 500 
#define SLOW_BLINK_ON_INTERVAL 50

#define MEDIUM_BLINK_OFF_INTERVAL 250 
#define MEDIUM_BLINK_ON_INTERVAL 250 

#define FAST_BLINK_ON_INTERVAL 100
#define FAST_BLINK_OFF_INTERVAL 100

#define BUTTON_GPIO 33
#define DEBOUNCE_DELAY 25 

#define SHORT_BUTTON_PRESS_INTERVAL 1000 
#define LONG_BUTTON_PRESS_INTERVAL 2000

// ===================== Constants & Globals =====================
const char* jwtSecret = "your-256-bit-secret";
const char* aesKey = "this_is_16_byte!";
const char* aesIV = "16_byte_iv_vector";
const char* INSUFFICIENT_STACK_JSON   = "{\"error\":\"insufficient stack\"}";
const char* INSUFFICIENT_BUFFER_JSON  = "{\"error\":\"insufficient stack for buffer\"}";
const char* BASE_FIRMWARE_DESCRIPTION = "ESP32 v1.0.0";
const char* validUsername = "admin";
const char* validPassword = "secret";
const char* AP_HUB_NAME_PREFIX = "ZANNDTECH_CFG ";
const char* DEFAULT_DEVICE_NAME = "No device name";
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

RTC_DS1307 rtc;

const int LED_PIN = 5;
int BLINK_SPEED = 0;
unsigned long ledOnMillis = 0;
unsigned long ledOffMillis = 0;
unsigned long ledMillis = 0;

unsigned long buttonPressStartMillis = 0;
unsigned long buttonPressDurationMillis = 0;

Preferences prefs;
AsyncWebServer server(80);

RTC_DATA_ATTR int bootCount = 0;  //Save boot count to RTC memory, which persists between reboots/deep sleep cycles.

String userDefinedName = "";
bool encryptionSupported = true;
String locationJson;
String wifiSsid;
String wifiPassword;
String wifiMacAddress;
StaticJsonDocument<CONFIG_DOC_SIZE> configuration; 
int ledOnInterval = 0;
int ledOffInterval = 0;
int blinkMode = 0;
int restartTimeMillis = 0;
bool isRTCAvailable = false; 

ezButton mainButton(BUTTON_GPIO); //Set up the main control button.

const int wifiConnectTimeoutMs = 15000;
const char* fallbackApPassword = "password";

// ===================== Utilities =====================
String timestamp() {
  time_t now = time(nullptr);
  char buf[20];
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  strftime(buf, sizeof(buf), "%F %T", &tmNow);
  return String(buf);
}

void setBlinkSpeed(int speed){
  switch(speed){
    case 0:
      //Turn LED off. 
      ledOnInterval = 0;
      ledOffInterval = 0;
      break;
    case 1:
      //Very slow blink. 
      ledOnInterval =  VERY_SLOW_BLINK_ON_INTERVAL;
      ledOffInterval = VERY_SLOW_BLINK_OFF_INTERVAL;
      break;
    case 2: 
      ledOnInterval =  SLOW_BLINK_ON_INTERVAL;
      ledOffInterval = SLOW_BLINK_OFF_INTERVAL;
      break;
    case 3:
      ledOnInterval =  MEDIUM_BLINK_ON_INTERVAL;
      ledOffInterval = MEDIUM_BLINK_OFF_INTERVAL;
      break;
    case 4: 
      ledOnInterval =  FAST_BLINK_ON_INTERVAL;
      ledOffInterval = FAST_BLINK_OFF_INTERVAL;
      break;
    default:
      ledOnInterval = 0;
      ledOffInterval = 0;
  }
}

bool checkStackForJson(size_t requiredBytes, const char* /*context*/ = nullptr) {
  const size_t safetyMargin = 512;
  size_t freeStack = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
  return freeStack >= (requiredBytes + safetyMargin);
}

bool checkStackForBuffer(size_t requiredBytes, const char* /*context*/ = nullptr) {
  const size_t safetyMargin = 512;
  size_t freeStack = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
  return freeStack >= (requiredBytes + safetyMargin);
}

#define CHECK_JSON_STACK(size, label, request) do { \
  if (!checkStackForJson(size, label)) { \
    (request)->send(500, "application/json", INSUFFICIENT_STACK_JSON); \
    return; \
  } \
} while(0)

#define CHECK_BUFFER_STACK(size, label, request) do { \
  if (!checkStackForBuffer(size, label)) { \
    (request)->send(500, "application/json", INSUFFICIENT_BUFFER_JSON); \
    return; \
  } \
} while(0)

void logEvent(const String& msg) {
  String fullMsg = "[" + timestamp() + "] " + msg + "\n";

  Serial.print(fullMsg);
  
  if (!LittleFS.begin(true)) return;
  
  File f = LittleFS.open("/logs.txt", FILE_APPEND);
  
  if (f) {
    if (f.size() > MAX_LOG_FILE_SIZE) {
      f.close();
      LittleFS.remove("/logs.txt");
      f = LittleFS.open("/logs.txt", FILE_APPEND);
    }
    f.print(fullMsg);
    f.close();
  }
}

// ===================== Crypto & JWT =====================
String encryptAES(const String& plainText) {
  size_t len = plainText.length();
  size_t paddedLen = ((len + 15) / 16) * 16;
  uint8_t input[256]; // guard (previous code used VLAs on stack)
  if (paddedLen > sizeof(input)) return String(""); // basic guard
  memset(input, 0, paddedLen);
  memcpy(input, plainText.c_str(), len);

  uint8_t iv[16];
  memcpy(iv, aesIV, 16);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, (const unsigned char*)aesKey, 128);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, input, input);
  mbedtls_aes_free(&aes);

  unsigned char output[512] = {0};
  encode_base64(input, paddedLen, output);
  return String((char*)output);
}

String decryptAES(const String& cipherTextBase64) {
  const int DECODED_SIZE = 512;
  unsigned char decoded[DECODED_SIZE] = {0};
  decode_base64((unsigned char*)cipherTextBase64.c_str(), decoded);

  // Use the full decoded buffer length (your original used a fixed size)
  size_t len = strlen((const char*)decoded);
  if (len % 16 != 0) {
    // Not a multiple of block size; return empty/error
    return String("");
  }

  uint8_t iv[16];
  memcpy(iv, aesIV, 16);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, (const unsigned char*)aesKey, 128);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, decoded, decoded);
  mbedtls_aes_free(&aes);

  return String((char*)decoded);
}

String base64UrlEncode(const String& input) {
  // Your original used a fixed 255 and ignored actual lengths; preserve spirit but safer
  unsigned char out[1024] = {0};
  encode_base64((uint8_t*)input.c_str(), input.length(), out);
  String encoded = (const char*)out;
  encoded.replace("+", "-");
  encoded.replace("/", "_");
  encoded.replace("=", "");
  return encoded;
}

String hmacSha256(const String& message, const char* secret) {
  unsigned char hmac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  char encoded[128] = {0};
  encode_base64(hmac, sizeof(hmac), (unsigned char*)encoded);
  String result(encoded);
  result.replace("+", "-");
  result.replace("/", "_");
  result.replace("=", "");
  return result;
}

String generateJWT(const String& username) {
  StaticJsonDocument<256> header, payload;
  header["alg"] = "HS256";
  header["typ"] = "JWT";

  time_t now = time(nullptr);
  payload["sub"] = username;
  payload["iat"] = now;
  payload["exp"] = now + 3600;

  String sHeader, sPayload;
  serializeJson(header, sHeader);
  serializeJson(payload, sPayload);

  String b64Header  = base64UrlEncode(sHeader);
  String b64Payload = base64UrlEncode(sPayload);
  String message    = b64Header + "." + b64Payload;
  String signature  = hmacSha256(message, jwtSecret);

  return message + "." + signature;
}

bool validateJWT(const String& token, JsonDocument& payloadOut) {
  int dot1 = token.indexOf('.');
  int dot2 = token.indexOf('.', dot1 + 1);
  if (dot1 < 0 || dot2 < 0) return false;

  String encodedHeader   = token.substring(0, dot1);
  String encodedPayload  = token.substring(dot1 + 1, dot2);
  String encodedSignature= token.substring(dot2 + 1);

  String signingInput    = encodedHeader + "." + encodedPayload;
  String expectedSignature = hmacSha256(signingInput, jwtSecret);
  if (expectedSignature != encodedSignature) return false;

  String payloadStr = encodedPayload;
  payloadStr.replace('-', '+');
  payloadStr.replace('_', '/');
  while (payloadStr.length() % 4) payloadStr += '=';

  unsigned char decoded[1024] = {0};
  decode_base64((unsigned char*)payloadStr.c_str(), decoded);

  String decodedPayload = (const char*)decoded;
  DeserializationError err = deserializeJson(payloadOut, decodedPayload);
  if (err) return false;

  if (payloadOut.containsKey("exp")) {
    time_t now = time(nullptr);
    if (now > payloadOut["exp"].as<time_t>()) return false;
  }
  return true;
}

// ===================== Auth helper (Async) =====================
bool authorizeRequest(AsyncWebServerRequest* request) {
  if (!request->hasHeader("Authorization")) return false;
  String authHeader = request->getHeader("Authorization")->value();
  if (!authHeader.startsWith("Bearer ")) return false;
  String token = authHeader.substring(7);
  StaticJsonDocument<512> payload;
  return validateJWT(token, payload);
}

// ===================== Data Helpers =====================
String getLocationJson() {
  return R"({
    "latitude": 40.7128,
    "longitude": -74.0060,
    "altitude": 15.2,
    "description": "Test Location"
  })";
}

// ===================== Route Handlers (Async) =====================
void handleRoot(AsyncWebServerRequest* request) {
  userDefinedName = String(DEFAULT_DEVICE_NAME);
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Device</title></head><body>";
  html += "<h1>";
  html += userDefinedName;
  html += "</h1>";
  html += "<p><strong>Firmware:</strong> " + String(BASE_FIRMWARE_DESCRIPTION) + "</p>";
  html += "<p><strong>MAC Address:</strong> " + wifiMacAddress + "</p>";
  html += "</body></html>";

  request->send(200, "text/html", html);
}

void handleGetLogs(AsyncWebServerRequest* request) {

}

void handleSetTime(AsyncWebServerRequest* request, const char* data) {
  //If the RTC hardware is on and present then set the time. 
  //Else return a 400. 
  bool error = false; 

  //Check presence of RTC hardware. 
  if (! rtc.begin()) {
    logEvent("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);

    //Return a 410 error. 
    request->send(410, "application/json", "{\"status\":\"Feature not available.\"}");
  }

  //Parse the request.
  //value parameter is assumed to be date/time formatted as 24h (military time).
  //YYYY-MM-DD HH:MM:SS

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  
  if (sscanf(data, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
    Serial.println("Date/Time Parsed Successfully:");
    Serial.print("Year: "); Serial.println(year);
    Serial.print("Month: "); Serial.println(month);
    Serial.print("Day: "); Serial.println(day);
    Serial.print("Hour: "); Serial.println(hour);
    Serial.print("Minute: "); Serial.println(minute);
    Serial.print("Second: "); Serial.println(second);

    logEvent("Setting date");
    
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
  } else {
    logEvent("Error parsing data.");
    error = true;
  }


/*
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  } else {
    //rtc.adjust(DateTime(2025, 12, 13, 18, 0, 0));
  }
*/

  if(error == true){
    request->send(400, "application/json", "{\"status\":\"Bad Request\"}");
  }
  else{
    request->send(200, "application/json", "{\"status\":\"OK\"}");
  }
}

void handleGetTime(AsyncWebServerRequest* request) {

}

void handleGetCapabilities(AsyncWebServerRequest* request){
  
}

void handleIdentity(AsyncWebServerRequest* request) {
  String mac = WiFi.macAddress();
  String uniqueId = String((uint32_t)ESP.getEfuseMac(), HEX);
  String json = "{";
  json += "\"unique_id\":\"" + uniqueId + "\",";
  json += "\"mac_address\":\"" + mac + "\",";
  json += "\"firmware\":\"" + String(BASE_FIRMWARE_DESCRIPTION) + "\",";
  json += "\"name\":\"" + userDefinedName + "\",";
  json += "\"encryption\":" + String(encryptionSupported ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json);
}

String jsonString = "[]";

void handleGetLocation(AsyncWebServerRequest* request) {
  request->send(200, "application/json", getLocationJson());
}

void handleSetLocation(AsyncWebServerRequest* request) {
  request->send(200, "application/json", getLocationJson());
}

void setRebootTimer(){
  if(restartTimeMillis == 0){
    restartTimeMillis = millis();
  }
}

void clearRebootTimer(){
  restartTimeMillis = 0;
}

void handleSoftReset(AsyncWebServerRequest* request) {
  logEvent("Performing soft reset.");
  request->send(200, "text/plain", "");
  setRebootTimer();  //Trigger a reboot by setting the restart time variable.
}

String processor(const String& var){
  Serial.println(var);
  if(var == "DEVICENAME"){
    return userDefinedName;
  }
  else if (var == "MACADDRESS"){
    return wifiMacAddress;
  }
  else if(var == "FIRMWAREVERSION"){
    return "0.01";
  }
  else if(var == "UPTIME"){
    return "10000";
  }
  else if(var == "SSIDS"){
    Serial.println("Returning jsonstring");
    return jsonString;
  }

  return String();
}

void handleFactoryReset(AsyncWebServerRequest* request) {
  if (!authorizeRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }
  prefs.begin("esp32-config", false);
  prefs.clear();
  prefs.end();
  request->send(200, "application/json", "{\"status\":\"factory reset\"}");
  delay(200);
  ESP.restart();
}

StaticJsonDocument<CONFIG_DOC_SIZE> createNullConfigDocument() {
  StaticJsonDocument<CONFIG_DOC_SIZE> doc; // Create a StaticJsonDocument with a buffer size
  doc.to<JsonVariant>().set(nullptr); // Set the document's type to null
  return doc;
}

StaticJsonDocument<CONFIG_DOC_SIZE> loadConfiguration(){
  StaticJsonDocument<CONFIG_DOC_SIZE> doc;
  
  int64_t uptimeInSeconds = esp_timer_get_time() / 1000000;
  char uptimeBuffer[32];
  snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%lld", uptimeInSeconds);

  prefs.begin("esp32-config", true);

  doc["deviceName"] = decryptAES(prefs.getString("deviceName", ""));
  doc["location"]  = decryptAES(prefs.getString("location", "")); //TODO: Build a sub-object here.
  doc["ssid"]      = decryptAES(prefs.getString("wifiSsid", ""));
  doc["password"]   = decryptAES(prefs.getString("wifiPassword", ""));
  doc["bootAttempts"] = decryptAES(String(prefs.getInt("bootAttempts", 0)));
  doc["connectAttempts"] = decryptAES(String(prefs.getInt("connectAttempts", 0)));
  doc["lastBootMode"] = decryptAES(prefs.getString("lastBootMode", ""));
  doc["ipAddress"] = WiFi.localIP();
  doc["macAddress"] = WiFi.softAPmacAddress();  //This only works in AP mode.
  doc["uptime"] = uptimeBuffer;
  doc["firmwareVersion"] = BASE_FIRMWARE_DESCRIPTION;
  prefs.end();
  return doc;
}

bool clearConfiguration(){
  prefs.begin("esp32-config", false);
  prefs.clear();
  prefs.end();
}

bool saveConfiguration(StaticJsonDocument<CONFIG_DOC_SIZE> doc){

  //Init and clear preferences
  prefs.begin("esp32-config", false);
  prefs.clear();

  String data = "";

  //Write the json document properties to preferences. 
  if (doc.containsKey("deviceName")) {
    data = doc["deviceName"].as<String>();
    prefs.putString("deviceName", encryptAES(data));
  }

  if(doc.containsKey("ssid")){
    data = doc["ssid"].as<String>();
    prefs.putString("wifiSsid", encryptAES(data));
  }

  if(doc.containsKey("password")){
    data = doc["password"].as<String>();
    prefs.putString("wifiPassword", encryptAES(data));
  }

  if(doc.containsKey("location")){
    data = doc["location"].as<String>();
  }
  else{
    data = R"({"latitude":40.7128,"longitude":-74.0060,"altitude":15.2,"description":"Test Location"})";
  }
  prefs.putString("location", encryptAES(data));

  if(doc.containsKey("bootAttempts")){
    prefs.putString("bootAttempts", encryptAES(doc["bootAttempts"]));
  }

  if(doc.containsKey("ssidConnectionAttempts")){
    prefs.putString("ssidConnectionAttempts", encryptAES(doc["ssidConnectionAttempts"]));
  }

  if(doc.containsKey("lastBootMode")){
    prefs.putString("lastBootMode", encryptAES(doc["lastBootMode"].as<String>()));
  }

  prefs.end();

  return true;
}

void listSPIFFSFiles() {
  Serial.println("\n--- Listing SPIFFS Files ---");
  
  // Open the root directory
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("- Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("- Not a directory");
    return;
  }

  // Iterate over all files
  File file = root.openNextFile();
  int fileCount = 0;
  
  while (file) {
    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print("\tSIZE: ");
    Serial.print(file.size());
    Serial.println(" bytes");
    
    fileCount++;
    file = root.openNextFile(); // Move to the next file
  }
  
  Serial.printf("Total files found: %d\n", fileCount);
  Serial.println("---------------------------\n");
}

// ===================== Routes Setup =====================
void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  server.on("/device-info.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/device-info.js", "text/javascript", false);
  });

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/config.html", "text/html", false, processor);
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/logs.txt", "text/plain", false);
  });

  server.on("/device/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    StaticJsonDocument<CONFIG_DOC_SIZE> doc = loadConfiguration();  

    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {

  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (request->url() == "/config") {
      StaticJsonDocument<CONFIG_DOC_SIZE> doc; // Adjust size as needed
      DeserializationError error = deserializeJson(doc, (const char *)data, len);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
        return;
      }

      // Access JSON elements
      String data = "";

      if (doc.containsKey("deviceName")) {
        data = doc["deviceName"].as<String>();
        Serial.print("Received deviceName: ");
        Serial.println(data);
      }

      if(doc.containsKey("ssid")){
        data = doc["ssid"].as<String>();
        Serial.print("Received ssid: ");
        Serial.println(data);
      }

      if(doc.containsKey("password")){
        data = doc["password"].as<String>();
        Serial.print("Received password: ");
        Serial.println(data);
      }

      //Pass the data along to be validated and saved.
      if(saveConfiguration(doc) == true){
        request->send(200, "application/json", "{\"status\":\"success\"}");
      } else {
        request->send(400, "application/json", "{\"status\":\"Bad Request\"}");
      }
    }
  });

  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/styles.css","text/css");
  });

  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    //Init a wifi network scan. 
    int networkCount = WiFi.scanNetworks();
    
    StaticJsonDocument<1000> doc;
    JsonObject root = doc.to<JsonObject>();
    JsonArray entries = doc.to<JsonArray>();

    for(int i=0;i<networkCount;i++){
      String ssid = WiFi.SSID(i);
      Serial.println(ssid);
      int rssi = WiFi.RSSI(i);
      int encryptionType = WiFi.encryptionType(i);   
      String encryptionTypeName = "";
      switch(encryptionType){
        case WIFI_AUTH_OPEN: encryptionTypeName = "OPEN";
        break;
        case WIFI_AUTH_WEP: encryptionTypeName = "WEP";
        break;
        case WIFI_AUTH_WPA_PSK: encryptionTypeName = "WPA/PSK";
        break;
        case WIFI_AUTH_WPA2_PSK: encryptionTypeName = "WPA2/PSK";
        break;
        case WIFI_AUTH_WPA_WPA2_PSK: encryptionTypeName = "WPA/WPA2/PSK";
        break;
        case WIFI_AUTH_WPA2_ENTERPRISE: encryptionTypeName = "WPA2 Enterprise";
        break;
        default: encryptionTypeName = "UNKNOWN";
      }
      
      JsonObject entry = entries.createNestedObject();
      entry["ssid"] = ssid;
      entry["rssi"] = rssi;
      entry["enctype"] = encryptionTypeName;
    }
    serializeJson(doc, jsonString);
    
    Serial.println(jsonString);
    request->send(200, "application/json", jsonString);
  });


  server.on("/clock", HTTP_PUT, [](AsyncWebServerRequest *request) {

  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
     String requestBody;

     if (index == 0) requestBody = "";
     
     requestBody += String((char*)data).substring(0, len);
     if (index + len == total) {
        handleSetTime(request, requestBody.c_str());
     }
  });


  server.on("/clock", HTTP_GET, handleGetTime);

  server.on("/identity", HTTP_GET, handleIdentity);

  server.on("/location", HTTP_GET, handleGetLocation);

  server.on("/location", HTTP_POST, handleSetLocation);

  server.on("/capabilities", HTTP_GET, handleGetCapabilities);

  server.on("/reset/soft", HTTP_POST, handleSoftReset);

  server.on("/reset/hard", HTTP_POST, handleFactoryReset);

  server.onNotFound([](AsyncWebServerRequest *request){
    String url = request->url();
    Serial.printf("404 Not Found: %s\n", url.c_str());
    request->send(404, "text/plain", "Not Found");
  });

  // /login via AsyncCallbackJsonWebHandler
  // Requires ArduinoJson
  AsyncCallbackJsonWebHandler* loginHandler = new AsyncCallbackJsonWebHandler("/login",
    [](AsyncWebServerRequest* request, JsonVariant &json) {
      JsonObject obj = json.as<JsonObject>();
      String username = obj["username"] | "";
      String password = obj["password"] | "";

      if (username != validUsername || password != validPassword) {
        request->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
        return;
      }
      String token = generateJWT(username);
      String response = String("{\"token\":\"") + token + "\"}";
      request->send(200, "application/json", response);
    }
  );
  server.addHandler(loginHandler);
}


// ===================== setup / loop =====================
void setup() {
  ++bootCount;
  prefs.begin("esp-32-bootmode", false);
  bool bootToApMode = prefs.getBool("bootToApMode", false);
  prefs.end();

  mainButton.setDebounceTime(DEBOUNCE_DELAY);

  Serial.begin(19200);

  pinMode(LED_PIN, OUTPUT);
  setBlinkSpeed(3);
  delay(1000); 

  logEvent("Starting up.");

  if(!rtc.begin()){
    logEvent("RTC is disabled or not available.");
    isRTCAvailable = false;
  }
  else 
  {
    isRTCAvailable = true;
  }

  if (!LittleFS.begin(true)) {
   logEvent("[ERROR] LittleFS mount failed");
  } 
  else 
  {
    Serial.println("Listing files...");
    listSPIFFSFiles();
  }

  logEvent("Checking configuration settings.");

  // Load encrypted settings
  configuration = loadConfiguration();

  Serial.print("bootToAPMode = ");
  Serial.println(bootToApMode);

  //If the configuration is null or does not contain a valid ssid, then go into AP mode. 
  if(configuration.isNull() 
      || configuration.containsKey("ssid") == false 
      || configuration["ssid"] == ""
      || bootToApMode == true) {

    logEvent("No valid configuration found or force boot to AP mode. Initializing AP mode.");

    //Reset the boot to AP mode flag.
    bootToApMode = false;
    prefs.begin("esp-32-bootmode", false);
    prefs.putBool("bootToApMode", false);
    prefs.end();

    // Always configure routes before server.begin()
    setupRoutes();

    setBlinkSpeed(3);

    logEvent("Initializing AP mode.");
    WiFi.mode(WIFI_AP);

    //Wait for 1 second to let the AP hardware finish initializing so we can 
    //get the MAC address.
    delay(1000);

    String macAddress = WiFi.softAPmacAddress();
    String apSsid = String("ZANNDTECH_CFG " + macAddress);

    WiFi.softAP(apSsid.c_str(), fallbackApPassword);
    
    logEvent("Starting AP webserver.");
    server.begin();
  } 
  else {
    //Attempt to connect using the config settings. 
    Serial.println("Valid configuration found. Initializing WiFi mode.");
    setBlinkSpeed(3);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(String(configuration["ssid"]).c_str(), String(configuration["password"]).c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiConnectTimeoutMs) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      setBlinkSpeed(1);

      wifiMacAddress = WiFi.macAddress();
      logEvent("[BOOT] Device started. IP: " + WiFi.localIP().toString());
    } 
    else {
      logEvent("[BOOT] WiFi connect failed. Rebooting to AP mode...");
      prefs.begin("esp-32-bootmode", false);
      prefs.putBool("bootToApMode", true);
      prefs.end();
      setRebootTimer();
    }
  }
}

void loop() {
  mainButton.loop();  //Call the EZButton loop handler.

  if(isRTCAvailable == true) {
    DateTime rtcnow = rtc.now();
    
    // Getting each time field in individual variables
    // And adding a leading zero when needed;
    String yearStr = String(rtcnow.year(), DEC);
    String monthStr = (rtcnow.month() < 10 ? "0" : "") + String(rtcnow.month(), DEC);
    String dayStr = (rtcnow.day() < 10 ? "0" : "") + String(rtcnow.day(), DEC);
    String hourStr = (rtcnow.hour() < 10 ? "0" : "") + String(rtcnow.hour(), DEC); 
    String minuteStr = (rtcnow.minute() < 10 ? "0" : "") + String(rtcnow.minute(), DEC);
    String secondStr = (rtcnow.second() < 10 ? "0" : "") + String(rtcnow.second(), DEC);
    String dayOfWeek = daysOfTheWeek[rtcnow.dayOfTheWeek()];

    // Complete time string
    String formattedTime = dayOfWeek + ", " + yearStr + "-" + monthStr + "-" + dayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;

    // Print the complete formatted time
    Serial.println(formattedTime);

    Serial.println();
  }
  unsigned long now = millis(); 

  //Check to see if the restart timer is not zero and 
  //has exceeded 1 second. If so, then we need to restart the device.
  if(restartTimeMillis > 0){
    if(now - restartTimeMillis > 1000){
      //Reboot.
      digitalWrite(LED_PIN, LOW);
      ESP.restart();
    }
  }
  else{
    //Check for a long button press. If a long button press is detected,
    //flash the LED quickly to indicate that to the user.
    if(buttonPressDurationMillis > LONG_BUTTON_PRESS_INTERVAL){
      setBlinkSpeed(4);
    }

    //If we're tracking a button press, accumulate milliseconds.
    if(buttonPressStartMillis > 0){
      buttonPressDurationMillis = now - buttonPressStartMillis;
    };

    //Manage button states. This is simple short/long click logic,
    //should implement a class to enable more complex logic such as 
    //double clicks. 

    //When the button is pressed, start tracking how long it's been pressed
    //by setting an accunulator to count the milliseconds after the press event.
    if(mainButton.isPressed()){
      buttonPressStartMillis = now;
      buttonPressDurationMillis = 0;
    }

    //When the button is released, check to see if it was released after a short press,
    //or a long press. Depending on the result, either reboot the device to AP mode, or 
    //soft reboot the device in its current mode.
    if(mainButton.isReleased()){
      if(buttonPressDurationMillis <= SHORT_BUTTON_PRESS_INTERVAL){
        Serial.println("Short press");
        setRebootTimer();
      }
      else {
        Serial.println("Long press");
        prefs.begin("esp-32-bootmode", false);
        prefs.putBool("bootToApMode", true);
        prefs.end();

        //A long press will force the device to restart without changing the boot mode.
        setRebootTimer();
        logEvent("Rebooting to AP mode...");

        setRebootTimer();
      }

      buttonPressDurationMillis = 0;
      buttonPressStartMillis = 0;
    }

    //Manage LED blink state. Eventually, this should be replaced with a 
    //class to keep the logic clean, and to allow more complex blink patterns. 
    //But this simple solution works for now.
    bool ledState = digitalRead(LED_PIN);

    if(ledState == HIGH){
      //LED is on. Determine if it's time to turn it off. 
      if(now - ledMillis > ledOnInterval){
        digitalWrite(LED_PIN, LOW);
        ledMillis = now;
      }
    }
    else{
      //LED is off. Determine if it's time to turn it on. 
      if(now - ledMillis > ledOffInterval){
        digitalWrite(LED_PIN, HIGH);
        ledMillis = now;
      }
    }
  }
}
