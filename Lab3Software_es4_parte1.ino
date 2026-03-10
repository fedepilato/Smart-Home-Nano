#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <PDM.h>
#include <LiquidCrystal_PCF8574.h>

//file con credenziali
#include "secrets.h"

const int GROUP_ID = 0;
int BROKER_PORT = 1883;
String BROKER = "";

String base_topic = "/tiot/" + String(GROUP_ID); // topic per MQTT 
String pub_topic_temperatura = base_topic + "/temperature";
String pub_topic_led = base_topic + "/led";
String pub_topic_resence = base_topic + "/presence";
String pub_topic_noise = base_topic + "/noise";
String sub_topic_led = base_topic + "/led";
String sub_topic_fan = base_topic + "/fan";
String sub_topic_display = base_topic + "/display";

int flagMessage = 0;
String message;

char server_address[] = "192.168.194.120";
int server_port = 8080;
WiFiClient wifi;
HttpClient client = HttpClient(wifi, server_address, server_port);

LiquidCrystal_PCF8574 lcd(0x20);
short int sampleBuffer[256];
float temperatura = 0.0;

volatile int samplesRead = 0;
volatile int flagMicrofono = 0;

const int LED_PIN = A2;
volatile int lux = 0;

const int TEMP_PIN = A0;
const int B = 4275;
const long int R0 = 100000;

const int intervalloLettura = 2000;  //10 secondi in millisecondi
unsigned int ultimaLettura = 0; //da aggiornare per memorizzare ultimo tempo di lettura
const int stepSize = 51;

const int FAN_PIN = A1;
volatile int currentSpeed = 0;

const int PIR_PIN = 4;
const int timeout_pir = 5000;
volatile int flagPir = 0;
unsigned int ultimaLetturaPir = 0;

volatile int flagMic = 0;
//10 = numero in buffer 
int N = 10;
volatile int indice = 0;
volatile int circularBuffer[10] = {0};
const int sound_interval = 10*60*1000; // 10 min
const int sound_theshold = 10000; // Da prof, forse

volatile int personPresence = 0;

float min_temp_risc;
float max_temp_risc;
float max_temp_lux;
float min_temp_lux;
int percentage_lux = ((float) lux /255 ) * 100;
int percentage_speed = ((float) currentSpeed /255)*100;

unsigned long lastScreenSwitch = 0;
bool screenToggle = false;

// aggiunta per pto 9
const int sound_theshold_clap = 12000;
const int GREEN_LED = A3;
volatile int clapBuffer[2] = {0};
const int clapWindow = 10*1000;
volatile int clapIndex = 0;
volatile int lampState = 0;
volatile int lastTimeVerifyClap = 0;
const int clapResetTime = 10*1000;
// dichiarazione funzioni per pulizia codice

void addBuffer(int val);
void checkPresencePir();
void onPDMdata();
void verifyPresence();
float calculateTemperature(int a);

//---------------------------------------------REST-------------------------------------------
void printResponse(WiFiClient client, int code, String body){
  client.println("HTTP/1.1 "+ String(code));
  if (code == 200){
    client.println("Content-type: application/json; charset=utf-8");
    client.println();
    client.println(body);
  }else {
    client.println();
  }
}
void process(WiFiClient client){ // prende info da REST
  String req_type = client.readStringUntil(' ');
  req_type.trim();
  String url = client.readStringUntil(' ');
  url.trim();
  if (url.startsWith("/led/")){
    String led_val = url.substring(5);
    Serial.println(led_val);
    if (led_val == "0" || led_val == "1"){
      int int_val = led_val.toInt();
      printResponse(client, 200, senMlEncode("led", int_val, "lux"));
    }else{
      printResponse(client, 400, senMlEncode("led", -1, "lux"));
    }
  }else{
    printResponse(client, 404, senMlEncode("led", -1, "lux"));
  }
}

//--------------------------------------------------------------------------------------------


//---------------------------------------------MQTT-------------------------------------------
PubSubClient mqttClient(wifi);

const size_t capacity = JSON_OBJECT_SIZE(4) + 60;
DynamicJsonDocument doc(capacity);
const size_t JSON_CAPACITY = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(4) + 100;
DynamicJsonDocument doc_out(JSON_CAPACITY);
DynamicJsonDocument doc_in(JSON_CAPACITY);


