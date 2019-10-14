#include <Arduino.h>


#define INTERMEZZO_SIZE 92

#define INTERMEZZO_SIZE 92
const byte intermezzo[INTERMEZZO_SIZE] =
{
  0x44, 0x00, 0x9F, 0xE5, 0x01, 0x11, 0xA0, 0xE3, 0x40, 0x20, 0x9F, 0xE5, 0x00, 0x20, 0x42, 0xE0,
  0x08, 0x00, 0x00, 0xEB, 0x01, 0x01, 0xA0, 0xE3, 0x10, 0xFF, 0x2F, 0xE1, 0x00, 0x00, 0xA0, 0xE1,
  0x2C, 0x00, 0x9F, 0xE5, 0x2C, 0x10, 0x9F, 0xE5, 0x02, 0x28, 0xA0, 0xE3, 0x01, 0x00, 0x00, 0xEB,
  0x20, 0x00, 0x9F, 0xE5, 0x10, 0xFF, 0x2F, 0xE1, 0x04, 0x30, 0x90, 0xE4, 0x04, 0x30, 0x81, 0xE4,
  0x04, 0x20, 0x52, 0xE2, 0xFB, 0xFF, 0xFF, 0x1A, 0x1E, 0xFF, 0x2F, 0xE1, 0x20, 0xF0, 0x01, 0x40,
  0x5C, 0xF0, 0x01, 0x40, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x01, 0x40,
};

#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.print (x)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#define DEBUG_PRINTHEX(x,y)  serialPrintHex (x,y)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTHEX(x,y)
#endif

#define PACKET_CHUNK_SIZE 0x1000

void usbOutTransferChunk(uint32_t addr, uint32_t ep, uint32_t nbytes, uint8_t* data)
{


  EpInfo* epInfo = usb.getEpInfoEntry(addr, ep);

  usb_pipe_table[epInfo->epAddr].HostDescBank[0].CTRL_PIPE.bit.PDADDR = addr;

  if (epInfo->bmSndToggle)
    USB->HOST.HostPipe[epInfo->epAddr].PSTATUSSET.reg = USB_HOST_PSTATUSSET_DTGL;
  else
    USB->HOST.HostPipe[epInfo->epAddr].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_DTGL;

  UHD_Pipe_Write(epInfo->epAddr, PACKET_CHUNK_SIZE, data);
  uint32_t rcode = usb.dispatchPkt(tokOUT, epInfo->epAddr, 15);
  if (rcode)
  {
    if (rcode == USB_ERROR_DATATOGGLE)
    {
      epInfo->bmSndToggle = USB_HOST_DTGL(epInfo->epAddr);
      if (epInfo->bmSndToggle)
        USB->HOST.HostPipe[epInfo->epAddr].PSTATUSSET.reg = USB_HOST_PSTATUSSET_DTGL;
      else
        USB->HOST.HostPipe[epInfo->epAddr].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_DTGL;
    }
    else
    {
      #ifdef DOTSTAR_ENABLED
      strip.setPixelColor(0, 64, 0, 0); strip.show();
      #endif
      return;
    }
  }

  epInfo->bmSndToggle = USB_HOST_DTGL(epInfo->epAddr);
}

void usbFlushBuffer()
{
  usbOutTransferChunk(tegraDeviceAddress, 0x01, PACKET_CHUNK_SIZE, usbWriteBuffer);

  memset(usbWriteBuffer, 0, PACKET_CHUNK_SIZE);
  usbWriteBufferUsed = 0;
  packetsWritten++;
}

// This accepts arbitrary sized USB writes and will automatically chunk them into writes of size 0x1000 and increment
// packetsWritten every time a chunk is written out.
void usbBufferedWrite(const byte *data, uint32_t length)
{
  while (usbWriteBufferUsed + length >= PACKET_CHUNK_SIZE)
  {
    uint32_t bytesToWrite = min(PACKET_CHUNK_SIZE - usbWriteBufferUsed, length);
    memcpy(usbWriteBuffer + usbWriteBufferUsed, data, bytesToWrite);
    usbWriteBufferUsed += bytesToWrite;
    usbFlushBuffer();
    data += bytesToWrite;
    length -= bytesToWrite;
  }

  if (length > 0)
  {
    memcpy(usbWriteBuffer + usbWriteBufferUsed, data, length);
    usbWriteBufferUsed += length;
  }
}

