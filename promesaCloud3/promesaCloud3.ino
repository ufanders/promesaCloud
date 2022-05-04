//M5Stack
#include <SPI.h>
#define PLAT_M5STACK

#ifdef PLAT_M5STACK
//#include <M5Stack.h>

#endif

// Define ALTERNATE_PINS to use non-standard GPIO pins for SPI bus

#ifdef ALTERNATE_PINS
  #define VSPI_MISO   2
  #define VSPI_MOSI   4
  #define VSPI_SCLK   0
  #define VSPI_SS     33

  #define HSPI_MISO   26
  #define HSPI_MOSI   27
  #define HSPI_SCLK   25
  #define HSPI_SS     32
#else
  #define VSPI_MISO   MISO
  #define VSPI_MOSI   MOSI
  #define VSPI_SCLK   SCK
  #define VSPI_SS     SS

  #define HSPI_MISO   12
  #define HSPI_MOSI   13
  #define HSPI_SCLK   14
  #define HSPI_SS     15
#endif

static const int spiClk = 1000000; // 1 MHz

//nRF24L01
#include <RF24.h>
#include <RF24Network.h>

#define NODEADDRESSINDEX_MAX 11

#ifdef PLAT_M5STACK
RF24 radio(21, SS, spiClk); //CEpin, CSpin
void IRAM_ATTR isrRF24(void);
#else
RF24 radio(18, 10); // nRF24L01(+) radio attached to Arduino Pro Micro
void isrRF24(void)
#endif
RF24Network network(radio);          // Network uses that radio
//uint16_t this_node = 00;       // Address of our node in Octal format
uint16_t other_node = 01;      // Address of the other node in Octal format
const unsigned long interval = 2000; // How often (in ms) to send 'hello world' to the other unit
unsigned long last_sent;             // When did we last send?
unsigned long packets_sent;          // How many have we sent already
bool networkOk = false;
uint16_t nodesPresent[NODEADDRESSINDEX_MAX];

const uint16_t nodeAddress[NODEADDRESSINDEX_MAX] = { //octal number format.
  0, //00.
  1, 2, 3, 4, 5, //01, 02, 03, 04, 05.
  0b001001, 0b010001, 0b011001, 0b100001, 0b101001 //11, 21, 31, 41, 51.
};

bool radioInt = false;
bool tx_ok = false;
bool tx_fail = false;
bool rx_ready = false;

//Promesa Cloud
#ifdef PLAT_M5STACK
#else
#include <EEPROM.h>
#endif

#define CLOUDSTORAGE_MAGIC 0xA1B2C3D4

struct cloudStorage
{
  uint32_t magic;
  bool netMaster;
  uint16_t netNodeAddress;
  uint8_t netChannel;
  uint8_t netNodeMax;
  uint8_t ledBrightness;
  uint8_t ledFps;
  uint8_t ledPattern;
  CRGB ledColor;
};

#ifdef PLAT_M5STACK
IRAM_ATTR cloudStorage eeprom;
#else
cloudStorage eeprom;
#endif

struct cloudMsg
{
  uint8_t msgType;
  uint8_t msgPayload[16];
};

//TODO: consider making MSBit in msgType indicative of a response.
enum msgType {MSG_NOOP, MSG_RESP, MSG_SETCOLOR, MSG_SETPATTERN, MSG_SETADDR, MSG_SETBRIGHT, MSG_PASSTHROUGH, MSG_MAX};
char ser[256+1];
unsigned short charIndex;
bool resetCharReception = true;

void printCloudMsg(cloudMsg* msg);
int handleConsole(void);
int handleMessage(RF24NetworkHeader*, cloudMsg*);
bool cmdSetBrightness(unsigned char addrDst, unsigned char brightnessVal);
bool cmdSetColor(unsigned char addrDst, uint32_t colorVal);

//FastLED
#include <FastLED.h>

#ifdef PLAT_M5STACK
#define DATA_PIN 2
#else
#endif

