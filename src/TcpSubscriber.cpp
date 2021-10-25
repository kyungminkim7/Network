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
                                                     unsigned short port,
                                                     MsgReceivedHandler msgReceivedHandler) {
    std::shared_ptr<TcpSubscriber> subscriber(new TcpSubscriber(mainContext, subscriberContext,
                                                                host, port, std::move(msgReceivedHandler)));
    connect(subscriber);
    return subscriber;
}

TcpSubscriber::TcpSubscriber(asio::io_context &mainContext,
                             asio::io_context &subscriberContext,
                             const std::string &host,
                             unsigned short port,
                             MsgReceivedHandler msgReceivedHandler) :
    mainContext(mainContext), subscriberContext(subscriberContext),
    socket(subscriberContext), endpoint(make_address(host), port),
    msgReceivedHandler(std::move(msgReceivedHandler)) {}

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
            receiveMsgHeader(std::move(subscriber), std::make_unique<std_msgs::Header>(), 0u);
        }
    });
}

void TcpSubscriber::receiveMsgHeader(std::shared_ptr<TcpSubscriber> subscriber,
                                     std::unique_ptr<std_msgs::Header> msgHeader,
                                     unsigned int totalMsgHeaderBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsgHeader = reinterpret_cast<uint8_t*>(msgHeader.get());

    asio::async_read(pSubscriber->socket, asio::buffer(pMsgHeader + totalMsgHeaderBytesReceived,
                                                        sizeof(std_msgs::Header) - totalMsgHeaderBytesReceived),
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
        if (totalMsgHeaderBytesReceived < sizeof(std_msgs::Header)) {
            receiveMsgHeader(std::move(subscriber), std::move(msgHeader), totalMsgHeaderBytesReceived);
            return;
        }

        // Start receiving the msg
        receiveMsg(std::move(subscriber), std::make_unique<uint8_t[]>(msgHeader->msgSize()),
                   msgHeader->msgSize(), 0u);
    });
}

void TcpSubscriber::receiveMsg(std::shared_ptr<TcpSubscriber> subscriber,
                               std::unique_ptr<uint8_t[]> msg, unsigned int msgSize_bytes,
                               unsigned int totalMsgBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsg = msg.get();

    asio::async_read(pSubscriber->socket, asio::buffer(pMsg + totalMsgBytesReceived,
                                                       msgSize_bytes - totalMsgBytesReceived),
                     [subscriber=std::move(subscriber), msg=std::move(msg),
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
            receiveMsg(std::move(subscriber), std::move(msg), msgSize_bytes, totalMsgBytesReceived);
            return;
        }

        processMsg(subscriber, std::move(msg));

        // Acknowledge msg reception
        sendMsgControl(std::move(subscriber),
                       std::make_unique<std_msgs::MessageControl>(std_msgs::MessageControl::ACK),
                       0u);
    });
}

void TcpSubscriber::processMsg(std::shared_ptr<TcpSubscriber> subscriber,
                               std::unique_ptr<uint8_t[]> msgBuffer) {
    // Enqueue msg for handling
    std::lock_guard<std::mutex> guard(subscriber->msgQueueMutex);

    subscriber->msgQueue.push(std::move(msgBuffer));

    if (subscriber->msgQueue.size() > MSG_QUEUE_SIZE) {
        subscriber->msgQueue.pop();
    } else {
        postMsgHandlingTask(std::move(subscriber));
    }
}

void TcpSubscriber::postMsgHandlingTask(std::shared_ptr<TcpSubscriber> subscriber) {
    auto pSubscriber = subscriber.get();

    asio::post(pSubscriber->mainContext, [pSubscriber, subscriber=std::move(subscriber)]() mutable {
        std::unique_ptr<uint8_t[]> msg;

        // Grab next msg for handling
        {
            std::lock_guard<std::mutex> guard(subscriber->msgQueueMutex);
            msg = std::move(subscriber->msgQueue.front());
            subscriber->msgQueue.pop();
        }

        pSubscriber->msgReceivedHandler(std::move(msg));
    });
}

void TcpSubscriber::sendMsgControl(std::shared_ptr<TcpSubscriber> subscriber,
                                   std::unique_ptr<std_msgs::MessageControl> msgCtrl,
                                   unsigned int totalMsgCtrlBytesTransferred) {
    auto pSubscriber = subscriber.get();
    auto pMsgCtrl = reinterpret_cast<const uint8_t*>(msgCtrl.get());

    asio::async_write(pSubscriber->socket, asio::buffer(pMsgCtrl + totalMsgCtrlBytesTransferred,
                                                        sizeof(std_msgs::MessageControl) - totalMsgCtrlBytesTransferred),
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
        if (totalMsgCtrlBytesTransferred < sizeof(std_msgs::MessageControl)) {
            sendMsgControl(std::move(subscriber), std::move(msgCtrl), totalMsgCtrlBytesTransferred);
            return;
        }

        // Start listening for new msgs
        receiveMsgHeader(std::move(subscriber), std::make_unique<std_msgs::Header>(), 0u);
    });
}

} // namespace ntwk
