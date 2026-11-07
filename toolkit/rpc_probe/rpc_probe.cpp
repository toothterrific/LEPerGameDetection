#include "../../src/Common.hpp"
#include "../../src/DiscordIpc.hpp"
#include "../../src/Log.hpp"
#include "../../src/Presence.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    mele::LogInit(false);
    std::string clientId = argc >= 2 ? argv[1] : "1568689342417739806";

    auto* ipc = &mele::DiscordIpc::Instance();
    ipc->Configure(clientId);

    if (!ipc->TryConnect()) {
        printf("connect failed\n");
        return 1;
    }

    long long nowSec = mele::NowEpochMs() / 1000;

    struct Cand { const char* label; std::string act; };
    std::vector<Cand> candidates;

    candidates.push_back({ "A: state+details+instance:true",
        "{\"state\":\"State A\",\"details\":\"Details A\",\"instance\":true}" });

    candidates.push_back({ "B: +type 0",
        "{\"type\":0,\"state\":\"State B\",\"details\":\"Details B\",\"instance\":true}" });

    candidates.push_back({ "C: +timestamps seconds",
        "{\"state\":\"State C\",\"details\":\"Details C\",\"instance\":true,\"timestamps\":{\"start\":" + std::to_string(nowSec) + "}}" });

    candidates.push_back({ "D: +assets",
        "{\"state\":\"State D\",\"details\":\"Details D\",\"instance\":true,\"timestamps\":{\"start\":" + std::to_string(nowSec) + "},\"assets\":{\"large_image\":\"le1\",\"small_image\":\"menu\"}}" });

    candidates.push_back({ "E: no instance field",
        "{\"state\":\"State E\",\"details\":\"Details E\",\"timestamps\":{\"start\":" + std::to_string(nowSec) + "}}" });

    candidates.push_back({ "F: type 2 listening musiclike",
        "{\"type\":2,\"state\":\"State F\",\"details\":\"Details F\",\"instance\":true,\"timestamps\":{\"start\":" + std::to_string(nowSec) + "}}" });

    int sentAnySuccess = -1;
    for (size_t i = 0; i < candidates.size(); ++i) {
        ipc->DrainIncoming();
        printf("\n--- Candidate %s\n", candidates[i].label);
        if (!ipc->SendActivityFrame(candidates[i].act)) {
            printf("send failed (pipe)\n");
            break;
        }
        ipc->DrainIncoming();
        Sleep(1500);
        ipc->DrainIncoming();
    }

    printf("\nDone. Check the latest log file in %%LOCALAPPDATA%%\\MELEDiscordRPC for which candidate returned data != null.\n");
    Sleep(300);
    ipc->SendClearActivity();
    Sleep(300);
    ipc->DrainIncoming();
    mele::LogShutdown();
    return 0;
}
