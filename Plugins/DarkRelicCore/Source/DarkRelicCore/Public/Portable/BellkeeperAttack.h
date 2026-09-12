#pragma once
#include <cmath>
#include <algorithm>

namespace dark_relic {
struct BellkeeperAttack {
    double remaining = 0;
    double duration = 1.6;
    double cooldown = 3;
    bool enraged = false;
    static constexpr double Radius = 360;

    bool update_health(double health, double maximum) {
        if (!enraged && std::isfinite(health) && std::isfinite(maximum) &&
            health > 0 && maximum > 0 && health < maximum * 0.5) {
            enraged = true;
            return true;
        }
        return false;
    }
    bool begin() {
        if (remaining > 0 || cooldown > 0) return false;
        duration = enraged ? 1.25 : 1.6;
        remaining = duration;
        return true;
    }
    bool tick(double dt) {
        if (!std::isfinite(dt) || dt <= 0) return false;
        if (remaining > 0) {
            remaining = std::max(0.0, remaining - dt);
            if (remaining == 0) {
                cooldown = enraged ? 4.5 : 6;
                return true;
            }
        } else cooldown = std::max(0.0, cooldown - dt);
        return false;
    }
    void cancel() { remaining = 0; cooldown = enraged ? 4.5 : 6; }
    bool hits(double horizontal_distance, double height_difference, bool visible) const {
        return visible && std::isfinite(horizontal_distance) && horizontal_distance >= 0 &&
            horizontal_distance <= Radius && std::isfinite(height_difference) &&
            std::abs(height_difference) <= 180;
    }
    double damage() const { return enraged ? 34 : 26; }
};
}
