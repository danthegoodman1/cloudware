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
#include <FastLED.h>
#define DATA_PIN    14 // D5
#define BUILTINLED 2 // Pin 2
#define RESET_WIFI_AND_SPIFFS 15 // D8
//#define CLK_PIN   4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    16
#define BRIGHTNESS  255
// Animation Buttons
#define buttonPinA 2
#define buttonPinB 0
#define buttonPinC 3
#define SECONDS_PER_PALETTE 10
int buttonStateA;
int buttonStateB;
int buttonStateC;
int buttonStateD;
int runColor = 1; // This is for the color selection, on powerloss we want to make sure it gets turned back on
// Then it can be turned off from the app (will automatically if off) when it reconnects
// Blynk colors
int redColor;
int blueColor;
int greenColor;
// For checking reset
int resetButton;

CRGB leds[NUM_LEDS];

#include <BlynkSimpleEsp8266.h>
#define BLYNK_PRINT Serial

// For music react
#define ANALOG_READ 0
#define MIC_LOW 0.0
#define MIC_HIGH 737.0
#define AVGLEN 5
#define LONG_SECTOR 20
#define HIGH 3
#define NORMAL 2
#define DELAY 1
#define DEV_THRESH 0.8
#define MSECS 30 * 1000
#define CYCLES MSECS / DELAY

BlynkTimer timer;

// Blink codes:
// 5 times/second for 3 seconds (15 times): Resetting WiFi and SPIFFS (user caused by holding reset button on powerup)
// 2 times/second for 5 seconds (10 times): Blynk connection failed, rebooting, if put in wrong API key above reset required
// 

// Music React
float fscale( float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve);
void insert(int val, int *avgs, int len);
int compute_average(int *avgs, int len);
void visualize_music();

int curshow = NUM_LEDS;
int mode = 0;
int songmode = NORMAL;
unsigned long song_avg;
int iter = 0;
float fade_scale = 1.2;
int avgs[AVGLEN] = {-1};
int long_avg[LONG_SECTOR] = {-1};
struct time_keeping {
  unsigned long times_start;
  short times;
};
struct color {
  int r;
  int g;
  int b;
};
struct time_keeping high;
struct color Color;


//define your default values here, if there are different values in config.json, they are overwritten.
char apikey[34];
char redC[4] = {'0'};
char greenC[4] = {'0'};
char blueC[4] = {'0'};

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
  Blynk.virtualWrite(V4, 1);
  Blynk.virtualWrite(V1, 0);
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V3, 0);
  Blynk.virtualWrite(V5, 0);
  buttonStateA = 0;
  buttonStateB = 0;
  buttonStateC = 0;
  buttonStateD = 0;
  redColor = param[0].asInt();
  greenColor = param[1].asInt();
  blueColor = param[2].asInt();
  Serial.println(redColor);
  Serial.println(greenColor);
  Serial.println(blueColor);

  // We also want to save the color, but I am not actually loading it yet since I need to do atoi() stuff
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["apikey"] = apikey;
  json["redC"] = redColor;
  json["greenC"] = greenColor;
  json["blueC"] = blueColor;
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  Serial.println("Printing new JSON:::::::");
  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}
