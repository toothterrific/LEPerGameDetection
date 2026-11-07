#include "../../src/Common.hpp"
#include "../../src/DiscordIpc.hpp"
#include "../../src/Log.hpp"
#include "../../src/Presence.hpp"

#include <cstdio>
#include <string>
#include <cstdlib>

int main(int argc, char** argv)
{
    const char* useDefaultsDetails = "rpc_test: pipeline works!";
    std::string clientId;
    std::string details = useDefaultsDetails;
    std::string largeKey = "le1";
    std::string smallKey;
    std::string state;
    int durationSeconds = 30;

    mele::LogInit(false);

    if (argc < 2) {
        printf("usage: rpc_test <client_id> [large_image] [small_image] [details] [duration_sec]\n");
        printf("example: rpc_test 1234567890123456789 le1 menu \"Hello from MELE\" 60\n");
        return 1;
    }

    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    clientId = argv[1];
    if (argc >= 3 && argv[2] && argv[2][0]) largeKey = argv[2];
    if (argc >= 4 && argv[3] && argv[3][0]) smallKey = argv[3];
    if (argc >= 5 && argv[4] && argv[4][0]) details = argv[4];
    if (argc >= 6) durationSeconds = atoi(argv[5]);

    auto* ipc = &mele::DiscordIpc::Instance();
    ipc->Configure(clientId);

    printf("Connecting to Discord...\n");
    if (!ipc->TryConnect()) {
        printf("Could not connect: is Discord running?\n");
        mele::LogShutdown();
        return 2;
    }

    long long start = mele::NowEpochMs();
    std::string json = mele::BuildActivityJson(
        largeKey, !smallKey.empty(), smallKey.c_str(), details, state, start);
    printf("JSON: %s\n", json.c_str());
    if (!ipc->SendActivityFrame(json)) {
        printf("Send failed\n");
        mele::LogShutdown();
        return 3;
    }

    printf("Presence pushed: large='%s' small='%s' details='%s'\n",
        largeKey.c_str(), smallKey.c_str(), details.c_str());
    printf("Keep-alive for %d seconds, then clears presence...\n", durationSeconds);

    for (int i = 0; i < durationSeconds; ++i) {
        Sleep(1000);
        ipc->DrainIncoming();
    }

    if (ipc->SendClearActivity()) {
        printf("Presence cleared. Test complete.\n");
    }
    Sleep(500);
    ipc->DrainIncoming();

    mele::LogShutdown();
    return 0;
}
