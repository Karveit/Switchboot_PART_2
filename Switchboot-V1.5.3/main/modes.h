#include <Arduino.h>

bool switchboot; bool fusee; bool update_samd;
void mode_check() {
  if (EEPROM_MODE_NUMBER == 1) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 0;
    switchboot = true; fusee = false; update_samd = false;
    }
    else if (EEPROM_MODE_NUMBER == 2) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 0;
    fusee = true; switchboot = false; update_samd = false;;
    }
    else if (EEPROM_MODE_NUMBER == 3) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 1;
    switchboot = true; fusee = false; update_samd = false;
    }
    else if (EEPROM_MODE_NUMBER == 4) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 1;
    fusee = true; switchboot = false; update_samd = false;
    } 
    if (EEPROM_MODE_NUMBER > 4) {
    AMOUNT_OF_PAYLOADS = 1;
    AUTO_INCREASE_PAYLOAD_on = 0;
    switchboot = false; fusee = false; update_samd = true;
    } 
  }
