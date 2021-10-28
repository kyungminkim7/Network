#include <network/TcpSubscriber.h>

#include <chrono>

#include <asio/read.hpp>
#include <asio/write.hpp>

namespace {

constexpr auto SOCKET_RECONNECT_WAIT_DURATION = std::chrono::milliseconds(30);

} // namespace

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpSubscriber> TcpSubscriber::create(asio::io_context &mainContext,
                                                     asio::io_context &subscriberContext,
                                                     const std::string &host,
                                                     unsigned short port) {
    std::shared_ptr<TcpSubscriber> subscriber(new TcpSubscriber(mainContext, subscriberContext,
                                                                host, port));
    connect(subscriber);
    return subscriber;
}

TcpSubscriber::TcpSubscriber(asio::io_context &mainContext, asio::io_context &subscriberContext,
                             const std::string &host, unsigned short port) :
    mainContext(mainContext), subscriberContext(subscriberContext),
    socket(subscriberContext), endpoint(make_address(host), port) {}

void TcpSubscriber::subscribe(MsgTypeId msgTypeId, MsgHandler msgHandler) {
    this->msgHandlers[msgTypeId] = std::move(msgHandler);
}

void TcpSubscriber::connect(std::shared_ptr<TcpSubscriber> subscriber) {
    auto pSubscriber = subscriber.get();

    pSubscriber->socket.async_connect(pSubscriber->endpoint, [pSubscriber, subscriber=std::move(subscriber)](const auto &error) mutable {
        if (error) {
            subscriber->socket.close();

            subscriber->socketReconnectTimer = std::make_unique<asio::steady_timer>(subscriber->subscriberContext,
                                                                                    SOCKET_RECONNECT_WAIT_DURATION);
            pSubscriber->socketReconnectTimer->async_wait([pSubscriber, subscriber=std::move(subscriber)](const auto &error) mutable {
                connect(std::move(subscriber));
            });

        } else {
            // Start receiving messages
            receiveMsgHeader(std::move(subscriber), std::make_unique<msgs::Header>(), 0u);
        }
    });
}

void TcpSubscriber::receiveMsgHeader(std::shared_ptr<TcpSubscriber> subscriber,
                                     std::unique_ptr<msgs::Header> msgHeader,
                                     unsigned int totalMsgHeaderBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsgHeader = reinterpret_cast<uint8_t*>(msgHeader.get());

    asio::async_read(pSubscriber->socket, asio::buffer(pMsgHeader + totalMsgHeaderBytesReceived,
                                                        sizeof(msgs::Header) - totalMsgHeaderBytesReceived),
                     [subscriber=std::move(subscriber), msgHeader=std::move(msgHeader),
                     totalMsgHeaderBytesReceived](const auto &error, auto bytesReceived) mutable {
        // Try reconnecting upon fatal error
        if (error) {
            subscriber->socket.close();
            connect(std::move(subscriber));
            return;
        }

        // Receive the rest of the header if it was only partially received
        totalMsgHeaderBytesReceived += bytesReceived;
        if (totalMsgHeaderBytesReceived < sizeof(msgs::Header)) {
            receiveMsgHeader(std::move(subscriber), std::move(msgHeader), totalMsgHeaderBytesReceived);
            return;
        }

        // Start receiving the msg
        receiveMsg(std::move(subscriber),
                   msgHeader->msg_type_id(), std::make_unique<uint8_t[]>(msgHeader->msg_size()),
                   msgHeader->msg_size(), 0u);
    });
}

void TcpSubscriber::receiveMsg(std::shared_ptr<TcpSubscriber> subscriber,
                               MsgTypeId msgTypeId, MsgPtr msg, unsigned int msgSize_bytes,
                               unsigned int totalMsgBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsg = msg.get();

    asio::async_read(pSubscriber->socket, asio::buffer(pMsg + totalMsgBytesReceived,
                                                       msgSize_bytes - totalMsgBytesReceived),
                     [subscriber=std::move(subscriber), msgTypeId, msg=std::move(msg),
                     msgSize_bytes, totalMsgBytesReceived](const auto &error, auto bytesReceived) mutable {
        // Try reconnecting upon fatal error
        if (error) {
            subscriber->socket.close();
            connect(std::move(subscriber));
            return;
        }

        // Receive the rest of the msg if it was only partially received
        totalMsgBytesReceived += bytesReceived;
        if (totalMsgBytesReceived < msgSize_bytes) {
            receiveMsg(std::move(subscriber),
                       msgTypeId, std::move(msg),
                       msgSize_bytes, totalMsgBytesReceived);
            return;
        }

        // Enqueue msg for handling (only process latest msg)
        if (!subscriber->msgBuffers[msgTypeId]) {
            asio::post(subscriber->mainContext, [subscriber, msgTypeId]() mutable {
                postMsgHandlingTask(std::move(subscriber), msgTypeId);
            });
        }
        subscriber->msgBuffers[msgTypeId] = std::move(msg);

        // Acknowledge msg reception
        sendMsgControl(std::move(subscriber),
                       std::make_unique<msgs::MsgCtrl>(msgs::MsgCtrl::ACK),
                       0u);
    });
}

void TcpSubscriber::postMsgHandlingTask(std::shared_ptr<TcpSubscriber> subscriber,
                                        MsgTypeId msgTypeId) {
    auto pSubscriber = subscriber.get();
    asio::post(pSubscriber->subscriberContext,
               [pSubscriber, subscriber=std::move(subscriber), msgTypeId]() mutable {
        asio::post(pSubscriber->mainContext,
                   [subscriber=std::move(subscriber), msgTypeId,
                   msg=std::move(pSubscriber->msgBuffers[msgTypeId])]() mutable {
            auto handler = subscriber->msgHandlers.find(msgTypeId);
            if (handler != subscriber->msgHandlers.end()) {
                handler->second(std::move(msg));
            }
        });
    });
}

void TcpSubscriber::sendMsgControl(std::shared_ptr<TcpSubscriber> subscriber,
                                   std::unique_ptr<msgs::MsgCtrl> msgCtrl,
                                   unsigned int totalMsgCtrlBytesTransferred) {
    auto pSubscriber = subscriber.get();
    auto pMsgCtrl = reinterpret_cast<const uint8_t*>(msgCtrl.get());

    asio::async_write(pSubscriber->socket, asio::buffer(pMsgCtrl + totalMsgCtrlBytesTransferred,
                                                        sizeof(msgs::MsgCtrl) - totalMsgCtrlBytesTransferred),
                      [subscriber=std::move(subscriber), msgCtrl=std::move(msgCtrl),
                      totalMsgCtrlBytesTransferred](const auto &error, auto bytesTransferred) mutable {
        // Close down socket and try reconnecting upon fatal error
        if (error) {
            subscriber->socket.close();
            connect(std::move(subscriber));
            return;
        }

        // Send the rest of the msg if it was only partially received
        totalMsgCtrlBytesTransferred += bytesTransferred;
        if (totalMsgCtrlBytesTransferred < sizeof(msgs::MsgCtrl)) {
            sendMsgControl(std::move(subscriber), std::move(msgCtrl), totalMsgCtrlBytesTransferred);
            return;
        }

        // Start listening for new msgs
        receiveMsgHeader(std::move(subscriber), std::make_unique<msgs::Header>(), 0u);
    });
}

} // namespace ntwk
