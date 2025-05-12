#include <Arduino.h>
// #include <Wire.h> we are removing this because it is already added in liquid crystal library
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h> // for HTTP requests, highly delays
#include <PubSubClient.h> // for MQTT, better than HTTP requests


//************* CONSTANTS *************/

#define NUM_SENSORS 7

#define RAIN 32
#define WATER_LEVEL 33
#define SOIL_MOISTURE_1 34
#define SOIL_MOISTURE_2 35
#define MOISTURE_TEMPERATURE 4
#define SDA_PIN 21
#define SCL_PIN 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define COLLECT_TIME 2000 // collect data every 2 seconds
#define LCD_PRINT_TIME 2000 // print each data on LCD every 2 second

#define MAX_WATER_LEVEL_VALUE 4095

// #define SSID "Thach"
// #define WIFI_PASSWORD "11062011"


#define SSID "Thin_redmi"
#define WIFI_PASSWORD "0963229542"


// #define THINGSBOARD_SERVER "192.168.31.178"
// #define THINGSBOARD_ACCESS_TOKEN "23QPgE3WSphhLdEgZne1"

#define MQTT_PORT 1883

// local server
// #define THINGSBOARD_SERVER "192.168.1.207"
// #define THINGSBOARD_ACCESS_TOKEN "23QPgE3WSphhLdEgZne1"

// free cloud server
#define THINGSBOARD_SERVER "thingsboard.cloud"
#define THINGSBOARD_ACCESS_TOKEN "7h9k2d0f58g1x5ocbffl"


#define ID "ESP32_Client"



/************* FUNCTION DECLARATION *************/

template <typename T>
void printConsole(T n){
  Serial.println(String(n));
}
void printConsole(String n=""){
  Serial.println(n);
}


/************* CLASS DECLARATION *************/

class myESP32Class {
private:
  BH1750 *lightMeter;
  DHT *dht;
  LiquidCrystal_I2C *lcd;
  Adafruit_SSD1306 *oled;

  String thingsBoardURL;

  String * labels;
  String * data;
  String * units;

  long long lastCollectTime;

  long long lastLCDPrintTime;
  int currentSensorIdx;


  WiFiClient* espClient;
  PubSubClient *client;


public:
  myESP32Class() {
    lightMeter = new BH1750();
    dht = new DHT(MOISTURE_TEMPERATURE, DHT22);
    lcd = new LiquidCrystal_I2C(0x27, 16, 2);
    oled = new Adafruit_SSD1306(128, 64, &Wire, -1);
  
    lastCollectTime = 0;

    lastLCDPrintTime = 0;
    currentSensorIdx = 0;

    labels = new String[NUM_SENSORS]{"Light Level", "Humidity", "Temperature", "Water Level", "Rain Level", "Soil Moisture 1", "Soil Moisture 2"};
    data = new String[NUM_SENSORS]{"None", "None", "None", "None", "None", "None", "None"};
    units = new String[NUM_SENSORS]{"lux", "%", "C", "%", "%", "%", "%"};


    thingsBoardURL = "http://" + String(THINGSBOARD_SERVER) + "/api/v1/" +
    THINGSBOARD_ACCESS_TOKEN + "/telemetry";

    espClient = new WiFiClient();
    client = new PubSubClient(*espClient);

  }

  void setup() {
    Serial.begin(9600);
    Wire.begin(SDA_PIN, SCL_PIN);

    setUpLightMeter();
    setUpDHT();
    setUpLCD();
    setUpOLED();
    
    connectWiFi();

    this->client->setServer(THINGSBOARD_SERVER, MQTT_PORT);
  }

  bool isCollectTime() {
    if (millis() - this->lastCollectTime >= COLLECT_TIME) {
      this->lastCollectTime = millis();
      return true;
    }
    return false;
  }

  void setData(String value, int idx){
    if (idx >= 0 && idx < NUM_SENSORS) {
      this->data[idx] = value;
    } else {
      Serial.println("Index out of range!");
    }
  }

private:
  void setUpLightMeter() {
    lightMeter->begin();
  }

  void setUpDHT() {
    dht->begin();
  }

  void setUpLCD() {
    lcd->init();
    lcd->backlight();
    lcd->setCursor(0, 0);
    lcd->print("Hello, world!");
  }

