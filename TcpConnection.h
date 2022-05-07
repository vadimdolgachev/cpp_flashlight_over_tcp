#include <string>
#include <stdexcept>
#include <array>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>

#include "ConcurrentQueue.h"
#include "Log.h"

#ifndef FLASHLIGHT_TCP_CONNECTION_H
#define FLASHLIGHT_TCP_CONNECTION_H

class TcpConnection final {
public:
    TcpConnection() = default;

    TcpConnection(const TcpConnection &) = delete;

    TcpConnection &operator=(const TcpConnection &) = delete;

    ~TcpConnection() {
        if (mSockFd != -1) {
            close(mSockFd);
        }
    }

    void receiveTo(const std::string &hostName,
                   uint16_t port,
                   const std::shared_ptr<ConcurrentQueue<std::vector<std::uint8_t>>> &queue) {
        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        if (const auto he = gethostbyname(hostName.c_str()); he != nullptr) {
            if (const auto addr = reinterpret_cast<in_addr *>(he->h_addr); addr != nullptr) {
                serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr));
            }
        }
        if (serverAddr.sin_addr.s_addr == 0) {
            throw std::runtime_error("error getting host name");
        }
        serverAddr.sin_port = htons(port);

        if (mSockFd = socket(AF_INET, SOCK_STREAM, 0); mSockFd != -1) {
            if (connect(mSockFd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1) {
                throw std::runtime_error(strerror(errno));
            }
            ssize_t length = 0;
            while ((length = recv(mSockFd, mBuffer.data(), mBuffer.size(), MSG_WAITALL)) && length > 0) {
                queue->push({std::begin(mBuffer), std::begin(mBuffer) + length});
            }
        } else {
            throw std::runtime_error(strerror(errno));
        }
    }

private:
    int mSockFd = -1;
    std::array<std::uint8_t, 4> mBuffer = {};
};

#endif // FLASHLIGHT_TCP_CONNECTION_H