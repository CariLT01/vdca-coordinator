#include <iostream>
#include "client.h"


int main() {

    std::cout << "Program initializing" << std::endl;

    std::unique_ptr<ControlClient> client = std::make_unique<ControlClient>(8080, 5000);
    client->initialize();
    client->run();
    return 0;
}