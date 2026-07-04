#include <ESP32QRCodeReader.h>
#include <string>

#include "../logging.h"

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
boolean qrDone = false;
std::string qrCodeText = "";

void onQrCodeTask(void *pvParameters)
{
  struct QRCodeData qrCodeData;

  while (true)
  {
    if (reader.receiveQrCode(&qrCodeData, 100))
    {
      LOG_SUCCESS("QR code found");
      if (qrCodeData.valid)
      {
        qrCodeText = (const char *)qrCodeData.payload;
        if (qrCodeText.find("WIFI:") == 0)
        {
          qrDone = true;
          vTaskDelete(NULL);
        }
        else
        {
          LOG_WARNINGF("QR ignored: not WiFi (%s)", (const char *)qrCodeData.payload);
        }
      }
      else
      {
        LOG_ERRORF("QR invalid: %s", (const char *)qrCodeData.payload);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

std::string ReadWifiQrCode()
{
  reader.setup();

  LOG_INFO("QR reader setup");

  reader.beginOnCore(1);

  LOG_INFO("QR reader started on core 1");

  xTaskCreate(onQrCodeTask, "onQrCode", 4 * 1024, NULL, 4, NULL);

  while (!qrDone)
  {
    delay(100);
  }
  reader.end();
  return qrCodeText;
}
