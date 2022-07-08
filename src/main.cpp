#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include "DHT.h"
#define IRR_SIZE 2                //灌溉模块数量
#define IRR_PORT (IRR_SIZE * 4)   //灌溉模块总端口数量
const char *ssid = "Wokwi-GUEST"; //你的网络名称
const char *password = "";        //你的网络密码

struct Irrigator
{
  int amount, ontask;
};

void wifi_connect(const char *ssid, const char *password)
{
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
void get_task(Irrigator *irr)
{
  // memcpy(irr, 0, sizeof(Irrigator) * IRR_PORT);
  for (int i = 0; i < IRR_PORT; i++)
    irr[i].ontask = 1;
  HTTPClient http;                            // 声明HTTPClient对象
  http.begin("http://43.138.46.52/api/task"); // 准备启用连接
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST("{\"mac\":\"ff:ff:ff:ff:ff:11\"}"); // 发起GET请求
  if (httpCode > 0)                                            // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {

      WiFiClient *stream = http.getStreamPtr(); // 获取响应正文数据流指针
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, *stream);
      if (!doc.isNull())
      {
        for (JsonObject item : doc.as<JsonArray>())
        {

          int port = item["port"];
          int amount = item["amount"];
          // Serial.println("Here's the paser loop");
          // Serial.println(port);
          // Serial.println(amount);
          irr[port - 1].amount = amount;
          irr[port - 1].ontask = 0;
        }
      }
      else
      {
        Serial.println("No Task Received");
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); // 结束当前连接
  }
}
void task_hdata(Irrigator *irr, uint8_t *hardware_data)
{
  for (int i = 0; i < IRR_SIZE; i++)
  {
    hardware_data[i] = 0;
    for (int j = 0; j < 4; j++)
    {
      hardware_data[i] = hardware_data[i] << 1;
      hardware_data[i] += irr[i * 4 + j].ontask;
    }
  }
}
void push_hardware(uint8_t *hd)
{
  // hd内存放高4位数据,低使能
  // hd:  1110
  // tar: 0111 1111
  // tar = (hd << 4) + 0xf

  // Pin connected to ST_CP of 74HC595
  int latchPin = 23;
  // Pin connected to SH_CP of 74HC595
  int clockPin = 22;
  ////Pin connected to DS of 74HC595
  int dataPin = 21;
  // set pins to output so you can control the shift register
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  for (int i = IRR_SIZE - 1; i >= 0; i--)
  {
    shiftOut(dataPin, clockPin, LSBFIRST, (hd[i] << 4) + 0xf);
  }
  digitalWrite(latchPin, HIGH);
}
void setup()
{
  Serial.begin(115200);
  Serial.println();

  wifi_connect(ssid, password);
}

void loop()
{
  Irrigator irr[IRR_PORT];
  get_task(irr);
  Serial.println("Get");
  for (size_t i = 0; i < IRR_PORT; i++)
  {
    Serial.print(irr[i].ontask);
  }

  byte hardware_data[IRR_SIZE];
  task_hdata(irr, hardware_data);
  Serial.println("Hard");
  char buffer[40];
  sprintf(buffer, "%x %x", hardware_data[0], hardware_data[1]);
  Serial.println(buffer);
  push_hardware(hardware_data);
  delay(3000);
}