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

#define UPPER_LIMIT_SENSOR 2  //Pin used for input from the garage door upper limit sensor
#define LOWER_LIMIT_SENSOR 3  //Pin used for input from the garage door lower limit sensor
#define DOOR_ACTUATOR_RELAY_OPEN 4 //Pin used to toggle the door actuator relay in
#define DOOR_ACTUATOR_RELAY_CLOSE 5 //Pin used to toggle the door actuator relay out
#define RELAY_ON 0            // GPIO value to write to turn on attached relay
#define RELAY_OFF 1           // GPIO value to write to turn off attached relay

#define TOGGLE_INTERVAL 1500  //Tells how many milliseconds the relay will be held closed
#define HEARTBEAT_INTERVAL 120000 //Number of miliseconds before the next heartbeat

#define CLOSED 0
#define OPEN 1
#define CLOSING 2
#define OPENING 3

#define MOTOR_OVERRUN_CLOSE 1000 // How long to overrun on open/close millis
#define MOTOR_OVERRUN_OPEN  10 // How long to overrun on open/close millis

#define OBSTR_MAX 20

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
int lLimit = 0; 

//Used for the error condition when the door is unexpectedly reversed
boolean obstruction = false;

int obstruction_count = 0;

// the timer object used for the heartbeat
SimpleTimer heartbeat;

MyMessage msgOpener(CHILD_ID_OPENER, V_STATUS);
MyMessage msgStatus(CHILD_ID_STATUS, V_TEXT);

/**
 * presentation - Present the garage door sensor
 */
void presentation() {
  
  sendSketchInfo( SKETCH_NAME, SKETCH_VERSION );
  
  // Register the garage door sensor with the gateway
  present( CHILD_ID_OPENER, S_BINARY );
  present( CHILD_ID_STATUS, S_INFO );
  
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

  digitalWrite(DOOR_ACTUATOR_RELAY_OPEN, HIGH);
  digitalWrite(DOOR_ACTUATOR_RELAY_CLOSE,HIGH);

  Serial.println( SKETCH_NAME );

  // Set the heartbeat timer to send the current state at the set interval
  heartbeat.setInterval(HEARTBEAT_INTERVAL, sendCurrentState);
  
} //End setup

/**
 * loop - The main program loop
 */
void loop() {

  //Get the state of the door
  getState();

  //Here we check if the state has changed and update the gateway with the change.  We do this
  //after all processing of the state because we need to know if there was an obstruction
  if ((lState != cState) || obstruction == true) {
    sendCurrentState();

    obstruction_count ++;
    if (obstruction_count >= OBSTR_MAX) {
      //Once the obstruction is checked and sent, we can clear it
      obstruction = false;
    }

    //If the current state is full open or full closed we need to set the last limit
    if ( (cState == OPEN) || (cState == CLOSED) ) {
      setLastLimit( cState );
    }
  }

  heartbeat.run();
  
} //End loop

/**
 * receive - Process the incoming messages and watch for an incoming boolean 1
 *           to toggle the garage door opener.
 */
void receive( const MyMessage &message ) {
  //We only expect one type of message from controller. But we better check anyway.
  if (message.type == V_STATUS){
    switch(message.getBool()){
      case 0:
        // CLOSE THE DOOR
        if(cState == OPEN){
          toggleDoor(0);
        }
        break;
      case 1:
        // OPEN THE DOOR
        if(cState == CLOSED){
          toggleDoor(1);
        }
        break;
    }
  }
  
} //End receive

/**
 * sendCurrentState - Sends the current state back to the gateway
 */
void sendCurrentState() {
  
    send( msgStatus.set( obstruction == true ? "OBSTRUCTION" : currentState[cState] ) ); 
    
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
 * relayOff - Used to turn off the door opener relay after the TOGGLE_INTERVAL
 */
void relayOff() {
  Serial.println("OFF ALL");
  digitalWrite( DOOR_ACTUATOR_RELAY_OPEN, RELAY_OFF );
  digitalWrite( DOOR_ACTUATOR_RELAY_CLOSE, RELAY_OFF );
  //Added this to tell the controller that we shut off the relay
  send( msgOpener.set(0) );
  
} //End relayOff

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
  } else if ((upper == HIGH) && (lower == HIGH)) {
    //If in motion and the last full position of the door was open
    if (lLimit == OPEN) {
      //Set the current state to closing
      cState = CLOSING;
      //If in motion and the last full position of the door was closed
    } else {
      //Set the current state to opening
      cState = OPENING;
    }
  }
  
} //End getState

/**
 * setLastLimit - Checks the limit passed with the last limit (lLimit) and sets the last limit
 *                if there was a change.  It also checks the states against the limits and issues 
 *                an obstruction error if the limit and last limit
 *                
 * @param limit  integer limit - The limit to be checked
 */
void setLastLimit( int limit ) {
  
  //Here is where we check for our error condition
  if ( (lLimit == OPEN) && (limit == OPEN) && (lState == CLOSING) ) {
    //An obstruction has reversed the door.  No need to set the last limit because it already equals
    //the limit we are trying to set it to
    obstruction = true;
    //If we made it here and the last limit does not equal the limit we are setting then change it.  If
    //the last limit is equal to the limit we are setting then the last state was something other than
    //closing, so we don't need to do anything.
  } else if ( lLimit != limit ) {
    //Everything okay, set the last limit
    lLimit = limit;
    obstruction = false;
  }
  
} //End setLastLimit
