/*
  Energy Monitor Shield Emoncms Server Updater

  Ricardo Mena C
  ricardo@crcibernetica.com
  http://crcibernetica.com

  based on:
  SparkFun ESP8266 AT library - Shield and Ping Demo
  Jim Lindblom @ SparkFun Electronics
  Original Creation Date: July 16, 2015
  https://github.com/sparkfun/SparkFun_ESP8266_AT_Arduino_Library  

  License
  **********************************************************************************
  This program is free software; you can redistribute it 
  and/or modify it under the terms of the GNU General    
  Public License as published by the Free Software       
  Foundation; either version 3 of the License, or        
  (at your option) any later version.                    
                                                        
  This program is distributed in the hope that it will   
  be useful, but WITHOUT ANY WARRANTY; without even the  
  implied warranty of MERCHANTABILITY or FITNESS FOR A   
  PARTICULAR PURPOSE. See the GNU General Public        
  License for more details.                              
                                                        
  You should have received a copy of the GNU General    
  Public License along with this program.
  If not, see <http://www.gnu.org/licenses/>.
                                                        
  Licence can be viewed at                               
  http://www.gnu.org/licenses/gpl-3.0.txt

  Please maintain this license information along with authorship
  and copyright notices in any redistribution of this code
  **********************************************************************************
  */

#include <EmonLib.h>//https://github.com/openenergymonitor/EmonLib
#include <SparkFunESP8266WiFi.h>//https://github.com/sparkfun/SparkFun_ESP8266_AT_Arduino_Library
#include <SoftwareSerial.h>
#include <avr/wdt.h>

// Replace these two character strings with the name and
// password of your WiFi network.
#define mySSID "SSID"
#define myPSK "password"


//Your Emon Server IP
#define emonServer "your_server"
#define APIKEY "your_APKEY"


uint8_t nodeID = 8;

#define N_CH 4
EnergyMonitor monitors[N_CH];
float Irms[N_CH];

uint32_t previousMillis = 0;//Will store last time system was updated
uint32_t emonPreviousMillis = 0;//Will store last time emon system was updated
uint32_t check_time = 1000;//Timer interval to check Wifi conection
uint32_t emon_time = 1000;//Timer interval to upload to emoncms

#define db_serial Serial

void setup(){
  delay(2500);
  
  check_time *= 5*60;//Timer interval
  emon_time *= 1*60;//Emon Timer interval
  
  #if defined(db_serial)
    db_serial.begin(9600);
    db_serial.println(F("Starting program"));
  #endif
  
  if(!initializeESP8266()){
    halt(F("Error talking to ESP8266."));
  }else{
    #if defined(db_serial)
      db_serial.println(F("ESP8266 Shield Present"));
    #endif    
  }//end if

  //0.Connection ok
  //1.Error connecting
  //2.Error setting mode.
  uint8_t connection_status = connectESP8266(0);//0 means connect otherwise is reconnecting
  if(!connection_status){
    #if defined(db_serial)
      db_serial.println(F("ESP8266 Shield Present"));
    #endif       
  }else if(connection_status == 1){
    halt(F("Error connecting"));
  }else if(connection_status == 2){
    halt(F("Error setting mode."));
  }//end if
  
  display_connect_info();

  // Current: Analog input pins, calibration.
  //(AC_current÷transformed_voltage)÷18
  for(uint8_t i = 0; i < N_CH; i++){
    monitors[i].current(i+1, 110.63);
  }//end for

  delay(2500);
  #if defined(db_serial)
    db_serial.println(F("Ready...\n"));
  #endif

}//end setup

void loop(){
  updateData();//Read current sensors

  emonUpdate(millis(), emon_time);
  
  check_ESP8266_connection(millis(), check_time);
}//loop

