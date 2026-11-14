# Solar Panel Optimization
This project is created for the 2026 HackEd Beta focusing on environmental aspects. 
We choose to create a that seeks the point of highest light with a photoresistor. 
It also looks at an API and DHT11 for local weather conditions. 


## Circuit Concept

- **Goals:** 
1. Use a stepper motor to find optimal position for photoresistor (analagous to a solar panel)
2. Pull live API weather data for general weather, can switch to live readings from our own sensors

- **Components:** 
1. ESP32 DEVKIT V1
2. LCD Display
3. 1kΩ Potentiometer 
4. DHT 11 Temperature and Humidity Sensor
5. External Power Supply Module + 9V Battery 
6. Stepper Motor & Driver
7. 220Ω Resistor
8. Jumper Wires 

## Mathematical Physical Proof / Analysis / Logic 

- We are using buttons to switch between states or operate Logic
- Given that this is a real project, we strayed away from blocking code such as loops and delays
- This means we resorted heavily to boolean logic, flags, and states. 

- We utilized various timers along with the math to find out how long a given operation must take
- This along with state tracking ensured things only ran once and only when they were supposed to.

- The API call is to get local conditions, however due to the length of the weather condition string,
  we chose to not implement it on the LCD. However, the data is parseable from the json file. 


