#include <atomic>
#include <functional>
#include <thread>

#include "TcpConnection.h"
#include "ConcurrentQueue.h"
#include "Log.h"

class Cmd {
public:
    enum Type : std::uint8_t {
        CmdSwitchOn = 0x12,
        CmdSwitchOff = 0x13,
        CmdSetColor = 0x20,
        CmdSetBrightness = 0x21
    };

    explicit Cmd(const Type &type) noexcept:
            cmdType(type) {}

    virtual ~Cmd() = default;

    const Type cmdType;
};

class CmdColor : public Cmd {
public:
    struct Rgb {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;

        friend std::ostream &operator<<(std::ostream &os, const Rgb &rgb) {
            return os << "(" << static_cast<int>(rgb.r)
                      << "," << static_cast<int>(rgb.g)
                      << "," << static_cast<int>(rgb.b) << ")";
        }
    };

    explicit CmdColor(const Rgb &color_) :
            Cmd(Type::CmdSetColor),
            color(color_) {}

    static Rgb createPayload(const std::vector<std::uint8_t> &data) {
        return {data[0], data[1], data[2]};
    }

    const Rgb color;
};

class CmdBrightness : public Cmd {
public:
    explicit CmdBrightness(const std::uint8_t level_) :
            Cmd(Type::CmdSetBrightness),
            level(level_) {}

    static std::uint8_t createPayload(const std::vector<std::uint8_t> &data) {
        return data[0];
    }

    const std::uint8_t level;
};

//[T,L,V]
//[std::uint8_t, std::uint16_t, [std::uint8_t,]]
class TlvCmdParser final {
public:
    TlvCmdParser() = default;

    TlvCmdParser(const TlvCmdParser &) = delete;

    TlvCmdParser &operator=(const TlvCmdParser &) = delete;

    void parseTo(std::vector<std::uint8_t> data,
                 const std::shared_ptr<ConcurrentQueue<std::shared_ptr<Cmd>>> &commands) {
        if (!mUncompleted.empty()) {
            mUncompleted.insert(std::end(mUncompleted),
                                std::begin(data),
                                std::end(data));
            data = std::move(mUncompleted);
            mUncompleted = {};
        }
        auto it = data.cbegin();
        while (it != std::cend(data)) {
            std::shared_ptr<Cmd> cmd;
            bool isKnownCmd = true;

            switch (*it) {
                case Cmd::Type::CmdSwitchOn:
                    cmd = std::make_shared<Cmd>(Cmd::Type::CmdSwitchOn);
                    break;
                case Cmd::Type::CmdSwitchOff:
                    cmd = std::make_shared<Cmd>(Cmd::Type::CmdSwitchOff);
                    break;
                case Cmd::Type::CmdSetColor:
                    cmd = parseCmd<CmdColor>(it,
                                             std::distance(it, std::cend(data)),
                                             [](auto payload) {
                                                 return std::make_shared<CmdColor>(CmdColor::createPayload(payload));
                                             });
                    break;
                case Cmd::Type::CmdSetBrightness:
                    cmd = parseCmd<CmdBrightness>(it,
                                                  std::distance(it, std::cend(data)),
                                                  [](auto payload) {
                                                      return std::make_shared<CmdBrightness>(CmdBrightness::createPayload(payload));
                                                  });
                    break;
                default:
                    isKnownCmd = false;
                    break;
            }

            if (cmd) {
                commands->push(std::move(cmd));
            } else if (isKnownCmd) {
                mUncompleted.insert(std::end(mUncompleted),
                                    it,
                                    std::cend(data));
                break;
            }
            it++;
        }
    }

private:
    static const std::size_t BYTES_OF_TYPE = sizeof(Cmd::Type);
    static const std::size_t BYTES_OF_LENGTH = sizeof(std::uint16_t);
    static const std::size_t BYTES_OF_HEADER = BYTES_OF_LENGTH + BYTES_OF_TYPE;

    template<typename CmdType>
    std::shared_ptr<CmdType> parseCmd(std::vector<std::uint8_t>::const_iterator &it,
                                      const std::int64_t leftBytes,
                                      std::function<std::shared_ptr<CmdType>(
                                              const std::vector<std::uint8_t> &payload)> factory) {
        if (auto payload = extractPayload(it, leftBytes); payload.has_value()) {
            return factory(*payload);
        }
        return {};
    }

