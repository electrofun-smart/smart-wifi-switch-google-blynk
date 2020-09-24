#include <EEPROM.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <MQTTClient.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char blynk_token[34] = "Add your BlynkToken here";
char device_google[16] = "device id here";

WiFiClient net;

MQTTClient client;
// MQTT info
const char *thehostname = "postman.cloudmqtt.com"; // change to your mqtt broker
const char *user = "xxxxxxx";                     // mqtt user
const char *user_password = "yyyyyyyyyyy";        // mqtt password
const char *id = "ESP01-Smart-Outlet-01";

#define RELAY_PIN 0

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {
  // put your setup code here, to run once:
  pinMode(RELAY_PIN, OUTPUT);  
  Serial.begin(115200);
  Serial.println();

  loadBlynkToken();
  
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);
  WiFiManagerParameter custom_device_google("deviceid", "device id", device_google, 16);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_device_google);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "SmartSwitch-Electrofun"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("SmartSwitch-Electrofun-01", "12345678")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(device_google, custom_device_google.getValue());

  //save the custom parameters to EEPROM
  if (shouldSaveConfig) {
    Serial.println("saving config");
    saveBlynkToken();
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  client.begin(thehostname, 16157, net);
  client.onMessage(messageReceived);
  connect();
  delay(1000);
  
  Blynk.config(blynk_token);

  if(!Blynk.connect()) {
     Serial.println("Blynk connection timed out.");
     //handling code (reset)
  }else{
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
    if (deviceOn == "true"){
      digitalWrite(RELAY_PIN, HIGH); 
      Serial.println("pin high");
    }
    if (deviceOn == "false"){
      digitalWrite(RELAY_PIN, LOW); 
      Serial.println("pin low");
    }
  }
}

void loadBlynkToken() {
  EEPROM.begin(512);
  EEPROM.get(0, blynk_token);
  EEPROM.get(100, device_google);
  EEPROM.end();
  Serial.println("Recovered credentials from EEPROM:");
  Serial.println(blynk_token);
  Serial.println(device_google);
}

void saveBlynkToken() {
  EEPROM.begin(512);
  EEPROM.put(0, blynk_token);
  EEPROM.put(100, device_google);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Save credentials from EEPROM:");
  Serial.println(blynk_token);
  Serial.println(device_google);
}

void loop() {
  if (!client.connected())
  {
    connect();
  } else {
    client.loop();
  }
  Blynk.run();
}
