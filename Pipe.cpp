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
pipeMessage Pipe::readMessage()
{
    pipeMessage receivedMessage;

    DWORD bytesRead;

    int messageID{};
    int mainVectorSize{};
    DWORD callerAddress{};
    WORD header{};
    std::vector<Segment> segments{};
    Packet buffer{};
    //deseraliize the messageID
    if (!ReadFile(this->hNamedPipe, &messageID, sizeof(int), &bytesRead, NULL))
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
    SetFilePointer(this->hNamedPipe, 4, NULL, FILE_CURRENT);

    //deseralize callerAddress
    if (!ReadFile(this->hNamedPipe, &callerAddress, sizeof(DWORD), &bytesRead, NULL))
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
    SetFilePointer(this->hNamedPipe, sizeof(DWORD), NULL, FILE_CURRENT);

    //deseralize header
    if (!ReadFile(this->hNamedPipe, &header, sizeof(WORD), &bytesRead, NULL))
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
    SetFilePointer(this->hNamedPipe, sizeof(WORD), NULL, FILE_CURRENT);


    //deseralize mainVectorSize
    if (!ReadFile(this->hNamedPipe, &mainVectorSize, sizeof(int), &bytesRead, NULL))
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
    SetFilePointer(this->hNamedPipe, 4, NULL, FILE_CURRENT);

    for (int i = 0; i < mainVectorSize; i++)
    {
        Segment seg;
        std::vector<BYTE> buffer;
        int elementLen;
        int segmentType;

        //deseralize segmentType
        if (!ReadFile(this->hNamedPipe, &segmentType, sizeof(int), &bytesRead, NULL))
        {
            receivedMessage.id = -1; // Indicate an error
            return receivedMessage;
        }
        SetFilePointer(this->hNamedPipe, sizeof(int), NULL, FILE_CURRENT);

        //Deseralize element Length
        if (!ReadFile(this->hNamedPipe, &elementLen, sizeof(int), &bytesRead, NULL))
        {
            receivedMessage.id = -1; // Indicate an error
            return receivedMessage;
        }
        SetFilePointer(this->hNamedPipe, 4, NULL, FILE_CURRENT);


        //Deseralize element Data
        for (int j = 0; j < elementLen; j++)
        {
            BYTE buff;
            if (!ReadFile(this->hNamedPipe, &buff, sizeof(BYTE), &bytesRead, NULL))
            {
                receivedMessage.id = -1; // Indicate an error
                return receivedMessage;
            }
            buffer.emplace_back(buff);
        }

        seg.bytes = buffer;
        seg.type = segmentType;
        segments.emplace_back(seg);

    }
    receivedMessage.id = messageID;

    buffer.callerAddress = callerAddress;
    buffer.header = header;
    buffer.segments = segments;

    receivedMessage.data = buffer;
    return receivedMessage;
}

bool Pipe::sendMessage(const pipeMessage& message)
{
    DWORD bytesWritten;

    // Serialize the message into a byte vector
    std::vector<BYTE> serializedData;

    // Serialize the id
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.id), reinterpret_cast<const BYTE*>(&message.id) + sizeof(int));




    // Serialize the callerAddress
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.data), reinterpret_cast<const BYTE*>(&message.data) + sizeof(DWORD));

    // Serialize the  header
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.data) + sizeof(DWORD), reinterpret_cast<const BYTE*>(&message.data) + sizeof(DWORD) + sizeof(WORD));


    // Serialize the segments length
    int vectorLen = message.data.segments.size();
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&vectorLen), reinterpret_cast<const BYTE*>(&vectorLen) + sizeof(int));


#   //seralize elements
    for (int i = 0; i < vectorLen; i++)
    {
        int elementSize = message.data.segments.at(i).bytes.size();
        //Serlize the encoding type
        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.data.segments.at(i).type), reinterpret_cast<const BYTE*>(&message.data.segments.at(i).type) + sizeof(int));

        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&elementSize), reinterpret_cast<const BYTE*>(&elementSize) + sizeof(int));
        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.data.segments.at(i).bytes.at(0)), reinterpret_cast<const BYTE*>(&message.data.segments.at(i).bytes.at(0)) + (elementSize * sizeof(BYTE)));

    }
    // Send the serialized message over the pipe

    if (!WriteFile(this->hNamedPipe, serializedData.data(), static_cast<DWORD>(serializedData.size()), &bytesWritten, NULL))
    {
        return false;
    }

    return true;
}