#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// ── OLED Pin Config ───────────────────────────────────────
#define OLED_SDA  32
#define OLED_SCL  33

// ── Shared sensor data (written by sensor tasks, read by display task) ──
extern volatile int    g_bpm;           // AD8232 BPM (-1 = no reading)
extern volatile int    g_ecgRaw;        // AD8232 raw ADC value
extern volatile bool   g_leadsOff;      // AD8232 lead-off flag

extern volatile int32_t g_spo2;         // MAX30102 SpO2 %
extern volatile int32_t g_hr;           // MAX30102 heart rate
extern volatile bool    g_fingerOn;     // MAX30102 finger detected

extern volatile float   g_palmTemp;     // DS18B20 palm temp
extern volatile float   g_coreTemp;     // DS18B20 estimated core temp
extern volatile bool    g_tempReady;    // DS18B20 result available
extern volatile bool    g_measuring;    // DS18B20 countdown in progress
extern volatile int     g_countdown;    // DS18B20 seconds remaining

// ── Init & Task ───────────────────────────────────────────
void initDisplay(void);
void displayTask(void *pvParameters);

#endif // DISPLAY_H