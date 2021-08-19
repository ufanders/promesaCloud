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

RF24 radio(18, 10);                  // nRF24L01(+) radio attached to Arduino Pro Micro
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

const uint16_t nodeAddress[11] = { //octal number format.
  0, //00.
  1, 2, 3, 4, 5, //01, 02, 03, 04, 05.
  0b001001, 0b010001, 0b011001, 0b100001, 0b101001 //11, 21, 31, 41, 51.
};

bool networkOk = false;

//FastLED
#include <FastLED.h>
#define NUM_LEDS 90 //30leds/m, 3m.
#define DATA_PIN 9
CRGB leds[NUM_LEDS];
unsigned long timeToShow;
unsigned long frameDelayMs;
bool autoAdvanceEnabled = true;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
CRGB sunrise[6] = {
  CRGB(0,0,0), //off.
  CRGB(62,51,85), //twilight.
  CRGB(202,62,1), //sunrise.
  CRGB(253,127,4), //daybreak.
  CRGB(121,184,175), //skylight.
  CRGB(255,242,181) //noon.
}; //sunrise color palette.

DEFINE_GRADIENT_PALETTE( sunrise_gp ) {
  0,     0,  0,  0,   //black
64,   255,  0,  0,   //red
128,   255,255,  0,   //bright yellow
255,   255,255,255 }; //full white

CRGBPalette16 palOff(CRGB(0,0,0));
CRGBPalette16 palTwilight(CRGB(62,51,85));
CRGBPalette16 palSunrise(CRGB(202,62,1));
CRGBPalette16 palDaybreak(CRGB(253,127,4));
CRGBPalette16 palSkylight(CRGB(121,184,175));
CRGBPalette16 palNoon(CRGB(255,242,181));
CRGBPalette16 palOrigin, palTarget;

//Quickpatterns
#include <quickPatterns.h>
#define TICKLENGTH  25
quickPatterns quickPatterns(leds, NUM_LEDS);
uint8_t scenesAvailable = 0;

//Application
void setup(void) {

  delay(1000);

  Serial.begin(115200);
  if (!Serial) {
    // some boards need this because of native USB capability
  }
  Serial.println(F("RF24Network/examples/helloworld_tx/"));

  SPI.begin();
  if (!radio.begin()) {
    Serial.println(F("Radio hardware not responding!"));
  } 
  else
  {
    networkOk = true;
    network.begin(/*channel*/ 90, /*node address*/ this_node);
  } 
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 5000);
  FastLED.clear(); FastLED.show();
  timeToShow = 0;
  frameDelayMs = 1000/30; //30 fps.

  quickPatterns.setTickMillis(TICKLENGTH);

 /* 
  //Scene 1: Lightning.
  quickPatterns.newScene().addPattern(new qpLightning(10))
      .singleColor(CRGB::White)
      .activatePeriodicallyEveryNTicks(100, 200)
      .stayActiveForNCycles(2, 4);

  scenesAvailable++;

  //Scene 2: dawn-dusk.
  quickPatterns.newScene().addPattern(new qpPaletteBreathe(10))
      .chooseColorFromPalette(RainbowColors_p, SEQUENTIAL)
      .changeColorEveryNActivations(2); //change color every second time this pattern is activated
  
  scenesAvailable++;
  

 //cylon of 8 pixels that cycles through the rainbow
  quickPatterns.newScene().addPattern(new qpBouncingBars(8))
    .chooseColorFromPalette(RainbowColors_p, SEQUENTIAL)
    .changeColorEveryNTicks(2);

  //Periodically toss in some lightning flashes at random places along the strand. Flash for 10x
  quickPatterns.sameScene().addPattern(new qpLightning(10))
      .singleColor(CRGB::White)
      .activatePeriodicallyEveryNTicks(100, 200)
      .stayActiveForNCycles(2, 4);

  scenesAvailable++;

  quickPatterns.playScene(0);
*/
}

void loop() {

  handleConsole();

  if(networkOk)
  {
    network.update(); // Check the network regularly

    unsigned long now = millis();

    // If it's time to send a message, send it!
    if (now - last_sent >= interval) {
      last_sent = now;

      if(networkOk)
      {
        Serial.print("Sending...");
        cloudMsg msg = {MSG_NOOP, 0xAB, 0xCD, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
        RF24NetworkHeader header(/*to node*/ other_node);
        bool ok = network.write(header, &msg, sizeof(msg));
        if (ok)
          Serial.println("ok.");
        else
          Serial.println("failed.");
      }
    }

    if(networkOk && network.available()) {
      RF24NetworkHeader header;
      cloudMsg msg;
      network.read(header, &msg, sizeof(msg));
      Serial.print("Received: ");
      printCloudMsg(&msg);
    }
  }

  if(quickPatterns.draw())
      FastLED.show();

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
      else if(strcmp(ser,"s") == 0)
      {
        //change scene
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        uint8_t sceneToSelect = atoi(pToken);

        Serial.print("Scene ");
        Serial.print(" -> ");
        Serial.print(sceneToSelect);
        Serial.print(": ");

        if(sceneToSelect <= scenesAvailable)
        {
          quickPatterns.playScene(sceneToSelect-1);
          Serial.print("OK.\n");
          return 0;
        }
        else
        {
          Serial.print("FAIL.\n");
          return 1;
        }
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
    uint8_t hn = (msgBytes[i] >> 4) & 0x0F;
    uint8_t ln = msgBytes[i] & 0x0F;
    
    Serial.print(hn, HEX); //print out high nybble in hex.
    Serial.print(ln, HEX); //print out low nybble in hex.
  }
  Serial.println();
}
