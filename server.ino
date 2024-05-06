#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>

#include <HTTPClient.h>
#include <Arduino_JSON.h>

// ESTE PROGRAMA ENVIA IMAGEN SI SE COLOCA EN IP WEB, PERO SI SE COLOCA EN PYTHON ENVIA VIDEO POR LAS ITERACIONES. . . (SI FUNCIONA EN PYTHON)
const char* WIFI_SSID = "Alex5";
const char* WIFI_PASS = "Cristian";

// DEFINICION DE PINES
const int RED_LED = 2;
const int GREEN_LED = 14;
const int BUTTON = 13;
const int CHAPA = 12;
//#define LIGHT_PIN 33
#define LIGHT_PIN 4
const int PWMLightChannel = 4;

unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 10 seconds (10000)
unsigned long timerDelay = 30000;

// variables for the camera and the querys
boolean cameraOn = false;
boolean apiRequests = false;
boolean chapaOn = false;
int cont = 0;
String jsonBuffer;

WebServer server(80); //servidor en el puerto 80

static auto loRes = esp32cam::Resolution::find(320, 240); //baja resolucion
static auto hiRes = esp32cam::Resolution::find(800, 600); //alta resolucion 
//static auto hiRes = esp32cam::Resolution::find(640, 480); //alta resolucion  (para tazas de fps) (IP CAM APP)


void serveJpg() //captura imagen .jpg
{
  ledcWrite(PWMLightChannel, 30);  
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);  // y envia a un cliente (en este caso sera python)
}

void
handleJpgLo()  //permite enviar la resolucion de imagen baja
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  //serveJpg();
}

void
handleJpgHi() //permite enviar la resolucion de imagen alta
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

// funciones para peticiones de API

void triggerStream(){
   if(WiFi.status()== WL_CONNECTED){
    String serverPath = "http://192.168.207.214:8000/stream/";
    
    jsonBuffer = httpGETRequest(serverPath.c_str());
    JSONVar myObject = JSON.parse(jsonBuffer);

    // JSON.typeof(jsonVar) can be used to get the type of the var
    if (JSON.typeof(myObject) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }
   }
}

void sendApiRequests(){
  Serial.print("Hizo la peticion");
  if(WiFi.status()== WL_CONNECTED){
    String serverPath = "http://192.168.207.214:8000/leds/";
    
    jsonBuffer = httpGETRequest(serverPath.c_str());
    Serial.print(jsonBuffer);
    JSONVar myObject = JSON.parse(jsonBuffer);
    // JSON.typeof(jsonVar) can be used to get the type of the var
    if (JSON.typeof(myObject) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }
    
    Serial.print("JSON object = ");
    String response = JSON.stringify(myObject["led"]);
    response.remove(0, 1);
    response.remove(int(response.length() - 1), 1);
    Serial.println(response);
    if (response == "green"){
      apiRequests = false;
      // enciende el led verde y abre la chapa 2 segundos
      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(CHAPA, HIGH);
      delay(2000);
      digitalWrite(CHAPA, LOW);
      digitalWrite(GREEN_LED, LOW);
      // mantiene el led verde encendido
      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, HIGH);
      delay(10000);
    }
    else if (response == "red"){
      apiRequests = false;
      digitalWrite(RED_LED, HIGH);
      digitalWrite(GREEN_LED, LOW);
      delay(10000);
    }

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(CHAPA, LOW);
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  /*WiFi.persistent(false);
  WiFi.mode(WIFI_STA);*/
  WiFi.begin(WIFI_SSID, WIFI_PASS); //nos conectamos a la red wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(1);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMARA OK" : "CAMARA FAIL");
  }
 /*
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/cam-lo.jpg");//para conectarnos IP res baja*/

  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/cam-hi.jpg");//para conectarnos IP res alta

  // server.on("/cam-lo.jpg",handleJpgLo);//enviamos al servidor
  server.on("/cam-hi.jpg", handleJpgHi);

  server.begin();

  // Leds in output for high or low.
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(CHAPA, OUTPUT);
  ledcSetup(PWMLightChannel, 1000, 8);
  pinMode(LIGHT_PIN, OUTPUT);    
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);
  pinMode(BUTTON, INPUT);
  ledcWrite(PWMLightChannel, 0);

  // Iniciar todo en bajo
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(CHAPA, LOW);
}

// Tiempo máximo en milisegundos para considerar un doble clic
const unsigned long DOUBLE_CLICK_TIME = 400;

// Variable para almacenar el tiempo de la última pulsación del botón
unsigned long lastButtonPressTime = 0;

// Variable para rastrear si estamos en espera de un doble clic
bool awaitingDoubleClick = false;

void loop()
{
   // Verifica si el botón ha sido presionado
    if (digitalRead(BUTTON) == LOW and cameraOn == false and apiRequests == false) {
        unsigned long currentTime = millis(); // Obtiene el tiempo actual

        // Verifica si estamos en espera de un doble clic
        if (awaitingDoubleClick) {
            // Si el tiempo entre pulsaciones es menor o igual a DOUBLE_CLICK_TIME, se trata de un doble clic
            if (currentTime - lastButtonPressTime <= DOUBLE_CLICK_TIME) {
                cameraOn = true;
                apiRequests = false;
                lastTime = millis();
                awaitingDoubleClick = false; // Restablece el estado de espera
            }
        } else {
            // De lo contrario, considera que es un solo clic
            awaitingDoubleClick = true;
            lastButtonPressTime = currentTime; // Almacena el tiempo de la pulsación actual
        }

        // Espera a que se suelte el botón para evitar múltiples pulsaciones
        while (digitalRead(BUTTON) == LOW) {
            delay(10); // Pequeña demora para evitar rebotes
        }
    }

    // Si hemos detectado un solo clic y ha pasado el tiempo de espera para un doble clic
    if (awaitingDoubleClick && millis() - lastButtonPressTime > DOUBLE_CLICK_TIME) {
        triggerStream();
        cameraOn = true;
        apiRequests = false;
        lastTime = millis();
        awaitingDoubleClick = false; // Restablece el estado de espera
    }
    
  /*if(digitalRead(BUTTON) == LOW and cameraOn == false and apiRequests == false){
    triggerStream();
    cameraOn = true;
    apiRequests = false;
    lastTime = millis();
  }*/
  else if((millis() - lastTime) < timerDelay and cameraOn == true){
    server.handleClient();
    serveJpg();
    if (timerDelay - (millis() - lastTime) <= 1000){
      ledcWrite(PWMLightChannel, 0);
      cameraOn = false;
      apiRequests = true;
      lastTime = millis();
    }
  }
  else if ((millis() - lastTime) > (timerDelay - 28000) and apiRequests == true) {
    ledcWrite(PWMLightChannel, 0);
    sendApiRequests();
    cont++;
    if(cont == 5){
      digitalWrite(RED_LED, HIGH);
      delay(5000);
      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(CHAPA, LOW);
      cont = 0;
      apiRequests = false;
    }
    lastTime = millis();
  }
}
