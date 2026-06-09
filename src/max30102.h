#ifndef MAX30102_H
#define MAX30102_H

// MAX30102 Sensor configuration

#define MAX_BRIGHTNESS 255
#define ledBrightness 60
#define MY_SAMPLE_AVERAGE 4
#define MY_LED_MODE 2
#define MY_SAMPLE_RATE 100
#define MY_PULSE_WIDTH 411
#define MY_ADC_RANGE 4096

static void sortArray(int32_t *arr, int n);
static int32_t trimmedMean(int32_t *src, int n);
static bool physiologicallyValid(int32_t spo2, int32_t hr);
void initMAX30102(void);
void sp02Reading(void *pvParameters);



#endif