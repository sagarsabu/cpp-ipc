#pragma once

#include <cstdint>

namespace Sage
{

namespace IPC
{

enum Message
{
    Invalid = 0,
    Test
};

struct TestMessage
{
    Message message{ Message::Test };
    uint id{ 0 };
    char data[1024]{ 0 };
};

} // namespace IPC

} // namespace Sage