String senMlEncode(const String& name, float value, const String& unit) {
  doc_out.clear();
  doc_out["bn"] = "ArduinoGroup" + String(GROUP_ID);
  JsonObject e = doc_out["e"].createNestedObject();
  e["n"] = name;
  e["t"] = millis() / 1000;
  e["v"] = value;
  e["u"] = unit;

  String json;
  if (serializeJson(doc_out, json) == 0) {
    Serial.println(F("Errore serializzazione JSON"));
    return "";
  }
  return json;
}


String senMlEncodeRegistration(String topic) {
  doc_out.clear();
  doc_out["bn"] = "ArduinoGroup" + String(GROUP_ID);
  doc_out["deviceID"] = 12123;
  doc_out["endpoint"] = topic.c_str();

  String json;
  if (serializeJson(doc_out, json) == 0) {
    Serial.println(F("Errore serializzazione JSON"));
    return "";
  }
  return json;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("Messaggio su topic: "));
  Serial.println(topic);

  payload[length] = '\0';

  DeserializationError err = deserializeJson(doc_in, payload);
  if (err) {
    Serial.print(F("Errore parsing JSON: "));
    Serial.println(err.c_str());
    return;
  }

  if (!doc_in.containsKey("e") || !doc_in["e"].is<JsonArray>()) {
    Serial.println(F("JSON invalido: 'e' mancante o non array"));
    return;
  }

  JsonArray events = doc_in["e"];
  if (events.size() == 0 || !events[0].is<JsonObject>()) {
    Serial.println(F("JSON invalido: 'e[0]' non presente o non oggetto"));
    return;
  }

  JsonObject event = events[0];

  if (event["n"] == "led" && event["v"].is<int>()) {
    int state = event["v"];
    analogWrite(LED_PIN, state == 1 ? HIGH : LOW);
    Serial.println(state == 1 ? F("LED ACCESO") : F("LED SPENTO"));
  } else if (event["n"] == "fan" && event["v"].is<int>()) {
    int state = event["v"];
    if(state == 1) {
      if(currentSpeed + stepSize <= 255) {
        currentSpeed += stepSize;
      }
    } else if(state == 0) {
      if(currentSpeed - stepSize >= 0) {
        currentSpeed -= stepSize;
      }
    } 
  } else if(event["n"] == "display"){
    flagMessage = 1;
    message = event["v"].as<String>();
  } else {
    Serial.println(F("JSON non contiene comando valido"));
  }

  //1 aumenta, 0 diminuisce


}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print(F(" Connessione MQTT... "));
    String clientId = "ArduinoGroup" + String(GROUP_ID) + "-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(F(" Connesso!"));
      mqttClient.subscribe(sub_topic_led.c_str());
      mqttClient.subscribe(sub_topic_fan.c_str());
      mqttClient.subscribe(sub_topic_display.c_str());
    } else {
      Serial.print(F(" MQTT errore "));
      Serial.print(mqttClient.state());
      Serial.println(F(", retry tra 5 sec"));
      delay(5000);
    }
  }
}



//---------------------------------------------------------------------------------------------

void checkPresencePir() {
  int pirState = digitalRead(PIR_PIN);
  if(pirState){
    flagPir = 1;
    ultimaLetturaPir = millis();
  } else {
    if(millis() - ultimaLetturaPir >= timeout_pir) {
      flagPir = 0;
    }
  }
}

float calculateTemperature(int a) { 
  float R = R0 * (1023.0/a-1.0);
  float T = 1.0 / (log(R/R0)/B+1/298.15)-273.15;
  return T;
}


void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
  for (int i = 0; i < samplesRead ; i++){
    if (sampleBuffer[i] < 0) {
      sampleBuffer[i] = sampleBuffer[i] * -1;
    }
    if (sampleBuffer[i] > sound_theshold){ 
      addBuffer(millis()); //c'è almeno una persona e notifico al buffer
      break;
    }
  }


  verifyPresence();
}