#define NUM_LEDS 90 //30leds/m, 3m.

CRGB leds[NUM_LEDS];
unsigned long timeToShow;
unsigned char colorToShow;
unsigned long frameDelayMs;
CRGB staticColor = CRGB(255,0,0);

enum patternType {PAT_NONE, PAT_FADE, PAT_NOISE, PAT_LIGHTNING, PAT_STATIC, PAT_RAINBOW, PAT_MAX};
uint8_t patternToExec = PAT_FADE;
bool patternChanged = true;
bool autoAdvanceEnabled = true;
int changePattern(uint8_t);
bool gUpdated = false;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
CRGB sunrise[6] = {
  CRGB(0,0,0), //off.
  CRGB(62,51,85), //twilight.
  CRGB(202,62,1), //sunrise.
  CRGB(253,127,4), //daybreak.
  CRGB(121,184,175), //skylight.
  CRGB(255,242,181) //noon.
}; //sunrise color palette.

CRGBPalette16 palOff(CRGB(0,0,0));
CRGBPalette16 palTwilight(CRGB(62,51,85));
CRGBPalette16 palSunrise(CRGB(202,62,1));
CRGBPalette16 palDaybreak(CRGB(253,127,4));
CRGBPalette16 palSkylight(CRGB(121,184,175));
CRGBPalette16 palNoon(CRGB(255,242,181));
CRGBPalette16 palOrigin, palTarget;

void sinelon();
void rainbow();
void lightning();

void noiseLoop();
CRGBPalette16 currentPalette(CRGB::Black);
CRGBPalette16 targetPalette(ForestColors_p);
unsigned int noiseLoopFrames = 0;

//Buttons
#include <ButtonDebounce.h>

ButtonDebounce buttonPwr(37, 100);
ButtonDebounce buttonPat(38, 100);
ButtonDebounce buttonBrt(39, 100);
bool buttonPwrIsPressed = false;
bool buttonPatIsPressed = false;
bool buttonBrtIsPressed = false;
enum patSteps {PATSTEP_OFF, PATSTEP_RED, PATSTEP_GRN, PATSTEP_BLU, \
                PATSTEP_PUR, PATSTEP_YEL, PATSTEP_CYN };
unsigned char patStep;
bool pwrToggle;
uint32_t gColor;

void handleButtons(void);

//Power
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP_DEEP 5*uS_TO_S_FACTOR //5 seconds.
#define DEEPSLEEP_GPIO_PINMASK (GPIO_NUM_39 | GPIO_NUM_38 | GPIO_NUM_37 | GPIO_NUM_22) 