// Stock Animation
BLYNK_WRITE(V1) //Button Widget is writing to pin V1
{
  buttonStateA = param.asInt();
  runColor = 1;
  Blynk.virtualWrite(V4, 1);
  Serial.println(buttonStateA);
  // Update the buttons on the app
  if (buttonStateB == 1){
    Blynk.virtualWrite(V2, 0);
    buttonStateB = 0;
  }
  if (buttonStateC == 1){
    Blynk.virtualWrite(V3, 0);
    buttonStateC = 0;
  }
  if (buttonStateD == 1){
    Blynk.virtualWrite(V5, 0);
    buttonStateD = 0;
  }
}
// Custom 1
BLYNK_WRITE(V2)
{
  buttonStateB = param.asInt();
  runColor = 1;
  Blynk.virtualWrite(V4, 1);
  if (buttonStateA == 1){
    Blynk.virtualWrite(V1, 0);
    buttonStateA = 0;
  }
  if (buttonStateC == 1){
    Blynk.virtualWrite(V3, 0);
    buttonStateC = 0;
  }
  if (buttonStateD == 1){
    Blynk.virtualWrite(V5, 0);
    buttonStateD = 0;
  }
}
// Custom 2
BLYNK_WRITE(V3)
{
  buttonStateC = param.asInt();
  runColor = 1;
  Blynk.virtualWrite(V4, 1);
  if (buttonStateA == 1){
    Blynk.virtualWrite(V1, 0);
    buttonStateA = 0;
  }
  if (buttonStateB == 1){
    Blynk.virtualWrite(V2, 0);
    buttonStateB = 0;
  }
  if (buttonStateD == 1){
    Blynk.virtualWrite(V5, 0);
    buttonStateD = 0;
  }
}
// Power switch (runColor)
BLYNK_WRITE(V4)
{
  runColor = param.asInt();
  // if Turned off, turn off all animations, that way on power on it will just return to the color
  // Chosen from the RGB Zebra
  if (runColor == 0){
    Blynk.virtualWrite(V1, 0);
    buttonStateA = 0;
    Blynk.virtualWrite(V2, 0);
    buttonStateB = 0;
    Blynk.virtualWrite(V3, 0);
    buttonStateC = 0;
    Blynk.virtualWrite(V5, 0);
    buttonStateD = 0;
  }
}
BLYNK_WRITE(V5)
{
  buttonStateD = param.asInt();
  runColor = 1;
  Blynk.virtualWrite(V4, 1);
  if (buttonStateA == 1){
    Blynk.virtualWrite(V1, 0);
    buttonStateA = 0;
  }
  if (buttonStateB == 1){
    Blynk.virtualWrite(V2, 0);
    buttonStateB = 0;
  }
  if (buttonStateC == 1){
    Blynk.virtualWrite(V3, 0);
    buttonStateC = 0;
  }
}


void batteryCheck(){
  pinMode(A0, INPUT);
  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH);
  // This will enable the read, since the 100k Resistor will consume 10uA
  raw = analogRead(A0);
  volt=raw/1023.0;
  digitalWrite(D5, LOW); // Hopefully the volt variable has copied the raw value
  // Otherwise just put the digitalWrite at the bottom, but they are primitive data types
  percentage=volt*100;
  volt=volt*4.2;
  Blynk.virtualWrite(V6, volt);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  // Check if reset button held
  pinMode(BUILTINLED, OUTPUT);
  resetButton = digitalRead(RESET_WIFI_AND_SPIFFS);
  if(resetButton == LOW){
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
  }

  timer.setInterval(300000L, myTimerEvent); // Send battery status every 2 min

  // Music React
  for (int i = 0; i < AVGLEN; i++) {  
    insert(250, avgs, AVGLEN);
  }
  high.times = 0;
  high.times_start = millis();
  Color.r = 0;  
  Color.g = 0;
  Color.b = 1;



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
          strcpy(redC, json["redC"]);
          strcpy(greenC, json["greenC"]);
          strcpy(blueC, json["blueC"]);

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

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(40);
  // DAN: Ok for some reason this does not like to connect the first time, so this
  // timeout forces a reset which then lets it connect to the network and start
  // doing stuff
  // someone has to finish setup before the timeout ends or it reboots
  // and it could reboot in middle of setup
  // UPDATE FIXED IT!!! by adding the lines in WiFiManeger.cpp (216) to reset
  // instead of waiting for this reset lol

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Dan Test // ESP8266")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
//    ESP.restart();
//    delay(5000);
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
    json["redC"] = redC;
    json["greenC"] = greenC;
    json["blueC"] = blueC;
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
  Serial.println("Red Color: ");
  Serial.println(redC);
  Serial.println("Green Color: ");
  Serial.println(greenC);
  Serial.println("Blue Color: ");
  Serial.println(blueC);
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


  // Not sure that these are necessary since D works without this... and they aren't real buttons/pins...
  // Yeah they aren't necessary
  pinMode(buttonPinA, INPUT);
  buttonStateA = digitalRead(buttonPinA);
  pinMode(buttonPinB, INPUT);
  buttonStateB = digitalRead(buttonPinB);
  pinMode(buttonPinC, INPUT);
  buttonStateC = digitalRead(buttonPinC);


  Blynk.config(apikey);
  if(!Blynk.connect()) {
     Serial.println("Blynk connection timed out.");
     //handling code (reset esp?)
     // Blink 2 times per second for 5 seconds (10 times)
     for(int i = 0; i<10; i++){
      digitalWrite(BUILTINLED, HIGH);
      delay(500);
      digitalWrite(BUILTINLED, LOW);
      delay(500);
     }
     // Reset (if put in wrong API key storage reset required)
     Serial.println("Resetting ESP, if you put in wrong API key, do a storage reset by holding the reset button on power-up");
     delay(1000);
     ESP.restart();
  }

}

extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const TProgmemRGBGradientPalettePtr gGradientPalettesCustom1[];
extern const TProgmemRGBGradientPalettePtr gGradientPalettesCustom2[];
extern const uint8_t gGradientPaletteCount;

// Current palette number from the 'playlist' of color palettes
uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );
CRGBPalette16 gTargetPaletteCustom1( gGradientPalettesCustom1[0] );
CRGBPalette16 gTargetPaletteCustom2( gGradientPalettesCustom2[0] );


void loop() {
  // initial LED display from the json and update leds

//  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
//
//    HTTPClient http;
//
//    http.begin("http://104.236.103.190/getcolors"); //Specify the URL
//    int httpCode = http.GET();                                        //Make the request
//
//    if (httpCode > 0) { //Check for the returning code
//
//        String payload = http.getString();
//        Serial.println(httpCode);
//        Serial.println(payload);
//
//        // parse the payload
//        StaticJsonBuffer<200> jsonBuffers;
//        JsonObject& colorData = jsonBuffers.parseObject(payload);
//        Serial.println("HERE IS THE JSON DATATA");
//        colorData.printTo(Serial);
////        const char* newRedC = colorData["red"];
////        Serial.print("HERE IS RED: ");
////        Serial.println(newRedC);
//        strcpy(redC, colorData["red"]);
//        strcpy(greenC, colorData["green"]);
//        strcpy(blueC, colorData["blue"]);
//        Serial.println("PARSED THE NEW JSON");
//
//        // update the stored json
//        DynamicJsonBuffer jsonBuffer;
//        JsonObject& json = jsonBuffer.createObject();
//        json["apikey"] = apikey;
//        json["redC"] = redC;
//        json["greenC"] = greenC;
//        json["blueC"] = blueC;
//        File configFile = SPIFFS.open("/config.json", "w");
//        if (!configFile) {
//          Serial.println("failed to open config file for writing");
//        }
//        Serial.println("Printing new JSON:::::::");
//        json.printTo(Serial);
//        json.printTo(configFile);
//        configFile.close();
//      }
//
//    else {
//      Serial.println("Error on HTTP request");
//    }
//
//    http.end(); //Free the resources
//  }
//  Serial.println(apikey);
//  delay(1000);

  Blynk.run();
  timer.run(); // Initiates BlynkTimer

///////////////////////////////////////////////////////////////////////

//Uncomment these lines of code if you want to reset the device
//It could be linked to a physical reset button on the device and set
//to trigger the next 3 lines of code.
  //WiFi.disconnect(true); //erases store credentially
  //SPIFFS.format();  //erases stored values
  //ESP.restart();
///////////////////////////////////////////////////////////////////////

// Handle the buttons to switch between modes and color changes
//buttonStateA = digitalRead(buttonPinA);
//buttonStateB = digitalRead(buttonPinB);
//buttonStateC = digitalRead(buttonPinC);

// Check for changes, update variables so we can make the buttons push buttons and not switches on Blynk app
// Using virtual pins now so don't have to worry about loop speed checking for pin since Blynk checking virtual reads will
// Run in the background
// Can probably remove this:::
// if(buttonStateA == 1){
//   buttonStateB = 0;
//   buttonStateC = 0;
//   runColor = false;
// }

// Old hardware button checking
//if (digitalRead(buttonPinA) == 1){
//  buttonStateA = 1;
//  buttonStateB = 0;
//  buttonStateC = 0;
//  runColor = false;
//}
//if (digitalRead(buttonPinB) == 1){
//  buttonStateA = 0;
//  buttonStateB = 1;
//  buttonStateC = 0;
//  runColor = false;
//}
//if (digitalRead(buttonPinC) == 1){
//  buttonStateA = 0;
//  buttonStateB = 0;
//  buttonStateC = 1;
//  runColor = false;
//}
//if (runColor){
//  buttonStateA = 0;
//  buttonStateB = 0;
//  buttonStateC = 0;
//}

  if(buttonStateA == 1 && runColor == 1){

    EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
      gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
      gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
    }

    EVERY_N_MILLISECONDS(40) {
      nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
    }

    colorwaves( leds, NUM_LEDS, gCurrentPalette);

    FastLED.show();
    FastLED.delay(20);

    } else if(buttonStateB == 1 && runColor == 1){
      // gGradientPalettesCustom1
      EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
      gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
      gTargetPaletteCustom1 = gGradientPalettesCustom1[ gCurrentPaletteNumber ];
    }

    EVERY_N_MILLISECONDS(40) {
      nblendPaletteTowardPalette( gCurrentPalette, gTargetPaletteCustom1, 16);
    }

    colorwaves( leds, NUM_LEDS, gCurrentPalette);

    FastLED.show();
    FastLED.delay(20);
    } else if(buttonStateC == 1 && runColor == 1){
      // gGradientPalettesCustom1
      EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
      gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
      gTargetPaletteCustom2 = gGradientPalettesCustom2[ gCurrentPaletteNumber ];
    }

    EVERY_N_MILLISECONDS(40) {
      nblendPaletteTowardPalette( gCurrentPalette, gTargetPaletteCustom2, 16);
    }

    colorwaves( leds, NUM_LEDS, gCurrentPalette);

    FastLED.show();
    FastLED.delay(20);

  } else if(buttonStateD == 1 && runColor == 1){ // Music React

    switch(mode) {
    case 0:
      visualize_music();
      break;
    default:
      break;
  }
    delay(DELAY);
  
  } else if(runColor == 1){
//      Serial.println("Displaying LEDS");
//      Serial.println(atoi(redC));
//      Serial.println(atoi(greenC));
//      Serial.println(atoi(blueC));
      for(int i = 0; i < NUM_LEDS; i++){
        // Could use leds[i] = CRGB(0, 0, 255); ?
        leds[i].r = redColor;
        leds[i].g = greenColor;
        leds[i].b = blueColor;
      };
      FastLED.show();
      
  } else { // set all to black is runColor is 0
    for(int i = 0; i < NUM_LEDS; i++){
      leds[i].r = 0;
      leds[i].g = 0;
      leds[i].b = 0;
    };
    FastLED.show();
  }


}


