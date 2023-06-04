/****************************************************************************
*                                                                           *
*        Advanced Youtube Subscriber Display With Dual Timezone Clock       *                                                        
*                                                           
*        Features"
*        Ability to use multiple displays chained together using 
*        Brian Lough's HUB75 Libarary.
*        Uses Sprite Sheets for animated images.
*        Web Configurable IoT features.
*        
*        Code Written By Kent Andersen / Broken Circuit Ranch                                                         
*        Copyright 2023. Released under MIT Licence. 
*        
*                                                                  
*                                                                  
 ***************************************************************************/

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h> //
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <Fonts/FreeSansBold9pt7b.h> // pretty font
#include <lvgl.h> // graphics library for the graphics
#include "image.h" // additional animated sprite sheet.
#include "ytimage.h" // youtube play animated sprite sheet.
#include <TimeLib.h>
#include <Timezone.h>
#include <Preferences.h>
#include <AsyncTCP.h>  // https://github.com/me-no-dev/AsyncTCP
#include "ESPAsyncWebServer.h" // https://github.com/me-no-dev/ESPAsyncWebServer
#include <map>

const unsigned long DEBOUNCE_TIME = 400;  // Debounce time in milliseconds
unsigned long lastButtonPressTime = 0;
bool buttonReleased = false;
const char* ntpServer = "pool.ntp.org"; // Time Server
const char* ssid = nullptr;
const char* password = nullptr;
extern const lv_img_dsc_t catopt;
extern const uint8_t catopt_map[];
extern const lv_img_dsc_t ytimage;
extern const uint8_t ytimage_map[];
//Matix Panel Defines each panel x so many panels in string.
const int panelResX = 64;
const int panelResY = 32;
const int panel_chain = 2;
String channelID;
String wifiName;
String wifiPassword;
String channelName;
String apiKey;
String adminPassword;
String setupMode = "false";
uint16_t myBLACK;
uint16_t myGRAY;
uint16_t myRED;
uint16_t myGREEN;
uint16_t myBLUE;
uint16_t myWHITE;
String subscriberCount = "";
String text = "";
bool animationMode = true;
String api_key;
unsigned long lastSubscriberUpdateMillis = 0;
unsigned long lastAnimationUpdateMillis = 0;
bool clockDisplayActivated = false;
hw_timer_t *hw_timer = NULL;
char timezoneAcode[4]; // Change from String to char array
char timezoneBcode[4]; // Change from String to char array
// Define the time change rules for DST and standard time
Timezone *timezoneA = NULL;
Timezone *timezoneB = NULL;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer);
Preferences preferences;
time_t currentTime;
time_t lastUpdateTime;
AsyncWebServer server(80);
MatrixPanel_I2S_DMA *dma_display = nullptr;
const lv_img_dsc_t catopt_dsc = {
  {LV_IMG_CF_TRUE_COLOR, 0, 32, 544},
  17408 * LV_COLOR_SIZE / 8,
  catopt_map,
};
const lv_img_dsc_t ytimage_dsc = {
  {LV_IMG_CF_TRUE_COLOR, 0, 22, 22},
  484 * LV_COLOR_SIZE / 8,
  ytimage_map,
};
std::map<String, Timezone*> timeZones;
#define BUTTON_PIN 33 //reset button pin
#define RESET_TIME 1000  // 1 second
#define FACTORY_RESET_TIME 10000  // 10 seconds
#define BUFFER_TIME 100  // 100 milliseconds
volatile unsigned long buttonPressTime = 0;
volatile bool buttonPressed = false;
void IRAM_ATTR button_ISR() {
  if (!buttonPressed) {
    buttonPressTime = millis();
    buttonPressed = true;
  }
}

void syncTime() {
  timeClient.update();
  time_t utcTime = timeClient.getEpochTime();
  // Ensure that timezoneA is not NULL before using it
  if (timezoneA != NULL) {
    time_t customTimeA = timezoneA->toLocal(utcTime);
    setTime(timezoneA->toUTC(customTimeA));
  }
}

