  #include <PinChangeInt.h>
  #include <LiquidCrystal.h>
  #include <limits.h>
  
  #define buttonPin 7
  #define motorPin 3
  
  int distance;
  enum Status { Kakken, Pissen, Schoonmaken, Idle };
  Status wc_status;
  int temp;
  int uses_left;
  bool spray_needed;
  bool door_open;
  float plastimer;
  float poeptimer;
  unsigned long currentMillis;
  unsigned long previousMillis;
  //LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
  
  void interruptFunction() {
    // Activate motor pin
    //lcd.setCursor(0, 0);
    //lcd.print("Spraying...");
    Serial.println("Trying to spray...");
    int mosfet = digitalRead(2);
    Serial.println(mosfet);  
    digitalWrite(motorPin, HIGH);
  }
  
  void setup() {
    Serial.begin(9600);
    
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(motorPin, OUTPUT);
    
    attachPinChangeInterrupt(buttonPin, interruptFunction, FALLING);
  }
  
  void loop() {
    currentMillis = millis();
    //updateDistance();
    //updateTemperature();
    
    if(!door_open) {
      if(distance >= 0 && distance <= 60) {
        spray_needed = true;
        if(distance >= 30) {
          plastimer += (currentMillis - previousMillis);
        } else {
          poeptimer += (currentMillis - previousMillis);  
        }
      } 
    }
    
    if(door_open && spray_needed) {  
      getStatus(plastimer, poeptimer);
      Spray();
    }  
    
    previousMillis = millis();
  }
  
  void getStatus(long plastimer, long poeptimer) {
    if(plastimer > poeptimer)
    {
      wc_status = Pissen;
    }
    else
    {
      if(poeptimer >= 30)
        wc_status = Kakken;
      else 
        wc_status = Pissen;
    }    
  }
  
  void Spray() {
    spray_needed = false;
    
    switch(wc_status) {
       case Kakken:
         // Spray twice
       break;
       case Pissen:
         // Spray once
       break;
    } 
  }
  
  
  
  
  
  