//Application
void setup(void) {

  delay(2000);
  Serial.begin(115200);
  if (!Serial) {
    // some boards need this because of native USB capability
  }
  Serial.println(F("RF24Network/examples/helloworld_tx/"));

#ifdef PLAT_M5STACK
  /*
  M5.begin();  //Init M5Core.  初始化 M5Core
  M5.Power.begin(); //Init Power module.  初始化电源模块
  M5.Lcd.setTextColor(YELLOW);  //Set the font color to yellow.  设置字体颜色为黄色
  M5.Lcd.setTextSize(2);  //Set the font size.  设置字体大小为2
  M5.Lcd.setCursor(65, 10); //Move the cursor position to (65, 10).  移动光标位置到 (65, 10) 处
  M5.Lcd.println("Button example"); //The screen prints the formatted string and wraps the line.  输出格式化字符串并换行
  M5.Lcd.setCursor(3, 35);
  M5.Lcd.println("Press button B for 700ms");
  M5.Lcd.println("to clear screen.");
  M5.Lcd.setTextColor(RED);
  */

  //disable M5Stack speaker clicking noise.
  dacWrite(25,0);

  //TODO: load NV memory values from SPIFFS.
  //populate defaults.
  eeprom.magic = CLOUDSTORAGE_MAGIC;
  eeprom.netMaster = false;
  eeprom.netNodeAddress = 0x00;
  eeprom.netChannel = 90;
  eeprom.netNodeMax = NODEADDRESSINDEX_MAX;
  eeprom.ledBrightness = 128;
  eeprom.ledFps = 30;
  eeprom.ledPattern = PAT_NONE;
#else
  //load NV memory values from EEPROM.
  uint8_t* eepromPtr = (uint8_t*)&eeprom;
  for(uint8_t i = 0; i < sizeof(eeprom); i++)
  {
    eepromPtr[i] = EEPROM.read(i);
  }

  Serial.print("EEPROM.magic = 0x");
  Serial.print(eeprom.magic, HEX);
  Serial.println();

  if(eeprom.magic != CLOUDSTORAGE_MAGIC)
  {
    //populate defaults if necessary.
    eeprom.magic = CLOUDSTORAGE_MAGIC;
    eeprom.netMaster = false;
    eeprom.netNodeAddress = 0x00;
    eeprom.netChannel = 90;
    eeprom.netNodeMax = NODEADDRESSINDEX_MAX;
    eeprom.ledBrightness = 128;
    eeprom.ledFps = 30;
    eeprom.ledPattern = PAT_FADE;

    for(uint8_t i = 0; i < sizeof(eeprom); i++)
    {
       EEPROM.write(i, eepromPtr[i]); //save to EEPROM.
    }

    Serial.print("EEPROM.magic = 0x");
    Serial.print(eeprom.magic, HEX);
    Serial.println();
  }
#endif

#ifdef PLAT_M5STACK
  SPI.setDataMode(SPI_MODE2);
#else
#endif

  SPI.begin();
  if (!radio.begin())
  {
    Serial.println(F("Radio hardware not responding!"));
  }
  else
  {
    radio.setRadiation(RF24_PA_MAX, RF24_1MBPS); //+3dB
    networkOk = true;
    network.begin(eeprom.netChannel, eeprom.netNodeAddress);
    
#ifdef PLAT_M5STACK
    radio.maskIRQ(false, false, false);
    pinMode(22, INPUT);
    attachInterrupt(digitalPinToInterrupt(22), isrRF24, FALLING);
    radioInt = false;
#else
#endif
  }

  memset(nodesPresent, 0, sizeof(nodesPresent));

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 5000);
  FastLED.setBrightness(eeprom.ledBrightness);
  timeToShow = 0;
  frameDelayMs = 1000/eeprom.ledFps;
  patternToExec = eeprom.ledPattern;

  patStep = PATSTEP_OFF;
  pwrToggle = false;
}

void loop() {

  handleConsole();

  handleButtons();

  if(radioInt)
  {
    radio.whatHappened(tx_ok, tx_fail, rx_ready); //get IRQ status flags

    if(networkOk)
    {
      network.update(); // Check the network regularly

      if(network.available()) {
        RF24NetworkHeader header;
        cloudMsg msg;
        memset(&msg, 0, sizeof(msg));
        network.read(header, &msg, sizeof(msg));
        Serial.print("<- (Node ");
        Serial.print(header.from_node);
        Serial.print("):");
        printCloudMsg(&msg);
        handleMessage(&header, &msg);
      }
    }

    radioInt = false;
  }

  if(millis() >= timeToShow + frameDelayMs) //30fps
  {
    patternExec();
    if(gUpdated)
    {
      FastLED.show();
      gUpdated = false;
    }
    timeToShow = millis();
  }
}

