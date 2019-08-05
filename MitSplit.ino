/*
 * Copyright (c) 2017 by 2 Much Sun, LLC
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include "HeatPump.h"
#include <EEPROM.h>

#define VERSION "1.2"
#define ESP8266

// for EEPROM map
#define EEPROM_SIZE 512
#define SSID_START 0
#define SSID_MAX_LENGTH 64
#define PASS_START 64
#define PASS_MAX_LENGTH 64
#define KEY_START 128
#define KEY_MAX_LENGTH 50
#define KEY2_START 178
#define KEY2_MAX_LENGTH 50
#define NODE_START 228
#define NODE_MAX_LENGTH 1
#define HOST_START 229
#define HOST_MAX_LENGTH 32
#define HOST2_START 261
#define HOST2_MAX_LENGTH 32
#define DIRECTORY_START 293
#define DIRECTORY_MAX_LENGTH 32
#define DIRECTORY2_START 325
#define DIRECTORY2_MAX_LENGTH 32
#define LOCATION_START 357
#define LOCATION_MAX_LENGTH 32
#define DISPLAY_FAHREN_START 389
#define DISPLAY_FAHREN_MAX_LENGTH 1
#define MEMORY_CHECK_START 511
#define MEMORY_CHECK_MAX_LENGTH 1

#define SERVER_UPDATE_RATE 10000 //update emoncms server every 10 seconds
#define SERVER_FAST_UPDATE_RATE 2000 //update emoncms server every 2 seconds

#define RED_LED 0
#define AMBER_LED 12
#define LED_UPDATE_RATE 3750
#define SLOW_BLINK_RATE 1000
#define FAST_BLINK_RATE 500
#define RETRY_NETWORK_PERIOD 1800000 //check network every 30 min
ESP8266WebServer server(80);
const char* ssid = "MitSplit";
const char* password = "mitsplit";
const char* html = "Current Room Temp: _ROOMTEMP_\n&deg;_TEMPUNITS_<P><form action = 'change_states'>\n<table>\n"
                   "<tr>\n<td>Power:</td>\n<td>\n_POWER_</td>\n</tr>\n"
                   "<tr>\n<td>Mode:</td>\n<td>\n_MODE_</td>\n</tr>\n"
                   "<tr>\n<td>Set Temp:</td>\n<td>\n_TEMP_</td>\n</tr>\n"
                   "<tr>\n<td>Fan:</td>\n<td>\n_FAN_</td>\n</tr>\n"
                   "<tr>\n<td>Vane:</td><td>\n_VANE_</td>\n</tr>\n"
                   "<tr>\n<td>WideVane:</td>\n<td>\n_WVANE_</td>\n</tr>\n"
                   "</table><BR><BR><TABLE><TR><TD><input type='submit' value='Save Changes'/></TD></FORM>"
                   "<FORM ACTION='settings'><TD><INPUT TYPE='submit' VALUE='Setup'></TD></FORM></TR></TABLE></FONT>"
                   "</body>\n</html>\n";
String st = "not_scanned";
String header = "<HTLML><HEAD><META NAME='viewport' CONTENT='width=device-width, initial-scale=_SCALE_'>"
                   "_REFRESH_"
                   "<BODY><FONT SIZE=5><FONT COLOR=FF00FF> Mit</FONT><B>Split</B></FONT>"
                   "<FONT FACE='Arial'><FONT SIZE=4><B> by 2MuchSun, LLC</B><P><FONT SIZE=5>_LOCATION_</FONT></P>";  //2 Much Sun, LLC
String esid = "";
String epass = "";
String privateKey = "";
String privateKey2 = "";
String node = "0";
String host = "";
String host2 =  "";
String directory = "";
String directory2 = "";
String location_name = "";
const char* e_url = "input/post.json?node=";

const char* inputID_RM_TEMP  = "MIT_RM_TEMP:";
const char* inputID_POWER  = "MIT_POWER:";
const char* inputID_MODE  = "MIT_MODE:";
const char* inputID_SET_TEMP  = "MIT_SET_TEMP:";
const char* inputID_FAN  = "MIT_FAN:";
const char* inputID_VANE  = "MIT_VANE:";
const char* inputID_WIDE_VANE = "MIT_WIDE_VANE:";
const char* inputID_DISPLAY_FAHREN = "FAHRENHEIT:";

int wifi_mode = 0;
unsigned long Timer;
unsigned long Timer2;
unsigned long Timer3;
unsigned long Timer4;
unsigned long reset_timer;
unsigned long reset_timer2;

bool blink_LED1 = false;
bool blink_LED2 = false;
bool immediate_update = false;

// determines server state
int server_down = 0;
int server2_down = 0;

//used to display last digit of assigned IP Address using LEDs
int timer_enabled = 0;
int first_ip_digit;
int second_ip_digit;
int third_ip_digit;
int digits;
int ip_position;
int total_ip;

int display_Fahrenheit;
int count = 0;
int timer_rate = SERVER_UPDATE_RATE;

String power[2] = {"OFF", "ON"};
String mode_hp[5] = {"HEAT", "DRY", "COOL", "FAN", "AUTO"};
String fan[6] = {"AUTO", "QUIET", "1", "2", "3", "4"};
String vane[7] = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
String widevane[7] = {"<<", "<", "|", ">", ">>", "<>", "SWING"}; 

HeatPump hp;

String readEEPROM(int start_byte, int allocated_size) {
  String variable;
  for (int i = start_byte; i < (start_byte + allocated_size); ++i) {
    variable += char(EEPROM.read(i));
  }
  delay(10);
  return variable;
}

void writeEEPROM(int start_byte, int allocated_size, String contents) {
  int length_of_contents = contents.length();
  if (length_of_contents > allocated_size)
    length_of_contents = allocated_size;
  for (int i = 0; i < length_of_contents; ++i) {
    EEPROM.write(start_byte + i, contents[i]);
  }
  for (int i = (start_byte + length_of_contents); i < (start_byte + allocated_size); ++i)
    EEPROM.write(i,0);
  EEPROM.commit();
  delay(10);
}

void resetEEPROM(int start_byte, int end_byte) {
  for (int i = start_byte; i < end_byte; ++i) {
   EEPROM.write(i, 0);
  }
  EEPROM.commit();   
}

void downloadEEPROM() {
  String mem_check;
  String temp_str;
  mem_check = readEEPROM(MEMORY_CHECK_START, MEMORY_CHECK_MAX_LENGTH);
  if (mem_check != "B"){    //if memory hasn't been set up or corrupt
    resetEEPROM(0, EEPROM_SIZE);
  }
  else {
    esid = readEEPROM(SSID_START, SSID_MAX_LENGTH);
    epass = readEEPROM(PASS_START, PASS_MAX_LENGTH);
    privateKey  = readEEPROM(KEY_START, KEY_MAX_LENGTH);
    privateKey2  = readEEPROM(KEY2_START, KEY2_MAX_LENGTH);
    node = readEEPROM(NODE_START, NODE_MAX_LENGTH);
    host = readEEPROM(HOST_START, HOST_MAX_LENGTH);
    host2 = readEEPROM(HOST2_START, HOST2_MAX_LENGTH);
    directory = readEEPROM(DIRECTORY_START, DIRECTORY_MAX_LENGTH);
    directory2 = readEEPROM(DIRECTORY2_START, DIRECTORY2_MAX_LENGTH);
    location_name = readEEPROM(LOCATION_START, LOCATION_MAX_LENGTH);
    if (location_name == "")
      location_name = "My Room";
    location_name = location_name.c_str();
    temp_str = readEEPROM(DISPLAY_FAHREN_START, DISPLAY_FAHREN_MAX_LENGTH);
    display_Fahrenheit = temp_str.toInt();
  }
}


void bootOTA() {
  ArduinoOTA.onStart([]() {
  });
  ArduinoOTA.onEnd([]() {
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
   /* if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");*/
  });
  ArduinoOTA.begin();
}

