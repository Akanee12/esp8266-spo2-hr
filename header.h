#ifndef _HEADER_H_
#define _HEADER_H_

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MLX90614.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Ticker.h>


// 模块
#define DEFAULT_FONT_SIZE       1
void oled_init(int font_size = DEFAULT_FONT_SIZE);

#define SSID        "myWifi"
#define PSWD        "12345679"
void wifi_init(void);

void temp_sensor_init(void);
void update_temp_sensor(void);
void heart_rate_and_ox_sensor_init(void);
void update_heart_rate_and_ox_sensor(void);

// LOG打印
#define LOG_ERR         1
#define LOG_WARN        2
#define LOG_INFO        3

#define LOG_LEVEL       LOG_ERR

template <class T>
void print_serial_log(unsigned char type, T message);


// 日期与时间
#define DEFAULT_NTP_SERVER          "ntp3.aliyun.com"
#define DEFAULT_PORT                2390

void date_time_init(void);
String get_date_time(void);
static void time_work(void);
static void update_date_time(void);

// health status
#define MAX_BODY_TEMP               50
#define MIN_BODY_TEMP               0

#define MAX_OXYGEN_SATURATION       100
#define MIN_OXYGEN_SATURATION       0

#define MAX_HEART_RATE              200
#define MIN_HEART_RATE              0

class health_status
{
public:
    void    hs_set_body_temperature(double temp);
    double  hs_get_body_temperature(void);
    void    hs_set_Oxygen_saturation(int ox);
    int     hs_get_Oxygen_saturation(void);
    void    hs_set_heart_rate(int hr);
    int     hs_get_heart_rate(void);

private:
    double body_temperature = 0;
    int Oxygen_saturation = 0;
    int heart_rate = 0;
};


// oled更新
void update_oled_display_buf(void);

#endif