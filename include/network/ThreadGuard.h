#pragma once

#include <thread>

namespace ntwk {

class ThreadGuard {
public:
    explicit ThreadGuard(std::thread &&t);
    ~ThreadGuard();

    ThreadGuard(ThreadGuard &&other) noexcept = default;
    ThreadGuard &operator=(ThreadGuard &&other) noexcept = default;

private:
    std::thread t;
};

} // namespace ntwk
