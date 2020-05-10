#include <network/Node.h>

#include <network/TcpPublisher.h>
#include <network/TcpSubscriber.h>

namespace ntwk {

std::shared_ptr<TcpPublisher> Node::advertise(unsigned short port, unsigned int msgQueueSize) {
    this->publishers.emplace_back(TcpPublisher::create(this->ioContext, port, msgQueueSize));
    return this->publishers.back();
}

std::shared_ptr<TcpSubscriber> Node::subscribe(const std::string &host, unsigned short port,
                                               std::function<void (std::unique_ptr<uint8_t[]>)> msgReceivedHandler) {
    return TcpSubscriber::create(this->ioContext, host, port, std::move(msgReceivedHandler));
}

void Node::update() {
    for (auto &p : this->publishers) {
        p->update();
    }

    this->ioContext.poll();
    this->ioContext.restart();
}

} // namespace ntwk
