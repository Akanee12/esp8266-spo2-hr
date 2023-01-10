#include "header.h"
#include "algorithm_by_RF.h"
#include "max30102.h"

using namespace std;

// Wifi
const char* ssid     = SSID;
const char* password = PSWD;

// OLED
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// 温度传感器
Ticker timer_temp_read;
double ambient_temp = 0;
Adafruit_MLX90614 temp_sensor = Adafruit_MLX90614();

// 时间
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, DEFAULT_NTP_SERVER); //NTP地址
Ticker timer_timework;
struct tm ptm;
bool is_need_update = false;

// OLED
Ticker timer_oled_display;
bool is_display_data_available = false;

// 健康状况对象
health_status status;

// 心率血氧传感器
const unsigned char oxiInt = D0; // pin connected to MAX30102 INT

uint32_t elapsedTime, timeStart;

uint32_t aun_ir, aun_red;
uint32_t aun_ir_buffer[BUFFER_SIZE];  // infrared LED sensor data
uint32_t aun_red_buffer[BUFFER_SIZE]; // red LED sensor data


uint8_t uch_dummy;
bool showMeasuring = false;
Ticker timer_hr_ox_read;


void setup()
{
    Serial.begin(115200);
    Serial.println();

    oled_init(DEFAULT_FONT_SIZE);

    oled.setCursor(15, 5);
    oled.println("WiFi Information");
    oled.setCursor(2, 20);

    wifi_init();
    temp_sensor_init();
    heart_rate_and_ox_sensor_init();

    oled.setCursor(2, 35);//设置显示位置
    oled.println("Connected,IP address:");
    oled.println();
    oled.println(WiFi.localIP());
    oled.display(); // 开显示

    date_time_init();

    timer_oled_display.attach_ms(500, update_oled_display_buf);
}

void loop()
{
    if (is_need_update)
    {
        update_date_time();
        is_need_update = false;
    }

    if (is_display_data_available)
    {
        oled.display();
        is_display_data_available = false;
    }

    update_heart_rate_and_ox_sensor();
}




// 模块初始化

void oled_init(int font_size)
{
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    oled.setTextColor(WHITE);//开像素点发光
    oled.clearDisplay();//清屏

    oled.setTextSize(font_size); //设置字体大小

    return;
}

void wifi_init(void)
{
    int cnt = 0;
    WiFi.begin(ssid, password);//启动网络连接

    print_serial_log(LOG_INFO, String("Connect WiFi SSID: ") + String(ssid));
    print_serial_log(LOG_INFO, "......");

    while (WiFi.status() != WL_CONNECTED)//检测网络是否连接成功
    {
        delay(500);
        oled.print('.');
        oled.display();
        if (cnt++ > 30)
        {
            print_serial_log(LOG_ERR, "cannot connect to Wifi");
            break;
        }
    }

    print_serial_log(LOG_INFO, "successfully connect Wifi");

    print_serial_log(LOG_INFO, WiFi.localIP());

    return;
}

void temp_sensor_init(void)
{
    while (!temp_sensor.begin())
    {
        print_serial_log(LOG_ERR, "Error connecting to temp sensor. Check wiring.");
        delay(500);
    }
    timer_temp_read.attach_ms(2000, update_temp_sensor);

    return;
}


void heart_rate_and_ox_sensor_init(void)
{
    pinMode(oxiInt, INPUT);

    while (!maxim_max30102_reset())
    {
        print_serial_log(LOG_ERR, "Error reseting hr_ox sensor.");
        delay(500);
    }

    while (!maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_dummy))
    {
        print_serial_log(LOG_ERR, "Error reading hr_ox sensor's reg.");
        delay(500);
    }

    while (!maxim_max30102_init())
    {
        print_serial_log(LOG_ERR, "Error initing hr_ox sensor.");
        delay(500);
    }

    // timer_hr_ox_read.attach_ms(100, update_heart_rate_and_ox_sensor);

    timeStart = millis();
}


// LOG 打印

template <class T>
void print_serial_log(unsigned char type, T message)
{
    if (LOG_LEVEL < type)
    {
        return;
    }

    switch (type)
    {
        case LOG_ERR:
            Serial.print("[ERR] ");
            break;
        case LOG_WARN:
            Serial.print("[WARN] ");
            break;
        case LOG_INFO:
            Serial.print("[INFO] ");
            break;
        default:
            break;
    }

    Serial.println(message);

    return;
}



// 日期与时间

void date_time_init(void)
{
    print_serial_log(LOG_INFO, String("NTP Server: ") + String(DEFAULT_NTP_SERVER));

    if (WiFi.status() == WL_CONNECTED)
    {
        timeClient.begin(DEFAULT_PORT);
        timeClient.setTimeOffset(28800); //+1区，偏移3600，+8区，偏移3600*8
    }

    memset(&ptm, 0, sizeof(ptm));
    update_date_time();

    timer_timework.attach_ms(1000, time_work);

    return;
}

