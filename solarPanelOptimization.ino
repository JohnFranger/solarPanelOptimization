/*
solarPanelOptimization's tasks are as follows:

1. We will have an LCD screen that switches between our measured DHT11 Data (Fully local), and the weather data from an API
  -We can use a button to switch displays

2. We will have a stepper motor with a photoresistor that rotates to find the highest light value. Once it does a full sweep
   it will return to the highest angle.
  -Runs every time a button is pressed 

*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <LiquidCrystal.h>
#include <DHT.h>
#include <Stepper.h>
#define Type DHT11

//// Extra ////

int lastTime = 0;
int coolDown = 4000;
int apiCoolDown = 60000;   // weather doesn't change every few seconds
int curCycle = 0;
bool reverseComplete = false;

// Buttons || But1 is LCD || But2 is Light Sensor 
const uint8_t but1Pin = 33;
const uint8_t but2Pin = 32; 

bool butPressed = false;

uint8_t but1New;
uint8_t but1Old = 1; 
uint8_t but2New;
uint8_t but2Old = 1; 

bool but1Pressed = false;
bool but2Pressed = false;

//// Photoresistor /////

// ESP32 ADC notes:
// - analogRead only works while WiFi is on if the pin is on ADC1 (GPIO 32, 33, 34, 35, 36, 39).
// - The ESP32 ADC is 12 bit, so readings go from 0 to 4095 (an Uno was 0 to 1023).
constexpr uint8_t lightPin = 34;
uint16_t curLightVal;
uint16_t maxLightVal = 0;


//// LCD Screen ////

constexpr uint8_t rs = 25;
constexpr uint8_t en = 26;
constexpr uint8_t d4 = 27;
constexpr uint8_t d5 = 14;
constexpr uint8_t d6 = 13;
constexpr uint8_t d7 = 4;

LiquidCrystal lcd(rs,en,d4,d5,d6,d7);

//// DHT11 ////

// False means from API, true means from sensor

int sensePin = 23;

bool THSensor = false;

float tempC;
float humidity; 
DHT HT(sensePin, Type);

//// Stepper ////

int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution,5,19,18,21); 
//rpm
int motSpeed = 5;

// We only sweep half a turn (180 degrees) so the photoresistor wires can never wrap around.
// Step 0 is wherever the motor is when it powers on, so start it at one end of the arc.
const int sweepSteps = stepsPerRevolution / 2;
int stepPos = 0;     // where the motor is right now, in steps (0 to sweepSteps)
int bestStep = 0;    // the step where we saw the most light
bool completed = true;

//// API STUFF ////

// Wifi network and password, I used my hotspot. Pointers
// Requires A Private Network
const char* ssid = "esp32";
const char* password = "12345678";

// OpenWeatherMap API details
const String city = "Edmonton";
const String countryCode = "ca";
const String apiKey = "dabdb633cde2f5e3cbef02448c369e18";


// Endpoint URL: Specific URL where the application sends requests to retrieve data from
String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";


void setup() {
  // put your setup code here, to run once:
  pinMode(lightPin, INPUT);
  pinMode(but1Pin, INPUT_PULLUP);
  pinMode(but2Pin, INPUT_PULLUP);
  Serial.begin(9600);
  lcd.begin(16,2);
  HT.begin();
  myStepper.setSpeed(motSpeed);
  analogReadResolution(12); // 12 bit is the ESP32 default, set explicitly so it's obvious


  // API STUFF

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  // If Wi-Fi has not connected yet, print a period to the end of the previous string every 0.5 seconds in the serial monitor
  // Gives up after about 10 seconds so a missing network doesn't freeze the whole device
  int wifiTries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTries < 20) {
    delay(500);
    Serial.print(".");
    wifiTries++;
  }
  // When it is connected, print the user's IP address.
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed, continuing without the API");
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  but1New = digitalRead(but1Pin);
  but2New = digitalRead(but2Pin);

  // average 8 readings so one bad sample can't be mistaken for the brightest spot
  uint32_t lightSum = 0;
  for(uint8_t i = 0; i < 8; i++){
    lightSum += analogRead(lightPin);
  }
  curLightVal = lightSum / 8;

  // Only on falling button edge (button being pressed down)
  if(but1New != but1Old && but1New == 0){
    but1Pressed = true;
    THSensor = !THSensor;
    Serial.println("Button 1 Pressed");
  } else{
    but1Pressed = false;
  }

  // Only on falling button edge (button being pressed down)
  if(but2New != but2Old && but2New == 0){
    but2Pressed = true;
    Serial.println("Button 2 Pressed");
  } else{
    but2Pressed = false;
  }

  // Runs if a certain amount of time has passed, its the first pass, or we press the button
  if(millis() - lastTime >= (THSensor ? coolDown : apiCoolDown) || lastTime == 0 || but1Pressed){
    // Our Measured Values
    if(THSensor){
    
      humidity = HT.readHumidity();
      tempC = HT.readTemperature();

      // Worst case is "-40.0°C || 100%" = 15 characters, so it always fits on the 16 wide LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(tempC, 1);
      lcd.write((uint8_t)223); // degree symbol from the LCD's own character ROM (a "°" in a string prints junk)
      lcd.print("C || ");
      lcd.print(humidity, 0);
      lcd.print("%");
    
    }else {
      // API data only goes to the Serial Monitor, the LCD just says where to look
      lcd.setCursor(0, 0);
      lcd.print("API -> Serial   ");

      if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      // Begin HTTP request
      http.begin(serverPath.c_str());

      http.setTimeout(2000);
      http.setConnectTimeout(2000);

      // Send HTTP GET request
      // 200: is a success, 401: invalid key, 404: for bad city name
      int httpResponseCode = http.GET();

      // Only 200 has real weather data, 401/404 still send back JSON but without "main"
      if (httpResponseCode == 200) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String payload = http.getString();
        
        // Parse the JSON response 
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
        else {
          // Extract temperature and humidity values from the JSON file
          float temp = doc["main"]["temp"];
          float apiHumidity = doc["main"]["humidity"];

          // Print results to Serial Monitor
          Serial.print("API: ");
          Serial.print(temp);
          Serial.print(" C || ");
          Serial.print(apiHumidity);
          Serial.println(" %");

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(temp, 1);
          lcd.write((uint8_t)223); // degree symbol from the LCD's own character ROM (a "°" in a string prints junk)
          lcd.print("C || ");
          lcd.print(apiHumidity, 0);
          lcd.print("%");
        }

      } 
      else 
      {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }

      // Free resources
      http.end();
      } 
      else 
      {
        Serial.println("WiFi Disconnected");
      }

      
    } 
    lastTime = millis();
  }

  // We pressed the button to start the search and the previous search is complete
  if(but2Pressed && completed){
    maxLightVal = 0;
    bestStep = 0;
    curCycle = 0;
    stepPos = 0;
    completed = false;
  }

  // Must go forward then backwards to avoid tangling wires
  // This could definitely be optimized and generalized to avoid copy pasting
  // Completed means we reached the max point, allowing the button to be pressed again.

  if(!completed){
    // First 180 degrees forwards
    if(curCycle == 0){
      if(stepPos == sweepSteps){
        curCycle = 1;
        Serial.println("Cycle 1");
      } else{
      myStepper.step(1);
      stepPos++;
      }
      if(curLightVal > maxLightVal){
        maxLightVal = curLightVal;
        bestStep = stepPos;
      }
    }

    // Back to zero
    if(curCycle == 1){
      if(stepPos == 0){
        curCycle = 2;
        Serial.println("Cycle 2");
      } else{
      myStepper.step(-1);
      stepPos--;
      }
      if(curLightVal > maxLightVal){
        maxLightVal = curLightVal;
        bestStep = stepPos;
      }
    }

    // 180 degrees backwards
    if(curCycle == 2){
      if(stepPos == -sweepSteps){
        curCycle = 3;
        Serial.println("Cycle 3");
      } else{
      myStepper.step(-1);
      stepPos--;
      }
      if(curLightVal > maxLightVal){
        maxLightVal = curLightVal;
        bestStep = stepPos;
      }
    }

    // Back to Zero
    if(curCycle == 3){
      if(stepPos == 0){
        curCycle = 4;
        Serial.println("Cycle 4");
      } else{
      myStepper.step(1);
      stepPos++;
      }
      if(curLightVal > maxLightVal){
        maxLightVal = curLightVal;
        bestStep = stepPos;
      }
    }

    // To highest measured value
    if(curCycle == 4){
      if(stepPos == bestStep){
        completed = true;
        Serial.println("Completed");
      } else if(stepPos > bestStep){
      myStepper.step(-1);
      stepPos--;
      } else{
      myStepper.step(1);
      stepPos++;
      }
    }
  }

  /*
  // Non-blocking sweep: only ONE step per pass of loop(), reading the light at every step.
  // (myStepper.step(1) waits at most ~3 ms, instead of step(2048) locking up everything for 6 s)
  if(!cycle1Complete){
    if(!atHome){
      // First walk back to step 0 (no reading) so every sweep starts from the same end of the arc
      if(stepPos > 0){
        myStepper.step(-1);
        stepPos--;
      } else {
        atHome = true;
      }
    } else {
      // Remember which STEP had the most light (not a time), so no time-to-angle conversion is needed
      if(maxLightVal <= curLightVal){
        maxLightVal = curLightVal;
        bestStep = stepPos;
      }

      if(stepPos < sweepSteps){
        myStepper.step(1);
        stepPos++;
      } else {
        cycle1Complete = true;
      }
    }
  }

  // finished the sweep, walks backwards one step per pass back to the brightest step
  if(cycle1Complete && !reverseComplete){
    if(stepPos > bestStep){
      myStepper.step(-1);
      stepPos--;
    } else {
      reverseComplete = true;
    }
  }
  */

  //Updating each pass to see if the button was pressed
  but1Old = but1New;
  but2Old = but2New;
}
