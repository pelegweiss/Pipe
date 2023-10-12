#pragma once
#include <Windows.h>
#include <iostream>
#include <codecvt>
#include <vector>
struct Segment
{
    int type;
    std::vector<BYTE> bytes;
};
struct Packet
{
    DWORD callerAddress{};
    WORD header{};
    std::vector<Segment> segments;

};
struct pipeMessage
{
    int id;           // ID field
    Packet data;
};
class Pipe
{
public:
    std::wstring pipeName;
    HANDLE hNamedPipe;

    Pipe(std::wstring name);
    bool createPipe();
    bool waitForClient();
    bool connectPipe();
    pipeMessage readMessage();
    bool sendMessage(const pipeMessage& message);

};