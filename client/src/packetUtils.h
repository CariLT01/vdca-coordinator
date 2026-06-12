#include "messageTypes.h"
#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace PacketUtils {
    std::string encodePacket(MessageTypes mt, const std::vector<uint8_t>& payload);

    std::vector<uint8_t> intToBytes(int value);
    int bytesToInt(const std::array<uint8_t, 4>& bytes);
}