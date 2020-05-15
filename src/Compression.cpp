#include <network/Compression.h>

#include <std_msgs/Compressed_generated.h>
#include <std_msgs/Header_generated.h>
#include <zlib/zlib.h>

namespace ntwk {

namespace zlib {

std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(std::shared_ptr<flatbuffers::DetachedBuffer> msg) {
    // Initialize compression
    z_stream zStream;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;

    if (deflateInit(&zStream, Z_DEFAULT_COMPRESSION) != Z_OK) {
        return nullptr;
    }

    // Compress msg
    zStream.avail_in = msg->size();
    zStream.next_in = msg->data();

    const auto numAvailableBytes = deflateBound(&zStream, msg->size());
    auto compressedBytes = std::make_unique<uint8_t[]>(numAvailableBytes);
    zStream.avail_out = numAvailableBytes;
    zStream.next_out = compressedBytes.get();

    if (deflate(&zStream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&zStream);
        return nullptr;
    }

    const auto numCompressedBytes = numAvailableBytes - zStream.avail_out;

    // Release resources no longer needed
    if (deflateEnd(&zStream)!= Z_OK) {
        return nullptr;
    }

    const auto uncompressedMsgSize = msg->size();
    msg = nullptr;

    // Build compressedMsg
    flatbuffers::FlatBufferBuilder msgBuilder;
    auto compressedMsgData = msgBuilder.CreateVector(compressedBytes.get(), numCompressedBytes);
    auto compressedMsg = std_msgs::CreateCompressed(msgBuilder,
                                                    uncompressedMsgSize,
                                                    compressedMsgData);
    msgBuilder.Finish(compressedMsg);

    return std::make_shared<flatbuffers::DetachedBuffer>(msgBuilder.Release());
}

std::unique_ptr<uint8_t[]> decompressMsg(std::unique_ptr<uint8_t[]> compressedMsgBuffer) {
    // Initialize decompression
    auto compressedMsg = std_msgs::GetMutableCompressed(compressedMsgBuffer.get());

    z_stream zStream;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;
    zStream.avail_in = compressedMsg->compressedData()->size();
    zStream.next_in = compressedMsg->mutable_compressedData()->data();

    if (inflateInit(&zStream) != Z_OK) {
        return nullptr;
    }

    // Decompress msg
    auto msg = std::make_unique<uint8_t[]>(compressedMsg->uncompressedDataSize());
    zStream.avail_out = compressedMsg->uncompressedDataSize();
    zStream.next_out = msg.get();

    auto result = inflate(&zStream, Z_FINISH);
    inflateEnd(&zStream);
    return result == Z_STREAM_END ? std::move(msg) : nullptr;
}

} // namespace zlib

namespace jpeg {

} // namespace jpeg

} // namespace ntwk
