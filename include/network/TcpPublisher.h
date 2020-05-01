#pragma once

#include <memory>
#include <vector>

#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>

namespace ntwk {

class TcpPublisher : public std::enable_shared_from_this<TcpPublisher> {
public:
    static std::shared_ptr<TcpPublisher> create(asio::io_context &ioContext, unsigned short port);

private:
    TcpPublisher(asio::io_context &ioContext, unsigned short port);
    void listenForConnections();

    asio::io_context &ioContext;
    asio::ip::tcp::acceptor socketAcceptor;
    std::vector<std::shared_ptr<asio::ip::tcp::socket>> connectedSockets;
};

} // namespace ntwk
