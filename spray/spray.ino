  //Libraries
  #include <DallasTemperature.h>
  #include <OneWire.h>
  #include <PinChangeInt.h>
  #include <LiquidCrystal.h>
  #include <limits.h>
  
  //Arduino pins
  #define oneWireBus 2
  #define motorPin 3
  #define buttonPin 7
  #define triggerPin 11
  #define echoPin 12
  
  //Variables
  int distance, temperature, uses_left;
  enum Status { Kakken, Pissen, Schoonmaken, Idle };
  Status wc_status;
  bool spray_needed, door_open;
  float plastimer, poeptimer;
  float temperatureDelay;
  unsigned long currentMillis;
  unsigned long previousMillis;
  //LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
  
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors(&oneWire);
  
  void interruptFunction() 
  {
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
    
    //Define inputs and outputs for distance sensor
    pinMode(triggerPin, OUTPUT);
    pinMode(echoPin, INPUT);
    
    //Start up the temperature library
    sensors.begin();
    
    attachPinChangeInterrupt(buttonPin, interruptFunction, FALLING);
  }
  
  void loop() 
  {
    currentMillis = millis();
    unsigned long timePassed = currentMillis - previousMillis;
    
    UpdateDistance();
    UpdateTemperature(timePassed);
    UpdateLCD();
    
    if(!door_open) {
      if(distance >= 0 && distance <= 60) 
      {
        spray_needed = true;
        if(distance >= 30) 
        {
          plastimer += (currentMillis - previousMillis);
        } 
        else 
        {
          poeptimer += (currentMillis - previousMillis);  
        }
      } 
    }
    
    if(door_open && spray_needed) 
    {  
      GetStatus(plastimer, poeptimer);
      Spray();
    }  
    
    previousMillis = millis();
  }
  
  void UpdateDistance()
  {
    digitalWrite(triggerPin, LOW);
    delayMicroseconds(5);
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(triggerPin, LOW);
  
    pinMode(echoPin, INPUT);
    float duration = pulseIn(echoPin, HIGH);
  
    distance = (duration/2) / 29.1;
  }
  
  void UpdateTemperature(unsigned long timePassed)
  {
    if(temperatureDelay < 2000)
    {
      temperatureDelay += timePassed;
    }
    else //Only update temperature every 2 seconds
    {
      sensors.requestTemperatures();
      temperature = sensors.getTempCByIndex(0);
      temperatureDelay = 0;
    }
  }
  
  void UpdateLCD()
  {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C"); // 10 chars used
    
    // Debug
    lcd.setCursor(0, 1);
    lcd.print("Dist: ");
    lcd.print(distance);
    lcd.print(" cm");
  }
  
  void GetStatus(long plastimer, long poeptimer) 
  {
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
