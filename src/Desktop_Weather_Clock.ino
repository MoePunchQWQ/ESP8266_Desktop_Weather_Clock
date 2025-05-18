#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
// 包含解析 Json , 连接网络 , U8g2 库 , I2C 通讯库等

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // 在 ESP8266 上，GPIO5（D1）：用作SCL, GPIO4（D2）：用作SDA

String ssid = "ENTER_YOUR_SSID_HERE";
String pwd = "ENTER_YOUR_PASSWORD_HERE";

const char* host = "api.seniverse.com";
const int port = 80;

String reqKey = "ENTER_YOU_KEY_HERE";
String reqLocation = "ENTER_YOUR_CITY_HERE";
String reqUnit = "c";

struct Data{
    String text;
    int code;
    int temp;
    String last_update;
  };

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.aliyun.com", 8*3600, 60000);

void setup() {
  u8g2.begin();
  Serial.begin(9600);
  connectWiFi();
  timeClient.setTimeOffset(8 * 3600);
  timeClient.begin();
}

void loop() {
  String reqUrl = "/v3/weather/now.json?key=" + reqKey + "&location=" + reqLocation + "&language=zh-CN&unit=" + reqUnit;
  Data WeatherData = httpReq(reqUrl);
  String time_now = timeUpdate();
  uint16_t mark = MatchCode(WeatherData.code);
  showInfo(WeatherData.text, WeatherData.temp, WeatherData.last_update, time_now, mark);
  delay(60000);
}

Data httpReq(String reqUrl) {
  WiFiClient client;
  Data result = {"", -1, -100, ""};
  // 建立http请求信息
  // 请求方法(GET)+空格+URL+空格+协议(HTTP/1.1)+回车+换行+
  // 头部字段(Host)+冒号+值(服务器地址)+回车+换行+
  // 头部字段(Connection)+冒号+值(close)+回车+换行+回车+换行
  String httpRequest = String("GET ") + reqUrl + " HTTP/1.1\r\n" +
                       "Host: " + host + "\r\n" +
                       "Connection: close\r\n\r\n";
  Serial.print("The request url is:");
  Serial.println(httpRequest);
  if (client.connect(host, port)) {
    client.print(httpRequest); 
    Serial.println("Request Sent");

    // 跳过HTTP头
    if (!client.find("\r\n\r\n")) {
      Serial.println("Invalid Response");
      return result;
    }

    // 解析JSON
    result = parseInfo(client);
  } else {
    Serial.println("Connection Failed");
  }
  client.stop();
  return result;
}

void connectWiFi() {
  u8g2.enableUTF8Print();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_boutique_bitmap_9x9_t_gb2312); // 切换中文字体
  WiFi.begin(ssid, pwd);
  int i =0;
  while (WiFi.status() != WL_CONNECTED) {
    u8g2.setCursor(0, 12);
    u8g2.print("正在连接 WiFi");
    u8g2.sendBuffer();
    delay(1000);
    Serial.print(".");
    i++;
  }
  u8g2.clearBuffer();
  u8g2.setCursor(0, 12);
  u8g2.print("连接成功!");
  u8g2.sendBuffer();
  Serial.println("");
  Serial.println("Connect Success!");
  delay(500);
}

Data parseInfo(WiFiClient& client) {
  DynamicJsonDocument doc(1024);
  
  deserializeJson(doc, client);
  
  JsonObject results_0 = doc["results"][0];
  
  JsonObject results_0_now = results_0["now"];

  const char* results_0_now_text = results_0_now["text"]; 
  const char* results_0_now_code = results_0_now["code"]; 
  const char* results_0_now_temperature = results_0_now["temperature"]; 
  const char* results_0_last_update = results_0["last_update"]; 

  Data data = {"", -1, -100, ""};
  data.text = results_0_now["text"].as<String>(); 
  data.code = results_0_now["code"].as<int>(); 
  data.temp = results_0_now["temperature"].as<int>(); 
  data.last_update = results_0["last_update"].as<String>();   
  return data;
}


void showInfo(String text, int temp, String last_update, String time_now, int mark) {
  u8g2.enableUTF8Print();
  u8g2.setFontPosBaseline();
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_open_iconic_weather_4x_t);
  u8g2.drawGlyph(0, 48, mark); // 显示气象图标

  u8g2.drawVLine(36, 0, 64); // 绘制分界线

  u8g2.setFont(u8g2_font_boutique_bitmap_9x9_t_gb2312); // 切换中文字体
  
  u8g2.setCursor(40, 12);
  u8g2.print("您目前在 "); // 显示位置i

  u8g2.setCursor(40, 24);
  u8g2.print("天气: " + text); // 显示天气
  
  u8g2.setCursor(40, 36);
  u8g2.print("温度: " + String(temp) + "°C"); // 显示温度
  
  u8g2.setCursor(40, 48);
  u8g2.print("上次更新: " + last_update.substring(11,16)); // 显示天气更新时间
  
  u8g2.setCursor(40, 60);
  u8g2.print("时间: " + time_now); // 显示当前时间
  u8g2.sendBuffer();
  // 中文字体: u8g2_font_boutique_bitmap_7x7/9x9(_bold)_t_gb2312
  //          u8g2_font_wqy12_t_gb2312
}

String timeUpdate() {
  int retry = 0;
  while (!timeClient.update() && retry < 5) {
    Serial.println("Syncing Failed...Retrying");
    timeClient.forceUpdate();
    delay(1000);
    retry++;
  }
  String time_now = timeClient.getFormattedTime();
  String minute = time_now.substring(0, 5);
  Serial.print("The time gotten from NTP Server is: ");
  Serial.println(time_now);
  return minute;
}
uint16_t MatchCode(int code) {
  uint16_t mark = 0x0040; // 默认阴
  switch (code) {
    case 0: // 日间晴
      mark = 0x0045;
      break;
    case 1: // 夜晚晴
      mark = 0x0044;
      break;
    case 4: case 5: case 6: case 7: case 8: // 多云
      mark = 0x0041;
      break;
    case 9: // 阴
      break;
    case 10: case 11: case 12: case 13:
    case 14: case 15: case 16:
    case 17: case 18: case 19: // 雨
      mark = 0x0043;
      break;
    default:
      Serial.println("Unknown Weather Code!");
      break;
  }
  return mark;
}