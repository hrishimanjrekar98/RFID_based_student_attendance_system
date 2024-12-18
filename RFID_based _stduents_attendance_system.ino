

#include <WiFi.h>
#include <HTTPClient.h>
// #include <Arduino_JSON.h>
#include <ArduinoJson.h>
#include <assert.h>
#include "time.h"
#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED true

typedef struct 
{
  char ssid[25];
  char pass[25];
  char url[50];
}config;
config siwi_config;

// char ssid[50]     = "SiWi_TPL_2G";
// char password[50] = "@Chaukas9833916981";
const char* ntpServer = "time.google.com";
char param_buff[5][50] = {0};

unsigned long epochTime; 
char imei[30];
uint64_t chipid;
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
String uart_buff;
boolean newData = false;
uint8_t set_flg = 0;
uint8_t wifi_cntr = 0;

//Your Domain name with URL path or IP address with path
//const char* serverName = "http://chaukas.in/siwi/index.php";

void readFile(fs::FS &fs, const char * path){

  memset(&siwi_config, 0, sizeof(siwi_config));
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
      Serial.println("- failed to open file for reading");
      return;
  }

  Serial.println("- read from file:");
  // while(file.available()){

    //     //Serial.write(file.read());
    // }
  file.readBytes((char*)&siwi_config, sizeof(siwi_config));
  file.close();

  Serial.print("SSID:"), 
  Serial.println(siwi_config.ssid);
  Serial.print("PASS:"); 
  Serial.println(siwi_config.pass);
  Serial.print("URL:"); 
  Serial.println(siwi_config.url);
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.write((uint8_t*)message, sizeof(siwi_config))){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

// Initialize WiFi
void initWiFi() {
  if((strlen(siwi_config.ssid) > 0) || (strlen(siwi_config.pass) > 0))
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(siwi_config.ssid, siwi_config.pass);
    Serial.print("Connecting to WiFi ..");
    while ((WiFi.status() != WL_CONNECTED) || (wifi_cntr > 60)) 
    {
      Serial.print('.');
      delay(1000);
      wifi_cntr++;
    }

    wifi_cntr = 0;

    if(WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi Connection error");
    }
    else
    {
      Serial.print("WiFi Connected to ");
      Serial.println(WiFi.localIP());
    }
  }
  else
  {
    Serial.println("SSID or Password not set");
  }
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void recv_uart_data() {
    static byte ndx = 0;
    char endMarker = '\r';
    char startMarker = 0x02;
    char rc;
    
    while (Serial1.available() > 0 && newData == false) {
        rc = Serial1.read();
        if (rc != endMarker) {
          if(set_flg)
          {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
          }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
            set_flg = 0;
        }

        if(rc == startMarker)
        {
          set_flg = 1;
        }
    }
}

void get_rfid_Data() 
{
  if (newData == true) 
  {
    Serial.print("RFID: ");
    Serial.println(receivedChars);
    newData = false;
    if(WiFi.status()== WL_CONNECTED)
    {
      WiFiClient client;
      HTTPClient http;
  
      // Your Domain name with URL path or IP address with path
      http.begin(client, siwi_config.url);

      //JSONVar myObject;
      StaticJsonDocument<200> myObject;

      epochTime = getTime();

      myObject["uid"] = imei;
      myObject["dt"] = epochTime;
      myObject["rfid"] = receivedChars;
      // If you need Node-RED/server authentication, insert user and password below
      //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

      String requestBody;
      serializeJson(myObject, requestBody);
    
      // Specify content-type header
      http.addHeader("Content-Type", "application/json");
      // Send HTTP POST request
      int httpResponseCode = http.POST(requestBody);
      Serial.print("data = ");
      Serial.println(requestBody);
    
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
  }
}

void External_cmd_parser(String cmd_data, char param[3][50])
{
  char *token = NULL;
  unsigned int param_count = 0;
  char char_data[50] = "\0";

  cmd_data.toCharArray(char_data, cmd_data.length()+1);
	token = strtok(char_data, "=,");
	while(token != NULL) 
  {
  	memset(param[param_count], 0x0, sizeof(&param[param_count]));
  	strcpy(param[param_count], token);
	  Serial.print("CMD_DATA:"); 
    Serial.println(param[param_count]);
  	param_count++;
  	token = strtok(NULL, "=,");
  }
  token = NULL;

  if(!strcmp(param[0], "WIFI"))
  {
    Serial.println("WIFI Command");
    memset(siwi_config.ssid, 0, sizeof(siwi_config.ssid));
    strcpy(siwi_config.ssid, param[1]);
    Serial.println(siwi_config.ssid);
    memset(siwi_config.pass, 0, sizeof(siwi_config.pass));
    strcpy(siwi_config.pass, param[2]);
    Serial.println(siwi_config.pass);

    writeFile(SPIFFS, "/siwi_config.txt", (char*)&siwi_config);
    delay(100);

    Serial.println("Restarting in 5 seconds");
 
    delay(5000);
    ESP.restart();
  }
  else if(!strcmp(param[0], "URL"))
  {
    Serial.println("Url Command");
    memset(siwi_config.url, 0, sizeof(siwi_config.url));
    strcpy(siwi_config.url, param[1]);
    Serial.println(siwi_config.url);

    writeFile(SPIFFS, "/siwi_config.txt", (char*)&siwi_config);
    delay(100);

    Serial.println("Restarting in 5 seconds");
 
    delay(5000);
    ESP.restart();

  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
        Serial.println("SPIFFS Mount Failed");
        return;
  }

  readFile(SPIFFS, "/siwi_config.txt");

  chipid = ESP.getEfuseMac();
  Serial.println(chipid);

  sprintf(imei, "%lld", chipid);
  Serial.println(imei);

  initWiFi();
  configTime(0, 0, ntpServer);

}

void loop() 
{
  //Send an HTTP POST request every 10 minutes
  recv_uart_data();
  get_rfid_Data();

  while(Serial.available()) 
  {
    uart_buff = Serial.readString();// read the incoming data as string
    Serial.println(uart_buff);

    External_cmd_parser(uart_buff, param_buff);
  }
}