// Music React
void check_high(int avg) {
  if (avg > (song_avg/iter * 1.1))  {
    if (high.times != 0) {
      if (millis() - high.times_start > 200.0) {
        high.times = 0;
        songmode = NORMAL;
      } else {
        high.times_start = millis();  
        high.times++; 
      }
    } else {
      high.times++;
      high.times_start = millis();

    }
  }
  if (high.times > 30 && millis() - high.times_start < 50.0)
    songmode = HIGH;
  else if (millis() - high.times_start > 200) {
    high.times = 0;
    songmode = NORMAL;
  }
}

void visualize_music() {
  int sensor_value, mapped, avg, longavg;
  
  //Actual sensor value
  sensor_value = analogRead(ANALOG_READ);
  
  //If 0, discard immediately. Probably not right and save CPU.
  if (sensor_value == 0)
    return;

  //Discard readings that deviates too much from the past avg.
  mapped = (float)fscale(MIC_LOW, MIC_HIGH, MIC_LOW, (float)MIC_HIGH, (float)sensor_value, 2.0);
  avg = compute_average(avgs, AVGLEN);

  if (((avg - mapped) > avg*DEV_THRESH)) //|| ((avg - mapped) < -avg*DEV_THRESH))
    return;
  
  //Insert new avg. values
  insert(mapped, avgs, AVGLEN); 
  insert(avg, long_avg, LONG_SECTOR); 

  //Compute the "song average" sensor value
  song_avg += avg;
  iter++;
  if (iter > CYCLES) {  
    song_avg = song_avg / iter;
    iter = 1;
  }
    
  longavg = compute_average(long_avg, LONG_SECTOR);

  //Check if we enter HIGH mode 
  check_high(longavg);  

  if (songmode == HIGH) {
    fade_scale = 3;
    Color.r = 5;
    Color.g = 3;
    Color.b = -1;
  }
  else if (songmode == NORMAL) {
    fade_scale = 2;
    Color.r = -1;
    Color.b = 2;
    Color.g = 1;
  }

  //Decides how many of the LEDs will be lit
  curshow = fscale(MIC_LOW, MIC_HIGH, 0.0, (float)NUM_LEDS, (float)avg, -1);

  /*Set the different leds. Control for too high and too low values.
          Fun thing to try: Dont account for overflow in one direction, 
    some interesting light effects appear! */
  for (int i = 0; i < NUM_LEDS; i++) 
    //The leds we want to show
    if (i < curshow) {
      if (leds[i].r + Color.r > 255)
        leds[i].r = 255;
      else if (leds[i].r + Color.r < 0)
        leds[i].r = 0;
      else
        leds[i].r = leds[i].r + Color.r;
          
      if (leds[i].g + Color.g > 255)
        leds[i].g = 255;
      else if (leds[i].g + Color.g < 0)
        leds[i].g = 0;
      else 
        leds[i].g = leds[i].g + Color.g;

      if (leds[i].b + Color.b > 255)
        leds[i].b = 255;
      else if (leds[i].b + Color.b < 0)
        leds[i].b = 0;
      else 
        leds[i].b = leds[i].b + Color.b;  
      
    //All the other LEDs begin their fading journey to eventual total darkness
    } else {
      leds[i] = CRGB(leds[i].r/fade_scale, leds[i].g/fade_scale, leds[i].b/fade_scale);
    }
  FastLED.show(); 
}

