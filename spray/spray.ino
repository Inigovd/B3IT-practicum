  //Libraries
  #include <EEPROM.h>
  #include <OneWire.h>
  #include <PinChangeInt.h>
  #include <LiquidCrystal.h>
  #include <limits.h>
  
  //Arduino pins
  #define motorPin 7
  #define triggerPin 9
  #define echoPin 10
  #define tempPin 8
  #define doorPin A0
  #define menuPin1 A2
  #define menuPin2 A3
  #define buttonPin A5
  #define redLEDPin A1
  #define greenLEDPin 6
  
  /*
  *----------------------*
  * Variables
  *----------------------*
  */
  int distance, temperature, uses_left, menu_selection, spraycount, ledFrequency;
  enum Status { in_use_unknown, in_use_one, in_use_two, in_use_cleaning, idle, triggered_spraying, in_menu };
  const char* StatusNames[6] = { "In use: unknown", "In use: #1", "In use: #2", "In use: cleaning", "Idle", "Spray shot imminent..." };
  Status wc_status;
  bool spray_needed, door_open, manual_spray_needed, ledLightOn, manualSprayNeeded;
  unsigned long numberOneTimer, numberTwoTimer, idletimer, currentMillis, previousMillis, temperatureDelay;
  int sprayDelayInterval[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  int sprayDelayIndex = 0;

  // Interrupts
  bool interrupt_wait = false;
  int interrupt_timer = 0;
  
  LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
  OneWire ds(tempPin);
  
  /*
  *----------------------*
  * Interrupt functions
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
          writeSprayCount(spraycount);
        break;
      }
    } else {
      manual_spray_needed = true;
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

    pinMode(redLEDPin, OUTPUT);
    pinMode(greenLEDPin, OUTPUT);
    
    wc_status = idle;

    // Read EEPROM
    spraycount = readSprayCount();
    
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
    
    // Interrupt timer takes care of debouncing
    if(interrupt_wait) {
      interrupt_timer += timePassed;
      if(interrupt_timer > 30) {
        interrupt_timer = 0;
        interrupt_wait = false;
      }
    }

    if(wc_status == in_menu) {
      UpdateLCDMenu();
    } else {
      UpdateDistance();
      UpdateTemperature(timePassed);
      UpdateLCD();
      UpdateLEDs(timePassed);
      UpdateDoor();
      GetStatus(numberOneTimer, numberTwoTimer);

      if(!door_open) {  // Toilet is going to be used
        if(distance > 0 && distance <= 80) 
        {
          spray_needed = true;
          if(distance >= 50) 
            numberOneTimer += timePassed; // "Number one"
          else 
            numberTwoTimer += timePassed; // "Number two"
        }
      } else if(door_open && !spray_needed) { // Cleaning
        if(distance > 0 && distance < 60) {
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
      
      // Spray after a delay if needed
     if((distance == 0 || distance > 80) && (wc_status == in_use_one || wc_status == in_use_two) && spray_needed) {
       idletimer += timePassed;
       if(idletimer > GetSprayDelay()) {
         Spray(false);
         idletimer = 0; 
       }
      }

    }

    // Dedicated spray button
    if(manual_spray_needed) {
      Spray(true);
    }

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
    if(temperatureDelay < 200)
    {
      temperatureDelay += timePassed;
    }
    else // Only update temperature every 2 seconds
    {
      getTemperature();
      temperatureDelay = 0;
    }
  }
  
  void UpdateLCD()
  {
    lcd.setCursor(0, 0);
    lcd.print(StatusNames[wc_status]);
    lcd.print("                ");
    
    lcd.setCursor(0, 1);
    lcd.print(temperature);
    lcd.print("c ");
    lcd.print("Sprays: ");
    lcd.print(spraycount);
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
    if(analog < 500) {
      door_open = false; 
    } else {
      door_open = true;
    }
  }
  
  void UpdateLEDs(unsigned long timePassed)
  {
    ledFrequency += timePassed;
    if(ledFrequency > 50)
    {
      ledLightOn = !ledLightOn;
      ledFrequency = 0;
    }
    switch(wc_status) {
      case idle:
      case in_use_unknown:
        analogWrite(redLEDPin, 0);
        if(ledLightOn)
          digitalWrite(greenLEDPin, HIGH);
        else
          digitalWrite(greenLEDPin, LOW);
      break;
      case in_use_cleaning:
        analogWrite(redLEDPin, 0);
        digitalWrite(greenLEDPin, HIGH);
      break;
      case in_use_one:
        if(ledLightOn)
          analogWrite(redLEDPin, 255);
        else
          analogWrite(redLEDPin, 0);
        digitalWrite(greenLEDPin, LOW);
      break;
      case in_use_two:
        analogWrite(redLEDPin, 255);
        digitalWrite(greenLEDPin, LOW);
      break;
      default:
        analogWrite(redLEDPin, 0);
        digitalWrite(greenLEDPin, LOW);
      break;
    }
  }

  void GetStatus(long numberOneTimer, long numberTwoTimer) 
  {
    if(numberOneTimer > numberTwoTimer && numberOneTimer > 500) {
      wc_status = in_use_one;
    } else if(numberTwoTimer >= 2000) {
      wc_status = in_use_two;
    } else if(numberTwoTimer >= 500) {
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
      spraycount--;
    }

    writeSprayCount(spraycount);

    numberTwoTimer = 0; numberOneTimer = 0;
    wc_status = idle;
  }

  int GetSprayDelay()
  {
    return sprayDelayInterval[sprayDelayIndex] * 100;
  }

  // From DS18x20_Temperature Example
  // Needed because DallasTemperature.h's readSensors was a blocking function
  void getTemperature()
  {
    byte i;
    byte present = 0;
    byte type_s;
    byte data[12];
    byte addr[8];
    float celsius, fahrenheit;
    
    if ( !ds.search(addr)) {
      ds.reset_search();
      return;
    }
    
    if (OneWire::crc8(addr, 7) != addr[7]) {
      return;
    }
   
    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        type_s = 1;
        break;
      case 0x28:
        type_s = 0;
        break;
      case 0x22:
        type_s = 0;
        break;
      default:
        return;
    } 

    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
    
    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad

    for ( i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
    }
    temperature = (int)raw / 16.0;
  }

  /*
  *----------------------*
  * EEPROM functions
  *----------------------*
  */
  void writeSprayCount(int sprays)
  {
    int left = sprays >> 8;
    int right = sprays;
    EEPROM.write(0, left);
    EEPROM.write(1, right);
  }

  int readSprayCount()
  {
    int left = EEPROM.read(0);
    left = left << 8;
    int right = EEPROM.read(1);

    return left | right;
  }















