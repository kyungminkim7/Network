#include <network/Node.h>

#include <network/TcpPublisher.h>
#include <network/TcpSubscriber.h>

namespace ntwk {

std::shared_ptr<TcpPublisher> Node::advertise(unsigned short port,
                                              unsigned int msgQueueSize,
                                              bool compressed) {
    this->publishers.emplace_front(TcpPublisher::create(this->ioContext, port,
                                                        msgQueueSize, compressed));
    return this->publishers.front();
}

std::shared_ptr<TcpSubscriber> Node::subscribe(const std::string &host, unsigned short port,
                                               std::function<void (std::unique_ptr<uint8_t[]>)> msgReceivedHandler,
                                               unsigned int msgQueueSize, bool compressed) {
    this->subscribers.emplace_front(TcpSubscriber::create(this->ioContext, host, port,
                                                          std::move(msgReceivedHandler),
                                                          msgQueueSize, compressed));
    return this->subscribers.front();
}

void Node::update() {
    for (auto &p : this->publishers) {
        p->update();
    }

    for (auto &s : this->subscribers) {
        s->update();
    }

    this->ioContext.poll();
    this->ioContext.restart();
}

} // namespace ntwk
