/**
Based on the MySensors Project: http://www.mysensors.org

This sketch controls a (analog)RGBW strip by listening to new color values from a (domoticz) controller and then fading to the new color.

Version 2.0 - Updated to MySensors 2 and changed fading
Version 1.0 - Changed pins and gw definition
Version 0.9 - Oliver Hilsky

TODO
safe/request values after restart/loss of connection
*/


#define SN   "RGBW Schrankwand"
#define SV   "v2.0 13112016"

// library settings
#define MY_RF24_CE_PIN 4    // Radio specific settings for RF24 - changed this!
#define MY_RF24_CS_PIN 10 // Radio specific settings for RF24 (you'll find similar config for RFM69)
#define MY_RADIO_NRF24
#define MY_DEBUG    // Enables debug messages in the serial log
#define MY_BAUD_RATE  9600 // Sets the serial baud rate for console and serial gateway
#define MY_SIGNING_SOFT // Enables software signing
#define MY_SIGNING_REQUEST_SIGNATURES // Always request signing from gateway
#define MY_SIGNING_SOFT_RANDOMSEED_PIN 7 // floating pin for randomness
#define MY_SIGNING_NODE_WHITELISTING {{.nodeId = GATEWAY_ADDRESS,.serial = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}}} // gateway addres if you want to use whitelisting (node only works with messages from this one gateway)

#include <SPI.h>
#include <MyConfig.h>
#include <MySensors.h>
#include <Vcc.h>

#define SENSOR_ID 1
// Arduino pin attached to driver pins
#define RED_PIN 3 
#define WHITE_PIN 9
#define GREEN_PIN 5
#define BLUE_PIN 6
#define NUM_CHANNELS 4 // how many channels, RGBW=4 RGB=3...

// Smooth stepping between the values
#define STEP 1
#define INTERVAL 10
const int pwmIntervals = 255;
float R; // equation for dimming curve
   
// Stores the current color settings
byte channels[4] = {RED_PIN, GREEN_PIN, BLUE_PIN, WHITE_PIN};
byte values[4] = {0, 0, 0, 255};
byte target_values[4] = {0, 0, 0, 255}; 

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
  present(SENSOR_ID, S_RGBW_LIGHT, SN, true);

  // get old values if this is just a restart
  request(SENSOR_ID, V_RGBW);
}

void setup() {

  // Set all channels to output (pin number, type)
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(channels[i], OUTPUT);
  }

  // set up dimming
  R = (pwmIntervals * log10(2))/(log10(255));

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
}

// callback function for incoming messages
void receive(const MyMessage &message) {

  Serial.print("Got a message - ");
  Serial.print("Messagetype is: ");
  Serial.println(message.type);

  // acknoledgment
  if (message.isAck())
  {
   	Serial.println("Got ack from gateway");
  }
  
  // new dim level
  else if (message.type == V_DIMMER) {
      Serial.println("Dimming to ");
      Serial.println(message.getString());
      target_dimming = message.getByte();

      // a new dimmer value also means on, no seperate signal gets send (by domoticz)
    isOn = true;
  }

  // on / off message
  else if (message.type == V_STATUS) {
    Serial.print("Turning light ");

    isOn = message.getInt();

    if (isOn) {
      Serial.println("on");
    } else {
      Serial.println("off");
    }
  }

  // new color value
  else if (message.type == V_RGBW) {    
    const char * rgbvalues = message.getString();
    inputToRGBW(rgbvalues);  

    // a new color also means on, no seperate signal gets send (by domoticz); needed e.g. for groups
    isOn = true;  
  }  
}

// this gets called every INTERVAL milliseconds and updates the current pwm levels for all colors
void updateLights() {  

  // update pin values -debug
  //Serial.println(greenval);
  //Serial.println(redval);
  //Serial.println(blueval);
  //Serial.println(whiteval);

  //Serial.println(target_greenval);
  //Serial.println(target_redval);
  //Serial.println(target_blueval);
  //Serial.println(target_whiteval);
  //Serial.println("+++++++++++++++");

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

  /*
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
  */

  // set actual pin values
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (isOn) {
      // normal fading
      //analogWrite(channels[i], dimming / 100.0 * values[i]);

      // non linear fading, idea from https://diarmuid.ie/blog/pwm-exponential-led-fading-on-arduino-or-other-platforms/
      analogWrite(channels[i], pow (2, (dimming / R)) - 1);
    } else {
      analogWrite(channels[i], 0);
    }
  }
}

// converts incoming color string to actual (int) values
// ATTENTION this currently does nearly no checks, so the format needs to be exactly like domoticz sends the strings
void inputToRGBW(const char * input) {
  Serial.print("Got color value of length: "); 
  Serial.println(strlen(input));
  
  if (strlen(input) == 6) {
    Serial.println("new rgb value");
    target_values[0] = fromhex (& input [0]);
    target_values[1] = fromhex (& input [2]);
    target_values[2] = fromhex (& input [4]);
    target_values[3] = 0;
  } else if (strlen(input) == 9) {
    Serial.println("new rgbw value");
    target_values[0] = fromhex (& input [1]); // ignore # as first sign
    target_values[1] = fromhex (& input [3]);
    target_values[2] = fromhex (& input [5]);
    target_values[3] = fromhex (& input [7]);
  } else {
    Serial.println("Wrong length of input");
  }  


  Serial.print("New color values: ");
  Serial.println(input);
  
  for (int i = 0; i < NUM_CHANNELS; i++) {
    Serial.print(target_values[i]);
    Serial.print(", ");
  }
 
  Serial.println("");
  Serial.print("Dimming: ");
  Serial.println(dimming);
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
