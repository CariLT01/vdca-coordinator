#include <mutex>
#include <atomic>

class LockTracker {
    public:
        LockTracker();
        ~LockTracker();

        void lock(int clientId);
        void unlock(int clientId);
        bool isLocked() {
            return locked;
        }
        void waitFor(bool v) {
            locked.wait(!v);
        }

        int getCurrentLockHolder() {
            return lockedBy;
        }


    private:
        std::mutex mutexLock;
        int lockedBy;
        std::atomic<bool> locked{false};
};