void usbBufferedWriteU32(uint32_t data)
{
  usbBufferedWrite((byte *)&data, 4);
}

void readTegraDeviceID(byte *deviceID)
{
  byte readLength = 16;
  UHD_Pipe_Alloc(tegraDeviceAddress, 0x01, USB_HOST_PTYPE_BULK, USB_EP_DIR_IN, 0x40, 0, USB_HOST_NB_BK_1);
  if (usb.inTransfer(tegraDeviceAddress, 0x01, &readLength, deviceID))
    DEBUG_PRINTLN("Failed to get device ID!");
}

void sendPayload(const byte *payload, uint32_t payloadLength)
{
  byte zeros[0x1000] = {0};

  usbBufferedWriteU32(0x30298);
  usbBufferedWrite(zeros, 680 - 4);

  for (uint32_t i = 0; i < 0x3C00; i++)
    usbBufferedWriteU32(0x4001F000);

  usbBufferedWrite(intermezzo, INTERMEZZO_SIZE);
  usbBufferedWrite(zeros, 0xFA4);
  usbBufferedWrite(payload, payloadLength);
  usbFlushBuffer();
}

void send_switchboot(const byte *payload)
{
  byte zeros[0x1000] = {0};

  usbBufferedWriteU32(0x30298);
  usbBufferedWrite(zeros, 680 - 4);

  for (uint32_t i = 0; i < 0x3C00; i++)
    usbBufferedWriteU32(0x4001F000);

  usbBufferedWrite(intermezzo, INTERMEZZO_SIZE);
  usbBufferedWrite(zeros, 0xFA4);

  usbBufferedWrite(SWPART1, 102272);

if (reboot_reason == 1)
{
  EEPROM_WAKEUP_REASON.write(0);
  usbBufferedWrite(USB_POR_OFF_ENABLED,37);
}
else usbBufferedWrite(USB_POR_OFF_DISABLED,37);

usbBufferedWrite(AFTER_USB_POR,491);

#ifdef DISABLE_STRAP_INFO_TXT
  usbBufferedWrite(UNUSED_FILENAME, 32);
#else
  usbBufferedWrite(STRAP_FILENAME, 32);
#endif
  usbBufferedWrite(SWPART2, 524);
  ////////////////////////////////////////////////
  usbBufferedWrite(payload, 12);
  usbBufferedWrite(SWPART3, 1156);
  usbBufferedWrite(OVERRIDE1, 34); usbBufferedWrite(PAYLOADBIN, 11);
  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(OVERRIDE2, 33); usbBufferedWrite(payload, 12);
  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(MODE, 7);
  if (WRITTEN_MODE_NUMBER == 1) {
    usbBufferedWrite(NUM1, 38);
  } else if (WRITTEN_MODE_NUMBER == 2) {
    usbBufferedWrite(NUM2, 38);
  } else if (WRITTEN_MODE_NUMBER == 3) {
    usbBufferedWrite(NUM3, 38);
  } else if (WRITTEN_MODE_NUMBER == 4) {
    usbBufferedWrite(NUM4, 38);
  } else if (WRITTEN_MODE_NUMBER == 5) {
    usbBufferedWrite(NUM5, 38);
  }

  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(USBSTRAP, 7);
#ifdef DONGLE
  usbBufferedWrite(NOTFOUND, 38);
#else
  if (USB_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 38);
  } else {
    usbBufferedWrite(NO, 38);
  }
#endif
  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(VOLPLUSSTRAP, 7);
#ifdef DONGLE
  usbBufferedWrite(NOTFOUND, 38);
#else
  if (VOLUP_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 38);
  } else {
    usbBufferedWrite(NO, 38);
  }
#endif
  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(JOYCONSTRAP, 7);
#ifdef DONGLE
  usbBufferedWrite(NOTFOUND, 38);