  void setUpOLED() {
    if (!oled->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("Cannot find OLED"));
      while (true);
    }

    oled->clearDisplay();
    oled->setTextSize(1);
    oled->setTextWrap(false);
    oled->setTextColor(WHITE);
    oled->setCursor(10, 10);
    oled->println("Hello, ESP32!");
    oled->display();
  }

  void connectWiFi() {
    WiFi.begin(SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      printConsole("WiFi connecting...");
      printOnLCD("WiFi connecting...");
    }
    printConsole("WiFi connected successfully");
  }

public:
  void printOnLCD(String message, int x = 0, int y = 0) {
    lcd->setCursor(x, y);
    lcd->print(message);
  }

  void printDataOnLCD(String attr, String value, String unit = "") {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(attr + ":");
    lcd->setCursor(0, 1);
    lcd->print(value + unit);
  }


  void printAllDataOnLCD() {
    if (millis() - this->lastLCDPrintTime >= LCD_PRINT_TIME) {
      this->lastLCDPrintTime = millis();
      this->printDataOnLCD(labels[this->currentSensorIdx], data[this->currentSensorIdx], units[this->currentSensorIdx]);
      this->currentSensorIdx = (this->currentSensorIdx + 1) % NUM_SENSORS;
    }
  }

  void printOnOLED(String message, int x = 0, int y = 0) {
    oled->clearDisplay();
    oled->setCursor(x, y);
    oled->println(message);
    oled->display();
  }

  float getLightLevel() {
    return lightMeter->readLightLevel();
  }
  String getLightLevelFormat() {
    return "Light Level: " + String(getLightLevel())  + "lux";
  }

  float getHumidity() {
    return dht->readHumidity();
  }
  String getHumidityFormat(){
    return "Humidity Percentage: " + String(getHumidity()) + "%";
  }
  float getTemperature() {
    return dht->readTemperature();
  }
  String getTemperatureFormat(){
    return "Temperature: " + String(getTemperature()) + "°C";
  }
  int getWaterLevel() {
    return analogRead(WATER_LEVEL);
  }
  int getWaterLevelPercentage() {
    return map(getWaterLevel(), 0, MAX_WATER_LEVEL_VALUE, 0, 100);
  }
  String getWaterLevelPercentageFormat(){
    return "Water Level Percentage: " + String(getWaterLevelPercentage()) + "%";
  }
  int getRainLevel() {
    return analogRead(RAIN);
  }
  int getRainLevelPercentage() {
    return map(getRainLevel(), 0, 4095, 100, 0);
  }
  String getRainLevelPercentageFormat(){
    return "Rain Level Percentage: " + String(getRainLevelPercentage()) + "%";
  }

  int getSoilMoisture1() {
    return analogRead(SOIL_MOISTURE_1);
  }
  int getSoilMoisture2() {
    return analogRead(SOIL_MOISTURE_2);
  }
  int getSoilMoisture1Percentage() {
    return map(getSoilMoisture1(), 0, 4095, 100, 0);
  }
  
  String getSoilMoisture1PercentageFormat(){
    return "Soil Moisture 1 Percentage: " + String(getSoilMoisture1Percentage()) + "%";
  }
  int getSoilMoisture2Percentage() {
    return map(getSoilMoisture2(), 0, 4095, 100, 0);
  }

  String getSoilMoisture2PercentageFormat(){
    return "Soil Moisture 2 Percentage: " + String(getSoilMoisture2Percentage()) + "%";
  }

  void sendDataToThingsBoard(String key, String value) {
    if (WiFi.status() != WL_CONNECTED) {
      printConsole("WiFi is not connected!");
      return;
    }
  
    HTTPClient http;
    http.begin(this->thingsBoardURL);
    http.addHeader("Content-Type", "application/json");
  
    String payload = "{ " + key + ": " + value + " }";

    int httpResponseCode = http.POST(payload);
  
    // Serial.print("Send data: ");
    // Serial.println(payload);
    // Serial.print("Server response code: ");
    // Serial.println(httpResponseCode); // 200 là thành công
  
    printConsole("Send data: " + payload);
    printConsole("Server response code: " + String(httpResponseCode));
    http.end();
  }
  

  void reconnectMQTT() {
    while (!client->connected()) {
        printConsole("Connecting to MQTT...");
        if (client->connect(ID, THINGSBOARD_ACCESS_TOKEN, NULL, NULL, 0, 0, NULL, true)) {
            printConsole(" connected!");
        } else {
            printConsole(" failed, rc=");
            printConsole(client->state());
            printConsole(" retrying in 1 seconds...");
            delay(1000);
        }
    } 
  }

  void sendDataToThingsBoardByMQTT(String key, String value) {
    if (!client->connected()) {
      reconnectMQTT();
    }
    client->loop();
    String payload = "{ " + key + ": " + value + " }";
    client->publish("v1/devices/me/telemetry", payload.c_str());
    printConsole("Send data: " + payload);
  }
};