void updateData(){
  for(uint8_t i = 0; i < N_CH; i++){
    Irms[i] = monitors[i].calcIrms(1480);// Current channels reading
//    Power[i] = (Irms[i]*120)/1000;// Apparent Power = I*V = I*120
    #if defined(db_serial)
      db_serial.print(Irms[i]);
      db_serial.print("  ");
    #endif
  }//end for
  
  #if defined(db_serial)
    db_serial.println("");
  #endif
  
}//end updateData

int initializeESP8266(){
  // esp8266.begin() verifies that the ESP8266 is operational
  // and sets it up for the rest of the sketch.
  // It returns either true or false -- indicating whether
  // communication was successul or not.
  int test = esp8266.begin();
  if (test != true){
    return 0;//Return error talking to ESP8266
  }//end if
  return 1;//Return true = Shield Shield Present
}//end initializeESP8266

int connectESP8266(uint8_t reconnect){
  int retVal;
  if(!reconnect){
    // The ESP8266 can be set to one of three modes:
    //  1 - ESP8266_MODE_STA - Station only
    //  2 - ESP8266_MODE_AP - Access point only
    //  3 - ESP8266_MODE_STAAP - Station/AP combo
    // Use esp8266.getMode() to check which mode it's in:
    retVal = esp8266.getMode();
    if (retVal != ESP8266_MODE_STA){ // If it's not in station mode.
      // Use esp8266.setMode([mode]) to set it to a specified mode.
      retVal = esp8266.setMode(ESP8266_MODE_STA);
      if (retVal < 0){
        return 2;//Return error setting mode.
      }//end if
    }//end if
    #if defined(db_serial)
      db_serial.println(F("Mode set to station, ok"));
    #endif    
  }

  // esp8266.status() indicates the ESP8266's WiFi connect status
  // A return value of 1 indicates the device is already
  // connected. 0 indicates disconnected. (Negative values
  // equate to communication errors.)
  retVal = esp8266.status();
  if(retVal){
    return 0;//Return connected
  }else if (retVal <= 0){
    #if defined(db_serial)
      db_serial.print(F("Connecting to "));
      db_serial.println(mySSID);
    #endif
    
    // esp8266.connect([ssid], [psk]) connects the ESP8266
    // to a network.
    // On success the connect function returns a value >0
    // On fail, the function will either return:
    //  -1: TIMEOUT - The library has a set 30s timeout
    //  -3: FAIL - Couldn't connect to network.
    retVal = esp8266.connect(mySSID, myPSK);
    if (retVal < 0){
      return 1;//Return error connecting
    }//end if
  }//end if
  return 0;//Return connected
}//end connectESP8266

void display_connect_info(){
  char connectedSSID[24];
  memset(connectedSSID, 0, 24);
  // esp8266.getAP() can be used to check which AP the
  // ESP8266 is connected to. It returns an error code.
  // The connected AP is returned by reference as a parameter.
  int retVal = esp8266.getAP(connectedSSID);
  if (retVal > 0){
    #if defined(db_serial)
      db_serial.print(F("Connected to: "));
      db_serial.println(connectedSSID);
    #endif   
  }//end if

  // esp8266.localIP returns an IPAddress variable with the
  // ESP8266's current local IP address.
  IPAddress myIP = esp8266.localIP();
  #if defined(db_serial)
    db_serial.print(F("My IP: ")); db_serial.println(myIP);
  #endif
}//end displayConnectInfo

void check_ESP8266_connection(uint32_t timer, uint32_t interval){
  if((timer - previousMillis) > interval) {
    uint8_t connection_status = connectESP8266(1);//1 means reconnect otherwise is reconnecting
    if(!connection_status){
      #if defined(db_serial)
        db_serial.println(F("ESP8266 connection ok"));
      #endif       
    }else if(connection_status == 1){
      halt(F("Error connecting"));
    }else if(connection_status == 2){
      halt(F("Error setting mode."));
    }//end if
    previousMillis = timer;
  }//end if
}

