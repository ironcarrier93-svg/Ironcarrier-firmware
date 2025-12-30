#ifndef __PN532_BRUCE_H__
#define __PN532_BRUCE_H__

/**
 * @file PN532.h
 * @author Rennan Cockles (https://github.com/rennancockles)
 * @brief Read and Write RFID tags using PN532 module
 * @version 0.1
 * @date 2024-08-19
 */
#ifndef NFC_INTERFACE_I2C
#define NFC_INTERFACE_I2C
#endif
#ifndef NFC_INTERFACE_SPI
#define NFC_INTERFACE_SPI
#endif
#include "RFIDInterface.h"
#include <optional>
#include <SPI.h>
#include <Wire.h>

#include <PN532_I2C.h>
#include <PN532_SPI.h>

#include <PN532.h>

class PN_532 : public RFIDInterface {
public:
    enum CONNECTION_TYPE { I2C = 1, I2C_SPI = 2, SPI = 3 };
    enum PICC_Type {
        PICC_TYPE_MIFARE_MINI = 0x09, // MIFARE Classic protocol, 320 bytes
        PICC_TYPE_MIFARE_1K = 0x08,   // MIFARE Classic protocol, 1KB
        PICC_TYPE_MIFARE_4K = 0x18,   // MIFARE Classic protocol, 4KB
        PICC_TYPE_MIFARE_UL = 0x00,   // MIFARE Ultralight or Ultralight C
    };

    // Devices such as T-Embed CC1101 uses an embedded PN532 that needs the IRQ and RST pins to work
    // If using other device that uses, set -DPN532_IRQ=pin_num and -DPN532_RF_REST=pin_num to platformio.ini
    // of this particular device, should not be used in other devices on I2C mode
    PN_532(CONNECTION_TYPE connection_type = I2C);
    ~PN_532() = default;
    PN532 *getNfc();

    /////////////////////////////////////////////////////////////////////////////////////
    // Life Cycle
    /////////////////////////////////////////////////////////////////////////////////////
    bool begin();

    /////////////////////////////////////////////////////////////////////////////////////
    // Operations
    /////////////////////////////////////////////////////////////////////////////////////
    int read(int cardBaudRate = PN532_MIFARE_ISO14443A);
    int clone();
    int erase();
    int write(int cardBaudRate = PN532_MIFARE_ISO14443A);
    int write_ndef();
    int load();
    int save(String filename);
    int emulate();

private:
    bool _use_i2c;
    CONNECTION_TYPE _connection_type;
    SPIClass _spi;
    std::optional<PN532_SPI> pn532_spi;
    std::optional<PN532_I2C> pn532_i2c;
    std::optional<PN532> nfc;

    /////////////////////////////////////////////////////////////////////////////////////
    // Converters
    /////////////////////////////////////////////////////////////////////////////////////
    void format_data();
    void format_data_felica(uint8_t idm[8], uint8_t pmm[8], uint16_t sys_code);
    void parse_data();
    void set_uid();

    /////////////////////////////////////////////////////////////////////////////////////
    // PICC Helpers
    /////////////////////////////////////////////////////////////////////////////////////
    String get_tag_type();
    int read_data_blocks();
    int read_mifare_classic_data_blocks();
    int read_mifare_classic_data_sector(byte sector);
    int authenticate_mifare_classic(byte block);
    int read_mifare_ultralight_data_blocks();

    int write_data_blocks();
    bool write_mifare_classic_data_block(int block, String data);
    bool write_mifare_ultralight_data_block(int block, String data);

    int read_felica_data();

    int erase_data_blocks();
    int write_ndef_blocks();

    int write_felica_data_block(int block, String data);
};

#endif
