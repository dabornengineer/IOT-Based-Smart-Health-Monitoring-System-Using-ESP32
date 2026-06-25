#ifndef AD8232_H
#define AD8232_H



#define OUTPUT_AD8232         34
#define NOT_CONNECTED_RIGHT   25
#define NOT_CONNECTED_LEFT    26

void initAd8232(void);
void readAd8232Values(void *pvParameters);



#endif