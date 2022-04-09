#include <SPI.h>
#include <WiFiNINA.h>
#include<Wire.h>
#include <LiquidCrystal.h>
#include <Ds1302.h>
#include <ArduinoJson.h>

#include "secrets.h"

#define PIN_ENA 16
#define PIN_CLK 15
#define PIN_DAT 14

#define PIN_HR_LED 9
#define PIN_HR_SENSOR A3

#define BUTTON 8

// DS1302 RTC instance
Ds1302 rtc_module(PIN_ENA, PIN_CLK, PIN_DAT);

LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

const static char* WeekDays[] =
{
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    "Sun"
};

// WIFI VARIABLES
const char ssid[] = AP_SSID;
const char password[] = AP_PASSWORD;
int keyIndex = 0;
int status = WL_IDLE_STATUS;
WiFiClient client;
IPAddress server(192,168,45,190);
//char server[] = "http://192.168.45.190";

// RTC VARIABLES
unsigned long last_rtc = 0;
unsigned long rtc_interval = 500;

// HR VARIABLES
double alpha = 0.75;
int period = 20;
double change = 0.0;
unsigned long last_peak_time = 0;
unsigned long peak_delay = 300;
float threshold = 4.0f;
uint16_t intervals[10];
uint8_t i = 0;
float hr = 0.0f;
unsigned long last_ppg = 0;
unsigned long ppg_interval = 20;

// MPU6050 VARIABLES
const int MPU_addr=0x69;
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
int angleChange=0;
float ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
boolean fall = false; //stores if a fall has occurred
boolean trigger1=false; //stores if first trigger (lower threshold) has occurred
boolean trigger2=false; //stores if second trigger (upper threshold) has occurred
boolean trigger3=false; //stores if third trigger (orientation change) has occurred
boolean alarm = false;
byte trigger1count=0; //stores the counts past since trigger 1 was set true
byte trigger2count=0; //stores the counts past since trigger 2 was set true
byte trigger3count=0; //stores the counts past since trigger 3 was set true

unsigned long last_mpu = 0, mpu_interval = 100;
unsigned long lastConnectionTime = 0; 
const unsigned long postingInterval = 5L * 1000L;

String time_string = "", hour_string = "";

void setup()
{
    Serial.begin(9600);
    lcd.begin(16, 2);
    server_init();
    rtc_init();
    mpu_init();

    pinMode(PIN_HR_LED, OUTPUT);
    pinMode(BUTTON, INPUT);
}

void loop()
{
  Ds1302::DateTime now;
  rtc_module.getDateTime(&now);
    if(millis() - last_rtc > rtc_interval) {
        static uint8_t last_second = 0;
        if (last_second != now.second)
        {
            last_second = now.second;
    
            time_string = rtc_get_date(now);
            hour_string = rtc_get_hour(now);
    
            lcd.setCursor(0, 0);
            lcd.print(time_string);
            lcd.setCursor(0, 1);
            lcd.print(hour_string);
        }
        last_rtc = millis();
    }

    if(millis() - last_ppg > ppg_interval) {
        measure_ppg();
        last_ppg = millis();
    }

    if(millis() - last_mpu > mpu_interval) {
      measure_mpu();
      last_mpu = millis();
    }
//  while (client.available()) {
//    char c = client.read();
//    Serial.write(c);
//  }
  if (millis() - lastConnectionTime > postingInterval) {
    httpRequest();
  } 

  if (digitalRead(BUTTON) == HIGH) {
    Serial.print("p");
  }
}

// ---------------------- WIFI --------------------------/

void server_init() {
  if (WiFi.status() == WL_NO_MODULE) {
//    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
//    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
//    Serial.print("Attempting to connect to SSID: ");
//    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, password);

    // wait 1 second for connection:
    delay(5000);
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
//  Serial.print("SSID: ");
//  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
//  Serial.print("IP Address: ");
//  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
//  Serial.print("signal strength (RSSI):");
//  Serial.print(rssi);
//  Serial.println(" dBm");
}

void httpRequest() {
  client.stop();
  if(client.connect(server, 8080)) {
    DynamicJsonDocument doc(1024);
    doc["hr"] = hr;
    doc["time"] = time_string + " " + hour_string;
    String json = "";
    serializeJson(doc, json);
//    Serial.println(json);
    client.println("POST /data HTTP/1.1");
    
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(json.length()));
    client.println();
    
    client.println(json);

    lastConnectionTime = millis();
  } else {
    // if you couldn't make a connection:
//    Serial.println(F("Connection failed"));
  }
}

