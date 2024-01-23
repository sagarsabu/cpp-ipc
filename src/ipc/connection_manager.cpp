#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

#include "ipc/connection_manager.hpp"
#include "log/logger.hpp"

namespace Sage
{

namespace IPC
{

// Starts with null byte to create abstract unix socket as per UNIX man
constexpr char IPC_SOCKET_NAME[]{ "\0cpp-ipc.socket" };

constexpr sockaddr_un GetUnixSocketAddress()
{
    sockaddr_un socketAddress{};
    socketAddress.sun_family = AF_UNIX;
    std::copy(std::begin(IPC_SOCKET_NAME), std::end(IPC_SOCKET_NAME), socketAddress.sun_path);

    return socketAddress;
}

// ConnectionManager

ConnectionManager::~ConnectionManager()
{
    for (auto& [id, conn] : m_connections)
    {
        close(conn.connectionFd);
    }

    close(m_socketFd);
}

void ConnectionManager::CreateSocket()
{
    sockaddr_un socketAddress{ GetUnixSocketAddress() };

    int socketFd{ socket(AF_UNIX, SOCK_SEQPACKET, 0) };
    if (socketFd == -1)
    {
        throw std::runtime_error(std::string("Failed to create to socket. err ") + strerror(errno));
    }

    if (bind(socketFd, reinterpret_cast<sockaddr*>(&socketAddress), sizeof(socketAddress)) == -1)
    {
        throw std::runtime_error(std::string("Failed to bind to socket. err ") + strerror(errno));
    }

    if (listen(socketFd, 100) == -1)
    {
        throw std::runtime_error(std::string("Failed to listen on socket. err ") + strerror(errno));
    }

    m_socketFd = socketFd;
}

void ConnectionManager::AcceptConnection(uint id)
{
    if (m_connections.find(id) != m_connections.end())
    {
        throw std::runtime_error("Failed to accept connection on socket for child id: " + std::to_string(id) + ". Already in use");
    }

    int connectionFd{ accept(m_socketFd, nullptr, nullptr) };
    if (connectionFd == -1)
    {
        throw std::runtime_error("Failed to accept connection on socket for child id: " + std::to_string(id) + ". err " + strerror(errno));
    }

    m_connections[id] = { .id = id, .connectionFd = connectionFd };
}

void ConnectionManager::BroadCast(const uint8_t* data, size_t size)
{
    for (auto& con : m_connections)
    {
        SendToChild(con.first, data, size);
    }
}

void ConnectionManager::SendToChild(uint id, const uint8_t* data, size_t size)
{
    auto& con{ m_connections.at(id) };
    ssize_t nBytesSent{ send(con.connectionFd, data, size, 0) };
    if (nBytesSent == -1)
    {
        throw std::runtime_error("Failed to send data to child id: " + std::to_string(id) + ". err " + strerror(errno));
    }

    Log::Debug("sent %ld bytes to child id:%d", nBytesSent, id);
}

void ConnectionManager::Read()
{ }

// ChildConnection

ChildConnection::~ChildConnection()
{
    close(m_ipcSocketFd);
}

void ChildConnection::Connect()
{
    sockaddr_un ipcSocketAddress{ GetUnixSocketAddress() };

    int ipcSocketFd{ socket(AF_UNIX, SOCK_SEQPACKET, 0) };
    if (ipcSocketFd == -1)
    {
        throw std::runtime_error("Failed to create to ipc socket for id " + std::to_string(m_id) + " .err " + strerror(errno));
    }

    if (connect(ipcSocketFd, reinterpret_cast<sockaddr*>(&ipcSocketAddress), sizeof(ipcSocketAddress)) == -1)
    {
        throw std::runtime_error("Failed to connect to ipc socket for id " + std::to_string(m_id) + " .err " + strerror(errno));
    }

    m_ipcSocketFd = ipcSocketFd;
}

void ChildConnection::SendToMaster()
{ }

MsgBuffer ChildConnection::Read()
{
    MsgBuffer buffer;
    ssize_t nBytesRead{ read(m_ipcSocketFd, buffer.data(), buffer.size()) };
    if (nBytesRead == -1)
    {
        throw std::runtime_error("Failed to read from ipc socket for id " + std::to_string(m_id) + " .err " + strerror(errno));
    }

    return buffer;
}

} // namespace IPC

} // namespace Sage