void setup() {
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED,HIGH);
  pinMode(AMBER_LED, OUTPUT);
  digitalWrite(AMBER_LED,HIGH);
    //Serial.begin(115200);
  EEPROM.begin(512);
  char tmpStr[40];
  downloadEEPROM();
  //go ahead and make a list of networks in case user needs to change it
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  st = "<SELECT NAME='ssid'>";
  int found_match = 0;
  delay(1500);
  for (int i = 0; i < n; ++i) {
    st += "<OPTION VALUE='";
    st += String(WiFi.SSID(i)) + "'";
    if (String(WiFi.SSID(i)) == esid.c_str()) {
      found_match = 1;
      st += "SELECTED";
    }
    st += "> " + String(WiFi.SSID(i));
    st += " </OPTION>";
  }
  if (!found_match)
    if (esid != 0) {
      st += "<OPTION VALUE='" + esid + "'SELECTED>" + esid + "</OPTION>";
    }
    else {
      if (!n)
        st += "<OPTION VALUE='not chosen'SELECTED> No Networks Found!  Select Rescan or Manually Enter SSID</OPTION>";
      else
        st += "<OPTION VALUE='not chosen'SELECTED> Choose One </OPTION>";
    }
  st += "</SELECT>";
  delay(100);     
  if ( esid != 0 ) { 
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(esid.c_str(), epass.c_str());
    delay(50);
    int t = 0;
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      t++;
      if (t >= 20) {
        delay(2000);
        WiFi.disconnect();
        WiFi.begin(esid.c_str(), epass.c_str());
        t = 0;
        attempt++;
        if (attempt >= 10) {
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
          delay(100);
          int n = WiFi.scanNetworks();
          delay(1000);
          st = "<SELECT NAME='ssid'><OPTION VALUE='not chosen'SELECTED> Try again </OPTION>";
          esid = ""; // clears out esid in case only the password is incorrect-used only to display the right instructions to user
          for (int i = 0; i < n; ++i) {
            st += "<OPTION VALUE='";
            st += String(WiFi.SSID(i)) + "'> " + String(WiFi.SSID(i));
            st += " </OPTION>";
          }
          st += "</SELECT>";
          delay(100);
          WiFi.softAP(ssid, password);
          IPAddress myIP = WiFi.softAPIP();
          wifi_mode = 1;
          break;
        }
      }
    }
  }
  else {
    delay(100);
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();    
    wifi_mode = 2; //AP mode with no SSID in EEPROM
  }
  
  if (wifi_mode == 0) {
    get_ip_address_for_LED();
  }
  delay(100);
  hp.connect(&Serial);
  //hp.setSettings({ //set some default settings
  /*    "OFF",  /* ON/OFF */
  /*    "HEAT", /* HEAT/COOL/FAN/DRY/AUTO */
  /*     25,    /* Between 16 and 31 */
  /*    "AUTO",   /* Fan speed: 1-4, AUTO, or QUIET */
  /*    "4",   /* Air direction (vertical): 1-5, SWING, or AUTO */
  /*    "|"    /* Air direction (horizontal): <<, <, |, >, >>, <>, or SWING */
  //});
  delay(100);
  server.on("/a", handleCfg);
  server.on("/confirm", handleCfm);
  server.on("/rescan", handleRescan);
  server.on("/", handle_root);
  server.on("/change_states", handle_change_states);
  server.on("/settings", handleSettings);
  server.on("/r", handleRapiR);
  server.on("/reset", handleRst);
  server.begin(); 
  delay(100);
  bootOTA();
  Timer = millis();
  Timer2 = millis();
  Timer3 = millis();
  Timer4 = millis();
}