String float_to_string(float value, uint8_t places) {
  // this is used to cast digits 
  int digit;
  float tens = 0.1;
  int tenscount = 0;
  //int i;
  float tempfloat = value;
  String float_obj = "";

    // make sure we round properly. this could use pow from <math.h>, but doesn't seem worth the import
  // if this rounding step isn't here, the value  54.321 prints as 54.3209

  // calculate rounding term d:   0.5/pow(10,places)  
  float d = 0.5;
  if (value < 0){
    d *= -1.0;
  }
  // divide by ten for each decimal place
  for (uint8_t i = 0; i < places; i++){
    d/= 10.0;
  }
  // this small addition, combined with truncation will round our values properly 
  tempfloat +=  d;

  // first get value tens to be the large power of ten less than value
  // tenscount isn't necessary but it would be useful if you wanted to know after this how many chars the number will take

  if (value < 0){
    tempfloat *= -1.0;
  }
  while ((tens * 10.0) <= tempfloat) {
    tens *= 10.0;
    tenscount += 1;
  }
  // write out the negative if needed
  if (value < 0){
    float_obj += "-";
  }//en if
  
  if (tenscount == 0){
    float_obj += String(0, DEC);
  }//en if
  
  for (uint8_t i = 0; i< tenscount; i++) {
    digit = (int) (tempfloat/tens);
    float_obj += String(digit, DEC);
    tempfloat = tempfloat - ((float)digit * tens);
    tens /= 10.0;
  }//en for

  // if no places after decimal, stop now and return
  if (places <= 0){
    return float_obj;
  }//end if

  // otherwise, write the point and continue on
  float_obj += ".";

  // now write out each decimal place by shifting digits one by one into the ones place and writing the truncated value
  for (uint8_t i = 0; i < places; i++) {
    tempfloat *= 10.0; 
    digit = (int) tempfloat;
    float_obj += String(digit,DEC);  
    // once written, subtract off that digit
    tempfloat = tempfloat - (float) digit; 
  }//end for
  return float_obj;
}//end 

void emonUpdate(uint32_t timer, uint32_t interval){
  if((timer - emonPreviousMillis) > interval){
    ESP8266Client client;
    char host[25];
    sprintf(host, "Host: %s", emonServer);    
    if(client.connect(emonServer, 80) <= 0){
      #if defined(DEBUG)
        serial.println(F("Failed to connect to server."));
      #endif
      return;  
    }//end if
    client.print("GET /input/post.json?node=");
    client.print(nodeID);
    client.print("&csv=");
    client.print(Irms[0]);
    client.print(",");
    client.print(Irms[1]);
    client.print(",");
    client.print(Irms[2]);
    client.print(",");
    client.print(Irms[3]);        
    client.print("&apikey=");
    client.print(APIKEY);         //assuming MYAPIKEY is a char or string
    client.println(" HTTP/1.1");   //make sure there is a [space] BEFORE the HTTP
    client.println(host);//Your emonc server IP
    client.print(F("User-Agent: Arduino-ethernet"));
    client.println(F("Connection: close"));
    client.println();
    #if defined(DEBUG)
      serial.println(F("Packet send"));
    #endif
    String msgret = "";
    while (client.available()){
      msgret += char(client.read()); //serial.write(client.read()); // read() gets the FIFO char
    }//end while
    #if defined(DEBUG)
      serial.println(msgret);
    #endif
//    addStatus(msgret.c_str());
    // connected() is a boolean return value - 1 if the 
    // connection is active, 0 if it's closed.
    if (client.connected()){
      client.stop(); // stop() closes a TCP connection.
    }//end if
    emonPreviousMillis = timer;
  }//end if
}//end emonUpdate


void halt(const __FlashStringHelper *error) {
  #if defined(db_serial)
    db_serial.println(error);
    db_serial.println("Restarting");
  #endif
  //addStatus("Restarting");
  delay(1000);
  esp8266.reset();//esp8266 reset
  wdt_enable(WDTO_1S);
  wdt_reset();
  while (1);// {//arduino reset
  //}//end while
}//end halt