void factorySet() {
  // Set the ESP32 in Access Point (AP) mode
  WiFi.mode(WIFI_AP);

  // Configure the AP network settings
  const char* apSSID = "MyESP32AP";
  const char* apPassword = "password123";

  // Start the AP with the specified SSID and password
  WiFi.softAP(apSSID, apPassword);

  // Retrieve the IP address of the AP
  IPAddress apIP = WiFi.softAPIP();

  // Set up DHCP configuration
  IPAddress subnetMask(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnetMask);

  // Print the AP network details
  Serial.println("ESP32 is in setup mode");
  Serial.print("AP SSID: ");
  Serial.println(apSSID);
  Serial.print("AP Password: ");
  Serial.println(apPassword);
  Serial.print("AP IP address: ");
  Serial.println(apIP);

  // Set up the web server in setup mode
  server.on("/", HTTP_GET, handleRoot); // Root page handler for setup
  server.on("/save", HTTP_POST, handleSave); // Save configuration handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });
  server.begin();

  // Serve the setup web page
  Serial.println("Open the web browser and navigate to the AP IP address");
  Serial.println("to access the setup page and configure the device.");

  while (true) {
    // Check if the user has completed the setup process
    bool setupComplete = checkSetupComplete();
    if (setupComplete) {
      break; // Exit the setup mode
    }
  }

  // Set the setup mode flag to false
  //preferences.begin("config");
  //String setupMode = "false";
  //preferences.putString("setupMode", setupMode);
  //preferences.end();
  //delay(1000);
  // Reboot the ESP32 to start in default mode
  ESP.restart();
}

void handleRoot(AsyncWebServerRequest *request) {
  // Serve the configuration web page
  String html = "<html><body>";
  html += "<h1>Device Configuration</h1>";
  html += "<form method='post' action='/save'>";
  html += "<table>";
  html += "<tr><td><label for='wifiName'>WiFi Name:</label></td><td><input type='text' name='wifiName'></td></tr>";
  html += "<tr><td><label for='wifiPassword'>WiFi Password:</label></td><td><input type='password' name='wifiPassword'></td></tr>";
  // Add additional input fields for channel name, channel ID, and API key
  html += "<tr><td><label for='channelName'>Channel Name:</label></td><td><input type='text' name='channelName'></td></tr>";
  html += "<tr><td><label for='channelID'>Channel ID:</label></td><td><input type='text' name='channelID'></td></tr>";
  html += "<tr><td><label for='apiKey'>API Key:</label></td><td><input type='text' name='apiKey'></td></tr>";
  html += "<tr><td><label for='timezoneA'>Timezone 1:</label></td><td><input type='text' name='timezoneA'></td></tr>";
  html += "<tr><td><label for='timezoneB'>Timezone 2:</label></td><td><input type='text' name='timezoneB'></td></tr>";
  html += "<tr><td><label for='factoryReset'>Factory Reset:</label></td><td><input type='radio' name='factoryReset' value='on'></td></tr>";
  html += "</table>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "</body></html>";
  request->send(200, "text/html", html);
}


