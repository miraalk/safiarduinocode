#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Time.h>
#include "time.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Preferences.h>
Preferences preferences;

// Bluetooth related definitions
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

ESP32Time rtc(3600);  // offset in seconds GMT+1

// Locations of hardware
const int oneWireBus = 3;   // GPIO for thermometer
#define blueLed 6
#define redLed 4
#define greenLed 5 
bool reachedTemp;

#define motorRight 9 
#define motorLeft 8 
#define motorCycle 18

const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
#define I2C_SDA 0
#define I2C_SCL 1
#define OLED_RESET -1 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ OLED_RESET, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA);

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

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

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  sensors.begin();

  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(motorRight, OUTPUT);
  pinMode(motorLeft, OUTPUT);
  pinMode(motorCycle, OUTPUT);

  preferences.begin("my-app", false); // Open the preferences storage

  // Testing storage
    int totalKeys = preferences.length();
     for (int i = 0; i < totalKeys; ++i) {
        String data = preferences.getString("dataPacket" + String(i), "");
        Serial.println("Stored Data " + String(i) + ": " + data);
     }

  // Setup BLE
  BLEDevice::init("ESP32 BLE Server");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();
}

void loop() {
  sensors.requestTemperatures();
  int temperatureC = (int)sensors.getTempCByIndex(0);
  String temperatureString = String(temperatureC);
  const char *temperatureChar = temperatureString.c_str();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.drawStr(0,14,"Temp");
  u8g2.setFont(u8g2_font_helvB24_tr);
  u8g2.drawStr(0,48, temperatureChar);
  u8g2.drawBitmap(64, 0, 20/8, 20, epd_bitmap_bluetooth);
  u8g2.sendBuffer();

  String timeString = rtc.getTime();
  String hourMinute = timeString.substring(0, 5);
  String dateString = rtc.getDate();
  String dataPacket = String(temperatureC) + "," + dateString + "," + timeString;

  digitalWrite(motorRight, HIGH);
  digitalWrite(motorLeft, LOW);

  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(motorCycle, pwmChannel);
  ledcWrite(pwmChannel, 255);

  if (deviceConnected) {
    pCharacteristic->setValue(dataPacket.c_str());
    pCharacteristic->notify();
  } else {
    // Store data in internal memory
    // This example stores the data in RTC memory
    Serial.print("NOT CONNECTED");
    preferences.putString("dataPacket", dataPacket);
    preferences.end(); // Close the preferences storage

  }

  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.println(" °C");
  Serial.print("Date: ");
  Serial.println(dateString);
  Serial.print("Time: ");
  Serial.println(timeString);

  if (temperatureC < 72) {
    digitalWrite(redLed, HIGH;
    digitalWrite(blueLed, HIGH);
    digitalWrite(greenLed, HIGH);
    Serial.println("Unpasteurized"); 
  } else if (temperatureC == 72) {
    reachedTemp = true;
    delay(15000); // Wait for 15 seconds
  } else if (temperatureC > 72 && temperatureC < 80) {
    digitalWrite(redLed, LOW);
    digitalWrite(blueLed, LOW);
    digitalWrite(greenLed, HIGH);
    Serial.println("Pasteurized"); 
  } else {
    digitalWrite(redLed, LOW);
    digitalWrite(blueLed, LOW);
    digitalWrite(greenLed, HIGH);
    Serial.println("Over Boiled"); 
  }

  delay(2000);
}
