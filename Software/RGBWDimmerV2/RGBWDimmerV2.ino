/**
Based on the MySensors Project: http://www.mysensors.org

This sketch controls a (analog)RGBW strip by listening to new color/dimmer/status values from a (domoticz/openhab/Homeassistant) controller and then fading to the new color.

Version 2.2 - Added initial state messages to the controller to support home-assistant, also added better nonlinear fading (thanks to Jan Gatzke from the forum)
Version 2.1 - Changed conversion to also work with openhab(2)
Version 2.0 - Updated to MySensors 2 and changed fading
Version 1.0 - Changed pins and gw definition
Version 0.9 - Oliver Hilsky
*/


#define SN   "Name"
#define SV   "v2.2 0108017"

// library settings
#define MY_RF24_CE_PIN 4                  // Radio specific settings for RF24 - changed this!
#define MY_RF24_CS_PIN 10                 // Radio specific settings for RF24 (you'll find similar config for RFM69)
#define MY_RADIO_NRF24                    // Use the nrf24 to send messages
#define MY_DEBUG                          // Enables debug messages in the serial log
#define MY_BAUD_RATE  115200              // Sets the serial baud rate for console
#define MY_SIGNING_SOFT                   // Enables software signing
#define MY_SIGNING_REQUEST_SIGNATURES     // Always request signing from gateway
#define MY_SIGNING_SOFT_RANDOMSEED_PIN 7  // floating pin for randomness
#define MY_SIGNING_NODE_WHITELISTING {{.nodeId = GATEWAY_ADDRESS,.serial = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}}} // add your gateway address here!


#include <SPI.h>
#include <MyConfig.h>
#include <MySensors.h>
#include <Vcc.h>

// IDs for the presentation at the gateway
#define RGBW_ID 1
#define DIMMER_ID 2
#define STATUS_ID 3

// Arduino pin attached to driver pins
#define RED_PIN 3 
#define WHITE_PIN 9
#define GREEN_PIN 5
#define BLUE_PIN 6
#define NUM_CHANNELS 4 // how many channels, RGBW=4 RGB=3...

// Smooth stepping between the values
#define STEP 1
#define INTERVAL 10
   
// Stores the current color settings
byte channels[4] = {RED_PIN, GREEN_PIN, BLUE_PIN, WHITE_PIN};
byte values[4] = {0, 0, 0, 255};
byte target_values[4] = {0, 0, 0, 255};
//Stores corrected values for each step from 0 to 255. See https://diarmuid.ie/blog/pwm-exponential-led-fading-on-arduino-or-other-platforms/
byte converted_values[256] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,9,9,9,9,9,10,10,10,10,11,11,11,11,12,12,12,13,13,13,13,14,14,14,15,15,15,16,16,17,17,17,18,18,19,19,20,20,20,21,21,22,22,23,23,24,24,25,26,26,27,27,28,29,29,30,31,31,32,33,34,34,35,36,37,38,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,54,55,56,57,58,60,61,62,64,65,67,68,70,71,73,75,76,78,80,81,83,85,87,89,91,93,95,97,99,101,104,106,108,111,113,116,118,121,123,126,129,132,135,138,141,144,147,150,154,157,161,164,168,171,175,179,183,187,191,195,200,204,209,213,218,223,228,233,238,243,249,255};


// stores dimming level
byte dimming = 100;
byte target_dimming = 100;

// tracks if the strip should be on of off
boolean isOn = true;

// time tracking for updates
unsigned long lastupdate = millis();
     
void presentation() 
{
  // Present sketch (name, version)
  sendSketchInfo(SN, SV);				
       
  // Register sensors (id, type, description, ack back)
  present(RGBW_ID, S_RGBW_LIGHT);

  // get old values if this is just a restart
  request(RGBW_ID, V_RGBW);
}

void setup() {

  // Set all channels to output (pin number, type)
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(channels[i], OUTPUT);
  }

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
  // and set the new light colors
  if (millis() > lastupdate + INTERVAL) {
    updateLights();
    lastupdate = millis();
  }

  // send initial state (needed to be detected by home assistant)
  static bool first_message_sent = false;

  if ( first_message_sent == false ) {
    Serial.println( "Sending initial state..." );
    sendinitialValuesToGateway();
    first_message_sent = true;
  }
}

void sendinitialValuesToGateway() {
  send(MyMessage(RGBW_ID, V_RGBW).set("000000ff")); // full white
  send(MyMessage(STATUS_ID, V_STATUS).set(isOn)); // on
  send(MyMessage(DIMMER_ID, V_DIMMER).set(target_dimming)); // dimmed to start level
}

