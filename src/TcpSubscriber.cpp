#include <network/TcpSubscriber.h>

#include <asio/read.hpp>

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpSubscriber> TcpSubscriber::create(asio::io_context &ioContext,
                                                     const std::string &host, unsigned short port,
                                                     MessageReceivedHandler msgReceivedHandler,
                                                     unsigned int msgQueueSize, Compression compression) {
    std::shared_ptr<TcpSubscriber> subscriber(new TcpSubscriber(ioContext, host, port,
                                                                std::move(msgReceivedHandler),
                                                                msgQueueSize, compression));
    {
        std::lock_guard<std::mutex> guard(subscriber->socketMutex);
        connect(subscriber);
    }
    return subscriber;
}

TcpSubscriber::TcpSubscriber(asio::io_context &ioContext,
                             const std::string &host, unsigned short port,
                             MessageReceivedHandler msgReceivedHandler,
                             unsigned int msgQueueSize, Compression compression) :
    socket(ioContext), endpoint(make_address(host), port),
    msgReceivedHandler(std::move(msgReceivedHandler)),
    msgQueueSize(msgQueueSize), compression(compression) {}

void TcpSubscriber::connect(std::shared_ptr<TcpSubscriber> subscriber) {
    auto pSubscriber = subscriber.get();

    pSubscriber->socket.async_connect(pSubscriber->endpoint, [subscriber=std::move(subscriber)](const auto &error) mutable {
        if (error) {
            std::lock_guard<std::mutex> guard(subscriber->socketMutex);
            subscriber->socket.close();
        } else {
            // Start receiving messages
            receiveMsgHeader(std::move(subscriber), std::make_unique<std_msgs::Header>(), 0u);
        }
    });
}

void TcpSubscriber::update() {
    // Attempt to connect to a socket
    {
        std::lock_guard<std::mutex> guard(this->socketMutex);
        if (!this->socket.is_open()) {
            connect(shared_from_this());
        }
    }

    std::unique_ptr<uint8_t[]> msg;

    // Process msgs in queue
    {
        std::lock_guard<std::mutex> guard(this->msgQueueMutex);
        if (this->msgQueue.empty()) {
            return;
        }

        msg = std::move(this->msgQueue.front());
        this->msgQueue.pop();
    }

    if (this->compression == Compression::ZLIB) {
        msg = zlib::decodeMsg(msg.get());

        if (msg == nullptr) {
            {
                // Close down socket and try reconnecting on the next update cycle upon fatal error
                std::lock_guard<std::mutex> guard(this->socketMutex);
                this->socket.close();
            }
            return;
        }
    }

    this->msgReceivedHandler(std::move(msg));
}

void TcpSubscriber::receiveMsgHeader(std::shared_ptr<TcpSubscriber> subscriber,
                                     std::unique_ptr<std_msgs::Header> msgHeader,
                                     unsigned int totalMsgHeaderBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsgHeader = reinterpret_cast<uint8_t*>(msgHeader.get());

    std::lock_guard<std::mutex> guard(subscriber->socketMutex);
    asio::async_read(pSubscriber->socket, asio::buffer(pMsgHeader + totalMsgHeaderBytesReceived,
                                                        sizeof(std_msgs::Header) - totalMsgHeaderBytesReceived),
                     [subscriber=std::move(subscriber), msgHeader=std::move(msgHeader),
                     totalMsgHeaderBytesReceived](const auto &error, auto bytesReceived) mutable {
        // Close down socket and try reconnecting on the next update cycle upon fatal error
        if (error) {
            {
                std::lock_guard<std::mutex> guard(subscriber->socketMutex);
                subscriber->socket.close();
            }
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
                               std::unique_ptr<uint8_t[]> msg,
                               unsigned int msgSize_bytes, unsigned int totalMsgBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsg = msg.get();

    std::lock_guard<std::mutex> guard(subscriber->socketMutex);
    asio::async_read(pSubscriber->socket, asio::buffer(pMsg + totalMsgBytesReceived,
                                                        msgSize_bytes - totalMsgBytesReceived),
                     [subscriber=std::move(subscriber), msg=std::move(msg),
                     msgSize_bytes, totalMsgBytesReceived](const auto &error, auto bytesReceived) mutable {
        // Close down socket and try reconnecting on the next update cycle upon fatal error
        if (error) {
            {
                std::lock_guard<std::mutex> guard(subscriber->socketMutex);
                subscriber->socket.close();
            }
            return;
        }

        // Receive the rest of the msg if it was only partially received
        totalMsgBytesReceived += bytesReceived;
        if (totalMsgBytesReceived < msgSize_bytes) {
            receiveMsg(std::move(subscriber), std::move(msg), msgSize_bytes, totalMsgBytesReceived);
            return;
        }

        // Queue the completed msg for handling
        {
            std::lock_guard<std::mutex> guard(subscriber->msgQueueMutex);
            subscriber->msgQueue.emplace(std::move(msg));
            while(subscriber->msgQueue.size() > subscriber->msgQueueSize) {
                subscriber->msgQueue.pop();
            }
        }

        // Start listening for new msgs
        receiveMsgHeader(std::move(subscriber), std::make_unique<std_msgs::Header>(), 0u);
    });
}

} // namespace ntwk
