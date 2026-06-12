#include "client.h"
#include "messageTypes.h"
#include "websocketpp/close.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include <stdexcept>
#include <windows.h>
#include "packetUtils.h"
#include "websocketpp/frame.hpp"
#include <csignal>
#include <thread>

ControlClient* ControlClient::sInstance = nullptr;

ControlClient::ControlClient(const int port, const int destinationPort) : port(port), destinationPort(destinationPort) {
    sInstance = this;
}

ControlClient::~ControlClient() {
    std::cout << "Shutting down client" << std::endl;
    wsClient->stop();
    wsServer->stop();
}

void ControlClient::initialize() {
    initializeClient();
    initializeServer();
    initializeHooks();

    std::cout << "Initialization complete" << std::endl;
}



void ControlClient::initializeServer() {
    wsServer = std::make_shared<Server>();

    try {

        wsServer->set_access_channels(websocketpp::log::alevel::all);
        wsServer->clear_access_channels(websocketpp::log::alevel::frame_payload);

        wsServer->init_asio();

        wsServer->set_message_handler([this](websocketpp::connection_hdl hdl, Server::message_ptr msg) {
            onMessage(hdl, msg);
        });

        wsServer->listen(port);
        wsServer->start_accept();

        std::cout << "Server listening on port " << port << std::endl;
    } catch (websocketpp::exception const & e) {
        throw std::runtime_error("WebSocket server creation error: " + std::string(e.what()));
    } catch (...) {
        throw std::runtime_error("Unknown exception occurred during server creation");
    }
}

void ControlClient::onClientOpen(websocketpp::connection_hdl hdl) {
    clientHdl = hdl;

    // Send identity
    wsClient->send(clientHdl, PacketUtils::encodePacket(MessageTypes::CLIENT_IDENTITY_HUMAN, {}), websocketpp::frame::opcode::BINARY);
}

void ControlClient::onBrowserOpen(websocketpp::connection_hdl hdl) {
    if (browserConnectionOpen) {
        std::cout << "Disconnecting new client; existing one already open" << std::endl;
        wsServer->close(hdl, websocketpp::close::status::normal, "Another client is already connected!");
    }

    browserHdl = hdl;
    std::cout << "New browser client connected" << std::endl;
}

void ControlClient::onMessage(websocketpp::connection_hdl hdl, Server::message_ptr msg) {
    std::string message = msg->get_payload();
    std::vector<uint8_t> payloadBinary(message.begin(), message.end());

    uint8_t messageType = payloadBinary[0];
    if (messageType == static_cast<uint8_t>(MessageTypes::BROWSER_REQUEST_COMPLETE)) {
        if (!isLocked) {
            std::cout << "Nothing to unlock, no lock pending in progress" << std::endl;
            return;
        }
        if (!isHolding) {
            std::cout << "Lock pending but not owned" << std::endl;
            return;
        }

        // release lock
        std::cout << "Releasing lock!" << std::endl;

        wsClient->send(clientHdl, PacketUtils::encodePacket(MessageTypes::CLIENT_UNLOCK, {}), websocketpp::frame::opcode::BINARY);
        
        isLocked = false;
        isHolding = false;
    
    } else {
        throw std::runtime_error(&"Unimplemented or unknown message type: " [ messageType]);
    }
}

