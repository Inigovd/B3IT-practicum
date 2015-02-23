  //Libraries
  #include <DallasTemperature.h>
  #include <OneWire.h>
  #include <PinChangeInt.h>
  #include <LiquidCrystal.h>
  #include <limits.h>
  
  //Arduino Digital pins
  #define motorPin 7
  #define triggerPin 9
  #define echoPin 10
  #define tempPin 8
  #define doorPin A0
  #define menuPin1 A2
  #define menuPin2 A3
  #define buttonPin A5
  
  /*
  *----------------------*
  * Variables
  *----------------------*
  */
  int distance, temperature, uses_left, menu_selection, spraycount;
  enum Status { in_use_unknown, in_use_one, in_use_two, in_use_cleaning, idle, triggered_spraying, in_menu };
  const char* StatusNames[6] = { "In use: unknown", "In use: #1", "In use: #2", "In use: cleaning", "Idle", "Spray shot imminent..." };
  Status wc_status;
  bool spray_needed, door_open, manual_spray_needed;
  unsigned long plastimer, poeptimer, idletimer, currentMillis, previousMillis, temperatureDelay;
  int sprayDelayInterval[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  int sprayDelayIndex = 0;

  // Interrupts
  bool interrupt_wait = false;
  int interrupt_timer = 0;
  
  LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
  OneWire oneWire(tempPin);
  DallasTemperature sensors(&oneWire);
  
  /*
  *----------------------*
  * Interrupts
  *----------------------*
  */
  void manualSprayInterrupt() 
  {
    if(interrupt_wait)
      return;
    interrupt_wait = true;

    if(wc_status != in_menu)
      manual_spray_needed = true;
  }

  void menuInterrupt() 
  {
    if(interrupt_wait)
      return;
    interrupt_wait = true;

    if(wc_status != in_menu){
      wc_status = in_menu;
      menu_selection = 0;
    } else {
      menu_selection++;
      if(menu_selection > 1) {
        menu_selection = 0;
        wc_status = idle;
      }
    }
  }

  void menuSelectInterrupt() {
    if(interrupt_wait)
      return;
    interrupt_wait = true;

    if(wc_status == in_menu) {
      switch(menu_selection) {
        case 0:
          sprayDelayIndex++;
          sprayDelayIndex %= 10;
        break;
        case 1:
          spraycount = 2400;
        break;
      }
    }
  }

  /*
  *----------------------*
  * Setup
  *----------------------*
  */
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
    
    wc_status = idle;
    
    attachPinChangeInterrupt(buttonPin, manualSprayInterrupt, FALLING);
    attachPinChangeInterrupt(menuPin1, menuInterrupt, FALLING);
    attachPinChangeInterrupt(menuPin2, menuSelectInterrupt, FALLING);
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
    
    if(interrupt_wait) {
      interrupt_timer += timePassed;
      if(interrupt_timer > 5) {
        interrupt_timer = 0;
        interrupt_wait = false;
      }
    }
    // In menu
    if(wc_status == in_menu) {
      UpdateLCDMenu();
    } else {
      // Not in menu
      UpdateDistance();
      UpdateTemperature(timePassed);
      UpdateLCD();
      UpdateDoor();
      GetStatus(plastimer, poeptimer);

      if(!door_open) {  // Toilet is going to be used
        if(distance > 0 && distance <= 80) 
        {
          spray_needed = true;
          if(distance >= 30) 
            plastimer += timePassed;
          else 
            poeptimer += timePassed;  
        }
      } else if(door_open && !spray_needed) { // Cleaning
        if(distance > 0) {
          wc_status = in_use_cleaning;
          idletimer = 0;
        } else if(wc_status != idle) {
          idletimer += timePassed;
          if(idletimer > GetSprayDelay()) {
            wc_status = idle;
            idletimer = 0;
          }
        }
      }
      
      if(spray_needed) {
       if((distance == 0 || distance > 80) && (wc_status == in_use_one || wc_status == in_use_two)) {
         idletimer += timePassed;
         if(idletimer > GetSprayDelay()) {
           Spray(false);
           idletimer = 0; 
         }
        }
      }

      if(manual_spray_needed) {
        Spray(true);
      }
    }

    delay(50); // DEBUG
    previousMillis = micros();
  }
  
  /*
  *----------------------*
  * Update Functions
  *----------------------*
  */
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
    else // Only update temperature every 2 seconds
    {
      // sensors.requestTemperatures();
      // temperature = sensors.getTempCByIndex(0);
      // temperatureDelay = 0;
    }
  }
  
  void UpdateLCD()
  {
    lcd.setCursor(0, 0);
    lcd.print(StatusNames[wc_status]);
    lcd.print("                ");
    
    lcd.setCursor(0, 1);
    lcd.print(idletimer);
    lcd.print("                ");
    /*
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C   "); // 10 chars used
    */
  }

  void UpdateLCDMenu()
  {
    String line_one = "  Delay: " + String(GetSprayDelay() / 100) + "s";
    String line_two = "  Reset sprays";
    switch(menu_selection) {
      case 0:
        line_one = "> Delay: " + String(GetSprayDelay() / 100) + "s";
      break;
      case 1:
        line_two = "> Reset sprays";
      break;
    }
    lcd.setCursor(0, 0);
    lcd.print(line_one);
    lcd.print("       ");
    lcd.setCursor(0, 1);
    lcd.print(line_two);
    lcd.print("       ");
  }
  
  void UpdateDoor()
  {
    int analog = analogRead(doorPin);
    temperature = analog;
    if(analog < 500) {
      door_open = false; 
    } else {
      door_open = true;
    }
  }
  
  void GetStatus(long plastimer, long poeptimer) 
  {
    if(plastimer > poeptimer && plastimer > 500) {
      wc_status = in_use_one;
    } else if(poeptimer >= 2000) {
      wc_status = in_use_two;
    } else if(poeptimer >= 500) {
      wc_status = in_use_one;
    } else if(!door_open) {
      wc_status = in_use_unknown;
    } 
  }
  
  /*
  *----------------------*
  * Spraying
  *----------------------*
  */
  void Spray(bool manual) {
    spray_needed = false;
    
    if(manual) {
       doSpray(1);
       manual_spray_needed = false;
       return;
    }
    switch(wc_status) {
       case in_use_two:
         doSpray(2);
       break;
       case in_use_one:
         doSpray(1);
       break;
    } 
  }
  
  void doSpray(int amount) {
    wc_status = triggered_spraying;
    UpdateLCD();
    
    for(int t = 0; t < amount; t++) {
      digitalWrite(motorPin, HIGH);
      delay(5000);
      digitalWrite(motorPin, LOW);
      delay(50);
    }
    
    poeptimer = 0; plastimer = 0;
    wc_status = idle;
  }

  int GetSprayDelay()
  {
    return sprayDelayInterval[sprayDelayIndex] * 100;
  }










