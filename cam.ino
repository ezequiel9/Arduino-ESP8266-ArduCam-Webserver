// ArduCAM demo (C)2017 Lee
// Web: http://www.ArduCAM.com
// This program is a demo of how to use most of the functions
// of the library with a supported camera modules, and can run on any Arduino platform.
//
// This demo was made for Omnivision 2MP/5MP sensor.
// It will run the ArduCAM 2MP/5MP as a real 2MP/5MP digital camera, provide both JPEG capture.
// The demo sketch will do the following tasks:
// 1. Set the sensor to JPEG mode.
// 2. Capture and buffer the image to FIFO every 5 seconds
// 3. Store the image to Micro SD/TF card with JPEG format in sequential.
// 4. Resolution can be changed by myCAM.set_JPEG_size() function.
// This program requires the ArduCAM V4.0.0 (or later) library and ArduCAM 2MP/5MP shield
// and use Arduino IDE 1.6.8 compiler or above

#include <ESP8266WiFi.h>

#ifndef STASSID
#define STASSID "Publica 2.4G"
#define STAPSK  "ilovehelveticamedium"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

const char* host = "nodes.ezequielfernandez.com";
const uint16_t port = 80;


#include <ArduCAM.h>
#include <Wire.h>
#include <SPI.h>
//#include <SD.h>
#include "memorysaver.h"
//This demo can only work on OV2640_MINI_2MP or OV5642_MINI_5MP or OV5642_MINI_5MP_BIT_ROTATION_FIXED platform.

const int SPI_CS = D0;

ArduCAM myCAM( OV5642, SPI_CS );

void setup() {


  pinMode(D3, OUTPUT);
  pinMode(D8, INPUT_PULLUP);
  digitalWrite(D3, HIGH);

  uint8_t vid, pid;
  uint8_t temp;
  Wire.begin();
  Serial.begin(115200);
  Serial.println(F("ArduCAM Start!"));
  myCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK);

  //set the CS as an output:
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_CS, HIGH);
  // initialize SPI:
  SPI.begin();

  //Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);

  while (1) {
    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    Serial.println(temp, HEX);
    if (temp != 0x55) {
      Serial.println(F("SPI interface Error!"));
      delay(1000); continue;
    } else {
      Serial.println(F("SPI interface OK.")); break;
    }
  }



  while (1) {
    myCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK);

    //Check if the camera module type is OV5642
    myCAM.wrSensorReg16_8(0xff, 0x01);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    Serial.println(vid, HEX);
    Serial.println(pid, HEX);
    if ((vid != 0x56) || (pid != 0x42)) {
      Serial.println(F("Can't find OV5642 module!"));
      delay(1000); continue;
    }
    else {
      Serial.println(F("OV5642 detected.")); break;
    }

break;
  }
  myCAM.set_format(JPEG);
  myCAM.InitCAM();

  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
  myCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK);

  //Connect to Wifi
  Serial.println("Connecting to Wifi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //  delay(1000);
}


void sendPhotoToServer() {
  myCAM.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK);
  yield();
  delay(1000);
  yield();

  WiFiClient client;

  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    yield();
    delay(500);

  } else {
    Serial.println("Connected to server");
  }

  int buffSize = 1024;
  char str[8];
  byte buf[buffSize];
  static int i = 0;
  static int k = 0;
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  bool is_header = false;
  //Flush the FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();
  Serial.println(F("start Capture"));
  while (!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK));
  myCAM.set_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK);

  Serial.println(F("Capture Done."));
  yield();
  length = myCAM.read_fifo_length();
  Serial.print(F("The fifo length is :"));
  Serial.println(length, DEC);
  if (length >= MAX_FIFO_SIZE) //384K
  {
    Serial.println(F("Over size."));
    return ;
  }
  if (length == 0 ) //0 kb
  {
    Serial.println(F("Size is 0."));
    return ;
  }

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    //Read JPEG data from FIFO
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
      buf[i++] = temp;  //save the last  0XD9
      //Write the remain bytes in the buffer
      myCAM.CS_HIGH();

      yield();
      client.write(buf, i);

      while (client.available()) {
        yield();
        char c = client.read();
        Serial.print(c);
      }
      //      client.stop();


      Serial.println(F("Image save OK."));
      is_header = false;
      i = 0;
    }
    if (is_header == true)
    {
      //Write image data to buffer if not full
      if (i < buffSize) {
        buf[i++] = temp;
      }
      else
      {
        myCAM.CS_HIGH();

        yield();
        client.write(buf, buffSize);

        i = 0;
        buf[i++] = temp;
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
      }
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      buf[i++] = temp_last;
      buf[i++] = temp;

      if (client.connected()) {
        Serial.println("Connected");
        client.println("POST /upload.php HTTP/1.1");
        client.println("Host: nodes.ezequielfernandez.com");
        client.println("Accept: text/plain");
        //        client.println("Connection: keep-alive");
        client.println("Content-Type: application/jpeg");
        client.print("Content-Length: ");
        client.println(length) ;
        client.println("");
        Serial.println(length);
      } else {
        Serial.println("Not Connected");
      }
    }
  }
}

int ticks = 0;
int foo = 0;

unsigned long lastTrigger = 0;
void loop() {

  if(foo == 0){
    foo = 1;
    sendPhotoToServer();
  }

//  if (digitalRead(D8) == HIGH) {
//    if (millis() - lastTrigger >= 1000) {
//      digitalWrite(D3, LOW);
//      lastTrigger = millis();
//      sendPhotoToServer();
//
//    }
//    digitalWrite(D3, HIGH); 
//  }
}
