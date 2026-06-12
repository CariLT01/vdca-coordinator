#include "websocketpp/common/connection_hdl.hpp"
#include <condition_variable>
#include <iostream>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

typedef websocketpp::server<websocketpp::config::asio> Server;
typedef websocketpp::client<websocketpp::config::asio> Client;

class ControlClient {
public:
    ControlClient(const int port, const int destinationPort);
    ~ControlClient();

    static ControlClient* sInstance;

    void initialize();
    void run();
private:

    int port;
    int destinationPort;

    std::shared_ptr<Server> wsServer;
    std::shared_ptr<Client> wsClient;

    websocketpp::connection_hdl clientHdl;
    websocketpp::connection_hdl browserHdl;
    bool browserConnectionOpen;
    bool isLocked;
    bool isHolding;

    std::thread clientThread;
    std::thread serverThread;

    // Exit on Ctrl + C
    std::condition_variable exitCv;
    std::atomic<bool> shouldExit{false};
    std::mutex exitMu;
    static void exitSignalHandler(int sigint);

    void initializeServer();
    void initializeClient();

    void initializeHooksInternal();
    void initializeHooks();
    void unhook();

    int queuedX = 0;
    int queuedY = 0;

    
    void onClientOpen(websocketpp::connection_hdl hdl);
    void onBrowserOpen(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, Server::message_ptr msg);
    void onClientMessage(websocketpp::connection_hdl hdl, Server::message_ptr msg);

    void handleMouse();

    // Windows hooks
    HHOOK hMouseHook = NULL;

    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wparam, LPARAM lparam);
};