#else
  if (JOYCON_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 38);
  } else {
    usbBufferedWrite(NO, 38);

  }
#endif
  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(BOARDNAME, 45);
  usbBufferedWrite(swnewline, 2);
  usbBufferedWrite(SWPART4, 20578);
  usbFlushBuffer();
}


void send_fusee(const byte *payload)
{

  byte zeros[0x1000] = {0};

  usbBufferedWriteU32(0x30298);
  usbBufferedWrite(zeros, 680 - 4);

  for (uint32_t i = 0; i < 0x3C00; i++)
    usbBufferedWriteU32(0x4001F000);

  usbBufferedWrite(intermezzo, INTERMEZZO_SIZE);
  usbBufferedWrite(zeros, 0xFA4);

  usbBufferedWrite(FSPART1, 41112);
  usbBufferedWrite(payload, 12);
  usbBufferedWrite(INBETWEENPL, 24);
  usbBufferedWrite(payload, 12);
  usbBufferedWrite(fsnewline4, 4);
  usbBufferedWrite(PAYLOADBIN, 11);
  usbBufferedWrite(INBETWEENPL2, 21);
  usbBufferedWrite(PAYLOADBIN, 11);
  usbBufferedWrite(FSAFTERPL1, 141);

  usbBufferedWrite(OVERRIDE1, 34); usbBufferedWrite(PAYLOADBIN, 11); usbBufferedWrite(UNDER_LINE1, 7);
  /////
  usbBufferedWrite(OVERRIDE2, 33); usbBufferedWrite(payload, 12); usbBufferedWrite(fsnewline3, 3);
  /////
  usbBufferedWrite(MODE, 7);
  if (WRITTEN_MODE_NUMBER == 1) {
    usbBufferedWrite(NUM1, 38);
  } else if (WRITTEN_MODE_NUMBER == 2) {
    usbBufferedWrite(NUM2, 38);
  } else if (WRITTEN_MODE_NUMBER == 3) {
    usbBufferedWrite(NUM3, 38);
  } else if (WRITTEN_MODE_NUMBER == 4) {
    usbBufferedWrite(NUM4, 38);
  } else if (WRITTEN_MODE_NUMBER == 5) {
    usbBufferedWrite(NUM5, 38);
  }
  /////
  usbBufferedWrite(fsnewline3, 3);
  usbBufferedWrite(USBSTRAP, 7);
#ifdef DONGLE
  usbBufferedWrite(NOTFOUND, 38);
#else
  if (USB_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 38);
  } else {
    usbBufferedWrite(NO, 38);
  }
#endif
  /////
  usbBufferedWrite(fsnewline3, 3);
  usbBufferedWrite(VOLPLUSSTRAP, 7);
#ifdef DONGLE
  usbBufferedWrite(NOTFOUND, 38);
#else
  if (VOLUP_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 38);
  } else {
    usbBufferedWrite(NO, 38);
  }
#endif
  /////
  usbBufferedWrite(fsnewline3, 3);
  usbBufferedWrite(JOYCONSTRAP, 7);
#ifdef DONGLE
  usbBufferedWrite(NOTFOUND, 38);
#else
  if (JOYCON_STRAP_TEST == 1) {
    usbBufferedWrite(YES, 38);
  } else {
    usbBufferedWrite(NO, 38);
  }
#endif
  /////
  usbBufferedWrite(fsnewline3, 3);
  usbBufferedWrite(BOARDNAME, 45);
  /////
  usbBufferedWrite(fsnewline3, 3);
  usbBufferedWrite(FSPART3, 1685);
  usbBufferedWrite(payload, 12);
  usbBufferedWrite(FSPART4, 8829);
  usbFlushBuffer();
}

void findTegraDevice(UsbDeviceDefinition *pdev)
{
  uint32_t address = pdev->address.devAddress;
  USB_DEVICE_DESCRIPTOR deviceDescriptor;
  if (usb.getDevDescr(address, 0, 0x12, (uint8_t *)&deviceDescriptor))
  {
    return;
  }

  if (deviceDescriptor.idVendor == 0x0955 && deviceDescriptor.idProduct == 0x7321)
  {
    tegraDeviceAddress = address;
    foundTegra = true;
  }
}

