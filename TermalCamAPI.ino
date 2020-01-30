#include <ESP8266WiFi.h>      //ESP8266WiFi.h .- ESP8266 Core WiFi Library
#include <ESP8266WebServer.h> //ESP8266WebServer.h .- Servidor web local utilizado para servir el portal de configuración
#include <DNSServer.h>        //DNSServer.h .- Local WebServer usado para servir el portal de configuración (https://github.com/zhouhan0126/DNSServer---esp32)
#include <WiFiManager.h>      //WiFiManager.h .- WiFi Configuration Magic (https://github.com/zhouhan0126/DNSServer---esp32) >> https://github.com/zhouhan0126/DNSServer---esp32 (ORIGINAL)
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>      // DNS
#include <Adafruit_AMG88xx.h>
#include <Wire.h>
#include <WebSocketsServer.h>

//define serial port diferente
#define USE_SERIAL Serial


//low range of the sensor (this will be blue on the screen)
#define MINTEMP 22

//high range of the sensor (this will be red on the screen)
#define MAXTEMP 34

float pixels[AMG88xx_PIXEL_ARRAY_SIZE]; //array of AMG8833
const int PIN_AP = 2;                 // pulsator for start AP mode
const char *AP_SSID = "ESP_AP";       // name of AP to conect
const char *AP_PASSWORD = "12345678"; // password of AP to conect

//declaration of object wifiManager
WiFiManager wifiManager;
//declaration of object Adafruit_AMG88xx
Adafruit_AMG88xx amg;



ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdateServer;

//html to show on the client 
static const char PROGMEM Index_HTML_8x8[] = R"( 
<!DOCTYPE html>
<html>
  <head>
      <meta charset=utf-8>
      <title>WebSocket ESP8266 - TermalCam</title>
      <!-- Compiled and minified CSS -->
      <link rel="stylesheet" href='https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/css/materialize.min.css'>
    </head>
  </head>
  <header class='top container'  >
      <div id='' class="row"  >
        <div class="col s12 m2"></div>
        <div class="col s12 m8">
          <h1>Vista TermalCam</h1> 
          <p>Comunicación vía WebSocket: Servidor (ESP8266) <---> Cliente</p>
        </div>
        
      </div>
    
  </header>
  <body class='brown darken-1 '>
    <div id='' class="container "  >
      <div id='' class="row"  >
        <div class="col s12 m2"></div>
        <div class="card col s12 m8">
          <div class="row valign-wrapper">
            <div class="col s2 ">
            </div>
            <div class="col s8 ">
              <canvas id="canvas" class="center" width="300" height="300" ></canvas>
            </div>
            <div class="col s2 ">
            </div>
          </div>
          <div class="card-content">
          <span class="card-title activator grey-text text-darken-4">Online<i class="fa fa-eye-slash right" ></i></span>
          </div>
          <div class="card-reveal">
            <span class="card-title  grey-text text-darken-4">Offline<i class="fa fa-eye right" ></i></span>
            <p>Termal cam is offline.</p>
          </div>
        </div>
        <div class="col s12 m1"></div>
      </div>
    </div>
    <script>
      
      let arrayDeCadenas, coma = ','; 
      var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
        connection.onopen = function () {
        connection.send('Conectado  -  ' + new Date()); 
      }
      connection.onmessage = function (event) {
        
        dibujarCanvas(event.data,coma)
        
      }
      let dibujarCanvas = (cadenaADividir,separador) => {
        arrayDeCadenas = cadenaADividir.split(separador); 
        arrayDeCadenas.map(Number);
        
        let canvas=document.getElementById("canvas");
        let ctx=canvas.getContext('2d');
        let width = canvas.width;
        let height = canvas.height;

        for(let x=0;x<8;x++) {
          for(let y=0;y<8;y++) {
            let temp = arrayDeCadenas[x*8+y];
            let h = -29+(temp-80)*25
            ctx.fillStyle='hsl('+h+', 100%, 50%)';
            ctx.fillRect(width/8*x,height/8*y,width/8,height/8);
          }
        }
      }
       connection.onerror = function (error) {
         console.log('WebSocket Error!!!', error);
       }      

    </script>
    
    <!-- Compiled and minified JavaScript -->
    <script src='https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/js/materialize.min.js'></script>
    <!-- font awesome -->
    <script src='https://kit.fontawesome.com/8ee530645c.js' crossorigin='anonymous'></script>        
  </body>
</html>
)";

// web socket event declaration
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

//callback notifying us of the need to save config

void saveConfigCallback(){
  USE_SERIAL.println("Debería guardar la configuración");
  shouldSaveConfig = true;
}

void setup(){

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

  webServer.on("/all", HTTP_GET, [](){
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    webServer.send(200, "text/plain", "chico te paseo");
  });

  webServer.on("/", []() {
      webServer.send(200, "text/html", Index_HTML_8x8);
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
  unsigned long t = millis();
  webServer.handleClient();
  static bool hasConnected = false;
  if (WiFi.status() != WL_CONNECTED) {
    hasConnected = false;
  }
  else if (!hasConnected) {
    hasConnected = true;
    USE_SERIAL.print("Connected! Open http://");
    USE_SERIAL.print(WiFi.localIP());
    USE_SERIAL.println(": in your browser");
  }
  //start web socket loop
  webSocket.loop();
  if((t - last_10sec) > 10 * 1000) {
    counter++;
    bool ping = (counter % 2);
    int i = webSocket.connectedClients(ping);
    USE_SERIAL.printf("%d Connected websocket clients ping: %d\n", i, ping);
    last_10sec = millis();
  }
  // declaration of temperatures array string
  String temp;
  //start lecture of pixel 
  amg.readPixels(pixels);
  // save temperatures
  for(int i=1; i<=AMG88xx_PIXEL_ARRAY_SIZE; i++){
    temp.concat(pixels[i-1]);
    if( i<AMG88xx_PIXEL_ARRAY_SIZE )temp.concat(",");;
  }
  delay(200);
  // send temperatures to event client 
  webSocket.broadcastTXT(temp);
}
