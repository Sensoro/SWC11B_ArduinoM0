#ifndef __SWC11B_H_
#define __SWC11B_H_

#include <Arduino.h>

#define SWC11B_OK             0
#define SWC11B_ERR_TIMEOUT    1
#define SWC11B_ERR_UNKNOWN    2

#define LINE_SIZE             128

#define LINE_FIFO_SUCCESS     0
#define LINE_FIFO_ERROR_NULL  1

#define LINE_FIFO_SIZE        8 /* must be power of two */
#define LINE_FIFO_MASK        (LINE_FIFO_SIZE - 1)

/** Structure to store line data
*/
typedef struct
{
  uint16_t  len;
  uint8_t   data[LINE_SIZE];
} line_t;

typedef void (*SentCallback_t)(char *result);

typedef void (*BleDtuCallback_t)(char *result);

class Swc11b {
  private:
    int _wakeupPin;
    int _resetPin;
    int _indicationPin;

    line_t _lineFifo[LINE_FIFO_SIZE];
    int _lineFifoHead;
    int _lineFifoTail;

    byte _lineBuff[LINE_SIZE];
    int _lineBuffLen;

    SentCallback_t _sentCb = NULL;
    BleDtuCallback_t _bleDtuCb = NULL;

  private:
    bool lineFifoFull(void);
    bool lineFifoEmpty(void);
    int lineFifoGetLength(void);
    int lineFifoPut(line_t* line);
    int lineFifoGet(line_t* line);
    void lineFifoFlush(void);

    int waitResponse(const char *pResp, byte *pOutData, int *pOutLen, unsigned long timeout);
    int waitChunkResponse(int len, byte* pOutBuff, int* pOutLen, unsigned long timeout);
    void sendCmdWithoutResponse(const char *pCmd);
    void sendDataWithoutResponse(byte *pData, int dataLen);
    int sendCmd(const char *pCmd, const char *pResp, unsigned long timeout);
    int sendData(byte *pData, int dataLen, const char *pResp, unsigned long timeout);

  public:

    /** Construction function
    */
    Swc11b();

    /** Function for setting callback on Swc11b sent data.
    */
    void setSentCallback(SentCallback_t callback);

    /** Function for setting callback on Swc11b revceiving ble data
    */
    void setBleDtuCallback(BleDtuCallback_t callback);

    /** Function for initializing a Swc11b instance
    */
    void begin(void);

    /** Function for checking the communication between Arduino and Swc11b

        @return   SWC11B_OK             Command is executed successfully
                  SWC11B_ERR_TIMEOUT    Timeout to receive command response
                  SWC11B_ERR_UNKNOWN    Receiving unknown command response
    */
    int ping(void);

    /** Function for wakeup the Swc11b
    */
    void reset(void);

    /** Function for resetting the Swc11b
    */
    int wakeup(void);

    /** Function for make the Swc11b enter sleep mode
    */
    int sleep(void);

    /** Function for receiving data from Swc11b

        @note This function must be called in another loop or thread.
              At this demo, we use library 'Scheduler'to run a new loop to call it
    */
    void receiveBytes(void);

    /** Function for sending data via Swc11b

        @param[in]    pData   Pointer of the data to send
        @param[in]    len     The length of data to send
        @param[in]    ack     0:don't need network server ack this sending
                              1:need network server ack this sending

        @return   SWC11B_OK             Command is executed successfully
                  SWC11B_ERR_TIMEOUT    Timeout to receive command response
                  SWC11B_ERR_UNKNOWN    Receiving unknown command response
    */
    int send(byte *pData, int len, int ack);

    /** Fuction for checking if the Swc11b receive data from network server

        @param[out]   pLen    The length of data Swc11b received and cached

        @return   SWC11B_OK             Command is executed successfully
                  SWC11B_ERR_TIMEOUT    Timeout to receive command response
                  SWC11B_ERR_UNKNOWN    Receiving unknown command response
    */
    int available(int *pLen);

    /** Function for reading data cached in Swc11b.

        @param[out]   pData   Pointer of the buffer to store the data
        @param[in]    len     Length of data to read

        @return   SWC11B_OK             Command is executed successfully
                  SWC11B_ERR_TIMEOUT    Timeout to receive command response
                  SWC11B_ERR_UNKNOWN    Receiving unknown command response
    */
    int readBytes(byte *pData, int len);

    /** Function for send data via ble interface of Swc11b.

        @param[in]   pData   Pointer of the buffer to send
        @param[in]    len    Length of data to send

        @return   SWC11B_OK             Command is executed successfully
                  SWC11B_ERR_TIMEOUT    Timeout to receive command response
                  SWC11B_ERR_UNKNOWN    Receiving unknown command response
    */
    int sendBleDtu(byte *pData, int len);

     /** Function for setting ble advertising data of Swc11b

        @param[in]   pData   Pointer of the data buffer
        @param[in]    len    Length of data

        @return   SWC11B_OK             Command is executed successfully
                  SWC11B_ERR_TIMEOUT    Timeout to receive command response
                  SWC11B_ERR_UNKNOWN    Receiving unknown command response
    */
    int setBleAdvertisingData(byte *pData, int len);

};

#endif

