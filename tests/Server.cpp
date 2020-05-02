#include <iostream>

#include <asio.hpp>
#include <network/TcpPublisher.h>

int main(int argc, char *argv[]) {
    asio::io_context ioContext;

    auto publisher = ntwk::TcpPublisher::create(ioContext, 4444);

    int x;

    while (true) {
        std::cout << "enter a number: ";
        std::cin >> x;

        publisher->publish();

        ioContext.poll();
        ioContext.restart();
    }

    return 0;
}
