#include <iostream>
#include <memory>

#include <asio.hpp>

#include <geometry_msgs/Vector3_generated.h>
#include <std_msgs/Header_generated.h>
#include <network/TcpPublisher.h>

int main(int argc, char *argv[]) {
    asio::io_context ioContext;

    auto vec3Publisher = ntwk::TcpPublisher::create(ioContext, 4444);
    auto headerPublisher = ntwk::TcpPublisher::create(ioContext, 5555);

    int x;

    while (x != 0) {
        std::cout << "enter a number: ";
        std::cin >> x;

        flatbuffers::FlatBufferBuilder msgBuilder(64);
        auto vec = geometry_msgs::CreateVector3(msgBuilder, 1.1f, 2.2f, 3.3f);
        msgBuilder.Finish(vec);

        vec3Publisher->publish(std::make_shared<flatbuffers::DetachedBuffer>(msgBuilder.Release()));
        headerPublisher->publish(std::make_shared<std_msgs::Header>(5849));

        ioContext.poll();
        ioContext.restart();
    }

    return 0;
}
