/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ad8232.h"
#include <Arduino.h>

void initAd8232(void)
{
    pinMode(NOT_CONNECTED_RIGHT, INPUT);
    pinMode(NOT_CONNECTED_LEFT, INPUT);
    //pinMode(OUTPUT_AD8232, INPUT);

    analogSetPinAttenuation(OUTPUT_AD8232, ADC_11db);  // 0–3.3V range
}

void readAd8232Values(void *pvParameters)
{
    int value{};

    while(1)
    {
        if ((digitalRead(NOT_CONNECTED_RIGHT) == HIGH) || (digitalRead(NOT_CONNECTED_LEFT) == HIGH))
        {
            Serial.println("The Leads is not attached, connect it ......");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;

        }
        value = analogRead(OUTPUT_AD8232);

        //Serial.print("UL:");
        //Serial.print(4095);
        //Serial.print(",");

        Serial.print("AD8232:");
        Serial.println(value);
        //Serial.print(",");

        //Serial.print("LL:");
        //Serial.println(2300);

        vTaskDelay(pdMS_TO_TICKS(4));
    }
}*/

/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ad8232.h"
#include <Arduino.h>

// ── BPM config ──────────────────────────────────────────
#define WINDOW          5        // rolling average over 5 beats
#define MIN_RR          300      // 200 BPM max
#define MAX_RR          3000     // 20 BPM min
#define DYNAMIC_RATIO   0.6f     // threshold = min + (max-min) * 0.6
#define ADAPT_SAMPLES   50       // recalibrate min/max every N samples

// ── State ────────────────────────────────────────────────
static unsigned long rrIntervals[WINDOW] = {0};
static int           rrIndex    = 0;
static int           rrCount    = 0;
static unsigned long lastBeat   = 0;
static bool          peaked     = false;

// Dynamic threshold state
static int sampleBuf[ADAPT_SAMPLES];
static int sampleIndex  = 0;
static int dynThreshold = 550;   // initial fallback
static int sigMin       = 4095;
static int sigMax       = 0;

// ── Helpers ──────────────────────────────────────────────
static void updateThreshold(int val)
{
    sampleBuf[sampleIndex++] = val;

    if (sampleIndex >= ADAPT_SAMPLES)
    {
        sigMin = 4095;
        sigMax = 0;
        for (int i = 0; i < ADAPT_SAMPLES; i++)
        {
            if (sampleBuf[i] < sigMin) sigMin = sampleBuf[i];
            if (sampleBuf[i] > sigMax) sigMax = sampleBuf[i];
        }
        dynThreshold = sigMin + (int)((sigMax - sigMin) * DYNAMIC_RATIO);
        sampleIndex  = 0;
    }
}

static int computeBPM(unsigned long rr)
{
    rrIntervals[rrIndex % WINDOW] = rr;
    rrIndex++;
    rrCount++;

    if (rrCount < WINDOW) return -1;   // not enough data yet

    unsigned long sum = 0;
    for (int i = 0; i < WINDOW; i++)
        sum += rrIntervals[i];

    return (int)(60000UL / (sum / WINDOW));
}

// ── Init ─────────────────────────────────────────────────
void initAd8232(void)
{
    pinMode(NOT_CONNECTED_RIGHT, INPUT);
    pinMode(NOT_CONNECTED_LEFT,  INPUT);
    analogSetPinAttenuation(OUTPUT_AD8232, ADC_11db);  // 0–3.3V range
}

