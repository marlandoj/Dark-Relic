#pragma once
#include <algorithm>
#include <cmath>

namespace dark_relic {
inline double fury_aura(double remaining, double duration, bool live) {
    if (!live || !std::isfinite(remaining) || !std::isfinite(duration) || duration <= 0 || remaining <= 0 || remaining > duration) return 0;
    auto smooth = [](double x) { x = std::clamp(x, 0.0, 1.0); return x*x*(3-2*x); };
    return smooth((duration-remaining)/0.25) * smooth(remaining/std::min(2.0,duration*0.5));
}
}
