/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "ad8232.h"
#include "max30102.h"
#include "ds18b20.h"
#include "display.h"
#include "esp_log.h"

extern "C" void app_main(void)
{
    initArduino();
    Serial.begin(115200);
    esp_log_level_set("gpio", ESP_LOG_NONE);

    initAd8232();
    initMAX30102();
    initDs18b20();
    initDisplay();

    xTaskCreate(readAd8232Values, "ECG_Task", 8192, NULL, 4, NULL);
    xTaskCreate(sp02Reading, "Sp02 Reading", 10240, NULL, 3, NULL);
    xTaskCreate(ds18b20Readings, "temp reading", 4096, NULL, 1, NULL);
    xTaskCreate(displayTask, "Display_Task", 12288, NULL, 2, NULL);
}*/

/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "ad8232.h"
#include "max30102.h"
#include "ds18b20.h"
#include "display.h"
#include "webserver.h"
#include "esp_log.h"

extern "C" void app_main(void)
{
    initArduino();
    Serial.begin(115200);
    esp_log_level_set("gpio", ESP_LOG_NONE);

    // ── Sensor init ───────────────────────────────────────────────────────────
    initAd8232();
    initMAX30102();
    initDs18b20();
    initDisplay();

    // ── Tasks ─────────────────────────────────────────────────────────────────
    // Priority ladder (higher = more urgent):
    //   5  webTask       — HTTP must respond promptly; self-deletes after init
    //   4  ECG_Task      — 4 ms sample loop, timing-sensitive
    //   3  Sp02 Reading  — buffer refill loop, tolerates brief preemption
    //   2  Display_Task  — UI refresh, lowest latency requirement
    //   1  temp reading  — 1 s state machine, lowest priority fine

    xTaskCreate(webTask,          "Web_Task",     8192,  NULL, 5, NULL);
    xTaskCreate(readAd8232Values, "ECG_Task",     8192,  NULL, 4, NULL);
    xTaskCreate(sp02Reading,      "Sp02 Reading", 10240, NULL, 3, NULL);
    xTaskCreate(displayTask,      "Display_Task", 12288, NULL, 2, NULL);
    xTaskCreate(ds18b20Readings,  "temp reading", 4096,  NULL, 1, NULL);
}*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "ad8232.h"
#include "max30102.h"
#include "ds18b20.h"
#include "display.h"
#include "webserver.h"
#include "esp_log.h"

void setup()
{
    Serial.begin(115200);
    esp_log_level_set("gpio", ESP_LOG_NONE);

    // ── Sensor init ───────────────────────────────────────────────────────
    initAd8232();
    initMAX30102();
    initDs18b20();
    initDisplay();

    // ── Tasks ─────────────────────────────────────────────────────────────
    xTaskCreate(webTask,          "Web_Task",     8192,  NULL, 5, NULL);
    xTaskCreate(readAd8232Values, "ECG_Task",     8192,  NULL, 4, NULL);
    xTaskCreate(sp02Reading,      "Sp02 Reading", 10240, NULL, 3, NULL);
    xTaskCreate(displayTask,      "Display_Task", 12288, NULL, 2, NULL);
    xTaskCreate(ds18b20Readings,  "temp reading", 4096,  NULL, 1, NULL);
}

void loop()
{
    vTaskDelay(portMAX_DELAY);  // park Arduino loop task; FreeRTOS tasks run freely
}