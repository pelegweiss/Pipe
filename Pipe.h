#pragma once
#pragma once
#include <Windows.h>
#include <iostream>
#include <codecvt>
#include <vector>
struct pipeMessage
{
    int id;           // ID field
    std::wstring data;
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