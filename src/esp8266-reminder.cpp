// NOTE: CURRENTLY ONLY TESTED ON ESP8266 AND NOT ON ESP32
// BECAUSE ESP32 HAS ISSUES WITH HTTPS REDIRECTS, SEE:
// https://github.com/espressif/arduino-esp32/issues/7069
// TO MAKE THIS WORK ON ESP32 EVENTUALLY, MORE WORK AND TESTING WILL BE NEEDED

#include <Arduino.h>
#include <SPI.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#if CONFIG_IDF_TARGET_ESP32
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#include <Preferences.h>
Preferences preferences;
#endif

#if CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#endif

#if CONFIG_IDF_TARGET_ESP32
#include <esp_attr.h>
#include <esp_sleep.h>
#endif

#if CONFIG_IDF_TARGET_ESP32
#include <HTTPUpdateServer.h>
#else
#include <ESP8266HTTPUpdateServer.h>
#endif

#if CONFIG_IDF_TARGET_ESP32
#include <WebServer.h>
#else
#include <ESP8266WebServer.h>
#endif

#include <ArduinoJson.h>

#if CONFIG_IDF_TARGET_ESP32
#include <HTTPClient.h>
#else
#include <ESP8266HTTPClient.h>
#endif

#include "HTTPSRedirect.h"

HTTPSRedirect *client = nullptr;

WiFiServer telnetServer(23);
WiFiClient serverClient;

WiFiManager wifiManager;

#include <WiFiUdp.h>
WiFiUDP udp;

#include <Timezone.h>

#include <NTPClient.h>

// This is hardcoded for Germany.
// Germany observes DST from the last Sunday in March to the last Sunday in
// October.
// TODO: Make this configurable via the web interface.
NTPClient timeClient(udp, "pool.ntp.org",
                     0); // NTP client object
TimeChangeRule CEST = {"CEST", Last, Sun,
                       Mar,    2,    120}; // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun,
                      Oct,    3,    60}; // Central European Standard Time
Timezone timezone(CEST, CET);            // Timezone object for Germany

// HTTP Server
#if CONFIG_IDF_TARGET_ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

// Display
#include <LCD_I2C.h>
LCD_I2C lcd(0x27, 16,
            2); // Default address of most PCF8574 modules, change accordingly

// Buzzer or speaker
// #include <ESP8266RTTTLPlus.h>
// #define BUZZER_PIN D5

// Audio, https://github.com/earlephilhower/ESP8266Audio
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorRTTTL.h"
#include "AudioOutputI2S.h"
AudioGeneratorRTTTL *rtttl;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;

// Timer
#include <Ticker.h>
Ticker timer;
Ticker clock_timer;
Ticker reboot_timer;

// OTA update via web interface upload
#if CONFIG_IDF_TARGET_ESP32
HTTPUpdateServer httpUpdater;
#else
ESP8266HTTPUpdateServer httpUpdater;
#endif

String lines[128];
int line_count = 0;

// Prevent from all devices rebooting at the same time
int randomized_seconds = 0;

String device_name = "Reminder";
String event_title = "";
String api = "";
String ringtone = "Entertainer:d=4,o=5,b=140:8d,8d#,8e,c6,8e,c6,8e,2c.6,8c6,"
                  "8d6,8d#6,8e6,8c6,8d6,e6,8b,d6,2c6,p,8d,8d#,8e,c6,8e,c6,8e,"
                  "2c.6,8p,8a,8g,8f#,8a,8c6,e6,8d6,8c6,8a,2d6";
int alarm_volume = 11; // 1-100
int alarm_repeat_seconds = 30;

// Define the variables to check if the button has been pressed
static bool button_pressed = false;
static unsigned long button_pressed_time = 0;

const int BUTTON_PIN =
    0; // GPIO0 has a button attached to it on the WeMos NodeMCU

bool use_deep_sleep = false;
unsigned long sleep_timer_begin_time =
    0; // Time at which the sleep should begin in milliseconds from the start of
       // the program; automatically set by deepSleep()
unsigned int deep_sleep_millis =
    1 * 60 * 1000; // After this many minutes, the ESP32 will go to deep sleep;
                   // can be set by the user via the web interface

