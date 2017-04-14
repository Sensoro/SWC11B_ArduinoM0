#include <Scheduler.h>
#include <Wire.h>
#include "Swc11b.h"
#include "SHT3XD.h"

#define TX_INTERVAL   10000 // 10s

#define SHT3XD_I2CADDR      0X44

Swc11b swc11b;
SHT3XD tempHumiSensor(SHT3XD_I2CADDR);

bool mGetInfo  = true;
bool mTxData = false;
bool mTxDone = false;
bool mOnBleDtu = false;
bool mWaitNextTx = false;

unsigned long mTimeLast = 0;

char mSentResultBuff[16] = {0};
char mBleDtuDataBuff[128] = {0};

void swc11bSentCallback(char *result) {
  mTxDone = true;
  strcpy(mSentResultBuff, result);
}

void swc11bBleDtuCallback(char *result) {
  mOnBleDtu = true;
  strcpy(mBleDtuDataBuff, result);
}

void setup() {

  SerialUSB.begin(115200);
  while (!SerialUSB);
  SerialUSB.println("setup");

  Wire.begin();

  // Initializing sensor.
  if ( SHT3XD_NO_ERROR != tempHumiSensor.begin() ) {
    SerialUSB.println("Failed to begin temp&humi sensoro!");
    while (1);
  }
  SerialUSB.println("tempHumiSensor init success!");

  // Initializing swc11b.
  swc11b.setSentCallback(swc11bSentCallback);
  swc11b.setBleDtuCallback(swc11bBleDtuCallback);
  swc11b.begin();
  SerialUSB.println("swc11b init success!");

  Scheduler.startLoop(swc11bLoop);

  SerialUSB.println("setup done!");
}

void loop() {
  if (mGetInfo) {
    mGetInfo = false;
    if (SWC11B_OK != swc11b.ping()) {
      SerialUSB.println("swc11b.ping error!!!");
      return;
    }
    mTxData = true;
  }

  if (mTxData) {
    mTxData = false;
    SerialUSB.println("tx data");

    swc11b.wakeup();

    byte data[8] = {0};
    SHT3XD_Result shtResult = tempHumiSensor.readTempAndHumidity(REPEATABILITY_LOW, MODE_CLOCK_STRETCH, 50);
    if ( SHT3XD_NO_ERROR == shtResult.error) {
      SerialUSB.print("temp ="); SerialUSB.println(shtResult.t);
      SerialUSB.print("humi ="); SerialUSB.println(shtResult.rh);
      memcpy(data, &shtResult.t, 4);
      memcpy(&data[4], &shtResult.rh, 4);
    }

    // Sending data via the swc11b
    if (SWC11B_OK != swc11b.send(data, sizeof(data), 0)) {
      SerialUSB.println("swc11b.send error!!!");
      return;
    }
  }

  if (mTxDone) {
    mTxDone = false;
    SerialUSB.println("tx done");

    int result = 0;
    sscanf(mSentResultBuff, "+SEND:%u", &result);
    SerialUSB.print("+SEND:");SerialUSB.println(result);
    memset(mSentResultBuff, 0, sizeof(mSentResultBuff));

    // Checking if there is data available
    int availableLen = 0;
    if (SWC11B_OK != swc11b.available(&availableLen)) {
      SerialUSB.println("swc11b.available error!!!");
      return;
    }

    if (availableLen > 0) {
      SerialUSB.print(availableLen);SerialUSB.println(" bytes data available");

      // Reading data
      byte data[64];
      if (SWC11B_OK != swc11b.readBytes(data, availableLen)) {
        SerialUSB.println("swc11b.readBytes error!!!");
        return;
      }
      SerialUSB.print("read data = ");
      dumpData(data, availableLen);
    } else {
      SerialUSB.println("no data available");
    }

    if (SWC11B_OK != swc11b.sleep()) {
      SerialUSB.println("swc11b.sleep error!!!");
      return;
    }

    SerialUSB.println("waiting next tx\r\n");
    mWaitNextTx = true;
    mTimeLast = millis();
  }

  if (mOnBleDtu) {
    mOnBleDtu = false;
    SerialUSB.println("on ble data");

    byte hexBuff[64];
    int hexBuffLen = 0;

    // Decoding the 'hex string' format data, 5 bytes header, "+DTU:"
    hexBuffLen = hexStrDecode(&mBleDtuDataBuff[5], hexBuff);
    if (hexBuffLen != -1) {
      SerialUSB.print("decode data = ");
      dumpData(hexBuff, hexBuffLen);
    }
    memset(mBleDtuDataBuff, 0, sizeof(mBleDtuDataBuff));

    // Sending back the ble data
    if (SWC11B_OK != swc11b.sendBleDtu(hexBuff, hexBuffLen)) {
      SerialUSB.println("swc11b.sendBleDtu error!!! please check the ble connection state");
    }
  }

  if (mWaitNextTx) {
    // Waiting 10 s to next tx
    if ((millis() - mTimeLast) > TX_INTERVAL)
    {
      mWaitNextTx = false;
      mTxData = true;
    }
  }

  delay(1); // This delay must be called for scheduling other loop
}

void swc11bLoop() {
  swc11b.receiveBytes();
  delay(1); // This delay must be called for scheduling other loop
}

/** Function for decode a hex string to a buffer,
    Ex: "11223344" is decoded as {0x11,0x22,0x33,0x44}
*/
int hexStrDecode(char* pStrIn, byte * pOut)
{
  char* str = pStrIn;
  int strLen = 0;
  int index = 0;
  char c;
  uint8_t* p = pOut;
  strLen = strlen(str);
  if (strLen % 2 == 0) {
    for (int i = 0; i < strLen; ) {
      if ((str[i] >= '0') && (str[i] <= '9')) {
        c = str[i] - '0';
      } else if ((str[i] >= 'a') && (str[i] <= 'f')) {
        c = str[i] - 'a' + 0x0a;
      } else if ((str[i] >= 'A') && (str[i] <= 'F')) {
        c = str[i] - 'A' + 0x0a;
      } else {
        return -1;
      }
      c <<= 4;
      i++;
      if ((str[i] >= '0') && (str[i] <= '9')) {
        c |= str[i] - '0';
      } else if ((str[i] >= 'a') && (str[i] <= 'f')) {
        c |= str[i] - 'a' + 0x0a;
      } else if ((str[i] >= 'A') && (str[i] <= 'F')) {
        c |= str[i] - 'A' + 0x0a;
      } else {
        return -1;
      }
      i++;
      p[index++] = c;
    }
    return index;
  } else {
    return -1;
  }
}

void dumpData(void *pData, int len)
{
  byte *p = (byte *)pData;
  for (int i = 0; i < len; i++) {
    SerialUSB.print("0x");SerialUSB.print(p[i], HEX);SerialUSB.print(' ');
  }
  SerialUSB.println();
}
