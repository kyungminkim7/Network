#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>

#include <asio/ip/tcp.hpp>

#include <std_msgs/Header_generated.h>

namespace ntwk {

class TcpSubscriber : public std::enable_shared_from_this<TcpSubscriber> {
public:
    using MessageReceivedHandler = std::function<void(std::unique_ptr<uint8_t[]>)>;

    static std::shared_ptr<TcpSubscriber> create(asio::io_context &ioContext,
                                                 const std::string &host, unsigned short port,
                                                 MessageReceivedHandler msgReceivedHandler,
                                                 unsigned int msgQueueSize, bool compressed);

    void update();

private:
    TcpSubscriber(asio::io_context &ioContext,
                  const std::string &host, unsigned short port,
                  MessageReceivedHandler msgReceivedHandler,
                  unsigned int msgQueueSize, bool compressed);

    static void connect(std::shared_ptr<TcpSubscriber> subscriber);

    static void receiveMsgHeader(std::shared_ptr<TcpSubscriber> subscriber,
                                 std::unique_ptr<std_msgs::Header> msgHeader,
                                 unsigned int totalMsgHeaderBytesReceived);
    static void receiveMsg(std::shared_ptr<TcpSubscriber> subscriber,
                           std::unique_ptr<uint8_t[]> msg,
                           unsigned int msgSize_bytes, unsigned int totalMsgBytesReceived);

    asio::ip::tcp::socket socket;
    asio::ip::tcp::endpoint endpoint;

    MessageReceivedHandler msgReceivedHandler;

    std::queue<std::unique_ptr<uint8_t[]>> msgQueue;
    unsigned int msgQueueSize;

    bool compressed;
};

} // namespace ntwk
