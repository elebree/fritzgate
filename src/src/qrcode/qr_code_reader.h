#include <ESP32QRCodeReader.h>
#include <string>

#include "../logging.h"

// Minimum time each camera orientation is tried before mirroring again
#define QR_MIRROR_TOGGLE_INTERVAL_MS 2000

// The flash LED pulses dimly while the reader waits for a code.
// Channel 2 runs on LEDC timer 1; the camera clock uses channel 0 on timer 0.
#define QR_LED_LEDC_CHANNEL 2
// Brightness out of 255, low enough not to glare in the camera image
#define QR_LED_BRIGHTNESS 8

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

  ledcSetup(QR_LED_LEDC_CHANNEL, 5000, 8);
  ledcAttachPin(FLASH_GPIO_NUM, QR_LED_LEDC_CHANNEL);

  // Pulse the LED for 100 ms every two seconds to show that the reader is running
  for (int i = 0; !qrDone; i++)
  {
    ledcWrite(QR_LED_LEDC_CHANNEL, i % 20 == 0 ? QR_LED_BRIGHTNESS : 0);
    delay(100);
  }
  ledcWrite(QR_LED_LEDC_CHANNEL, 0);
  ledcDetachPin(FLASH_GPIO_NUM);
  reader.end();
  return qrCodeText;
}