inline void println(const String line) {
  if (serverClient &&
      serverClient.connected()) // send data to telnet client if connected
    serverClient.println(line);
  Serial.println(line);
}

inline void print(const String line) {
  if (serverClient &&
      serverClient.connected()) // send data to telnet client if connected
    serverClient.print(line);
  Serial.print(line);
}

const char *extractHostFromURL(const char *url) {
  int len = strlen(url);
  int start = 0;
  int end = 0;
  for (int i = 0; i < len - 1; i++) {
    if (url[i] == '/' && url[i + 1] == '/') {
      start = i + 2;
      break;
    }
  }
  for (int i = start; i < len; i++) {
    if (url[i] == '/') {
      end = i;
      break;
    }
  }
  char *host = new char[end - start + 1];
  memcpy(host, &url[start], end - start);
  host[end - start] = '\0';
  return host;
}

const char *extractPathFromURL(const char *url) {
  int len = strlen(url);
  int start = 0;
  for (int i = 0; i < len - 1; i++) {
    if (url[i] == '/' && url[i + 1] == '/') {
      start = i + 2;
      break;
    }
  }
  for (int i = start; i < len; i++) {
    if (url[i] == '/') {
      start = i;
      break;
    }
  }
  return &url[start];
}

void handleRoot() {
  println(F("/ requested"));
  // Simple html page with buttons to control the radio
  String html = "<!DOCTYPE html><html><head><title>" + device_name +
                "</title></head><body><center><h1>" + device_name + "</h1>\n";
  html += "<script>\n";
  html += "function send(url) { var xhttp = new XMLHttpRequest(); "
          "xhttp.open('GET', url, true); xhttp.send(); }\n";
  html += "function play(url) { var xhttp = new XMLHttpRequest(); "
          "xhttp.open('POST', '/play_url', true); "
          "xhttp.setRequestHeader('Content-Type', "
          "'application/x-www-form-urlencoded'); xhttp.send('url=' + "
          "encodeURIComponent(url)); }\n";
  html += "function play_station_id(station_id) { var xhttp = new "
          "XMLHttpRequest(); xhttp.open('POST', '/play_station_id', true); "
          "xhttp.setRequestHeader('Content-Type', "
          "'application/x-www-form-urlencoded'); xhttp.send('station_id=' + "
          "encodeURIComponent(station_id)); }\n";
  html += "</script>\n";
  html += "<p>\n";
  html += "<button onclick='send(\"/reboot\")'>Reboot</button>\n";
  html += "<p>\n";

  html += "</p>";
  html += "<p><a href='/config'>Configuration</a> | <a href='/update'>Update "
          "</a></p>";
#if defined(GIT_IDENT)
  html += "<p>" + String(GIT_IDENT) + "</p>";
#endif

  html += "</center></body></html>";
  server.send(200, "text/html", html);
}

void updateSleepTime() {
  // Call this function whenever the sleep time should start over again
  println("Prolonging the sleep time");
  sleep_timer_begin_time = millis() + (deep_sleep_millis);
}

void deepSleep(int minutes) {
  deep_sleep_millis = minutes * 60 * 1000;
  println("Entering deep sleep in " + String(minutes) + " minutes");
  updateSleepTime();
  use_deep_sleep = true;
}

void parseConfigurationData() {

  // Retrieve the stored multi-line string from NVS
  String storedData = preferences.getString("data", "");
  println("storedData:");
  println(storedData);

  for (int i = 0; i < storedData.length(); i++) {
    if (storedData[i] == '\n') {
      line_count++;
    } else {
      lines[line_count] += storedData[i];
    }
  }
  print("line_count: ");
  println(String(line_count));

  for (int i = 0; i <= line_count; i++) {
    if (lines[i].startsWith("api ")) {
      print("api: ");
      String api = lines[i].substring(4);
      api.trim();
      println(api);
      preferences.putString("api", api);
    } else if (lines[i].startsWith("ringtone ")) {
      print("ringtone: ");
      String ringtone = lines[i].substring(9);
      ringtone.trim();
      println(ringtone);
      preferences.putString("ringtone", ringtone);
    } else if (lines[i].startsWith("sleep ")) {
      print("Sleep: ");
      String minutes = lines[i].substring(6);
      minutes.trim();
      println(minutes);
      int m = minutes.toInt();
      if (m > 0) {
        deepSleep(m);
        use_deep_sleep = true;
      }
    } else if (lines[i].startsWith("name ")) {
      print("Name: ");
      String name = lines[i].substring(5);
      name.trim();
      println(name);
      device_name = name;
    } else if (lines[i].startsWith("volume ")) {
      print("Volume: ");
      String volume = lines[i].substring(7);
      volume.trim();
      println(volume);
      int v = volume.toInt();
      if (v >= 1 && v <= 100) {
        preferences.putInt("alarm_volume", v);
      }
    } else if (lines[i].startsWith("repeat ")) {
      print("Repeat: ");
      String repeat = lines[i].substring(7);
      repeat.trim();
      println(repeat);
      int r = repeat.toInt();
      if (r >= 0 && r <= 3600) {
        preferences.putInt("alarm_repeat_seconds", r);
      }
    }
  }
}

