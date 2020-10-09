
#include <EEPROM.h>
#include <ESP8266WiFi.h> //http://github.com/esp8266/Arduino
#include <MQTTClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //http://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> //http://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>

//for LED status
#include <Ticker.h>
Ticker ticker;

#define UPDATE_INTERVAL 1000 // 1000 miliseconds = 1 seconds

//message values on captive portal input fields
char blynk_token[34] = "Add your BlynkToken here";
char device_google[16] = "device id here";
char userid[16] = "user id here";

WiFiClient net;

MQTTClient client;

// MQTT info and Google Smart Home User Id - change the values below according to your project
const char *thehostname = "postman.cloudmqtt.com"; // change to your mqtt broker
const char *user = "xxxx"; // change to your mqtt user
const char *user_password = "yyyy";// change to your mqtt password
const char *id = "ESP01-Smart-Outlet"; //// change to your mqtt clientid
// -------------------------------------------------------------------------------

#define RELAY_PIN 0        // Relay output pin GPIO 0 (ESP8266-1)
const byte keyPin = 2;     // Reset pin GPIO 2 (ESP8266-1) - keep pushed for 5 seconds to reset

unsigned long keyPrevMillis = 0;
const unsigned long keySampleIntervalMs = 25;
byte longKeyPressCountMax = 200;    // 200 * 25 = 5000 ms
byte longKeyPressCount = 0;

byte prevKeyState = HIGH;         // button is active low

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void setup()
{
    // put your setup code here, to run once:
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(keyPin, INPUT);
    pinMode(BUILTIN_LED, OUTPUT); //set led pin as output
    // start ticker with 0.5 because we start in AP mode and try to connect
    ticker.attach(0.6, tick);

    Serial.begin(115200);
    Serial.println();

    loadEEPROMdata();

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);
    WiFiManagerParameter custom_device_google("deviceid", "device id", device_google, 16);
    WiFiManagerParameter custom_userid_google("userid", "user id", userid, 16);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add all your parameters here
    wifiManager.addParameter(&custom_blynk_token);
    wifiManager.addParameter(&custom_device_google);
    wifiManager.addParameter(&custom_userid_google);

    //reset settings - for testing
    //wifiManager.resetSettings();

    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "SmartSwitch-02"
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("SmartSwitch-05", "12345678"))
    {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    ticker.detach();
    //keep LED off
    digitalWrite(BUILTIN_LED, HIGH);

    //read updated parameters
    strcpy(blynk_token, custom_blynk_token.getValue());
    strcpy(device_google, custom_device_google.getValue());
    strcpy(userid, custom_userid_google.getValue());

    //save the custom parameters to EEPROM
    if (shouldSaveConfig)
    {
        Serial.println("saving config");
        saveEEPROMdata();
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    if (device_google != ""){
        client.begin(thehostname, 16157, net);
        client.onMessage(messageReceived);
        connect();
        delay(1000);
    }
    Blynk.config(blynk_token);

    if (!Blynk.connect())
    {
        Serial.println("Blynk connection timed out.");
        //handling code (reset)
    }
    else
    {
        Serial.println("Blynk connected");
    }
}

void connect()
{
    Serial.print("checking wifi…");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.print("\nconnecting…");
    while (!client.connect(id, user, user_password))
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nconnected!");
    String devId = device_google;
    Serial.println("device id : " + devId);
    client.subscribe(devId + "-client");
    delay(1000);
}

void messageReceived(String &topic, String &payload)
{
    Serial.println("incoming: " + topic + " - " + payload);
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(payload);
    String deviceOn = json["on"];
    String devId = device_google;
    if (topic == (devId + "-client"))
    {
        if (deviceOn == "true")
        {
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("pin high");
            updateBlynk(1);
        }
        if (deviceOn == "false")
        {
            digitalWrite(RELAY_PIN, LOW);
            Serial.println("pin low");
            updateBlynk(0);
        }
    }
}

void loadEEPROMdata()
{
    EEPROM.begin(512);
    EEPROM.get(0, blynk_token);
    EEPROM.get(100, device_google);
    EEPROM.get(150, userid);
    EEPROM.end();
    Serial.println("Recovered credentials from EEPROM:");
    Serial.println(blynk_token);
    Serial.println(device_google);
    Serial.println(userid);
}

void saveEEPROMdata()
{
    EEPROM.begin(512);
    EEPROM.put(0, blynk_token);
    EEPROM.put(100, device_google);
    EEPROM.put(150, userid);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("Save credentials from EEPROM:");
    Serial.println(blynk_token);
    Serial.println(device_google);
    Serial.println(userid);
}

void loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!client.connected())
        {
            connect();
        }
        else
        {
            client.loop();
        }

        Blynk.run();
    }

    if (millis() - keyPrevMillis >= keySampleIntervalMs) {
        keyPrevMillis = millis();
       
        byte currKeyState = digitalRead(keyPin);
       
        if ((prevKeyState == HIGH) && (currKeyState == LOW)) {
            keyPress();
        }
        else if ((prevKeyState == LOW) && (currKeyState == HIGH)) {
            keyRelease();
        }
        else if (currKeyState == LOW) {
            longKeyPressCount++;
        }
       
        prevKeyState = currKeyState;
    }
}

