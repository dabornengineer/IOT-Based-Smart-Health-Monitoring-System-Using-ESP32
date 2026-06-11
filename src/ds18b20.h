#ifndef DS18B20_H
#define DS18B20_H

#define DS18B20_PIN 4
#define TOUCH_PIN    T4   // GPIO13

void initDs18b20(void);
void ds18b20Readings(void *pvParameters);

#endif