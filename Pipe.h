#pragma once
#include <Windows.h>
#include <iostream>
#include <codecvt>
#include <vector>
enum messagesTypes
{
    bHeader = 0, outGoingPacket, inGoingPacket
};
enum encodingTypes
{
    encode1 = 1, encode2 = 2, encode4 = 4, encodeStr = 5, encodeBuffer = 6, decode1 = 7, decode2 = 8, decode4 = 9, decodeStr = 10, decodeBuffer = 11
};
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
struct Header
{
    int action;
    WORD header;
};
struct pipeMessage
{
    int id;
    void* data;
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
    pipeMessage readPipeMessage();
    bool sendPacketMessage(const pipeMessage& message);
    bool sendBlockHeaderMessage(const pipeMessage& message);
};