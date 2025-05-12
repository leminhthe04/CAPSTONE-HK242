#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <HTTPClient.h>

using namespace std;

// const char* ssid = "Thin_redmi";
// const char* password = "0963229542";

#define SSID "Thach"
#define PASSWORD "11062011"


#define MQTT_SERVER "thingsboard.cloud"
#define ACCESS_TOKEN "7h9k2d0f58g1x5ocbffl"


#define THINGSBOARD_USERNAME "baokhan327@gmail.com"
#define THINGSBOARD_PASSWORD "baokhan327"
#define DEVICE_ID "7a84a2a0-111a-11f0-b4bd-8b00c50fcae5"

#define BUMP 26
                                                  // 15 minutes
#define SOIL_MOISTURE_CHECK_INTERVAL_WHEN_BUMP_OFF (15*60*1000) 
                                                  // 2 minutes
#define SOIL_MOISTURE_CHECK_INTERVAL_WHEN_BUMP_ON (2*60*1000)
                              // 3 minutes 
#define MAX_AUTO_BUMP_ON_TIME (3*60*1000) 
                                          // 30 minutes
#define BUMP_STATE_UPDATE_PERIOD_INTERVAL (30*60*1000)

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
unsigned long lastCheckSoilMoisture = 0;
unsigned long lastAutoBumpOnTime = 0;

String bumpOnBy = "";
String bumpOffBy = "";
bool isBumpForceOffByRPC = false;


void turnOnBump() {
  digitalWrite(BUMP, LOW);
}

void turnOffBump() {
  digitalWrite(BUMP, HIGH);
}

bool isBumpOn(){ return digitalRead(BUMP) == LOW; }
bool isBumpOff(){ return digitalRead(BUMP) == HIGH; }

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void sendMTTQ(String key, String value, String topic) {
  JsonDocument doc;
  doc[key] = value;
  char buffer[128];
  serializeJson(doc, buffer);
  client.publish(topic.c_str(), buffer);
}

