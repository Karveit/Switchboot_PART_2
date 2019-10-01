#include <Arduino.h>

extern void confirm_led(int confirmledlength, int colorvalR, int colorvalG, int colorvalB);
extern void setLedColor(const char color[]);
extern void all_leds_off();
extern void setPayloadColor(int payloadcolornumber);
extern void full_eeprom_reset();
extern void partial_eeprom_reset();
extern void set_dotstar_brightness();
extern void disable_chip();
extern void wakeup();

#define MASTER_TIMER 16
#define TRIGGER1 5
#define TRIGGER2 8
#define TRIGGER3 12
#define TRIGGER4 16

#define OFF1 300
#define ON2 50
#define OFF3 500
#define RAPID_BLINK_SPEED_OFF 50
#define RAPID_BLINK_SPEED_ON 10

void writetoflash(uint32_t UNWRITTEN_PAYLOAD_NUMBER, uint32_t UNWRITTEN_MODE_NUMBER) {
  if (UNWRITTEN_PAYLOAD_NUMBER != 0) {
    if (WRITTEN_PAYLOAD_NUMBER != UNWRITTEN_PAYLOAD_NUMBER) {
      EEPROM_PAYLOAD_NUMBER.write(UNWRITTEN_PAYLOAD_NUMBER);
    }
  }

  if (UNWRITTEN_MODE_NUMBER != 0) {
    if (WRITTEN_MODE_NUMBER != UNWRITTEN_MODE_NUMBER) {
      EEPROM_MODE_NUMBER.write(UNWRITTEN_MODE_NUMBER);
    }
  }
  confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
  NVIC_SystemReset();
  return;
}

int not_pressed_pause(uint32_t milliseconds, bool flash_write, uint32_t UNWRITTEN_PAYLOAD_NUMBER, uint32_t UNWRITTEN_MODE_NUMBER)
{
  uint32_t i = 0; uint32_t res;
  while ( i < (milliseconds * 10))
  {
    if (digitalRead(VOLUP_STRAP_PIN) == 1 && flash_write) {
      writetoflash(UNWRITTEN_PAYLOAD_NUMBER, UNWRITTEN_MODE_NUMBER);
    } else res = 0;
    if (digitalRead(VOLUP_STRAP_PIN) == 1 && !flash_write) {
      break;
      res = 1;
    } else res = 0;
    delayMicroseconds(100);
    ++i;
  }
  return res;
}

int pressed_pause(uint32_t milliseconds, bool flash_write, uint32_t UNWRITTEN_PAYLOAD_NUMBER, uint32_t UNWRITTEN_MODE_NUMBER)
{
  uint32_t i = 0; uint32_t res;
  while ( i < (milliseconds * 10))
  {
    if (digitalRead(VOLUP_STRAP_PIN) == 0 && flash_write) {
      writetoflash(UNWRITTEN_PAYLOAD_NUMBER, UNWRITTEN_MODE_NUMBER);
    } else res = 0;
    if (digitalRead(VOLUP_STRAP_PIN) == 0 && !flash_write) {
      break;
      res = 1;
    } else res = 0;
    delayMicroseconds(100);
    ++i;
  }
  return res;
}

void rapid_blink (const char colour[])
{

  uint32_t i = 0;

  while (i < 21) {
#ifdef DOTSTAR_ENABLED
    if (colour == "red") {
      strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, 0, 0);
    } else if (colour == "green") {
      strip.setPixelColor(0, 0, NEW_DOTSTAR_BRIGHTNESS, 0);
    } else if (colour == "yellow") {
      strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
    } else if (colour == "blue") {
      strip.setPixelColor(0, 0, 0, NEW_DOTSTAR_BRIGHTNESS);
    } else if (colour == "black") {
      strip.setPixelColor(0, 0, 0, 0);
    } else if (colour == "white") {
      strip.setPixelColor(0, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
    }
    strip.show();
#endif
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, HIGH);
#endif
    
    not_pressed_pause(RAPID_BLINK_SPEED_ON, false, 0, 0);
#ifdef DOTSTAR_ENABLED   
    strip.setPixelColor(0, 0, 0, 0);
    strip.show();
#endif
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
    
    not_pressed_pause(RAPID_BLINK_SPEED_OFF, false, 0, 0);
    ++i;
  }

}

