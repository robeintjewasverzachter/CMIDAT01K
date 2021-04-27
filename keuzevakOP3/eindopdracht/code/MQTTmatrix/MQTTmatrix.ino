#include <ESP8266WiFi.h>
#include <PubSubClient.h>
// Update these with values suitable for your network.
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";

WiFiClient espClient;
PubSubClient client(espClient);

#include <PxMatrix.h>
#include <Ticker.h>
Ticker display_ticker;
#define P_LAT 16          //D0
#define P_A 5             //D1
#define P_B 4             //D2
#define P_C 15            //D8
#define P_D 12            //D6
#define P_OE 2            //D4
                          //R1 naar D7
                          //clk naar D5
#define matrix_width 32
#define matrix_height 16
#define NUM_LEDS 32

uint8_t display_draw_time=20;
//call the matrix constructor
PxMATRIX display(32,16,P_LAT, P_OE,P_A,P_B,P_C,P_D);
uint16_t currentColor;

unsigned long lastMsg = 0, lastTempMeasurement = 0;
#define MSG_BUFFER_SIZE  (255)
char msg[MSG_BUFFER_SIZE];

#include <ArduinoJson.h>
StaticJsonDocument<256> doc;

#include "DHT.h"
#define DHTPIN 0 //D3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void display_updater()
{
  display.display(display_draw_time);
}