void handleSave(AsyncWebServerRequest *request) {
  // Process the form submission and save the preferences
  String wifiName = request->arg("wifiName");
  String wifiPassword = request->arg("wifiPassword");
  String channelName = request->arg("channelName");
  String channelID = request->arg("channelID");
  String apiKey = request->arg("apiKey");
  String timezoneA = request->arg("timezoneA");
  String timezoneB = request->arg("timezoneB");
  //reset eeprom flag for -setupmode
  String factoryReset = request->arg("factoryReset");
  // Check if factory reset option is selected
  if (factoryReset == "on") {
    // Perform the factory reset
    Serial.println("factory reset triggered.");
    factoryReseta();
   
    // Respond with a success message
    String html = "<html><body>";
    html += "<h1>Factory Reset</h1>";
    html += "<p>The device has been reset to factory defaults.</p>";
    html += "</body></html>";
    request->send(200, "text/html", html);
    return;
  }
preferences.end();
  // Store the preferences
  preferences.begin("config");
  preferences.putString("wifiName", wifiName);
  preferences.putString("wifiPassword", wifiPassword);
  preferences.putString("channelName", channelName);
  preferences.putString("channelID", channelID);
  preferences.putString("apiKey", apiKey);
  preferences.putString("timezoneA", timezoneA);
  preferences.putString("timezoneB", timezoneB);
  preferences.putBool("setupComplete", true);
  String setupMode1 = "false";  
  Serial.println("Before setting, setupMode is: " + preferences.getString("setupMode", setupMode1));
  preferences.putString("setupMode", setupMode1);
  Serial.println("After setting, setupMode is: " + preferences.getString("setupMode", setupMode1));
  preferences.end();
  // Display a success message
  String html = "<html><body>";
  html += "<h1>Configuration Saved</h1>";
  html += "<p>Device will reboot...</p>";
  html += "</body></html>";
  request->send(200, "text/html", html);

  // Delay for a short time to allow the response to be sent
  delay(1000);

  // Reboot the device
  ESP.restart();
}

bool checkSetupComplete() {
  preferences.begin("config", false);
  bool setupComplete = preferences.getBool("setupComplete", false);
  preferences.end();
  return setupComplete;
}

void defaultMode() {
preferences.begin("config");
String storedWifiName = preferences.getString("wifiName", "");
String storedWifiPassword = preferences.getString("wifiPassword", "");
preferences.end();

Serial.println("Stored WiFi Name: " + storedWifiName);
Serial.println("Stored WiFi Password: " + storedWifiPassword);

// Set up the device in client mode
Serial.println("Setting up Client mode");
WiFi.mode(WIFI_MODE_STA);
WiFi.begin(storedWifiName.c_str(), storedWifiPassword.c_str());

// Set a timeout for the WiFi connection
unsigned long wifiTimeout = millis();
unsigned long wifiTimeoutPeriod = 8000;  // Set timeout period to 5 seconds for example

// Wait for connection
while (WiFi.status() != WL_CONNECTED) {
  if (millis() - wifiTimeout > wifiTimeoutPeriod) {
    // WiFi connection timeout, check the reset button state
    if (digitalRead(BUTTON_PIN) == LOW) { 
      // Reset button pressed, perform the necessary action
      factoryReseta();
    }
    wifiTimeout = millis(); // Reset the timeout
  }
  delay(1000);
  Serial.println("Connecting to WiFi...");
  Serial.print("WiFi Name: ");
  Serial.println(storedWifiName);
  Serial.print("WiFi Password: ");
  Serial.println(storedWifiPassword);
}

// Connected to WiFi
Serial.println("Connected to WiFi");
Serial.print("SSID: ");
Serial.println(WiFi.SSID());
Serial.print("IP address: ");
Serial.println(WiFi.localIP());
  // Start the web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  // Display the configuration
  Serial.println("Web server started");
  Serial.println("Connect to the WiFi network and visit the IP address");
  Serial.println("to configure the device.");
}

void factoryReseta() {
  preferences.begin("config");
  String setupMode1 = "true";
  Serial.println("Before setting, setupMode is: " + preferences.getString("setupMode", setupMode1));
  preferences.remove("setupMode");
  String setupModeNew = "true";
  preferences.putString("setupMode", setupModeNew);
  Serial.println("After setting, setupMode is: " + preferences.getString("setupMode", setupModeNew));
  Serial.print("Reset");
  preferences.end();
  delay(1000);
  // Reboot the device
  ESP.restart();
}