int compute_average(int *avgs, int len) {
  int sum = 0;
  for (int i = 0; i < len; i++)
    sum += avgs[i];

  return (int)(sum / len);

}

void insert(int val, int *avgs, int len) {
  for (int i = 0; i < len; i++) {
    if (avgs[i] == -1) {
      avgs[i] = val;
      return;
    }  
  }

  for (int i = 1; i < len; i++) {
    avgs[i - 1] = avgs[i];
  }
  avgs[len - 1] = val;
}

float fscale( float originalMin, float originalMax, float newBegin, float
    newEnd, float inputValue, float curve){

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;


  // condition curve parameter
  // limit range

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ; // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output 
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function

  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin){ 
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd; 
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal  =  zeroRefCurVal / OriginalRange;   // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine 
  if (originalMin > originalMax ) {
    return 0;
  }

  if (invFlag == 0){
    rangedValue =  (pow(normalizedCurVal, curve) * NewRange) + newBegin;

  }
  else     // invert the ranges
  {   
    rangedValue =  newBegin - (pow(normalizedCurVal, curve) * NewRange); 
  }

  return rangedValue;
}


// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;

  for( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds-1) - pixelnumber;

    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}

DEFINE_GRADIENT_PALETTE( ib_jul01_gp ) {
    0, 194,  1,  1,
   94,   1, 29, 18,
  132,  57,131, 28,
  255, 113,  1,  1};

DEFINE_GRADIENT_PALETTE( es_vintage_57_gp ) {
    0,   2,  1,  1,
   53,  18,  1,  0,
  104,  69, 29,  1,
  153, 167,135, 10,
  255,  46, 56,  4};

DEFINE_GRADIENT_PALETTE( es_vintage_01_gp ) {
    0,   4,  1,  1,
   51,  16,  0,  1,
   76,  97,104,  3,
  101, 255,131, 19,
  127,  67,  9,  4,
  153,  16,  0,  1,
  229,   4,  1,  1,
  255,   4,  1,  1};

DEFINE_GRADIENT_PALETTE( es_rivendell_15_gp ) {
    0,   1, 14,  5,
  101,  16, 36, 14,
  165,  56, 68, 30,
  242, 150,156, 99,
  255, 150,156, 99};

DEFINE_GRADIENT_PALETTE( rgi_15_gp ) {
    0,   4,  1, 31,
   31,  55,  1, 16,
   63, 197,  3,  7,
   95,  59,  2, 17,
  127,   6,  2, 34,
  159,  39,  6, 33,
  191, 112, 13, 32,
  223,  56,  9, 35,
  255,  22,  6, 38};

DEFINE_GRADIENT_PALETTE( retro2_16_gp ) {
    0, 188,135,  1,
  255,  46,  7,  1};

DEFINE_GRADIENT_PALETTE( Analogous_1_gp ) {
    0,   3,  0,255,
   63,  23,  0,255,
  127,  67,  0,255,
  191, 142,  0, 45,
  255, 255,  0,  0};

DEFINE_GRADIENT_PALETTE( es_pinksplash_08_gp ) {
    0, 126, 11,255,
  127, 197,  1, 22,
  175, 210,157,172,
  221, 157,  3,112,
  255, 157,  3,112};