void set_dotstar_brightness() {
#ifdef DOTSTAR_ENABLED
  delayMicroseconds(1000000);

  int currentfade = 0; int currentbrightness = 0; int i = 0;
  while (currentfade < 3) {
    for (i = 0; i < 256; ++i) {
      if (digitalRead(VOLUP_STRAP_PIN) == LOW) {
        EEPROM_DOTSTAR_BRIGHTNESS.write(i);
        confirm_led(20, i, i, i);
        NVIC_SystemReset();
      }
      strip.setPixelColor(0, i, i, i);
      strip.show();
      delayMicroseconds(10000);
    }
    for (i = 255; i > 1; --i) {
      if (digitalRead(VOLUP_STRAP_PIN) == LOW) {
        EEPROM_DOTSTAR_BRIGHTNESS.write(i);
        confirm_led(20, i, i, i);
        NVIC_SystemReset();
      }
      strip.setPixelColor(0, i, i, i);
      strip.show();
      delayMicroseconds(10000);
    }
    ++currentfade;
  }
  NVIC_SystemReset();
#endif
}

void partial_eeprom_reset() {
  uint32_t UNWRITTEN_MODE_NUMBER = DEFAULT_MODE;
  uint32_t UNWRITTEN_PAYLOAD_NUMBER = DEFAULT_PAYLOAD;
  confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, 0);
  writetoflash(UNWRITTEN_PAYLOAD_NUMBER, UNWRITTEN_MODE_NUMBER);
}

void full_eeprom_reset() {
  EEPROM_INITIAL_WRITE = !EEPROM_INITIAL_WRITE;
  uint32_t UNWRITTEN_MODE_NUMBER = !DEFAULT_MODE;
  uint32_t UNWRITTEN_PAYLOAD_NUMBER = !UNWRITTEN_PAYLOAD_NUMBER;
  USB_STRAP_TEST = !USB_STRAP_TEST;
  VOLUP_STRAP_TEST = !VOLUP_STRAP_TEST;
  JOYCON_STRAP_TEST = !JOYCON_STRAP_TEST;
  EEPROM_EMPTY.write(EEPROM_INITIAL_WRITE);
  EEPROM_USB_REBOOT_STRAP.write(USB_STRAP_TEST);
  EEPROM_VOL_CONTROL_STRAP.write(VOLUP_STRAP_TEST);
  EEPROM_JOYCON_CONTROL_STRAP.write(JOYCON_STRAP_TEST);
  EEPROM_DOTSTAR_BRIGHTNESS.write(DEFAULT_DOTSTAR_BRIGHTNESS);
  EEPROM_CHIP_DISABLED.write(false);
  writetoflash(UNWRITTEN_PAYLOAD_NUMBER, UNWRITTEN_MODE_NUMBER);
}

void disable_chip() {
  EEPROM_CHIP_DISABLED.write(true);
  NVIC_SystemReset();
}

int pressed_pause_other_options(uint32_t milliseconds, uint32_t OPTION_NUMBER)
{
  uint32_t i = 0; uint32_t res;
  while ( i < (milliseconds * 10))
  {
    if (digitalRead(VOLUP_STRAP_PIN) == 0 && OPTION_NUMBER == 1) set_dotstar_brightness();
    if (digitalRead(VOLUP_STRAP_PIN) == 0 && OPTION_NUMBER == 2)
    {
      if (payload_indication_disabled != 1) EEPROM_PAYLOAD_INDICATION.write(1);
      else if (payload_indication_disabled == 1) EEPROM_PAYLOAD_INDICATION.write(0);

      confirm_led(20, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS, NEW_DOTSTAR_BRIGHTNESS);
      NVIC_SystemReset();
    }
    if (digitalRead(VOLUP_STRAP_PIN) == 0 && OPTION_NUMBER == 3) partial_eeprom_reset();
    if (digitalRead(VOLUP_STRAP_PIN) == 0 && OPTION_NUMBER == 4) disable_chip();
    delayMicroseconds(100);
    ++i;
  }
  return res;
}

