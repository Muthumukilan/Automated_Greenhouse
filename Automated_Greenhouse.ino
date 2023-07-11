#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <string.h>
//Constants
#define DHTPIN D3     // what pin we're connected to
#define DHTTYPE DHT22   // DHT22
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor for normal 16mhz Arduino
//Variables
float humidity;  //Stores humidity value
float temperature; //Stores temperature value
int lightIntensity;
int soilMoisture;

const char *ssid = "muthu"; // Enter your WiFi name
const char *password = "muthu.123";  // Enter WiFi password// MQTT Broker
const char *mqtt_broker = "13.232.239.3"; // IP address of MQTT broker server

/*
  Constants used in MQTT communication and internet connnection
*/
const char *subscribeTopic = "greenhouse/control/#";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);


/*
 CHANGE LATER
*/
int automate = 1;


/*
  Function to push a char to end of a string
*/
void push(char* source,char ch){
  char cstring[2]; cstring[1] = '\0';
  cstring[0] = ch;
  strcat(source, cstring);
}
/*
  Function to get String input from Serial input. parameter is where the input is stored.
*/
void getInput(char* input){
  strcpy(input,"");
  while(Serial.available()==0){}
  delay(100);
  while(Serial.available() > 0){
    push(input, (char)Serial.read());
  }
  Serial.println(input);
}

/*
  Converts a given string into numeric
*/
int convertStringInteger(char* number, unsigned int length){
  int result = 0;
  for(int i = length ; i >=1;i--){
    int x = number[i-1] - '0';
    result+=x*pow(10,length-i);
  }
  return result;
}

/*
  Reverses the array
*/
char* reverse(char* arr){
    int n = strlen(arr)-1;
    int end = n;
    for (int c = 0; c <= n/2; c++) {
      char t=arr[c];
      arr[c]=arr[end];
      arr[end]=t;

      end--;
    }
    return arr;
}
/*
  Get the nth last subtopic of a given topic string
*/
char* getNthLastSubtopic(char* topic, int n){
  int size = strlen(topic);
  reverse(topic);
  char str[100];
  strcpy(str,topic);
  const char delimiter[2] = "/";
  char* token = strtok(str,delimiter);
  while(token != NULL){
      if(!--n){
      reverse(topic);
      return reverse(token);
    } 
    token = strtok(NULL,delimiter);
  }
  reverse(topic);
  return NULL;
}

/*
  Effective way to consolidate output controls
*/
struct CHANNEL{
  const char* name;
  byte pin;
};

/*
  Edit the pin numbers here
*/
CHANNEL channels[] ={
  {"fan", D7},
  {"led", D6},
  {"pump", D5}
};

struct CONSTRAINT{
  const char* name;
  int threshold;
  byte pin;
};

CONSTRAINT constraints[]= {
  {"temperature", 25, D7},
  {"humidity", 60, D7},
  {"soilmoisture", 1000, D5},
  {"light", 300, D6}
};

struct SENSORS{
  const char* name;
  float value;
};
SENSORS sensors[]={
  {"temperature",0},
  {"humidity",0},
  {"soilmoisture",0},
  {"light",0}
};

/*
  Function to send output to specified channel
*/
void send_output(char *channel, char unprocessed_signal){
  int numchannels = sizeof(channels)/sizeof(channels[0]);
  int signal = unprocessed_signal-'0';
  if(strcmp(channel, "automate") == 0){
    automate = signal;
    Serial.println("AUTOMATION IS toggled..");
    return;
  }
  for(int i = 0; i < numchannels; i++){    
    if(strcmp(channel, channels[i].name) == 0){
      digitalWrite(channels[i].pin, signal);
      Serial.print(channels[i].name);
      Serial.print(" is ");
      Serial.println(signal);
      break;
    }
  }
}


/*
  Called when a new message is published
*/
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message Length: ");
  Serial.println(length);
  if(!strcmp(getNthLastSubtopic(topic, 2), "control")){
    // If the payload is not single character, discard it.
    if(length>1){
      Serial.println("Message Discarded due to incorrect length.");
      Serial.println(" - - - - - - - - - - - -");
      return;
    }
    // Send signals to actuators
    send_output(getNthLastSubtopic(topic, 1), payload[0]);
  }
  else if(!strcmp(getNthLastSubtopic(topic, 2), "constraints")){
    // Need to convert the message to integer
    int threshold = convertStringInteger((char*)payload, length);
    char* name = getNthLastSubtopic(topic, 1);
    for(int i = 0; i < sizeof(constraints)/sizeof(constraints[0]); i++){
      if(!strcmp(name, constraints[i].name)){
        int temp = constraints[i].threshold;
        constraints[i].threshold = threshold;
        Serial.print("CHANGED constraint ");
        Serial.print(constraints[i].name);
        Serial.print(" from ");
        Serial.print(temp);
        Serial.print(" to ");
        Serial.println(threshold);
        break;
      }
    }
  }  
  Serial.println();
  Serial.println(" - - - - - - - - - - - -");
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
      client.publish("Announcements", "CONNECTED");
      // ... and resubscribe
      client.subscribe(subscribeTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}




/*
 Flags to check if net and mqtt are connected
*/
bool netconfig = false;
bool mqttconfig = false;