void displaySetup() {
  HUB75_I2S_CFG mxconfig(
    panelResX,   // module width
    panelResY,   // module height
    panel_chain    // Chain length
  );
  mxconfig.gpio.e = 18;
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setFont(&FreeSansBold9pt7b);
  dma_display->setPanelBrightness(50);

  // Initialize color variables after initializing the DMA display
  myBLACK = dma_display->color565(0, 0, 0);
  myGRAY = dma_display->color565(16, 16, 16); // Change to a dim gray color
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);
  myWHITE = dma_display->color565(255, 255, 255);
}

void displaySubscriberCount() {
  preferences.begin("config");
  String storedchannelName = preferences.getString("channelName", "");
  preferences.end();
  dma_display->setTextSize(1);
  //Screen Centering
  int16_t x, y;
  uint16_t w, h;
  dma_display->getTextBounds(text, 0, 0, &x, &y, &w, &h);
  int16_t x_pos = (dma_display->width() - w) / 2;
  int16_t y_pos = (dma_display->height() - h) / 2;

  // Clear the screen by filling it with white pixels
  dma_display->fillScreen(myWHITE);

  //Drop Shadow
  dma_display->setTextColor(dma_display->color565(0, 0, 75)); // Set the shadow color
  dma_display->setCursor(23 + 1, 13 + 1); // Offset the position
  dma_display->print(storedchannelName);

  //Text
  dma_display->setTextColor(dma_display->color565(0, 0, 139));
  dma_display->setCursor(23, 13);
  dma_display->print(storedchannelName);

  dma_display->setTextColor(dma_display->color565(0, 0, 75)); // Set the shadow color
  dma_display->setCursor(23 + 1, 30 + 1); // Offset the position
  dma_display->print(subscriberCount);
  dma_display->setTextColor(dma_display->color565(0, 0, 139));
  dma_display->setCursor(23, 30);
  dma_display->print(subscriberCount);
 
}

void updateSubscriberCountDisplay() {
  preferences.begin("config");
  String storedchannelID = preferences.getString("channelID", "");
  String storedapiKey = preferences.getString("apiKey", "");
  preferences.end();
  
  // Make API request
  String url = "https://www.googleapis.com/youtube/v3/channels?part=statistics&id=" + storedchannelID + "&key=" + storedapiKey;
  HTTPClient http;
  http.begin(url);
  int http_code = http.GET();
  if (http_code == HTTP_CODE_OK) {
    // Parse JSON response
    String json_str = http.getString();
    DynamicJsonDocument json_doc(1024);
    DeserializationError error = deserializeJson(json_doc, json_str);

    if (!error) {
      long subscriber_count = json_doc["items"][0]["statistics"]["subscriberCount"].as<long>();
      Serial.println("Subscriber count: " + String(subscriber_count));

      // Update subscriber count
      subscriberCount = (String(subscriber_count));

      // Display subscriber count on matrix display
      displaySubscriberCount();
    } else {
      Serial.println("JSON parsing failed");
    }
  } else {
    Serial.println("HTTP GET failed");
  }
  http.end();
}

