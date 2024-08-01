// ==================================================================================================================  //
//                                                  Installing Libraries                                               //
// ==================================================================================================================  //

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DallasTemperature.h>
#include <ESP32Time.h>
#include <OneWire.h>
#include <Preferences.h>
#include "time.h"
#include <U8g2lib.h>


// ==================================================================================================================  //
//                                                    Setting Variables                                                //
// ==================================================================================================================  //

Preferences preferences;
int packetCounter;

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool reachedTemp;
int pasteurizingStartTime = 0;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

ESP32Time rtc(3600);  // offset in seconds GMT+1


// ==================================================================================================================  //
//                                                    Setting Pin Outs                                                 //
// ==================================================================================================================  //

// LED Lights
#define redLed 4
#define orangeLed 6
#define greenLed 5

// Motor
#define motorRight 9 
#define motorLeft 8 
#define motorCycle 18

const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;

// LED 
#define I2C_SDA 0
#define I2C_SCL 1
#define OLED_RESET -1 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ OLED_RESET, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA);

// Thermometer
const int oneWireBus = 3;  
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Bluetooth
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

// Bluetooth Icon
const unsigned char epd_bitmap_bluetooth [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xf8, 0xfc, 0xfe, 0xfe, 0x00, 0x9e, 0xcc, 0xe0, 0xf1, 0xfb, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xf1, 0x63, 0x07, 0x07, 0x00, 0x07, 
	0x63, 0xf0, 0xf8, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
	0xf0, 0xf0, 0x00, 0x90, 0x30, 0x70, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0
};

const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
    epd_bitmap_bluetooth
};


// ==================================================================================================================  //
//                                           Functions for Internal Storage                                            //
// ==================================================================================================================  //

void storeDataPacket(const String& dataPacket) {
    preferences.begin("my-app", false);
    String key = "dataPacket" + String(packetCounter);
    preferences.putString(key.c_str(), dataPacket);  // Convert String to const char*
    packetCounter++;
    preferences.putInt("packetCounter", packetCounter); // Store the counter value
    preferences.end();
}

void printStoredData() {
    preferences.begin("my-app", true);
    packetCounter = preferences.getInt("packetCounter", 0); // Retrieve the counter value

    if (packetCounter > 0) {
        Serial.println("Stored data:");
        for (int i = 0; i < packetCounter; ++i) {
            String key = "dataPacket" + String(i);
            String value = preferences.getString(key.c_str(), "N/A");  // Convert String to const char*
            Serial.print("Key: ");
            Serial.print(key);
            Serial.print(", Value: ");
            Serial.println(value);
        }
    } else {
        Serial.println("No data stored.");
    }
    preferences.end();
}

void clearStoredData() {
    preferences.begin("my-app", false);
    preferences.clear(); // Erase all keys and values in the current namespace
    packetCounter = 0;
    Serial.println("All stored data has been cleared.");
    preferences.end();
}


// ==================================================================================================================  //
//                                                        Setup                                                        //
// ==================================================================================================================  //

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    preferences.begin("my-app", false);
    packetCounter = preferences.getInt("packetCounter", 0);
    sensors.begin();

    pinMode(redLed, OUTPUT);
    pinMode(greenLed, OUTPUT);
    pinMode(orangeLed, OUTPUT);

    pinMode(motorRight, OUTPUT);
    pinMode(motorLeft, OUTPUT);
    pinMode(motorCycle, OUTPUT);

    reachedTemp = false;

    // Setting up Bluetooth 
    BLEDevice::init("ESP32 BLE Server");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
    Serial.println("Waiting for connections...");  

    // Store Initial Data
    sensors.requestTemperatures();
    int temperatureC = (int)sensors.getTempCByIndex(0);
    String timeString = rtc.getTime();
    String hourMinute = timeString.substring(0, 5);
    int date = rtc.getDay();
    int month = rtc.getMonth() + 1;
    int year = rtc.getYear();
    String dataPacket = String(temperatureC) + "," + hourMinute + "," + date + "," + month + "," + year;
    storeDataPacket(dataPacket);
}


// ==================================================================================================================  //
//                                                         Loop                                                        //
// ==================================================================================================================  //