// Called when receive RPC
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  String method = doc["method"];
  if (method == "setBumpStatus") {
    int bumpStatus = doc["params"];
    if (bumpStatus == 1) {
        turnOnBump();
        bumpOnBy = "RPC";
        Serial.println("Bump is on through RPC");
    } else {
      turnOffBump();
      if (bumpOnBy == "Auto") {
        isBumpForceOffByRPC = true;
        Serial.println("Bump forcing off is activated");
      }
      else if (bumpOnBy == "RPC") {
        isBumpForceOffByRPC = false;
        Serial.println("Bump forcing off is cancelled");
      }
      bumpOffBy = "RPC";
      Serial.println("Bump is off through RPC");
    }

    // Respone
    String topicStr = String(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    String requestId = topicStr.substring(lastSlash + 1);

    // Response to topic: v1/devices/me/rpc/response/{requestId}
    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
    String msg = String("Bump is turning ") + String(isBumpOn() ? "on" : "off");
    sendMTTQ("msg", msg, responseTopic);

    // Update state to ThingsBoard
    sendMTTQ("bump_state", isBumpOn() ? "1" : "0", "v1/devices/me/telemetry");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", ACCESS_TOKEN, NULL)) {
      Serial.println("Connected to MQTT");
      client.subscribe("v1/devices/me/rpc/request/+"); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

String getJWTToken() {
  HTTPClient http;
  String url = "https://thingsboard.cloud/api/auth/login";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"username\":\"" + String(THINGSBOARD_USERNAME) + "\",\"password\":\"" + String(THINGSBOARD_PASSWORD) + "\"}";
  int httpCode = http.POST(payload);

  String jwt_token;
  if (httpCode == 200) {
    String response = http.getString();
    JsonDocument doc;
    deserializeJson(doc, response);
    jwt_token = doc["token"].as<String>();
    // Serial.println(jwt_token);
    Serial.println("Get JWT token successfully");
    return jwt_token;
  }

  Serial.println("Failed to get JWT token");
  Serial.println(httpCode);
  return "";
}

vector<int>* getData(vector<String>* keys) {
  String jwt_token = getJWTToken();
  if (jwt_token == "") {
    Serial.println("Failed to get JWT token");
    return nullptr;
  }

  HTTPClient http;
  String keysStr = "";
  for (int i = 0; i < keys->size(); i++) {
    keysStr += keys->at(i) + ",";
  }
  keysStr.remove(keysStr.length() - 1);

  String url = "https://thingsboard.cloud/api/plugins/telemetry/DEVICE/" + String(DEVICE_ID) + "/values/timeseries?keys=" + keysStr;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Authorization", "Bearer " + jwt_token);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    JsonDocument doc;
    deserializeJson(doc, response);
    vector<int>* values = new vector<int>();
    for (int i = 0; i < keys->size(); i++) {
      values->push_back(doc[keys->at(i)][0]["value"].as<int>());
    }
    return values;
  }

  Serial.println("Failed to get data");
  Serial.println(httpCode);
  return nullptr;
}

int getMinSoilMoisture() {
  vector<String>* keys = new vector<String>({"soil_moisture_1(%)", "soil_moisture_2(%)"});
  vector<int>* values = getData(keys);
  if (!values) {
    return -1;
  }
  Serial.println("Soil moisture 1: " + String(values->at(0)));
  Serial.println("Soil moisture 2: " + String(values->at(1)));
  return min(values->at(0), values->at(1));
}


bool isTimeToCheckSoilMoisture() {
  unsigned long now = millis();
  if (now - lastCheckSoilMoisture > (isBumpOn() && bumpOnBy == "Auto" ? SOIL_MOISTURE_CHECK_INTERVAL_WHEN_BUMP_ON : SOIL_MOISTURE_CHECK_INTERVAL_WHEN_BUMP_OFF)) {
    lastCheckSoilMoisture = now;
    return true;
  }
  return false;
}

void checkSoilMoisture() {
  
  if (!isTimeToCheckSoilMoisture()) {
    return;
  }

  Serial.println("Check soil moisture");

  int minSoilMoisture = getMinSoilMoisture();
  if (minSoilMoisture == -1) { 
    Serial.println("Failed to get soil moisture");
    return;
  }

  if (minSoilMoisture < 40) {
    
    if (isBumpOn() || isBumpForceOffByRPC) return;

    turnOnBump();
    bumpOnBy = "Auto";
    lastAutoBumpOnTime = millis(); // start the timer for auto bump on
    Serial.println("Bump is automatically on because soil moisture is too low");
    // also update state to ThingsBoard to change the state of switch
    sendMTTQ("bump_state", "1", "v1/devices/me/telemetry");
    // update bump_auto_state to ThingsBoard
    sendMTTQ("bump_auto_state", "1", "v1/devices/me/telemetry");
    return;
  }
  
  if (minSoilMoisture > 60) {
    isBumpForceOffByRPC = false;

    if (isBumpOff() || bumpOnBy == "RPC") return;

    turnOffBump();
    bumpOffBy = "Auto";
    Serial.println("Bump is automatically off because soil moisture is enough");
    // also update state to ThingsBoard to change the state of switch
    sendMTTQ("bump_state", "0", "v1/devices/me/telemetry");
    // update bump_auto_state to ThingsBoard
    sendMTTQ("bump_auto_state", "0", "v1/devices/me/telemetry");
  }

  // moisture is between 40 and 60, do nothing
}

void checkAutoBumpOnTime() {
  if (isBumpOn() && bumpOnBy == "Auto") { 
    unsigned long now = millis();
    if (now - lastAutoBumpOnTime < MAX_AUTO_BUMP_ON_TIME) return;

    lastAutoBumpOnTime = now;

    turnOffBump();
    bumpOffBy = "Auto";
    isBumpForceOffByRPC = false;

    Serial.println("Auto bump is automatically off because it has been on for too long");
    // also update state to ThingsBoard to change the state of switch
    sendMTTQ("bump_state", "0", "v1/devices/me/telemetry");
    sendMTTQ("bump_auto_state", "0", "v1/devices/me/telemetry");
  }
}


void sendBumpStatePeriodically() {
  unsigned long now = millis();
  if (now - lastMsg > BUMP_STATE_UPDATE_PERIOD_INTERVAL) {
    lastMsg = now;
    sendMTTQ("bump_state", isBumpOn() ? "1" : "0", "v1/devices/me/telemetry");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUMP, OUTPUT);
  turnOffBump();

  setup_wifi();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);

  lastMsg = lastCheckSoilMoisture = lastAutoBumpOnTime = millis();

  reconnect();
  sendMTTQ("bump_state", "0", "v1/devices/me/telemetry");
  sendMTTQ("bump_auto_state", "0", "v1/devices/me/telemetry");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  checkSoilMoisture();

  checkAutoBumpOnTime();

  sendBumpStatePeriodically();
}