void get_ip_address_for_LED(){
  IPAddress myAddress = WiFi.localIP();
  delay(100);
  if (myAddress[3] <= 9){
    first_ip_digit = 0;
    second_ip_digit = 0;
    third_ip_digit = myAddress[3];
    digits = 2;
  }
  else if (myAddress[3] <= 99){
    first_ip_digit = 0;
    second_ip_digit = myAddress[3]/10;
    third_ip_digit = myAddress[3] - second_ip_digit*10;  
    digits = 3;    
  }
  else {
    first_ip_digit = myAddress[3]/100;
    second_ip_digit = (myAddress[3] - first_ip_digit*100)/10;
    third_ip_digit = myAddress[3] - first_ip_digit*100 - second_ip_digit*10; 
    digits= 4;    
  }
  total_ip = first_ip_digit + second_ip_digit + third_ip_digit + digits;
  ip_position = 1;
}

void updateRedLED(){
  int blink_rate;
 
  if (wifi_mode == 1)
    blink_rate = FAST_BLINK_RATE;
  else if (wifi_mode == 2)
    blink_rate = SLOW_BLINK_RATE;
  else if (wifi_mode == 0)
    return;  
  if (((millis() - Timer3) >= blink_rate)) {
    if (blink_LED1){
      blink_LED1 = false;
      digitalWrite(RED_LED,HIGH);
    }
    else{
      blink_LED1 = true;
      digitalWrite(RED_LED,LOW);
    }
    Timer3 = millis();
  }
}

void blinkYellowLED(){
  if (blink_LED2){
    blink_LED2 = false;
    digitalWrite(AMBER_LED,HIGH);
    ip_position++;
   }
   else{
     blink_LED2 = true;
     digitalWrite(AMBER_LED,LOW);
   }
}

void blinkRedLED(){
  if (blink_LED2){
    blink_LED2 = false;
    digitalWrite(RED_LED,HIGH);
    ip_position++;
   }
   else{
     blink_LED2 = true;
     digitalWrite(RED_LED,LOW);
   }
}

void updateYellowLED(){  
  if (((millis() - Timer4) >= FAST_BLINK_RATE) && wifi_mode == 0) {
    if (WiFi.status() != WL_CONNECTED){
      digitalWrite(AMBER_LED,LOW);
      digitalWrite(RED_LED,HIGH);
      blink_LED1 = false;
      blink_LED2 = false;
      ip_position = 1;
      return;
    }
    if (ip_position <= 2){   
      blinkRedLED();
      digitalWrite(AMBER_LED,HIGH);
    }
    else {
      if (digits == 2){
        if (ip_position <= total_ip)
           blinkYellowLED();
        else
           ip_position = 1;
      }
      if (digits == 3){
        if (ip_position <= (2 + second_ip_digit))
           blinkYellowLED();
        else if (ip_position == (3 + second_ip_digit))  
           blinkRedLED();
        else if (ip_position <= total_ip)
           blinkYellowLED();
        else
           ip_position = 1;
      }
      if (digits == 4){
        if (ip_position <= (2 + first_ip_digit))
           blinkYellowLED();
        else if (ip_position == (3 + first_ip_digit))  
           blinkRedLED();
        else if (ip_position <= (3 + first_ip_digit + second_ip_digit))
           blinkYellowLED();
        else if (ip_position == (4 + first_ip_digit + second_ip_digit))  
           blinkRedLED();
        else if (ip_position <= total_ip)
           blinkYellowLED();
        else
           ip_position = 1;
       }
    }
    Timer4 = millis();   
  }
}

void checkReset(){
  int erase = 0;
  int buttonState;
  if (!blink_LED1 && !blink_LED2) {
    pinMode(RED_LED,INPUT);
    delay(50);
    buttonState = digitalRead(0);
    while (buttonState == LOW) {
      buttonState = digitalRead(0);
      erase++;
      if (erase >= 15000) {  //increased the hold down time before erase
        resetEEPROM(0, SSID_MAX_LENGTH + PASS_MAX_LENGTH); // only want to erase ssid and password
        int erase = 0;
        WiFi.disconnect();
        delay(2000);
        ESP.reset();
      } 
    }
    pinMode(RED_LED,OUTPUT);
    delay(50);
  }
}

