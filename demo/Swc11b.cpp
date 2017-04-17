#include "Swc11b.h"

#define RESET_PIN   3
#define WAKEUP_PIN  A0
#define INDICATION_PIN A1


/** Construction function
*/
Swc11b::Swc11b():
  _wakeupPin(WAKEUP_PIN),
  _resetPin(RESET_PIN),
  _indicationPin(INDICATION_PIN)
{
}

bool Swc11b::lineFifoFull(void)
{
  return ((_lineFifoTail + LINE_FIFO_SIZE) == _lineFifoHead);
}

bool Swc11b::lineFifoEmpty(void)
{
  return (_lineFifoHead == _lineFifoTail);
}

int Swc11b::lineFifoGetLength(void)
{
  return (_lineFifoHead - _lineFifoTail) & 0xFF;
}

int Swc11b::lineFifoPut(line_t* line)
{
  if (lineFifoFull()) {
    return -1;
  }
  line_t* head = &_lineFifo[_lineFifoHead & LINE_FIFO_MASK];
  memcpy(head, line, sizeof(line_t));
  return ((_lineFifoHead++) & LINE_FIFO_MASK);
}

int Swc11b::lineFifoGet(line_t* line)
{
  if (lineFifoEmpty()) {
    return LINE_FIFO_ERROR_NULL;
  }
  line_t* tail = &_lineFifo[_lineFifoTail & LINE_FIFO_MASK];
  memcpy(line, tail, sizeof(line_t));
  ++_lineFifoTail;
  return LINE_FIFO_SUCCESS;
}

void Swc11b::lineFifoFlush(void)
{
  _lineFifoTail = _lineFifoHead;
}

int Swc11b::waitResponse(const char *pResp, byte *pOutData, int *pOutLen, unsigned long timeout)
{
  line_t line;
  unsigned long timeStart = 0;
  timeStart = millis();
  while (1) {
    if (LINE_FIFO_SUCCESS == lineFifoGet(&line)) {
      break;
    }
    if ((timeout == 0) || ((millis() - timeStart ) > timeout)) {
      return SWC11B_ERR_TIMEOUT;
    }
    delay(1);
  }
  if (!memcmp(line.data, pResp, strlen(pResp))) {
    if (pOutData != NULL && pOutLen != NULL) {
      memcpy(pOutData, line.data, line.len);
      *pOutLen = line.len;
    }
    return SWC11B_OK;
  }
  return SWC11B_ERR_UNKNOWN;
}

int Swc11b::waitChunkResponse(int len, byte* pOutBuff, int* pOutLen, unsigned long timeout)
{
  line_t line;
  unsigned long timeStart = 0;
  timeStart = millis();
  while (1) {
    if (LINE_FIFO_SUCCESS == lineFifoGet(&line)) {
      break;
    }
    if ((timeout == 0) || ((millis() - timeStart ) > timeout)) {
      return SWC11B_ERR_TIMEOUT;
    }
    delay(1);
  }
  if (line.len >= len) {
    if (pOutBuff != NULL && pOutLen != NULL) {
      memcpy(pOutBuff, line.data, line.len);
      *pOutLen = line.len;
    }
    return SWC11B_OK;
  }
  return SWC11B_ERR_UNKNOWN;
}

void Swc11b::sendCmdWithoutResponse(const char *pCmd)
{
  const char crcf[] = "\r\n";
  Serial1.write(pCmd, strlen(pCmd));
  Serial1.write(crcf);
}

void Swc11b::sendDataWithoutResponse(byte *pData, int dataLen)
{
  Serial1.write(pData, dataLen);
}

int Swc11b::sendCmd(const char *pCmd, const char *pResp, unsigned long timeout)
{
  sendCmdWithoutResponse(pCmd);
  return waitResponse(pResp, NULL, NULL, timeout);
}

int Swc11b::sendData(byte *pData, int dataLen, const char *pResp, unsigned long timeout)
{
  sendDataWithoutResponse(pData, dataLen);
  return waitResponse(pResp, NULL, NULL, timeout);
}

/** Function for setting callback on Swc11b sent data.
*/
void Swc11b::setSentCallback(SentCallback_t callback)
{
  _sentCb = callback;
}

/** Function for setting callback on Swc11b revceiving ble data
*/
void Swc11b::setBleDtuCallback(BleDtuCallback_t callback)
{
  _bleDtuCb = callback;
}

/** Function for initializing a Swc11b instance
*/
void Swc11b::begin(void)
{
  pinMode(_resetPin, OUTPUT);
  pinMode(_wakeupPin, OUTPUT);
  pinMode(_indicationPin, INPUT);
  
  digitalWrite(_resetPin, HIGH);
  digitalWrite(_wakeupPin, HIGH);
  reset();

  lineFifoFlush();
  _lineBuffLen = 0;
  Serial1.begin(9600);
}

