
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "Hash.h"


const char* ssid = "Hour";
const char* password = "12345678";

//IPAddress ip (192,168,1,129) , getway(192,168,1,1) , subnet(255,255,255,0);
String t ;

AsyncWebServer server(80);                                  // Create AsyncWebServer object on port 80

const char index_html[] PROGMEM = R"rawliteral(%TIMER%)rawliteral";
int count =0;
String processor(const String& var)
{
  if(var == "TIMER")
  {
    return t;
  }

  return String();
}

bool flagAdd = false , flagSub = false ;
bool flagStart = false , flagRestart = false ;
bool flagPause = false , flagContinue = false ;
void setup()
{
  Serial.begin(9600);
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
   //WiFi.softAPConfig(ip, getway , subnet);

  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/timer", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/plain", String(t).c_str());
  });
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
    flagStart = true ;
    Serial.print("start");
    request->send_P(200, String(count) , String(count).c_str());

  });
  server.on("/pause", HTTP_GET, [](AsyncWebServerRequest *request){
    flagPause = true ;
    request->send_P(200, "text/html",index_html, handle_Pause);

  });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request){
    flagRestart = true ;
    request->send_P(200, "text/html",index_html, handle_Restart);

  });
  server.on("/sub", HTTP_GET, [](AsyncWebServerRequest *request){
    flagSub= true ;
//    count-- ;
    handle_SpeedDown();
    request->send_P(200, String(count), String(count).c_str());
    
  });
    server.on("/add", HTTP_GET, [](AsyncWebServerRequest *request){
    flagAdd =true ; 
//     count++ ;
    handle_SpeedUp();
    request->send_P(200, String(count), String(count).c_str());

  });
    server.on("/continues", HTTP_GET, [](AsyncWebServerRequest *request){
    flagContinue = true ;
    request->send_P(200, "text/html",index_html, handle_Continue);

  });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(404,"text/html",index_html, handle_NotFound);

  });
  // Start server
  server.begin();
}
String reading  = " ";
void loop(){
//  for(int i = 0 ; i < 255 ; i++){
//     t = String(i) ; 
//     delay(1000);
//  }
 if(Serial.available()){
  reading = Serial.readString();
  if(reading.startsWith("finished") || reading.startsWith("0:" + String(count))  ){
    for(int i = 0 ; i < 10 ; i++){
      t = "finished" ;
      Serial.println(t);
      delay(1000);
      
    }
  }else if (reading.startsWith("0:")){
    t =  reading ;
    if (reading.startsWith("0:0:0")){
      t = "finished" ;
      
    }
  }
//  Serial.println(reading);
 }
}
String handle_Start(const String& var) {

  if(flagStart ){
    flagStart = false ;
      Serial.print("start");
  }
return String();
}

String handle_Pause(const String& var) {
  if(flagPause){
    Serial.print("Pause");
    flagPause = false ; 
  }
return String();
}

String handle_Restart(const String& var) {
  if(flagRestart ){
    flagRestart = false ;
    Serial.print("restart");
  }
return String();
}
void handle_SpeedUp( ) {
  if(flagAdd){
    count++;
    Serial.print("Speed" + String(count));
    flagAdd = false ;
    t = count;
//    return String(t);
  }
//return String();
}
void handle_SpeedDown( ) {
  if(flagSub && count >= 1){
    count--;
    Serial.print("Speed" + String(count));
    flagSub = false ;
     t = count;
//    return String(t);
  }
//return String();
}
String handle_Continue(const String& var) {
  if(flagContinue ){
      Serial.print("start");
      flagContinue = false ;
  }
  return String();
}

String handle_NotFound(const String& var) {
  if(var =="Not found"){
      return "Not found";
  }
  return String();
}