// callback function for incoming messages
void receive(const MyMessage &message) {

  Serial.print("Got a message - ");
  Serial.print("Messagetype is: ");
  Serial.println(message.type);

  // acknowledgement
  if (message.isAck())
  {
   	Serial.println("Got ack from gateway");
  }
  
  // new dim level
  else if (message.type == V_DIMMER) {
    Serial.println("Dimming to ");
    Serial.println(message.getString());
    target_dimming = message.getByte();
    send(MyMessage(DIMMER_ID, V_DIMMER).set(target_dimming)); // send acknowledgement
    isOn = true; // a new dimmer value also means on, no seperate signal gets send (by domoticz)
  }

  // on / off message
  else if (message.type == V_STATUS) {
    Serial.print("Turning light ");

    isOn = message.getInt();
    send(MyMessage(STATUS_ID, V_STATUS).set(isOn)); // send acknowledgement

    if (isOn) {
      Serial.println("on");
    } else {
      Serial.println("off");
    }
  }

  // new color value
  else if (message.type == V_RGBW) {    
    Serial.println("Got new color");

    const char * rgbvalues = message.getString();
    inputToRGBW(rgbvalues);  
    send(MyMessage(RGBW_ID, V_RGBW).set(rgbvalues)); // send acknowledgement

    // a new color also means on, no seperate signal gets send (by domoticz); needed e.g. for groups
    isOn = true;  
  }  
}

// this gets called every INTERVAL milliseconds and updates the current pwm levels for all colors
void updateLights() {  

  // update pin values -debug
  // Serial.println(greenval);
  // Serial.println(redval);
  // Serial.println(blueval);
  // Serial.println(whiteval);

  // Serial.println(target_greenval);
  // Serial.println(target_redval);
  // Serial.println(target_blueval);
  // Serial.println(target_whiteval);
  // Serial.println("+++++++++++++++");

  // for each color
  for (int v = 0; v < NUM_CHANNELS; v++) {

    if (values[v] < target_values[v]) {
      values[v] += STEP;
      if (values[v] > target_values[v]) {
        values[v] = target_values[v];
      }
    }

    if (values[v] > target_values[v]) {
      values[v] -= STEP;
      if (values[v] < target_values[v]) {
        values[v] = target_values[v];
      }
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
  // Serial.println(greenval);
  // Serial.println(redval);
  // Serial.println(blueval);
  // Serial.println(whiteval);

  // Serial.println(target_greenval);
  // Serial.println(target_redval);
  // Serial.println(target_blueval);
  // Serial.println(target_whiteval);
  // Serial.println("+++++++++++++++");

  // set actual pin values
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (isOn) {
      // normal fading
      // analogWrite(channels[i], dimming / 100.0 * values[i]);

      //Fading with corrected values see https://diarmuid.ie/blog/pwm-exponential-led-fading-on-arduino-or-other-platforms/
      analogWrite(channels[i], dimming / 100.0 * converted_values[values[i]]);
    } else {
      analogWrite(channels[i], 0);
    }
  }
}

// converts incoming color string to actual (int) values
// ATTENTION this currently does nearly no checks, should work with domoticz / home assistant / openhab 2 though
void inputToRGBW(const char * input) {
  Serial.print("Got color value of length: "); 
  Serial.println(strlen(input));
  
  if (strlen(input) == 6) {
    Serial.println("new rgb value");
    target_values[0] = fromhex (& input [0]);
    target_values[1] = fromhex (& input [2]);
    target_values[2] = fromhex (& input [4]);
    target_values[3] = 0;
  } else if (strlen(input) == 8) { // from openhab
    Serial.println("new rgbw value 8");
    target_values[0] = fromhex (& input [0]);
    target_values[1] = fromhex (& input [2]);
    target_values[2] = fromhex (& input [4]);
    target_values[3] = fromhex (& input [6]);
  } else if (strlen(input) == 9) { // from domoticz
    Serial.println("new rgbw value 9");
    target_values[0] = fromhex (& input [1]); // ignore # as first sign
    target_values[1] = fromhex (& input [3]);
    target_values[2] = fromhex (& input [5]);
    target_values[3] = fromhex (& input [7]);
  } else {
    Serial.println("Wrong length of input");
  }  


  Serial.print("New color values: ");
  Serial.println(input);
  
  // for (int i = 0; i < NUM_CHANNELS; i++) {
  //   Serial.print(target_values[i]);
  //   Serial.print(", ");
  // }
 
  // Serial.println("");
  // Serial.print("Dimming: ");
  // Serial.println(dimming);
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