void resetCredentials()
{
    WiFi.disconnect();
    delay(1000);
    setup();
}

void tick()
{
    //toggle state
    int state = digitalRead(BUILTIN_LED); // get the current state of GPIO1 pin
    digitalWrite(BUILTIN_LED, !state);    // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
    Serial.println("Entered config mode");
    //entered config mode, make led toggle faster
    ticker.attach(0.2, tick);
}

// called when button is kept pressed for less than 5 seconds
void shortKeyPress() {
    Serial.println("short");
    //toggle state
    int state = digitalRead(RELAY_PIN); // get the current state of GPIO 0 pin
    Serial.print("state = ");
    Serial.println(state);
    digitalWrite(RELAY_PIN, !state);    // set pin to the opposite state
    updateBlynk(!state);
    updateGoogle(!state);
}

// called when button is kept pressed for more than 5 seconds
void longKeyPress() {
    Serial.println("long");
    resetCredentials();
}


// called when key goes from not pressed to pressed
void keyPress() {
    Serial.println("key press");
    longKeyPressCount = 0;
}


// called when key goes from pressed to not pressed
void keyRelease() {
    Serial.println("key release");
   
    if (longKeyPressCount >= longKeyPressCountMax) {
        longKeyPress();
    }
    else {
        shortKeyPress();
    }
}

BLYNK_WRITE(V0) // V0 is the number of Virtual Pin  
{
  int pinValue = param.asInt();
  Serial.print("Blink V0 = ");
  Serial.println(pinValue);
  digitalWrite(RELAY_PIN, pinValue);    // set pin to the opposite state
  updateGoogle(pinValue);
}

void updateBlynk(int state){
    Serial.print("Update blynk with Value = ");
    Serial.println(state);
    Blynk.virtualWrite(0, state);
}

void updateGoogle(int state){
  Serial.print("Update google with Value = ");
  Serial.println(state);
  sendhttp(state);
}


void sendhttp(int state)
{

  if (device_google != ""){
  HTTPClient http;

  http.begin("http://<your-smart-home-end-point>/smarthome/update");
  http.addHeader("Content-Type", "application/json");

  Serial.print("[http] POST...\n");
  // start connection and send HTTP header

  StaticJsonBuffer<256> jsonBuffer;
  JsonObject &res = jsonBuffer.createObject();

  res["userId"] = userid;
  res["deviceId"] = device_google;
  res["errorCode"] = "";

  JsonObject &states = res.createNestedObject("states");
  states["online"] = true;
  if (state == 1){
      states["on"] = true;
  }else{
    states["on"] = false;
  }
  String msg;
  res.prettyPrintTo(Serial);
  res.printTo(msg);

  int httpCode = http.POST(msg);

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[http] POST... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      Serial.println(payload);
    }
  }
  else
  {
    Serial.printf("[http] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  }
}
