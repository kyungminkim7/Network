#include <network/TcpPublisher.h>

#include <asio/write.hpp>
#include <std_msgs/Compressed_generated.h>
#include <zlib/zlib.h>

namespace {

std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(std::shared_ptr<flatbuffers::DetachedBuffer> msg) {
    // Initialize compression
    z_stream zStream;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;
    auto result = deflateInit(&zStream, Z_DEFAULT_COMPRESSION);
    if (result != Z_OK) {
        return nullptr;
    }

    // Compress msg
    zStream.avail_in = msg->size();
    zStream.next_in = msg->data();

    const auto numAvailableBytes = deflateBound(&zStream, msg->size());
    auto compressedBytes = std::make_unique<uint8_t[]>(numAvailableBytes);
    zStream.avail_out = numAvailableBytes;
    zStream.next_out = compressedBytes.get();

    if (deflate(&zStream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&zStream);
        return nullptr;
    }

    const auto numCompressedBytes = numAvailableBytes - zStream.avail_out;

    // Release resources no longer needed
    if (deflateEnd(&zStream)!= Z_OK) {
        return nullptr;
    }

    msg = nullptr;

    // Build compressedMsg
    flatbuffers::FlatBufferBuilder msgBuilder;
    auto compressedMsgData = msgBuilder.CreateVector(compressedBytes.get(), numCompressedBytes);
    auto compressedMsg = std_msgs::CreateCompressed(msgBuilder, compressedMsgData);
    msgBuilder.Finish(compressedMsg);

    return std::make_shared<flatbuffers::DetachedBuffer>(msgBuilder.Release());
}

} // namespace

namespace ntwk {

using namespace asio::ip;

std::shared_ptr<TcpPublisher> TcpPublisher::create(asio::io_context &ioContext, unsigned short port,
                                                   unsigned int msgQueueSize, bool compressed) {
    std::shared_ptr<TcpPublisher> publisher(new TcpPublisher(ioContext, port, msgQueueSize, compressed));
    publisher->listenForConnections();
    return publisher;
}

TcpPublisher::TcpPublisher(asio::io_context &ioContext, unsigned short port,
                           unsigned int msgQueueSize, bool compressed) :
    ioContext(ioContext),
    socketAcceptor(ioContext, tcp::endpoint(tcp::v4(), port)),
    msgQueueSize(msgQueueSize), compressed(compressed){ }

void TcpPublisher::listenForConnections() {
    auto socket = std::make_unique<tcp::socket>(this->ioContext);
    auto pSocket = socket.get();

    // Save connected sockets for later publishing and listen for more connections
    this->socketAcceptor.async_accept(*pSocket,
                                      [publisher=shared_from_this(), socket=std::move(socket)](const auto &error) mutable {
        if (error) {
            throw asio::system_error(error);
        }

        publisher->connectedSockets.emplace_back(std::move(socket));
        publisher->listenForConnections();
    });
}

void TcpPublisher::removeSocket(tcp::socket *socket) {
    for (auto iter = this->connectedSockets.cbegin(); iter != this->connectedSockets.cend(); ) {
        if (iter->get() == socket) {
            iter = this->connectedSockets.erase(iter);
            return;
        } else {
            ++iter;
        }
    }
}

void TcpPublisher::publish(std::shared_ptr<flatbuffers::DetachedBuffer> msg) {
    this->msgQueue.emplace(this->compressed ? compressMsg(std::move(msg)) : std::move(msg));
    while (this->msgQueue.size() > this->msgQueueSize) {
        this->msgQueue.pop();
    }
}

void TcpPublisher::update() {
    // Get next msg to send from the msgQueue
    auto msg = this->msgBeingSent.lock();
    if (msg || this->msgQueue.empty()) {
        return;
    }

    msg = std::move(this->msgQueue.front());
    this->msgBeingSent = msg;
    this->msgQueue.pop();

    // Build and send the msg header followed by the actual msg itself
    auto msgHeader = std::make_shared<std_msgs::Header>(msg->size());
    for (auto &socket : this->connectedSockets) {
        sendMsgHeader(shared_from_this(), socket.get(), msgHeader, msg, 0u);
    }
}

void TcpPublisher::sendMsgHeader(std::shared_ptr<ntwk::TcpPublisher> publisher,
                                 asio::ip::tcp::socket *socket,
                                 std::shared_ptr<std_msgs::Header> msgHeader,
                                 std::shared_ptr<flatbuffers::DetachedBuffer> msg,
                                 unsigned int totalMsgHeaderBytesTransferred) {
    // Publish msg header
    auto pMsgHeader = reinterpret_cast<uint8_t*>(msgHeader.get());
    asio::async_write(*socket, asio::buffer(pMsgHeader + totalMsgHeaderBytesTransferred,
                                            sizeof(std_msgs::Header) - totalMsgHeaderBytesTransferred),
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
        if (totalMsgHeaderBytesTransferred < sizeof(std_msgs::Header)) {
            sendMsgHeader(std::move(publisher), socket, std::move(msgHeader), std::move(msg), totalMsgHeaderBytesTransferred);
            return;
        }

        // Send the msg
        sendMsg(std::move(publisher), socket, std::move(msg), 0u);
    });
}

void TcpPublisher::sendMsg(std::shared_ptr<ntwk::TcpPublisher> publisher,
                           asio::ip::tcp::socket *socket,
                           std::shared_ptr<flatbuffers::DetachedBuffer> msg,
                           unsigned int totalMsgBytesTransferred) {
    auto pMsg = msg.get();
    asio::async_write(*socket, asio::buffer(pMsg->data() + totalMsgBytesTransferred,
                                            pMsg->size() - totalMsgBytesTransferred),
                      [publisher=std::move(publisher), socket,
                      msg=std::move(msg), totalMsgBytesTransferred](const auto &error, auto bytesTransferred) mutable {
        // Tear down socket if fatal error
        if (error) {
            publisher->removeSocket(socket);
            return;
        }

        // Send the rest of the msg if it was only partially sent
        totalMsgBytesTransferred += bytesTransferred;
        if (totalMsgBytesTransferred < msg->size()) {
            sendMsg(std::move(publisher), socket, std::move(msg), totalMsgBytesTransferred);
        }
    });
}

} // namespace ntwk
