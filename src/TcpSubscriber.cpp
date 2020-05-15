#include <network/TcpSubscriber.h>

#include <asio/read.hpp>
#include <std_msgs/Compressed_generated.h>
#include <zlib/zlib.h>

namespace {

std::unique_ptr<uint8_t[]> decompressMsg(std::unique_ptr<uint8_t[]> compressedMsgBuffer) {
    // Initialize decompression
    auto compressedMsg = std_msgs::GetMutableCompressed(compressedMsgBuffer.get());

    z_stream zStream;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;
    zStream.avail_in = compressedMsg->compressedData()->size();
    zStream.next_in = compressedMsg->mutable_compressedData()->data();

    if (inflateInit(&zStream) != Z_OK) {
        return nullptr;
    }

    // Decompress msg
    auto msg = std::make_unique<uint8_t[]>(compressedMsg->uncompressedDataSize());
    zStream.avail_out = compressedMsg->uncompressedDataSize();
    zStream.next_out = msg.get();

    auto result = inflate(&zStream, Z_FINISH);
    inflateEnd(&zStream);
    return result == Z_STREAM_END ? std::move(msg) : nullptr;
}

} // namespace

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpSubscriber> TcpSubscriber::create(asio::io_context &ioContext,
                                                     const std::string &host, unsigned short port,
                                                     MessageReceivedHandler msgReceivedHandler,
                                                     unsigned int msgQueueSize, bool compressed) {
    std::shared_ptr<TcpSubscriber> subscriber(new TcpSubscriber(ioContext, host, port,
                                                                std::move(msgReceivedHandler),
                                                                msgQueueSize, compressed));
    connect(subscriber);
    return subscriber;
}

TcpSubscriber::TcpSubscriber(asio::io_context &ioContext,
                             const std::string &host, unsigned short port,
                             MessageReceivedHandler msgReceivedHandler,
                             unsigned int msgQueueSize, bool compressed) :
    socket(ioContext), endpoint(make_address(host), port),
    msgReceivedHandler(std::move(msgReceivedHandler)),
    msgQueueSize(msgQueueSize), compressed(compressed) {}

void TcpSubscriber::connect(std::shared_ptr<TcpSubscriber> subscriber) {
    auto pSubscriber = subscriber.get();

    std::lock_guard<std::mutex> guard(subscriber->socketMutex);
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

    if (this->compressed) {
        msg = decompressMsg(std::move(msg));

        if (msg == nullptr) {
            {
                std::lock_guard<std::mutex> guard(this->socketMutex);
                this->socket.close();
            }
            connect(shared_from_this());
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
        // Try reconnecting upon fatal error
        if (error) {
            {
                std::lock_guard<std::mutex> guard(subscriber->socketMutex);
                subscriber->socket.close();
            }
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
                               std::unique_ptr<uint8_t[]> msg,
                               unsigned int msgSize_bytes, unsigned int totalMsgBytesReceived) {
    auto pSubscriber = subscriber.get();
    auto pMsg = msg.get();

    std::lock_guard<std::mutex> guard(subscriber->socketMutex);
    asio::async_read(pSubscriber->socket, asio::buffer(pMsg + totalMsgBytesReceived,
                                                       msgSize_bytes - totalMsgBytesReceived),
                     [subscriber=std::move(subscriber), msg=std::move(msg),
                     msgSize_bytes, totalMsgBytesReceived](const auto &error, auto bytesReceived) mutable {
        // Try reconnecting upon fatal error
        if (error) {
            {
                std::lock_guard<std::mutex> guard(subscriber->socketMutex);
                subscriber->socket.close();
            }
            connect(std::move(subscriber));
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
