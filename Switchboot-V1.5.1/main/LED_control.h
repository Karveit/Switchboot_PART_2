#include <Arduino.h>

extern uint32_t NEW_DOTSTAR_BRIGHTNESS;


#ifdef DOTSTAR_ENABLED
Adafruit_DotStar strip = Adafruit_DotStar(1, INTERNAL_DS_DATA, INTERNAL_DS_CLK, DOTSTAR_BGR);
#endif

void setLedColor(const char color[]) {
#ifdef DOTSTAR_ENABLED
  if (color == "red") {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, 0, 0);
  } else if (color == "green") {
    strip.setPixelColor(0, 0, NEW_DOTSTAR_BRIGHTNESS, 0);
  } else if (color == "yellow") {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
  } else if (color == "blue") {
    strip.setPixelColor(0, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
  } else if (color == "purple") {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, 0, NEW_DOTSTAR_BRIGHTNESS);
  } else if (color == "black") {
    strip.setPixelColor(0, 0, 0, 0);
  } else {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
  }
  strip.show();
#endif
}

void setPayloadColor(int payloadcolornumber) {
#ifdef DOTSTAR_ENABLED
  if (payloadcolornumber == 1) {
    strip.setPixelColor(0, 0, NEW_DOTSTAR_BRIGHTNESS, 0);
  } else if (payloadcolornumber == 2) {
    strip.setPixelColor(0, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
  } else if (payloadcolornumber == 3) {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
  } else if (payloadcolornumber == 4) {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, 0, NEW_DOTSTAR_BRIGHTNESS);
  } else if (payloadcolornumber == 5) {
    strip.setPixelColor(0, 0, NEW_DOTSTAR_BRIGHTNESS, 0);
  } else if (payloadcolornumber == 6) {
    strip.setPixelColor(0, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
  } else if (payloadcolornumber == 7) {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
  } else if (payloadcolornumber == 8) {
    strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, 0, NEW_DOTSTAR_BRIGHTNESS);
  }
  strip.show();
#endif
#ifdef DRAGONINJECTOR
  pinMode (REDLED, OUTPUT);
  pinMode (GREENLED, OUTPUT);
  pinMode (BLUELED, OUTPUT);
  if (payloadcolornumber == 1) {
    digitalWrite (REDLED, HIGH);
    digitalWrite (GREENLED, LOW);
    digitalWrite (BLUELED, HIGH);
  } else if (payloadcolornumber == 2) {
    digitalWrite (REDLED, HIGH);
    digitalWrite (GREENLED, HIGH);
    digitalWrite (BLUELED, LOW);
  } else if (payloadcolornumber == 3) {
    digitalWrite (REDLED, LOW);
    digitalWrite (GREENLED, LOW);
    digitalWrite (BLUELED, HIGH);
  } else if (payloadcolornumber == 4) {
    digitalWrite (REDLED, LOW);
    digitalWrite (GREENLED, HIGH);
    digitalWrite (BLUELED, LOW);
  } else if (payloadcolornumber == 5) {
    digitalWrite (REDLED, HIGH);
    digitalWrite (GREENLED, LOW);
    digitalWrite (BLUELED, HIGH);
  } else if (payloadcolornumber == 6) {
    digitalWrite (REDLED, HIGH);
    digitalWrite (GREENLED, HIGH);
    digitalWrite (BLUELED, LOW);
  } else if (payloadcolornumber == 7) {
    digitalWrite (REDLED, LOW);
    digitalWrite (GREENLED, LOW);
    digitalWrite (BLUELED, HIGH);
  } else if (payloadcolornumber == 8) {
    digitalWrite (REDLED, LOW);
    digitalWrite (GREENLED, HIGH);
    digitalWrite (BLUELED, LOW);
  }
#endif
}

void mode_payload_blink_led(int WRITTEN_ITEM, int ITEM_COUNT) {
  uint32_t INDICATED_ITEM;
#ifdef ONBOARD_LED
  pinMode(ONBOARD_LED, OUTPUT);
#endif
  for (INDICATED_ITEM = 0; INDICATED_ITEM < ITEM_COUNT; ++INDICATED_ITEM) {

    delayMicroseconds(PAYLOAD_FLASH_LED_OFF_TIME_SECONDS * 1000000);
    if(WRITTEN_ITEM == 1)
    {
      setPayloadColor(WRITTEN_PAYLOAD_NUMBER);
    } else setLedColor("white");
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, HIGH);
#endif
    delayMicroseconds(PAYLOAD_FLASH_LED_ON_TIME_SECONDS * 1000000);
  setLedColor("black");
#ifdef ONBOARD_LED
  digitalWrite(ONBOARD_LED, LOW);
#endif
  delayMicroseconds(PAYLOAD_FLASH_LED_OFF_TIME_SECONDS * 1000000);
  }
  setLedColor("black");
#ifdef ONBOARD_LED
  digitalWrite(ONBOARD_LED, LOW);
#endif
  delayMicroseconds(PAYLOAD_FLASH_LED_OFF_TIME_SECONDS * 1000000);
}

void confirm_led(int confirmledlength, int colorvalR, int colorvalG, int colorvalB) {
  uint32_t INDICATED_ITEM;
#ifdef ONBOARD_LED
  pinMode(ONBOARD_LED, OUTPUT);
#endif
  for (INDICATED_ITEM = 0; INDICATED_ITEM < confirmledlength; ++INDICATED_ITEM) {
    setLedColor("black");
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
    delayMicroseconds(25000);
#ifdef DOTSTAR_ENABLED
    strip.setPixelColor(0, colorvalR, colorvalG, colorvalB);
    strip.show();
#endif
#ifdef DRAGONINJECTOR
pinMode (REDLED, OUTPUT);
pinMode (GREENLED, OUTPUT);
pinMode (BLUELED, OUTPUT);
  if (colorvalR > 0){
    digitalWrite(REDLED, LOW);
  }
  if (colorvalG > 0){
    digitalWrite(GREENLED, LOW);
  }
  if (colorvalB > 0){
    digitalWrite(BLUELED, LOW);
  }
#endif
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, HIGH);
#endif
    delayMicroseconds(25000);
  }
  setLedColor("black");
#ifdef ONBOARD_LED
  digitalWrite(ONBOARD_LED, LOW);
#endif
  //.delayMicroseconds(PAYLOAD_FLASH_LED_OFF_TIME_SECONDS * 1000000);
}

void all_leds_off() {
#ifdef ONBOARD_LED
  digitalWrite(ONBOARD_LED, LOW);
#endif
  setLedColor("black");
}
