#include "GoveeSensorsScanner.h"
#include "../logging.h"
#include <BLEDevice.h>
#include <HardwareSerial.h>

// Constants for device name prefixes
constexpr const char *H5075_PREFIX = "GVH5075_";
constexpr const char *H5177_PREFIX = "GVH5177_";
constexpr const char *H5174_PREFIX = "GVH5174_";
constexpr const char *H5100_PREFIX = "GVH5100_";
constexpr const char *H5151_PREFIX = "Govee_H5151_";
constexpr const char *H5179_PREFIX = "Govee_H5179_";
constexpr const char *H5074_PREFIX = "Govee_H5074_";

class GoveeSensorsAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
private:
    SensorScannerCallback *m_pCallback = nullptr;

    void parseBasicInfo(const std::string &name, Sensor *sensor, const std::string &prefix, int modelOffset, int serialOffset)
    {
        sensor->model = name.substr(modelOffset, 5);
        sensor->serial = name.substr(serialOffset);
    }

    bool parseGvhSensorData(const std::string &name, const std::string &data, Sensor *sensor, int tempIndex, int humidityIndex, int batteryIndex)
    {
        uint32_t climate = data[tempIndex] * 0x10000 + data[humidityIndex] * 0x100 + data[batteryIndex];
        parseBasicInfo(name, sensor, "GVH", 2, 8);
        sensor->temperature = climate / 1000;
        sensor->humidity = climate % 1000;
        sensor->battery = data[batteryIndex + 1];
        return true;
    }

    bool parseSensorAdvertisingMessage(BLEAdvertisedDevice &advertisedDevice, Sensor *sensor)
    {
        sensor->id = advertisedDevice.getAddress().toString();
        std::string name = advertisedDevice.getName();
        auto data = advertisedDevice.getManufacturerData();

        if (name.find(H5075_PREFIX) == 0)
        {
            parseBasicInfo(name, sensor, H5075_PREFIX, 2, 8);
            uint32_t climate = data[3] * 0x10000 + data[4] * 0x100 + data[5];
            sensor->temperature = climate / 1000;
            sensor->humidity = climate % 1000;
            sensor->battery = data[6] & 0x7F;
            return true;
        }
        else if (name.find(H5177_PREFIX) == 0 || name.find(H5174_PREFIX) == 0 || name.find(H5100_PREFIX) == 0)
        {
            return parseGvhSensorData(name, data, sensor, 4, 5, 6);
        }
        else if (name.find(H5151_PREFIX) == 0)
        {
            parseBasicInfo(name, sensor, H5151_PREFIX, 6, 12);
            sensor->temperature = 0;
            sensor->humidity = 0;
            sensor->battery = 100;
            return true;
        }
        else if (name.find(H5179_PREFIX) == 0)
        {
            parseBasicInfo(name, sensor, H5179_PREFIX, 6, 12);
            sensor->temperature = (short(data[7]) << 8 | short(data[6])) / 10;
            sensor->humidity = (int(data[9]) << 8 | int(data[8])) / 10;
            sensor->battery = data[10];
            return true;
        }
        else if (name.find(H5074_PREFIX) == 0)
        {
            parseBasicInfo(name, sensor, H5074_PREFIX, 6, 12);
            sensor->temperature = 0;
            sensor->humidity = 0;
            sensor->battery = 0;
            return true;
        }

        return false;
    }

public:
    GoveeSensorsAdvertisedDeviceCallbacks(SensorScannerCallback *callback) : m_pCallback(callback) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override
    {
        Sensor sensorData;
        if (parseSensorAdvertisingMessage(advertisedDevice, &sensorData))
        {
            if (advertisedDevice.haveManufacturerData())
            {
                auto data = advertisedDevice.getManufacturerData();
                char *pHex = BLEUtils::buildHexData(nullptr, (uint8_t *)data.data(), data.length());
                sensorData.data = pHex;
                free(pHex);
            }
            else
            {
                sensorData.data = "";
            }
            //sensorData.id = advertisedDevice.getName();
            sensorData.name = "Govee " + sensorData.model + " " + sensorData.serial;
            sensorData.rssi = advertisedDevice.getRSSI();
            LOG_DEVICEF("Govee device: %s %s %s", sensorData.id.c_str(), sensorData.name.c_str(), sensorData.data.c_str());
            m_pCallback->onResult(&sensorData);
        }
    }
};

class GoveeSensorsScannerImpl : public SensorsScanner
{
private:
    BLEScan *pBLEScan = nullptr;

public:
    GoveeSensorsScannerImpl() {}

    void init(const std::__cxx11::string deviceName) override
    {
        BLEDevice::init(deviceName);
    }

    void start(SensorScannerCallback *pSensorCallback) override
    {
        if (pBLEScan == nullptr)
        {
            pBLEScan = BLEDevice::getScan();
        }
        pBLEScan->setAdvertisedDeviceCallbacks(new GoveeSensorsAdvertisedDeviceCallbacks(pSensorCallback), true, true);
        pBLEScan->start(0, nullptr, false);
    }
};

SensorsScanner *m_pGoveeScanner = nullptr;

SensorsScanner *GoveeSensorsScanner::getScan()
{
    if (m_pGoveeScanner == nullptr)
    {
        m_pGoveeScanner = new GoveeSensorsScannerImpl();
    }
    return m_pGoveeScanner;
}