void setupTegraDevice()
{
  epInfo[0].epAddr = 0;
  epInfo[0].maxPktSize = 0x40;
  epInfo[0].epAttribs = USB_TRANSFER_TYPE_CONTROL;
  epInfo[0].bmNakPower = USB_NAK_MAX_POWER;
  epInfo[0].bmSndToggle = 0;
  epInfo[0].bmRcvToggle = 0;

  epInfo[1].epAddr = 0x01;
  epInfo[1].maxPktSize = 0x40;
  epInfo[1].epAttribs = USB_TRANSFER_TYPE_BULK;
  epInfo[1].bmNakPower = USB_NAK_MAX_POWER;
  epInfo[1].bmSndToggle = 0;
  epInfo[1].bmRcvToggle = 0;

  usb.setEpInfoEntry(tegraDeviceAddress, 2, epInfo);
  usb.setConf(tegraDeviceAddress, 0, 0);
  usb.Task();

  UHD_Pipe_Alloc(tegraDeviceAddress, 0x01, USB_HOST_PTYPE_BULK, USB_EP_DIR_IN, 0x40, 0, USB_HOST_NB_BK_1);

  
}

void isfitted() {
  if (!JOYCON_STRAP_TEST) {
    JOYCON_STRAP_TEST = 1;
    EEPROM_JOYCON_CONTROL_STRAP.write(JOYCON_STRAP_TEST);
  }
}

extern void wakeup();
extern void wakeup_reason();

void sleep(int errorCode) {
#ifdef ONBOARD_LED
  digitalWrite(ONBOARD_LED, LOW);
#endif
  setLedColor("black");


#ifdef WAKEUP_PIN
  attachInterrupt(WAKEUP_PIN, wakeup, RISING);
#endif
#ifdef MODCHIP
#ifdef VOLUP_STRAP_PIN
  attachInterrupt(VOLUP_STRAP_PIN, long_press, LOW);
#endif
#endif
#ifdef USB_LOW_RESET
  if (USB_STRAP_TEST == 1) attachInterrupt(USB_LOW_RESET, wakeup_reason, FALLING);
#endif
#ifdef JOYCON_STRAP_PIN
  if (!JOYCON_STRAP_TEST) {
    attachInterrupt(JOYCON_STRAP_PIN, isfitted, FALLING);
  }
#endif


  EIC->WAKEUP.vec.WAKEUPEN |= (1 << 6);

  if (AUTO_INCREASE_PAYLOAD_on == 1 && errorCode != 1) {
    ++WRITTEN_PAYLOAD_NUMBER;
    if (WRITTEN_PAYLOAD_NUMBER > 8) WRITTEN_PAYLOAD_NUMBER = 1;
    EEPROM_PAYLOAD_NUMBER.write(WRITTEN_PAYLOAD_NUMBER);
  }

  delayMicroseconds(DELAY_BEFORE_SLEEP);

  if (payload_indication_disabled == 0)
  {
    mode_payload_blink_led(1, WRITTEN_PAYLOAD_NUMBER);
  }

  foundTegra = false;

  #ifdef USB_LOGIC
  digitalWrite(USB_LOGIC, HIGH);
  #endif
  if (DISABLE_USB == 1) {

    USB->DEVICE.CTRLB.bit.DETACH = 1;
    USB->DEVICE.CTRLA.bit.ENABLE = 0;
    while (USB->DEVICE.SYNCBUSY.bit.ENABLE | USB->DEVICE.CTRLA.bit.ENABLE == 0);
  }
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; /* Enable deepsleep */

  GCLK->CLKCTRL.reg = uint16_t(
                        GCLK_CLKCTRL_CLKEN |
                        GCLK_CLKCTRL_GEN_GCLK2 |
                        GCLK_CLKCTRL_ID( GCLK_CLKCTRL_ID_EIC_Val )
                      );
  while (GCLK->STATUS.bit.SYNCBUSY) {}

  __DSB(); /* Ensure effect of last store takes effect */
  __WFI(); /* Enter sleep mode */
}
