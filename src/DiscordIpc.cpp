#include "DiscordIpc.hpp"
#include "Common.hpp"
#include "Log.hpp"

#include <ctime>
#include <sstream>

namespace mele {

    static std::wstring Utf8ToPathWide(const std::string& In)
    {
        int wlen = ::MultiByteToWideChar(CP_UTF8, 0, In.c_str(), static_cast<int>(In.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(wlen > 0 ? wlen : 0), L'\0');
        if (wlen > 0) {
            ::MultiByteToWideChar(CP_UTF8, 0, In.c_str(), static_cast<int>(In.size()), out.data(), wlen);
        }
        return out;
    }

    DiscordIpc& DiscordIpc::Instance()
    {
        static DiscordIpc instance;
        return instance;
    }

    void DiscordIpc::Configure(const std::string& ClientId)
    {
        m_ClientId = ClientId;
    }

    bool DiscordIpc::OpenPipeAnySlot()
    {
        if (m_ClientId.empty()) {
            LOG_ERROR("DiscordIpc: client id not configured");
            return false;
        }
        for (int slot = 0; slot < 10; ++slot) {
            char pipeName[96];
            snprintf(pipeName, sizeof(pipeName), "%s%d", k_IpcPipePrefix, slot);
            HANDLE h = ::CreateFileW(Utf8ToPathWide(pipeName).c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                m_Pipe = h;
                return true;
            }
        }
        return false;
    }

    bool DiscordIpc::TryConnect()
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        if (m_Connected) {
            return true;
        }

        if (m_Pipe == INVALID_HANDLE_VALUE) {
            if (!OpenPipeAnySlot()) {
                return false;
            }
            LOG_INFO("Opened Discord IPC pipe");
        }

        char handshake[192];
        int len = snprintf(handshake, sizeof(handshake),
            "{\"v\":1,\"client_id\":\"%s\"}", m_ClientId.c_str());

        bool needReconnect = false;
        if (!WriteFrameChecked(0, handshake, static_cast<size_t>(len), needReconnect)) {
            if (needReconnect) {
                Disconnect();
            }
            return false;
        }

        long long deadline = ::GetTickCount64() + 5000;
        bool bGotReady = false;
        while (::GetTickCount64() < deadline) {
            int opcode = 0;
            std::string payload;
            if (ReadFullFrame(&opcode, &payload, nullptr)) {
                if (opcode == 1 && payload.find("\"evt\":\"READY\"") != std::string::npos) {
                    bGotReady = true;
                    break;
                }
                if (opcode == 1 && payload.find("\"error\"") != std::string::npos) {
                    LOG_ERROR("IPC auth error while waiting for READY: %s",
                        payload.size() > 400 ? payload.substr(0, 400).c_str() : payload.c_str());
                    break;
                }
            } else {
                ::Sleep(50);
            }
        }
        if (!bGotReady) {
            LOG_ERROR("No READY frame from Discord within 5s; giving up");
            Disconnect();
            return false;
        }

        m_Connected = true;
        LOG_INFO("Handshake sent (client_id=%s); READY received", m_ClientId.c_str());
        DrainIncoming();
        return true;
    }

