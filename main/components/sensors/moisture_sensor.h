#ifndef MOISTURE_SENSOR_H
#define MOISTURE_SENSOR_H

// ADC初期化
void init_adc(void);
// 水分センサー読み取り
uint16_t read_moisture_sensor(void);

#endif // MOISTURE_SENSOR_H