#include <ESP8266WiFi.h>      //ESP8266WiFi.h .- ESP8266 Core WiFi Library
#include <ESP8266WebServer.h> //ESP8266WebServer.h .- Servidor web local utilizado para servir el portal de configuración
#include <DNSServer.h>        //DNSServer.h .- Local WebServer usado para servir el portal de configuración (https://github.com/zhouhan0126/DNSServer---esp32)
#include <WiFiManager.h>      //WiFiManager.h .- WiFi Configuration Magic (https://github.com/zhouhan0126/DNSServer---esp32) >> https://github.com/zhouhan0126/DNSServer---esp32 (ORIGINAL)
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>      // DNS
#include <Adafruit_AMG88xx.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <WebSocketsServer.h>

//define serial port diferente
#define USE_SERIAL Serial


//low range of the sensor (this will be blue on the screen)
#define MINTEMP 22

//high range of the sensor (this will be red on the screen)
#define MAXTEMP 34

float pixels[AMG88xx_PIXEL_ARRAY_SIZE]; //ARRAY DE AMG8833
const int PIN_AP = 2;                 // pulsador para volver al modo AP
const char *AP_SSID = "ESP_AP";       // nombre del access point para configurar la conexión WiFi
const char *AP_PASSWORD = "12345678"; // password del access point

//json declare
String Json;

//declaración de objeto wifiManager
WiFiManager wifiManager;

Adafruit_AMG88xx amg;



ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdateServer;

static const char PROGMEM INDEX_HTML[] = R"( 
<!DOCTYPE html>
<html>
  <head>
      <meta charset=utf-8>
      <title>WebSocket ESP8266 - REGULACIÓN DE INTENSIDAD DE UN LED</title>
  </head>
  <body>
    <h1>Vista TermalCam</h1> 
    <p>Comunicación vía WebSocket: Servidor (ESP8266) <---> Cliente</p>

    
    <input type='range' min='0' max='255' value='127' id='miValor' oninput='verValor()'>
    <p id='valor'></p>
    <script>
       var x; 
       var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
       connection.onopen = function () {
         connection.send('Conectado  -  ' + new Date()); 
         verValor();
       }
       connection.onmessage = function (event) {
         verValor(event.data);
       }
       connection.onerror = function (error) {
         console.log('WebSocket Error!!!', error);
       }
       function verValor(valor) {
         x = document.getElementById('miValor').value;
         document.getElementById('valor').innerHTML = valor;
         enviarValor();
       }
       function enviarValor(){
         console.log('Cliente (envía): ' + x);
         connection.send(x);
       }
    </script>
  </body>
</html>
)";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
      webSocket.sendTXT(num, "Connected");
      }
      break;
        
  }

}
void configModeCallback(WiFiManager *myWiFiManager)
{
  USE_SERIAL.println("Modo de configuración ingresado");
  USE_SERIAL.println(WiFi.softAPIP());

  USE_SERIAL.println(myWiFiManager->getConfigPortalSSID());
}

//flag for saving data
bool shouldSaveConfig = false;

// En https://github.com/tzapu/WiFiManager
//callback notifying us of the need to save config

void saveConfigCallback()
{
  USE_SERIAL.println("Debería guardar la configuración");
  shouldSaveConfig = true;
}