    void DiscordIpc::Disconnect()
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        if (m_Pipe && m_Pipe != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_Pipe);
        }
        m_Pipe = INVALID_HANDLE_VALUE;
        m_Connected = false;
        m_ReadBufferUsed = 0;
        LOG_INFO("Discord IPC disconnected");
    }

    bool DiscordIpc::WriteFrameChecked(int Opcode, const void* Payload, size_t PayloadSize, bool& OutNeedReconnect)
    {
        OutNeedReconnect = false;
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            OutNeedReconnect = true;
            return false;
        }
        if (PayloadSize > 0x100000u) {
            LOG_ERROR("payload too large");
            return false;
        }

        uint8_t header[8];
        header[0] = static_cast<uint8_t>(Opcode & 0xFF);
        header[1] = static_cast<uint8_t>((Opcode >> 8) & 0xFF);
        header[2] = static_cast<uint8_t>((Opcode >> 16) & 0xFF);
        header[3] = static_cast<uint8_t>((Opcode >> 24) & 0xFF);
        header[4] = static_cast<uint8_t>(PayloadSize & 0xFF);
        header[5] = static_cast<uint8_t>((PayloadSize >> 8) & 0xFF);
        header[6] = static_cast<uint8_t>((PayloadSize >> 16) & 0xFF);
        header[7] = static_cast<uint8_t>((PayloadSize >> 24) & 0xFF);

        DWORD writtenHeader = 0;
        if (!::WriteFile(m_Pipe, header, 8, &writtenHeader, nullptr) || writtenHeader != 8) {
            m_LastWriteError = static_cast<long>(::GetLastError());
            LOG_ERROR("IPC header write failed: GLE=%ld", m_LastWriteError);
            OutNeedReconnect = true;
            return false;
        }

        if (PayloadSize > 0) {
            DWORD writtenBody = 0;
            if (!::WriteFile(m_Pipe, Payload, static_cast<DWORD>(PayloadSize), &writtenBody, nullptr)
                || writtenBody != PayloadSize) {
                m_LastWriteError = static_cast<long>(::GetLastError());
                LOG_ERROR("IPC body write failed: GLE=%ld", m_LastWriteError);
                OutNeedReconnect = true;
                return false;
            }
        }
        return true;
    }

    bool DiscordIpc::WriteFrame(int Opcode, const void* Payload, size_t PayloadSize)
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        bool needReconnect = false;
        if (!WriteFrameChecked(Opcode, Payload, PayloadSize, needReconnect)) {
            if (needReconnect) {
                Disconnect();
            }
            return false;
        }
        return true;
    }

    bool DiscordIpc::SendActivityFrame(const std::string& ActivityJson)
    {
        std::string body;
        body.reserve(ActivityJson.size() + 128);
        char nonce[32];
        m_NonceCounter += 1;
        snprintf(nonce, sizeof(nonce), "melernpc%u", m_NonceCounter);

        body = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
        body += std::to_string(::GetCurrentProcessId());
        body += ",\"activity\":";
        body += ActivityJson;
        body += "},\"nonce\":\"";
        body += nonce;
        body += "\"}";

        return WriteFrame(1, body.data(), body.size());
    }

    bool DiscordIpc::SendClearActivity()
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        m_NonceCounter += 1;
        char nonce[32];
        snprintf(nonce, sizeof(nonce), "melernpc%u", m_NonceCounter);

        std::string body = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
        body += std::to_string(::GetCurrentProcessId());
        body += ",\"activity\":null},\"nonce\":\"";
        body += nonce;
        body += "\"}";
        return WriteFrame(1, body.data(), body.size());
    }

    bool DiscordIpc::SendPing()
    {
        return WriteFrame(3, nullptr, 0);
    }

    bool DiscordIpc::ReadAvailable(void* Buffer, size_t BufferSize, size_t* OutBytesRead)
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD available = 0;
        if (!::PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        if (available == 0) {
            return false;
        }
        DWORD toRead = static_cast<DWORD>(BufferSize < available ? BufferSize : available);
        DWORD read = 0;
        if (!::ReadFile(m_Pipe, Buffer, toRead, &read, nullptr) || read == 0) {
            return false;
        }
        *OutBytesRead = read;
        return true;
    }

    bool DiscordIpc::ReadFullFrame(int* OutOpcode, std::string* OutPayload, bool* OutDisconnected)
    {
        while (true) {
            if (m_ReadBufferUsed < 8) {
                size_t got = 0;
                uint8_t chunk[2048];
                if (!ReadAvailable(chunk, sizeof(chunk), &got)) {
                    return false;
                }
                size_t space = sizeof(m_ReadBuffer) - m_ReadBufferUsed;
                size_t copy = got < space ? got : space;
                memcpy(m_ReadBuffer + m_ReadBufferUsed, chunk, copy);
                m_ReadBufferUsed += copy;
                if (m_ReadBufferUsed < 8) {
                    return false;
                }
                (void)OutDisconnected;
                continue;
            }

            int opcode = 0;
            int length = 0;
            memcpy(&opcode, m_ReadBuffer, 4);
            memcpy(&length, m_ReadBuffer + 4, 4);
            if (length < 0 || static_cast<size_t>(length) > sizeof(m_ReadBuffer)) {
                LOG_ERROR("IPC frame GC frame too large (%d) — resetting buffer", length);
                m_ReadBufferUsed = 0;
                return false;
            }
            if (static_cast<size_t>(8 + length) > m_ReadBufferUsed) {
                size_t got = 0;
                uint8_t chunk[8192];
                if (!ReadAvailable(chunk, sizeof(chunk), &got)) {
                    return false;
                }
                size_t space = sizeof(m_ReadBuffer) - m_ReadBufferUsed;
                size_t copy = got < space ? got : space;
                if (copy == 0) {
                    return false;
                }
                memcpy(m_ReadBuffer + m_ReadBufferUsed, chunk, copy);
                m_ReadBufferUsed += copy;
                continue;
            }

            *OutOpcode = opcode;
            if (OutPayload) {
                OutPayload->assign(reinterpret_cast<char*>(m_ReadBuffer + 8), static_cast<size_t>(length));
            }
            size_t remaining = m_ReadBufferUsed - (8 + length);
            if (remaining) {
                memmove(m_ReadBuffer, m_ReadBuffer + 8 + length, remaining);
            }
            m_ReadBufferUsed = remaining;
            return true;
        }
    }

    bool DiscordIpc::ProbeHealth()
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            if (m_Connected) {
                m_Connected = false;
            }
            return false;
        }
        DWORD available = 0;
        if (!::PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &available, nullptr)) {
            DWORD err = ::GetLastError();
            if (err == 232 || err == 109 || err == 233) {
                LOG_INFO("Pipe health probe failed (GLE=%lu); connection lost", err);
                Disconnect();
                return false;
            }
        }
        return true;
    }

    void DiscordIpc::DrainIncoming()
    {
        std::lock_guard<std::recursive_mutex> lock(m_IoMutex);
        int opcode = 0;
        bool disconnected = false;
        std::string payload;
        int guard = 0;
        while (guard < 64) {
            guard += 1;
            payload.clear();
            if (!ReadFullFrame(&opcode, &payload, &disconnected)) {
                break;
            }
            if (opcode == 1 && !payload.empty()) {
                LOG_INFO("IPC frame: %s", payload.size() > 600 ? payload.substr(0, 600).c_str() : payload.c_str());
            } else if (opcode != 1) {
                LOG_INFO("IPC opcode %d (len %d)", opcode, (int)payload.size());
            }
        }
    }

    std::string BuildActivityJson(const std::string& LargeKey, bool bSmallKey, const char* SmallKey,
        const std::string& Details, const std::string& State, long long StartEpochMs)
    {
        std::string json = "{\"type\":0";

        json += ",\"details\":\"";
        for (char c : Details) {
            if (c == '"' || c == '\\') {
                json.push_back('\\');
            }
            if (static_cast<unsigned char>(c) < 32) {
                c = ' ';
            }
            json.push_back(c);
        }
        json += "\"";

        if (!State.empty()) {
            json += ",\"state\":\"";
            for (char c : State) {
                if (c == '"' || c == '\\') {
                    json.push_back('\\');
                }
                if (static_cast<unsigned char>(c) < 32) {
                    c = ' ';
                }
                json.push_back(c);
            }
            json += "\"";
        }

        if (StartEpochMs > 0) {
            long long startSeconds = StartEpochMs / 1000;
            if (startSeconds > 0) {
                json += ",\"timestamps\":{\"start\":";
                json += std::to_string(startSeconds);
                json += "}";
            }
        }

        json += ",\"assets\":{\"large_image\":\"";
        json += LargeKey;
        json += "\"";
        if (bSmallKey) {
            json += ",\"small_image\":\"";
            json += SmallKey;
            json += "\"";
        }
        json += "}";

        json += "}";
        return json;
    }

}
