#include <Arduino.h>
#include <Usb.h>
#include <Adafruit_DotStar.h>
#include <FlashStorage.h>

#define RCM_STRAP_TIME_us 1000000
#define LED_CONFIRM_TIME_us 500000
#define PACKET_CHUNK_SIZE 0x1000

#define DEFAULT_PAYLOAD 1
#define DEFAULT_MODE 1
#define DEFAULT_DOTSTAR_BRIGHTNESS 180
#define DEFAULT_JOYCON 0
#define DEFAULT_VOLUME 0
#define MODES_AVAILABLE 4


#define PAYLOAD_FLASH_LED_ON_TIME_SECONDS 0.05 // controls blink during payload indication. On
#define PAYLOAD_FLASH_LED_OFF_TIME_SECONDS 0.3 // as above, but amount of time for DARKNESS ;)
#define DELAY_BEFORE_SLEEP 500000
#define AUTO_SEND_ON_PAYLOAD_INCREASE_PIN 0  //Automatic send when payload pin is activated. 1 = on, 0 = off
#define LOOK_FOR_TEGRA_LED_SPEED 100 //How fast to blink when searching. Higher = slower
#define AMOUNT_OF_OTHER_OPTIONS 4 // lock out disable chip
#define BLINK_PAYLOAD_BEFORE_SEARCH 0

///////////////////////////////////////////
//#define DISABLE_STRAP_INFO_TXT

//#define TRINKET
//#define TRINKETMETHOD3
//#define TRINKETLEGACY3
//#define REBUG
//#define RCMX86_INTERNAL
//#define RCMX86
//#define GENERIC_TRINKET_DONGLE
//#define R4S
//#define DRAGONINJECTOR
//#define GEMMA
//#define GENERIC_GEMMA_DONGLE
//#define ITSYBITSY
#define FEATHER
//#define EXEN_MINI **currently incomplete

FlashStorage(EEPROM_PAYLOAD_NUMBER, uint32_t);
FlashStorage(EEPROM_MODE_NUMBER, uint32_t);
FlashStorage(EEPROM_USB_REBOOT_STRAP, uint32_t);
FlashStorage(EEPROM_VOL_CONTROL_STRAP, uint32_t);
FlashStorage(EEPROM_JOYCON_CONTROL_STRAP, uint32_t);
FlashStorage(EEPROM_EMPTY, uint32_t);
FlashStorage(EEPROM_AUTOINCREASE_PAYLOAD, uint32_t);
FlashStorage(EEPROM_DOTSTAR_BRIGHTNESS, uint32_t);
FlashStorage(EEPROM_CHIP_DISABLED, bool);
FlashStorage(EEPROM_PAYLOAD_INDICATION, uint32_t);

uint32_t WRITTEN_PAYLOAD_NUMBER = EEPROM_PAYLOAD_NUMBER.read();
uint32_t WRITTEN_MODE_NUMBER = EEPROM_MODE_NUMBER.read();
uint32_t USB_STRAP_TEST = EEPROM_USB_REBOOT_STRAP.read();
uint32_t VOLUP_STRAP_TEST = EEPROM_VOL_CONTROL_STRAP.read();
uint32_t JOYCON_STRAP_TEST = EEPROM_JOYCON_CONTROL_STRAP.read();
uint32_t EEPROM_INITIAL_WRITE = EEPROM_EMPTY.read();
uint32_t STORED_AUTOINCREASE_PAYLOAD_FLAG = EEPROM_AUTOINCREASE_PAYLOAD.read();
uint32_t NEW_DOTSTAR_BRIGHTNESS = EEPROM_DOTSTAR_BRIGHTNESS.read();
bool chipdisabled = EEPROM_CHIP_DISABLED.read();
uint32_t payload_indication_disabled = EEPROM_PAYLOAD_INDICATION.read();
//non eeprom globals
uint32_t ITEM;
uint32_t AMOUNT_OF_PAYLOADS;
uint32_t AUTO_INCREASE_PAYLOAD_on;
uint32_t CHANGE_STORED_AUTOINCREASE_PAYLOAD_FLAG;
unsigned long battvalue;
bool flatbatt = false;

USBHost usb;
EpInfo epInfo[3];

byte usbWriteBuffer[PACKET_CHUNK_SIZE] = {0};
uint32_t usbWriteBufferUsed = 0;
uint32_t packetsWritten = 0;

bool foundTegra = false;
byte tegraDeviceAddress = -1;

unsigned long lastCheckTime = 0;


#include "boards.h"
#include "PL1.h"
#include "PL2.h"
#include "PL3.h"
#include "PL4.h"
#include "FS_misc.h"
#include "STRAP_INFO.h"
#include "LED_control.h"
#include "modes.h"
#include "longpress.h"
#include "usb_control.h"

