#pragma once

#include <list>
#include <memory>
#include <mutex>

#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>

namespace ntwk {

class TcpPublisher : public std::enable_shared_from_this<TcpPublisher> {
public:
    static std::shared_ptr<TcpPublisher> create(asio::io_context &ioContext, unsigned short port);

    void publish(std::shared_ptr<uint8_t[]> msg, std::size_t msgSize_bytes);

//    template<typename T>
//    void publish(std::shared_ptr<T> msg);

private:
    TcpPublisher(asio::io_context &ioContext, unsigned short port);
    void listenForConnections();
    void removeSocket(asio::ip::tcp::socket *socket);

    asio::io_context &ioContext;
    asio::ip::tcp::acceptor socketAcceptor;

    std::list<std::unique_ptr<asio::ip::tcp::socket>> connectedSockets;
    std::mutex socketsMutex;
};

} // namespace ntwk

//#include "TcpPublisher_impl.h"
