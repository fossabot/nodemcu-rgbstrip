#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

/* Store strings in PROGMEM */
const char CANNOT_CONNECT[] PROGMEM = {'Cannot connect to the AP: '};
const char MOUNTFAIL[] PROGMEM = {'Failed to mount file system!'};
const char FSMOUNTED[] PROGMEM = {'mounted file system'};
const char SAVEWCONF[] PROGMEM = {'Saving the WiFi configuration...'};
const char CONFMODE[] PROGMEM = {'Entered to configuration mode...'};
const char RESETNODE[] PROGMEM = {'Entered to configuration mode...'};
const char CONFSTR[] PROGMEM = {'Config: '};
const char SETUPSTR[] PROGMEM = {'Setup...'};
const char MACMSG[] PROGMEM = {'MAC Address: '};
const char APIPSTR[] PROGMEM = {'AP IP: '};
const char NOCONF[] PROGMEM = {'Config file not found!'};
const char CONFOPEN[] PROGMEM = {'saveConfigCallback: opened config file for writing the config'};
const char CONFFAILOPEN[] PROGMEM = {'saveConfigCallback: cannot open the config file for write'};
const char CONFOPENSAVE[] PROGMEM = {'saveConfigCallback: config file has been opened for savig the config and JSON buffer has been created successfully'};
const char CONFSAVED[] PROGMEM = {'saveConfigCallback: config save ok'};
const char CONFOPENED[] PROGMEM = {'config opened for read'};
const char CONFOPENING[] PROGMEM = {'opening the config file...'};
const char CONFPARSED[] PROGMEM = {'\nparsed json'};
const char CONFPARSEFAIL[] PROGMEM = {'failed to parse the config'};
const char CONFDEL[] PROGMEM = {'Deleting the saved config file...'};
const char RESET[] PROGMEM = {'Reset...'};
const char WIFIOK[] PROGMEM = {'Connected'};
const char WIFIFAIL[] PROGMEM = {'WiFi: failed to connect to the AP'};
const char WIFICONF[] PROGMEM = {'Current config: '};
const char WIFIIP[] PROGMEM = {'IP: '};

/* Global constants */
#define MAX_CONNTRY 5
#define PWM_FREQ 100
#define CONFNAME "/wificreds.conf"

/* Global variables */
ESP8266WebServer myServer(8080);
String stassid = "";
String stapass = "";

void deleteWifiConfig() {
  cli();
  if (SPIFFS.exists(CONFNAME)) {
    Serial.println(FPSTR(CONFDEL));
    SPIFFS.remove(CONFNAME);
    Serial.println(FPSTR(RESET));
    ESP.reset();   
  }
}

void saveWifiConfigCallback() {
  Serial.println(FPSTR(SAVEWCONF));
  // Store the credentials for using it in the program
  stassid = WiFi.SSID();
  stapass = WiFi.psk();
  File configFile = SPIFFS.open(CONFNAME, "w");
  if (configFile) {
        Serial.println(FPSTR(CONFOPEN));
  } else {
        Serial.println(FPSTR(CONFFAILOPEN));
        return;
  }
  Serial.println(FPSTR(CONFOPENSAVE));
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["sta_ssid"] = stassid;
  json["sta_pass"] = stapass;
  json.printTo(configFile);
  Serial.println(FPSTR(CONFSAVED));
}

void configModeCallback(WiFiManager *myWfMan) {
  Serial.println(FPSTR(CONFMODE));
  Serial.print(FPSTR(APIPSTR));
  Serial.println(WiFi.softAPIP());
  Serial.print(FPSTR(MACMSG));
  Serial.println(WiFi.softAPmacAddress());
  Serial.println(myWfMan->getConfigPortalSSID());
}

void setup() {
  boolean hasSavedConfig = false;

  delay(500);
  Serial.begin(115200);
  Serial.println(FPSTR(SETUPSTR));

  pinMode(D1, OUTPUT);  /* RED */
  pinMode(D2, OUTPUT);  /* GREEN */
  pinMode(D4, OUTPUT);  /* BLUE */
  pinMode(D3, INPUT_PULLUP);   /* for the FLASH => reset config button */

  /* remove saved config and restart if the flash button is pushed */
  attachInterrupt(digitalPinToInterrupt(D3), deleteWifiConfig, CHANGE);

/* Setup PWM */
  analogWriteFreq(PWM_FREQ);
  analogWriteRange(1023);

 /* Try to load config */
 if (SPIFFS.begin()) {
    Serial.println(FPSTR(FSMOUNTED));
    if (SPIFFS.exists(CONFNAME)) {
      //file exists, reading and loading
      Serial.println(FPSTR(CONFOPENING));
      File configFile = SPIFFS.open(CONFNAME, "r");
      if (configFile) {
        Serial.println(FPSTR(CONFOPENED));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println(FPSTR(CONFPARSED));
          stassid = String((const char*)json["sta_ssid"]);
          stapass = String((const char*)json["sta_pass"]);
          hasSavedConfig = true;
        } else {
          Serial.println(FPSTR(CONFPARSEFAIL));
        }
      }
    } else {
      Serial.println(FPSTR(NOCONF));
    }
  } else {
    Serial.println(FPSTR(MOUNTFAIL));
    Serial.println(FPSTR(RESET));
    Serial.println("");
    ESP.reset();
  }

  /* No saved config file => start in AP mode with the config portal */
  if (!hasSavedConfig) {
    // Setup WiFiManager and start the config portal
    WiFiManager wfMan;
    String ssid = "ESP" + String(ESP.getChipId());
    wfMan.setConfigPortalTimeout(180);
    wfMan.setAPCallback(configModeCallback);
    wfMan.setSaveConfigCallback(saveWifiConfigCallback);
    wfMan.setMinimumSignalQuality(10);
    wfMan.setBreakAfterConfig(true);
    //wfMan.setDebugOutput(false);        // for final installation
    wfMan.startConfigPortal(ssid.c_str());
  }

  /* Connect to the AP if not connected by the WiFiManager */
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(true);
  for (uint8_t count = MAX_CONNTRY; count > 0; count--) {
    if (!WiFi.isConnected()) {
      WiFi.begin(stassid.c_str(), stapass.c_str());
    } else {
      Serial.println(FPSTR(WIFIFAIL));
      break;
    }
    delay(1000);
  }
  if (!WiFi.isConnected()) {
    Serial.print(FPSTR(CANNOT_CONNECT));
    Serial.println(stassid.c_str());
    Serial.println(FPSTR(CONFSTR));
    WiFi.printDiag(Serial);
    Serial.println(FPSTR(RESETNODE));
    delay(1000);
    ESP.reset();
  } else {
    WiFi.setAutoConnect(true);
    Serial.println("");
    Serial.println(FPSTR(WIFIOK));
    Serial.println(FPSTR(WIFICONF));
    WiFi.printDiag(Serial);
    Serial.print(FPSTR(WIFIIP));
    Serial.println(WiFi.localIP());
  }

  /*
      TODO: Setup the webserver (myServer)
      TODO: Setup the MQTT client
  */
}

void loop() {
  // put your main code here, to run repeatedly:
  // myServer.handleClient();
  delay(200);
}