void displayClock() {
  dma_display->fillScreen(myBLACK);

  preferences.begin("config");
  String storedtimezoneA = preferences.getString("timezoneA", "");
  String storedtimezoneB = preferences.getString("timezoneB", "");
  //preferences.end();

  // Get the current time in UTC
  time_t currentTime = now();

  // Calculate and display the local time for timezoneA
  if (timezoneA != NULL) {
    time_t localTimeA = timezoneA->toLocal(currentTime);

    char customTimeAStr[9];
    snprintf(customTimeAStr, sizeof(customTimeAStr), "%02d:%02d", hour(localTimeA), minute(localTimeA));

//   Uncomment for 12 hour clock instead of 24 hour clock. 

//    int hour12Format = hour(localTimeA) % 12;  
//    if (hour12Format == 0) hour12Format = 12;  
//     snprintf(customTimeAStr, sizeof(customTimeAStr), "%02d:%02d", hour12Format, minute(localTimeA)); 

    dma_display->setTextColor(myGREEN);
    dma_display->setCursor(15, 12);
    dma_display->print(storedtimezoneA);
    dma_display->setTextColor(myWHITE);
    dma_display->setCursor(10, 31);
    dma_display->print(customTimeAStr);
  }

  // Calculate and display the local time for timezoneB
  if (timezoneB != NULL) {
    time_t localTimeB = timezoneB->toLocal(currentTime);

    char customTimeBStr[9];
    snprintf(customTimeBStr, sizeof(customTimeBStr), "%02d:%02d", hour(localTimeB), minute(localTimeB));

//   Uncomment for 12 hour clock instead of 24 hour clock. 
//
//    int hour12Format = hour(localTimeB) % 12;  // Convert to 12-hour format
//    if (hour12Format == 0) hour12Format = 12;  // Handle the case of 00 as 12
//     snprintf(customTimeBStr, sizeof(customTimeBStr), "%02d:%02d", hour12Format, minute(localTimeB));

    dma_display->setTextColor(myGREEN);
    dma_display->setCursor(75, 12);
    dma_display->print(storedtimezoneB);
    dma_display->setTextColor(myWHITE);
    dma_display->setCursor(73, 31);
    dma_display->print(customTimeBStr);
  }
}

void animate() {
  static uint8_t frame = 0;
  const uint8_t *catopt_map = (const uint8_t *)catopt_dsc.data;

  // Calculate the offset for the current frame
  uint16_t y_offset = frame * 18;

  // Draw the image on the matrix display
  for (int16_t y = 0; y < 18; y++) {
    for (int16_t x = 0; x < 24; x++) {
      uint16_t index = ((y_offset + y) * 24 + x) * 4; // 4 bytes per pixel (8-bit each for R, G, B, and unused)
      uint8_t b = catopt_map[index + 0]; // Blue component
      uint8_t g = catopt_map[index + 1]; // Green component
      uint8_t r = catopt_map[index + 2]; // Red component
      dma_display->drawPixel(x + (dma_display->width() - 32), y, dma_display->color565(r, g, b));
    }
  }

  // Move to the next frame
  frame = (frame + 1) % 19; // 1 to 17 frames
}

void animate1() {
  static uint8_t frame1 = 0;
  const uint8_t *ytimage_map = (const uint8_t *)ytimage_dsc.data;

  // Calculate the offset for the current frame
  uint16_t y1_offset = frame1 * 18;

  // Draw the image on the matrix display
  for (int16_t y = 0; y < 18; y++) {
    for (int16_t x = 0; x < 24; x++) {
      uint16_t index = ((y1_offset + y) * 24 + x) * 4; // 4 bytes per pixel (8-bit each for R, G, B, and unused)
      uint8_t b = ytimage_map[index + 0]; // Blue component
      uint8_t g = ytimage_map[index + 1]; // Green component
      uint8_t r = ytimage_map[index + 2]; // Red component
      dma_display->drawPixel(x, y + 7, dma_display->color565(r, g, b));
    }
  }

  // Move to the next frame
  frame1 = (frame1 + 1) % 19; // 1 to 19 frames
}

void splash() {
  dma_display->setPanelBrightness(35);
  dma_display->fillScreen(myBLACK);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 13); // Offset the position
  dma_display->print("Broken Circuit");
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 27); // Offset the position
  dma_display->print("Ranch (c)2023");
  delay(4000);
  dma_display->fillScreen(myBLACK);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 13); // Offset the position
  dma_display->print("IP Address");
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 27); // Offset the position
  dma_display->print(WiFi.localIP());
  delay(4000);
}

