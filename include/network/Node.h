#pragma once

#include <asio/io_context.hpp>

namespace ntwk {

class Node {
private:
    asio::io_context ioContext;
};

} // namespace ntwk
