
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Time.h>
#include "time.h"

#include <U8g2lib.h>
#include <Wire.h>


bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

//ESP32Time rtc;
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

// LED Screen information
#define I2C_SDA 0
#define I2C_SCL 1
#define OLED_RESET -1 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ OLED_RESET, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA);
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// 'bluetooth', 20x20px
const unsigned char epd_bitmap_bluetooth [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xf8, 0xfc, 0xfe, 0xfe, 0x00, 0x9e, 0xcc, 0xe0, 0xf1, 0xfb, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xf1, 0x63, 0x07, 0x07, 0x00, 0x07, 
	0x63, 0xf0, 0xf8, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
	0xf0, 0xf0, 0x00, 0x90, 0x30, 0x70, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 80)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
    epd_bitmap_bluetooth
};


void setup() {
  Serial.begin(115200);
  u8g2.begin();

  reachedTemp = false;

  // Start the DS18B20 sensor
  sensors.begin();

  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);

  pinMode(motorRight, OUTPUT);
  pinMode(motorLeft, OUTPUT);
  pinMode(motorCycle, OUTPUT);
}

void loop() {

    // Read temperature
    sensors.requestTemperatures();
    int temperatureC = (int)sensors.getTempCByIndex(0);
    String temperatureString = String(temperatureC);
    const char *temperatureChar = temperatureString.c_str();

   // Update the display with temperature
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB10_tr);
    u8g2.drawStr(0,14,"Temp");
    u8g2.setFont(u8g2_font_helvB24_tr);
    u8g2.drawStr(0,48, temperatureChar);

    u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);  
    u8g2.drawGlyph(65, 0, 74);

   u8g2.drawBitmap(64, 0, 20/8, 20, epd_bitmap_bluetooth);


    u8g2.sendBuffer();


    // Get date and time
    String timeString = rtc.getTime(); // Assuming rtc is initialized somewhere in your code
    String hourMinute = timeString.substring(0, 5);
    String dateString = rtc.getDate();

  

    //Setting PWM properties for pin 18(motorCycle) 
    const int freq = 30000;
    const int pwmChannel = 0;
    const int resolution = 8;
    const int dutyCycle  = 255;

    String dataPacket = String(temperatureC) + "," + dateString + "," + timeString;

    digitalWrite(motorRight, HIGH);
    digitalWrite(motorLeft, LOW);

    ledcSetup(pwmChannel, freq, resolution); //Sets up a PWM channel
    ledcAttachPin(motorCycle, pwmChannel); // Assigns PWM channel to the correct pin
    ledcWrite(pwmChannel, dutyCycle); 




    
     if (deviceConnected) {
       // Convert data to bytes and send
   
     }

    // Log data
    Serial.print("Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");
    Serial.print("Date: ");
    Serial.println(dateString);
    Serial.print("Time: ");
    Serial.println(timeString);


    // LED control based on temperature
    if (temperatureC < 72)
    {
      digitalWrite(redLed, HIGH);    // Red
      digitalWrite(blueLed, LOW);
      digitalWrite(greenLed, LOW);
      Serial.println("Unpasteurized"); 
    }
    else if (temperatureC == 72)
    {
      reachedTemp = true;
      delay(15000); // Wait for 15 seconds
    }
    else if (temperatureC > 72 && temperatureC < 80)
    {
      digitalWrite(redLed, LOW);
      digitalWrite(blueLed, HIGH);
      digitalWrite(greenLed, LOW);    // Green
      Serial.println("Pasteurized"); 
    }
    else
    {
      digitalWrite(redLed, LOW);    // Red
      digitalWrite(blueLed, LOW);
      digitalWrite(greenLed, HIGH);
      Serial.println("Over Boiled"); 
    }

    // Delay
    delay(2000);
  
}

