#include "messageTypes.h"
#include <vector>
#include <string>

namespace PacketUtils {
    std::string encodePacket(MessageTypes mt, const std::vector<uint8_t>& payload);

    std::vector<uint8_t> intToBytes(int value);
}