void retryNetworkConnection() {
  // Remain in AP for 30 min before trying again if SSID was saved
  if ((millis() - Timer2) >= RETRY_NETWORK_PERIOD){
    Timer2 = millis(); 
    if (wifi_mode == 1){
      esid = readEEPROM(SSID_START, SSID_MAX_LENGTH);
      if (esid != 0){ 
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        WiFi.begin(esid.c_str(), epass.c_str());
        delay(500);
        int t = 0;
        while ((WiFi.status() != WL_CONNECTED) && (t <= 19)) {   
          delay(500);
          t++;
        }
        if (WiFi.status() == WL_CONNECTED){
          wifi_mode = 0;
          int n = WiFi.scanNetworks();
          st = "<SELECT NAME='ssid'>";
          delay(1500);
          for (int i = 0; i < n; ++i) {
            st += "<OPTION VALUE='";
            st += String(WiFi.SSID(i)) + "'";
            if (String(WiFi.SSID(i)) == esid.c_str()) {
              st += "SELECTED";
            }
            st += "> " + String(WiFi.SSID(i));
            st += " </OPTION>";
          }
          get_ip_address_for_LED();
        }
        else {
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
          delay(100);
          WiFi.softAP(ssid, password);
          esid = "";
        }  
      }
    }
  }
}

void loop() {
  ArduinoOTA.handle();        // initiates OTA update capability
  server.handleClient();
  updateRedLED();
  updateYellowLED();
  checkReset();
  retryNetworkConnection();
  hp.sync();
  if (wifi_mode == 0 && privateKey != 0) { 
    if (((millis() - Timer) >= timer_rate)) {
      // We now create a URL for data upload request
      String url;
      String url2;
      String tmp;
      String mode_num;
      String url_rtemp;
      String url_set_temp;
      String url_power;
      String url_mode;
      String url_fan;
      String fan_num;
      String url_vane;
      String vane_num;
      String url_wide_vane;
      String wide_vane_num;
      if (immediate_update){
        count += 1;
        if (count > 9){          
          timer_rate = SERVER_UPDATE_RATE;
          immediate_update = false;
          count = 0;
        }
      }
      if (hp.getModeSetting() == "HEAT")
        mode_num = "0";
      if (hp.getModeSetting() == "DRY")
        mode_num = "1";
      if (hp.getModeSetting() == "COOL")
        mode_num = "2";
      if (hp.getModeSetting() == "FAN")
        mode_num = "3";
      if (hp.getModeSetting() == "AUTO")
        mode_num = "4";
      if (hp.getFanSpeed() == "AUTO")
        fan_num = "0";
      if (hp.getFanSpeed() == "QUIET")
        fan_num = "1";
      if (hp.getFanSpeed() == "1")
        fan_num = "2";
      if (hp.getFanSpeed() == "2")
        fan_num = "3";
      if (hp.getFanSpeed() == "3")
        fan_num = "4";
      if (hp.getFanSpeed() == "4")
        fan_num = "5";
      if (hp.getVaneSetting() == "AUTO")
        vane_num = "0";
      if (hp.getVaneSetting() == "1")
        vane_num = "1";
      if (hp.getVaneSetting() == "2")
        vane_num = "2";
      if (hp.getVaneSetting() == "3")
        vane_num = "3";
      if (hp.getVaneSetting() == "4")
        vane_num = "4";
      if (hp.getVaneSetting() == "5")
        vane_num = "5";
      if (hp.getVaneSetting() == "SWING")
        vane_num = "6";
       if (hp.getWideVaneSetting() == "<<")
        wide_vane_num = "0";
      if (hp.getWideVaneSetting() == "<")
        wide_vane_num = "1";
      if (hp.getWideVaneSetting() == "|")
        wide_vane_num = "2";
      if (hp.getWideVaneSetting() == ">")
        wide_vane_num = "3";
      if (hp.getWideVaneSetting() == ">>")
        wide_vane_num = "4";
      if (hp.getWideVaneSetting() == "<>")
        wide_vane_num = "5";
      if (hp.getWideVaneSetting() == "AUTO")
        wide_vane_num = "6";
      url_rtemp = inputID_RM_TEMP;
      if (display_Fahrenheit)
       url_rtemp += String(hp.getRoomTemperature()*1.8 + 32,1);
      else
        url_rtemp += hp.getRoomTemperature();
      url_rtemp += ",";
      url_power = inputID_POWER;
      url_power += hp.getPowerSettingBool();
      url_power += ",";
      url_mode = inputID_MODE;
      url_mode += mode_num;
      url_mode += ",";
      url_set_temp = inputID_SET_TEMP;
      if (display_Fahrenheit)
        url_set_temp += String(hp.getTemperature()*1.8 + 32,1);
      else
        url_set_temp += hp.getTemperature();
      url_set_temp += ","; 
      url_fan = inputID_FAN;
      url_fan += fan_num;
      url_fan += ","; 
      url_vane = inputID_VANE;
      url_vane += vane_num;
      url_vane += ",";
      url_wide_vane = inputID_WIDE_VANE;
      url_wide_vane += wide_vane_num;
      url_wide_vane += ",";
      String url_display_Fahrenheit = inputID_DISPLAY_FAHREN;
      url_display_Fahrenheit += display_Fahrenheit;
      tmp = e_url;
      tmp += node; 
      tmp += "&json={"; 
      tmp += url_rtemp;
      tmp += url_power;
      tmp += url_mode;
      tmp += url_set_temp;
      tmp += url_fan;
      tmp += url_vane;
      tmp += url_wide_vane;
      tmp += url_display_Fahrenheit;  
      tmp += "}&"; 
      url = directory.c_str();   //needs to be constant character to filter out control characters padding when read from memory
      url += tmp;
      url2 = directory2.c_str();    //needs to be constant character to filter out control characters padding when read from memory
      url2 += tmp;
      url += privateKey.c_str();    //needs to be constant character to filter out control characters padding when read from memory
      url2 += privateKey2.c_str();  //needs to be constant character to filter out control characters padding when read from memory
      // Use WiFiClient class to create TCP connections
      WiFiClient client;
      const int httpPort = 80;  
      if (server2_down && (millis()-reset_timer2) > 600000)   //retry in 10 minutes
        server2_down = 0;  
      if (!server2_down) {  
        if (!client.connect(host2.c_str(), httpPort)) { //needs to be constant character to filter out control characters padding when read from memory
          server2_down = 1;  
          reset_timer2 = millis();
        }
        else { 
          client.print(String("GET ") + url2 + " HTTP/1.1\r\n" + "Host: " + host2.c_str() + "\r\n" + "Connection: close\r\n\r\n");
          delay(10);
          while (client.available()) {
            String line = client.readStringUntil('\r');
          }
        }
      }    
      if (server_down && (millis()-reset_timer) > 600000)   //retry in 10 minutes 
        server_down = 0;  
      if (!server_down) { 
        if (!client.connect(host.c_str(), httpPort)) { //needs to be constant character to filter out control characters padding when read from memory    
          server_down = 1; 
          reset_timer = millis();
        }
        else {
          // This will send the request to the server
          client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host.c_str() + "\r\n" + "Connection: close\r\n\r\n");
          delay(10);
          while (client.available()) {
            String line = client.readStringUntil('\r');
          }
        }
      }
      Timer = millis();
    }
  }
}



