#include <Arduino.h>

const PROGMEM byte NUMBER1 [1] = {0x31};
const PROGMEM byte NUMBER2 [1] = {0x32};
const PROGMEM byte NUMBER3 [1] = {0x33};
const PROGMEM byte NUMBER4 [1] = {0x34};
const PROGMEM byte NUMBER5 [1] = {0x35};
const PROGMEM byte NUMBER6 [1] = {0x36};
const PROGMEM byte NUMBER7 [1] = {0x37};
const PROGMEM byte NUMBER8 [1] = {0x38};

const PROGMEM byte YES [3] = {
  0x59, 0x45, 0x53
};

const PROGMEM byte NO [3] = {
  0x20, 0x4E, 0x4F
};

//hksection

const PROGMEM byte MODCHIP_MODE[23] = {
  0x00, 0x00, 0x00, 0x4D, 0x6F, 0x64, 0x65, 0x3A, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};



//mode num

const PROGMEM byte USB_STRAP_DETECTED[21] = {
  0x00, 0x00, 0x00, 0x55, 0x53, 0x42, 0x20, 0x53, 0x74, 0x72, 0x61, 0x70,
  0x3A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

//y/n

const PROGMEM byte VOL_STRAP_DETECTED[21] = {
  0x00, 0x00, 0x00, 0x56, 0x4F, 0x4C, 0x2B, 0x20, 0x53, 0x74, 0x72, 0x61,
  0x70, 0x3A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

//y/n

const PROGMEM byte JOYCON_STRAP_DETECTED[21] = {
  0x00, 0x00, 0x00, 0x4A, 0x6F, 0x79, 0x63, 0x6F, 0x6E, 0x20, 0x53, 0x74,
  0x72, 0x61, 0x70, 0x3A, 0x20, 0x20, 0x20, 0x20, 0x20
};

//y/n
