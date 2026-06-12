#include "server.h"
#include "messageTypes.h"
#include "packetUtils.h"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/frame.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

const int BUFFER_SIZE = 1024;

ControlServer::ControlServer(const int port) : port(port), lockTracker(std::make_shared<LockTracker>()) {}

ControlServer::~ControlServer() {
    std::cout << "Shutting down server" << std::endl;
    stop = true;
    queueThreadThread.join();
    wsServer->stop();
}

void ControlServer::initialize() {
    wsServer = std::make_shared<Server>();

    try {

        std::thread qt = std::thread(&ControlServer::queueThread, this);
        queueThreadThread = std::move(qt);

        wsServer->set_access_channels(websocketpp::log::alevel::all);
        wsServer->clear_access_channels(
            websocketpp::log::alevel::frame_payload);

        wsServer->init_asio();

        wsServer->set_message_handler(
            [this](websocketpp::connection_hdl hdl, Server::message_ptr msg) {
                onMessage(hdl, msg);
            });
        wsServer->set_open_handler(
            [this](websocketpp::connection_hdl hdl) { onOpen(hdl); });

        wsServer->listen(port);
        wsServer->start_accept();

        std::cout << "Server listening on port " << port << std::endl;

        wsServer->run();
    } catch (websocketpp::exception const &e) {
        throw std::runtime_error("WebSocket server creation error: " +
                                 std::string(e.what()));
    } catch (...) {
        throw std::runtime_error(
            "Unknown exception occurred during server creation");
    }
}

std::shared_ptr<ConnectedClient>
ControlServer::getClientFromHdl(websocketpp::connection_hdl hdl) {

    if (!clients.contains(hdl)) {
        throw std::runtime_error("Connection HDL not found");
    }

    return clients[hdl];
}

void ControlServer::queueThread() {
    while (true) {

        LockQueueAction action;

        std::cout << "Waiting for action" << std::endl;

        {
            std::unique_lock<std::mutex> lock(lockQueueMutex);
            cv.wait(lock, [this]() { return !lockQueue.empty() || stop; });

            std::cout << "We found something" << std::endl;
            
            if (stop && lockQueue.empty()) {
                return;
            }

            LockQueueAction top = lockQueue.top();

            // If it's a low priority request, wait 750ms to see if a higher
            // priority one interrupts
            if (top.owner->clientType == ClientType::BOT) {

                // tell the bot that we are waiting for a higher priority
                // request
                QueueStatus qs = QueueStatus::WAITING_FOR_HIGHER_PRIORITY;
                wsServer->send(top.owner->hdl,
                               PacketUtils::encodePacket(
                                   MessageTypes::SERVER_RESPONSE_QUEUE_STATUS,
                                   {static_cast<uint8_t>(qs)}),
                               websocketpp::frame::opcode::BINARY);

                cv.wait_for(lock, std::chrono::milliseconds(750), [this]() {
                    return stop || (!lockQueue.empty() &&
                                    lockQueue.top().owner->clientType ==
                                        ClientType::HUMAN);
                });

                if (stop)
                    return;

                if (
                    lockQueue.top().owner->clientType == ClientType::HUMAN) {
                    continue;
                }
            }

            action = lockQueue.top();
            lockQueue.pop();

            clientsInQueue.erase(action.owner->id);
            clientsInFront.erase(action.owner->id);

            // notify who we are waiting on
            for (const auto &[hdl, client] : clients) {

                QueueStatus qs = QueueStatus::WAITING_IN_QUEUE;

                if (action.owner == client) {
                    qs = QueueStatus::LOCK_OWNED;
                }

                if (hdl.expired()) {
                    throw std::runtime_error("Expired HDL pointer");
                }

                wsServer->send(hdl,
                               PacketUtils::encodePacket(
                                   MessageTypes::SERVER_RESPONSE_QUEUE_STATUS,
                                   {static_cast<uint8_t>(qs)}),
                               websocketpp::frame::opcode::BINARY);
            }

            std::cout << "There are " << lockQueue.size()
                      << " requests waiting in queue" << std::endl;
        }

        // give permission to lock
        lockTracker->lock(action.owner->id);
        // wait for unlocking
        lockTracker->waitFor(false);
        // check if unlocked
        if (lockTracker->isLocked()) {
            throw std::logic_error("Lock is still locked!");
        }
    }
}

