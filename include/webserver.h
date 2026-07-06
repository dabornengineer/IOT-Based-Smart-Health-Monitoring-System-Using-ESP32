/*#ifndef MY_WEBSERVER_H
#define MY_WEBSERVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ── ECG circular buffer ───────────────────────────────────────────────────────
#define ECG_BUF_SIZE 200

extern int                g_ecgBuf[ECG_BUF_SIZE];
extern volatile int       g_ecgHead;
extern SemaphoreHandle_t  g_webMutex;

// ── Called from readAd8232Values() instead of writing g_ecgRaw ───────────────
void pushEcgSample(int value);

// ── Spawn this from app_main via xTaskCreate ──────────────────────────────────
void webTask(void *pvParameters);
void initWebserver(void);

#endif*/
#ifndef MY_WEBSERVER_H
#define MY_WEBSERVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define ECG_BUF_SIZE 200

extern int                g_ecgBuf[ECG_BUF_SIZE];
extern volatile int       g_ecgHead;
extern SemaphoreHandle_t  g_webMutex;

void pushEcgSample(int value);   // call from ad8232 task
void webTask(void *pvParameters); // spawn from app_main

#endif