/************* MAIN CODE *************/

myESP32Class myESP32;

void setup(){
  myESP32.setup();
}

void loop()
{

  if (myESP32.isCollectTime()) {

    String lightLevel = String(myESP32.getLightLevel());
    myESP32.setData(lightLevel, 0);   
    printConsole(myESP32.getLightLevelFormat());
    // myESP32.sendDataToThingsBoard("light_level(lux)", String(myESP32.getLightLevel()));
    myESP32.sendDataToThingsBoardByMQTT("light_level(lux)", String(myESP32.getLightLevel()));

    String humidity = String(myESP32.getHumidity());  
    myESP32.setData(humidity, 1);
    printConsole(myESP32.getHumidityFormat());
    // myESP32.sendDataToThingsBoard("humidity(%)", String(myESP32.getHumidity()));
    myESP32.sendDataToThingsBoardByMQTT("humidity(%)", String(myESP32.getHumidity()));

    String temperature = String(myESP32.getTemperature());
    myESP32.setData(temperature, 2);
    printConsole(myESP32.getTemperatureFormat());
    // myESP32.sendDataToThingsBoard("temperature(°C)", String(myESP32.getTemperature()));
    myESP32.sendDataToThingsBoardByMQTT("temperature(°C)", String(myESP32.getTemperature()));

    String waterLevelPercentage = String(myESP32.getWaterLevelPercentage());
    myESP32.setData(waterLevelPercentage, 3);
    printConsole("Water Level: " + String(myESP32.getWaterLevel()));
    printConsole(myESP32.getWaterLevelPercentageFormat());
    // myESP32.sendDataToThingsBoard("water_level(%)", String(myESP32.getWaterLevelPercentage()));
    myESP32.sendDataToThingsBoardByMQTT("water_level(%)", String(myESP32.getWaterLevelPercentage()));

    String rainLevelPercentage = String(myESP32.getRainLevelPercentage());
    myESP32.setData(rainLevelPercentage, 4);
    printConsole("Rain Level: " + String(myESP32.getRainLevel()));
    printConsole(myESP32.getRainLevelPercentageFormat());
    // myESP32.sendDataToThingsBoard("rain_level(%)", String(myESP32.getRainLevelPercentage()));
    myESP32.sendDataToThingsBoardByMQTT("rain_level(%)", String(myESP32.getRainLevelPercentage()));

    String soilMoisture1Percentage = String(myESP32.getSoilMoisture1Percentage());
    myESP32.setData(soilMoisture1Percentage, 5);
    printConsole("Soil Moisture 1: " + String(myESP32.getSoilMoisture1()));
    printConsole(myESP32.getSoilMoisture1PercentageFormat());
    // myESP32.sendDataToThingsBoard("soil_moisture_1(%)", String(myESP32.getSoilMoisture1Percentage()));
    myESP32.sendDataToThingsBoardByMQTT("soil_moisture_1(%)", String(myESP32.getSoilMoisture1Percentage()));

    String soilMoisture2Percentage = String(myESP32.getSoilMoisture2Percentage());
    myESP32.setData(soilMoisture2Percentage, 6);
    printConsole("Soil Moisture 2: " + String(myESP32.getSoilMoisture2()));
    printConsole(myESP32.getSoilMoisture2PercentageFormat());
    // myESP32.sendDataToThingsBoard("soil_moisture_2(%)", String(myESP32.getSoilMoisture2Percentage()));
    myESP32.sendDataToThingsBoardByMQTT("soil_moisture_2(%)", String(myESP32.getSoilMoisture2Percentage()));

    printConsole();
  }

  myESP32.printAllDataOnLCD();
}

