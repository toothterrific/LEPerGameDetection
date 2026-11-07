#include "Presence.hpp"
#include "Log.hpp"
#include "Common.hpp"

namespace mele {

    PresenceState& PresenceState::Instance()
    {
        static PresenceState instance;
        return instance;
    }

    void PresenceState::StartSessionOnce()
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (m_SessionStart == 0) {
            m_SessionStart = NowEpochMs();
        }
    }

    void PresenceState::Publish(const PresenceField& New)
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        if (m_Latest.Differs(New)) {
            m_Latest = New;
            m_LastChange = NowEpochMs();
        }
    }

    bool PresenceState::GetLatest(PresenceField* Out) const
    {
        if (!Out) return false;
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        *Out = m_Latest;
        return true;
    }

}