/** Function for checking the communication between Arduino and Swc11b
*/
int  Swc11b::ping(void)
{
  int ret;
  ret = sendCmd("AT", "OK", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  return SWC11B_OK;
}

/** Function for resetting the Swc11b
*/
void Swc11b::reset(void)
{
  digitalWrite(_resetPin, LOW);
  delay(5);
  digitalWrite(_resetPin, HIGH);
  delay(100);
}

/** Function for wakeup the Swc11b
*/
int Swc11b::wakeup(void)
{
  int timeout = 500;
  unsigned long timeStart = 0;
  timeStart = millis();
  
  digitalWrite(_wakeupPin, LOW);
  delay(100);
  digitalWrite(_wakeupPin, HIGH);

   while (digitalRead(_indicationPin) == LOW) {
    if ((timeout == 0) || ((millis() - timeStart ) > timeout)) {
      return SWC11B_ERR_TIMEOUT;
    }
    delay(1);
  }
  return SWC11B_OK;
}

/** Function for make the Swc11b enter sleep mode
*/
int Swc11b::sleep(void)
{
  int ret;
  ret = sendCmd("AT+SLEEP", "OK", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  return SWC11B_OK;
}

/** Function for sending data via Swc11b
*/
int Swc11b::send(byte *pData, int len, int ack)
{
  int ret;
  char buff[50] = {0};

  sprintf(buff, "AT+SEND=%u,%u", len, ack);
  ret = sendCmd(buff, ">>>", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  ret = sendData(pData, len, "OK", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  return SWC11B_OK;
}

/** Fuction for checking if the Swc11b receive data from network server
*/
int Swc11b::available(int *pLen)
{
  int ret;
  byte buff[16] = {0};
  int buffLen = 0;
  int dataLen = 0;
  const char cmd[] = "AT+DQ?";

  sendCmdWithoutResponse(cmd);
  ret = waitResponse("+DQ:", buff, &buffLen, 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  ret = waitResponse("OK", NULL, NULL, 100);
  if (SWC11B_OK != ret) {
    return ret;
  }
  sscanf((char *)buff, "+DQ:%u", &dataLen);
  *pLen = dataLen;
  return SWC11B_OK;
}

/** Function for reading data cached in Swc11b.
*/
int Swc11b::readBytes(byte *pData, int len)
{
  int ret;
  byte buff[16];
  int buffLen = 0;
  const char cmd[] = "AT+RCV=1";

  sendCmdWithoutResponse(cmd);
  ret = waitChunkResponse(len, buff, &buffLen, 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  ret = waitResponse("OK", NULL, NULL, 100);
  if (SWC11B_OK != ret) {
    return ret;
  }
  memcpy(pData, buff, len);
  return SWC11B_OK;
}

/** Function for sending data via ble interface of Swc11b.
*/
int Swc11b::sendBleDtu(byte *pData, int len)
{
  int ret;
  byte cmd[] = "AT+DTU=";
  byte hexBuff[2] = {0};
  byte crcf[] = "\r\n";

  sendDataWithoutResponse(cmd, strlen((char *)cmd));
  for (int i = 0; i < len; i++) {
    sprintf((char *)hexBuff, "%02X", pData[i]);
    sendDataWithoutResponse(hexBuff, 2);
  }
  ret = sendData(crcf, 2, "OK", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  return SWC11B_OK;
}

/** Function for setting ble advertising data of Swc11b
*/
int Swc11b::setBleAdvertisingData(byte *pData, int len)
{
  int ret;
  char buff[50] = {0};

  sprintf(buff, "AT+BD=%u", len);
  ret = sendCmd(buff, ">>>", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  ret = sendData(pData, len, "OK", 500);
  if (SWC11B_OK != ret) {
    return ret;
  }
  return SWC11B_OK;
}

/** Function for receiving data from Swc11b
*/
void Swc11b::receiveBytes(void)
{
  static bool crGet = false;
  byte ch;
  while (Serial1.available()) {
    ch = Serial1.read();
    _lineBuff[_lineBuffLen++] = ch;

    if (crGet && ch != '\n') {
      crGet = false;
    }
    if (ch == '\r') {
      crGet = true;
    }
    if (crGet && ch == '\n') {
      crGet = false;

      // delete "\r\n"
      _lineBuff[_lineBuffLen - 1] = 0;
      _lineBuff[_lineBuffLen - 2] = 0;

      // Checking whether the buffer is a ble dtu data
      if (memcmp(_lineBuff, "+DTU:", 5) == 0) {
        if (_bleDtuCb != NULL) {
          _bleDtuCb((char *)_lineBuff);
        }
      }
      // Checking whether the buffer is the result of transmission
      else if (memcmp(_lineBuff, "+SEND:", 6) == 0) {
        if (_sentCb != NULL) {
          _sentCb((char *)_lineBuff);
        }
      }
      else {
        line_t line;
        line.len = _lineBuffLen;
        memcpy(line.data, _lineBuff, _lineBuffLen);
        if (-1 == lineFifoPut(&line)) {
          // Fifo full, do something
        }
      }
      memset(_lineBuff, 0, sizeof(_lineBuff));
      _lineBuffLen = 0;
    }
  }
}

