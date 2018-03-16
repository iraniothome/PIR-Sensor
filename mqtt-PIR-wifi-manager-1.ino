#include <FS.h>                   
#include <ESP8266WiFi.h>          
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         
#include <PubSubClient.h>        
#include <ArduinoJson.h>         
#include <WiFiUDP.h>
#include <ArduinoOTA.h>
#include <MQTT.h>
#include <EEPROM.h>


#define DEBUG


#define PIRPIN D2     // what pin we're connected to



// MQTT settings

WiFiClient espClient;

PubSubClient client(espClient);

long lastMsg = 0;

char msg[128];

long interval = 10000;     // interval at which to send mqtt messages (milliseconds)



//define your default values here, if there are different values in config.json, they are overwritten.

char mqtt_server[40] = "84.241.11.190";

char mqtt_port[6] = "1883";

char username[34] = "";

char password[34] = "";

const char* willTopic = "home/PIRsensor";

String node=String(ESP.getChipId());
const char* clientid=node.c_str();

const char* apName = clientid;
const char* apPass = clientid;



//flag for saving data

bool shouldSaveConfig = false;



//callback notifying us of the need to save config

void saveConfigCallback () {

  Serial.println("Should save config");

  shouldSaveConfig = true;

}






void setup() {

  ArduinoOTA.begin();

 pinMode(PIRPIN,INPUT);
 digitalWrite(PIRPIN,LOW);

  Serial.begin(115200);

  Serial.println();

  
  //clean FS, for testing

  //SPIFFS.format();



  //read configuration from FS json

  Serial.println("mounting FS...");



  if (SPIFFS.begin()) {

    Serial.println("mounted file system");

    if (SPIFFS.exists("/config.json")) {

      //file exists, reading and loading

      Serial.println("reading config file");

      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile) {

        Serial.println("opened config file");

        size_t size = configFile.size();

        // Allocate a buffer to store contents of the file.

        std::unique_ptr<char[]> buf(new char[size]);



        configFile.readBytes(buf.get(), size);

        DynamicJsonBuffer jsonBuffer;

        JsonObject& json = jsonBuffer.parseObject(buf.get());

        json.printTo(Serial);

        if (json.success()) {

          Serial.println("\nparsed json");



          strcpy(mqtt_server, json["mqtt_server"]);

          strcpy(mqtt_port, json["mqtt_port"]);

          strcpy(username, json["username"]);

          strcpy(password, json["password"]);



        } else {

          Serial.println("failed to load json config");

        }

      }

    }

  } else {

    Serial.println("failed to mount FS");

  }

  //end read



  // The extra parameters to be configured (can be either global or just in the setup)

  // After connecting, parameter.getValue() will get you the configured value

  // id/name placeholder/prompt default length

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);

  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);

  WiFiManagerParameter custom_username("username", "username", username, 32);

  WiFiManagerParameter custom_password("password", "password", password, 32);



  //WiFiManager

  //Local intialization. Once its business is done, there is no need to keep it around

  WiFiManager wifiManager;



  //set config save notify callback

  wifiManager.setSaveConfigCallback(saveConfigCallback);



  //set static ip

  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  

  //add all your parameters here

  wifiManager.addParameter(&custom_mqtt_server);

  wifiManager.addParameter(&custom_mqtt_port);

  wifiManager.addParameter(&custom_username);

  wifiManager.addParameter(&custom_password);



  //reset settings

  //wifiManager.resetSettings();

  

  //set minimu quality of signal so it ignores AP's under that quality

  //defaults to 8%

  wifiManager.setMinimumSignalQuality();

  

  //sets timeout until configuration portal gets turned off

  //useful to make it all retry or go to sleep

  //in seconds

  //wifiManager.setTimeout(120);



  //fetches ssid and pass and tries to connect

  //if it does not connect it starts an access point with the specified name

  //here  "AutoConnectAP"

  //and goes into a blocking loop awaiting configuration

  if (!wifiManager.autoConnect(apName,apPass)) {

    Serial.println("failed to connect and hit timeout");

    delay(3000);

    //reset and try again, or maybe put it to deep sleep

    ESP.reset();

    delay(5000);

  }



  //if you get here you have connected to the WiFi

  Serial.println("connected...");



  //read updated parameters

  strcpy(mqtt_server, custom_mqtt_server.getValue());

  strcpy(mqtt_port, custom_mqtt_port.getValue());

  strcpy(username, custom_username.getValue());

  strcpy(password, custom_password.getValue());



  //save the custom parameters to FS

  if (shouldSaveConfig) {

    Serial.println("saving config");

    DynamicJsonBuffer jsonBuffer;

    JsonObject& json = jsonBuffer.createObject();

    json["mqtt_server"] = mqtt_server;

    json["mqtt_port"] = mqtt_port;

    json["username"] = username;

    json["password"] = password;



    File configFile = SPIFFS.open("/config.json", "w");

    if (!configFile) {

      Serial.println("failed to open config file for writing");

    }



    json.printTo(Serial);

    json.printTo(configFile);

    configFile.close();

    //end save

  }



  Serial.println("local ip");

  Serial.println(WiFi.localIP());

  Serial.println(clientid);
  

  // mqtt

  client.setServer(mqtt_server, atoi(mqtt_port)); // parseInt to the port
 // client.setCallback(callback);



}





void reconnect() {

  // Loop until we're reconnected to the MQTT server

  while (!client.connected()) {

    Serial.print("Attempting MQTT connection...");

    // Attempt to connect

    if (client.connect(username, username, password, willTopic, 0, 1, (String("I'm dead: ")+username).c_str())) {     // username as client ID

      Serial.println("connected");

      // Once connected, publish an announcement... (if not authorized to publish the connection is closed)

      client.publish("all", (String("home/PIRsensor")+clientid).c_str());

      // ... and resubscribe

        client.subscribe((String("home/PIRsensor")+clientid).c_str());
        

      
     



      delay(5000);

      

    } else {

      Serial.print("failed, rc=");

      Serial.print(client.state());

      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying

      delay(5000);

    }

  }

}



void loop() {


    if(digitalRead(PIRPIN)==HIGH)  
    {
      Serial.println("Movement detected.");
      client.publish((String("home/PIRsensor")+clientid).c_str(), "Movement detected."); // retained message
    }
    else  
    {
      Serial.println("Nothing.");
      client.publish((String("home/PIRsensor")+clientid).c_str(), "Nothing detected."); // retained message

        }
    delay(1000);



  if (!client.connected()) { // MQTT

    reconnect();

  }

  client.loop();



  unsigned long now = millis();

 

  if(now - lastMsg > interval) {

    
    lastMsg = now;


  }

 
 ArduinoOTA.handle(); 

}


