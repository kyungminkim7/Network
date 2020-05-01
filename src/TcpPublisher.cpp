#include <network/TcpPublisher.h>
#include <iostream>

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpPublisher> TcpPublisher::create(asio::io_context &ioContext, unsigned short port) {
    std::shared_ptr<TcpPublisher> publisher(new TcpPublisher(ioContext, port));
    publisher->listenForConnections();
    return publisher;
}

TcpPublisher::TcpPublisher(asio::io_context &ioContext, unsigned short port) :
    ioContext(ioContext),
    socketAcceptor(ioContext, tcp::endpoint(tcp::v4(), port)) {
}

void TcpPublisher::listenForConnections() {
    auto socket = std::make_shared<tcp::socket>(this->ioContext);
    auto pSocket = socket.get();
    this->socketAcceptor.async_accept(*pSocket,
                                      [publisher=shared_from_this(), socket=std::move(socket)](const auto &error) {
        if (error) {
            throw asio::system_error(error);
        }
        std::cout << "Connection established\n";

        publisher->connectedSockets.emplace_back(std::move(socket));
        publisher->listenForConnections();
    });
}

} // namespace ntwk