void setup() {
  Serial.begin(115200);
  displaySetup();
  // Set up button pin as an input with an internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Attach interrupt handler to the button pin, to be called on falling edge (button press)
  attachInterrupt(BUTTON_PIN, button_ISR, FALLING);

  preferences.begin("config", false);
  String inSetupMode = preferences.getString("setupMode", "true");  // Retrieve the flag
  Serial.print("Setup mode flag: ");
  Serial.println(inSetupMode);

if (inSetupMode == "true") {
  // Device is in setup mode
  splash();
  dma_display->fillScreen(myBLACK);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 12); // Offset the position
  dma_display->print("Setup");
  dma_display->setCursor(0, 30); // Offset the position
  dma_display->print("Mode");
  preferences.putBool("setupComplete", false);
  factorySet(); // This now clearly refers to a function
} else {
  // Device is in default mode
  defaultMode();
}
  preferences.end(); 
  splash();
// TIMEZONE DEFINES: Add or remove timezones if you live in one of the 80 different ones than listed.
// Eastern Standard Time (EST)
TimeChangeRule EST_DST = {"EDT", Second, Sun, Mar, 2, -240}; // Daylight saving time starts 2nd Sunday in March
TimeChangeRule EST_STD = {"EST", First, Sun, Nov, 2, -300};  // Standard time starts 1st Sunday in November
Timezone* EST = new Timezone(EST_DST, EST_STD);
timeZones["EST"] = EST;

// Central Standard Time (CST)
TimeChangeRule CST_DST = {"CDT", Second, Sun, Mar, 2, -300}; // Daylight saving time starts 2nd Sunday in March
TimeChangeRule CST_STD = {"CST", First, Sun, Nov, 2, -360};  // Standard time starts 1st Sunday in November
Timezone* CST = new Timezone(CST_DST, CST_STD);
timeZones["CST"] = CST;

// Mountain Standard Time (MST)
TimeChangeRule MST_DST = {"MDT", Second, Sun, Mar, 2, -360}; // Daylight saving time starts 2nd Sunday in March
TimeChangeRule MST_STD = {"MST", First, Sun, Nov, 2, -420};  // Standard time starts 1st Sunday in November
Timezone* MST = new Timezone(MST_DST, MST_STD);
timeZones["MST"] = MST;

// Pacific Standard Time (PST)
TimeChangeRule PST_DST = {"PDT", Second, Sun, Mar, 2, -420}; // Daylight saving time starts 2nd Sunday in March
TimeChangeRule PST_STD = {"PST", First, Sun, Nov, 2, -480};  // Standard time starts 1st Sunday in November
Timezone* PST = new Timezone(PST_DST, PST_STD);
timeZones["PST"] = PST;

// Coordinated Universal Time (UTC)
// Since UTC does not have daylight saving time, we only need one rule
TimeChangeRule UTC_RULE = {"UTC", Last, Sun, Mar, 1, 0}; 
Timezone* UTC = new Timezone(UTC_RULE, UTC_RULE);
timeZones["UTC"] = UTC;
preferences.begin("config");
String storedtimezoneA = preferences.getString("timezoneA", "");
String storedtimezoneB = preferences.getString("timezoneB", "");
preferences.end();

