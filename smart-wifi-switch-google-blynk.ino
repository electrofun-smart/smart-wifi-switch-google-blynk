#include <EEPROM.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <MQTTClient.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>

//for LED status
#include <Ticker.h>
Ticker ticker;

//define your default values here, if there are different values in config.json, they are overwritten.
char blynk_token[34] = "Add your BlynkToken here";
char device_google[16] = "device id here";

WiFiClient net;

MQTTClient client;
// MQTT info
const char *thehostname = "postman.cloudmqtt.com"; // change to your mqtt broker - just needed if using Google Home
const char *user = "xxxxxxx";                     // mqtt user
const char *user_password = "yyyyyyyyyyy";        // mqtt password
const char *id = "ESP01-Smart-Outlet-04";

#define RELAY_PIN 0         // Relay output pin GPIO 0 (ESP8266-1)
#define RESET_PIN 2         // Reset pin GPIO 2 (ESP8266-1) - keep pushed for 5 seconds to reset
#define RESET_INTERVAL 5000 // 5000 miliseconds = 5 seconds

int buttonState = 0; // variable for reading the pushbutton status
int timeCounter = 0; // variable to timer 5 seconds
uint64_t resetTimestamp = 0;

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
    pinMode(RESET_PIN, INPUT);
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

    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "SmartSwitch-02"
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("SmartSwitch-04", "12345678"))
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

    //save the custom parameters to EEPROM
    if (shouldSaveConfig)
    {
        Serial.println("saving config");
        saveEEPROMdata();
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    client.begin(thehostname, 16157, net);
    client.onMessage(messageReceived);
    connect();
    delay(1000);

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
        }
        if (deviceOn == "false")
        {
            digitalWrite(RELAY_PIN, LOW);
            Serial.println("pin low");
        }
    }
}

void loadEEPROMdata()
{
    EEPROM.begin(512);
    EEPROM.get(0, blynk_token);
    EEPROM.get(100, device_google);
    EEPROM.end();
    Serial.println("Recovered credentials from EEPROM:");
    Serial.println(blynk_token);
    Serial.println(device_google);
}

void saveEEPROMdata()
{
    EEPROM.begin(512);
    EEPROM.put(0, blynk_token);
    EEPROM.put(100, device_google);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("Save credentials from EEPROM:");
    Serial.println(blynk_token);
    Serial.println(device_google);
}

void loop()
{
    uint64_t now = millis();

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

    // read the state of the pushbutton value:
    buttonState = digitalRead(RESET_PIN);

    // check if the pushbutton is pressed. If it is, the buttonState is LOW:
    if (buttonState == HIGH)
    {
        resetTimestamp = now;
    }
    else
    {
        if ((now - resetTimestamp) > RESET_INTERVAL)
        {
            resetTimestamp = now;
            Serial.println("Reset reached 5 seconds");
            resetCredentials();
        }
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
