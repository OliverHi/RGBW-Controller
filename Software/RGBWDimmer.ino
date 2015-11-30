/**
Based on the MySensors Project: http://www.mysensors.org

This sketch controls a (analog)RGBW strip by listening to new color values from a (domoticz) controller and then fading to the new color.

Version 0.8 - Oliver Hilsky

TODO
safe/request values after restart/loss of connection
*/


#define SN   "RGBW Led strip"
#define SV   "v0.8"

// Load mysensors library	
#include <MySensor.h>	
// Load Serial Peripheral Interface library  
#include <SPI.h>

// Arduino pin attached to driver pins
#define RED_PIN 3 
#define WHITE_PIN 4	
#define GREEN_PIN 5
#define BLUE_PIN 6
#define SENSOR_ID 1

// Smooth stepping between the values
#define STEP 1
#define INTERVAL 10

MySensor gw;	
   
// Stores the current color settings
// TODO random start values => save & load them
int redval = 100;
int greenval = 100;
int blueval = 100;
int whiteval = 100;
int dimming = 100;
int target_redval = 100;
int target_greenval = 100;
int target_blueval = 100;
int target_whiteval = 100;
int target_dimming = 100;
boolean isOn = true;
unsigned long lastupdate = millis();
     
void setup() 
{
  // Initializes the sensor node (with callback function for incoming messages
  gw.begin(incomingMessage);		
       
  // Present sketch (name, version)
  gw.sendSketchInfo(SN, SV);				
       
  // Register sensors (id, type, description, ack back)
  gw.present(SENSOR_ID, S_RGBW_LIGHT, "RGBW test light", true);

  // Define pin mode (pin number, type)
  pinMode(RED_PIN, OUTPUT);		
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(WHITE_PIN, OUTPUT);

  // init lights
  updateLights();
  
  // debug
  if (isOn) {
    Serial.println("RGBW is running...");
  }

  Serial.println("Waiting for messages...");  
}

void loop()
{
  // Process incoming messages (like config and light state from controller) - basically keep the mysensors protocol running
  gw.process();		

  // and set the new light colors
  if (millis() > lastupdate + INTERVAL) {
    updateLights();
    lastupdate = millis();
  } 
}

// callback function for incoming messages
void incomingMessage(const MyMessage &message) {

  Serial.print("Got a message - ");
  Serial.print("Messagetype is: ");
  Serial.println(message.type);

  // acknoledgment
  if (message.isAck())
  {
   	Serial.println("Got ack from gateway");
  }
  
  // new dim level
  else if (message.type==V_DIMMER) {
      Serial.println("Dimming to ");
      Serial.println(message.getString());
      target_dimming = message.getByte();
  }

  // on / off message
  else if (message.type==V_STATUS) {
    Serial.print("Turning light ");

    isOn = message.getInt();

    if (message.getInt()) {
      Serial.println("on");
    } else {
      Serial.println("off");
    }
  }

  // new color value
  else if (message.type == V_RGBW) {    
    const char * rgbvalues = message.getString();
    inputToRGBW(rgbvalues);    
  }  
}

// this gets called every INTERVAL milliseconds and updates the current pwm levels for all colors
void updateLights() {  

  // update pin values -debug
  Serial.println(greenval);
  Serial.println(redval);
  Serial.println(blueval);
  Serial.println(whiteval);

  Serial.println(target_greenval);
  Serial.println(target_redval);
  Serial.println(target_blueval);
  Serial.println(target_whiteval);
  Serial.println("+++++++++++++++");

  // red
  if (redval < target_redval) {
    redval += STEP;
    if (redval > target_redval) {
      redval = target_redval;
    }
  }
  if (redval > target_redval) {
    redval -= STEP;
    if (redval < target_redval) {
      redval = target_redval;
    }
  }

  // green
  if (greenval < target_greenval) {
    greenval += STEP;
    if (greenval > target_greenval) {
      greenval = target_greenval;
    }
  }
  if (greenval > target_greenval) {
    greenval -= STEP;
    if (greenval < target_greenval) {
      greenval = target_greenval;
    }
  }

  // blue
  if (blueval < target_blueval) {
    blueval += STEP;
    if (blueval > target_blueval) {
      blueval = target_blueval;
    }
  }
  if (blueval > target_blueval) {
    blueval -= STEP;
    if (blueval < target_blueval) {
      blueval = target_blueval;
    }
  }

  // white
  if (whiteval < target_whiteval) {
    whiteval += STEP;
    if (whiteval > target_whiteval) {
      whiteval = target_whiteval;
    }
  }
  if (whiteval > target_whiteval) {
    whiteval -= STEP;
    if (whiteval < target_whiteval) {
      whiteval = target_whiteval;
    }
  }

  // dimming
  if (dimming < target_dimming) {
    dimming += STEP;
    if (dimming > target_dimming) {
      dimming = target_dimming;
    }
  }
  if (dimming > target_dimming) {
    dimming -= STEP;
    if (dimming < target_dimming) {
      dimming = target_dimming;
    }
  }

  // debug - new values
  Serial.println(greenval);
  Serial.println(redval);
  Serial.println(blueval);
  Serial.println(whiteval);

  Serial.println(target_greenval);
  Serial.println(target_redval);
  Serial.println(target_blueval);
  Serial.println(target_whiteval);
  Serial.println("+++++++++++++++");

  // set actual pin values
  if (isOn) {
    analogWrite(RED_PIN, dimming / 100.0 * redval);     
    analogWrite(GREEN_PIN, dimming / 100.0 * greenval);
    analogWrite(BLUE_PIN, dimming / 100.0 * blueval);
    analogWrite(WHITE_PIN, dimming / 100.0 * whiteval);
  } else {
    analogWrite(RED_PIN, 0);    
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
    analogWrite(WHITE_PIN, 0);
  }
}

// converts incoming color string to actual (int) values
// ATTENTION this currently does nearly no checks, so the format needs to be exaclty like domoticz sends the strings
void inputToRGBW(const char * input) {
  Serial.print("Got color value of lentht: "); 
  Serial.println(strlen(input));
  
  if (strlen(input) == 6) {
    Serial.println("new rgb value");
    target_redval   = fromhex (& input [0]);
    target_greenval = fromhex (& input [2]);
    target_blueval  = fromhex (& input [4]);
    target_whiteval = 0;
  } else if (strlen(input) == 9) {
    Serial.println("new rgbw value");
    target_redval   = fromhex (& input [1]); // ignore # as first sign
    target_greenval = fromhex (& input [3]);
    target_blueval  = fromhex (& input [5]);
    target_whiteval = fromhex (& input [7]);
  } else {
    Serial.println("Wrong length of input");
  }  

/*
  Serial.println("New color values: ");
  Serial.println(input);
  Serial.println(redval);
  Serial.println(greenval);
  Serial.println(blueval);
  Serial.println(whiteval);
  Serial.println(dimming);
  */
}

// converts hex char to byte
byte fromhex (const char * str)
{
  char c = str [0] - '0';
  if (c > 9)
    c -= 7;
  int result = c;
  c = str [1] - '0';
  if (c > 9)
    c -= 7;
  return (result << 4) | c;
}