void handleConfig() {
  println("/config requested");
  String html = "<html><body>";
  html += "<form action='/update_config' method='post'>";
  html += "<textarea rows='40' cols='80' name='multiline'>";
  html += preferences.getString("data", "");
  html += "</textarea><br>";
  html += "<input type='submit' value='Update'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void reboot() {
  // Reboot
#if CONFIG_IDF_TARGET_ESP32
  esp_restart();
#else
  ESP.restart();
#endif
}

void updateConfig() {
  println(F("/update_config requested"));
  String data = server.arg("multiline");

  // Store the multi-line string in NVS
  preferences.putString("data", data);

  static const char successResponse[] PROGMEM =
      "<META http-equiv=\"refresh\" content=\"1;URL=/\">Configuration updated! "
      "Rebooting...";
  server.send(200, "text/html", successResponse);

  reboot();
}

void write_stationName(String sName) { println(sName); }

void write_streamTitle(String sTitle) { println(sTitle); }

void loopTelnet() {
  if (telnetServer.hasClient() &&
      (!serverClient || !serverClient.connected())) {
    if (serverClient)
      serverClient.stop();
    serverClient = telnetServer.available();
    serverClient.flush(); // clear input buffer, else you get strange characters
  }
}

void off() { Serial.println("off: TO BE IMPLEMENTED"); }

void play_reminder_sound() {
  serverClient.println(F("Playing sound"));
  // e8rtp::stop();  // Stop playing
  // e8rtp::start(); // Play reminder sound
  // Workaround for: https://github.com/earlephilhower/ESP8266Audio/issues/604
  // RTTTL ringtone format
  // https://www.laub-home.de/wiki/RTTTL_Songs
  // To test: https://adamonsoon.github.io/rtttl-play/
  ringtone = preferences.getString("ringtone", "");
  if (ringtone == "") {
    ringtone =
        F("Entertainer:d=4,o=5,b=140:8d,8d#,8e,c6,8e,c6,8e,2c.6,8c6,8d6,"
          "8d#6,8e6,8c6,8d6,e6,8b,d6,2c6,p,8d,8d#,8e,c6,8e,c6,8e,2c.6,8p,"
          "8a,8g,8f#,8a,8c6,e6,8d6,8c6,8a,2d6");
  }
  file = new AudioFileSourcePROGMEM(ringtone.c_str(), ringtone.length());
  out = new AudioOutputI2S();
  alarm_volume = preferences.getUInt("alarm_volume", 11);
  out->SetGain(((float)alarm_volume) / 100.0);
  rtttl->begin(file, out); // Play reminder sound
}

