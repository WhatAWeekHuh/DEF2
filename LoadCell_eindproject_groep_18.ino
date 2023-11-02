/*
   -------------------------------------------------------------------------------------
   HX711_ADC
   Arduino library for HX711 24-Bit Analog-to-Digital Converter for Weight Scales
   Olav Kallhovd sept2017

   modified for DEF2 20-10-2023 by Damien Wolda
   Added LCD display. Accelerometer and time measuring between 2 weight levels that change
   in time. and some buttons.
   -------------------------------------------------------------------------------------
*/

#include <HX711_ADC.h> // Loadcell library
#include <Wire.h> // wire library - used for I2C communication
#include <LiquidCrystal_I2C.h> //lcd library
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

//pins:
const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin
#define BUTTON_PIN 6 //pin for the reset button. pin 6
#define BUTTON_PIN 7 //button pin 3
#define BUTTON_PIN 2 //button pin 2
#define BUTTON_PIN 1 //button pin 1


//liquid crystal
LiquidCrystal_I2C lcd(0x27, 16, 2); //lcd adress + 16 chars 2 lines.

//accelerometer
int ADXL345 = 0x53; //I2C adress accelecometer
float X_out, Y_out, Z_out; // outputs of accelerometer

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

//variables
const int calVal_eepromAdress = 0;
unsigned long t = 0;
unsigned long t1 = 0;
unsigned long t2 = 0;
unsigned long t3 = 0;
unsigned long t4 = 0; 
float roll,pitch,rollF,pitchF=0;
float actualmass = 0;
unsigned long scale = 0;
unsigned long measure = 0;
unsigned long eg = 0;


