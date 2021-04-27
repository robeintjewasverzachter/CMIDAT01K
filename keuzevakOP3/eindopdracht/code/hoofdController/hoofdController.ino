#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
// Update these with values suitable for your network.

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (255)
char msg[MSG_BUFFER_SIZE];
int value = 0;
String inputString;
bool stringComplete = false;

void setup_wifi() {

  delay(10);
  Serial.print("Connecting to Wifi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  //display the error given by the matrix to the user
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("error");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}
String incomming = "";
bool inputGiven = false;
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if (Serial.available() > 0) {
    // read the incoming byte:
    char f = (char)Serial.read();
    
    if(f == '\n') {
      Serial.println(incomming);
      inputGiven = true;
    }else{
      incomming += f;
    }
  }
  if(!inputGiven) return;
  //convert the user input to an string array so that every word can be checked
  int str_len = incomming.length() + 1; 
  char char_array[str_len];
  incomming.toCharArray(char_array, str_len);
  //allow a maximum of 5 words
  String strArray[5];
  int index = 0;
  //check how many words there actually are
  for(int i = 0; i < sizeof(char_array); i++){
    if(char_array[i] == ' '){
      index++;
      i++;  
    }
    if(index > sizeof(strArray))break;
    strArray[index] += char_array[i];
  }
  int dataAmount = 0;
  //check how many numerical values there are, for RGB or brightness
  for(int i = 0; i <= index; i++){
    bool isNumber = true;
    for(int j = 0; j < strArray[i].length(); j++){
      //don't check with the endline 
      if(i == index && j+2 == strArray[i].length()) break;
      if(!isDigit(strArray[i].charAt(j))){
          isNumber = false;
          break;
      }
    }
    if(isNumber){
      dataAmount++;
    }
  }
  index++;
  bool validFormat = true;
  String json = "";
  //check for an correct format
  if(dataAmount == 1 && index == dataAmount+2){
    json = "{\"function\":\""+strArray[0]+"\",\"DATA\":["+strArray[1]+","+strArray[2]+","+strArray[3]+"],\"text\":\""+strArray[4]+"\"}";
  }else if(dataAmount == 3 && index == dataAmount+2){
    json = "{\"function\":\""+strArray[0]+"\",\"DATA\":["+strArray[1]+","+strArray[2]+","+strArray[3]+"],\"text\":\""+strArray[4]+"\"}";
  }else if(dataAmount == 3 && index == dataAmount+1){
    json = "{\"function\":\""+strArray[0]+"\",\"DATA\":["+strArray[1]+","+strArray[2]+","+strArray[3]+"],\"text\":\""+strArray[4]+"\"}";
  }else if(dataAmount == 1 && index == dataAmount+1){
    json = "{\"function\":\""+strArray[0]+"\",\"DATA\":["+strArray[1]+"],\"text\":\""+strArray[4]+"\"}";
  }else if(dataAmount == 0 && index == dataAmount+1){
    json = "{\"function\":\"";
    for(int i = 0; i < strArray[0].length()-2; i++)
      json += strArray[0].charAt(i);
    json += "\",\"DATA\":[""],\"text\":\""+strArray[4]+"\"}";
  }else{
    validFormat = false;
    Serial.println("invalid");
    incomming = "";
    inputGiven = false;
    return;
  }
  int len = json.length() + 1;  
  char ca[len];
  json.toCharArray(ca, len);
  snprintf (msg, MSG_BUFFER_SIZE, ca);
  client.publish("Matrix", msg);
  
  incomming = "";
  inputGiven = false;
}
