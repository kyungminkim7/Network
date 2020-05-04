#include <network/TcpPublisher.h>

#include <forward_list>
#include <unordered_map>

#include <asio/write.hpp>

#include <std_msgs/Header.h>

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpPublisher> TcpPublisher::create(asio::io_context &ioContext, unsigned short port) {
    std::shared_ptr<TcpPublisher> publisher(new TcpPublisher(ioContext, port));
    publisher->listenForConnections();
    return publisher;
}

TcpPublisher::TcpPublisher(asio::io_context &ioContext, unsigned short port) :
    ioContext(ioContext),
    socketAcceptor(ioContext, tcp::endpoint(tcp::v4(), port)) { }

void TcpPublisher::listenForConnections() {
    auto socket = std::make_shared<tcp::socket>(this->ioContext);
    auto pSocket = socket.get();

    // Save connected sockets for later publishing and listen for more connections
    this->socketAcceptor.async_accept(*pSocket,
                                      [publisher=shared_from_this(), socket=std::move(socket)](const auto &error) {
        if (error) {
            throw asio::system_error(error);
        }

        {
            std::lock_guard<std::mutex> guard(publisher->socketsMutex);
            publisher->connectedSockets.emplace_back(std::move(socket));
        }

        publisher->listenForConnections();
    });
}

void TcpPublisher::removeSocket(tcp::socket *socket) {
    std::lock_guard<std::mutex> guard(this->socketsMutex);
    for (auto iter = this->connectedSockets.cbegin(); iter != this->connectedSockets.cend(); ) {
        if (iter->get() == socket) {
            iter = this->connectedSockets.erase(iter);
            return;
        } else {
            ++iter;
        }
    }
}

void TcpPublisher::publish() {
    auto msg = std::make_shared<std::string>("Hello all");
    auto msgHeader = std::make_shared<std_msgs::Header>(msg->length());

    for (auto &socket : this->connectedSockets) {
        asio::async_write(*socket, asio::buffer(msgHeader.get(), sizeof(std_msgs::Header)),
                          [publisher=shared_from_this(), socket, msgHeader, msg](const auto &error, auto bytesTransferred){
            // Remove sockets that have errored out
            if (error) {
                publisher->removeSocket(socket.get());
                return;
            }
        });
    }
}

} // namespace ntwk