void setup() {
  Serial.begin(57600); delay(10);
  Serial.println();
  Serial.println("Starting...");
  
// LCD display setup :
  pinMode(BUTTON_PIN, INPUT);
  lcd.init();
  lcd.backlight();
  

// ===== accelerometer Off-set Calibration ========
  //X-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x1E);  // X-axis offset register
  Wire.write(1);
  Wire.endTransmission();
  delay(10);
  //Y-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x1F); // Y-axis offset register
  Wire.write(-2);
  Wire.endTransmission();
  delay(10);
  
  //Z-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x20); // Z-axis offset register
  Wire.write(-7);
  Wire.endTransmission();
  delay(10);
//accelerometer setup :

  Wire.begin(); // Begin wire library 
  // Set ADXL345 in measuring mode
  Wire.beginTransmission(ADXL345); // Start communicating with the device 
  Wire.write(0x2D); // Access/ talk to POWER_CTL Register - 0x2D
  // Enable measurement
  Wire.write(8); // (8dec -> 0000 1000 binary) Bit D3 High for measuring enable 
  Wire.endTransmission();

  
// Loadcell setup : 
  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 696.0; // uncomment this if you want to set the calibration value in the sketch
#if defined(ESP8266)|| defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
  EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity
  

   // ===== Read acceleromter data ====== //
    
  Wire.beginTransmission(ADXL345);
  Wire.write(0x32); // Start with register 0x32 (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345, 6, true); // Read 6 registers total, each axis value is stored in 2 registers
  X_out = ( Wire.read()| Wire.read() << 8); // X-axis value
  X_out = X_out/256; //For a range of +-2g, we need to divide the raw values by 256, according to the datasheet
  Y_out = ( Wire.read()| Wire.read() << 8); // Y-axis value
  Y_out = Y_out/256;
  Z_out = ( Wire.read()| Wire.read() << 8); // Z-axis value
  Z_out = Z_out/256;

  //Serial.print("Xa= ");
  //Serial.print(X_out);
  //Serial.print("   Ya= ");
  ///Serial.print(Y_out);
  //Serial.print("   Za= ");
  ////Serial.println(Z_out);
  //delay(500);
  
  // Calculate Roll and Pitch (rotation around X-axis, rotation around Y-axis)
  roll = atan(Y_out / sqrt(pow(X_out, 2) + pow(Z_out, 2)));
  pitch = atan(-1 * X_out / sqrt(pow(Y_out, 2) + pow(Z_out, 2)));

// ===== calculating actual mass taking into acount the angle ===== //
  actualmass = LoadCell.getData() / cos(roll);
  //Serial.print("   roll=  ");
  //Serial.print(roll);
  Serial.println("Loadcell mass");
  Serial.print(LoadCell.getData());
  Serial.println("Actual mass = ");
  Serial.print(actualmass);

  // =======  check for new data/start next conversion for loadcell: ===== //
  if (LoadCell.update()) newDataReady = true;

  if (scale == 0 && measure == 0){
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Starting up...");
    delay(1000);
    lcd.clear();
    measure = 1;
  }
    
    

    // =====  Button to reset the values and restart measuring and turning off scale mode ====== // 
  if (digitalRead(6) == HIGH){ // button to reset all things ;)
    t1 = 0;
    t2 = 0;
    t3 = 0;
    t4 = 0;
    scale = 0;
    measure = 1;
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Reset button");
    lcd.setCursor(4,1);
    lcd.print("Pressed!");
    Serial.print("Reset button pressed");
    delay(1000);
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("initialising..");
    delay(1500);
    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("Ready to");
    lcd.setCursor(7,1);
    lcd.print("GO!");
  }


//  =======setting mode to scale and turning off measuring ========//
  if (digitalRead(7) == HIGH){
    scale = 1;
    measure = 0;
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Scale mode");
    lcd.setCursor(2,1);
    lcd.print("Initiating..");
    delay(1000);
    lcd.clear();
    }
  
  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval && scale == 1) { // only print the gram value if scale mode is turned on (works only with the accelerometer. To use only loadcell print i instead). //
      float i =LoadCell.getData();
      //Serial.print("Load_cell output val: ");
      //Serial.println(i);
      //lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("Weight is:");
      lcd.setCursor(3,1);
      lcd.print(actualmass);  //print either i for only loadcell or actualmass for loadcell + accelerometer
      lcd.print(" gram");
      delay(100); 
      lcd.clear();
      
      }


      // =====  Loadcell + measuring the time and printing it on the lcd monitor ==== //

   if (measure == 1){   // only execute the following code when the mode is measuring mode //
    
      if (t4 == 0){ //Only execute this when the time for drinking hasnt been measured yet.
        if (t1 == 0 && t2 == 0){
        lcd.setCursor(4,0); //Message displayed on lcd screen
        lcd.print("Ready to");
        lcd.setCursor(7,1);
        lcd.print("GO!");
        }

      if (actualmass >= 620) { //Check for when the bottle is full and on the loadcell.
        t1 = millis(); //time for bottle to start getting empty
        lcd.clear();
        lcd.setCursor(3,0);
        lcd.print("Measuring..");
        lcd.setCursor(4,1);
        lcd.print("Drink!!");
        //Serial.print(t1); //print the time when the bottle starts to get empty
      }
      if (actualmass <= 85 && t1 != 0 && t4 == 0) { //If the bottle was full and the bottle is now empty. check the time
        t2 = millis(); //time for bottle when empty
        //Serial.print(t2); //print the time when bottle is empty
      }
      if (t2 != 0 && t4 == 0){ // If the empty bottle measurement after drinking is done. Execute the following
        t3 = (t2-t1); // checking the time between drinking
      //Serial.print("Time to drink is:");
      //Serial.println(t3);
      //Serial.println("milliseconds");  //printing
      }
      }
      if (t3 != 0){ // If the measured drinking time isnt 0, delay and shut off the program or print on the lcd monitor. 
        lcd.clear();
        lcd.setCursor(3,0);
        lcd.print("Calculating..");
        delay(1000);
        t4 = t3;
        t3 = 0;
      // Here should be line fore the lcd monitor printing :)
      }
      if(t4 != 0){
        Serial.println("Time is:");
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Time is:");
        lcd.setCursor(2,1);
        lcd.print(t4 /1000.0,3);
        lcd.setCursor(8,1);
        lcd.print("Seconds");
        Serial.println(t4/ 1000.0 , 3);
        Serial.print("Seconds"); //printing
        // there should be a line here for the lcd monitor
      }
      newDataReady = 0;
      t = millis();
    }
  }
    
    
  // receive command from serial terminal, send 't' to initiate tare operation:

  // ==== button 2 tare's the scale === //
    if (digitalRead(2) == HIGH){
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("Tarring.....");
      delay(500);
      LoadCell.tareNoDelay();
      lcd.clear();
    }
 

  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

}