// ── Task ─────────────────────────────────────────────────
void readAd8232Values(void *pvParameters)
{
    int value{};

    while (1)
    {
        // Lead-off check
        if (digitalRead(NOT_CONNECTED_RIGHT) == HIGH ||
            digitalRead(NOT_CONNECTED_LEFT)  == HIGH)
        {
            Serial.println("Leads not attached, connect them...");

            // Reset BPM state on disconnect
            rrIndex  = 0;
            rrCount  = 0;
            lastBeat = 0;
            peaked   = false;
            sigMin   = 4095;
            sigMax   = 0;

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        value = analogRead(OUTPUT_AD8232);

        // Update dynamic threshold every ADAPT_SAMPLES readings
        updateThreshold(value);

        // ── Peak detection with hysteresis ──
        if (value > dynThreshold && !peaked)
        {
            unsigned long now = millis();
            unsigned long rr  = now - lastBeat;

            if (rr > MIN_RR && rr < MAX_RR)   // reject impossible beats
            {
                int bpm = computeBPM(rr);
                if (bpm > 0)
                {
                    Serial.print("BPM: ");
                    Serial.println(bpm);
                }
            }
            lastBeat = now;
            peaked   = true;
        }

        if (value < dynThreshold - 20)   // hysteresis — reset after peak
            peaked = false;

        // Still print raw signal for Serial Plotter
        //Serial.print("AD8232:");
        //Serial.println(value);

        vTaskDelay(pdMS_TO_TICKS(4));   // ~250 Hz
    }
}*/

/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ad8232.h"
#include "display.h"
#include <Arduino.h>

// ── BPM Config ───────────────────────────────────────────
#define WINDOW          8        // rolling average over 8 beats
#define MIN_RR          500      // 120 BPM max
#define MAX_RR          3000     // 20 BPM min
#define DYNAMIC_RATIO   0.80f    // threshold between T wave and R peak
#define ADAPT_SAMPLES   250      // recalibrate every ~1 second

// ── BPM State ────────────────────────────────────────────
static unsigned long rrIntervals[WINDOW] = {0};
static int           rrIndex             = 0;
static int           rrCount             = 0;
static unsigned long lastBeat            = 0;
static bool          peaked              = false;

// ── Dynamic Threshold State ──────────────────────────────
static int sampleBuf[ADAPT_SAMPLES];
static int sampleIndex  = 0;
static int dynThreshold = 2670;  // safe initial value based on your signal
static int sigMin       = 4095;
static int sigMax       = 0;

// ── Update Dynamic Threshold ─────────────────────────────
static void updateThreshold(int val)
{
    sampleBuf[sampleIndex++] = val;

    if (sampleIndex >= ADAPT_SAMPLES)
    {
        sigMin = 4095;
        sigMax = 0;
        for (int i = 0; i < ADAPT_SAMPLES; i++)
        {
            if (sampleBuf[i] < sigMin) sigMin = sampleBuf[i];
            if (sampleBuf[i] > sigMax) sigMax = sampleBuf[i];
        }
        dynThreshold = sigMin + (int)((sigMax - sigMin) * DYNAMIC_RATIO);
        sampleIndex  = 0;

        // Debug: uncomment to monitor threshold
        // Serial.print("Threshold:"); Serial.println(dynThreshold);
    }
}

// ── Compute BPM from RR Interval ─────────────────────────
static int computeBPM(unsigned long rr)
{
    rrIntervals[rrIndex % WINDOW] = rr;
    rrIndex++;
    rrCount++;

    if (rrCount < WINDOW) return -1;  // wait until window is full

    unsigned long sum = 0;
    for (int i = 0; i < WINDOW; i++)
        sum += rrIntervals[i];

    return (int)(60000UL / (sum / WINDOW));
}

// ── Init ─────────────────────────────────────────────────
void initAd8232(void)
{
    pinMode(NOT_CONNECTED_RIGHT, INPUT);
    pinMode(NOT_CONNECTED_LEFT,  INPUT);
    analogSetPinAttenuation(OUTPUT_AD8232, ADC_11db);  // 0–3.3V range
}

// ── Main Task ────────────────────────────────────────────
void readAd8232Values(void *pvParameters)
{
    int           value        = 0;
    unsigned long refractoryEnd = 0;

    while (1)
    {
        // ── Lead-off detection ───────────────────────────
        if (digitalRead(NOT_CONNECTED_RIGHT) == HIGH ||
            digitalRead(NOT_CONNECTED_LEFT)  == HIGH)
        {
            Serial.println("Leads not attached, connect them...");

            // Reset all state
            rrIndex       = 0;
            rrCount       = 0;
            lastBeat      = 0;
            peaked        = false;
            sigMin        = 4095;
            sigMax        = 0;
            sampleIndex   = 0;
            refractoryEnd = 0;

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── Read signal ──────────────────────────────────
        value = analogRead(OUTPUT_AD8232);

        // ── Update dynamic threshold ─────────────────────
        updateThreshold(value);

        unsigned long now = millis();
        int bpm = -1;

        // ── Peak detection (outside refractory period) ───
        if (now > refractoryEnd)
        {
            if (value > dynThreshold && !peaked)
            {
                unsigned long rr = now - lastBeat;

                if (rr > MIN_RR && rr < MAX_RR)
                {
                    bpm           = computeBPM(rr);
                    lastBeat      = now;
                    refractoryEnd = now + 500;  // 500ms lockout after beat
                }
                peaked = true;
            }

            if (value < dynThreshold - 50)  // hysteresis
                peaked = false;
        }

        // ── Serial output ────────────────────────────────
       //Serial.print("AD8232:");
        //Serial.print(value);
        if (bpm > 0)
        {
            Serial.print(",BPM:");
            Serial.println(bpm);
        }
        //else
        //{
          //  Serial.println();
        //}

        vTaskDelay(pdMS_TO_TICKS(4));  // 250 Hz sample rate
    }
}*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ad8232.h"
#include "display.h"   // ← shared globals
#include <Arduino.h>

// ── BPM Config ───────────────────────────────────────────
#define WINDOW          8
#define MIN_RR          500
#define MAX_RR          3000
#define DYNAMIC_RATIO   0.80f
#define ADAPT_SAMPLES   250

// ── BPM State ────────────────────────────────────────────
static unsigned long rrIntervals[WINDOW] = {0};
static int           rrIndex             = 0;
static int           rrCount             = 0;
static unsigned long lastBeat            = 0;
static bool          peaked              = false;

// ── Dynamic Threshold State ──────────────────────────────
static int sampleBuf[ADAPT_SAMPLES];
static int sampleIndex  = 0;
static int dynThreshold = 2670;
static int sigMin       = 4095;
static int sigMax       = 0;

static void updateThreshold(int val)
{
    sampleBuf[sampleIndex++] = val;
    if (sampleIndex >= ADAPT_SAMPLES)
    {
        sigMin = 4095; sigMax = 0;
        for (int i = 0; i < ADAPT_SAMPLES; i++)
        {
            if (sampleBuf[i] < sigMin) sigMin = sampleBuf[i];
            if (sampleBuf[i] > sigMax) sigMax = sampleBuf[i];
        }
        dynThreshold = sigMin + (int)((sigMax - sigMin) * DYNAMIC_RATIO);
        sampleIndex  = 0;
    }
}

static int computeBPM(unsigned long rr)
{
    rrIntervals[rrIndex % WINDOW] = rr;
    rrIndex++;
    rrCount++;
    if (rrCount < WINDOW) return -1;
    unsigned long sum = 0;
    for (int i = 0; i < WINDOW; i++) sum += rrIntervals[i];
    return (int)(60000UL / (sum / WINDOW));
}

void initAd8232(void)
{
    pinMode(NOT_CONNECTED_RIGHT, INPUT);
    pinMode(NOT_CONNECTED_LEFT,  INPUT);
    analogSetPinAttenuation(OUTPUT_AD8232, ADC_11db);
}

void readAd8232Values(void *pvParameters)
{
    int           value         = 0;
    unsigned long refractoryEnd = 0;

    while (1)
    {
        // ── Lead-off detection ───────────────────────────
        if (digitalRead(NOT_CONNECTED_RIGHT) == HIGH ||
            digitalRead(NOT_CONNECTED_LEFT)  == HIGH)
        {
            Serial.println("Leads not attached, connect them...");

            // ── Update display globals ───────────────────
            g_leadsOff = true;
            g_bpm      = -1;

            rrIndex = rrCount = 0;
            lastBeat = 0; peaked = false;
            sigMin = 4095; sigMax = 0;
            sampleIndex = 0; refractoryEnd = 0;

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        g_leadsOff = false;

        value      = analogRead(OUTPUT_AD8232);
        g_ecgRaw   = value;   // ← feed sparkline

        updateThreshold(value);

        unsigned long now = millis();
        int bpm = -1;

        if (now > refractoryEnd)
        {
            if (value > dynThreshold && !peaked)
            {
                unsigned long rr = now - lastBeat;
                if (rr > MIN_RR && rr < MAX_RR)
                {
                    bpm           = computeBPM(rr);
                    lastBeat      = now;
                    refractoryEnd = now + 500;
                    if (bpm > 0) g_bpm = bpm;  // ← update display global
                }
                peaked = true;
            }
            if (value < dynThreshold - 50) peaked = false;
        }

        if (bpm > 0)
        {
            Serial.print(",BPM:");
            Serial.println(bpm);
        }

        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ad8232.h"
#include "display.h"
#include <Arduino.h>

// ── BPM Config ───────────────────────────────────────────
#define WINDOW          3        // 3 beats for fast first reading
#define MIN_RR          500      // 120 BPM max
#define MAX_RR          3000     // 20 BPM min
#define DYNAMIC_RATIO   0.80f
#define ADAPT_SAMPLES   250

// ── BPM State ────────────────────────────────────────────
static unsigned long rrIntervals[WINDOW] = {0};
static int           rrIndex             = 0;
static int           rrCount             = 0;
static unsigned long lastBeat            = 0;
static bool          peaked              = false;

// ── Dynamic Threshold State ──────────────────────────────
static int sampleBuf[ADAPT_SAMPLES];
static int sampleIndex  = 0;
static int dynThreshold = 2048;  // neutral midpoint — adapts within first second
static int sigMin       = 4095;
static int sigMax       = 0;

static void updateThreshold(int val)
{
    sampleBuf[sampleIndex++] = val;
    if (sampleIndex >= ADAPT_SAMPLES)
    {
        sigMin = 4095; sigMax = 0;
        for (int i = 0; i < ADAPT_SAMPLES; i++) {
            if (sampleBuf[i] < sigMin) sigMin = sampleBuf[i];
            if (sampleBuf[i] > sigMax) sigMax = sampleBuf[i];
        }
        dynThreshold = sigMin + (int)((sigMax - sigMin) * DYNAMIC_RATIO);
        sampleIndex  = 0;
        Serial.print("Threshold updated: "); Serial.println(dynThreshold);
    }
}

static int computeBPM(unsigned long rr)
{
    rrIntervals[rrIndex % WINDOW] = rr;
    rrIndex++;
    rrCount++;
    if (rrCount < WINDOW) {
        // Window not full yet — show instant BPM from single beat
        // so screen doesn't stay blank during warmup
        return (int)(60000UL / rr);
    }
    unsigned long sum = 0;
    for (int i = 0; i < WINDOW; i++) sum += rrIntervals[i];
    return (int)(60000UL / (sum / WINDOW));
}

void initAd8232(void)
{
    pinMode(NOT_CONNECTED_RIGHT, INPUT);
    pinMode(NOT_CONNECTED_LEFT,  INPUT);
    analogSetPinAttenuation(OUTPUT_AD8232, ADC_11db);
}

void readAd8232Values(void *pvParameters)
{
    int           value         = 0;
    unsigned long refractoryEnd = 0;

    while (1)
    {
        // ── Lead-off detection ───────────────────────────
        if (digitalRead(NOT_CONNECTED_RIGHT) == HIGH ||
            digitalRead(NOT_CONNECTED_LEFT)  == HIGH)
        {
            g_leadsOff  = true;
            g_bpm       = -1;
            g_beatCount = 0;

            rrIndex = rrCount = 0;
            lastBeat = 0; peaked = false;
            sigMin = 4095; sigMax = 0;
            sampleIndex = 0; refractoryEnd = 0;
            dynThreshold = 2048;  // reset threshold on disconnect

            Serial.println("Leads not attached...");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        g_leadsOff = false;

        value    = analogRead(OUTPUT_AD8232);
        g_ecgRaw = value;

        updateThreshold(value);

        unsigned long now = millis();
        int bpm = -1;

        if (now > refractoryEnd)
        {
            if (value > dynThreshold && !peaked)
            {
                unsigned long rr = now - lastBeat;
                if (lastBeat > 0 && rr > MIN_RR && rr < MAX_RR)
                {
                    bpm           = computeBPM(rr);
                    lastBeat      = now;
                    refractoryEnd = now + 300;
                    g_beatCount++;
                    if (bpm > 20 && bpm < 220) {
                        g_bpm = bpm;
                        Serial.print("BPM:"); Serial.println(bpm);
                    }
                }
                else if (lastBeat == 0) {
                    // First beat detected — record timestamp, wait for second
                    lastBeat = now;
                    Serial.println("First beat detected — waiting for second...");
                }
                peaked = true;
            }
            if (value < dynThreshold - 50) peaked = false;
        }

        vTaskDelay(pdMS_TO_TICKS(4));  // 250 Hz
    }
}*/