if (!storedtimezoneA.isEmpty() && timeZones.find(storedtimezoneA) != timeZones.end()) {
    timezoneA = timeZones[storedtimezoneA];
}
if (!storedtimezoneB.isEmpty() && timeZones.find(storedtimezoneB) != timeZones.end()) {
    timezoneB = timeZones[storedtimezoneB];
}
// timezone maping; Add or change timezones if you live in a different one, must have match in 
// timezone define above.
std::map<String, Timezone*> timeZones = {
    {"EST", EST},
    {"CST", CST},
    {"MST", MST},
    {"PST", PST},
    {"UTC", UTC}
};


  Serial.println("Connecting to WiFi");
  dma_display->fillScreen(myBLACK);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 12); // Offset the position
  dma_display->print("WIFI");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
    dma_display->fillScreen(myBLACK);
    dma_display->setTextColor(myBLUE);
    dma_display->setCursor(0, 12); 
    dma_display->print("Connecting..");
  }
  Serial.println("Connected to WiFi");
  dma_display->fillScreen(myBLACK);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 12); 
  dma_display->print("Connected.");
  syncTime(); // Synchronize the time
  Serial.println("UDP Time Sync");
  dma_display->fillScreen(myBLACK);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(0, 12); 
  dma_display->print("Syncing Time");

  // Web Server
  // Set up the web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  dma_display->fillScreen(myWHITE);
  
  // Update the subscriber count display using the channel ID
  updateSubscriberCountDisplay();
  
  // Set the initial screen mode
  if (currentTime == 0) {
    animationMode = true; // Start with the animation screen if time is not synchronized yet
  } else {
    animationMode = false; // Start with the clock screen if time is already synchronized
  }

  // Set up animation timer to call onTimer function every 100ms
  hw_timer = timerBegin(0, 80, true);
  timerAlarmWrite(hw_timer, 100000, true);
  timerAlarmEnable(hw_timer);
}

void loop() {
  //server.handleClient();
  // Update the current time
  currentTime = timeClient.getEpochTime();
  // Display Loop: These define how long each screen displays and does the animations. 
  // Clock Screen (30 seconds)
  if (!animationMode) {
    if (!clockDisplayActivated) {
      displayClock(); // Update clock display
      clockDisplayActivated = true;
      lastUpdateTime = millis(); // Reset the update time
    }

    if (millis() - lastUpdateTime >= 30000) {
      lastUpdateTime = millis();
      displaySubscriberCount(); // Display subscriber count once
      animationMode = true; // Switch to animation screen
      lastAnimationUpdateMillis = millis(); // Reset animation timer
    }
  }
  // Animation Screen (30 seconds)
  else {
    if (millis() - lastAnimationUpdateMillis >= 100) {
      lastAnimationUpdateMillis = millis();
      animate(); // Play animation
      animate1(); // Display the custom animation on the left side
    }

    if (millis() - lastUpdateTime >= 30000) {
      lastUpdateTime = millis();
      animationMode = false; // Switch to clock screen
      clockDisplayActivated = false; // Reset clock display flag
    }
  }

  // Update the subscriber count every 5 minutes in clock screen mode
  if (!animationMode && millis() - lastSubscriberUpdateMillis >= 300000) {
    lastSubscriberUpdateMillis = millis();
    updateSubscriberCountDisplay(); // Update the subscriber count display using the channel ID
    currentTime = timeClient.getEpochTime(); // Update the current time after subscriber count update
  }

  // Reset millisecond counters to prevent overflow
  if (millis() - lastUpdateTime >= 60000) {
    lastUpdateTime = millis();
  }
  if (millis() - lastAnimationUpdateMillis >= 30000) {
    lastAnimationUpdateMillis = millis();
  }
  if (millis() - lastSubscriberUpdateMillis >= 300000) {
    lastSubscriberUpdateMillis = millis();
  }

  
  // Check for button press
  if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) {
    // Button has been pressed, start tracking press time
    buttonPressed = true;
    buttonPressTime = millis();
  }
  else if (digitalRead(BUTTON_PIN) == HIGH && buttonPressed) {
    // Button has been released
    unsigned long pressDuration = millis() - buttonPressTime;
    
    // Check how long the button was pressed and act accordingly
    if (pressDuration >= (RESET_TIME + BUFFER_TIME) && pressDuration < FACTORY_RESET_TIME) {
      // The button has been pressed for more than 1 second + buffer time, perform a reset
      Serial.println("Triggering reset...");
      ESP.restart();
      buttonPressed = false;
    }
    else if (pressDuration >= FACTORY_RESET_TIME) {
      // The button has been pressed for more than 10 seconds, perform a factory reset
      Serial.println("Triggering factory reset...");
      factoryReseta();
      buttonPressed = false;
    }
  }

}