String get_date_time(void)
{
    char str[32] = {0};
    sprintf(str, "%d-%d-%d %d:%d:%d", ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday, ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
    return String(str);
}

static void update_date_time(void)
{
    print_serial_log(LOG_INFO, "updating date and time");
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (!timeClient.update())
    {
        print_serial_log(LOG_WARN, "update date and time fail");
        return;
    }
    print_serial_log(LOG_INFO, "update date and time successful");

    unsigned long epochTime = timeClient.getEpochTime();

    memcpy(&ptm, gmtime((time_t *)&epochTime), sizeof(ptm));

    char str[32] = {0};
    sprintf(str, "current: %d-%d-%d %d:%d:%d", ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday, ptm.tm_hour, ptm.tm_min, ptm.tm_sec);

    print_serial_log(LOG_INFO, str);

    return;
}

static void time_work(void)
{
    if (++ptm.tm_sec > 59)
    {
        ptm.tm_sec = 0;
        if (++ptm.tm_min > 59)
        {
            ptm.tm_min = 0;
            is_need_update = true;
            if (++ptm.tm_hour > 23)
            {
                ptm.tm_hour = 0;
            }
        }
    }

    return;
}


// 传感器更新
void update_temp_sensor(void)
{
    status.hs_set_body_temperature(temp_sensor.readObjectTempC());

    static unsigned char cnt = 0;
    if (cnt++ % 5 == 0)
    {
        double tmp = temp_sensor.readAmbientTempC();
        if (tmp > 0 || tmp < 0)
        {
            ambient_temp = tmp;
        }
    }

    return;
}

void update_heart_rate_and_ox_sensor(void)
{
    // if (!digitalRead(oxiInt))
    // {
    //     Serial.println("hr_ox sensor data not ready");
    //     return;
    // }

    #define REVERSE_LED
    // Serial.println("looping");
    float n_spo2, ratio, correl; // SPO2 value
    int8_t ch_spo2_valid;        // indicator to show if the SPO2 calculation is valid
    int32_t n_heartrate;         // heart rate value
    int8_t ch_hr_valid;          // indicator to show if the heart rate calculation is valid
    int32_t i;

    bool measuring_display = false;

    // buffer length of BUFFER_SIZE stores ST seconds of samples running at FS sps
    // read BUFFER_SIZE samples, and determine the signal range
    for (i = 0; i < BUFFER_SIZE; i++)
    {
        while (digitalRead(oxiInt) == 1)
        ; // wait until the interrupt pin asserts
        delay(1);
        // wdt_reset();
    #ifdef REVERSE_LED
        maxim_max30102_read_fifo(&aun_ir, &aun_red); // read from MAX30102 FIFO
    #else
        maxim_max30102_read_fifo(&aun_red, &aun_ir); // read from MAX30102 FIFO
    #endif
        if (aun_ir < 5000)
        {
            break;
        }

        if (i == 0)
        {
            if (showMeasuring)
            {
                // print_measuring();
                showMeasuring = false;
            }
            // Serial.println("Measuring...");
        }

        measuring_display = true;

        *(aun_ir_buffer + i) = aun_ir;
        *(aun_red_buffer + i) = aun_red;
    }

    if (aun_ir < 5000)
    {
        // print_press();
        showMeasuring = true;
        // Serial.println("Put On Finger");
    }
    else
    {
        // calculate heart rate and SpO2 after BUFFER_SIZE samples (ST seconds of samples) using Robert's method
        rf_heart_rate_and_oxygen_saturation(aun_ir_buffer, BUFFER_SIZE, aun_red_buffer, &n_spo2, &ch_spo2_valid, &n_heartrate, &ch_hr_valid, &ratio, &correl);
        // Serial.println("rf_heart_rate_and_oxygen_saturation");
        elapsedTime = millis() - timeStart;
        elapsedTime /= 1000; // Time in seconds

        if (ch_spo2_valid)
        {
            // print_hr_spo2(n_heartrate, (int)(n_spo2+0.5));
            status.hs_set_Oxygen_saturation(n_spo2);
        }

        if (ch_hr_valid)
        {
            status.hs_set_heart_rate(n_heartrate);
        }
    }

    update_oled_display_buf();

    if (measuring_display)
    {
        oled.println();
        oled.print("      Measuring...");
    }

    oled.display();
}



// health status
void health_status::hs_set_body_temperature(double temp)
{
    if (temp < MAX_BODY_TEMP + 1 && temp > MIN_BODY_TEMP - 1)
    {
        body_temperature = temp;
        print_serial_log(LOG_INFO, "Successfully set body temperature");
        print_serial_log(LOG_INFO, body_temperature);
    }
    else
    {
        print_serial_log(LOG_WARN, "Invalid value of body temperature");
    }

    return;
}

double health_status::hs_get_body_temperature(void)
{
    return body_temperature;
}



void health_status::hs_set_Oxygen_saturation(int ox)
{
    if (ox < MAX_OXYGEN_SATURATION + 1 && ox > MIN_OXYGEN_SATURATION - 1)
    {
        Oxygen_saturation = ox;
        print_serial_log(LOG_INFO, "Successfully set Oxygen saturation");
        print_serial_log(LOG_INFO, ox);
    }
    else
    {
        print_serial_log(LOG_WARN, "invalid value of Oxygen saturation");
    }

    return;
}

int health_status::hs_get_Oxygen_saturation(void)
{
    return Oxygen_saturation;
}



void health_status::hs_set_heart_rate(int hr)
{
    if (hr < MAX_HEART_RATE + 1 && hr > MIN_HEART_RATE - 1)
    {
        heart_rate = hr;
        print_serial_log(LOG_INFO, "Successfully set heart rate");
        print_serial_log(LOG_INFO, hr);
    }
    else
    {
        print_serial_log(LOG_WARN, "invalid value of heart rate");
    }

    return;
}

int health_status::hs_get_heart_rate(void)
{
    return heart_rate;
}



// oled更新
void update_oled_display_buf(void)
{
    oled.clearDisplay();

    oled.setCursor(0, 0);

    oled.println(get_date_time());
    oled.println();

    oled.print("Amb:"); oled.print(ambient_temp);
    oled.print("  ");

    oled.print("Obj:"); oled.println(status.hs_get_body_temperature());
    oled.println();

    oled.print("HR:"); oled.print(status.hs_get_heart_rate());
    oled.print("  ");

    oled.print("SpO2:"); oled.print(status.hs_get_Oxygen_saturation());
    oled.println();

    is_display_data_available = true;
}