void ControlServer::onOpen(websocketpp::connection_hdl hdl) {

    auto client = std::make_shared<ConnectedClient>();
    client->id = clientIdCounter;
    clientIdCounter++; // TODO: secure client ID counter with atomic
    client->lockInProgress = false;
    client->clientType = ClientType::UNKNOWN;
    client->hdl = hdl;

    if (!hdl.expired()) {
        std::cout << "The HDL pointer is valid" << std::endl;
    }  else {
        std::cout << "HDL pointer expired" << std::endl;
    }

    clients[hdl] = client;

    std::cout << "Registered new client ID: " << clientIdCounter << std::endl;

    // send

    std::vector<uint8_t> clientIdVec = PacketUtils::intToBytes(client->id);
    std::string data = PacketUtils::encodePacket(
        MessageTypes::SERVER_RESPONSE_CLIENT_ID, clientIdVec);

    wsServer->send(hdl, data, websocketpp::frame::opcode::BINARY);
}

void ControlServer::pushAction(const LockQueueAction &action) {
    std::unique_lock<std::mutex> lock(lockQueueMutex);
    if (clientsInQueue.contains(action.owner->id)) {
        throw std::runtime_error(
            "This client is already in queue. Cannot add it again");
    }

    clientsInFront[action.owner->id] = lockQueue.size();
    lockQueue.push(action);
    clientsInQueue.insert(action.owner->id);

    std::cout << "Added action from: " << action.owner->id << std::endl;

    cv.notify_one();
}

void ControlServer::processLock(std::shared_ptr<ConnectedClient> client) {
    if (client->lockInProgress) {
        std::cout << "ignoring new lock: a lock is already in progress"
                  << std::endl;
        return;
    }
    client->lockInProgress = true;

    if (client->clientType == ClientType::BOT) {
        // need to wait 750ms

        auto action = LockQueueAction{};
        action.owner = client;

        pushAction(action);
    } else if (client->clientType == ClientType::HUMAN) {
        // wait for lock

        auto action = LockQueueAction{};
        action.owner = client;

        pushAction(action);

    } else {
        throw std::runtime_error("Unknown client type");
    }
}

void ControlServer::onMessage(websocketpp::connection_hdl hdl,
                              Server::message_ptr msg) {
    try {
        const std::string payload = msg->get_payload();
        const std::vector<uint8_t> payloadBinary(payload.begin(),
                                                 payload.end());
        const uint8_t messageType = payloadBinary[0];

        auto client = getClientFromHdl(hdl);

        if (messageType ==
            static_cast<uint8_t>(MessageTypes::CLIENT_IDENTITY_BOT)) {
            client->clientType = ClientType::BOT;
        } else if (messageType ==
                   static_cast<uint8_t>(MessageTypes::CLIENT_IDENTITY_HUMAN)) {
            client->clientType = ClientType::HUMAN;
        } else if (messageType ==
                   static_cast<uint8_t>(MessageTypes::CLIENT_REQUEST_LOCK)) {
            try {
                processLock(client);
            } catch (std::runtime_error const &e) {
                std::cerr << "Failed to process lock request on client: "
                          << e.what() << std::endl;
            }
        } else if (messageType ==
                   static_cast<uint8_t>(MessageTypes::CLIENT_UNLOCK)) {
            int currentLockHolder = lockTracker->getCurrentLockHolder();
            if (currentLockHolder != client->id) {
                std::cerr << "Cannot unlock: wrong lock holder" << std::endl;
                return;
            }
            lockTracker->unlock(client->id);
        } else {
            std::cerr << "Unknown message type: " << messageType << std::endl;
        }
    } catch (std::exception const &e) {
        std::cerr << "An error occurred during the handling of a message: "
                  << e.what() << std::endl;
    }
}

void ControlServer::listenForClients() { std::cout << "Listen" << std::endl; }