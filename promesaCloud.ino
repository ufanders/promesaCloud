/**
 * Simplest possible example of using RF24Network
 *
 * TRANSMITTER NODE
 * Every 2 seconds, send a payload to the receiver node.
 */

//Promesa Cloud
#include <EEPROM.h>

struct cloudStorage
{
  bool netMaster;
  uint16_t netNodeAddress;
  uint8_t netChannel;
  uint8_t netNodeMax;
  uint8_t ledBrightness;
  uint8_t ledFps;
  uint8_t ledPattern;
};

cloudStorage eeprom;

/*
struct cloudMsg
{
  uint16_t msgLen;
  uint16_t msgType;
  void* msgPayload;
};
*/

struct cloudMsg
{
  uint8_t msgType;
  uint8_t msgPayload[16];
};

enum msgType {MSG_NOOP, MSG_SETCOLOR, MSG_SETPATTERN, MSG_MAX};
void printCloudMsg(cloudMsg* msg);
int handleConsole(void);

//nRF24L01
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>

RF24 radio(18, 10);                    // nRF24L01(+) radio attached using Getting Started board
RF24Network network(radio);          // Network uses that radio
const uint16_t this_node = 00;       // Address of our node in Octal format
const uint16_t other_node = 01;      // Address of the other node in Octal format
const unsigned long interval = 2000; // How often (in ms) to send 'hello world' to the other unit
unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already
struct payload_t {                   // Structure of our payload
  unsigned long ms;
  unsigned long counter;
};

//FastLED
#include <FastLED.h>
#define NUM_LEDS 90 //30leds/m, 3m.
#define DATA_PIN 9
CRGB leds[NUM_LEDS];
unsigned long timeToShow;
unsigned char colorToShow;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void setup(void) {
  Serial.begin(115200);
  if (!Serial) {
    // some boards need this because of native USB capability
  }
  Serial.println(F("RF24Network/examples/helloworld_tx/"));

  SPI.begin();
  if (!radio.begin()) {
    Serial.println(F("Radio hardware not responding!"));
    while (1) {
      // hold in infinite loop
    }
  }
  network.begin(/*channel*/ 90, /*node address*/ this_node);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  timeToShow = 0;
}

void loop() {

  handleConsole();

  network.update(); // Check the network regularly

  unsigned long now = millis();

  // If it's time to send a message, send it!
  if (now - last_sent >= interval) {
    last_sent = now;

    Serial.print("Sending...");
    cloudMsg msg;
    msg.msgType = MSG_NOOP;
    RF24NetworkHeader header(/*to node*/ other_node);
    bool ok = network.write(header, &msg, sizeof(msg));
    if (ok)
      Serial.println("ok.");
    else
      Serial.println("failed.");
  }

  if(network.available()) {
    RF24NetworkHeader header;
    cloudMsg msg;
    network.read(header, &msg, sizeof(msg));
    Serial.print("Received: ");
    printCloudMsg(&msg);
  }

  /*
  // Turn the LED on, then pause
  if(millis() >= timeToShow + 500)
  {
    Serial.print("Blink: "); 
    Serial.println(colorToShow);

    switch(colorToShow++)
    {
      case 0: leds[0] = CRGB::Red; break;
      case 1: leds[0] = CRGB::Green; break;
      case 2: leds[0] = CRGB::Blue; break;
      default: leds[0] = CRGB::White; break;
    }

    if(colorToShow >= 3) colorToShow = 0;

    FastLED.show();

    timeToShow = millis();
  }
  */

  //FastLED's built-in rainbow generator
  if(millis() >= timeToShow + 1000/60) //60fps
  {
    fill_rainbow(leds, NUM_LEDS, gHue++, 7);
    FastLED.show();
    timeToShow = millis();
  }

}

int handleConsole(void)
{
  if(Serial.available())
  {
    char ser[256+1];
    memset(ser, 0, sizeof(ser));
    Serial.readBytesUntil('\n', ser, sizeof(ser));
    char* pToken = strtok (ser," "); //get first input portion.

    if(pToken != NULL)
    {
      if (strcmp(ser,"a") == 0)
      {
        //change network address
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        int addrVal = atoi(pToken);

        Serial.print("Address -> ");
        Serial.print(addrVal);
        Serial.print(": ");

        if(network.is_valid_address(addrVal))
        {
          eeprom.netNodeAddress = addrVal;
          Serial.print("OK.\n");
          return 0;
        }
        else
        {
          Serial.print("FAIL.\n");
          return 1;
        }
      }
      else if(strcmp(ser,"b") == 0)
      {
        //change brightness
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        uint8_t brightnessVal = atoi(pToken);

        Serial.print("Brightness -> ");
        Serial.print(brightnessVal);
        Serial.print(": ");

        eeprom.ledBrightness = brightnessVal;
        FastLED.setBrightness(brightnessVal);

        if(1) //QUES: can we get a kickback from the power limiter function?
        {
          Serial.print("OK.\n");
          return 0;
        }
        else
        {
          Serial.print("FAIL.\n");
          return 1;
        }
      }
      else if(strcmp(ser,"i") == 0) // e.g. "i [network address] [#RRGGBB]"
      {
        //set network node to solid color of your choosing
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        uint16_t addrVal = atoi(pToken);
        if(!network.is_valid_address(addrVal)) return 1;

        pToken = strtok (NULL," "); //get third input portion.
        if(!pToken) return 1;
        uint32_t colorVal = strtol(pToken, 0, 16); //atol(pToken);

        Serial.print("Set node ");
        Serial.print(addrVal, OCT);
        Serial.print(" to #");
        Serial.print(colorVal, HEX);
        Serial.print(": ");
        
        cloudMsg msg;
        msg.msgType = MSG_SETCOLOR;
        memcpy(msg.msgPayload, &colorVal, sizeof(colorVal));
        RF24NetworkHeader header(addrVal);
        if(network.write(header, &msg, sizeof(msg)))
        {
          Serial.print("OK.\n");
          return 0;
        }
        else
        {
          Serial.print("FAIL.\n");
          return 1;
        }
      }
      else if(strcmp(ser,"d") == 0)
      {
        //dump storage
        Serial.print("netMaster = ");
        Serial.print(eeprom.netMaster);
        Serial.println();
        Serial.print("netNodeAddress = ");
        Serial.print(eeprom.netNodeAddress);
        Serial.println();
        Serial.print("netChannel = ");
        Serial.print(eeprom.netChannel);
        Serial.println();
        Serial.print("netNodeMax = ");
        Serial.print(eeprom.netNodeMax);
        Serial.println();
        Serial.print("ledBrightness = ");
        Serial.print(eeprom.ledBrightness);
        Serial.println();
        Serial.print("ledFps = ");
        Serial.print(eeprom.ledFps);
        Serial.println();
        Serial.print("ledPattern = ");
        Serial.print(eeprom.ledPattern);
        Serial.println();
      }
    }
  } 
  
  return -1;
}

void printCloudMsg(cloudMsg* msg)
{
  uint8_t msgBytes[sizeof(cloudMsg)];
  memcpy(&msgBytes, msg, sizeof(cloudMsg)); //JFC I can't cast this to save my life.

  for(uint8_t i = 0; i < sizeof(msgBytes); i++)
  {
    Serial.print(msgBytes[i], HEX); //print out bytes in hex
  }
  Serial.println();
}