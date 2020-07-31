#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

#include <asio/io_context.hpp>

#include "Compression.h"
#include "Image.h"
#include "TcpPublisher.h"
#include "TcpSubscriber.h"

namespace ntwk {

class Node {
public:
    Node();
    ~Node();

    template<typename CompressionStrategy=decltype(compression::none::compressMsg)>
    std::shared_ptr<TcpPublisher<CompressionStrategy>> advertise(unsigned short port,
                                                                 CompressionStrategy compressionStrategy=compression::none::compressMsg);

    template<typename CompressionStrategy=decltype(compression::image::none::compressMsg)>
    std::shared_ptr<TcpPublisher<CompressionStrategy>> advertiseImage(unsigned short port,
                                                                      CompressionStrategy compressionStrategy=compression::image::none::compressMsg);

    template<typename DecompressionStrategy=decltype(compression::none::decompressMsg)>
    std::shared_ptr<TcpSubscriber<uint8_t[], DecompressionStrategy>> subscribe(const std::string &host, unsigned short port,
                                                                               std::function<void(std::unique_ptr<uint8_t[]>)> msgReceivedHandler,
                                                                               DecompressionStrategy decompressionStrategy=compression::none::decompressMsg);

    template<typename DecompressionStrategy=decltype(compression::image::none::decompressMsg)>
    std::shared_ptr<TcpSubscriber<Image, DecompressionStrategy>> subscribeImage(const std::string &host, unsigned short port,
                                                                                std::function<void(std::unique_ptr<Image>)> imgMsgReceivedHandler,
                                                                                DecompressionStrategy decompressionStrategy=compression::image::none::decompressMsg);

    void run();
    void runOnce();

private:
    asio::io_context mainContext;
    asio::io_context tasksContext;

    std::thread tasksThread;
};

template<typename CompressionStrategy>
std::shared_ptr<TcpPublisher<CompressionStrategy>> Node::advertise(unsigned short port, CompressionStrategy compressionStrategy) {
    return TcpPublisher<CompressionStrategy>::create(this->tasksContext, port, std::move(compressionStrategy));
}

template<typename CompressionStrategy>
std::shared_ptr<TcpPublisher<CompressionStrategy>> Node::advertiseImage(unsigned short port, CompressionStrategy compressionStrategy) {
    return TcpPublisher<CompressionStrategy>::create(this->tasksContext, port, std::move(compressionStrategy));
}

template<typename DecompressionStrategy>
std::shared_ptr<TcpSubscriber<uint8_t[], DecompressionStrategy>> Node::subscribe(const std::string &host, unsigned short port,
                                                                                 std::function<void (std::unique_ptr<uint8_t[]>)> msgReceivedHandler,
                                                                                 DecompressionStrategy decompressionStrategy) {
    return TcpSubscriber<uint8_t[], DecompressionStrategy>::create(this->mainContext, this->tasksContext,
                                                                   host, port,
                                                                   std::move(msgReceivedHandler),
                                                                   std::move(decompressionStrategy));
}

template<typename DecompressionStrategy>
std::shared_ptr<TcpSubscriber<Image, DecompressionStrategy>> Node::subscribeImage(const std::string &host, unsigned short port,
                                                                                  std::function<void (std::unique_ptr<Image>)> imgMsgReceivedHandler,
                                                                                  DecompressionStrategy decompressionStrategy) {
    return TcpSubscriber<Image, DecompressionStrategy>::create(this->mainContext, this->tasksContext,
                                                               host, port, std::move(imgMsgReceivedHandler),
                                                               std::move(decompressionStrategy));
}

} // namespace ntwk
