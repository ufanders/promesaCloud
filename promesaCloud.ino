/**
 * Simplest possible example of using RF24Network
 *
 * TRANSMITTER NODE
 * Every 2 seconds, send a payload to the receiver node.
 */

#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>

//nRF24L01
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
#define NUM_LEDS 1
#define DATA_PIN 9
CRGB leds[NUM_LEDS];
unsigned long timeToShow;
unsigned char colorToShow;

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

  network.update(); // Check the network regularly

  unsigned long now = millis();

  // If it's time to send a message, send it!
  if (now - last_sent >= interval) {
    last_sent = now;

    Serial.print("Sending...");
    payload_t payload = { millis(), packets_sent++ };
    RF24NetworkHeader header(/*to node*/ other_node);
    bool ok = network.write(header, &payload, sizeof(payload));
    if (ok)
      Serial.println("ok.");
    else
      Serial.println("failed.");
  }

  if(network.available()) {
    RF24NetworkHeader header;
    payload_t payload;
    network.read(header, &payload, sizeof(payload));
    Serial.print("Received packet #");
    Serial.print(payload.counter);
    Serial.print(" at ");
    Serial.println(payload.ms);
  }

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

}

