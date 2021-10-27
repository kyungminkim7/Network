#include <network/TcpPublisher.h>

#include <asio/read.hpp>
#include <asio/write.hpp>

namespace ntwk {

using namespace asio::ip;

struct TcpPublisher::Socket {
    std::unique_ptr<asio::ip::tcp::socket> socket;
    bool readyToWrite;

    explicit Socket(std::unique_ptr<asio::ip::tcp::socket> socket) :
        socket(std::move(socket)), readyToWrite(true){}
};

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

void TcpPublisher::removeSocket(Socket *socket) {
    this->connectedSockets.remove_if([socket](const auto &s){
        return s.socket.get() == socket->socket.get();
    });
}

void TcpPublisher::publish(MsgTypeId msgTypeId, std::shared_ptr<flatbuffers::DetachedBuffer> msg) {
    asio::post(this->publisherContext, [publisher=this->shared_from_this(),
               msgTypeId, msg=std::move(msg)]() mutable {
        auto msgHeader = std::make_shared<msgs::Header>(msgTypeId, msg->size());

        for (auto &s : publisher->connectedSockets) {
            if (s.readyToWrite) {
                s.readyToWrite = false;
                publisher->sendMsgHeader(publisher, &s, msgHeader, msg, 0u);
            }
        }
    });
}

void TcpPublisher::sendMsgHeader(std::shared_ptr<ntwk::TcpPublisher> publisher, Socket *socket,
                                 std::shared_ptr<const msgs::Header> msgHeader,
                                 std::shared_ptr<const flatbuffers::DetachedBuffer> msg,
                                 unsigned int totalMsgHeaderBytesTransferred) {
    // Publish msg header
    auto pMsgHeader = reinterpret_cast<const uint8_t*>(msgHeader.get());
    asio::async_write(*socket->socket, asio::buffer(pMsgHeader + totalMsgHeaderBytesTransferred,
                                                    sizeof(msgs::Header) - totalMsgHeaderBytesTransferred),
                      [publisher=std::move(publisher), socket,
                      msgHeader=std::move(msgHeader), msg=std::move(msg),
                      totalMsgHeaderBytesTransferred](const auto &error, auto bytesTransferred) mutable {
        // Tear down socket if fatal error
        if (error) {
            publisher->removeSocket(socket);
            return;
        }

        // Send the rest of the header if it was only partially sent
        totalMsgHeaderBytesTransferred += bytesTransferred;
        if (totalMsgHeaderBytesTransferred < sizeof(msgs::Header)) {
            sendMsgHeader(std::move(publisher), socket, std::move(msgHeader), std::move(msg), totalMsgHeaderBytesTransferred);
            return;
        }

        // Send the msg
        sendMsg(std::move(publisher), socket, std::move(msg), 0u);
    });
}

void TcpPublisher::sendMsg(std::shared_ptr<ntwk::TcpPublisher> publisher, Socket *socket,
                           std::shared_ptr<const flatbuffers::DetachedBuffer> msg,
                           unsigned int totalMsgBytesTransferred) {
    auto pMsg = msg.get();
    asio::async_write(*socket->socket, asio::buffer(pMsg->data() + totalMsgBytesTransferred,
                                                    pMsg->size() - totalMsgBytesTransferred),
                      [publisher=std::move(publisher), socket, msg=std::move(msg),
                      totalMsgBytesTransferred](const auto &error, auto bytesTransferred) mutable {
        // Tear down socket if fatal error
        if (error) {
            publisher->removeSocket(socket);
            return;
        }

        // Send the rest of the msg if it was only partially sent
        totalMsgBytesTransferred += bytesTransferred;
        if (totalMsgBytesTransferred < msg->size()) {
            sendMsg(std::move(publisher), socket, std::move(msg), totalMsgBytesTransferred);
            return;
        }

        // Wait for ack signal from subscriber
        receiveMsgControl(std::move(publisher), socket,
                          std::make_unique<msgs::MessageControl>(), 0u);
    });
}

void TcpPublisher::receiveMsgControl(std::shared_ptr<TcpPublisher> publisher, Socket *socket,
                                     std::unique_ptr<msgs::MessageControl> msgCtrl,
                                     unsigned int totalMsgCtrlBytesReceived) {
    auto pMsgCtrl = reinterpret_cast<uint8_t*>(msgCtrl.get());
    asio::async_read(*socket->socket, asio::buffer(pMsgCtrl + totalMsgCtrlBytesReceived,
                                                   sizeof(msgs::MessageControl) - totalMsgCtrlBytesReceived),
                     [publisher=std::move(publisher), socket, msgCtrl=std::move(msgCtrl),
                     totalMsgCtrlBytesReceived](const auto &error, auto bytesReceived) mutable {
        // Tear down socket if fatal error
        if (error) {
            publisher->removeSocket(socket);
            return;
        }

        // Receive the rest of the msg ctrl if it was only partially received
        totalMsgCtrlBytesReceived += bytesReceived;
        if (totalMsgCtrlBytesReceived < sizeof(msgs::MessageControl)) {
            receiveMsgControl(std::move(publisher), socket,
                              std::move(msgCtrl), totalMsgCtrlBytesReceived);
            return;
        }

        if (*msgCtrl != msgs::MessageControl::ACK) {
            // Reset the connection if a successful Ack signal is not received
            publisher->removeSocket(socket);
        } else {
            socket->readyToWrite = true;
        }
    });
}

} // namespace ntwk
