#pragma once

#include <string>
#include <mutex>
#include <windows.h>

namespace mele {

    class DiscordIpc {
    public:
        static DiscordIpc& Instance();

        void Configure(const std::string& ClientId);

        bool TryConnect();
        void Disconnect();
        bool IsConnected() const { return m_Connected; }

        bool SendActivityFrame(const std::string& ActivityJson);
        bool SendClearActivity();
        bool SendPing();
        void DrainIncoming();
        bool ProbeHealth();

        int RecentWriteError() const { return m_LastWriteError; }

    private:
        DiscordIpc() = default;

        bool WriteFrame(int Opcode, const void* Payload, size_t PayloadSize);
        bool WriteFrameChecked(int Opcode, const void* Payload, size_t PayloadSize, bool& OutNeedReconnect);
        bool ReadAvailable(void* Buffer, size_t BufferSize, size_t* OutBytesRead);
        bool ReadFullFrame(int* OutOpcode, std::string* OutPayload, bool* OutDisconnected);

        bool OpenPipeAnySlot();

        std::string m_ClientId;
        HANDLE m_Pipe = INVALID_HANDLE_VALUE;
        volatile bool m_Connected = false;
        volatile long m_LastWriteError = 0;
        unsigned int m_NonceCounter = 0;
        std::recursive_mutex m_IoMutex;

        uint8_t m_ReadBuffer[64 * 1024];
        size_t m_ReadBufferUsed = 0;
    };

    std::string BuildActivityJson(const std::string& LargeKey, bool bSmallKey, const char* SmallKey,
        const std::string& Details, const std::string& State, long long StartEpochMs);

}