void get_next_event() {
  lcd.setCursor(0, 1);
  api = preferences.getString("api", "");
  if (api.length() == 0) {
    println(F("No API URL found, please configure it in the web interface"));
  }

  Serial.println(F("Connecting to host..."));

  const char *url =
      api.c_str(); // "https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxx_xxxxxx_xxxxxxxxxxxxxxxxxxxxx-x_xxxxxxxxxxxxxxxxxxxxxx/exec"

  const char *host = extractHostFromURL(url); // "script.google.com"
  const char *path = extractPathFromURL(
      url); // "/macros/s/xxxxxxxxxxxxxxxxxx_xxxxxx_xxxxxxxxxxxxxxxxxxxxx-x_xxxxxxxxxxxxxxxxxxxxxx/exec"

  // Settings for the HTTPSRedirect client
  client = new HTTPSRedirect(443);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  // Serial.println(F("Trying 5 times to connect to host..."));

  // // Try to connect 5 times, then exit
  // bool flag = false;
  // for (int i = 0; i < 5; i++) {
  //   lcd.print(i);
  //   int retval = client->connect(host, 443);
  //   if (retval == 1) {
  //     flag = true;
  //     break;
  //   } else {
  //     Serial.println(F("Connection failed. Retrying..."));
  //     // delay(500);
  //   }
  // }

  // /*
  // On ESP32, we get this error:
  // [  3285][E][ssl_client.cpp:37] _handle_error(): [send_ssl_data():387]:
  // (-27136) SSL - A buffer is too small to receive or write a message
  // https://github.com/electronicsguy/HTTPSRedirect/issues/7
  // On ESP8266, it works fine.
  // */

  // if (!flag) {
  //   Serial.print("Could not connect to server: ");
  //   Serial.println(host);
  //   Serial.println(
  //       "Exiting..."); // TODO: Should we reboot here after some delay?
  //   lcd.setCursor(0, 0);
  //   lcd.clear();
  //   lcd.print("Error 0");
  //   return;
  // }

  Serial.println("");

  // Append "?action=listEvents" to the path
  // path += "?action=listEvents"; // error: invalid operands of types 'const
  // char*' and 'const char*' to binary 'operator+' so use String instead:
  String pathString = String(path);
  pathString += "?action=listEvents";
  path = pathString.c_str();
  String payload = "";
  int i = 0;
  while (payload == "") {
    Serial.print("Attempt ");
    Serial.println(i);
    client->connect(host, 443);
    client->GET(path, host);
    payload = client->getResponseBody();
    delay(1000);
    i++;
    if (i >= 10)
      break;
  }

  // Parse JSON object
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print("Error 1");
    return;
  }

  for (JsonObject event : doc["events"].as<JsonArray>()) {
    const char *event_title = event["title"];
    const char *event_description = event["description"];
    int event_minutesFromNow = event["minutesFromNow"];
  }

  // Print first event
  // TODO: Handle multiple events
  Serial.print("First event: ");
  event_title = doc["events"][0]["title"].as<const char *>();

  // Show event on LCD
  if (event_title != "") {
    Serial.println(event_title);
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(event_title);
    play_reminder_sound();
    // Set a timer that gets called every X seconds
    alarm_repeat_seconds = preferences.getUInt("alarm_repeat_seconds", 30);
    timer.attach(alarm_repeat_seconds, play_reminder_sound);
  } else {
    timer.detach();
    Serial.println("No event found");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(".");
    lcd.noBacklight();
  }
}

void confirm_event() {
  // e8rtp::stop(); // Stop event sound
  rtttl->stop(); // Stop event sound
  timer.detach();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.noBacklight();
  if (event_title.length() == 0) {
    Serial.println(F("No event title found"));
    reboot();
    return;
  }
  Serial.println(F("Confirming event..."));
  const char *url = api.c_str();
  const char *host = extractHostFromURL(url);
  const char *path = extractPathFromURL(url);
  String pathString = String(path);
  pathString += "?action=confirmEvent&eventTitle=" + event_title;
  path = pathString.c_str();
  client->GET(path, host);
  String response = client->getResponseBody();
  if (response.indexOf("confirmed") == -1) {
    lcd.backlight();
    Serial.println(F("Error confirming event"));
    lcd.print(response);
    delay(2000);
  }
  reboot(); // Never got it to work without rebooting, FIXME
}

//**************************************************************************************************
//                                            I S R *
//**************************************************************************************************

// Interrupt service routine that is triggered when the button is pressed
void IRAM_ATTR button_isr_handler() {

  // Measure how long the button is pressed
  unsigned long start = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    delay(1);
  }
  button_pressed_time = millis() - start;

  // If the button is pressed for more than 2 seconds, reset the device
  if (button_pressed_time > 2000) {
    // Reset the device
    ESP.restart();
  }

  // If the button was pressed for more than 10 ms, set button_pressed to true
  if (button_pressed_time > 10) {
    button_pressed = true;
  }
}