void display_update_enable(bool is_enable)
{
  if (is_enable)
    display_ticker.attach(0.002, display_updater);
  else
    display_ticker.detach();
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

bool messageRecieved = false;
void callback(char* topic, byte* payload, unsigned int length) {
  //read the json that arrived in the subscribed topic
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  deserializeJson(doc, (const byte*)payload, length);
  messageRecieved = true;
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
      client.subscribe("Matrix");
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
  //initialize the matrix display
  display.begin(4);
  display.setMuxPattern(STRAIGHT);
  display.setScanPattern(ZAGGIZ);
  display.setMuxPattern(STRAIGHT);
  display.setFastUpdate(true);
  display.clearDisplay();
  display.setTextColor(display.color565(0, 255, 255));
  display.setCursor(2,0);
  display.print("WIFI");
  display.setCursor(2,8);
  display.print("INNIT");
  display_update_enable(true);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  display.clearDisplay();
}

void sendErrorMessage(String errorMessage){
  //sends an error message back to the error topic of the MQTT server
  int str_len = errorMessage.length() + 1;  
  char ca[str_len];
  errorMessage.toCharArray(ca, str_len);
  snprintf (msg, MSG_BUFFER_SIZE, ca);
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("error", msg);
  messageRecieved = false;
}

void rgbError(){
  sendErrorMessage("RGB value invalid, should be: function r g b\n where R,G,B [0..255]\nExample: fillMatrix 0 125 255");
}

void rainbowCycle() {
    byte *c;
    uint16_t i, j;
    for(j=0; j<256; j++) {
    for(i=0; i< NUM_LEDS; i++) {
      c=Wheel(((i * 256 / NUM_LEDS) + j) & 255);
      for(int k = 0; k < 17; k++){
        display.drawPixel(i, k, display.color565(*c, *(c+1), *(c+2)));
      }
    }
    delay(10);
  }
}

byte * Wheel(byte WheelPos) {
  //define the location in the rgb color wheel
  static byte c[3];
  if(WheelPos < 85) {
   c[0]=WheelPos * 3;
   c[1]=255 - WheelPos * 3;
   c[2]=0;
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   c[0]=255 - WheelPos * 3;
   c[1]=0;
   c[2]=WheelPos * 3;
  } else {
   WheelPos -= 170;
   c[0]=0;
   c[1]=WheelPos * 3;
   c[2]=255 - WheelPos * 3;
  }

  return c;
}


void setBrightness(int val[], String){
  //check for correct user input
  if(!(val[0] != -1 && val[1] == -1 && val[2] == -1)){
    sendErrorMessage("This function only has one parameter");
    return;
  }
  int brightnessValue = val[0];
  if(brightnessValue < 0 || brightnessValue > 255){
    sendErrorMessage("Invalid parameter value, should be [0..255]");
    messageRecieved = false;
    return;
  }
  
  display.setBrightness(brightnessValue);
  messageRecieved = false;
}

void displayTemp(int val[], String){
  //check for correct user input
  if(val[0] == -1 || val[1] == -1 || val[2] == -1){
    rgbError();
    return;  
  }
  uint16_t color = createColor(val[0],val[1],val[2]);;
  unsigned long now = millis();
  //only check for the temperature every 2 seconds
  if (now - lastTempMeasurement > 2000) {
    lastTempMeasurement = now;
    long temp = dht.readTemperature();
    if (isnan(temp)) {
      return;
    }
    String t = String(temp);
    //set display settings
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(color);
    display.setTextSize(1);
    display.setCursor(1,1);
    display.println(t+"C");
  }
}

void rainbow(int [], String){
  rainbowCycle();
}

void displayText(int val[], String text){
  //check for correct user input
  if(val[0] == -1 || val[1] == -1 || val[2] == -1){
    rgbError();
    return;  
  }
  uint16_t color = createColor(val[0],val[1],val[2]);
  //define the length of the text in pixels, one character averages 12 pixels in width
  int textLength = (text.length()+2)*12;
  //set display settings
  display.setTextWrap(false);
  display.setTextColor(color);
  display.setTextSize(2);
  for(int i = 31; i >= 0-textLength; i--){
    display.clearDisplay();
    display.setCursor(i,1);
    display.println(text);
    delay(50);
  }
  messageRecieved = false;
}

void fillMatrix(int val[], String){
  //check for correct user input
  if(val[0] == -1 || val[1] == -1 || val[2] == -1){
    rgbError();
    return;  
  }
  uint16_t color = createColor(val[0],val[1],val[2]);
  //fill the matrix with color color
  display.fillRect(0, 0, 32, 16, color); 
  messageRecieved = false;
}

//struct for connecting function and function name
typedef struct{
  String names;
  void (*function)(int[], String);
}commandType;

// array of command structures
static commandType command[] = {
  {"displayText", &displayText},
  {"fillMatrix", &fillMatrix},
  {"rainbow", &rainbow},
  {"displayTemp", &displayTemp},
  {"setBrightness", &setBrightness}
};
static int commandListSize = sizeof(command)/ sizeof(commandType);

uint16_t createColor(int r, int g, int b){
  return display.color565(r,g,b); 
}

void loop() {
  //check if the connection is still live
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  unsigned long now = millis();
  //let the user know that the connection is still live every 2 seconds
  if (now - lastMsg > 2000) {
    lastMsg = now;
    snprintf (msg, MSG_BUFFER_SIZE, "StillAllive");
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("aliveCheck", msg);
  }

  //check if an command has been recieved
  if(!messageRecieved){
    return;
  }
  //obtain the function that has been called
  const String calledFunction = doc["function"];
  int functionIndex = -1;
  bool invalidFunction = true;
  //check if the called function exists in this program
  for(int i = 0; i < commandListSize; i++){
    const String tempString = String(command[i].names);
    if(tempString.equals(calledFunction)){
      functionIndex = i;
    }
  }
  //if the called function doesn't exist, send a list of valid comands to the user
  if(functionIndex == -1){ 
    String errorString = "Function not found, try: [";
    for(int i = 0; i < commandListSize; i++){
      errorString += String(command[i].names)+ " ";
    }
    errorString += "]";
    sendErrorMessage(errorString);
    messageRecieved = false;
    return;
  }
  if(doc["DATA"].size() > 3){
    sendErrorMessage("To many parameters");
    messageRecieved = false;
    return;
  }
  int val[3] ={-1,-1,-1};
  for(int i = 0; i < doc["DATA"].size(); i++){
    val[i] = (int)doc["DATA"][i];
  }
  //call the requested function
  void (*function)(int[], String) = command[functionIndex].function;
  const String text = doc["text"];
  function(val,text);
}
