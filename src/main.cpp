/*
 *                                     SegServer Template
 *            
 *                          Hosts files from SPIFFS local filesystem
 *                          via WiFi, connects to WiFi via captive
 *                          portal, otherwise autoconnects.
 *                          
 *                          Handles GET requests and respond
 *                          flexibly. 
 *                          
 *                          Able to read and write data from SPIFFS
 *                          storage.
 *                                                
 *                                        - Seglectic Softworks 2019
 */

 
/****************************************************************************************************************************************************************
 *  
 *                                                          Libraries
 *  
 ****************************************************************************************************************************************************************/ 

#include <Arduino.h>
#include <FS.h>                   // SPIFFS File system                              https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
#include <ESP8266WiFi.h>          // Main WiFi library                               https://github.com/esp8266/Arduino
#include <DNSServer.h>            // ESP8266 DNS required for captive portal
#include <WiFiManager.h>          // Starts WiFi captive portal for WiFi connection  https://github.com/tzapu/WiFiManager
#include <ESP8266WebServer.h>     // Main web server library
#include <ArduinoOTA.h>           // Allows updating firmware wirelessly             https://docs.platformio.org/en/latest/platforms/espressif8266.html#over-the-air-ota-update




/****************************************************************************************************************************************************************
 *
 *                                                          Web Server File Handling
 *
 *****************************************************************************************************************************************************************/

ESP8266WebServer server(80);                         // Setup web server proper

/*       getContentType()      
 *            
 *    Returns MIME type based
 *    on file extension for 
 *    proper HTTP response
 */
String getContentType(String filename) { 
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

/*      handleFileRead()
 *             
 *    Pulls and sends file to
 *    client else returns false
 */
bool handleFileRead(String path) { 
  Serial.println("Serving file: " + path);
  if (path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file (LOCAL LINKS TO FOLDERS MUST END WITH / )
  String contentType = getContentType(path);              // Get the MIME type
  if (SPIFFS.exists(path)) {                              // If the file exists
    File file = SPIFFS.open(path, "r");                   // Open it
    server.streamFile(file, contentType);                 // And send it to the client
    file.close();                                         // Then close the file again
    return true;        
  }       
  return false;                                           // False if no file
}


/****************************************************************************************************************************************************************
 *  
 *                                                          Vape Relay Setup
 *  
 ****************************************************************************************************************************************************************/ 


String VERSION = "VAPEBOT 1.0.2";
int VAPEPIN = 5;                                          // Relay pin to activate vape
int VAPETIMER = 0;                                        // Timer to hold how long to vape
int VAPEDELTA = 0;                                        // Delta time between ticks
int VAPELAST = 0;                                         // Holds millis() of last frame

//Reads a random line from text file
String getRandLine(String fileName){
  String randLine;                                        // Result of chosen line 
  int fLength = 0;                                        // File line length
  int lineChoice = 0;                                     // Index of random line chosen
  File f = SPIFFS.open(fileName,"r");                     // Load the file
  if (!f){return "Something went wrong!!";}               // Return an error string if no file
  while(f.available()){if(f.read() == '\n'){fLength++;}}  // Count available lines for creating random number
  f.seek(0,SeekSet);                                      // Seek file back to beginning
  lineChoice = roundl(random(0,fLength));                 // Random number between 0 and # of lines
  fLength=0;                                              // Reset the length and use this var as current line
  while(f.available()){
    char fileChar = f.read();                                             
    if(lineChoice==fLength && fileChar!= '\n'){
      randLine += fileChar;                               // If we're on the right line & not a new line, buffer result
    } 
    if(fileChar == '\n'){fLength++;}                      // Else if we get a newline char, add to the line counter
  }
  f.close();
  return randLine;
}

//Web callback for firmware version
void ver(){
  server.send(200,"text/plain","Version: "+VERSION);
}

//Web callback to increment vape timer & send random response
void vape(){
  int timeAdd = server.arg("duration").toInt();
  if(timeAdd){VAPETIMER += timeAdd;}
  server.send(200,"text/plain",getRandLine("/vapeLines.txt"));
}

//Vape processor, applies voltage to relay if vape time remains
void vapeTick(int delta){
  if (VAPETIMER>0){
    VAPETIMER-=delta;
    digitalWrite(VAPEPIN,HIGH);
    Serial.println(VAPETIMER);
  }else{
    digitalWrite(VAPEPIN,LOW);
  }
}

/****************************************************************************************************************************************************************
 *  
 *                                                          Main Setup
 *  
 ****************************************************************************************************************************************************************/ 

void setup() {
  Serial.begin(115200);

  // Setup vapePin for toggling relay
  pinMode(VAPEPIN, OUTPUT);

 /*******************************
  *      Setup OTA Update
  *******************************/
  ArduinoOTA.setPort(8266);                               // Port defaults to 8266
  ArduinoOTA.setHostname("segStripRGB");                  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword((const char *)"Seglectique");    // Set OTA password

 /*******************************
  *      Setup WiFi Manager
  *******************************/
  WiFiManager wifiManager;
  wifiManager.autoConnect("segSetup","seglectic");        //Autoconnect; else start portal with this SSID, P/W
              
  //Serves file to client               
  server.onNotFound([]() {                                // If the client requests any URI
    if (!handleFileRead(server.uri()))                    // send it if it exists
      server.send(404, "text/plain", "404'd");            // otherwise, respond with a 404 (Not Found) error
    });

 /*******************************
  *    Assign web handlers
  *******************************/
  server.on("/vape",vape);
  server.on("/version",ver);

 /*******************************
  *    Start services
  *******************************/
  server.begin();                                         // Starts file server
  SPIFFS.begin();                                         // Starts local file system
  ArduinoOTA.begin();                                     // Starts OTA service
}


/****************************************************************************************************************************************************************
 * 
 *                                                          Main Loop
 *  
 ****************************************************************************************************************************************************************/ 

void loop() {
  VAPEDELTA = millis() - VAPELAST;
  VAPELAST = millis();
  vapeTick(VAPEDELTA);
  server.handleClient();
  ArduinoOTA.handle();
  delay(20);
}
