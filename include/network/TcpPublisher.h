#pragma once

#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>
#include <std_msgs/Header_generated.h>

namespace ntwk {

class TcpPublisher : public std::enable_shared_from_this<TcpPublisher> {
public:
    static std::shared_ptr<TcpPublisher> create(asio::io_context &ioContext,
                                                unsigned short port,
                                                unsigned int msgQueueSize,
                                                bool compressed);

    void publish(std::shared_ptr<flatbuffers::DetachedBuffer> msg);

    void update();

private:
    TcpPublisher(asio::io_context &ioContext, unsigned short port,
                 unsigned int msgQueueSize, bool compressed);
    void listenForConnections();
    void removeSocket(asio::ip::tcp::socket *socket);

    static void sendMsgHeader(std::shared_ptr<ntwk::TcpPublisher> publisher,
                              asio::ip::tcp::socket *socket,
                              std::shared_ptr<std_msgs::Header> msgHeader,
                              std::shared_ptr<flatbuffers::DetachedBuffer> msg,
                              unsigned int totalMsgHeaderBytesTransferred);

    static void sendMsg(std::shared_ptr<ntwk::TcpPublisher> publisher,
                        asio::ip::tcp::socket *socket,
                        std::shared_ptr<flatbuffers::DetachedBuffer> msg,
                        unsigned int totalMsgBytesTransferred);

    asio::io_context &ioContext;
    asio::ip::tcp::acceptor socketAcceptor;

    std::list<std::unique_ptr<asio::ip::tcp::socket>> connectedSockets;
    std::mutex socketsMutex;

    std::queue<std::shared_ptr<flatbuffers::DetachedBuffer>> msgQueue;
    std::queue<std::future<std::shared_ptr<flatbuffers::DetachedBuffer>>> compressedMsgQueue;
    unsigned int msgQueueSize;
    std::mutex msgQueueMutex;
    std::weak_ptr<flatbuffers::DetachedBuffer> msgBeingSent;

    bool compressed;
};

} // namespace ntwk