void verifyClap(){
  if (abs(clapBuffer[1] - clapBuffer[0]) <= clapWindow){
    if (lampState == 1){
      lampState = 0;
      digitalWrite(GREEN_LED, LOW);
    } else{
      lampState = 1;
      digitalWrite(GREEN_LED, HIGH);
      lastTimeVerifyClap = millis();
    }
  }

}
void addClapBuffer(int val){
  clapBuffer[clapIndex] = val;
  clapIndex = (clapIndex + 1) % 2; // mi bastano solo due battiti
}
void addBuffer(int val){
  circularBuffer[indice] = val;
  indice = (indice + 1)  % N;
}

void verifyPresence(){
  flagMic = 1;
  for (int i = 0; i < N; i++){
    if (millis() - circularBuffer[i] >= sound_interval){
      flagMic = 0; // significa che negli ultimi dieci minuti ho trovato meno di 10 persone.
      break;
    }
  }
}


void setup() {
  Serial.begin(9600);
  while(!Serial);
  PDM.onReceive(onPDMdata);
  if(!PDM.begin(1,16000)) {
    while(1);
  }
  pinMode(TEMP_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), checkPresencePir, CHANGE);
  analogWrite(FAN_PIN, currentSpeed);
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.home();
  lcd.clear();

//---------------------------------------------------------------WIFI-------------------------------------
  Serial.println(F("Connessione WiFi..."));
  WiFi.begin(USERID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(USERID, PASSWORD);
    delay(1000);
    Serial.print(F("."));
  }

  Serial.println(F("\n WiFi connesso"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  String body = senMlEncodeRegistration(sub_topic_led);
  client.beginRequest();
  client.post("/devices/subscription");
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", body.length());
  client.beginBody();
  client.print(body);
  client.endRequest();

  body = senMlEncodeRegistration(sub_topic_fan);
  client.beginRequest();
  client.post("/devices/subscription");
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", body.length());
  client.beginBody();
  client.print(body);
  client.endRequest();

  client.beginRequest();
  client.get("/services/subscription/Broker-Mqtt");
  client.endRequest();
  int httpCode = client.responseStatusCode();
  Serial.println(httpCode);
  if (httpCode > 0){
    Serial.println(httpCode);
    String payload = client.responseBody();
    DeserializationError error = deserializeJson(doc, payload);
    BROKER = doc["Broker"].as<String>();
    BROKER_PORT = doc["Port"];
    mqttClient.setServer(BROKER.c_str(), BROKER_PORT);
    mqttClient.setCallback(callback);
  }
//------------------------------------------------------------------------------------------------------------------

}

void loop() {
  if(millis() - ultimaLettura >= intervalloLettura) {
    ultimaLettura = millis();
    // ** punto 5
    //noInterrupts();
    personPresence = 0; // se nessuno dei due flag attivi, rimane a 0
    if (flagMic || flagPir){
      personPresence = 1;
    }
    //interrupts();
    if (personPresence == 0 && lampState == 1 && (millis() - lastTimeVerifyClap >= clapResetTime)){ // se non ci sono persone, la lampadina e accesa e dopo l'ultima volta che l'ho accesa sono passi 10 secondi
      lampState = 0;
      digitalWrite(GREEN_LED, LOW);
    }
    int a = analogRead(TEMP_PIN);
    temperatura = calculateTemperature(a);

    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      if (cmd.startsWith("SET MIN_TEMP_RISC ")) {
        min_temp_risc = cmd.substring(18).toFloat();
      } else if (cmd.startsWith("SET MAX_TEMP_RISC ")){
        max_temp_risc = cmd.substring(18).toFloat();
      } else if (cmd.startsWith("SET MIN_TEMP_LUX ")){
        min_temp_lux = cmd.substring(17).toFloat();
      } else  if (cmd.startsWith("SET MAX_TEMP_LUX ")){
        max_temp_lux = cmd.substring(17).toFloat();
      } else {
        Serial.println("Comando Errato.\n");
      }
    }
    
    
    //** punto 6 | ampiezze degli intervalli invariata
    // se ci sono persone la temperatura di soglia per il riscaldamento diminuisce, mentre per la luce aumenta

    max_temp_risc = 30.0 - 2.5* personPresence;
    min_temp_risc = 25.0 - 2.5* personPresence;

    max_temp_lux = 20.0 + 2.5* personPresence;
    min_temp_lux = 15.0 + 2.5* personPresence;
   
    Serial.print("Temperatura: ");
    Serial.println(temperatura);
    Serial.print("Dati: ");
    Serial.print(flagMic);
    Serial.print(" ");
    Serial.print(flagPir);
    Serial.print(" ");

    Serial.print(personPresence);
    Serial.print(" ");
    Serial.print(max_temp_risc);
    Serial.print(" ");
    Serial.println(max_temp_lux);
    Serial.print("velocita ventola");
    Serial.println(currentSpeed);
    Serial.print("");
    Serial.println(message);
    Serial.print("");
    Serial.print(flagMessage);

    
  //   if(temperatura >= min_temp_lux && temperatura <= max_temp_lux) {  //dai 15 ai 20
  //     lux = round((max_temp_lux - temperatura) * (255.0/5.0));
  //   } if (temperatura > max_temp_lux) {
  //     lux = 0;
  //   } if (temperatura < min_temp_lux) {
  //     lux = 255;
  //   }
  //   analogWrite(LED_PIN, lux);


  
  //   if (temperatura >= min_temp_risc && temperatura <= max_temp_risc) { // DutyCycleMin = 0 | DutyCycleMax = 255
  //     currentSpeed = round(abs(temperatura - min_temp_risc) * (255.0/5.0));
  //   } if (temperatura > max_temp_risc){
  //     currentSpeed = 255.0;
  //   } if (temperatura < min_temp_risc){
  //     currentSpeed = 0.0;
  //   }
  //   analogWrite(FAN_PIN, currentSpeed);
  //   percentage_lux = (lux /255 ) * 100;
  //   percentage_speed = (currentSpeed /255)*100;
  // }  
  // if (millis() - lastScreenSwitch > 5000) {
  //   screenToggle = !screenToggle;
  //   lastScreenSwitch = millis();
  

  //   lcd.clear();
  //   lcd.home();
  //   lcd.setCursor(0, 0);
  //   if (screenToggle) {
  //     lcd.print("T:");
  //     lcd.print(temperatura);
  //     lcd.print("Pres:");
  //     lcd.print(personPresence);
  //     lcd.setCursor(0, 1);
  //     lcd.print("AC:");
  //     lcd.print(percentage_lux);
  //     lcd.print(" HT:");
  //     lcd.print(percentage_speed);
  //   } else {
  //     lcd.print("AC m:");
  //     lcd.print(min_temp_lux);
  //     lcd.print(" M:");
  //     lcd.print(max_temp_lux);
  //     lcd.setCursor(0, 1);
  //     lcd.print("HT:");
  //     lcd.print(min_temp_risc);
  //     lcd.print("M:");
  //     lcd.print(max_temp_risc);
  //   }
  }
  if(flagMessage) {
    flagMessage = 0;
    lcd.clear();
    lcd.home();
    lcd.setCursor(0, 0);
    lcd.print(message);
  }
  analogWrite(FAN_PIN, currentSpeed);
  if (!mqttClient.connected()) {
    reconnect(); // si connette e regsitra al topic MQTT
  }
  mqttClient.loop();

  static unsigned long lastSent = 0;
  if (millis() - lastSent >= 60000) { // ogni 30 secondi
    lastSent = millis();

    String payload1 = senMlEncode("temperature", temperatura, "Cel");
    if (!payload1.isEmpty()) {
      mqttClient.publish(pub_topic_temperatura.c_str(), payload1.c_str()); // pubblica la temperatura su quel topic
    }
    String payload2 = senMlEncode("led", lampState, "bool");
    if (!payload2.isEmpty()) {
      mqttClient.publish(pub_topic_led.c_str(), payload2.c_str()); // pubblica la temperatura su quel topic
    }

    String payload3 = senMlEncode("person", flagPir, "bool");
    if (!payload3.isEmpty()) {
      mqttClient.publish(pub_topic_resence.c_str(), payload3.c_str()); // pubblica la temperatura su quel topic
    }

    String payload4 = senMlEncode("noise", flagMic, "bool");
    if (!payload4.isEmpty()) {
      mqttClient.publish(pub_topic_noise.c_str(), payload4.c_str()); // pubblica la temperatura su quel topic
    }
  }
}

