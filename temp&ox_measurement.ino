#include "header.h"

using namespace std;

const char* ssid     = SSID;
const char* password = PSWD;

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

Ticker timer_temp_read;
double ambient_temp = 0.0;
Adafruit_MLX90614 temp_sensor = Adafruit_MLX90614();

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, DEFAULT_NTP_SERVER); //NTP地址

Ticker timer_timework;
struct tm ptm;
bool is_need_update = false;

Ticker timer_oled_display;
bool is_display_data_available = false;

health_status status;

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

    oled.setCursor(2, 35);//设置显示位置
    oled.println("Connected,IP address:");
    oled.println();
    oled.println(WiFi.localIP());
    oled.display(); // 开显示

    date_time_init();

    timer_oled_display.attach_ms(500, update_oled_display_buf);

    delay(3000);
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
    WiFi.begin(ssid, password);//启动网络连接

    print_serial_log(LOG_INFO, String("Connect WiFi SSID: ") + String(ssid));
    print_serial_log(LOG_INFO, "......");

    while (WiFi.status() != WL_CONNECTED)//检测网络是否连接成功
    {
        delay(500);
        oled.print('.');
        oled.display();
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

    timeClient.begin(DEFAULT_PORT);
    timeClient.setTimeOffset(28800); //+1区，偏移3600，+8区，偏移3600*8

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



// health status
void health_status::hs_set_body_temperature(double temp)
{
    if (temp < MAX_BODY_TEMP && temp > MIN_BODY_TEMP)
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
    if (ox < MAX_OXYGEN_SATURATION && ox > MIN_OXYGEN_SATURATION)
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



// oled更新
void update_oled_display_buf(void)
{
    oled.clearDisplay();

    oled.setCursor(0, 0);

    oled.println(get_date_time());
    oled.println();

    oled.print("Ambient: "); oled.println(ambient_temp);
    oled.println();

    oled.print("Object: "); oled.println(status.hs_get_body_temperature());
    oled.println();

    is_display_data_available = true;
}