void connectingWiFi(){
  //char ssid[30], password[30];  
  while(!netconfig){
    Serial.print("Enter Network SSID: ");
    //getInput(ssid);
    Serial.print("Enter Network PASSWORD: ");
    //getInput(password);
    /*
      Connecting to WiFi
    */
    WiFi.begin(ssid, password);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);tries++;
      Serial.println("Connecting to WiFi..");
      if(++tries > 10){
        Serial.println("Unable to connect to network. Try Again.");
        break;
      }
    }
    if(tries > 100) continue;
    Serial.println("Connected to the WiFi network");
    netconfig = true;
  }
}

void connectingMQTTBroker(){
  //char mqtt_broker[20];
  if(!mqttconfig){
    Serial.print("Enter MQTT Broker IP Address: ");
    //getInput(mqtt_broker);
    int tries=0;
    client.setServer(mqtt_broker, mqtt_port);
    while (!client.connected() && tries < 50) {
      reconnect();tries++;
    }
    if(client.connected()) mqttconfig = true;
  }
}

void setup()
{
  /*
    Set the callback for the published messages    
  */
  client.setCallback(callback);
  // Initialize serial port
  Serial.begin(115200);
  //Initialize the DHT sensor
  dht.begin();
  pinMode(D1,OUTPUT);
  pinMode(D2,OUTPUT);
  pinMode(D5,OUTPUT);
  pinMode(D6,OUTPUT);
  pinMode(D7,OUTPUT);
  Serial.println("DHT22 sensor testing");

  connectingWiFi();
  connectingMQTTBroker();
  
}



void loop()
{
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Disconnected from Network. Connecting again...Please enter details.");
    netconfig = false;
    connectingWiFi();
  }
  if (!client.connected()) {
    Serial.println("Disconnected from MQTT Broker. Try Again...Please enter details.");
    mqttconfig = false;
    connectingMQTTBroker();
  }
  client.loop();

  float converted = 0.00;
  //Read data and store it to variables hum and temp
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  sensors[0].value = temperature;
  sensors[1].value = humidity;
  Serial.print("Celsius = ");
  Serial.print(temperature);
  
  String ts = String(temperature);
  char* tema;

  int tsl = ts.length() + 1;
  char taa[tsl];
  ts.toCharArray(taa, tsl);
  tema = taa;

  client.publish("greenhouse/sensor/temperature", tema);
  Serial.println("C");

  
  Serial.print("Humidity =");
  Serial.println(humidity);
  String hs=String(humidity);
  char* hua;

  int hsl = hs.length() + 1;
  char haa[hsl];
  hs.toCharArray(haa, hsl);
  hua = haa;

  client.publish("greenhouse/sensor/humidity",hua);

  digitalWrite(D1, HIGH);
  lightIntensity = analogRead(A0);
  sensors[3].value = lightIntensity;
  delay(200);
  digitalWrite(D1, LOW);
  
  Serial.print("Light Intensity =");
  
  Serial.println(lightIntensity);
  String lm=String(lightIntensity);
  //char* lia;

  char* lma;
  //String nameS = sm;
  int lmal = lm.length() + 1;
  char lmaa[lmal];
  lm.toCharArray(lmaa, lmal);
  lma = lmaa;
  
  
  //lia=changeTOCharArr(lightIntensityString);
  client.publish("greenhouse/sensor/light", lma);
  digitalWrite(D2, HIGH);
  soilMoisture = analogRead(A0);
  sensors[2].value = soilMoisture;
  delay(200);
  Serial.print("Soil Moisture =");
  Serial.println(soilMoisture);
  String sm=String(soilMoisture);
  /*int sl = sm.length() + 1;
  char char_array[sl];
*/
  
  char* sma;
  //String nameS = sm;
  int smal = sm.length() + 1;
  char smaa[smal];
  sm.toCharArray(smaa, smal);
  sma = smaa;
  
  //char* soilMoistureString; sprintf(soilMoistureString, "%d", soilMoisture);
  client.publish("greenhouse/sensor/soilmoisture", sma);
  digitalWrite(D2, LOW);

  if(automate){
    for(int i = 0; i < 4; i++){
      if(i == 0 || i == 1){
        if(sensors[i].value < constraints[i].threshold){
          digitalWrite(constraints[i].pin, LOW);
        }
        else{
          digitalWrite(constraints[i].pin, HIGH);
        }
      }
      if(sensors[i].value < constraints[i].threshold){
        digitalWrite(constraints[i].pin, HIGH);
      }
      else{
        digitalWrite(constraints[i].pin, LOW);
      }
    }
  }

  
  // if(automate){
  //   if (soilMoisture < 1000) {
  //     digitalWrite(D5, HIGH);
      
  //     Serial.println("Water Pump is ON ");
  //   } else {
  //     digitalWrite(D5, LOW);
      
  //     Serial.println("Water Pump is OFF ");
  //   }
  //   if(lightIntensity<300){
  //     digitalWrite(D6, HIGH);
  //     Serial.println("LED is ON ");  
  //   }
  //   else{  
  //     digitalWrite(D6, LOW);
  //     Serial.println("LED is OFF ");  
  //   }
  //   if((temperature>25 && humidity>60)){
  //     digitalWrite(D7, HIGH);
  //     Serial.println("FAN is ON ");  
  //   }
  //   else{
  //     digitalWrite(D7, LOW);
  //     Serial.println("FAN is OFF ");  
  //   }
  // }


  
    

  
  //2000mS delay between reads
  delay(2000);
}
