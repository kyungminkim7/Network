#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <queue>

#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>
#include <flatbuffers/flatbuffers.h>

namespace ntwk {

class TcpPublisher : public std::enable_shared_from_this<TcpPublisher> {
public:
    static std::shared_ptr<TcpPublisher> create(asio::io_context &ioContext, unsigned short port, unsigned int msgQueueSize);

    void publish(std::shared_ptr<flatbuffers::DetachedBuffer> msg);

    void update();

private:
    TcpPublisher(asio::io_context &ioContext, unsigned short port, unsigned int msgQueueSize);
    void listenForConnections();
    void removeSocket(asio::ip::tcp::socket *socket);

    asio::io_context &ioContext;
    asio::ip::tcp::acceptor socketAcceptor;

    std::list<std::unique_ptr<asio::ip::tcp::socket>> connectedSockets;
    std::mutex socketsMutex;

    std::queue<std::shared_ptr<flatbuffers::DetachedBuffer>> msgQueue;
    unsigned int msgQueueSize;
    std::mutex msgQueueMutex;
};

} // namespace ntwk
