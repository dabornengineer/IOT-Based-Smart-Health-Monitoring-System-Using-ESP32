/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include "max30102.h"
#include <Arduino.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"


MAX30105 particleSensor;
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

void initMAX30102(void)
{
    Wire.begin();

    while (!particleSensor.begin(Wire, I2C_SPEED_FAST))  //use default I2C port, 400kHz
    {
        Serial.println("MAX30102 was not found!, please check wiring/power");
        vTaskDelay(pdMS_TO_TICKS(800));
    }

    Serial.println("Attach sensor to finger with rubber band.");

    particleSensor.setup(
        ledBrightness,
        MY_SAMPLE_AVERAGE,
        MY_LED_MODE,
        MY_SAMPLE_RATE,
        MY_PULSE_WIDTH,
        MY_ADC_RANGE);
}*/

/*void sp02Reading(void *pvParameters)
{
    
    
    bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

    //read the first 100 samples, and determine the signal range
    for (byte i = 0 ; i < bufferLength ; i++)
    {
        while (!particleSensor.available()) //do we have new data?
        {
            particleSensor.check(); //Check the sensor for new data
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample(); //We're finished with this sample so move to next sample

        Serial.print(F("red="));
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", ir="));
        Serial.println(irBuffer[i], DEC);
    }

    //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
    while (1)
    {
        //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
        for (byte i = 25; i < 100; i++)
        {
        redBuffer[i - 25] = redBuffer[i];
        irBuffer[i - 25] = irBuffer[i];
        }

        //take 25 sets of samples before calculating the heart rate.
        for (byte i = 75; i < 100; i++)
        {
        while (particleSensor.available() == false) //do we have new data?
            particleSensor.check(); //Check the sensor for new data

        //digitalWrite(readLED, !digitalRead(readLED)); //Blink onboard LED with every data read

        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample(); //We're finished with this sample so move to next sample

        //send samples and calculation result to terminal program through UART
        //Serial.print(F("red="));
        //Serial.print(redBuffer[i], DEC);
        //Serial.print(F(", ir="));
        //Serial.print(irBuffer[i], DEC);

        //Serial.print(F(", HR="));
        //Serial.print(heartRate, DEC);

        //Serial.print(F(", HRvalid="));
        //Serial.print(validHeartRate, DEC);

        Serial.print(F(", SPO2="));
        Serial.print(spo2, DEC);

        Serial.print(F(", SPO2Valid="));
        Serial.println(validSPO2, DEC);
        }

        //After gathering 25 new samples recalculate HR and SP02
        maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    }
    
}*/
/*void sp02Reading(void *pvParameters)
{
    bufferLength = 100;

    // ── state tracking ──────────────────────────────────────────
    const int   AVERAGE_COUNT   = 5;      // readings to average before displaying
    int32_t     spo2Readings[AVERAGE_COUNT];
    int32_t     hrReadings[AVERAGE_COUNT];
    int         readingIndex    = 0;
    bool        fingerWasAbsent = true;   // tracks previous finger state

    // ── fill initial 100-sample buffer ──────────────────────────
    for (byte i = 0; i < bufferLength; i++)
    {
        while (!particleSensor.available())
        {
            particleSensor.check();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample();
    }

    maxim_heart_rate_and_oxygen_saturation(
        irBuffer, bufferLength, redBuffer,
        &spo2, &validSPO2, &heartRate, &validHeartRate);

    // ── main loop ───────────────────────────────────────────────
    while (1)
    {
        // shift oldest 25 samples out, keep newest 75
        for (byte i = 25; i < 100; i++)
        {
            redBuffer[i - 25] = redBuffer[i];
            irBuffer[i - 25]  = irBuffer[i];
        }

        // collect 25 fresh samples
        for (byte i = 75; i < 100; i++)
        {
            while (!particleSensor.available())
                particleSensor.check();

            redBuffer[i] = particleSensor.getRed();
            irBuffer[i]  = particleSensor.getIR();
            particleSensor.nextSample();
        }

        // recalculate with the fresh window
        maxim_heart_rate_and_oxygen_saturation(
            irBuffer, bufferLength, redBuffer,
            &spo2, &validSPO2, &heartRate, &validHeartRate);

        // ── finger detection ────────────────────────────────────
        bool fingerPresent = (validSPO2 == 1 && spo2 > 0);

        if (!fingerPresent)
        {
            // finger removed — reset averaging state
            if (!fingerWasAbsent)          // print once on transition
            {
                Serial.println("No finger detected — please place finger on sensor.");
            }
            else
            {
                // keep reprinting so the user sees the prompt
                Serial.println("No finger detected — please place finger on sensor.");
            }

            readingIndex    = 0;           // reset accumulator
            fingerWasAbsent = true;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;                      // skip averaging logic below
        }

        // ── finger is present: accumulate readings ───────────────
        if (fingerWasAbsent)
        {
            // first detection after absence — tell user we're acquiring
            Serial.println("Finger detected — acquiring readings, please hold still...");
            fingerWasAbsent = false;
            readingIndex    = 0;           // start fresh
        }

        spo2Readings[readingIndex] = spo2;
        hrReadings[readingIndex]   = heartRate;
        readingIndex++;

        Serial.print("Acquiring reading ");
        Serial.print(readingIndex);
        Serial.print(" / ");
        Serial.println(AVERAGE_COUNT);

        // ── once we have AVERAGE_COUNT good readings, display avg ─
        if (readingIndex >= AVERAGE_COUNT)
        {
            int32_t avgSpo2 = 0, avgHR = 0;
            for (int i = 0; i < AVERAGE_COUNT; i++)
            {
                avgSpo2 += spo2Readings[i];
                avgHR   += hrReadings[i];
            }
            avgSpo2 /= AVERAGE_COUNT;
            avgHR   /= AVERAGE_COUNT;

            Serial.println("──────────────────────────");
            Serial.print("SpO2 : ");
            Serial.print(avgSpo2);
            Serial.println(" %");
            Serial.print("Heart Rate: ");
            Serial.print(avgHR);
            Serial.println(" bpm");
            Serial.println("──────────────────────────");

            readingIndex = 0;   // reset so next batch of 5 starts fresh
        }
    }
}
*/
// ─────────────────────────────────────────────────────────────
//  max30102.cpp  —  SpO2 / Heart-rate reader (improved)
//
//  Improvements over baseline:
//    1. IR threshold check  — rejects weak / lifted-finger signal
//    2. Trimmed mean        — drops outlier high/low before averaging
//    3. Larger average pool — AVERAGE_COUNT = 7, middle 5 used
//    4. Named constants     — no magic numbers in logic
//    5. SensorData_t struct — all sensor state in one place
// ─────────────────────────────────────────────────────────────

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Arduino.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "max30102.h"

