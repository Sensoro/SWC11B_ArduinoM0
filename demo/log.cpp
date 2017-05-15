#include "log.h"
#include <Arduino.h>

void _log_init(void){
  SerialUSB.begin(115200);
  while (!SerialUSB);
}

void _log_dump(void *pData, int len){
  byte *p = (byte *)pData;
  for (int i = 0; i < len; i++) {
    SerialUSB.print("0x"); SerialUSB.print(p[i], HEX); SerialUSB.print(' ');
  }
  SerialUSB.println();
}
