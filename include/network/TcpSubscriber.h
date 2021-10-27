#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <std_msgs/Header_generated.h>
#include <std_msgs/MessageControl_generated.h>

namespace ntwk {

class TcpSubscriber {
public:
    using MsgReceivedHandler = std::function<void(std::unique_ptr<uint8_t[]>)>;
    using MsgPtr = std::unique_ptr<uint8_t[]>;

    static std::shared_ptr<TcpSubscriber> create(asio::io_context &mainContext,
                                                 asio::io_context &subscriberContext,
                                                 const std::string &host, unsigned short port,
                                                 MsgReceivedHandler msgReceivedHandler);

private:
    TcpSubscriber(asio::io_context &mainContext,
                  asio::io_context &subscriberContext,
                  const std::string &host, unsigned short port,
                  MsgReceivedHandler msgReceivedHandler);

    static void connect(std::shared_ptr<TcpSubscriber> subscriber);

    static void receiveMsgHeader(std::shared_ptr<TcpSubscriber> subscriber,
                                 std::unique_ptr<std_msgs::Header> msgHeader,
                                 unsigned int totalMsgHeaderBytesReceived);

    static void receiveMsg(std::shared_ptr<TcpSubscriber> subscriber,
                           MsgPtr msg, unsigned int msgSize_bytes,
                           unsigned int totalMsgBytesReceived);

    static void postMsgHandlingTask(std::shared_ptr<TcpSubscriber> subscriber);

    static void sendMsgControl(std::shared_ptr<TcpSubscriber> subscriber,
                               std::unique_ptr<std_msgs::MessageControl> msgCtrl,
                               unsigned int totalMsgCtrlBytesTransferred);

private:
    asio::io_context &mainContext;
    asio::io_context &subscriberContext;

    asio::ip::tcp::socket socket;
    std::unique_ptr<asio::steady_timer> socketReconnectTimer;
    asio::ip::tcp::endpoint endpoint;

    MsgReceivedHandler msgReceivedHandler;
    MsgPtr receivedMsg;
};

} // namespace ntwk
