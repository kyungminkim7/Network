#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include "MsgTypeId.h"
#include "msgs/Header_generated.h"
#include "msgs/MsgCtrl_generated.h"

namespace ntwk {

class TcpSubscriber {
private:
    using MsgTypeIdUnderlyingType = std::underlying_type_t<MsgTypeId>;
    using MsgPtr = std::unique_ptr<uint8_t[]>;
    using MsgBufferMap = std::unordered_map<MsgTypeIdUnderlyingType, MsgPtr>;

    using MsgHandler = std::function<void(MsgPtr)>;
    using MsgHandlerMap = std::unordered_map<MsgTypeIdUnderlyingType, MsgHandler>;

public:
    static std::shared_ptr<TcpSubscriber> create(asio::io_context &mainContext,
                                                 asio::io_context &subscriberContext,
                                                 const std::string &host, unsigned short port);

    void subscribe(MsgTypeId msgTypeId, MsgHandler msgHandler);

private:
    TcpSubscriber(asio::io_context &mainContext,
                  asio::io_context &subscriberContext,
                  const std::string &host, unsigned short port);

    static void connect(std::shared_ptr<TcpSubscriber> subscriber);

    static void receiveMsgHeader(std::shared_ptr<TcpSubscriber> subscriber,
                                 std::unique_ptr<msgs::Header> msgHeader,
                                 unsigned int totalMsgHeaderBytesReceived);

    static void receiveMsg(std::shared_ptr<TcpSubscriber> subscriber,
                           MsgTypeIdUnderlyingType msgTypeId, MsgPtr msg,
                           unsigned int msgSize_bytes,
                           unsigned int totalMsgBytesReceived);

    static void postMsgHandlingTask(std::shared_ptr<TcpSubscriber> subscriber,
                                    MsgTypeIdUnderlyingType msgTypeId);

    static void sendMsgControl(std::shared_ptr<TcpSubscriber> subscriber,
                               std::unique_ptr<msgs::MsgCtrl> msgCtrl,
                               unsigned int totalMsgCtrlBytesTransferred);

private:
    asio::io_context &mainContext;
    asio::io_context &subscriberContext;

    asio::ip::tcp::socket socket;
    std::unique_ptr<asio::steady_timer> socketReconnectTimer;
    asio::ip::tcp::endpoint endpoint;

    MsgHandlerMap msgHandlers;
    MsgBufferMap msgBuffers;
};

} // namespace ntwk
