#include <iostream>
#include "server.h"

int main() {

    std::cout << "Starting the server!" << std::endl;

    auto server = std::make_unique<ControlServer>(5000);
    server->initialize();
    server->listenForClients();

    
    return 0;
}