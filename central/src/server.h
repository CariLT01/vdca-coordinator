#include "messageTypes.h"
#include "lockTracker.h"
#include <WinSock2.h>
#include <condition_variable>
#include <unordered_set>
#define ASIO_STANDALONE 1 // Add this line FIRST
#define WEBSOCKETPP_CPP11_CHRONO 1 // Forces native modern STL chrono clocks
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> Server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

struct ConnectedClient {
    websocketpp::connection_hdl hdl;
    ClientType clientType;
    bool lockInProgress;
    int id;
};

struct LockQueueAction {
    std::shared_ptr<ConnectedClient> owner;

    bool operator<(const LockQueueAction& other) const {
        if (other.owner->clientType == ClientType::HUMAN) {
            return true;
        } else {
            return false;
        }
    }
};

class ControlServer {
public:
    ControlServer(const int port);
    ~ControlServer();

    void listenForClients();
    void initialize();

private:

    void cleanup();
    

    void onMessage(websocketpp::connection_hdl hdl, Server::message_ptr msg);
    void onOpen(websocketpp::connection_hdl hdl);

    void processLock(std::shared_ptr<ConnectedClient> client);
    void lockThread(std::shared_ptr<ConnectedClient> client);
    void queueThread();
    void pushAction(const LockQueueAction& action);

    void updateClientStatuses();

    int port;
    int clientIdCounter;
    bool stop = false;

    std::shared_ptr<ConnectedClient> getClientFromHdl(websocketpp::connection_hdl hdl);
    
    std::shared_ptr<Server> wsServer;
    std::shared_ptr<LockTracker> lockTracker;
    std::map<websocketpp::connection_hdl, std::shared_ptr<ConnectedClient>, std::owner_less<std::weak_ptr<void>>> clients;

    std::priority_queue<LockQueueAction> lockQueue;
    std::unordered_set<int> clientsInQueue;
    std::unordered_map<int, int> clientsInFront;
    std::mutex lockQueueMutex;
    std::condition_variable cv;
    std::thread queueThreadThread;
};