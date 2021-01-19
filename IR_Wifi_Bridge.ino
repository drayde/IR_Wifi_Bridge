#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>

// define if you want output on the M5Stack display
//#define M5_STACK_SUPPORT
// define if you want output on the M5Stick display
#define M5_STICK_SUPPORT

#if defined(M5_STACK_SUPPORT)
#include <M5Stack.h>
#define USE_DISPLAY
#define _M5 m5
#elif defined(M5_STICK_SUPPORT)
#include <M5StickC.h>
#define USE_DISPLAY
#define _M5 M5
#endif

#include "webserver/WebServer.h"
#include "webserver/WebServer.cpp"
#include "webserver/parsing.cpp"

#include <Preferences.h>
#include <IRremote.h>

#include "indexHtml.h"

#if defined(M5_STICK_SUPPORT)
const byte LED_PIN = 9; // internal IR LED on M5StickC
#else
const byte LED_PIN = 21; // set to where you IR LED is connected to
#endif
IRsend irsend(LED_PIN);

const IPAddress apIP(192, 168, 4, 1);
const char* apSSID = "IR_WIFI_BRIDGE";
boolean settingMode;
String ssidList;
String wifi_ssid;
String wifi_password;

WebServer webServer(80);

// wifi config store
Preferences preferences;

const size_t ir_buffer_length = 512;
unsigned int ir_buffer[ir_buffer_length];

void setup() 
{
#if defined(USE_DISPLAY)
  _M5.begin();
#endif  
  preferences.begin("wifi-config");

  delay(10);
  if (restoreConfig()) {
    if (checkConnection()) {
      settingMode = false;
      startWebServer();
      return;
    }
  }
  settingMode = true;
  setupMode();
}

void loop() 
{
  webServer.handleClient();
}

void output(const char* s)
{
  Serial.println(s);
#if defined(USE_DISPLAY)  
  _M5.Lcd.println(s);
#endif  
}
void output(const char* s1, const char* s2)
{
  Serial.print(s1);
  Serial.println(s2);
#if defined(USE_DISPLAY)  
  _M5.Lcd.print(s1);
  _M5.Lcd.println(s2);
#endif  
}
void output(char c)
{
  Serial.print(c);
#if defined(USE_DISPLAY)  
  _M5.Lcd.print(c);
#endif  
}

bool sendIR(String carrier, String code)
{
  // parsing carrier
  long carrierInt = carrier.toInt();
  if (carrierInt == 0) { output("carrier not a number or zero"); return false; } 
  if (carrierInt < 0) { output("carrier is negative"); return false; } 
  
  // parsing code
  int index = 0;
  int indexStart = 0;
  int indexEnd = -1;
  int bufferIndex = 0;
  int length = code.length();

  auto addToBuffer = [&](){
    if (bufferIndex >= ir_buffer_length) { output("code too long"); return false; } 
    int entryLength = indexEnd - indexStart + 1;
    if (entryLength < 1) { output("code entry is empty"); return false; } 
    if (entryLength > 6) { output("code entry too long"); return false; } 
    String entry = code.substring(indexStart, indexEnd+1);
    long entryInt = entry.toInt();
    if (entryInt == 0) {
      if (entryLength == 1 && entry.charAt(0) == '0') {
        // really "0"
      }
      else { output("code entry is not a number"); return false; } 
    }
    if (entryInt < 0) { output("code entry is negative"); return false; } 
    ir_buffer[bufferIndex] = (unsigned int) entryInt;
    
    //Serial.print(entryInt);
    //Serial.print("->");
    //Serial.println(bufferIndex);
    
    bufferIndex++;
    return true;
  };
  
  for (index = 0; index<length; ++index)
  {
    char c = code.charAt(index);    
    if (c == '+' || c == ' ')
    {      
      if (!addToBuffer())
        return false;
      indexStart = index+1;
      indexEnd = -1;
      continue;
    }
    
    indexEnd = index;
  }

  if (!addToBuffer())
    return false;
  
  output('+');
  output(' ');

  if (true)
  {
    Serial.print("carrier: "); 
    Serial.println(carrierInt);
    Serial.print("code:");
    for (int i=0; i<bufferIndex; ++i) {
      Serial.print(i==0 ? ' ' : ',');
      Serial.print(ir_buffer[i]);
    }
    Serial.println();
  }
  irsend.sendRaw(ir_buffer, bufferIndex, carrierInt);

  return true;
}

boolean restoreConfig() 
{
  wifi_ssid = preferences.getString("WIFI_SSID");
  wifi_password = preferences.getString("WIFI_PASSWD");

  output("WIFI-SSID: ", wifi_ssid.c_str());
  
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  return (wifi_ssid.length() > 0);
}

boolean checkConnection() 
{
  int count = 0;
  output("Waiting for Wi-Fi connection");

  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      output("");
      output("Connected!");
      return (true);
    }
    delay(500);
    output('.');
    count++;
  }
  output("Timed out.");
  return false;
}

void startWebServer() 
{
  if (settingMode) {
    output("Starting Web Server at ", WiFi.softAPIP().toString().c_str());
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    webServer.on("/setap", []() {
      String ssid = urlDecode(webServer.arg("ssid"));
      output("SSID: ", ssid.c_str());
      String pass = urlDecode(webServer.arg("pass"));
  
      output("Writing SSID to EEPROM...");

      // Store wifi config
      output("Writing Password to nvr...");
      preferences.putString("WIFI_SSID", ssid);
      preferences.putString("WIFI_PASSWD", pass);

      output("Write nvr done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      delay(3000);
      ESP.restart();
    });
  }
  else {
    output("Starting Web Server at ", WiFi.localIP().toString().c_str());

    webServer.on("/reset", []() {
      // reset the wifi config
      preferences.remove("WIFI_SSID");
      preferences.remove("WIFI_PASSWD");
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
      delay(3000);
      ESP.restart();
    });
  }

  webServer.on("/", [&webServer]() 
  {
    //output("Got request");
    
    String carrier = webServer.arg("carrier");
    String code = webServer.arg("code");

    if (carrier.length() && code.length() && sendIR(carrier, code))
    {
      String s = "done";
      webServer.send(200, "text/html", makePage("IR server", s));
      return;
    }

    if (settingMode)
    {
      String s = "<h1>AP mode</h1><p><a href=\"/ui\">UI</a></p><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    }
    else
    {
      String s = "<h1>STA mode</h1><p><a href=\"/ui\">UI</a></p><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";      
      webServer.send(200, "text/html", makePage("STA mode", s));
    }
  });


  webServer.on("/ui", []() 
  {
      webServer.send(200, "text/html", indexHtml);    
  });

  webServer.begin();
}

void setupMode() 
{
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  output("");
  for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  WiFi.mode(WIFI_MODE_AP);
  // WiFi.softAPConfig(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
  // WiFi.softAP(const char* ssid, const char* passphrase = NULL, int channel = 1, int ssid_hidden = 0);
  // dnsServer.start(53, "*", apIP);
  startWebServer();
  output("Starting Access Point at ", apSSID);
}

String makePage(String title, String contents) 
{
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

String urlDecode(String input) 
{
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}
