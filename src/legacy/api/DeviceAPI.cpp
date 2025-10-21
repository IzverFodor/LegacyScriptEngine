#include "api/DeviceAPI.h"

#include "api/APIHelp.h"
#include "ll/api/service/Bedrock.h"
#include "magic_enum.hpp"
#include "mc/certificates/WebToken.h"
#include "mc/common/ActorRuntimeID.h"
#include "mc/deps/input/InputMode.h"
#include "mc/deps/json/Value.h"
#include "mc/network/ConnectionRequest.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/SerializedSkin.h"
#include "cpp-base64/base64.h"
#include "mc/deps/core/image/Image.h"
#include "mc/deps/core/container/Blob.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
// #include <filesystem>


#include <string>

//////////////////// Class Definition ////////////////////
ClassDefine<void> InputModeStaticBuilder = EnumDefineBuilder<InputMode>::build("InputMode");

ClassDefine<DeviceClass> DeviceClassBuilder = defineClass<DeviceClass>("LLSE_Device")
                                                  .constructor(nullptr)
                                                  .instanceProperty("ip", &DeviceClass::getIP)
                                                  .instanceProperty("avgPing", &DeviceClass::getAvgPing)
                                                  .instanceProperty("avgPacketLoss", &DeviceClass::getAvgPacketLoss)
                                                  .instanceProperty("lastPing", &DeviceClass::getLastPing)
                                                  .instanceProperty("lastPacketLoss", &DeviceClass::getLastPacketLoss)
                                                  .instanceProperty("os", &DeviceClass::getOs)
                                                  .instanceProperty("inputMode", &DeviceClass::getInputMode)
                                                  //   .instanceProperty("playMode", &DeviceClass::getPlayMode)
                                                  .instanceProperty("serverAddress", &DeviceClass::getServerAddress)
                                                  .instanceProperty("clientId", &DeviceClass::getClientId)
                                                  .instanceProperty("skin", [](DeviceClass* self) {
                                                      return self->getSkin();
                                                  })
                                                  .instanceProperty("skinSize", [](DeviceClass* self) {
                                                      return self->getSkinSize();
                                                  })
                                                  .build();

//////////////////// Classes ////////////////////

// 生成函数
Local<Object> DeviceClass::newDevice(Player* player) {
    auto newp = new DeviceClass(player);
    return newp->getScriptObject();
}

// 成员函数
void DeviceClass::setPlayer(Player* player) {
    try {
        if (player) {
            mWeakEntity = player->getWeakEntity();
            mValid      = true;
        }
    } catch (...) {
        mValid = false;
    }
}

Player* DeviceClass::getPlayer() {
    if (mValid) {
        return mWeakEntity.tryUnwrap<Player>().as_ptr();
    } else {
        return nullptr;
    }
}

Local<Value> DeviceClass::getSkinSize() {
    Player* player = getPlayer();
    if (!player) return Local<Value>();

    const SerializedSkin& skin = player->getSkin();
    const mce::Image& img = skin.mSkinImage;

    int width  = static_cast<int>(img.mWidth);
    int height = static_cast<int>(img.mHeight);

    Local<Object> result = Object::newObject(); 
    result.set("width", Number::newNumber(width));
    result.set("height", Number::newNumber(height));

    return result;
}

Local<Value> DeviceClass::getSkin() {
    Player* player = getPlayer();
    if (!player) return Local<Value>();

    const SerializedSkin& skin = player->getSkin();
    const mce::Image& img = skin.mSkinImage;

    const mce::Blob& blob = img.mImageBytes.get();
    const unsigned char* pixels = blob.cbegin();
    int width  = static_cast<int>(img.mWidth);
    int height = static_cast<int>(img.mHeight);
    int channels = 4; // RGBA

    // std::filesystem::path path = "skins";
    // std::filesystem::create_directories(path);
    // std::string filename = (path / (player->getName() + "_skin.png")).string();
    // int resFile = stbi_write_png(filename.c_str(), width, height, channels, pixels, width * channels);
    // if (resFile == 0) {
    //     return String::newString("Failed to save PNG");
    // }


    std::vector<unsigned char> pngBuffer;
    auto write_callback = [](void* context, void* data, int size) {
        auto* buffer = reinterpret_cast<std::vector<unsigned char>*>(context);
        buffer->insert(buffer->end(), (unsigned char*)data, (unsigned char*)data + size);
    };

    int resMem = stbi_write_png_to_func(write_callback, &pngBuffer, width, height, channels, pixels, width * channels);
    if (resMem == 0) {
        return String::newString("Failed to encode PNG to memory");
    }

    std::string base64Skin = base64_encode(pngBuffer.data(), pngBuffer.size());

    return String::newString(base64Skin);
}


Local<Value> DeviceClass::getIP() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return String::newString(player->getNetworkIdentifier().getIPAndPort());
    }
    CATCH("Fail in GetIP!")
}

Local<Value> DeviceClass::getAvgPing() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return Number::newNumber(player->getNetworkStatus()->mAveragePing);
    }
    CATCH("Fail in getAvgPing!")
}

Local<Value> DeviceClass::getAvgPacketLoss() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return Number::newNumber(player->getNetworkStatus()->mAveragePacketLoss);
    }
    CATCH("Fail in getAvgPacketLoss!")
}

Local<Value> DeviceClass::getLastPing() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return Number::newNumber(player->getNetworkStatus()->mCurrentPing);
    }
    CATCH("Fail in getLastPing!")
}

Local<Value> DeviceClass::getLastPacketLoss() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return Number::newNumber(player->getNetworkStatus()->mCurrentPacketLoss);
    }
    CATCH("Fail in getLastPacketLoss!")
}

Local<Value> DeviceClass::getOs() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return String::newString(magic_enum::enum_name(player->getPlatform()));
    }
    CATCH("Fail in getOs!")
}

Local<Value> DeviceClass::getServerAddress() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        if (player->isSimulatedPlayer()) String::newString("unknown");
        Json::Value& requestJson = player->getConnectionRequest()->mRawToken->mDataInfo;
        return String::newString(requestJson["ServerAddress"].asString("unknown"));
    }
    CATCH("Fail in getServerAddress!")
}

Local<Value> DeviceClass::getClientId() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        return String::newString(player->getConnectionRequest()->getDeviceId());
    }
    CATCH("Fail in getClientId!")
}

Local<Value> DeviceClass::getInputMode() {
    try {
        Player* player = getPlayer();
        if (!player) return Local<Value>();

        Json::Value& requestJson = player->getConnectionRequest()->mRawToken->mDataInfo;
        return Number::newNumber(requestJson["CurrentInputMode"].asInt(0));
    }
    CATCH("Fail in getInputMode!")
}

// Local<Value> DeviceClass::getPlayMode() {
//     try {
//         Player* player = getPlayer();
//         if (!player) return Local<Value>();

//         return Number::newNumber(0);
//     }
//     CATCH("Fail in getPlayMode!")
// }
