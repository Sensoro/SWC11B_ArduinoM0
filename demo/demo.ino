#include "ZeroTimer.h"
#include <Wire.h>
#include "Swc11b.h"
#include "SHT3XD.h"
#include "log.h"

#define TX_INTERVAL   10000 // 10s
#define SHT3XD_I2CADDR      0X44
#define LEDPIN              13

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

  log_init();  
  log_println("setup");
  
  Wire.begin();

  // Initializing sensor.
  if ( SHT3XD_NO_ERROR != tempHumiSensor.begin() ) {
    log_println("Failed to begin temp&humi sensoro!");
    while (1);
  }
  log_println("tempHumiSensor init success!");

  // Initializing swc11b.
  swc11b.setSentCallback(swc11bSentCallback);
  swc11b.setBleDtuCallback(swc11bBleDtuCallback);
  swc11b.begin();
  log_println("swc11b init success!");

  // call swc11bLoop evert 10ms
  TC.startTimer(10000, swc11bLoop);

  log_println("setup done!\n");
}

void loop() {
  if (mGetInfo) {
    mGetInfo = false;
    if (SWC11B_OK != swc11b.ping()) {
      log_println("swc11b.ping error!!!");
      NVIC_SystemReset();
    }
    mTxData = true;
  }

  if (mTxData) {
    mTxData = false;

    log_println("wake up swc11b");
    if (SWC11B_OK != swc11b.wakeup()) {
      log_println("wakeup error!!!");
      NVIC_SystemReset();
    }

    // Getting temperature and humidity from sensor.
    log_println("read sensor data");
    byte data[8] = {0};
    SHT3XD_Result shtResult = tempHumiSensor.readTempAndHumidity(REPEATABILITY_LOW, MODE_CLOCK_STRETCH, 50);
    if ( SHT3XD_NO_ERROR == shtResult.error) {
      log_print("temp ="); log_println(shtResult.t);
      log_print("humi ="); log_println(shtResult.rh);

      // Putting data in buffer
      memcpy(data, &shtResult.t, 4);
      memcpy(&data[4], &shtResult.rh, 4);
    }

    // Setting ble advertising data.
    log_println("set ble advertising");
    if (SWC11B_OK != swc11b.setBleAdvertisingData(data, sizeof(data))) {
      log_println("swc11b.setBleAdvertisingData error!!!");
      NVIC_SystemReset();
    }

    // Sending data via the swc11b
    log_println("send data");
    if (SWC11B_OK != swc11b.send(data, sizeof(data), 0)) {
      log_println("swc11b.send error!!!");
      NVIC_SystemReset();
    }
  }

  if (mTxDone) {
    mTxDone = false;
    log_println("send done");

    int result = 0;
    sscanf(mSentResultBuff, "+SEND:%u", &result);
    log_print("+SEND:"); log_println(result);
    memset(mSentResultBuff, 0, sizeof(mSentResultBuff));

    // Checking if there is data available
    int availableLen = 0;
    if (SWC11B_OK != swc11b.available(&availableLen)) {
      log_println("swc11b.available error!!!");
      NVIC_SystemReset();
    }

    if (availableLen > 0) {
      log_print(availableLen); log_println(" bytes data available");

      // Reading data
      byte data[64];
      if (SWC11B_OK != swc11b.readBytes(data, availableLen)) {
        log_println("swc11b.readBytes error!!!");
        NVIC_SystemReset();
      }
      log_print("read data = ");
      log_dump(data, availableLen);
    } else {
      log_println("no data available");
    }

    if (SWC11B_OK != swc11b.sleep()) {
      log_println("swc11b.sleep error!!!");
      NVIC_SystemReset();
    }

    log_println("waiting next tx\r\n");
    mWaitNextTx = true;
    mTimeLast = millis();
  }

  if (mOnBleDtu) {
    mOnBleDtu = false;
    log_println("on ble data");

    byte hexBuff[64];
    int hexBuffLen = 0;

    // Decoding the 'hex string' format data, 5 bytes header, "+DTU:"
    hexBuffLen = hexStrDecode(&mBleDtuDataBuff[5], hexBuff);
    if (hexBuffLen != -1) {
      log_print("decode data = ");
      log_dump(hexBuff, hexBuffLen);
    }
    memset(mBleDtuDataBuff, 0, sizeof(mBleDtuDataBuff));

    // Sending back the ble data
    if (SWC11B_OK != swc11b.sendBleDtu(hexBuff, hexBuffLen)) {
      log_println("swc11b.sendBleDtu error!!! please check the ble connection state");
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
}

void swc11bLoop() {
  swc11b.receiveBytes();
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