void printTime() {
  lcd.setCursor(0, 1);
  time_t utc =
      timeClient.getEpochTime(); // Get the current UTC time from the NTP client
  time_t local = timezone.toLocal(
      utc); // Convert the UTC time to local time observing DST if needed

  // Construct a string in the format "HH:MM:SS"
  char timeString[9];
  sprintf(timeString, "%02d:%02d:%02d", hour(local), minute(local),
          second(local));
  // Serial.println(timeString);
  lcd.print(timeString);
  lcd.setCursor(0, 0);

  // If minutes are 00 or 30 and seconds are randomized_seconds, reboot the
  // device. This means that every half hour plus a few seconds, the device
  // will reboot
  if (timeClient.getMinutes() == 0 || timeClient.getMinutes() == 30) {
    if (timeClient.getSeconds() == randomized_seconds) {
      reboot();
    }
  }
}

//**************************************************************************************************
//                                           S E T U P *
//**************************************************************************************************
void setup() {

  // Attach the interrupt to the button pin
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr_handler,
                  FALLING);

#if PLATFORMIO_BOARD == esp32cam
  // SD/Mode: Leave this pin alone
  pinMode(12, INPUT);

  // Gain select: Pull GPIO13 low
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
#endif

  Serial.begin(115200);

  lcd.begin(); // If you are using more I2C devices using the Wire library use
               // lcd.begin(false)
  lcd.noBacklight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("...");

  preferences.begin("Reminder",
                    false); // instance of preferences for defaults

  wifiManager.autoConnect();

  lcd.print("<->");

  parseConfigurationData();

  while (WiFi.status() != WL_CONNECTED) {
    delay(1500);
    print(".");
  }
  // println("Connected to %s", WiFi.SSID().c_str());
  // error: too many arguments to function 'void println(String)'
  // so use printf instead:
  printf("IP address: %s", WiFi.localIP().toString().c_str());
  println("");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(WiFi.localIP().toString().c_str());

  // Start the telnet server
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Start the HTTP server
  // with callback functions to handle requests
  server.on("/", handleRoot);
  server.on("/off", off);
  server.on("/config", handleConfig);
  server.on("/update_config", updateConfig);
  server.on("/reboot", reboot);

  httpUpdater.setup(&server);
  println("HTTPUpdateServer ready! Open /update in your browser");

  server.begin();
  print(F("HTTP server started, listening on IP ")),
      println(WiFi.localIP().toString());

  // reboot_timer.attach(
  //     60 * 25,
  //     reboot); // If Google hasn't answered in 25 minutes, we
  //              // restart so that we don't sit waiting here forever
  timeClient.begin();

  if (!MDNS.begin(device_name.c_str())) {
    println(F("Error setting up MDNS responder!"));
    while (1) {
      delay(1000);
    }
  }
  println(F("mDNS responder started"));

  // Add services to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("telnet", "tcp", 23);

  // Configure the sound playing, this must be done before calling
  // get_next_event()
  rtttl = new AudioGeneratorRTTTL();
  // e8rtp::setup(BUZZER_PIN, 5, ringtone.c_str());
  // e8rtp::setVolume(alarm_volume);

  // Get the next event; do this after setting up the sound so that the sound
  // will work
  get_next_event();

  // reboot_timer.detach(); // Google has answered

  randomized_seconds = random(0, 59);

  clock_timer.attach(1, printTime); // Call printTime function every 1 second
}

//**************************************************************************************************
//                                            L O O P *
//**************************************************************************************************
void loop() {

  // e8rtp::loop(); // This must be called from your main 'loop()', otherwise
  //                // nothing will happen.
  if (rtttl->isRunning()) {
    if (!rtttl->loop())
      rtttl->stop();
  }

  // listen for web requests
  server.handleClient();

  // See if time to sleep has arrived
  if (use_deep_sleep == true && millis() >= sleep_timer_begin_time) {
    off();
  }

  loopTelnet();
  if (button_pressed == true) {
    button_pressed = false;
    confirm_event();
  }

  // Reboot the device every 66 minutes;
  // in practice, it should be rebooted before that by the NTP
  long unsigned int reboot_after_ms = 60000 * 66; // minutes in milliseconds
  if (millis() > reboot_after_ms) {
    reboot();
  }

  timeClient.update();
}