void setup()
{
  USE_SERIAL.begin(115200);
  amg.begin();
  pinMode(PIN_AP, INPUT);

  //modificamos el portal
  wifiManager.setCustomHeadElement("<style>html { color: #c5c6c7; background-color: #1f2833; } a:visited, a { color: #66fcf1; transition: 200ms; } a:hover, button:hover { color: #ff256c; } button { background-color: transparent; transition: 200ms; border: 5px solid #66fcf1; color: #c5c6c7; height: 15vh; }</style>");

  // utilizando ese comando, como las configuraciones se apagarán en la memoria
  // en caso de que la redacción se conecte automáticamente, ella é apagada.
  // wifiManager.resetSettings();

  //devolución de llamada para cuando entra en el modo de configuración AP
  wifiManager.setAPCallback(configModeCallback);
  //devolución de llamada cuando se conecta a una red, es decir, cuando pasa a trabajar en modo EST
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //crea una red de nombre ESP_AP con pass 12345678
  wifiManager.autoConnect(AP_SSID, AP_PASSWORD);

  
  httpUpdateServer.setup(&webServer);

  // llamada a funcion readCam
  //readCam;
  //webServer.on("/", readCam);

  webServer.on("/all", HTTP_GET, [](){
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    webServer.send(200, "text/plain", "chico te paseo");
  });

  webServer.on("/", []() {
      // send index.html
      webServer.send(200, "text/html", INDEX_HTML);
  });


  //start webServer
  webServer.begin();
  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // start MDNS server
  if(MDNS.begin("esp8266")) {
    USE_SERIAL.println("MDNS responder started");
  }

  
  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);
}

unsigned long last_10sec = 0;
unsigned int counter = 0;

void loop()
{
  /* //si el botón se ha presionado
  if (digitalRead(PIN_AP) == HIGH)
  {
    Serial.println("reajustar"); //resetear intenta abrir el portal
    if (!wifiManager.startConfigPortal(AP_SSID, AP_PASSWORD))
    {
      Serial.println("No se pudo conectar");
      delay(2000);
      ESP.restart();
      delay(1000);
    }
    Serial.println("conectado ESP_AP!!!");
  } */
  unsigned long t = millis();
  webServer.handleClient();
  static bool hasConnected = false;
  if (WiFi.status() != WL_CONNECTED) {
  //      Serial.printf("Connecting to %s\n", ssid);
    hasConnected = false;
  }
  else if (!hasConnected) {
    hasConnected = true;
    USE_SERIAL.print("Connected! Open http://");
    USE_SERIAL.print(WiFi.localIP());
    USE_SERIAL.println(": in your browser");
  }

  webSocket.loop();
  if((t - last_10sec) > 10 * 1000) {
    counter++;
    bool ping = (counter % 2);
    int i = webSocket.connectedClients(ping);
    USE_SERIAL.printf("%d Connected websocket clients ping: %d\n", i, ping);
    last_10sec = millis();
  }

  String temp;
  amg.readPixels(pixels);
  StaticJsonDocument<200> doc;
  JsonArray data = doc.createNestedArray("data");
  /* for(int i=1; i<=AMG88xx_PIXEL_ARRAY_SIZE; i++){
    

    //JSON.concat(String(temp));
    data.add(pixels[i-1]);
    

  } */
  temp.concat("[");
    for(int i=1; i<=AMG88xx_PIXEL_ARRAY_SIZE; i++){
      temp.concat(pixels[i-1]);
      if( i<AMG88xx_PIXEL_ARRAY_SIZE )temp.concat(",");
      if( i%8 == 0 ) Serial.println();
  }
  temp.concat("]");
  //serializeJson(doc, Json);
  //USE_SERIAL.println(Json);
  USE_SERIAL.println(temp);
  delay(1000);
  webSocket.broadcastTXT(temp);
}

/* void readCam(const char * payload, size_t length) {
  float temp;
  amg.readPixels(pixels);
  USE_SERIAL.println("chucoco");
  StaticJsonDocument<200> doc;
  JsonArray data = doc.createNestedArray("data");
  for(int i=0; i<AMG88xx_PIXEL_ARRAY_SIZE; i++){
    

    //JSON.concat(String(temp));
    data.add(pixels[i]);
   

  }
  serializeJson(doc, Json);
  for(int i=0; i<AMG88xx_PIXEL_ARRAY_SIZE; i++){
      JSON.concat(String(pixels[i]));
      if(i==AMG88xx_PIXEL_ARRAY_SIZE-1)
      JSON.concat(String("}"));
  } 
  USE_SERIAL.println(Json);
  webSocket.sendTXT(Json.c_str(), Json.length() + 1);
}
 */