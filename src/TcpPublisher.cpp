#include <network/TcpPublisher.h>

#include <system_error>

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <network/Utils.h>
#include <network/msgs/Header_generated.h>
#include <network/msgs/MsgCtrl_generated.h>

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpPublisher> TcpPublisher::create(asio::io_context &publisherContext,
                                                   unsigned short port) {
    std::shared_ptr<TcpPublisher> publisher(new TcpPublisher(publisherContext, port));
    publisher->listenForConnections();
    return publisher;
}

TcpPublisher::TcpPublisher(asio::io_context &publisherContext, unsigned short port) :
    publisherContext(publisherContext),
    socketAcceptor(publisherContext, tcp::endpoint(tcp::v4(), port)) { }

void TcpPublisher::listenForConnections() {
    auto socket = std::make_unique<tcp::socket>(this->publisherContext);
    auto pSocket = socket.get();

    // Save connected sockets for later publishing and listen for more connections
    this->socketAcceptor.async_accept(*pSocket,
                                      [publisher=this->shared_from_this(),
                                       socket=std::move(socket)](const auto &error) mutable {
        if (!error) {
            publisher->connectedSockets.emplace_back(std::move(socket));
        }
        publisher->listenForConnections();
    });
}

void TcpPublisher::publish(MsgTypeId msgTypeId,
                           std::shared_ptr<flatbuffers::DetachedBuffer> msg) {
    asio::post(this->publisherContext,
               [publisher=this->shared_from_this(), msgTypeId, msg=std::move(msg)]() mutable {
        msgs::Header header(toUnderlyingType(msgTypeId), msg->size());
        msgs::MsgCtrl msgCtrl;

        for (auto iter = publisher->connectedSockets.begin();
             iter != publisher->connectedSockets.end();) {
            try {
                auto &socket = *iter;
                asio::write(*socket, asio::buffer(&header, sizeof(msgs::Header)));
                asio::write(*socket, asio::buffer(msg->data(), msg->size()));

                asio::read(*socket, asio::buffer(&msgCtrl, sizeof(msgs::MsgCtrl)));
                if (msgCtrl != msgs::MsgCtrl::ACK) {
                    throw std::system_error(std::make_error_code(std::io_errc::stream));
                }
            } catch (...) {
                iter = publisher->connectedSockets.erase(iter);
                continue;
            }
            ++iter;
        }
    });
}

} // namespace ntwk
