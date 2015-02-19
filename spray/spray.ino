  //Libraries
  #include <DallasTemperature.h>
  #include <OneWire.h>
  #include <PinChangeInt.h>
  #include <LiquidCrystal.h>
  #include <limits.h>
  
  //Arduino Digital pins
  #define motorPin 7
  #define buttonPin A5
  #define triggerPin 9
  #define echoPin 10
  #define tempPin 8
  
  //Variables
  int distance, temperature, uses_left;
  enum Status { Kakken, Pissen, Schoonmaken, Idle };
  Status wc_status;
  bool spray_needed, door_open, manual_spray_needed;
  float plastimer, poeptimer;
  float temperatureDelay;
  unsigned long currentMillis;
  unsigned long previousMillis;
  
  LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
  OneWire oneWire(tempPin);
  DallasTemperature sensors(&oneWire);
  
  void interruptFunction() 
  {
    manual_spray_needed = true;
  }
  
  void setup() {
    Serial.begin(9600);
    lcd.begin(16, 2);
    
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(motorPin, OUTPUT);
    
    //Define inputs and outputs for distance sensor
    pinMode(triggerPin, OUTPUT);
    pinMode(echoPin, INPUT);
    
    //Start up the temperature library
    sensors.begin();
    
    attachPinChangeInterrupt(buttonPin, interruptFunction, FALLING);
  }
  
  /*
  *----------------------*
  * The Loop
  *----------------------*
  */
  void loop() 
  {
    currentMillis = micros();
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
      Spray(false);
    }
    
    if(manual_spray_needed) {
      Spray(true);
    }
    
    previousMillis = micros();
  }
  
  void UpdateDistance()
  {
    digitalWrite(triggerPin, LOW);
    delayMicroseconds(2);
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(triggerPin, LOW);
  
    pinMode(echoPin, INPUT);
    float duration = pulseIn(echoPin, HIGH, 10000);
  
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
      // sensors.requestTemperatures();
      // temperature = sensors.getTempCByIndex(0);
      // temperatureDelay = 0;
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
    lcd.print(" cm ");
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
  
  void Spray(bool manual) {
    spray_needed = false;
    
    if(manual) {
       doSpray(1);
       manual_spray_needed = false;
       return;
    }
    switch(wc_status) {
       case Kakken:
         doSpray(2);
       break;
       case Pissen:
         doSpray(1);
       break;
    } 
  }
  
  void doSpray(int amount) {
    for(int t = 0; t < amount; t++) {
      digitalWrite(motorPin, HIGH);
      delay(500);
      digitalWrite(motorPin, LOW);
    }
  }