void battery_check() {
#ifdef DRAGONINJECTOR
  pinMode (REDLED, OUTPUT);
  pinMode (GREENLED, OUTPUT);
  pinMode (BLUELED, OUTPUT);
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  //digitalWrite (VOLUP_STRAP_PIN, LOW);
  digitalWrite(REDLED, HIGH);
  digitalWrite(GREENLED, HIGH);
  digitalWrite(BLUELED, HIGH);
  //pinMode (BATTERY_LEVEL_CHECK, INPUT);
  delayMicroseconds(100000);// allow to stabilise
  battvalue = analogRead (BATTERY_LEVEL_CHECK);
  //battvalue = 876;
  if (battvalue > 900) {
    digitalWrite(GREENLED, LOW);
    flatbatt = false;
  } else if (battvalue > 860 && battvalue < 899) {
    digitalWrite(REDLED, LOW);
    digitalWrite(GREENLED, LOW);
    flatbatt = false;
  } else if (battvalue < 859) {
    digitalWrite(REDLED, LOW);
    flatbatt = true;
  }
  delayMicroseconds(1000000);
  digitalWrite(REDLED, HIGH);
  digitalWrite(GREENLED, HIGH);
  digitalWrite(BLUELED, HIGH);
  pinMode (BATTERY_LEVEL_CHECK, INPUT);
  if (flatbatt) {
    sleep(-1);
  }
#endif
}

void run_once()
{

  if (!EEPROM_INITIAL_WRITE) {

    EEPROM_PAYLOAD_NUMBER.write(DEFAULT_PAYLOAD);
    EEPROM_MODE_NUMBER.write(DEFAULT_MODE);
    EEPROM_DOTSTAR_BRIGHTNESS.write(DEFAULT_DOTSTAR_BRIGHTNESS);
    EEPROM_JOYCON_CONTROL_STRAP.write(DEFAULT_JOYCON);
    EEPROM_VOL_CONTROL_STRAP.write(DEFAULT_VOLUME);
    EEPROM_PAYLOAD_INDICATION.write(0);
    
    STORED_AUTOINCREASE_PAYLOAD_FLAG = 0;
    CHANGE_STORED_AUTOINCREASE_PAYLOAD_FLAG = 0;

#ifdef USB_LOW_RESET
    pinMode(USB_LOW_RESET, INPUT_PULLDOWN); // use internal pulldown on this boot only
    uint32_t usb_voltage_check = digitalRead(USB_LOW_RESET); //check voltage on thermistor pad on BQ24193
    if (usb_voltage_check == HIGH) {
      setLedColor("blue");
      delayMicroseconds(2000000); //delay so I can activate bootloader mode to pull UF2 without Eeprom data
      USB_STRAP_TEST = 1; EEPROM_USB_REBOOT_STRAP.write(USB_STRAP_TEST); //strap is fitted. Lets store to flash
      EEPROM_INITIAL_WRITE = 1; EEPROM_EMPTY.write(EEPROM_INITIAL_WRITE); // run-once complete. Store to flash to say it has ran
    } else {
      setLedColor("red");
      delayMicroseconds(2000000);
      USB_STRAP_TEST = 0; EEPROM_USB_REBOOT_STRAP.write(USB_STRAP_TEST); //strap is not fitted. Lets store to flash
      

    }
    #endif
    EEPROM_INITIAL_WRITE = 1; EEPROM_EMPTY.write(EEPROM_INITIAL_WRITE); // run-once complete. Store to flash to say it has ran
    confirm_led(10, 255, 255, 255);
    NVIC_SystemReset(); //restart to reflect changes
  }

}




