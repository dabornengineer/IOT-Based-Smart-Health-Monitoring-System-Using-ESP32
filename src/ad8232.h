#ifndef AD8232_H
#define AD8232_H

#ifdef __cplusplus
extern "C" {
#endif

#define OUTPUT_AD8232         34
#define NOT_CONNECTED_RIGHT   25
#define NOT_CONNECTED_LEFT    26

void initAd8232(void);
void readAd8232Values(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif