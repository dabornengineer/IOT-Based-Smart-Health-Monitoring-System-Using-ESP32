#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ad8232.h"
#include <Arduino.h>

void initAd8232(void)
{
    pinMode(NOT_CONNECTED_RIGHT, INPUT);
    pinMode(NOT_CONNECTED_LEFT, INPUT);
    pinMode(OUTPUT_AD8232, INPUT);
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
        Serial.println(value);
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}