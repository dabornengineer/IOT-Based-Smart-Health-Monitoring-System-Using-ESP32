#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "ad8232.h"

extern "C" void app_main(void)
{
    initArduino();
    Serial.begin(115200);
    initAd8232();

    xTaskCreate(readAd8232Values, "ECG_Task", 4096, NULL, 1, NULL);

}