// ---------------------- RTC --------------------------/

void rtc_init() {
    // initialize the RTC
    rtc_module.init();

    // test if clock is halted and set a date-time (see example 2) to start it
//    if (rtc_module.isHalted()) {

        Ds1302::DateTime dt = {
            .year = 22,
            .month = Ds1302::MONTH_APR,
            .day = 9,
            .hour = 15,
            .minute = 15,
            .second = 20,
            .dow = Ds1302::DOW_SAT
        };

        rtc_module.setDateTime(&dt);
//    }
}

String rtc_get_date(Ds1302::DateTime now) {
    String time_string = "";
  
    time_string += "20";
    if (now.year < 10) {
      time_string += "0";
    }
    time_string += now.year;
    time_string += '-';
    if (now.month < 10) {
      time_string += "0";
    }
    time_string += now.month;
    time_string += '-';
    if (now.day < 10) {
      time_string += "0";
    }
    time_string += now.day;
    time_string += ' ';
    time_string += WeekDays[now.dow - 1];

    return time_string;
}

String rtc_get_hour(Ds1302::DateTime now) {

    String hour_string = "";
  
    if (now.hour < 10) {
      hour_string += '0';
    }
    hour_string += now.hour;
    hour_string += ':';
    if (now.minute < 10) {
      hour_string += '0';
    }
    hour_string += now.minute;
    hour_string += ':';
    if (now.second < 10) {
      hour_string += '0';
    }
    hour_string += now.second;

    return hour_string;
}

// -------------------- PPG -------------------------/

void measure_ppg() {
    static double oldValue = 0;
    static double oldChange = 0;
    int rawValue = analogRead(PIN_HR_SENSOR);
    double value = alpha * oldValue + (1 - alpha) * rawValue;

    if(value > threshold) {
        unsigned long actual_time = millis();
        if (actual_time - last_peak_time > peak_delay) {
            intervals[i] = actual_time - last_peak_time;
//            Serial.println(intervals[i]);
            last_peak_time = actual_time;
            i++;
            if (i >= 10) {
                hr = 0.0;
                for (int j = 0; j < 10; j++) {
                  hr += intervals[j];
                }
                hr /= 10;
                hr = 1/hr*60*1000;
                lcd.print(hr);
                i = 0;
            }
        }
    }
//    Serial.println(value);
    digitalWrite(PIN_HR_LED,(change<0.0&&oldChange>0.0));
    oldValue = value;
    oldChange = change;
}

// -------------------- MPU6050 -------------------------/

void mpu_init() {
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

void measure_mpu() {
  mpu_read();
  ax = (AcX-2050)/16384.00;
  ay = (AcY-77)/16384.00;
  az = (AcZ-1947)/16384.00;

  gx = (GyX+270)/131.07;
  gy = (GyY-351)/131.07;
  gz = (GyZ+136)/131.07;

  float Raw_AM = pow(pow(ax,2)+pow(ay,2)+pow(az,2),0.5);
  int AM = Raw_AM * 10;
  
  //Serial.println(AM);
  if (trigger3==true)
  {
     trigger3count++;
     if (trigger3count>=15)
     { 
        angleChange = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5);
        if ((angleChange>=0) && (angleChange<=15))
        {
          fall=true; trigger3=false; trigger3count=0;
        }
        else
        {
           trigger3=false; trigger3count=0;
        }
      }
   }
  if (fall==true)
  {
    alarm = true;
    if(alarm == true)
    {
//      Serial.println("ALARM ALARM t3");
    }
    alarm = false;
    fall=false;
  }
  if (trigger2==true)
  {
    trigger2count++;
    angleChange = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5); //Serial.println(angleChange);
    if (angleChange>=30 && angleChange<=400)
    {
      trigger3=true; trigger2=false; trigger2count=0;
      alarm = true;
       if(alarm == true)
      {
//        Serial.println("ALARM ALARM t2");
      }
      alarm = false; 
    }
  }
  if (trigger1==true)
  {
    trigger1count++;
    if (AM>=12)
    {
      trigger2=true;
      trigger1=false; trigger1count=0;
      alarm = true;
      if(alarm == true)
      {
//        Serial.println("ALARM ALARM t1");
      }
      alarm = false;
    }
  }
  if (AM<=2 && trigger2==false)
  {
    trigger1=true;
    alarm = true;
    if(alarm == true)
      {
//        Serial.println("ALARM ALARM unknown");
      }
    alarm = false;
  }
}

void mpu_read(){
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}
