#include <Arduino.h>

bool switchboot; bool fusee;
void mode_check() {
  if (WRITTEN_MODE_NUMBER == 1) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 0;
    switchboot = true; fusee = false;
    }
    else if (WRITTEN_MODE_NUMBER == 2) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 0;
    fusee = true; switchboot = false;
    }
    else if (WRITTEN_MODE_NUMBER == 3) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 1;
    switchboot = true; fusee = false;
    }
    else if (WRITTEN_MODE_NUMBER == 4) {
    AMOUNT_OF_PAYLOADS = 8;
    AUTO_INCREASE_PAYLOAD_on = 1;
    fusee = true; switchboot = false;
    }  
  }
