#pragma once

#include <forward_list>
#include <list>
#include <memory>
#include <mutex>

#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>

namespace ntwk {

class TcpPublisher : public std::enable_shared_from_this<TcpPublisher> {
public:
    static std::shared_ptr<TcpPublisher> create(asio::io_context &ioContext, unsigned short port);

    void publish();

private:
    TcpPublisher(asio::io_context &ioContext, unsigned short port);
    void listenForConnections();
    void removeSocket(asio::ip::tcp::socket *socket);

    asio::io_context &ioContext;
    asio::ip::tcp::acceptor socketAcceptor;

    std::list<std::shared_ptr<asio::ip::tcp::socket>> connectedSockets;
    std::mutex socketsMutex;

    // References to this TcpPublisher per connection in order to keep it alive
    // until the last connection is broken
    std::forward_list<std::shared_ptr<TcpPublisher>> publishers;
    std::mutex publishersMutex;
};

} // namespace ntwk