DEFINE_GRADIENT_PALETTE( es_pinksplash_07_gp ) {
    0, 229,  1,  1,
   61, 242,  4, 63,
  101, 255, 12,255,
  127, 249, 81,252,
  153, 255, 11,235,
  193, 244,  5, 68,
  255, 232,  1,  5};

DEFINE_GRADIENT_PALETTE( Coral_reef_gp ) {
    0,  40,199,197,
   50,  10,152,155,
   96,   1,111,120,
   96,  43,127,162,
  139,  10, 73,111,
  255,   1, 34, 71};

DEFINE_GRADIENT_PALETTE( es_ocean_breeze_068_gp ) {
    0, 100,156,153,
   51,   1, 99,137,
  101,   1, 68, 84,
  104,  35,142,168,
  178,   0, 63,117,
  255,   1, 10, 10};

DEFINE_GRADIENT_PALETTE( es_ocean_breeze_036_gp ) {
    0,   1,  6,  7,
   89,   1, 99,111,
  153, 144,209,255,
  255,   0, 73, 82};

DEFINE_GRADIENT_PALETTE( departure_gp ) {
    0,   8,  3,  0,
   42,  23,  7,  0,
   63,  75, 38,  6,
   84, 169, 99, 38,
  106, 213,169,119,
  116, 255,255,255,
  138, 135,255,138,
  148,  22,255, 24,
  170,   0,255,  0,
  191,   0,136,  0,
  212,   0, 55,  0,
  255,   0, 55,  0};

DEFINE_GRADIENT_PALETTE( es_landscape_64_gp ) {
    0,   0,  0,  0,
   37,   2, 25,  1,
   76,  15,115,  5,
  127,  79,213,  1,
  128, 126,211, 47,
  130, 188,209,247,
  153, 144,182,205,
  204,  59,117,250,
  255,   1, 37,192};

DEFINE_GRADIENT_PALETTE( es_landscape_33_gp ) {
    0,   1,  5,  0,
   19,  32, 23,  1,
   38, 161, 55,  1,
   63, 229,144,  1,
   66,  39,142, 74,
  255,   1,  4,  1};

DEFINE_GRADIENT_PALETTE( rainbowsherbet_gp ) {
    0, 255, 33,  4,
   43, 255, 68, 25,
   86, 255,  7, 25,
  127, 255, 82,103,
  170, 255,255,242,
  209,  42,255, 22,
  255,  87,255, 65};

DEFINE_GRADIENT_PALETTE( gr65_hult_gp ) {
    0, 247,176,247,
   48, 255,136,255,
   89, 220, 29,226,
  160,   7, 82,178,
  216,   1,124,109,
  255,   1,124,109};

DEFINE_GRADIENT_PALETTE( gr64_hult_gp ) {
    0,   1,124,109,
   66,   1, 93, 79,
  104,  52, 65,  1,
  130, 115,127,  1,
  150,  52, 65,  1,
  201,   1, 86, 72,
  239,   0, 55, 45,
  255,   0, 55, 45};

DEFINE_GRADIENT_PALETTE( GMT_drywet_gp ) {
    0,  47, 30,  2,
   42, 213,147, 24,
   84, 103,219, 52,
  127,   3,219,207,
  170,   1, 48,214,
  212,   1,  1,111,
  255,   1,  7, 33};

DEFINE_GRADIENT_PALETTE( ib15_gp ) {
    0, 113, 91,147,
   72, 157, 88, 78,
   89, 208, 85, 33,
  107, 255, 29, 11,
  141, 137, 31, 39,
  255,  59, 33, 89};

DEFINE_GRADIENT_PALETTE( Fuschia_7_gp ) {
    0,  43,  3,153,
   63, 100,  4,103,
  127, 188,  5, 66,
  191, 161, 11,115,
  255, 135, 20,182};
DEFINE_GRADIENT_PALETTE( es_emerald_dragon_08_gp ) {
    0,  97,255,  1,
  101,  47,133,  1,
  178,  13, 43,  1,
  255,   2, 10,  1};

DEFINE_GRADIENT_PALETTE( lava_gp ) {
    0,   0,  0,  0,
   46,  18,  0,  0,
   96, 113,  0,  0,
  108, 142,  3,  1,
  119, 175, 17,  1,
  146, 213, 44,  2,
  174, 255, 82,  4,
  188, 255,115,  4,
  202, 255,156,  4,
  218, 255,203,  4,
  234, 255,255,  4,
  244, 255,255, 71,
  255, 255,255,255};

