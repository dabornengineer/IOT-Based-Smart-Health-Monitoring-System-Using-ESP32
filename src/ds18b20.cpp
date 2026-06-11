/*#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ds18b20.h"

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

void initDs18b20(void)
{
    sensors.begin();

    if (sensors.getDeviceCount() == 0)
    {
        Serial.println("ERROR: DS18B20 not detected");
    }
    else
    {
        Serial.print("DS18B20 detected. Device count: ");
        Serial.println(sensors.getDeviceCount());
    }
}

void ds18b20Readings(void *pvParameters)
{
    float tempC{};
    while (1)
    {
        sensors.requestTemperatures();
        tempC = sensors.getTempCByIndex(0);

        if (tempC == DEVICE_DISCONNECTED_C)
        {
            Serial.println("ERROR: DS18B20 disconnected or not responding");
        }
        else
        {
            Serial.print("Temperature: ");
            Serial.print(tempC, 2);
            Serial.println(" °C");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}*/

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ds18b20.h"

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// Touch threshold — lower value means touched (tune during testing)
#define TOUCH_THRESHOLD 20

// Lookup table: returns offset based on palm temp range
// Core Temp = Palm Temp + offset
// Pattern: offset = 37 - palmTemp (converges around 37°C for normal)
float getOffset(float palmTemp)
{
    if      (palmTemp < 28.0f) return -1;   // invalid — signal poor contact
    else if (palmTemp < 29.0f) return 9.0f; // 28°C → 37°C
    else if (palmTemp < 30.0f) return 8.0f; // 29°C → 37°C
    else if (palmTemp < 31.0f) return 7.0f; // 30°C → 37°C
    else if (palmTemp < 32.0f) return 6.0f; // 31°C → 37°C
    else if (palmTemp < 33.0f) return 5.0f; // 32°C → 37°C
    else if (palmTemp < 34.0f) return 4.0f; // 33°C → 37°C
    else if (palmTemp < 35.0f) return 3.0f; // 34°C → 37°C
    else if (palmTemp <= 35.5f) return 2.0f; // 35°C → 37°C
    else return -2;                           // above 35.5°C — possible fever signal
}

void initDs18b20(void)
{
    sensors.begin();

    if (sensors.getDeviceCount() == 0)
    {
        Serial.println("ERROR: DS18B20 not detected");
    }
    else
    {
        Serial.print("DS18B20 detected. Device count: ");
        Serial.println(sensors.getDeviceCount());
    }
}

void ds18b20Readings(void *pvParameters)
{
    float tempC = 0.0f;

    // States
    enum State { IDLE, COUNTDOWN, RESULT };
    State state = IDLE;

    int countdownSeconds = 0;

    while (1)
    {
        bool isTouched = (touchRead(TOUCH_PIN) < TOUCH_THRESHOLD);

        switch (state)
        {
            case IDLE:
                Serial.println("Touch sensor to measure temperature...");
                if (isTouched)
                {
                    state = COUNTDOWN;
                    countdownSeconds = 0;
                    Serial.println("Hold sensor for 1 minute...");
                    Serial.println(touchRead(T4));
                }
                break;

            case COUNTDOWN:
                if (!isTouched)
                {
                    // Hand removed — reset
                    Serial.println("Measurement cancelled. Touch sensor to measure temperature...");
                    state = IDLE;
                    countdownSeconds = 0;
                    break;
                }

                countdownSeconds++;
                Serial.print("Hold sensor for 1 minute... ");
                Serial.print(60 - countdownSeconds);
                Serial.println("s remaining");

                if (countdownSeconds >= 60)
                {
                    // Take final reading
                    sensors.requestTemperatures();
                    tempC = sensors.getTempCByIndex(0);

                    if (tempC == DEVICE_DISCONNECTED_C)
                    {
                        Serial.println("ERROR: DS18B20 disconnected or not responding");
                        state = IDLE;
                        countdownSeconds = 0;
                        break;
                    }

                    state = RESULT;
                }
                break;

            case RESULT:
            {
                float offset = getOffset(tempC);

                if (offset == -1)
                {
                    Serial.println("Poor contact detected. Please place hand firmly on sensor.");
                }
                else if (offset == -2)
                {
                    float coreTemp = tempC + 2.0f;
                    Serial.println("WARNING: Possible fever detected!");
                    Serial.print("Estimated Core Temperature: ");
                    Serial.print(coreTemp, 1);
                    Serial.println(" °C");
                }
                else
                {
                    float coreTemp = tempC + offset;
                    Serial.print("Palm Temp: ");
                    Serial.print(tempC, 1);
                    Serial.print(" °C  |  Estimated Core Temp: ");
                    Serial.print(coreTemp, 1);
                    Serial.println(" °C");
                }

                // Reset after displaying result
                vTaskDelay(pdMS_TO_TICKS(3000)); // show result for 3 seconds
                state = IDLE;
                countdownSeconds = 0;
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second tick
    }
}