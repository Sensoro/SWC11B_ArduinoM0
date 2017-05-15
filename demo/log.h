#ifndef __LOG_H_
#define __LOG_H_

#include <Arduino.h>

// Uncomment this macro to open log via SerialUSB
//#define LOG

void _log_init(void);
void _log_dump(void *pData, int len);

#ifdef LOG
#define log_init      _log_init
#define log_print     SerialUSB.print    
#define log_println   SerialUSB.println
#define log_dump      _log_dump
#else
#define log_init(...)
#define log_print(...)   
#define log_println(...)
#define log_dump(...)
#endif


#endif

