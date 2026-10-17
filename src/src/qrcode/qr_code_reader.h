#include <ESP32QRCodeReader.h>
#include <string>

#include "../logging.h"

// Minimum time each camera orientation is tried before mirroring again
#define QR_MIRROR_TOGGLE_INTERVAL_MS 2000

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
boolean qrDone = false;
std::string qrCodeText = "";

/**
 * Mirror the camera image horizontally.
 * The quirc version bundled with ESP32QRCodeReader can't decode mirrored codes
 * (it reports "ECC failure"), and some camera modules deliver a mirrored image,
 * so after a failed decode the next frames are tried the other way round.
 */
void toggleCameraMirror()
{
  static unsigned long lastToggleMillis = 0;
  if (lastToggleMillis != 0 && millis() - lastToggleMillis < QR_MIRROR_TOGGLE_INTERVAL_MS)
    return;
  lastToggleMillis = millis();

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == NULL)
    return;
  sensor->set_hmirror(sensor, !sensor->status.hmirror);
  LOG_INFOF("QR camera mirror: %s", sensor->status.hmirror ? "on" : "off");
}

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
        toggleCameraMirror();
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
