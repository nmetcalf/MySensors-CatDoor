// Added to GitHub
#define MY_DEBUG

//Set up the nRF24L01+
#define MY_RADIO_NRF24

#include <SPI.h>
#include <MySensors.h>
#include <SimpleTimer.h>

#define SKETCH_NAME "MyCatDoor"
#define SKETCH_VERSION "1.2"

#define CHILD_ID_OPENER 0
#define CHILD_ID_STATUS 1

// Sensor Pins
#define UPPER_LIMIT_SENSOR 3  //Pin used for input from the garage door upper limit sensor
#define LOWER_LIMIT_SENSOR 2  //Pin used for input from the garage door lower limit sensor

// Motor Pins
#define DOOR_ACTUATOR_RELAY_OPEN 4 //Pin used to toggle the door actuator relay in
#define DOOR_ACTUATOR_RELAY_CLOSE 5 //Pin used to toggle the door actuator relay out

// HIGH or LOW logic?
#define RELAY_ON 0            // GPIO value to write to turn on attached relay
#define RELAY_OFF 1           // GPIO value to write to turn off attached relay

#define HEARTBEAT_INTERVAL 300000 //Number of miliseconds before the next heartbeat

// State int's
#define CLOSED 0
#define OPEN 1

// Motor Overrun times (millis())
#define MOTOR_OVERRUN_CLOSE 1200
#define MOTOR_OVERRUN_OPEN  700

const char *currentState[] = { 
                             "Closed", 
                             "Open", 
                             "Closing", 
                             "Opening" 
                             };

//Current state of the door
int cState;     

//Last state of the door
int lState;     

//The last full limit position of the door (open or closed)

// the timer object used for the heartbeat
SimpleTimer heartbeat;

MyMessage msgOpener(CHILD_ID_OPENER, V_STATUS);
MyMessage msgStatus(CHILD_ID_STATUS, V_TRIPPED);

/**
 * presentation - Present the garage door sensor
 */
void presentation() {
  
  sendSketchInfo( SKETCH_NAME, SKETCH_VERSION );
  
  // Register the garage door sensor with the gateway
  present( CHILD_ID_OPENER, S_BINARY );
  present( CHILD_ID_STATUS, S_DOOR );
  
} //End presentation

/**
 * setup - Initialize the garage door sensor
 */
void setup() {

  // Set up the pins for reading the upper and lower limit sensors
  pinMode(UPPER_LIMIT_SENSOR, INPUT);
  pinMode(LOWER_LIMIT_SENSOR, INPUT);
  
  // Set up the pin to control the door opener
  pinMode(DOOR_ACTUATOR_RELAY_OPEN, OUTPUT);
  pinMode(DOOR_ACTUATOR_RELAY_CLOSE, OUTPUT);

  // Disable both motors
  digitalWrite(DOOR_ACTUATOR_RELAY_OPEN, HIGH);
  digitalWrite(DOOR_ACTUATOR_RELAY_CLOSE,HIGH);

  // Why Not?
  Serial.println( SKETCH_NAME );
  
} //End setup

/**
 * loop - The main program loop
 */
void loop() {
  //Get the state of the door
  getState();
  
  if ((lState != cState)) {
    sendCurrentState();
  }

  // Sleep for a while to save energy
  sleep(UPDATE_INTERVAL); 
  
} //End loop

/**
 * receive - Process the incoming messages and watch for an incoming boolean 1
 *           to toggle the garage door opener.
 */
void receive( const MyMessage &message ) {
  //We only expect one type of message from controller. But we better check anyway.
  if (message.type == V_STATUS){
    switch(message.getBool()){
      case CLOSED:
        // CLOSE THE DOOR
        if(cState == OPEN){
          toggleDoor(CLOSED);
        }
        break;
      case OPEN:
        // OPEN THE DOOR
        if(cState == CLOSED){
          toggleDoor(OPEN);
        }
        break;
    }
  }
  
} //End receive

/**
 * sendCurrentState - Sends the current state back to the gateway
 */
void sendCurrentState() {
    send( msgStatus.set(cState) ); 
} //End sendCurrentState

/**
 * toggleDoor - Used to activate the garage door opener
 */
void toggleDoor(int rState) {
  //Get the state of the door
  getState();
  if(rState == CLOSED){
    // Door closed meow please!
    digitalWrite( DOOR_ACTUATOR_RELAY_CLOSE, RELAY_ON );
    while(digitalRead(LOWER_LIMIT_SENSOR)!= HIGH){
      delay(100);
    }
    delay(MOTOR_OVERRUN_CLOSE);
    digitalWrite( DOOR_ACTUATOR_RELAY_CLOSE, RELAY_OFF );  
  }else if(rState == OPEN){
    // Please let meow out MEOW!
    digitalWrite( DOOR_ACTUATOR_RELAY_OPEN, RELAY_ON );  
    while(digitalRead(UPPER_LIMIT_SENSOR)!= HIGH){
      delay(100);
    }
    delay(MOTOR_OVERRUN_OPEN);
    digitalWrite( DOOR_ACTUATOR_RELAY_OPEN, RELAY_OFF );  
  }  
} //End toggleDoor

/**
 * getState - Used to get the current state of the garage door.  This will set the cState variable
 *            to either OPEN, CLOSED, OPENING and CLOSING
 */
void getState() {
  //read the upper sensor
  int upper = digitalRead( UPPER_LIMIT_SENSOR ); 
  //read the lower sensor
  int lower = digitalRead( LOWER_LIMIT_SENSOR ); 

  //Save the last state of the door for later tests
  lState = cState;

  //Check if the door is open
  if ((upper == HIGH) && (lower == LOW)) {
    //Set the current state to open
    cState = OPEN;
    //Check if the door is closed
  } else if ((upper == LOW) && (lower == HIGH)) {
    //Set the current state to closed
    cState = CLOSED;
    //Check if the door is in motion
  }
} //End getState
