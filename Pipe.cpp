// Pipe.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"
#include "Pipe.h"
Pipe::Pipe(std::wstring name)
{
    this->pipeName = L"\\\\.\\pipe\\" + name;
};
bool Pipe::createPipe()
{
    this->hNamedPipe = CreateNamedPipe(
        this->pipeName.c_str(),  // Pipe name
        PIPE_ACCESS_OUTBOUND,      // Outbound access
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
        1,                          // Max instances
        0,                          // Buffer size
        0,                          // Output buffer size
        0,                          // Default timeout
        NULL                        // Security attributes
    );

    if (hNamedPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create named pipe." << std::endl;
        return false;
    }
    return true;
}
bool Pipe::waitForClient()
{
    if (!ConnectNamedPipe(this->hNamedPipe, NULL)) {
        std::cerr << "Failed to connect to named pipe." << std::endl;
        CloseHandle(hNamedPipe);
        return false;
    }
    return true;
}
bool Pipe::connectPipe()
{
    this->hNamedPipe = CreateFile(
        pipeName.c_str(),  // Pipe name
        GENERIC_READ,               // Desired access (read-only)
        0,                          // Share mode (0 means no sharing)
        NULL,                       // Security attributes
        OPEN_EXISTING,              // Open an existing pipe
        0,                          // File attributes
        NULL                        // Template file
    );

    if (hNamedPipe == INVALID_HANDLE_VALUE) {
        std::wcout << "Failed to open pipe: " << this->pipeName << std::endl;
        return false;
    }
    std::wcout << "Connected Pipe: " << this->pipeName << std::endl;
    return true;
}
bool Pipe::sendPacketMessage(const pipeMessage& message)
{
    DWORD bytesWritten;

    // Serialize the message into a byte vector
    std::vector<BYTE> serializedData;
    const Packet* p;
    // Serialize the id
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.id), reinterpret_cast<const BYTE*>(&message.id) + sizeof(int));


    p = reinterpret_cast<const Packet*>(message.data);
    int dataSize = 0;
    dataSize += sizeof(p->callerAddress);
    dataSize += sizeof(p->header);
    dataSize += sizeof(p->segments.size());
    for (int i = 0; i < p->segments.size(); i++)
    {
        dataSize += sizeof(p->segments.at(i).type);
        dataSize += sizeof(p->segments.at(i).bytes.size());
        for (int j = 0; j < p->segments.at(i).bytes.size(); j++)
        {
            dataSize += sizeof(BYTE);

        }
    }

    // Serialize the dataSize
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&dataSize), reinterpret_cast<const BYTE*>(&dataSize) + sizeof(int));

    // Serialize the callerAddress
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&p->callerAddress), reinterpret_cast<const BYTE*>(&p->callerAddress) + sizeof(DWORD));

    // Serialize the  header
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&p->header), reinterpret_cast<const BYTE*>(&p->header) + sizeof(WORD));


    // Serialize the segments length
    int vectorLen = p->segments.size();
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&vectorLen), reinterpret_cast<const BYTE*>(&vectorLen) + sizeof(int));


#   //seralize elements
    for (int i = 0; i < vectorLen; i++)
    {
        int elementSize = p->segments.at(i).bytes.size();
        //Serlize the encoding type
        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&p->segments.at(i).type), reinterpret_cast<const BYTE*>(&p->segments.at(i).type) + sizeof(int));

        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&elementSize), reinterpret_cast<const BYTE*>(&elementSize) + sizeof(int));
        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&p->segments.at(i).bytes.at(0)), reinterpret_cast<const BYTE*>(&p->segments.at(i).bytes.at(0)) + (elementSize * sizeof(BYTE)));

    }
    // Send the serialized message over the pipe

    if (!WriteFile(this->hNamedPipe, serializedData.data(), static_cast<DWORD>(serializedData.size()), &bytesWritten, NULL))
    {
        return false;
    }

    return true;
}
pipeMessage Pipe::readPipeMessage()
{
    pipeMessage receivedMessage;
    DWORD bytesRead;
    int messageID;
    int dataSize;
    BYTE* bytes;
    //deseraliize the messageID
    if (!ReadFile(this->hNamedPipe, &messageID, sizeof(int), &bytesRead, NULL))
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
    SetFilePointer(this->hNamedPipe, 4, NULL, FILE_CURRENT);
    //deseraliize the dataSize
    if (!ReadFile(this->hNamedPipe, &dataSize, sizeof(int), &bytesRead, NULL))
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
    SetFilePointer(this->hNamedPipe, 4, NULL, FILE_CURRENT);
    bytes = new BYTE[dataSize];
    //deseraliize the dataBytes
    for (int i = 0; i < dataSize; i++)
    {
        BYTE b;
        if (!ReadFile(this->hNamedPipe, &b, 1, &bytesRead, NULL))
        {
            receivedMessage.id = -1; // Indicate an error
            return receivedMessage;
        }
        SetFilePointer(this->hNamedPipe, sizeof(BYTE), NULL, FILE_CURRENT);
        bytes[i] = b;
    }
    receivedMessage.id = messageID;
    receivedMessage.data = bytes;

    return receivedMessage;
}