void handleCfg() {
  String s;
  String qsid = server.arg("ssid");
  String qhsid = server.arg("hssid");
  if (qhsid != "empty")
    qsid = qhsid;
  String qpass = server.arg("pass");   
  String qkey = server.arg("ekey"); 
  String qkey2 = server.arg("ekey2");
  String qnode = server.arg("node");
  String qhost = server.arg("host");       
  String qhost2 = server.arg("host2");  
  String qdirectory = server.arg("dir");
  String qdirectory2 = server.arg("dir2");
  String qlocation = server.arg("loc");
  String qFahren = server.arg("Fahr");

  writeEEPROM(PASS_START, PASS_MAX_LENGTH, qpass);
  writeEEPROM(MEMORY_CHECK_START, MEMORY_CHECK_MAX_LENGTH, "B");  //use to indicate if memory is ready
  
  if (wifi_mode == 0) {
    if (privateKey != qkey) {
      writeEEPROM(KEY_START, KEY_MAX_LENGTH, qkey);
      privateKey = qkey;
    }
    if (privateKey2 != qkey2) {
      writeEEPROM(KEY2_START, KEY2_MAX_LENGTH, qkey2);
      privateKey2 = qkey2;
    }
    if (node != qnode) {    
      writeEEPROM(NODE_START, NODE_MAX_LENGTH, qnode);
      node = qnode;
    }
    if (host != qhost) {
      writeEEPROM(HOST_START, HOST_MAX_LENGTH, qhost);
      host = qhost;
    }
    if (host2 != qhost2) {
      writeEEPROM(HOST2_START, HOST2_MAX_LENGTH, qhost2);
      host2 = qhost2;
    }
    if (directory != qdirectory) {
      writeEEPROM(DIRECTORY_START, DIRECTORY_MAX_LENGTH, qdirectory);
      directory = qdirectory;
    }
    if (directory2 != qdirectory2) {
      writeEEPROM(DIRECTORY2_START, DIRECTORY2_MAX_LENGTH, qdirectory2);
      directory2 = qdirectory2;
    }
    if (location_name != qlocation) {
      writeEEPROM(LOCATION_START, LOCATION_MAX_LENGTH, qlocation);
      location_name = qlocation;
    }
     if (display_Fahrenheit != qFahren.toInt()) {
      writeEEPROM(DISPLAY_FAHREN_START, DISPLAY_FAHREN_MAX_LENGTH, qFahren);
      display_Fahrenheit = qFahren.toInt();
    }
  }
  String tmp = header;
  tmp.replace("_LOCATION_",location_name);
  tmp.replace("_REFRESH_","");
  tmp.replace("_SCALE_","0.6");
  s = tmp;
  if (qsid != "not chosen") {
    writeEEPROM(SSID_START, SSID_MAX_LENGTH, qsid);
    s += "<P><FONT SIZE=4>Updating Settings...</P>";
    if (qsid != esid.c_str() || qpass != epass.c_str()) {
      s += "<P>Saved to Memory...</P>";
      s += "<P>The MitSplit will reset and try to join " + qsid + "</P>";
      s += "<P>After about 30 seconds, if successful, please use the IP address</P>";
      s += "<P>assigned by your DHCP server to the MitSplit in your Browser</P>";
      s += "<P>in order to re-access the Setup page.</P>";
      s += "<P>---------------------</P>";
      s += "<P>If unsuccessful after 90 seconds, the  MitSplit will go back to the";
      s += "<P>default access point at SSID:MitSplit.</P>";
      s += "</FONT></HTML>\r\n\r\n";
      server.send(200, "text/html", s);
      WiFi.disconnect();
      delay(2000);  
      ESP.reset();  
    }
    else {
      s += "<FORM ACTION='.'>";
      s += "<P>Saved to Memory...</P>";
      s += "<P><INPUT TYPE=submit VALUE='Continue'></P>";
    }
  }
  else {
     s += "<P><FONT SIZE=5>Warning. No network selected.</P>";
     s += "<P>All functions except data logging will continue to work.</P>";
     s += "<FORM ACTION='.'>";
     s += "<P><INPUT TYPE=submit VALUE='     OK     '></P>";
  }
  s += "</FORM></FONT></HTML>\r\n\r\n";
  server.send(200, "text/html", s);
}