void ControlClient::onClientMessage(websocketpp::connection_hdl hdl, Server::message_ptr msg) {
    try {
        std::string message = msg->get_payload();
        std::vector<uint8_t> payloadBinary(message.begin(), message.end());

        uint8_t messageType = payloadBinary[0];
        if (messageType == static_cast<uint8_t>(MessageTypes::SERVER_RESPONSE_QUEUE_STATUS)) {

            if (payloadBinary.size() < 2) {
                throw std::runtime_error("Payload binary is too small to contain queue status");
            }

            uint8_t queueStatus = payloadBinary[1];
            std::cout << "Queue status: " << queueStatus << std::endl;

            if (queueStatus == static_cast<uint8_t>(QueueStatus::LOCK_OWNED)) {
                std::cout << "Client owns the lock" << std::endl;
                isHolding = true;

                // reproducing click
                SetCursorPos(queuedX, queuedY);

                // Prepare this giant struct
                INPUT inputs[3] = {0};
                // Left button down
                inputs[0].type = INPUT_MOUSE;
                inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

                // Left button up
                inputs[1].type = INPUT_MOUSE;
                inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

                SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
                std::cout << "Synthetic event reproduced and sent" << std::endl;
            }

            // forward
            wsServer->send(browserHdl, message, websocketpp::frame::opcode::BINARY);
        } else if (messageType == static_cast<uint8_t>(MessageTypes::SERVER_RESPONSE_CLIENT_ID)) {
            if (payloadBinary.size() < 5) {
                throw std::runtime_error("Payload binary is too small to contain client ID");
            }

            int clientId = PacketUtils::bytesToInt({payloadBinary[1], payloadBinary[2], payloadBinary[3], payloadBinary[4]});
            
            std::cout << "Received client ID: " << clientId << std::endl;
        } else {
            throw std::runtime_error(&"Unknown or unimplemented packet type: " [ static_cast<uint8_t>(messageType)]);
        }
    } catch (std::exception const &e) {
        std::cerr << "Exception during handling of WebSocket message: " << e.what() << std::endl;
    }
}

void ControlClient::initializeClient() {
    wsClient = std::make_shared<Client>();
    try {
        wsClient->set_access_channels(websocketpp::log::alevel::all);
        wsClient->clear_access_channels(websocketpp::log::alevel::frame_payload);

        wsClient->init_asio();

        wsClient->set_message_handler([this](websocketpp::connection_hdl hdl, Server::message_ptr msg) {
            onClientMessage(hdl, msg);
        });

        wsClient->set_open_handler([this](websocketpp::connection_hdl hdl) {
            onClientOpen(hdl);
        });

        websocketpp::lib::error_code ec;
        Client::connection_ptr con = wsClient->get_connection("ws://127.0.0.1:5000", ec);

        if (ec) {
            throw std::runtime_error("Could not create connection: " + ec.message());
        }

        wsClient->connect(con);

    } catch (std::exception const &e) {
        std::cerr << "Init exception: " << e.what() << std::endl;
    }
}

void ControlClient::handleMouse() {
    // send request

    if (isLocked) {
        std::cout << "Request already sent" << std::endl;
        return;
    }

    POINT cursorPos;
    if (!GetCursorPos(&cursorPos)) {
        throw std::runtime_error("Failed to get cursor position");
    }

    queuedX = cursorPos.x;
    queuedY = cursorPos.y;

    isLocked = true;
    std::string packetData = PacketUtils::encodePacket(MessageTypes::CLIENT_REQUEST_LOCK, {});
    wsClient->send(clientHdl, packetData, websocketpp::frame::opcode::BINARY);
}

LRESULT CALLBACK ControlClient::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_LBUTTONDOWN) {
            std::cout << "Blocked left button" << std::endl;
            if (ControlClient::sInstance) {
                ControlClient::sInstance->handleMouse();
            }
            return 1;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void ControlClient::initializeHooksInternal() {
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, 0);

    if (!hMouseHook) {
        throw std::runtime_error("Failed to instantiate mouse hook handle");
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

}

void ControlClient::initializeHooks() {
    std::cout << "Instantiating hook thread" << std::endl;
    std::thread hookThread(&ControlClient::initializeHooksInternal, this);
    hookThread.detach();
}

void ControlClient::exitSignalHandler(int sigint) {
    if (sigint == SIGINT) {
        ControlClient::sInstance->shouldExit = true;
    }
}

void ControlClient::run() {
    clientThread = std::thread([this]() {
        wsClient->run();
    });

    serverThread = std::thread([this]() {
        wsServer->run();
    });

    std::cout << "Server and client are now running" << std::endl;
    std::cout << "Press Ctrl + C to quit" << std::endl;

    std::signal(SIGINT, ControlClient::exitSignalHandler);

    shouldExit.wait(false);

    std::cout << "Terminating application" << std::endl;


}