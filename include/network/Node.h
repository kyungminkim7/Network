#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <utility>

#include <asio/io_context.hpp>
#include <flatbuffers/flatbuffers.h>

namespace ntwk {

class TcpPublisher;
class TcpSubscriber;

class Node {
public:
    using Endpoint = std::pair<std::string, unsigned short>;
    using PublisherPtr = std::shared_ptr<TcpPublisher>;
    using SubscriberPtr = std::shared_ptr<TcpSubscriber>;

    Node();
    ~Node();

    void advertise(unsigned short port);
    void subscribe(const std::string &host, unsigned short port,
                   std::function<void(std::unique_ptr<uint8_t[]>)> msgReceivedHandler);

    void publish(std::shared_ptr<flatbuffers::DetachedBuffer> msg);

    void run();
    void runOnce();

private:
    asio::io_context mainContext;
    asio::io_context tasksContext;

    std::thread tasksThread;

    std::map<Endpoint, SubscriberPtr> subscribers;
    PublisherPtr publisher;

};

} // namespace ntwk
