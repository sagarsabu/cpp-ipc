#include <set>
#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <thread>
#include <chrono>

#include "ipc/connection_manager.hpp"
#include "ipc/messages.hpp"
#include "log/logger.hpp"

using namespace Sage;
using namespace std::chrono_literals;

int ChildEntry(void* /** int* */ childIdPtr)
{
    int res{ 0 };
    int childId{ *static_cast<int*>(childIdPtr) };

    try
    {
        Log::Info("child id:%d pid:%d has spawned", childId, getpid());
        IPC::ChildConnection connection{ childId };
        connection.Connect();

        for (uint idx = 0; idx < 5; idx++)
        {
            IPC::MsgBuffer buffer{ connection.Read() };
            IPC::TestMessage* testMsg{ reinterpret_cast<IPC::TestMessage*>(buffer.data()) };
            Log::Info("child id:%d. Id: %d Rx: %s. sleeping for a bit.", childId, testMsg->id, testMsg->data);
            std::this_thread::sleep_for(1s);
        }

        Log::Info("child id:%d pid:%d has finished working", childId, getpid());
    }
    catch (const std::exception& e)
    {
        Log::Error("child id:%d err:%s", childId, e.what());
        res = 1;
    }

    return res;
}

pid_t CreateChild(int childId)
{
    pid_t pid{ fork() };
    if (pid == -1)
    {
        throw std::runtime_error("Failed to create child " + std::to_string(childId) + ". err: " + strerror(errno));
    }

    if (pid == 0)
    {
        // The child process
        std::exit(ChildEntry(&childId));
    }

    // The parent process
    Log::Info("created child id:%d pid:%d", childId, pid);
    return pid;
}

void WaitForChildrenExit(std::set<pid_t> childrenPids)
{
    do
    {
        int status;
        std::set<pid_t> completedChildren{};

        for (auto pid : childrenPids)
        {
            Log::Debug("waiting for child pid:%d", pid);

            pid_t childPid{ waitpid(pid, &status, WNOHANG) };
            if (childPid == -1)
            {
                Log::Error("failed to wait on child pid:%d err:%s", pid, strerror(errno));
            }
            else if (childPid == 0)
            {
                Log::Debug("child pid:%d still running", pid);
            }
            else if (childPid == pid)
            {
                Log::Info("child pid:%d exited. status:%d", childPid, WEXITSTATUS(status));
                completedChildren.insert(pid);
            }
            else
            {
                Log::Error("failed to wait on child pid:%d. unexpected return code:%d", pid, childPid);
            }
        }

        for (auto completedChild : completedChildren)
        {
            childrenPids.erase(completedChild);
        }

    } while (not childrenPids.empty());
}

void DispatchWork(IPC::ConnectionManager& mgr)
{
    for (uint idx = 0; idx < 5; idx++)
    {
        IPC::TestMessage testMsg;
        testMsg.id = idx;
        std::string msg{ "IPC::Message Test Data Idx:" + std::to_string(idx) };
        std::copy(msg.begin(), msg.end(), testMsg.data);

        Log::Debug("dispatching Tx: %s", testMsg.data);
        mgr.BroadCast(reinterpret_cast<uint8_t*>(&testMsg), sizeof(IPC::TestMessage));
    }
}

int main(int, const char**)
{
    Logger::SetLogLevel(Logger::Info);

    Log::Info("starting");

    std::set<pid_t> childrenPids{};
    IPC::ConnectionManager connectionManager;
    connectionManager.CreateSocket();

    for (int idx = 0; idx < 6; idx++)
    {
        pid_t childPid{ CreateChild(idx) };
        connectionManager.AcceptConnection(idx);
        childrenPids.insert(childPid);
    }

    DispatchWork(connectionManager);
    WaitForChildrenExit(childrenPids);

    return 0;
}

