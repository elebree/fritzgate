#include <NimBLEDevice.h>
#include <decoder.h>

#include "TheengsSensorsScanner.h"
#include "../logging.h"

class TheengsSensorsAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
private:
    SensorScannerCallback *m_pCallback = nullptr;
    TheengsDecoder decoder;
    StaticJsonDocument<512> doc;

    std::string convertServiceData(std::string deviceServiceData)
    {
        int serviceDataLength = (int)deviceServiceData.length();
        char spr[2 * serviceDataLength + 1];
        for (int i = 0; i < serviceDataLength; i++)
            sprintf(spr + 2 * i, "%.2x", (unsigned char)deviceServiceData[i]);
        spr[2 * serviceDataLength] = 0;
        return spr;
    }

public:
    TheengsSensorsAdvertisedDeviceCallbacks(SensorScannerCallback *callback) : m_pCallback(callback) {}

    void onResult(BLEAdvertisedDevice *advertisedDevice) override
    {
        // Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
        doc.clear();
        JsonObject BLEdata = doc.to<JsonObject>();
        String mac_adress = advertisedDevice->getAddress().toString().c_str();
        mac_adress.toUpperCase();
        BLEdata["id"] = (char *)mac_adress.c_str();

        if (advertisedDevice->haveName())
            BLEdata["name"] = (char *)advertisedDevice->getName().c_str();

        if (advertisedDevice->haveManufacturerData())
        {
            char *manufacturerdata = BLEUtils::buildHexData(NULL, (uint8_t *)advertisedDevice->getManufacturerData().data(), advertisedDevice->getManufacturerData().length());
            BLEdata["manufacturerdata"] = manufacturerdata;
            free(manufacturerdata);
        }

        if (advertisedDevice->haveRSSI())
            BLEdata["rssi"] = (int)advertisedDevice->getRSSI();

        if (advertisedDevice->haveTXPower())
            BLEdata["txpower"] = (int8_t)advertisedDevice->getTXPower();

        if (advertisedDevice->haveServiceData())
        {
            int serviceDataCount = advertisedDevice->getServiceDataCount();
            for (int j = 0; j < serviceDataCount; j++)
            {
                std::string service_data = convertServiceData(advertisedDevice->getServiceData(j));
                BLEdata["servicedata"] = service_data;
                std::string serviceDatauuid = advertisedDevice->getServiceDataUUID(j).toString();
                BLEdata["servicedatauuid"] = serviceDatauuid;
            }
        }

        if (decoder.decodeBLEJson(BLEdata))
        {
            if (BLEdata.containsKey("id") && BLEdata.containsKey("tempc"))
            {
                LOG_BEGIN(LOG_COLOR_DEVICE);
                Serial.print("Theengs device: ");
                serializeJson(BLEdata, Serial);
                LOG_ENDLN();

                Sensor sensorData;
                auto brand = BLEdata.containsKey("brand") ? BLEdata["brand"].as<std::string>() : "";
                auto model = BLEdata.containsKey("model") ? BLEdata["model"].as<std::string>() : "";
                auto model_id = BLEdata.containsKey("model_id") ? BLEdata["model_id"].as<std::string>() : "";
                auto name = BLEdata.containsKey("name") ? BLEdata["name"].as<std::string>() : "";

                // get an exact model of govee sensors from the name.
                if (model_id == "H5072/75" || model_id == "H5100/01/02/04/05/74/77" || model_id == "H5072/75")
                    model_id = name.substr(2, 5);

                auto id = BLEdata["id"].as<std::string>();

                sensorData.id = id;
                sensorData.serial = id.substr(12, 2) + id.substr(15, 2);
                sensorData.name = brand + " " + model_id + " " + sensorData.serial;
                sensorData.model = model_id;
                sensorData.data = BLEdata.containsKey("manufacturerdata") ? BLEdata["manufacturerdata"].as<std::string>() : "";
                sensorData.rssi = BLEdata.containsKey("rssi") ? BLEdata["rssi"].as<int>() : 0;
                sensorData.temperature = BLEdata.containsKey("tempc") ? (int)(10.0 * BLEdata["tempc"].as<double>()) : 0;
                sensorData.humidity = BLEdata.containsKey("hum") ? (int)(10.0 * BLEdata["hum"].as<double>()) : 0;
                sensorData.battery = BLEdata.containsKey("batt") ? BLEdata["batt"].as<int>() : 0;
                m_pCallback->onResult(&sensorData);
            }
        }
    }
};

class TheengsSensorsScannerImpl : public SensorsScanner
{
private:
    BLEScan *pBLEScan = nullptr;

public:
    TheengsSensorsScannerImpl() {}

    void init(const std::__cxx11::string deviceName) override
    {
        BLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE);
        BLEDevice::setScanDuplicateCacheSize(200);
        BLEDevice::init(deviceName);
    }

    void start(SensorScannerCallback *pSensorCallback) override
    {
        if (pBLEScan == nullptr)
        {
            pBLEScan = BLEDevice::getScan();
        }
        pBLEScan->setAdvertisedDeviceCallbacks(new TheengsSensorsAdvertisedDeviceCallbacks(pSensorCallback), true);
        pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
        pBLEScan->setInterval(97);     // How often the scan occurs / switches channels; in milliseconds,
        pBLEScan->setWindow(37);       // How long to scan during the interval; in milliseconds.
        pBLEScan->setMaxResults(0);    // do not store the scan results, use callback only.

        if (pBLEScan->isScanning() == false)
        {
            // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
            pBLEScan->start(0, nullptr, false);
        }
    }
};

SensorsScanner *m_pTheengsScanner = nullptr;

SensorsScanner *TheengsSensorsScanner::getScan()
{
    if (m_pTheengsScanner == nullptr)
    {
        m_pTheengsScanner = new TheengsSensorsScannerImpl();
    }
    return m_pTheengsScanner;
}
