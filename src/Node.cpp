#include <network/Node.h>

#include <network/TcpPublisher.h>
#include <network/TcpSubscriber.h>

namespace ntwk {

Node::Node() : mainContext(), tasksContext(),
    tasksThread([this]{
        auto work = asio::make_work_guard(this->tasksContext);
        this->tasksContext.run();
    }) { }

Node::~Node() {
    this->tasksContext.stop();
    this->mainContext.stop();

    this->tasksThread.join();
}

std::shared_ptr<TcpPublisher> Node::advertise(unsigned short port) {
    return TcpPublisher::create(this->tasksContext, port);
}

std::shared_ptr<TcpSubscriber> Node::subscribe(const std::string &host, unsigned short port,
                                               std::function<void(std::unique_ptr<uint8_t[]>)> msgReceivedHandler) {
    return TcpSubscriber::create(this->mainContext, this->tasksContext,
                                 host, port, std::move(msgReceivedHandler));
}

void Node::run() {
    auto work = asio::make_work_guard(this->mainContext);
    this->mainContext.run();
}

void Node::runOnce() {
    this->mainContext.poll_one();
    this->mainContext.restart();
}

} // namespace ntwk
