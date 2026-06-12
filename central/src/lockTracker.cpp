#include "lockTracker.h"
#include <iostream>
#include <string>

using namespace std::string_literals;

LockTracker::LockTracker() {}

LockTracker::~LockTracker() {}

void LockTracker::lock(int clientId) {

    try {
        std::cout << "Waiting on mutex for processing of: " << clientId
                  << std::endl;

        locked.wait(true); // wait for false
        locked = true;

        std::cout << "Unlocked, lock again" << std::endl;

        lockedBy = clientId;
    } catch (std::exception const &e) {
        throw std::runtime_error("Exception in lock tracker lock: "s + e.what());
    }
}

void LockTracker::unlock(int clientId) {

    try {
        if (lockedBy != clientId) {
            throw std::runtime_error("Tried to unlock by wrong client ID");
        }

        locked = false;
        locked.notify_one();

        lockedBy = -1;
    } catch (std::exception const &e) {
        throw std::runtime_error("Exception in lock tracker unlock");
    }
}