DEFINE_GRADIENT_PALETTE( fire_gp ) {
    0,   1,  1,  0,
   76,  32,  5,  0,
  146, 192, 24,  0,
  197, 220,105,  5,
  240, 252,255, 31,
  250, 252,255,111,
  255, 255,255,255};

DEFINE_GRADIENT_PALETTE( Colorfull_gp ) {
    0,  10, 85,  5,
   25,  29,109, 18,
   60,  59,138, 42,
   93,  83, 99, 52,
  106, 110, 66, 64,
  109, 123, 49, 65,
  113, 139, 35, 66,
  116, 192,117, 98,
  124, 255,255,137,
  168, 100,180,155,
  255,  22,121,174};

DEFINE_GRADIENT_PALETTE( Magenta_Evening_gp ) {
    0,  71, 27, 39,
   31, 130, 11, 51,
   63, 213,  2, 64,
   70, 232,  1, 66,
   76, 252,  1, 69,
  108, 123,  2, 51,
  255,  46,  9, 35};

DEFINE_GRADIENT_PALETTE( Pink_Purple_gp ) {
    0,  19,  2, 39,
   25,  26,  4, 45,
   51,  33,  6, 52,
   76,  68, 62,125,
  102, 118,187,240,
  109, 163,215,247,
  114, 217,244,255,
  122, 159,149,221,
  149, 113, 78,188,
  183, 128, 57,155,
  255, 146, 40,123};

DEFINE_GRADIENT_PALETTE( Sunset_Real_gp ) {
    0, 120,  0,  0,
   22, 179, 22,  0,
   51, 255,104,  0,
   85, 167, 22, 18,
  135, 100,  0,103,
  198,  16,  0,130,
  255,   0,  0,160};

DEFINE_GRADIENT_PALETTE( es_autumn_19_gp ) {
    0,  26,  1,  1,
   51,  67,  4,  1,
   84, 118, 14,  1,
  104, 137,152, 52,
  112, 113, 65,  1,
  122, 133,149, 59,
  124, 137,152, 52,
  135, 113, 65,  1,
  142, 139,154, 46,
  163, 113, 13,  1,
  204,  55,  3,  1,
  249,  17,  1,  1,
  255,  17,  1,  1};

DEFINE_GRADIENT_PALETTE( BlacK_Blue_Magenta_White_gp ) {
    0,   0,  0,  0,
   42,   0,  0, 45,
   84,   0,  0,255,
  127,  42,  0,255,
  170, 255,  0,255,
  212, 255, 55,255,
  255, 255,255,255};

DEFINE_GRADIENT_PALETTE( BlacK_Magenta_Red_gp ) {
    0,   0,  0,  0,
   63,  42,  0, 45,
  127, 255,  0,255,
  191, 255,  0, 45,
  255, 255,  0,  0};


DEFINE_GRADIENT_PALETTE( BlacK_Red_Magenta_Yellow_gp ) {
    0,   0,  0,  0,
   42,  42,  0,  0,
   84, 255,  0,  0,
  127, 255,  0, 45,
  170, 255,  0,255,
  212, 255, 55, 45,
  255, 255,255,  0};

DEFINE_GRADIENT_PALETTE( Blue_Cyan_Yellow_gp ) {
    0,   0,  0,255,
   63,   0, 55,255,
  127,   0,255,255,
  191,  42,255, 45,
  255, 255,255,  0};


// Single array of defined cpt-city color palettes.
// This will let us programmatically choose one based on
// a number, rather than having to activate each explicitly
// by name every time.
// Since it is const, this array could also be moved
// into PROGMEM to save SRAM, but for simplicity of illustration
// we'll keep it in a regular SRAM array.
//
// This list of color palettes acts as a "playlist"; you can
// add or delete, or re-arrange as you wish.
const TProgmemRGBGradientPalettePtr gGradientPalettes[] = {
  Sunset_Real_gp,
  es_rivendell_15_gp,
  es_ocean_breeze_036_gp,
  rgi_15_gp,
  retro2_16_gp,
  Analogous_1_gp,
  es_pinksplash_08_gp,
  Coral_reef_gp,
  es_ocean_breeze_068_gp,
  es_pinksplash_07_gp,
  es_vintage_01_gp,
  departure_gp,
  es_landscape_64_gp,
  es_landscape_33_gp,
  rainbowsherbet_gp,
  gr65_hult_gp,
  gr64_hult_gp,
  GMT_drywet_gp,
  ib_jul01_gp,
  es_vintage_57_gp,
  ib15_gp,
  Fuschia_7_gp,
  es_emerald_dragon_08_gp,
  lava_gp,
  fire_gp,
  Colorfull_gp,
  Magenta_Evening_gp,
  Pink_Purple_gp,
  es_autumn_19_gp,
  BlacK_Blue_Magenta_White_gp,
  BlacK_Magenta_Red_gp,
  BlacK_Red_Magenta_Yellow_gp,
  Blue_Cyan_Yellow_gp };

  DEFINE_GRADIENT_PALETTE( cottonCandy ) {
    0,  75,  1,221,
   30, 252, 73,255,
   48, 169,  0,242,
  119,   0,149,242,
  170,  43,  0,242,
  206, 252, 73,255,
  232,  78, 12,214,
  255,   0,149,242};

