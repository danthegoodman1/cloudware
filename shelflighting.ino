#include <WiFiManager.h> // from library manager
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiServerSecure.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecure.h>

#include <FS.h>
//#include "SPIFFS.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>  //https://github.com/bblanchon/ArduinoJson
#include <ESP8266HTTPClient.h>

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#define FASTLED_ESP8266_D1_PIN_ORDER
int RESET_WIFI_AND_SPIFFS = 15; // D8
#define BUILTINLED 2
#include <FastLED.h>
#define DATA_PIN    5
//#define CLK_PIN   4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    180
#define BRIGHTNESS  255
int runColor = 1; // This is for the color selection
// Blynk colors
int red = 1;
int blue = 1;
int green = 1;

// Reset button
int resetButton;

// NOTES NOTES NOTES NOTES::::::::::::::::::

CRGB leds[NUM_LEDS];

#include <BlynkSimpleEsp8266.h>
#define BLYNK_PRINT Serial


//define your default values here, if there are different values in config.json, they are overwritten.
char apikey[34];
int brightnessL = 1;

//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Sync buttons with the Blynk app on reconnect, that way I don't have to deal
// With storing the states in flash
BLYNK_CONNECTED() {
    Blynk.syncAll();
}

// Blynk virtual pin handling
// All of the setting variable then writing the value might be redundant since we
// Set the value in the pin write...
// NO THEY AREN'T I HAD TO DO BOTH FOR THE POWER SWITCH (V4) IT ONLY UPDATES THE APP I THINK MAYBE IT WAS BEING WEIRD

// Chosen Color
BLYNK_WRITE(V0) //Button Widget is writing to pin V1
{
  runColor = 1;
  Blynk.virtualWrite(V1, 1);
  brightnessL = param.asInt();
  Serial.println("Brightness is:");
  Serial.println(brightnessL);
}
// Stock Animation
BLYNK_WRITE(V1) //Button Widget is writing to pin V1
{
  runColor = param.asInt();
  Serial.println("Power status is:");
  Serial.println(runColor);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  Serial.println("I am on");

  // Check if reset button held
  pinMode(BUILTINLED, OUTPUT);
  pinMode(RESET_WIFI_AND_SPIFFS, INPUT_PULLUP);

  //read configuration from FS json
  Serial.println("mounting FS...");
//  SPIFFS.begin (true);

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(apikey, json["apikey"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_apikey("Api Key", "Api Key", apikey, 34);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set to continue on failure so I can keep the parameters
  wifiManager.setBreakAfterConfig(true);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_apikey);

  //reset settings - for testing
  //wifiManager.resetSettings();

  if (!wifiManager.autoConnect("Todd's Custom Shelf Lighting")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
  }

  //read updated parameters
  strcpy(apikey, custom_apikey.getValue());

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["apikey"] = apikey;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println();
// for debugging:
  Serial.println("Api Key: ");
  Serial.println(apikey);
  Serial.println("brightnessL: ");
  Serial.println(brightnessL);
  Serial.println("%");

//  WiFi.disconnect(true); //erases store credentially
//  SPIFFS.format();  //erases stored values
  Serial.println("Done");

  // check if need initial restart
  // This will be determined why whether our localip is 0.0.0.0
  // Which means it was the first boot and it did its stupid fail crap
  if(WiFi.localIP().toString() == "0.0.0.0"){
    delay(2000);
    ESP.restart();
  }

  // led stuff
  delay(3000); // 3 second delay for recovery
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS)
  .setDither(BRIGHTNESS < 255);
  FastLED.setBrightness(BRIGHTNESS);

  Blynk.config(apikey);
  if(!Blynk.connect()) {
     Serial.println("Blynk connection timed out.");
     // Reset ESP if not connecting
//     delay(2000);
//     ESP.restart();
  }

}


void loop() {

  resetButton = digitalRead(RESET_WIFI_AND_SPIFFS);
  if(resetButton == HIGH){
    Serial.println("RESET IS HIGH");
    // Reset
    WiFi.disconnect(true); //erases store credentially
    SPIFFS.format();  //erases stored values
    // Blink LED 5 times per second for 3 seconds (15 times)
    for(int i = 0; i < 15; i++){
      digitalWrite(BUILTINLED, HIGH);
      delay(200);
      digitalWrite(BUILTINLED, LOW);
      delay(200);
    }
    // Reboot device, which will put it in setup mode
    Serial.println("Resetting ESP Storage, will reboot into setup mode");
    delay(1000);
    ESP.restart();
  } else {
    Serial.println("RESET IS LOW");
  }

  Blynk.run();

///////////////////////////////////////////////////////////////////////

//Uncomment these lines of code if you want to reset the device
//It could be linked to a physical reset button on the device and set
//to trigger the next 3 lines of code.
//  WiFi.disconnect(true); //erases store credentially
//  SPIFFS.format();  //erases stored values
  //ESP.restart();
///////////////////////////////////////////////////////////////////////

  if(runColor == 1){
    for(int i = 0; i < NUM_LEDS; i++){
        leds[i].r = red * brightnessL;
        leds[i].g = green * brightnessL;
        leds[i].b = blue * brightnessL;
        delay(20);
        FastLED.show();
    }
  } else {
      for(int i = 0; i < NUM_LEDS; i++){
        leds[i].r = 0;
        leds[i].g = 0;
        leds[i].b = 0;
        delay(20);
        FastLED.show();
    }
  }


}