void wakeup() {
  SCB->AIRCR = ((0x5FA << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk); //full software reset
}

void setup()
{
  #ifdef DRAGONINJECTOR
  battery_check();
  #endif
  run_once();
  mode_check();
  all_leds_off();
  
#ifdef DRAGONINJECTOR
  pinMode (VOLUP_STRAP_PIN, OUTPUT);
  digitalWrite (VOLUP_STRAP_PIN, LOW);
  delayMicroseconds (10);
  pinMode (VOLUP_STRAP_PIN, INPUT_PULLUP);
  if (digitalRead(VOLUP_STRAP_PIN) == LOW) {
    long_press();
  }
#endif

#ifdef JOYCON_STRAP_PIN
  pinMode(JOYCON_STRAP_PIN, INPUT);
#endif

#ifdef VOLUP_STRAP_PIN
  pinMode(VOLUP_STRAP_PIN, INPUT_PULLUP);
#endif

#ifdef WAKEUP_PIN
  pinMode(WAKEUP_PIN, INPUT);
#endif

#ifdef USB_LOW_RESET
  pinMode(USB_LOW_RESET, INPUT);
#endif

#ifdef DONGLE
#ifdef VOLUP_STRAP_PIN
  attachInterrupt(VOLUP_STRAP_PIN, long_press, LOW);
#endif
#endif
#ifdef USB_LOGIC
  pinMode(USB_LOGIC, OUTPUT);
  digitalWrite(USB_LOGIC, LOW);
#endif

#ifdef RCMX86
  pinMode(DCDC_EN_PIN, OUTPUT);
  digitalWrite(DCDC_EN_PIN, HIGH);
  pinMode(USBCC_PIN, INPUT);
  pinMode(USB_VCC_PIN, INPUT_PULLDOWN);
  pinMode(ONBOARD_LED, OUTPUT);

  digitalWrite(ONBOARD_LED, LOW);
  digitalWrite(ONBOARD_LED, HIGH); delay(30);
  digitalWrite(ONBOARD_LED, LOW);
  /*
    delay(300);
  */
  while (digitalRead(USB_VCC_PIN));
  delay(30);//delay to ready pull out
  while (digitalRead(USB_VCC_PIN));
#endif

#ifdef DOTSTAR_ENABLED
  strip.begin();
#endif

if(!chipdisabled){
  int usbInitialized = usb.Init();
  if (usbInitialized == -1) sleep(-1);

  bool blink = true;
  int currentTime = 0;
#ifdef ONBOARD_LED
  pinMode(ONBOARD_LED, OUTPUT);
#endif

if (BLINK_PAYLOAD_BEFORE_SEARCH == 1)
{
mode_payload_blink_led(1, WRITTEN_PAYLOAD_NUMBER);
} 

  while (!foundTegra)
  {
    currentTime = millis();
    usb.Task();

    if (currentTime > lastCheckTime + LOOK_FOR_TEGRA_LED_SPEED) {
      usb.ForEachUsbDevice(&findTegraDevice);
      if (blink && !foundTegra) {
        setPayloadColor(WRITTEN_PAYLOAD_NUMBER);
#ifdef ONBOARD_LED
        digitalWrite(ONBOARD_LED, HIGH);
#endif
      } else {
        setLedColor("black"); //led to black
#ifdef ONBOARD_LED
        digitalWrite(ONBOARD_LED, LOW);
#endif
      }
      blink = !blink;
      lastCheckTime = currentTime;
    }
    if (currentTime > (LOOK_FOR_TEGRA_SECONDS * 1000)) {
      sleep(-1);
    }

  }

  setupTegraDevice();

  byte deviceID[16] = {0};
  readTegraDeviceID(deviceID);
  UHD_Pipe_Alloc(tegraDeviceAddress, 0x01, USB_HOST_PTYPE_BULK, USB_EP_DIR_OUT, 0x40, 0, USB_HOST_NB_BK_1);
  packetsWritten = 0;

  if (switchboot == true) {
    if (WRITTEN_PAYLOAD_NUMBER == 1) {
      send_switchboot(PAYLOAD1);
    } else if (WRITTEN_PAYLOAD_NUMBER == 2) {
      send_switchboot(PAYLOAD2);
    } else if (WRITTEN_PAYLOAD_NUMBER == 3) {
      send_switchboot(PAYLOAD3);
    } else if (WRITTEN_PAYLOAD_NUMBER == 4) {
      send_switchboot(PAYLOAD4);
    } else if (WRITTEN_PAYLOAD_NUMBER == 5) {
      send_switchboot(PAYLOAD5);
    } else if (WRITTEN_PAYLOAD_NUMBER == 6) {
      send_switchboot(PAYLOAD6);
    } else if (WRITTEN_PAYLOAD_NUMBER == 7) {
      send_switchboot(PAYLOAD7);
    } else if (WRITTEN_PAYLOAD_NUMBER == 8) {
      send_switchboot(PAYLOAD8);
    }
  }

  if (fusee == true) {
    if (WRITTEN_PAYLOAD_NUMBER == 1) {
      send_fusee(PAYLOAD1);
    } else if (WRITTEN_PAYLOAD_NUMBER == 2) {
      send_fusee(PAYLOAD2);
    } else if (WRITTEN_PAYLOAD_NUMBER == 3) {
      send_fusee(PAYLOAD3);
    } else if (WRITTEN_PAYLOAD_NUMBER == 4) {
      send_fusee(PAYLOAD4);
    } else if (WRITTEN_PAYLOAD_NUMBER == 5) {
      send_fusee(PAYLOAD5);
    } else if (WRITTEN_PAYLOAD_NUMBER == 6) {
      send_fusee(PAYLOAD6);
    } else if (WRITTEN_PAYLOAD_NUMBER == 7) {
      send_fusee(PAYLOAD7);
    } else if (WRITTEN_PAYLOAD_NUMBER == 8) {
      send_fusee(PAYLOAD8);
    }
  }

  if (packetsWritten % 2 != 1)
  {
    usbFlushBuffer();
  }


  usb.ctrlReq(tegraDeviceAddress, 0, USB_SETUP_DEVICE_TO_HOST | USB_SETUP_TYPE_STANDARD | USB_SETUP_RECIPIENT_INTERFACE,
              0x00, 0x00, 0x00, 0x00, 0x7000, 0x7000, usbWriteBuffer, NULL);
  sleep(1);
} else sleep(-1);
}

void loop()
{
  sleep(1);
}