DEFINE_GRADIENT_PALETTE( pastels1 ) {
    0, 107,  1,205,
   35, 255,255,255,
   73, 107,  1,205,
  107,  10,149,210,
  130, 255,255,255,
  153,  10,149,210,
  170,  27,175,119,
  198,  53,203, 56,
  207, 132,229,135,
  219, 255,255,255,
  231, 132,229,135,
  252,  53,203, 56,
  255,  53,203, 56};

DEFINE_GRADIENT_PALETTE( sunconure ) {
    0,  20,223, 13,
  160, 232, 65,  1,
  252, 232,  5,  1,
  255, 232,  5,  1};

DEFINE_GRADIENT_PALETTE( bhw1_three_gp ) {
    0, 255,255,255,
   45,   7, 12,255,
  112, 227,  1,127,
  112, 227,  1,127,
  140, 255,255,255,
  155, 227,  1,127,
  196,  45,  1, 99,
  255, 255,255,255};

DEFINE_GRADIENT_PALETTE( bhw1_w00t_gp ) {
    0,   3, 13, 43,
  104,  78,141,240,
  188, 255,  0,  0,
  255,  28,  1,  1};

DEFINE_GRADIENT_PALETTE( purplefly_gp ) {
    0,   0,  0,  0,
   63, 239,  0,122,
  191, 252,255, 78,
  255,   0,  0,  0};

DEFINE_GRADIENT_PALETTE( bhw2_grrrrr_gp ) {
    0, 184, 15,155,
   35,  78, 46,168,
   84,  65,169,230,
  130,   9,127,186,
  163,  77,182,109,
  191, 242,246, 55,
  216, 142,128,103,
  255,  72, 50,168};

DEFINE_GRADIENT_PALETTE( bhw2_52_gp ) {
    0,   1, 55,  1,
  130, 255,255,  8,
  255,  42, 55,  0};

DEFINE_GRADIENT_PALETTE( bhw2_sunsetx_gp ) {
    0,  42, 55,255,
   25,  73,101,242,
   89, 115,162,228,
  107, 115,162,228,
  114, 100, 77,201,
  127,  86, 23,174,
  142, 190, 32, 24,
  171, 210,107, 42,
  255, 232,229, 67};

DEFINE_GRADIENT_PALETTE( bhw2_turq_gp ) {
    0,   1, 33, 95,
   38,   1,107, 37,
   76,  42,255, 45,
  127, 255,255, 45,
  178,  42,255, 45,
  216,   1,107, 37,
  255,   1, 33, 95};

const TProgmemRGBGradientPalettePtr gGradientPalettesCustom1[] = {
  cottonCandy,
  pastels1,
  sunconure,
  bhw1_three_gp,
  bhw1_w00t_gp,
  purplefly_gp,
  };

const TProgmemRGBGradientPalettePtr gGradientPalettesCustom2[] = {
  Sunset_Real_gp,
  es_rivendell_15_gp,
  es_ocean_breeze_036_gp,
  rgi_15_gp };

// Count of how many cpt-city gradients are defined:
const uint8_t gGradientPaletteCount =
  sizeof( gGradientPalettes) / sizeof( TProgmemRGBGradientPalettePtr );
const uint8_t gGradientPaletteCountCustom1 =
  sizeof( gGradientPalettesCustom1) / sizeof( TProgmemRGBGradientPalettePtr );
const uint8_t gGradientPaletteCountCustom2 =
  sizeof( gGradientPalettesCustom2) / sizeof( TProgmemRGBGradientPalettePtr );
