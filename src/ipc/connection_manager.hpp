#pragma once

#include <array>
#include <unordered_map>
#include <cstdint>

namespace Sage
{

namespace IPC
{

constexpr size_t IPC_BUFFER_SIZE{ 1024 }; // 1KB
using MsgBuffer = std::array<uint8_t, IPC_BUFFER_SIZE>;

struct Connection
{
    uint id{ 0 };
    int connectionFd{ -1 };
};

class ConnectionManager
{
public:
    ConnectionManager() = default;

    virtual ~ConnectionManager();

    void CreateSocket();

    void AcceptConnection(uint id);

    void BroadCast(const uint8_t* data, size_t size);

    void SendToChild(uint id, const uint8_t* data, size_t size);

    void Read();

private:
    int m_socketFd{ -1 };
    std::unordered_map<uint, Connection> m_connections{};

};

class ChildConnection
{
public:
    explicit ChildConnection(int id) :
        m_id{ id }
    { }

    virtual ~ChildConnection();

    void Connect();

    void SendToMaster();

    MsgBuffer Read();

private:
    const int m_id;
    int m_ipcSocketFd{ -1 };
};

} // namespace IPC

} // namespace Sage


