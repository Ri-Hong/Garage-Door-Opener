#include <ESP8266WiFi.h>
#include "PubSubClient.h"

//declare all the functions beforehand
void handleRcvMsg( String sMsg );
void setup_wifi();
void publishAck(char *pAckMsg);
void callback(char* topic, byte *payload, unsigned int length);
void reconnect();

int nextHeartbeatTime = 0; //Time in ms indicating when to send out next heartbeat. "heartbeat" is published regularly to show that this device is alive. Used in publishHeartbeat()
unsigned long wakeUpTime = millis(); //any msg received before this time is ignored. Used in handleRcvMsg()

// Update these with values suitable for your network.
const char* ssid = "SAMPLE_WIFI_SSID"; //FILL IN WITH YOUR SSID
const char* password = "SAMPLE_PASSWORD"; //FILL IN WITH YOUR NETWORK PASSWORD
const char* mqtt_server = "broker.hivemq.com"; //You can choose to use another server
#define mqtt_port 1883
#define MQTT_USER "ESP01" //You can choose to change the user if you want
#define MQTT_PASSWORD "" //leave blank if using a public broker
#define MQTT_PUBLISH_TOPIC "foo/bar" // CHOOSE A TOPIC NAME. 
#define MQTT_SUBSCRIBE_TOPIC "foo/bar" //SHOULD BE SAME AS MQTT_PUBLISH_TOPIC

#define GPIO0_PIN 0 //Using a variable instead of a magic number 0

// In this circuit design, when the GPIO0 is set to LOW, the relay will be closed.  
// Therefore, the garage will be opened.
// Also, the on-board LED will be ON when LED_BUILTIN is set to LOW.
#define ON LOW 
#define OFF HIGH

WiFiClient wifiClient;
PubSubClient client(wifiClient);


//------------------------------------------------------
void setup() {
  Serial.println("Beginning");
  Serial.begin(115200);

  //initialize all the pinmodes and set them off
  pinMode(GPIO0_PIN, OUTPUT);
  digitalWrite(GPIO0_PIN, OFF);
  pinMode(LED_BUILTIN, OUTPUT); //Just a note that LED_BUILTIN is linked to GPIO2, so that is why you cannot use GPIO2 to control the relay.
  digitalWrite(LED_BUILTIN, OFF); //Turn LED OFF
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port); //setup mqtt server and port
  client.setCallback(callback); //execture calback function when an incoming message is received
  reconnect();
}

//------------------------------------------------------
void loop() {
  client.loop(); //Used to monitor the server and read incoming messages

  publishHeartbeat();
   
  if(millis() < 10000){ //Used to deal with rollover of the millis() function
    wakeUpTime = millis();
  }
  delay(1000);
}


//------------------------------------------------------
//Function used to publish a message "heartbeat" onto the server every 10 seconds
void publishHeartbeat(){
  int heartbeatInterval = 10000; //the delay between each heartbeat message in milliseconds
  reconnect(); //reconnect to the server if we are somehow disconnected
  //nextHeartbeatTime stores the time at which the next heartbeat should be sent. If the current time is greater then the next heartbeat time, then a heartbeat will be published. It then updates nextHeartbeatTime and also flashes the built in LED on the ESP01
  if(millis() > nextHeartbeatTime){ 
    client.publish(MQTT_PUBLISH_TOPIC, "heartbeat");
    nextHeartbeatTime += heartbeatInterval;
    digitalWrite(LED_BUILTIN, ON); // Turn ON LED
    delay(500);
    digitalWrite(LED_BUILTIN, OFF);  //Turn OFF LED
  }
}


//------------------------------------------------------
//Used to publish an acknowledgement to a request to open the door
void publishAck(char *pAckMsg){ 
  if (!client.connected()) { //Will check if connected to server. If not, it will attempt to reconnect.
    reconnect();
  }
  client.publish(MQTT_PUBLISH_TOPIC, pAckMsg);
}

//------------------------------------------------------
//Used to setup wifi 
void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
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

//------------------------------------------------------
//Used to reconnect to the MQTT server and topic if not already connected
void reconnect() {
  static int reconnectionAttempts = 0;
  char pMsg[50];
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("connected");
      //Once connected, publish an announcement...

      sprintf( pMsg, "Hello from ESP: %d. Just connected", reconnectionAttempts++ );
      client.publish(MQTT_PUBLISH_TOPIC, pMsg);
      // ... and resubscribe
      client.subscribe(MQTT_SUBSCRIBE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//------------------------------------------------------
//Handles the first stage of incoming messages from the server
// when this function is called, the following is passed to this function:
// topic: the subscribed topic
// payload: the content of the received msg
// length: number of bytes in the msg
void callback(char* topic, byte *payload, unsigned int length) {
  char pPayload[50] = "";
  
  Serial.println("-------new message from broker-----");
  Serial.print("channel:");
  Serial.println(topic);
  Serial.print("data:");  
  Serial.write(payload, length);
  Serial.println();

  for( int i=0; i<length; i++ ) //convert the received bytes to chars
  {
    sprintf( pPayload, "%s%c", pPayload, payload[i] ); 
  }

  //Once a request to open the door is received, all other messages will be ignored for the miliseconds specified by msSleepTime found in handleRcvMsg()
  if(millis() >= wakeUpTime){
    handleRcvMsg( (String) pPayload );
  }
  else{
    Serial.println("Sleeping, message ignored");
  }
}

//------------------------------------------------------
//Handles the second stage of incoming messages from the server
void handleRcvMsg( String sMsg ){
  int msSleepTime = 3000; //How long after each request will all incoming requests for the next n miliseconds be ignored. Currently 3 sec. Purpose is to avoid accidential multiple presses of the button in the app
  Serial.println(sMsg);
  if (sMsg == "38"){ //once code is received, GPIO0 will be pulled low, the relay will activate and the door will open. The LED will flash twice and a "1" will be published onto the server
    Serial.println("Pin high");
    digitalWrite(GPIO0_PIN, ON);
    digitalWrite(LED_BUILTIN, ON);   // Turn ON LED
    delay(500);
    digitalWrite(LED_BUILTIN, OFF);   // Turn OFF LED
    delay(500);
    digitalWrite(LED_BUILTIN, ON);   // Turn ON LED
    delay(500);
    Serial.println("Pin low");
    digitalWrite(GPIO0_PIN, OFF);
    digitalWrite(LED_BUILTIN, OFF); //Turn OFF LED
    publishAck("1");

    wakeUpTime = millis() + msSleepTime; //update the time until which all further requests will be ignored
    }
  else if(sMsg == "1" or sMsg == "0" or sMsg == "heartbeat"){//self published message. No need to publish acknowledgement to server
  }
  else{ //for unrecognizable requests
    Serial.print("Unrecognized code: ");
    Serial.println(sMsg);
    publishAck("0");
  }

}

 
