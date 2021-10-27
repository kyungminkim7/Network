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

void Node::advertise(unsigned short port) {
    this->publisher = TcpPublisher::create(this->tasksContext, port);
}

void Node::subscribe(const std::string &host, unsigned short port,
                     std::function<void(std::unique_ptr<uint8_t[]>)> msgReceivedHandler) {
    auto endpoint = std::make_pair(host, port);
    auto &s = this->subscribers[endpoint];
    if (!s) {
        s = TcpSubscriber::create(this->mainContext, this->tasksContext,
                                  host, port, std::move(msgReceivedHandler));
    } else {
        // TODO subscribe
    }
}

void Node::publish(std::shared_ptr<flatbuffers::DetachedBuffer> msg) {
    this->publisher->publish(std::move(msg));
}

void Node::run() {
    auto work = asio::make_work_guard(this->mainContext);
    this->mainContext.run();
}

void Node::runOnce() {
    this->mainContext.poll();
    this->mainContext.restart();
}

} // namespace ntwk
