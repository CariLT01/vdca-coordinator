#include "packetUtils.h"
#include "messageTypes.h"
#include <cstring>
#include <bit>
#include <cstdint>

std::string PacketUtils::encodePacket(MessageTypes messageType, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> binaryData = payload;
    binaryData.insert(binaryData.begin(), static_cast<uint8_t>(messageType));

    std::string binaryDataString(reinterpret_cast<const char*>(binaryData.data()), 1);
    return binaryDataString;
}

std::vector<uint8_t> PacketUtils::intToBytes(int v) {
    std::vector<uint8_t> bytes(sizeof(v));
    std::memcpy(bytes.data(), &v, sizeof(v));
    return bytes;
}

int PacketUtils::bytesToInt(const std::array<uint8_t, 4>& bytes) {
    int32_t value = std::bit_cast<int32_t>(bytes);
    return value;
}