// Pipe.cpp : Defines the functions for the static library.
//

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
        std::wcerr << "Failed to create named pipe: " << this->pipeName << std::endl;
        return false;
    }
    return true;
}
bool Pipe::waitForClient()
{
    if (!WaitNamedPipe(this->pipeName.c_str(), 5000))
    {
        // Timeout occurred, handle as needed
        std::wcerr << "Couldn't find clients for pipe: " << this->pipeName << std::endl;
        CloseHandle(hNamedPipe);
        return false;
    }
    std::wcout << "Client found for pipe: " << this->pipeName << std::endl;
    return true;
}

bool Pipe::connectPipe()
{
    const int maxRetries = 3;
    int currentRetries = 0;
    while (currentRetries < maxRetries)
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
            std::wcout << "Failed connecting to pipe: " << this->pipeName << std::endl;
            Sleep(1000);
            currentRetries++;
        }
        else
        {
            std::wcout << "Connected to Pipe: " << this->pipeName << std::endl;
            return true;
        }
    }
    return false;



}
bool Pipe::sendBlockHeaderMessage(const pipeMessage& message)
{
    DWORD bytesWritten;
    int dataSize = 6;
    // Serialize the message into a byte vector
    std::vector<BYTE> serializedData;
    // Serialize the id
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&message.id), reinterpret_cast<const BYTE*>(&message.id) + sizeof(int));

    // Seralize the dataSize
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&dataSize), reinterpret_cast<const BYTE*>(&dataSize) + sizeof(int));
    // Seralize the data 
    serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(message.data), reinterpret_cast<const BYTE*>(message.data) + dataSize);

    // Send the serialized message over the pipe

    if (!WriteFile(this->hNamedPipe, serializedData.data(), static_cast<DWORD>(serializedData.size()), &bytesWritten, NULL))
    {
        return false;
    }

    return true;

}
size_t CalculateSerializedPacketSize(const Packet& p)
{
    size_t size = 0;

    size += sizeof(p.callerAddress);   // DWORD = 4 bytes
    size += sizeof(p.header);          // WORD  = 2 bytes

    size += sizeof(p.segments.size()); // size() returns a int

    //for each segment
    for (const Segment& seg : p.segments)
    {
        size += sizeof(seg.type);        // int = 4 bytes
        size += sizeof(seg.len);         // WORD = 2 bytes

        size += sizeof(seg.bytes.size());  // size() returns a int

        size += seg.bytes.size();        // the amoount of actual bytes we need to allocate memory for
    }

    return size;
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
    
    size_t dataSize = CalculateSerializedPacketSize(*p);

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
        serializedData.insert(serializedData.end(), reinterpret_cast<const BYTE*>(&p->segments.at(i).len), reinterpret_cast<const BYTE*>(&p->segments.at(i).len) + sizeof(WORD));
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
    if (dataSize < 0)
    {
        receivedMessage.id = -1; // Indicate an error
        return receivedMessage;
    }
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



