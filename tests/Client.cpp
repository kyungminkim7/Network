#include <iostream>

#include <asio.hpp>

#include <geometry_msgs/Vector3_generated.h>
#include <std_msgs/Header_generated.h>
#include <network/TcpSubscriber.h>

void onVector3Received(std::unique_ptr<uint8_t[]> vec3Buffer) {
    auto vec3 = geometry_msgs::GetVector3(vec3Buffer.get());
    std::cout << "(" << vec3->x() << ", " << vec3->y() << ", " << vec3->z() << ")\n";
}

int main(int argc, char *argv[]) {
    asio::io_context ioContext;

    auto vec3Subscriber = ntwk::TcpSubscriber::create(ioContext, "127.0.0.1", 4444, &onVector3Received);
    auto headerSubscriber = ntwk::TcpSubscriber::create(ioContext, "127.0.0.1", 5555, [](auto headerBuffer){
        auto header = reinterpret_cast<std_msgs::Header*>(headerBuffer.get());
        std::cout << "Header: " << header->msgSize() << "\n";
    });


    ioContext.run();

    return 0;
}