void loop() {
    // Read temperature
    sensors.requestTemperatures();
    int temperatureC = (int)sensors.getTempCByIndex(0);
    String temperatureString = String(temperatureC);
    const char *temperatureChar = temperatureString.c_str();
    String tempString = String(temperatureChar) + "°C";

    // Display temperature on the screen
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB10_tr);
    u8g2.drawStr(0,14,"Temp");
    u8g2.setFont(u8g2_font_helvB24_tr);
    u8g2.drawStr(0,48, tempString.c_str());
    u8g2.drawBitmap(64, 0, 20/8, 20, epd_bitmap_bluetooth);
    u8g2.sendBuffer();

    // Get date and time
    String timeString = rtc.getTime(); // Assuming rtc is initialized somewhere in your code
    String hourMinute = timeString.substring(0, 5);
   // String dateString = rtc.getDate();
    int date = rtc.getDay();
    int month = rtc.getMonth() + 1;
    int year = rtc.getYear();
    

    //Setting motor speed and direction
    const int freq = 30000;
    const int pwmChannel = 0;
    const int resolution = 8;
    const int dutyCycle  = 255;

    digitalWrite(motorRight, LOW);
    digitalWrite(motorLeft, HIGH);
    ledcSetup(pwmChannel, freq, resolution); //Sets up a PWM channel
    ledcAttachPin(motorCycle, pwmChannel); // Assigns PWM channel to the correct pin
    ledcWrite(pwmChannel, dutyCycle); 

  

    // LED control based on temperature
    if (temperatureC < 72)
    {
      digitalWrite(redLed, HIGH);    
      digitalWrite(orangeLed, LOW);
      digitalWrite(greenLed, LOW);
      Serial.println("Unpasteurized"); 
    }
    else if (temperatureC == 72)
    {
      unsigned long currentTime = millis();
      if (!reachedTemp) {
        reachedTemp = true;
        pasteurizingStartTime = currentTime;
      }

      if (currentTime - pasteurizingStartTime < 15000) {
        digitalWrite(redLed, LOW);
        digitalWrite(orangeLed, HIGH); // for sure RED
        digitalWrite(greenLed, LOW);    // Green
        Serial.println("Pasteurizing"); 
      } 
    }
    else if (temperatureC > 72 && temperatureC < 80)
    {
      digitalWrite(redLed, LOW); // green
      digitalWrite(orangeLed, LOW);  //red
      digitalWrite(greenLed, HIGH);    // for sure orange
      Serial.println("Pasteurized"); 

      // Create Data Packet
    String dataPacket = String(temperatureC) + "," + hourMinute + "," + date + "," + month + "," + year;

    if (deviceConnected) {
    
      if (packetCounter > 0) {
        preferences.begin("my-app", true);
        packetCounter = preferences.getInt("packetCounter", 0); // Retrieve the counter value
          for (int i = 0; i < packetCounter; ++i) {
              String key = "dataPacket" + String(i);
              String value = preferences.getString(key.c_str(), "N/A");  // Convert String to const char*

              pCharacteristic->setValue((uint8_t*)value.c_str(), value.length());
              pCharacteristic->notify();
          }
          preferences.end();
          clearStoredData();
      } 

      pCharacteristic->setValue((uint8_t*)dataPacket.c_str(), dataPacket.length());
      pCharacteristic->notify();
    } else {
      storeDataPacket(dataPacket);
    }
    }
    else
    {
      digitalWrite(redLed, HIGH);    // Red
      digitalWrite(orangeLed, LOW);
      digitalWrite(greenLed, LOW);
      Serial.println("Over Boiled"); 

      // Create Data Packet
    String dataPacket = String(temperatureC) + "," + hourMinute + "," + date + "," + month + "," + year;

    if (deviceConnected) {
    
      if (packetCounter > 0) {
        preferences.begin("my-app", true);
        packetCounter = preferences.getInt("packetCounter", 0); // Retrieve the counter value
          for (int i = 0; i < packetCounter; ++i) {
              String key = "dataPacket" + String(i);
              String value = preferences.getString(key.c_str(), "N/A");  // Convert String to const char*

              pCharacteristic->setValue((uint8_t*)value.c_str(), value.length());
              pCharacteristic->notify();
          }
          preferences.end();
          clearStoredData();
      } 

      pCharacteristic->setValue((uint8_t*)dataPacket.c_str(), dataPacket.length());
      pCharacteristic->notify();
    } else {
      storeDataPacket(dataPacket);
    }
    }

    // Print all stored data
    printStoredData();
    preferences.end(); // Close the preferences storage

}
