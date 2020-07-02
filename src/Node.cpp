#include <network/Node.h>

#include <network/TcpPublisher.h>
#include <network/TcpSubscriber.h>

namespace ntwk {

Node::Node(unsigned short fps) : mainContext(), tasksContext(), period(1.0f / static_cast<float>(fps)),
    tasksThread([this]{
        auto work = asio::make_work_guard(this->tasksContext);
        this->tasksContext.run();
    }) { }

Node::~Node() {
    this->tasksContext.stop();
    this->mainContext.stop();

    this->tasksThread.join();
}

std::shared_ptr<TcpPublisher> Node::advertise(unsigned short port, Compression compression) {
    auto p = TcpPublisher::create(this->tasksContext, port, compression);
    this->publishers.emplace_front(p);
    return p;
}

std::shared_ptr<TcpSubscriber> Node::subscribe(const std::string &host, unsigned short port,
                                               std::function<void (std::unique_ptr<uint8_t[]>)> msgReceivedHandler,
                                               Compression compression) {
    auto s = TcpSubscriber::create(this->mainContext, this->tasksContext, host, port,
                                   std::move(msgReceivedHandler), compression);
    this->subscribers.emplace_front(s);
    return s;
}

std::shared_ptr<TcpSubscriber> Node::subscribe(const std::string &host, unsigned short port,
                                               std::function<void (std::unique_ptr<Image>)> imgMsgReceivedHandler,
                                               Compression compression) {
    auto s = TcpSubscriber::create(this->mainContext, this->tasksContext, host, port,
                                   std::move(imgMsgReceivedHandler), compression);
    this->subscribers.emplace_front(s);
    return s;
}

void Node::run() {
    auto work = asio::make_work_guard(this->mainContext);
    this->mainContext.run();
}

void Node::runOnce() {
    this->mainContext.poll_one();
    this->mainContext.restart();
}

void Node::sleep() {
    if (this->lastUpdateTimeInitialized) {
        const auto currentTime = std::chrono::system_clock::now();
        const auto timeElapsed = currentTime - this->lastUpdateTime;
        if (timeElapsed < this->period) {
            std::this_thread::sleep_for(this->period - timeElapsed);
        }
        this->lastUpdateTime = currentTime;
    } else {
        this->lastUpdateTime = std::chrono::system_clock::now();
        this->lastUpdateTimeInitialized = true;
    }
}

} // namespace ntwk
