#ifndef EVENTBUS_H
#define EVENTBUS_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

// Thread-safe publish/subscribe bus used to push SSE events to all connected
// clients. Each SSE connection calls subscribe() to get a Connection handle,
// then blocks in its provider lambda waiting on the condition variable.
// publish() pushes a fully-formed SSE string (must end with "\n\n") to every
// live connection's queue and wakes its waiting thread.
class EventBus {
public:
    struct Connection {
        std::queue<std::string> queue;
        std::mutex              mu;
        std::condition_variable cv;
        bool                    closed{false};
    };

    std::shared_ptr<Connection> subscribe();
    void unsubscribe(std::shared_ptr<Connection> conn);
    void publish(const std::string& sseMsg);   // sseMsg must end with "\n\n"

private:
    std::vector<std::shared_ptr<Connection>> conns_;
    std::mutex                               mu_;
};

#endif
