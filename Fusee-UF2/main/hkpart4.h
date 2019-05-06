#include <Arduino.h>

const PROGMEM byte USB_STRAP_DETECTED[4] = {
  0x55, 0x53, 0x42, 0x3A
};

const PROGMEM byte VOL_STRAP_DETECTED[6] = {
  0x00, 0x56, 0x4F, 0x4C, 0x2B, 0x3A
};

const PROGMEM byte JOYCON_STRAP_DETECTED[11] = {
  0x00, 0x00, 0x00, 0x00, 0x4A, 0x6F, 0x79, 0x63, 0x6F, 0x6E, 0x3A
};

const PROGMEM byte AFTER_STRAPS[1093] = {
  0x00, 0x00, 0x70, 0x61, 0x74, 0x68, 0x73, 0x5F, 0x69, 0x6E, 0x66, 0x6F,
  0x2E, 0x74, 0x78, 0x74, 0x00, 0x00, 0x57, 0x65, 0x6C, 0x63, 0x6F, 0x6D,
  0x65, 0x20, 0x74, 0x6F, 0x20, 0x46, 0x75, 0x73, 0x65, 0x65, 0x2D, 0x55,
  0x46, 0x32, 0x20, 0x49, 0x6E, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x69,
  0x6F, 0x6E, 0x2E, 0x0A, 0x00, 0x00, 0x55, 0x73, 0x61, 0x62, 0x6C, 0x65,
  0x20, 0x70, 0x61, 0x74, 0x68, 0x73, 0x2E, 0x2E, 0x2E, 0x20, 0x4F, 0x6E,
  0x6C, 0x79, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x2E, 0x62,
  0x69, 0x6E, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F,
  0x61, 0x64, 0x78, 0x2E, 0x62, 0x69, 0x6E, 0x20, 0x61, 0x72, 0x65, 0x20,
  0x64, 0x69, 0x73, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x64, 0x20, 0x6F, 0x6E,
  0x20, 0x65, 0x72, 0x72, 0x6F, 0x72, 0x20, 0x73, 0x63, 0x72, 0x65, 0x65,
  0x6E, 0x2E, 0x0A, 0x00, 0x00, 0x00, 0x4D, 0x61, 0x64, 0x65, 0x20, 0x74,
  0x6F, 0x20, 0x6A, 0x75, 0x73, 0x74, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x20,
  0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x2E, 0x62, 0x69, 0x6E, 0x28,
  0x6F, 0x72, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x78, 0x2E,
  0x62, 0x69, 0x6E, 0x20, 0x64, 0x65, 0x70, 0x65, 0x6E, 0x64, 0x69, 0x6E,
  0x67, 0x20, 0x6F, 0x66, 0x20, 0x73, 0x65, 0x6C, 0x65, 0x63, 0x74, 0x69,
  0x6F, 0x6E, 0x20, 0x69, 0x6E, 0x20, 0x53, 0x41, 0x4D, 0x44, 0x32, 0x31,
  0x2C, 0x20, 0x65, 0x67, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64,
  0x32, 0x2E, 0x62, 0x69, 0x6E, 0x29, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x0A,
  0x00, 0x00, 0x48, 0x69, 0x64, 0x64, 0x65, 0x6E, 0x20, 0x68, 0x61, 0x72,
  0x64, 0x63, 0x6F, 0x64, 0x65, 0x64, 0x20, 0x70, 0x61, 0x74, 0x68, 0x73,
  0x3A, 0x00, 0x48, 0x65, 0x69, 0x72, 0x61, 0x72, 0x63, 0x68, 0x79, 0x20,
  0x6F, 0x66, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x73, 0x20,
  0x6C, 0x6F, 0x6F, 0x6B, 0x65, 0x64, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x28,
  0x69, 0x6E, 0x20, 0x6F, 0x72, 0x64, 0x65, 0x72, 0x2E, 0x2E, 0x2E, 0x70,
  0x61, 0x74, 0x68, 0x73, 0x2F, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x20, 0x73,
  0x68, 0x6F, 0x75, 0x6C, 0x64, 0x20, 0x62, 0x65, 0x20, 0x45, 0x58, 0x41,
  0x43, 0x54, 0x4C, 0x59, 0x20, 0x61, 0x73, 0x20, 0x62, 0x65, 0x6C, 0x6F,
  0x77, 0x2E, 0x2E, 0x2E, 0x20, 0x43, 0x68, 0x6F, 0x6F, 0x73, 0x65, 0x20,
  0x6F, 0x6E, 0x65, 0x2E, 0x2E, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x54, 0x68,
  0x65, 0x20, 0x66, 0x69, 0x72, 0x73, 0x74, 0x20, 0x6F, 0x6E, 0x65, 0x20,
  0x66, 0x6F, 0x75, 0x6E, 0x64, 0x20, 0x77, 0x69, 0x6C, 0x6C, 0x20, 0x62,
  0x6F, 0x6F, 0x74, 0x2E, 0x00, 0x00, 0x0A, 0x31, 0x2E, 0x20, 0x62, 0x6F,
  0x6F, 0x74, 0x6C, 0x6F, 0x61, 0x64, 0x65, 0x72, 0x2F, 0x73, 0x77, 0x69,
  0x74, 0x63, 0x68, 0x62, 0x6F, 0x6F, 0x74, 0x2E, 0x62, 0x69, 0x6E, 0x20,
  0x28, 0x6D, 0x79, 0x20, 0x68, 0x65, 0x6B, 0x61, 0x74, 0x65, 0x20, 0x6D,
  0x6F, 0x64, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x63, 0x68, 0x69, 0x70, 0x70,
  0x65, 0x64, 0x20, 0x75, 0x6E, 0x69, 0x74, 0x73, 0x29, 0x00, 0x0A, 0x32,
  0x2E, 0x20, 0x61, 0x72, 0x67, 0x6F, 0x6E, 0x2F, 0x61, 0x72, 0x67, 0x6F,
  0x6E, 0x2E, 0x62, 0x69, 0x6E, 0x20, 0x28, 0x74, 0x6F, 0x75, 0x63, 0x68,
  0x73, 0x63, 0x72, 0x65, 0x65, 0x6E, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F,
  0x61, 0x64, 0x20, 0x6C, 0x61, 0x75, 0x6E, 0x63, 0x68, 0x65, 0x72, 0x29,
  0x00, 0x00, 0x0A, 0x33, 0x2E, 0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61,
  0x64, 0x2E, 0x62, 0x69, 0x6E, 0x20, 0x28, 0x43, 0x61, 0x6E, 0x20, 0x62,
  0x65, 0x20, 0x61, 0x6E, 0x79, 0x74, 0x68, 0x69, 0x6E, 0x67, 0x20, 0x79,
  0x6F, 0x75, 0x20, 0x77, 0x61, 0x6E, 0x74, 0x20, 0x69, 0x74, 0x20, 0x74,
  0x6F, 0x20, 0x62, 0x65, 0x29, 0x00, 0x0A, 0x34, 0x2E, 0x20, 0x50, 0x61,
  0x79, 0x6C, 0x6F, 0x61, 0x64, 0x73, 0x2F, 0x70, 0x61, 0x79, 0x6C, 0x6F,
  0x61, 0x64, 0x2E, 0x62, 0x69, 0x6E, 0x20, 0x28, 0x43, 0x61, 0x6E, 0x20,
  0x62, 0x65, 0x20, 0x61, 0x6E, 0x79, 0x74, 0x68, 0x69, 0x6E, 0x67, 0x20,
  0x79, 0x6F, 0x75, 0x20, 0x77, 0x61, 0x6E, 0x74, 0x20, 0x69, 0x74, 0x20,
  0x74, 0x6F, 0x20, 0x62, 0x65, 0x29, 0x28, 0x6B, 0x65, 0x65, 0x70, 0x73,
  0x20, 0x53, 0x44, 0x20, 0x72, 0x6F, 0x6F, 0x74, 0x20, 0x74, 0x69, 0x64,
  0x79, 0x29, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x35, 0x2E, 0x20, 0x70, 0x61,
  0x79, 0x6C, 0x6F, 0x61, 0x64, 0x31, 0x2E, 0x62, 0x69, 0x6E, 0x20, 0x28,
  0x63, 0x68, 0x61, 0x6E, 0x67, 0x65, 0x73, 0x20, 0x64, 0x65, 0x70, 0x65,
  0x6E, 0x64, 0x69, 0x6E, 0x67, 0x20, 0x6F, 0x6E, 0x20, 0x53, 0x41, 0x4D,
  0x44, 0x20, 0x73, 0x65, 0x74, 0x74, 0x69, 0x6E, 0x67, 0x29, 0x00, 0x00,
  0x00, 0x00, 0x0A, 0x36, 0x2E, 0x20, 0x50, 0x61, 0x79, 0x6C, 0x6F, 0x61,
  0x64, 0x73, 0x2F, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x31, 0x2E,
  0x62, 0x69, 0x6E, 0x20, 0x28, 0x63, 0x68, 0x61, 0x6E, 0x67, 0x65, 0x73,
  0x20, 0x64, 0x65, 0x70, 0x65, 0x6E, 0x64, 0x69, 0x6E, 0x67, 0x20, 0x6F,
  0x6E, 0x20, 0x53, 0x41, 0x4D, 0x44, 0x20, 0x73, 0x65, 0x74, 0x74, 0x69,
  0x6E, 0x67, 0x29, 0x28, 0x6B, 0x65, 0x65, 0x70, 0x73, 0x20, 0x53, 0x44,
  0x20, 0x72, 0x6F, 0x6F, 0x74, 0x20, 0x74, 0x69, 0x64, 0x79, 0x29, 0x00,
  0x00, 0x00, 0x0A, 0x37, 0x2E, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x6C, 0x6F,
  0x61, 0x64, 0x65, 0x72, 0x2F, 0x75, 0x70, 0x64, 0x61, 0x74, 0x65, 0x2E,
  0x62, 0x69, 0x6E, 0x20, 0x28, 0x6C, 0x61, 0x73, 0x74, 0x20, 0x63, 0x68,
  0x61, 0x6E, 0x63, 0x65, 0x20, 0x73, 0x61, 0x6C, 0x6F, 0x6F, 0x6E, 0x20,
  0x2D, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x4B, 0x6F, 0x73, 0x6D, 0x6F, 0x73,
  0x20, 0x65, 0x74, 0x63, 0x20, 0x75, 0x73, 0x65, 0x72, 0x73, 0x20, 0x2D,
  0x20, 0x6E, 0x6F, 0x20, 0x6E, 0x65, 0x65, 0x64, 0x20, 0x66, 0x6F, 0x72,
  0x20, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x2E, 0x62, 0x69, 0x6E,
  0x20, 0x69, 0x66, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20, 0x70, 0x72, 0x65,
  0x73, 0x65, 0x6E, 0x74, 0x20, 0x2D, 0x20, 0x70, 0x72, 0x6F, 0x62, 0x61,
  0x62, 0x6C, 0x79, 0x20, 0x68, 0x65, 0x6B, 0x61, 0x74, 0x65, 0x29, 0x00,
  0x00, 0x00, 0x52, 0x65, 0x6D, 0x65, 0x6D, 0x62, 0x65, 0x72, 0x2E, 0x2E,
  0x2E, 0x20, 0x46, 0x69, 0x72, 0x73, 0x74, 0x20, 0x65, 0x6E, 0x74, 0x72,
  0x79, 0x20, 0x66, 0x6F, 0x75, 0x6E, 0x64, 0x20, 0x77, 0x69, 0x6C, 0x6C,
  0x20, 0x62, 0x6F, 0x6F, 0x74, 0x2E, 0x20, 0x4B, 0x6F, 0x73, 0x6D, 0x6F,
  0x73, 0x20, 0x75, 0x73, 0x65, 0x72, 0x73, 0x20, 0x64, 0x6F, 0x6E, 0x60,
  0x74, 0x20, 0x6E, 0x65, 0x65, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x64, 0x6F,
  0x20, 0x61, 0x6E, 0x79, 0x74, 0x68, 0x69, 0x6E, 0x67, 0x2E, 0x2E, 0x2E,
  0x20, 0x75, 0x70, 0x64, 0x61, 0x74, 0x65, 0x2E, 0x62, 0x69, 0x6E, 0x20,
  0x77, 0x69, 0x6C, 0x6C, 0x20, 0x62, 0x6F, 0x6F, 0x74, 0x20, 0x61, 0x75,
  0x74, 0x6F, 0x6D, 0x61, 0x74, 0x69, 0x63, 0x61, 0x6C, 0x6C, 0x79, 0x00,
  0x00, 0x00, 0x0A, 0x50, 0x6C, 0x65, 0x61, 0x73, 0x65, 0x20, 0x63, 0x68,
  0x65, 0x63, 0x6B, 0x20, 0x53, 0x44, 0x20, 0x63, 0x61, 0x72, 0x64, 0x0A,
  0x0A, 0x4E, 0x6F, 0x74, 0x20, 0x66, 0x6F, 0x75, 0x6E, 0x64, 0x3A, 0x0A,
  0x0A, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x2E, 0x62, 0x69, 0x6E,
  0x0A
};


const PROGMEM byte YES [3] = {
  0x59, 0x45, 0x53
};

const PROGMEM byte NO [3] = {
  0x20, 0x4E, 0x4F
};

const PROGMEM byte MODCHIP_MODE[23] = {
  0x00, 0x00, 0x00, 0x4D, 0x6F, 0x64, 0x65, 0x3A, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};