#pragma once

#include <cstdint>

enum class MessageTypes : uint8_t {
    CLIENT_REQUEST_LOCK,
    CLIENT_IDENTITY_BOT,
    CLIENT_IDENTITY_HUMAN,
    CLIENT_UNLOCK,
    SERVER_RESPONSE_STATE_WAIT,
    SERVER_RESPONSE_CLIENT_ID,
    SERVER_RESPONSE_QUEUE_STATUS
};

enum class QueueStatus : uint8_t {
    WAITING_FOR_HIGHER_PRIORITY,
    WAITING_IN_QUEUE,
    LOCK_OWNED
};

enum class ClientType : uint8_t {
    UNKNOWN,
    BOT,
    HUMAN
};