int handleConsole(void)
{
  static unsigned short charAvailable;
  static bool cmdReceived;
  
  if(resetCharReception)
  {
    memset(ser, 0, sizeof(ser));
    charIndex = 0;
    cmdReceived = false;

    //Serial.println("Console reset.");
    resetCharReception = false;
  }

  charAvailable = Serial.available();
  
  if(charAvailable >= 1)
  {
    charIndex += Serial.readBytes(&ser[charIndex], charAvailable);

    if(ser[charIndex-1] == '\r') //|| ser[charIndex] == '\n' || ser[charIndex] == '\f')
    {
      ser[--charIndex] = 0; //string terminator.
      cmdReceived = true;

      /*
      Serial.print("Received: ");
      Serial.write(ser, charIndex);
      */
      
      Serial.println(); //echo.
      
      resetCharReception = true;
    }
    else if(ser[charIndex-1] == '\n')
    {
      ser[--charIndex] = 0; //ignore LF
    }
    else
    {
      Serial.print(ser[charIndex-1]);
    }
  }

  if(cmdReceived == true)
  {
    char* pToken = strtok (ser," "); //get first input portion.

    if(pToken != NULL)
    {
      if (strcmp(pToken,"a") == 0) // e.g. "a [node address index]"
      {
        //change local network address
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        int addrVal = atoi(pToken);
        if((addrVal < 0) || (addrVal >= NODEADDRESSINDEX_MAX))
        {
          Serial.print("Address index must be between 0 and ");
          Serial.print(NODEADDRESSINDEX_MAX);
          Serial.println(".");
          return 1;
        } 

        Serial.print("nodeAddress[");
        Serial.print(addrVal);
        Serial.print("] (");
        Serial.print(nodeAddress[addrVal], OCT);
        Serial.print(") -> ");
        Serial.print(": ");

        if(network.is_valid_address(nodeAddress[addrVal]))
        {
          eeprom.netNodeAddress = nodeAddress[addrVal];
          network.begin(eeprom.netNodeAddress);
          Serial.print("OK.\n");
          return 0;
        }
        else
        {
          Serial.print("FAIL.\n");
          return 1;
        }
      }
      else if(strcmp(pToken,"b") == 0) // e.g. "b [network address] [0 to 255]"
      {
        //change brightness
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        uint16_t addrVal = atoi(pToken);

        pToken = strtok (NULL," "); //get third input portion.
        if(!pToken) return 1;
        uint8_t brightnessVal = atoi(pToken);

        bool ok = cmdSetBrightness(addrVal,brightnessVal);

        if(ok)
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
      else if(strcmp(pToken,"c") == 0) // e.g. "c [network address] [RRGGBB]"
      {
        //set network node to solid color of your choosing
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        uint16_t addrVal = atoi(pToken);

        pToken = strtok (NULL," "); //get third input portion.
        if(!pToken) return 1;
        uint32_t colorVal = strtol(pToken, 0, 16); //atol(pToken);

        bool ok = cmdSetColor(addrVal, colorVal);

        if(ok)
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
      else if(strcmp(pToken,"d") == 0) // e.g. "d"
      {
        //dump local storage
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

        return 0;
      }
      else if(strcmp(pToken,"p") == 0) // e.g. "p [network address] [0 to (PAT_MAX-1)]"
      {
        //change pattern
        pToken = strtok (NULL," "); //get second input portion.
        if(!pToken) return 1;
        uint16_t addrVal = atoi(pToken);

        pToken = strtok (NULL," "); //get third input portion.
        if(!pToken) return 1;
        uint8_t patternToSelect = atoi(pToken);

        Serial.print("Pattern ");
        Serial.print(" -> ");
        Serial.print(patternToSelect);
        Serial.print(": ");

        RF24NetworkHeader header(nodeAddress[addrVal]);
        header.from_node = eeprom.netNodeAddress;
        cloudMsg msg;
        msg.msgType = MSG_SETPATTERN;
        msg.msgPayload[0] = patternToSelect;
        bool ok = false;

        if(nodeAddress[addrVal] == eeprom.netNodeAddress)
        {
          handleMessage(&header, &msg);
          ok = true;
        }
        else
        {
          if(networkOk && network.is_valid_address(nodeAddress[addrVal]) 
            && network.write(header, &msg, sizeof(msg)))
            {
              ok = true;
            }
        }

        if(ok)
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
      else if(strcmp(pToken,"s") == 0)
      {
#ifdef PLAT_M5STACK
//TODO: save settings to SPIFFS.
#else
        //save settings to EEPROM.
        uint8_t* eepromPtr = (uint8_t*)&eeprom;
        for(uint8_t i = 0; i < sizeof(eeprom); i++)
        {
          EEPROM.write(i, eepromPtr[i]); //save to EEPROM.
        }

        Serial.println("Settings saved.");
        return 0;
#endif
      }
      else if(strcmp(pToken,"r") == 0)
      {
        RF24NetworkHeader header;
        header.from_node = eeprom.netNodeAddress;
        cloudMsg msg;
        msg.msgType = MSG_NOOP;
        bool ok = false;

        //scan network for nodes.
        for(uint8_t i = 0; i < NODEADDRESSINDEX_MAX; i++)
        {
          if(nodeAddress[i] != eeprom.netNodeAddress)
          {
            header.to_node = nodeAddress[i];
            
            if(networkOk && network.is_valid_address(nodeAddress[i]) 
              && network.write(header, &msg, sizeof(msg)))
              {
                ok = true;
              }
          }
        }

        Serial.println("Pings sent.");
        return 0;
      }
      
    }
  }
  
  return -1;
}

#ifdef PLAT_M5STACK
void IRAM_ATTR isrRF24(void)
#else
void isrRF24(void)
#endif
{
  //Serial.println("INT");
  radioInt = true;
}

int handleMessage(RF24NetworkHeader* hdr, cloudMsg* msg)
{
  int ret = 1; //success or failure.
  bool localMsg = false; //did the message originate from the console or a network node?
  bool suppressResponse = false; //need to guard against response loops.
  CRGB color;
  bool nodeFound = false;

  if(hdr->from_node == eeprom.netNodeAddress) localMsg = true; //print response to local console, not source network node.

  switch(msg->msgType)
  {
    case MSG_SETCOLOR:
    //memcpy(msg->msgPayload, &color, sizeof(CRGB));
    staticColor.r = msg->msgPayload[0];
    staticColor.g = msg->msgPayload[1];
    staticColor.b = msg->msgPayload[2];
    Serial.print("Color = #");
    Serial.print(staticColor.r, HEX);
    Serial.print(staticColor.g, HEX);
    Serial.print(staticColor.b, HEX);
    Serial.println();
    ret = changePattern(PAT_STATIC);
    break;

    case MSG_NOOP:
    ret = 0;
    break;

    case MSG_SETPATTERN:
    ret = changePattern(msg->msgPayload[0]);
    if(!ret) eeprom.ledPattern = msg->msgPayload[0];
    break;

    case MSG_SETBRIGHT:
    eeprom.ledBrightness = msg->msgPayload[0];
    FastLED.setBrightness(eeprom.ledBrightness);
    gUpdated = true;
    ret = 0;
    break;

    case MSG_SETADDR:
    break;

    case MSG_PASSTHROUGH: //arbitrary text
    break;

    case MSG_RESP:
    ret = 0;
    suppressResponse = true;
    nodeFound = false;

    //scan for presence of responding node in nodesPresent[] list.
    for(uint8_t i = 0; i < NODEADDRESSINDEX_MAX; i++)
    {
      if(nodesPresent[i] == hdr->from_node)
      {
        nodeFound = true;
        i = NODEADDRESSINDEX_MAX;
        //break;
      }
    }

    //if responding node not currently in list, add to list.
    if(!nodeFound)
    {
      for(uint8_t i = 0; i < NODEADDRESSINDEX_MAX; i++)
      {
        if(nodesPresent[i] == 0)
        {
          nodesPresent[i] = hdr->from_node;
          i = NODEADDRESSINDEX_MAX;
          //break;
        }
      }
    }

    break;

    default:
    break;
  }

  if(localMsg)
  {
    Serial.print("ret: ");
    Serial.println(ret);
  }
  else
  {
    if(!suppressResponse)
    {
      cloudMsg msgOut;
      memset(&msgOut, 0, sizeof(msgOut));
      msgOut.msgType = MSG_RESP;
      msgOut.msgPayload[0] = msg->msgType;
      memcpy(&msgOut.msgPayload[1], &ret, sizeof(ret));
      RF24NetworkHeader header(hdr->from_node);
      bool ok = network.write(header, &msgOut, sizeof(msgOut));
      Serial.print("-> ret: ");
      Serial.print(ret);
      if(ok) Serial.println(" [OK]");
      else Serial.println(" [FAIL]");
    }
  }

  return ret;
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

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue++, 7);
}

//Lightning
#define FLASHES 8
#define FREQUENCY 2 // delay between strikes
unsigned int dimmer = 1;
void lightning()
{
  uint8_t i;
  uint8_t length = random8(5,90); //physical length of strike.
  uint8_t indexStart = random8(0,80); //physical position of strike.
  uint8_t indexEnd = indexStart + length-1; //clip to array length.

  if(indexEnd >= NUM_LEDS)
  {
    indexEnd = NUM_LEDS-1;
  }

  for (int flashCounter = 0; flashCounter < random8(3,FLASHES); flashCounter++)
  {
    if(flashCounter == 0) dimmer = 5;     // the brightness of the leader is scaled down by a factor of 5
    else dimmer = random8(1,3);           // return strokes are brighter than the leader
    
    for(i = indexStart; i<=indexEnd; i++)
    {
      leds[i] = CHSV(255, 0, 255/dimmer);
    }
    FastLED.show();

    delay(random8(4,10));                 // each flash only lasts 4-10 milliseconds

    for(i = indexStart; i<=indexEnd; i++)
    {
      leds[i] = CHSV(255, 0, 0);
    }
    FastLED.show();
    
    if (flashCounter == 0) delay (150);   // longer delay until next flash after the leader
    delay(50+random8(100));               // shorter delay between strikes  
  }
  delay(random8(FREQUENCY)*100);          // delay between strikes
}

uint8_t amountOfP2 = 0;
uint8_t stage = 0;
bool stageChanged = true;
CRGB c1, c2;
void fadeThrough()
{
  if(stageChanged)
  {

    Serial.print("Stage ");
    Serial.println(stage);

     switch(stage)
      {
        case 0:
        c1 = CRGB::Black; c2 = CRGB::Navy; break;

        case 1:
        c1 = c2; c2 = CRGB::Maroon; break;

        case 2:
        c1 = c2; c2 = CRGB::DarkOrange; break;

        case 3:
        c1 = c2; c2 = CRGB::CornflowerBlue; break;

        case 4:
        c1 = c2; c2 = CRGB::LightSkyBlue; break;

        case 5:
        c1 = c2; c2 = CRGB::DarkSlateBlue; break;

        case 6:
        c1 = c2; c2 = CRGB(10, 10, 10); break;

        case 7:
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();
        lightning();

        case 8:
        if(autoAdvanceEnabled) changePattern(PAT_FADE); //advance.
        else { stage = 0; stageChanged = true;}
        break;

        default:
        break;
      }

      stageChanged = false;
  }
 
  CRGB outColor = blend(c1, c2, amountOfP2);

  amountOfP2++;

  if(amountOfP2 == 255)
  {
    stage++;
    if(stage > 8) stage = 0;
    stageChanged = true;
    amountOfP2 = 0;
  }

  fill_solid(leds, NUM_LEDS, outColor);
}

void patternExec()
{
  if(patternChanged)
  {
    Serial.print("Pattern ");
    Serial.println(patternToExec);

    FastLED.clear();
    gUpdated = true;

    switch(patternToExec) //init pattern.
    {
      case PAT_FADE:
      stage = 0;
      stageChanged = true;
      amountOfP2 = 0;
      break;

      case PAT_NOISE:
      noiseLoopFrames = 0;
      break;

      case PAT_STATIC:
      fill_solid(leds, NUM_LEDS, staticColor);
      gUpdated = true;
      break;

      case PAT_RAINBOW:
      gHue = 0;
      break;

      case PAT_LIGHTNING:
      default:
      break;
    }
    
    patternChanged = false;
  }

  switch(patternToExec) //run pattern loop.
  {
    case PAT_FADE:
    fadeThrough();
    gUpdated = true;
    break;

    case PAT_NOISE:
    noiseLoop();
    gUpdated = true;
    break;

    case PAT_LIGHTNING:
    lightning();
    break;

    case PAT_RAINBOW:
    rainbow();
    gUpdated = true;
    break;

    default:
    break;
  }
}

int changePattern(uint8_t nextPattern)
{
  if(nextPattern >= PAT_MAX) return 1;
  else
  {
    patternToExec = nextPattern;
    patternChanged = true;
  }

  return 0;
}

void noiseLoop() {

  fillnoise8();  

  //EVERY_N_MILLIS(10) {
    nblendPaletteTowardPalette(currentPalette, targetPalette, 48);          // Blend towards the target palette over 48 iterations.
  //}
}

void fillnoise8() {

  #define scale 30                                                          // Don't change this programmatically or everything shakes.
  
  for(int i = 0; i < NUM_LEDS; i++) {                                       // Just ONE loop to fill up the LED array as all of the pixels change.
    uint8_t index = inoise8(i*scale, millis()/10+i*scale);                   // Get a value from the noise function. I'm using both x and y axis.
    leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);    // With that value, look up the 8 bit colour palette value and assign it to the current LED.
  }

}

void handleButtons(void)
{
#ifdef PLAT_M5STACK
/*
  M5.update(); //Read the press state of the key.  读取按键 A, B, C 的状态
  if (M5.BtnA.wasReleased() || M5.BtnA.pressedFor(1000, 200)) {
    M5.Lcd.print('A');
  } else if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(1000, 200)) {
    M5.Lcd.print('B');
  } else if (M5.BtnC.wasReleased() || M5.BtnC.pressedFor(1000, 200)) {
    M5.Lcd.print('C');
  } else if (M5.BtnB.wasReleasefor(700)) {
    M5.Lcd.clear(WHITE);  // Clear the screen and set white to the background color.  清空屏幕并将白色设置为底色
    M5.Lcd.setCursor(0, 0);
  }
  */

  buttonPwr.update();
  if(buttonPwr.state() == LOW && !buttonPwrIsPressed)
  {
    //TODO: something
    Serial.println("buttonPwrIsPressed");

    pwrToggle ^= 1; //toggle power, actually just switch between off and the last color used.

    uint32_t c;
    if(pwrToggle) c = gColor;
    else c = 0;

    for(unsigned char i = 1; i<4; i++)
    {
      cmdSetColor(i, c);
    }

    buttonPwrIsPressed = true;
  }
  else if(buttonPwr.state() == HIGH && buttonPwrIsPressed)
  {
    buttonPwrIsPressed = false;
  }

  buttonPat.update();
  if(buttonPat.state() == LOW && !buttonPatIsPressed)
  {
    //TODO: something
    Serial.println("buttonPatIsPressed");
    
    patStep++;
    if(patStep > PATSTEP_CYN) patStep = PATSTEP_RED;

    switch(patStep)
    {
      case PATSTEP_RED:
        gColor = 0xFF0000;
        break;

      case PATSTEP_GRN:
        gColor = 0x00FF00;
        break;

      case PATSTEP_BLU:
        gColor = 0x0000FF;
        break;

      case PATSTEP_PUR:
        gColor = 0xFF00FF;
        break;

      case PATSTEP_YEL:
        gColor = 0xFFFF00;
        break;

      case PATSTEP_CYN:
        gColor = 0x00FFFF;
        break;

      case PATSTEP_OFF:
      default:
        gColor = 0;
        break;
    }

    //TODO: change pattern.
    for(unsigned char i = 1; i<4; i++)
    {
      cmdSetColor(i, gColor);
    }

    buttonPatIsPressed = true;
  }
  else if(buttonPat.state() == HIGH && buttonPatIsPressed)
  {
    buttonPatIsPressed = false;
  }

  buttonBrt.update();
  if(buttonBrt.state() == LOW && !buttonBrtIsPressed)
  {
    //TODO: something
    Serial.println("buttonBrtIsPressed");

    eeprom.ledBrightness += 0x1F; //8 steps out of 256 values.
    for(unsigned char i = 1; i<4; i++)
    {
      cmdSetBrightness(i, eeprom.ledBrightness);
    }

    buttonBrtIsPressed = true;
  }
  else if(buttonBrt.state() == HIGH && buttonBrtIsPressed)
  {
    buttonBrtIsPressed = false;
  }
#else
#endif

}

bool cmdSetColor(unsigned char addrDst, uint32_t colorVal)
{
  CRGB color;
  color.r = (colorVal >> 16) & 0xFF;
  color.g = (colorVal >> 8) & 0xFF;
  color.b = (colorVal & 0xFF);

  Serial.print("Set node ");
  Serial.print(addrDst);
  Serial.print(" (");
  Serial.print(nodeAddress[addrDst], OCT);
  Serial.print(") to #");
  Serial.print(colorVal, HEX);
  Serial.print(": ");

  RF24NetworkHeader header(nodeAddress[addrDst]);
  header.from_node = eeprom.netNodeAddress;
  cloudMsg msg;
  msg.msgType = MSG_SETCOLOR;
  //memcpy(msg.msgPayload, &color, sizeof(CRGB)); //not working.
  msg.msgPayload[0] = color.r;
  msg.msgPayload[1] = color.g;
  msg.msgPayload[2] = color.b;
  bool ok = false;

  if(nodeAddress[addrDst] == eeprom.netNodeAddress)
  {
    handleMessage(&header, &msg);
    ok = true;
  }
  else
  {
    if(networkOk && network.is_valid_address(nodeAddress[addrDst]) 
      && network.write(header, &msg, sizeof(msg)))
      {
        ok = true;
      }
  }

  return ok;
}

bool cmdSetBrightness(unsigned char addrDst, unsigned char brightnessVal)
{
  RF24NetworkHeader header(nodeAddress[addrDst]);
  header.from_node = eeprom.netNodeAddress;
  cloudMsg msg;
  msg.msgType = MSG_SETBRIGHT;
  msg.msgPayload[0] = brightnessVal;
  bool ok = false;

  Serial.print("Brightness -> ");
  Serial.print(brightnessVal);
  Serial.print(": ");

  if(nodeAddress[addrDst] == eeprom.netNodeAddress)
  {
    handleMessage(&header, &msg);
    ok = true;
  }
  else
  {
    if(networkOk && network.is_valid_address(nodeAddress[addrDst]) 
      && network.write(header, &msg, sizeof(msg)))
      {
        ok = true;
      }
  }

  if(ok)
  {
    Serial.print("OK.\n");
  }
  else
  {
    Serial.print("FAIL.\n");
  }

  return ok;
}

int device_prepDeepSleep(void)
{
  Serial.println("Entering deep sleep...");

  radio.powerDown(); //shut down NRF24L01 radio.

  Serial.println("Sleeping.");
  Serial.flush();

  esp_sleep_enable_ext0_wakeup(DEEPSLEEP_GPIO_PINMASK, LOW);
  //NOTE: we only want to exit deep sleep when we see human activity again via 
  //the PIR sensor and not the wakeup timer.
  if(1) esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  else esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_DEEP);

  esp_deep_sleep_start();

  radio.powerUp(); //start up NRF24L01 radio.

  return 0;
}