    static std::optional<std::vector<std::uint8_t>>
    extractPayload(std::vector<std::uint8_t>::const_iterator &it, const std::int64_t leftBytes) {
        if (leftBytes > static_cast<std::int64_t>(BYTES_OF_HEADER)) {
            std::array<std::uint8_t, BYTES_OF_LENGTH> lengthData = {};
            std::copy_n(it + BYTES_OF_TYPE, lengthData.size(), std::begin(lengthData));
            // from big-endian to little-endian
            std::reverse(std::begin(lengthData), std::end(lengthData));
            const auto payloadLength = *reinterpret_cast<std::uint16_t *>(lengthData.data());
            const auto leftBytesForPayload = leftBytes - BYTES_OF_HEADER;
            if (leftBytesForPayload >= payloadLength) {
                it += BYTES_OF_HEADER;
                std::vector<std::uint8_t> payload(it, it + payloadLength);
                it += payloadLength - 1;
                // from big-endian to little-endian
                std::reverse(std::begin(payload), std::end(payload));
                return payload;
            }
        }
        return {};
    }

    std::vector<std::uint8_t> mUncompleted;
};

int main(int argc, char *argv[]) {
    std::string hostName = "localhost";
    if (argc > 1) {
        hostName = argv[1];
    }
    std::size_t port = 9999;
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }

    try {
        std::vector<std::thread> threads;
        // connection
        auto connQueue = std::make_shared<ConcurrentQueue<std::vector<std::uint8_t>>>();
        std::exception_ptr mConnException;
        std::atomic<bool> isConnectionFinished = false;
        threads.emplace_back([hostName, port, connQueue, &mConnException, &isConnectionFinished]() {
            Log() << "start connection thread" << std::endl;
            TcpConnection connection;
            try {
                connection.receiveTo(hostName, port, connQueue);
            } catch (...) {
                mConnException = std::current_exception();
            }
            isConnectionFinished.store(true, std::memory_order_relaxed);
            connQueue->interruptWaiting();
            Log() << "finish connection thread" << std::endl;
        });
        // parsing
        std::atomic<bool> isParserFinished = false;
        auto cmdQueue = std::make_shared<ConcurrentQueue<std::shared_ptr<Cmd>>>();
        threads.emplace_back([&isConnectionFinished, &isParserFinished, connQueue, cmdQueue]() {
            Log() << "start cmd parser" << std::endl;
            TlvCmdParser parser;
            while (!isConnectionFinished.load(std::memory_order_relaxed) || !connQueue->isEmpty()) {
                if (const auto result = connQueue->waitAndPop(); result.has_value()) {
                    parser.parseTo(*result, cmdQueue);
                }
            }
            isParserFinished.store(true, std::memory_order_relaxed);
            cmdQueue->interruptWaiting();
            Log() << "finish cmd parser" << std::endl;
        });
        // consuming
        threads.emplace_back([&isParserFinished, cmdQueue] {
            Log() << "start flashlight thread" << std::endl;
            while (!isParserFinished.load(std::memory_order_relaxed) || !cmdQueue->isEmpty()) {
                if (const auto result = cmdQueue->waitAndPop(); result.has_value()) {
                    const auto &cmd = *result;
                    switch (cmd->cmdType) {
                        case Cmd::Type::CmdSwitchOn:
                            Log() << "cmd: switch on" << std::endl;
                            break;
                        case Cmd::Type::CmdSwitchOff:
                            Log() << "cmd: switch off" << std::endl;
                            break;
                        case Cmd::Type::CmdSetColor:
                            if (auto cmdColor = dynamic_cast<CmdColor *>(cmd.get()); cmdColor != nullptr) {
                                Log() << "cmd: set color=" << cmdColor->color << std::endl;
                            }
                            break;
                        case Cmd::Type::CmdSetBrightness:
                            if (auto cmdBrightness = dynamic_cast<CmdBrightness *>(cmd.get());
                                    cmdBrightness != nullptr) {
                                Log() << "cmd: set brightness=" << static_cast<int>(cmdBrightness->level) << std::endl;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
            Log() << "finish flashlight thread" << std::endl;
        });

        for (auto &thread : threads) {
            thread.join();
        }
        if (mConnException != nullptr) {
            std::rethrow_exception(mConnException);
        }
    } catch (const std::exception &e) {
        Log() << "error: " << e.what() << std::endl;
    }
    return 0;
}
