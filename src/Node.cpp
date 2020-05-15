#include <network/Node.h>

#include <network/TcpPublisher.h>
#include <network/TcpSubscriber.h>

namespace ntwk {

Node::Node() : running(true) {
    // Start a new thread that updates publishers and subscribers
    this->updateFuture = std::async(std::launch::async, [this]{
        while (this->running.load()) {
            {
                std::lock_guard<std::mutex> guard(this->publishersMutex);
                for (auto &p : this->publishers) {
                    p->update();
                }
            }

            {
                std::lock_guard<std::mutex> guard(this->subscribersMutex);
                for (auto &s : this->subscribers) {
                    s->update();
                }
            }
        }
    });
}

Node::~Node() {
    this->running.store(false);
}

std::shared_ptr<TcpPublisher> Node::advertise(unsigned short port,
                                              unsigned int msgQueueSize,
                                              bool compressed) {
    auto p = TcpPublisher::create(this->ioContext, port, msgQueueSize, compressed);
    {
        std::lock_guard<std::mutex> guard(this->publishersMutex);
        this->publishers.emplace_front(p);
    }
    return p;
}

std::shared_ptr<TcpSubscriber> Node::subscribe(const std::string &host, unsigned short port,
                                               std::function<void (std::unique_ptr<uint8_t[]>)> msgReceivedHandler,
                                               unsigned int msgQueueSize, bool compressed) {
    auto s = TcpSubscriber::create(this->ioContext, host, port,
                                   std::move(msgReceivedHandler),
                                   msgQueueSize, compressed);
    {
        std::lock_guard<std::mutex> guard(this->subscribersMutex);
        this->subscribers.emplace_front(s);
    }
    return s;
}

void Node::update() {
    this->ioContext.poll();
    this->ioContext.restart();
}

} // namespace ntwk
