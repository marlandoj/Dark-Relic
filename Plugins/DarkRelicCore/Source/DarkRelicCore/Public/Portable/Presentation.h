#pragma once
#include <cstddef>

namespace dark_relic {
enum class CuePriority { Ambient, Action, Warning, Terminal };

template<class PriorityAt>
int cue_victim(std::size_t count, CuePriority incoming, PriorityAt priority_at) {
    if (count < 16) return -1;
    int victim = -2;
    auto lowest = incoming;
    for (std::size_t i = 0; i < count; ++i) {
        if (priority_at(i) < lowest) {
            victim = static_cast<int>(i);
            lowest = priority_at(i);
        }
    }
    return victim;
}

inline bool voice_may_interrupt(int incoming, int active, double remaining) {
    return remaining <= 0 || incoming >= active;
}

enum class WardPrompt { None, MissingRelic, Ready, Holding };
inline WardPrompt ward_prompt(bool live, bool extracting, bool in_zone, bool relic) {
    if (!live) return WardPrompt::None;
    if (extracting) return WardPrompt::Holding;
    if (!in_zone) return WardPrompt::None;
    return relic ? WardPrompt::Ready : WardPrompt::MissingRelic;
}
}
