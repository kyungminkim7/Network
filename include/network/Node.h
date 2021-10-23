#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

#include <asio/io_context.hpp>

namespace ntwk {

class TcpPublisher;
class TcpSubscriber;

class Node {
public:
    Node();
    ~Node();

    std::shared_ptr<TcpPublisher> advertise(unsigned short port);
    std::shared_ptr<TcpSubscriber> subscribe(const std::string &host, unsigned short port,
                                             std::function<void(std::unique_ptr<uint8_t[]>)> msgReceivedHandler);

    void run();
    void runOnce();

private:
    asio::io_context mainContext;
    asio::io_context tasksContext;

    std::thread tasksThread;
};

} // namespace ntwk