void handleSettings() {
  IPAddress myAddress = WiFi.localIP();
  char tmpStr[20];
  String s;
  String sTmp;
  String tmp = header;
  tmp.replace("_LOCATION_",location_name + " Setup");
  tmp.replace("_REFRESH_","");
  tmp.replace("_SCALE_","0.8");
  s = tmp;
  s += "<P><FONT SIZE=2>WiFi FW v";
  s += VERSION;
  s += "</P>";
  if (wifi_mode == 0 ){
    sprintf(tmpStr,"%d.%d.%d.%d",myAddress[0],myAddress[1],myAddress[2],myAddress[3]);
    sTmp = String(tmpStr);
  }
  else
    sTmp = "192.168.4.1";
  s += "<P>Connected at ";
  s += tmpStr;
  s += "</FONT></P>";
  s += "<P>====================</P>";
  s += "<P><B>NETWORK CONNECT</B></P>";
  s += "<FORM ACTION='rescan'>";  
  s += "<INPUT TYPE=submit VALUE='     Rescan     '>";
  s += "</FORM>";
  s += "<FORM METHOD='get' ACTION='a'>";
  if (wifi_mode == 0)
    s += "<B><I>Connected to </B></I>";
  else
    s += "<B><I>Choose a network </B></I>";
  s += st;
  s += "<P><LABEL><B><I>&nbsp;&nbsp;&nbsp;&nbsp;or enter SSID manually:</B></I></LABEL><INPUT NAME='hssid' MAXLENGTH='32' VALUE='empty'></P>";
  s += "<P><LABEL><B><I>Password:</B></I></LABEL><INPUT TYPE='password' SIZE = '25' NAME='pass' MAXLENGTH='32' VALUE='";
  sTmp = "";
  for (int i = 0; i < epass.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (epass[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += epass[i];
    }
  s += sTmp.c_str();       //needs to be constant character to filter out control characters padding when read from memory
  s += "'></P>";
  s += "<P>=====================</P>";
  s += "<P><B>DATABASE SERVER</B></P>";
  if (wifi_mode != 0) {
    s += "<P>Note. You are not connected to any network so no data will be sent</P>";
    s += "<P>out. However, you can still control your MitSplit by selecting (Home Page).</P>";
    s += "<P>If you do want to send data, then please fill in the info above and (Save Changes).</P>";
    s += "<P>After you successfully connected to your network,</P>";
    s += "<P>please select (Setup) and fill in</P>";
    s += "<P>the appropiate information about the database server.</P>";
  }
  else {
    s += "<P>Fill in the appropriate information about the</P>";
    s += "<P>Emoncms server you want use.</P>";
  }
  s += "<P>__________</P>";
  if (wifi_mode == 0) {
    s += "<P><B><I>Primary Server</B></I></P>";
    s += "<P><LABEL><I>Write key (devicekey=1..32):</I></LABEL><INPUT NAME='ekey' MAXLENGTH='50' VALUE='";
    sTmp = "";
    for (int i = 0; i < privateKey.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (privateKey[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += privateKey[i]; 
    }
    s += sTmp.c_str();     //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";
    s += "<P><LABEL><I>Server address (example.com):</I></LABEL><INPUT NAME='host' MAXLENGTH='32' VALUE='";
    sTmp = "";
    for (int i = 0; i < host.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (host[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += host[i];
    }
    s += sTmp.c_str();    //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";
    s += "<P><LABEL><I>Database directory (/emoncms/):</I></LABEL><INPUT NAME='dir' MAXLENGTH='32' VALUE='";
    sTmp = "";
    for (int i = 0; i < directory.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (directory[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += directory[i];
    }
    s += sTmp.c_str();    //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";
    s += "<P>__________</P>";   
    s += "<P><B><I>Backup Server (optional)</B></I></P>";
    s += "<P><LABEL><I> Write key (apikey=1..32):</I></LABEL><INPUT NAME='ekey2' MAXLENGTH='50' VALUE='";
    sTmp = "";
    for (int i = 0; i < privateKey2.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (privateKey2[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += privateKey2[i];
    }
    s += sTmp.c_str();    //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";
    s += "<P><LABEL><I>Server address (example2.com):</I></LABEL><INPUT NAME='host2' mzxlength='31' VALUE='";
    sTmp = "";
    for (int i = 0; i < host2.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (host2[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += host2[i];
    }
    s += sTmp.c_str();    //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";  
    s +=  "<P><LABEL><I>Database directory (/):</I></LABEL><INPUT NAME='dir2' MAXLENGTH='32' VALUE='";
    sTmp = "";
    for (int i = 0; i < directory2.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (directory2[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += directory2[i];
    }
    s += sTmp.c_str();    //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";
    s += "<P>__________</P>";
    s += "<P><LABEL><I>Node for both servers (default is 0):</I></LABEL><SELECT NAME='node'>"; 
    for (int i = 0; i <= 8; ++i) {
      s += "<OPTION VALUE='" + String(i) + "'";
      if (node == String(i))
      s += "SELECTED";
      s += ">" + String(i) + "</OPTION>";
    }
    s += "</SELECT></P>";
    s += "<P>====================</P>";
    s +=  "<P><LABEL>Name or location of unit:</LABEL><INPUT NAME='loc' MAXLENGTH='32' VALUE='";
    sTmp = "";
    for (int i = 0; i < location_name.length(); ++i) {     // this is to allow single quote entries to be displayed
      if (location_name[i] == '\'')
        sTmp += "&#39;";
      else
        sTmp += location_name[i];
    }
    s += sTmp.c_str();    //needs to be constant character to filter out control characters padding when read from memory
    s += "'></P>";
    s += "<P>====================</P>";
    s += "<P>Show as  <INPUT TYPE='radio' NAME='Fahr' VALUE='1'";
    if (display_Fahrenheit) // Fahrenheit
      s += " CHECKED";
    s += ">Fahrenheit   <INPUT TYPE='radio' NAME='Fahr' VALUE='0'";
    if (display_Fahrenheit == 0) //Celsius
      s += " CHECKED";
    s += ">Celsius</P>";
  }  
  s += "&nbsp;<TABLE><TR>";
  s += "<TD><INPUT TYPE=submit VALUE=' Save Changes  '></TD>";
  s += "</FORM><FORM ACTION='.'>";
  s += "<TD><INPUT TYPE=submit VALUE='    Home   '></TD>";
  s += "</FORM>";
  s += "</TR></TABLE>";
  s += "<FORM ACTION='confirm'>";  
  s += "<P>&nbsp;<INPUT TYPE=submit VALUE='Erase Settings'></P>";
  s += "</FORM></FONT>";
  s += "</HTML>\r\n\r\n";
  server.send(200, "text/html", s);
}

void handleRescan() {
  String s;
  String tmp = header;
  tmp.replace("_LOCATION_",location_name);
  tmp.replace("_REFRESH_","");
  tmp.replace("_SCALE_","0.6");
  s = tmp;
  s += "<P><FONT SIZE=4>Rescanning...</P>";
  s += "<P>Note.  You may need to manually reconnect to the access point after rescanning.</P>";  
  s += "<P><B>Please wait at least 30 seconds before continuing.</B></P>";
  s += "<FORM ACTION='settings'>";  
  s += "<P><INPUT TYPE=submit VALUE='Continue'></P>";
  s += "</FORM></P>";
  s += "</FONT></HTML>\r\n\r\n";
  server.send(200, "text/html", s);
  WiFi.disconnect();
  delay(2000);  
  ESP.reset();
}

void handleRst() {
  String s;
  String tmp = header;
  tmp.replace("_LOCATION_",location_name + " Reset to Default");
  tmp.replace("_REFRESH_","");
  tmp.replace("_SCALE_","0.6");
  s = tmp;
  s += "<P>Clearing the EEPROM...</P>";
  s += "<P>The MitSplit will reset and have an IP address of 192.168.4.1</P>";
  s += "<P>After about 30 seconds, the MitSplit will activate the access point</P>";
  s += "<P>SSID:MitSplit and password:mitsplit</P>";
  s += "</FONT></FONT></HTML>\r\n\r\n";
  resetEEPROM(0, EEPROM_SIZE);
  server.send(200, "text/html", s);
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
}

void handleCfm() {
  String s;
  String tmp = header;
  tmp.replace("_LOCATION_",location_name + " Confirmation");
  tmp.replace("_REFRESH_","");
  tmp.replace("_SCALE_","0.6");
  s = tmp;
  s += "<P>You are about to erase the settings!</P>";
  s += "<FORM ACTION='reset'>";
  s += "&nbsp;<TABLE><TR>";
  s += "<TD><INPUT TYPE=submit VALUE='    Continue    '></TD>";
  s += "</FORM><FORM ACTION='settings'>";
  s += "<TD><INPUT TYPE=submit VALUE='    Cancel    '></TD>";
  s += "</FORM>";
  s += "</TR></TABLE>";
  s += "</FONT></HTML>";
  s += "\r\n\r\n";
  server.send(200, "text/html", s);
}

String encodeString(String toEncode) {
  toEncode.replace("<", "&lt;");
  toEncode.replace(">", "&gt;");
  toEncode.replace("|", "&vert;");
  return toEncode;
}

String createOptionSelector(String name, const String values[], int len, String value) {
  String str = "<select name='" + name + "'>\n";
  for (int i = 0; i < len; i++) {
    String encoded = encodeString(values[i]);
    str += "<option value='";
    str += values[i];
    str += "'";
    str += values[i] == value ? " selected" : "";
    str += ">";
    str += encoded;
    str += "</option>\n";
  }
  str += "</select>\n";
  return str;
}

void handleRapiR() {
  int ctemp;
  String s;
  String rapiString;
  String rapi = server.arg("rapi");
  String argument = String((int)rapi[1] - 65);
  switch (rapi[0]){
    case 'P':
      argument = power[argument.toInt()];
      hp.setPowerSetting(argument);
      break;
    case 'M':
      argument = mode_hp[argument.toInt()];
      hp.setModeSetting(argument);
      break;
    case 'T':
      ctemp = argument.toInt() + 16;
      hp.setTemperature(ctemp);
      break;
    case 'F':
      argument = fan[argument.toInt()];
      hp.setFanSpeed(argument);
      break;
    case 'V':
      argument = vane[argument.toInt()];
      hp.setVaneSetting(argument);
      break;
    case 'W':
      argument = widevane[argument.toInt()];
      hp.setWideVaneSetting(argument);
      break;
    default:
      break;
    }
  hp.update();
  Timer = millis();
  immediate_update = true;
  timer_rate = SERVER_FAST_UPDATE_RATE;
  String tmp = header;
  tmp.replace("_LOCATION_",location_name + " RAPI");
  tmp.replace("_REFRESH_","");
  tmp.replace("_SCALE_","1");
  s = tmp;
  s += "<P>Commands Set</P>";
  s += "<P>Power (OFF - ON) - P(A - B)</P>";
  s += "<P>Set Fan Speed (0 - 6) - F(A - G)</P>";
  s += "<P>Set Mode (0 - 3) - M(A - D)</P>";
  s += "<P>Set Temperature (16 - 31) - T(A - P)</P>";
  s += "<P>Set Vane (0 - 6) - V(A - G)</P>";
  s += "<P>Set Wide Vane (0 - 6) - W(A - G)</P>";
  s += "<P>";
  s += "<P><FORM METHOD='get' ACTION='r'><LABEL><B><I>RAPI Command:</B></I></LABEL><INPUT NAME='rapi' MAXLENGTH='32'></P>";
  s += "<P>&nbsp;<TABLE><TR>";
  s += "<TD><INPUT TYPE=SUBMIT VALUE='    Submit    '></TD>";
  s += "</FORM><FORM ACTION='.'>";
  s += "<TD><INPUT TYPE=SUBMIT VALUE='    Home   '></TD>";   s += "</FORM>";
  s += "</TR></TABLE></P>";
  s += rapi;
  s += "<P>";
  s += "</P></FONT></HTML>\r\n\r\n";
  server.send(200, "text/html", s);
}

void handle_root() {
  String rtemp;
  String toSend = header + html;
  toSend.replace("_REFRESH_", "<META HTTP-EQUIV='refresh' CONTENT='30; URL=/'>");
  toSend.replace("_SCALE_","1");
  toSend.replace("_LOCATION_", location_name + " Home Page");
  if (display_Fahrenheit)
    toSend.replace("_TEMPUNITS_", "F");
  else
    toSend.replace("_TEMPUNITS_", "C");
  toSend.replace("_POWER_", createOptionSelector("POWER", power, 2, hp.getPowerSetting()));
  toSend.replace("_MODE_", createOptionSelector("MODE", mode_hp, 5, hp.getModeSetting()));
  if (display_Fahrenheit){
    String temp[16] = {"87.8", "86.0", "84.2", "82.4", "80.6", "78.8", "77.0", "75.2", "73.4", "71.6", "69.8", "68.0", "66.2", "64.4", "62.6", "60.8"};
    toSend.replace("_TEMP_", createOptionSelector("TEMP", temp, 16, String(hp.getTemperature()*1.8 + 32,1)));
  }
  else{
    String temp[16] = {"31", "30", "29", "28", "27", "26", "25", "24", "23", "22", "21", "20", "19", "18", "17", "16"};
    toSend.replace("_TEMP_", createOptionSelector("TEMP", temp, 16, String(hp.getTemperature())));
  }
  toSend.replace("_FAN_", createOptionSelector("FAN", fan, 6, hp.getFanSpeed()));
  toSend.replace("_VANE_", createOptionSelector("VANE", vane, 7, hp.getVaneSetting()));
  toSend.replace("_WVANE_", createOptionSelector("WIDEVANE", widevane, 7, hp.getWideVaneSetting()));
  if (display_Fahrenheit)
    rtemp = String((1.8*hp.getRoomTemperature() + 32),1);
  else
    rtemp = String(hp.getRoomTemperature(),1);
  toSend.replace("_ROOMTEMP_", rtemp);
  server.send(200, "text/html", toSend);
}

void handle_change_states() {
  String s;
  hp.setPowerSetting(server.arg("POWER"));
  hp.setModeSetting(server.arg("MODE"));
  if (display_Fahrenheit)
     hp.setTemperature((server.arg("TEMP").toFloat()-32)/1.8);
  else
     hp.setTemperature(server.arg("TEMP").toInt());
  hp.setFanSpeed(server.arg("FAN"));
  hp.setVaneSetting(server.arg("VANE"));
  hp.setWideVaneSetting(server.arg("WIDEVANE"));
  hp.update();
  s += "<HTML><head><meta name='viewport' content='width=device-width, initial-scale=1.5'><meta http-equiv='refresh' content='3; url=/'/><FORM ACTION='/'>"; 
  s += "<P><FONT SIZE=4><FONT FACE='Arial'>Success!</P>";
  s += "<P><INPUT TYPE=submit VALUE='     OK     '></FONT></P>";
  s += "</FORM>";
  s += "</HTML>";
  s += "\r\n\r\n";
  server.send(200, "text/html", s);
}

