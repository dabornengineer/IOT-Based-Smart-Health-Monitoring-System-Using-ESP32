#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "ad8232.h"
#include "max30102.h"
#include "esp_log.h"

extern "C" void app_main(void)
{
    initArduino();
    Serial.begin(115200);
    esp_log_level_set("gpio", ESP_LOG_NONE);
    initAd8232();
    initMAX30102();

    xTaskCreate(readAd8232Values, "ECG_Task", 8192, NULL, 1, NULL);
    //xTaskCreate(sp02Reading, "Sp02 Reading", 10240, NULL, 2, NULL);

}