void cycle_other_options() {

  for (ITEM = 1; ITEM < (AMOUNT_OF_OTHER_OPTIONS + 1); ++ITEM) {
    for (int CURRENT_FLASH = 0; CURRENT_FLASH < (ITEM) ; ++CURRENT_FLASH) {
      setLedColor("black");
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, LOW);
#endif
      pressed_pause_other_options(OFF1, ITEM);

      setLedColor("yellow");


#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, HIGH);
#endif
      pressed_pause_other_options(ON2, ITEM);
    }
    setLedColor("black");
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
    pressed_pause_other_options(OFF3, ITEM);
  }
}


void cycle_modes() {
  if (MODES_AVAILABLE != 1) {
    pressed_pause(400, false, 0, 0);
    for (ITEM = 1; ITEM < (MODES_AVAILABLE + 1); ++ITEM) {
      for (int CURRENT_FLASH = 0; CURRENT_FLASH < (ITEM) ; ++CURRENT_FLASH) {
        setLedColor("black");

#ifdef ONBOARD_LED
        digitalWrite(ONBOARD_LED, LOW);
#endif
        pressed_pause(OFF1, true, 0, ITEM);

        setLedColor("yellow");


#ifdef ONBOARD_LED
        digitalWrite(ONBOARD_LED, HIGH);
#endif
        pressed_pause(ON2, true, 0, ITEM);
      }
      setLedColor("black");
#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, LOW);
#endif
      pressed_pause(OFF3, true, 0, ITEM);
    }
  }
}

void cycle_payloads() {
  pressed_pause(400, false, 0, 0);
  for (ITEM = 1; ITEM < (AMOUNT_OF_PAYLOADS + 1); ++ITEM) {

    for (int CURRENT_FLASH = 0; CURRENT_FLASH < (ITEM) ; ++CURRENT_FLASH) {

      setLedColor("black");

#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, LOW);
#endif
      pressed_pause(OFF1, true, ITEM, 0);

      setPayloadColor(ITEM);


#ifdef ONBOARD_LED
      digitalWrite(ONBOARD_LED, HIGH);
#endif
      pressed_pause(ON2, true, ITEM, 0);
    }
    setLedColor("black");
#ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
#endif
    pressed_pause(OFF3, true, ITEM, 0);
  }
}


void long_press()

{

  uint32_t master_timer = 12;

  uint32_t current_tick = 0;
  if (!VOLUP_STRAP_TEST){
  VOLUP_STRAP_TEST = 1;
  EEPROM_VOL_CONTROL_STRAP.write(VOLUP_STRAP_TEST);
  }
#ifdef ONBOARD_LED
  pinMode (ONBOARD_LED, OUTPUT);
#endif

  while (digitalRead(VOLUP_STRAP_PIN) == 0)
  {
    #ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, HIGH);
    #endif
    ++current_tick;
    if (current_tick < TRIGGER1) setLedColor("red");
    if (current_tick >  TRIGGER1 && current_tick <  TRIGGER2) setLedColor("green");
    if (current_tick >  TRIGGER2 && current_tick <  TRIGGER3) setLedColor("blue");
    if (current_tick >  TRIGGER3 && current_tick <  MASTER_TIMER) setLedColor("white");
    if (current_tick == MASTER_TIMER) 
    {
      rapid_blink("white");
      if (digitalRead(VOLUP_STRAP_PIN) == 1 )full_eeprom_reset();
      else break;
    }
    
    if (current_tick == TRIGGER1)
    {
      rapid_blink("red");
      if (digitalRead(VOLUP_STRAP_PIN) == 1 )cycle_payloads();
    }
    if (current_tick == TRIGGER2)
    {
      rapid_blink("green");
      if (digitalRead(VOLUP_STRAP_PIN) == 1 )cycle_modes();
    }
    if (current_tick == TRIGGER3)
    {
      rapid_blink("blue");
      if (digitalRead(VOLUP_STRAP_PIN) == 1 )cycle_other_options();
    }
    not_pressed_pause(1000, false, 0, 0);
  }
  current_tick = 0;
  setLedColor("black");
  #ifdef ONBOARD_LED
    digitalWrite(ONBOARD_LED, LOW);
  #endif
}