// ── Constants ────────────────────────────────────────────────
#define BUFFER_LENGTH       100          // total ring-buffer size (4 s at 25 sps)
#define SHIFT_AMOUNT        25           // samples dumped each cycle
#define REFILL_START        (BUFFER_LENGTH - SHIFT_AMOUNT)  // = 75

#define AVERAGE_COUNT       7            // readings collected before output
#define TRIM_DROP           1            // drop this many from each end (trimmed mean)
#define TRIM_KEEP           (AVERAGE_COUNT - 2 * TRIM_DROP)  // = 5 used in average

#define MIN_IR_THRESHOLD    30000UL      // minimum IR value for a valid finger contact
                                         // raise if you get false "finger present" readings;
                                         // lower if a firm press still triggers weak-signal

// ── Sensor configuration ─────────────────────────────────────
#define ledBrightness       60
#define MY_SAMPLE_AVERAGE   4
#define MY_LED_MODE         2
#define MY_SAMPLE_RATE      100
#define MY_PULSE_WIDTH      411
#define MY_ADC_RANGE        4096

// ── Sensor state struct ───────────────────────────────────────
//    All mutable sensor data lives here instead of scattered globals.
//    If you later add a display or BLE task, protect this with a mutex.
typedef struct {
    uint32_t irBuffer[BUFFER_LENGTH];
    uint32_t redBuffer[BUFFER_LENGTH];
    int32_t  bufferLength;

    int32_t  spo2;
    int8_t   validSPO2;
    int32_t  heartRate;
    int8_t   validHeartRate;
} SensorData_t;

static SensorData_t sd;           // single instance, file-scope
MAX30105 particleSensor;

// ── Helpers ───────────────────────────────────────────────────

