#pragma once

#include <string>
#include <mutex>
#include <windows.h>

namespace mele {

    struct PresenceField {
        std::string Details;
        std::string State;
        std::string LargeKey;
        bool bSmallKey = false;
        char SmallKey[32] = {};

        bool Differs(const PresenceField& Other) const
        {
            if (Details != Other.Details) return true;
            if (State != Other.State) return true;
            if (LargeKey != Other.LargeKey) return true;
            if (bSmallKey != Other.bSmallKey) return true;
            if (bSmallKey && strcmp(SmallKey, Other.SmallKey) != 0) return true;
            return false;
        }
    };

    class PresenceState {
    public:
        static PresenceState& Instance();

        void Publish(const PresenceField& New);

        bool GetLatest(PresenceField* Out) const;

        long long LastChange() const { return m_LastChange; }
        long long SessionStart() const { return m_SessionStart; }
        void StartSessionOnce();

    private:
        PresenceState() = default;

        mutable std::recursive_mutex m_Mutex;
        PresenceField m_Latest;
        volatile long long m_LastChange = 0;
        long long m_SessionStart = 0;
    };

    long long NowEpochMs();

}