// Simple insertion sort (fine for AVERAGE_COUNT ≤ 10)
static void sortArray(int32_t *arr, int n)
{
    for (int i = 1; i < n; i++) {
        int32_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// Trimmed mean: sort, drop TRIM_DROP from each end, average the rest
static int32_t trimmedMean(int32_t *src, int n)
{
    int32_t tmp[AVERAGE_COUNT];
    memcpy(tmp, src, n * sizeof(int32_t));
    sortArray(tmp, n);

    int64_t sum = 0;
    for (int i = TRIM_DROP; i < n - TRIM_DROP; i++)
        sum += tmp[i];

    return (int32_t)(sum / TRIM_KEEP);
}

// ── Init ──────────────────────────────────────────────────────
void initMAX30102(void)
{
    Wire.begin();

    while (!particleSensor.begin(Wire, I2C_SPEED_FAST))
    {
        Serial.println("MAX30102 not found — check wiring/power.");
        vTaskDelay(pdMS_TO_TICKS(800));
    }

    Serial.println("Sensor ready. Attach to finger with rubber band.");

    particleSensor.setup(
        ledBrightness,
        MY_SAMPLE_AVERAGE,
        MY_LED_MODE,
        MY_SAMPLE_RATE,
        MY_PULSE_WIDTH,
        MY_ADC_RANGE);

    sd.bufferLength = BUFFER_LENGTH;
}

// ── Main task ─────────────────────────────────────────────────
void sp02Reading(void *pvParameters)
{
    // ── Accumulator state ──────────────────────────────────────
    int32_t spo2Readings[AVERAGE_COUNT];
    int32_t hrReadings[AVERAGE_COUNT];
    int     readingIndex  = 0;
    bool    fingerWasAbsent = true;

    // ── Fill initial 100-sample buffer ─────────────────────────
    for (byte i = 0; i < BUFFER_LENGTH; i++)
    {
        while (!particleSensor.available()) {
            particleSensor.check();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        sd.redBuffer[i] = particleSensor.getRed();
        sd.irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample();
    }

    maxim_heart_rate_and_oxygen_saturation(
        sd.irBuffer, sd.bufferLength, sd.redBuffer,
        &sd.spo2, &sd.validSPO2, &sd.heartRate, &sd.validHeartRate);

    // ── Main loop ──────────────────────────────────────────────
    while (1)
    {
        // Shift oldest SHIFT_AMOUNT samples out
        for (byte i = SHIFT_AMOUNT; i < BUFFER_LENGTH; i++) {
            sd.redBuffer[i - SHIFT_AMOUNT] = sd.redBuffer[i];
            sd.irBuffer[i  - SHIFT_AMOUNT] = sd.irBuffer[i];
        }

        // Collect SHIFT_AMOUNT fresh samples
        for (byte i = REFILL_START; i < BUFFER_LENGTH; i++)
        {
            while (!particleSensor.available())
                particleSensor.check();

            sd.redBuffer[i] = particleSensor.getRed();
            sd.irBuffer[i]  = particleSensor.getIR();
            particleSensor.nextSample();
        }

        // Recalculate
        maxim_heart_rate_and_oxygen_saturation(
            sd.irBuffer, sd.bufferLength, sd.redBuffer,
            &sd.spo2, &sd.validSPO2, &sd.heartRate, &sd.validHeartRate);

        // ── IMPROVEMENT 1: IR threshold check ─────────────────
        //    Read the last IR sample as a proxy for contact quality.
        //    A weak signal means the finger is too light or not present.
        uint32_t irValue = sd.irBuffer[BUFFER_LENGTH - 1];
        bool signalStrong = (irValue >= MIN_IR_THRESHOLD);

        // ── Finger detection ───────────────────────────────────
        bool fingerPresent = (sd.validSPO2 == 1 && sd.spo2 > 0 && signalStrong);

        if (!fingerPresent)
        {
            if (irValue < MIN_IR_THRESHOLD && sd.validSPO2 == 1) {
                // Finger is there but not pressed firmly enough
                Serial.println("Weak signal — press finger more firmly on sensor.");
            } else {
                Serial.println("No finger detected — please place finger on sensor.");
            }

            // Reset accumulator so stale readings never mix with fresh ones
            readingIndex    = 0;
            fingerWasAbsent = true;

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;   // skip averaging logic below
        }

        // ── Finger is present ──────────────────────────────────
        if (fingerWasAbsent)
        {
            Serial.println("Finger detected — acquiring readings, please hold still...");
            fingerWasAbsent = false;
            readingIndex    = 0;
        }

        spo2Readings[readingIndex] = sd.spo2;
        hrReadings[readingIndex]   = sd.heartRate;
        readingIndex++;

        Serial.print("Acquiring reading ");
        Serial.print(readingIndex);
        Serial.print(" / ");
        Serial.println(AVERAGE_COUNT);

        // ── IMPROVEMENT 2 & 3: Trimmed mean over AVERAGE_COUNT ─
        if (readingIndex >= AVERAGE_COUNT)
        {
            int32_t avgSpo2 = trimmedMean(spo2Readings, AVERAGE_COUNT);
            int32_t avgHR   = trimmedMean(hrReadings,   AVERAGE_COUNT);

            Serial.println("──────────────────────────");
            Serial.print  ("SpO2       : ");
            Serial.print  (avgSpo2);
            Serial.println(" %");
            Serial.print  ("Heart Rate : ");
            Serial.print  (avgHR);
            Serial.println(" bpm");
            Serial.print  ("(from ");
            Serial.print  (AVERAGE_COUNT);
            Serial.print  (" readings, ");
            Serial.print  (TRIM_DROP);
            Serial.println(" outlier(s) dropped each end)");
            Serial.println("──────────────────────────");

            readingIndex = 0;   // start next batch immediately
        }
    }
}
