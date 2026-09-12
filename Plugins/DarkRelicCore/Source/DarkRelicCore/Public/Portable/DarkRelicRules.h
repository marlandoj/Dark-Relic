#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace dark_relic {
enum class Phase : std::uint8_t { Ready, Running, Extracting, Escaped, Dead };
enum class Action : std::uint8_t { None, Light, Heavy, Dodge, Heal, RelicBurst, Rally };
enum class Item : std::uint8_t { Iron, Tallow, Salt, Blackbell };

struct Tuning {
    double health = 100, stamina = 100, stamina_regen = 20;
    double light_cost = 15, heavy_cost = 30, dodge_cost = 25;
    double light_seconds = 0.5, heavy_seconds = 0.9, dodge_seconds = 0.55;
    double invulnerable_seconds = 0.3, heal_seconds = 0.8, heal_amount = 30;
    double extraction_seconds = 20;
    double combo_window = 0.8, finisher_damage = 50, finisher_cost = 20, finisher_seconds = 0.7;
    double burst_damage = 45, burst_cost = 35, burst_seconds = 0.8, burst_cooldown = 8;
    double rally_cost = 20, rally_seconds = 0.35, rally_duration = 6, rally_cooldown = 20;
    double rally_damage_multiplier = 1.35, rally_damage_taken = 0.75;
    int heal_charges = 2;
};

struct Bank {
    int version = 1;
    int credits = 0;
    int upgrade = 0;
    std::array<int, 4> items{};
};

struct Snapshot {
    Phase phase = Phase::Ready;
    Action action = Action::None;
    double health = 100, max_health = 100, stamina = 100;
    double action_remaining = 0, invulnerable_remaining = 0, extraction_remaining = 0;
    int heals = 2;
    bool in_zone = false;
    int combo_step = 0;
    bool finisher = false;
    double combo_remaining = 0, attack_damage = 0;
    double burst_cooldown = 0, rally_cooldown = 0, rally_remaining = 0;
    std::array<int, 4> carried{};
    Bank bank{};
};

class Rules {
public:
    static constexpr int MaxCount = 1000000;
    static constexpr int MaxCredits = 1000000000;
    static constexpr int MaxHeals = 100;
    static constexpr int UpgradeCost = 100;
    static constexpr std::array<int, 4> Prices{10, 15, 20, 100};

    const Snapshot& snapshot() const { return state_; }
    const Tuning& tuning() const { return tuning_; }
    bool live() const { return state_.phase == Phase::Running || state_.phase == Phase::Extracting; }

    bool configure(const Tuning& t) {
        if (live()) return false;
        const double positive[] = {t.health, t.stamina, t.light_seconds, t.heavy_seconds,
            t.dodge_seconds, t.heal_seconds, t.heal_amount, t.extraction_seconds,
            t.combo_window, t.finisher_damage, t.finisher_seconds,
            t.burst_damage, t.burst_seconds, t.burst_cooldown,
            t.rally_seconds, t.rally_duration, t.rally_cooldown, t.rally_damage_multiplier};
        const double nonnegative[] = {t.stamina_regen, t.light_cost, t.heavy_cost,
            t.dodge_cost, t.invulnerable_seconds, t.finisher_cost, t.burst_cost, t.rally_cost};
        for (double v : positive) if (!finite(v) || v <= 0 || v > 1000000) return false;
        for (double v : nonnegative) if (!finite(v) || v < 0 || v > 1000000) return false;
        if (t.heal_charges < 0 || t.heal_charges > MaxHeals) return false;
        if (!finite(t.rally_damage_taken) || t.rally_damage_taken <= 0 || t.rally_damage_taken > 1 ||
            t.rally_damage_multiplier < 1 || t.rally_damage_multiplier > 3 ||
            t.rally_duration >= t.rally_cooldown || t.rally_seconds >= t.rally_duration ||
            t.burst_seconds >= t.burst_cooldown || t.finisher_cost > t.stamina ||
            t.burst_cost > t.stamina || t.rally_cost > t.stamina) return false;
        if (t.invulnerable_seconds > t.dodge_seconds || t.light_cost > t.stamina ||
            t.heavy_cost > t.stamina || t.dodge_cost > t.stamina) return false;
        tuning_ = t;
        state_.max_health = t.health + state_.bank.upgrade * 20;
        state_.health = state_.max_health;
        state_.stamina = t.stamina;
        state_.heals = t.heal_charges;
        return true;
    }

    bool start() {
        if (live()) return false;
        const Bank bank = state_.bank;
        state_ = Snapshot{};
        state_.bank = bank;
        state_.phase = Phase::Running;
        state_.max_health = tuning_.health + bank.upgrade * 20;
        state_.health = state_.max_health;
        state_.stamina = tuning_.stamina;
        state_.heals = tuning_.heal_charges;
        pickups_.clear();
        return true;
    }

    bool pickup(Item item, int count, std::uint64_t pickup_id) {
        const auto i = static_cast<std::size_t>(item);
        if (!live() || i >= 4 || count <= 0 || count > MaxCount || pickup_id == 0 ||
            pickups_.count(pickup_id) || state_.carried[i] > MaxCount - count) return false;
        if (item == Item::Blackbell && (count != 1 || state_.carried[i] != 0)) return false;
        std::int64_t value = static_cast<std::int64_t>(count) * Prices[i];
        for (std::size_t j = 0; j < 4; ++j) {
            if (state_.bank.items[j] + state_.carried[j] > MaxCount - (j == i ? count : 0)) return false;
            value += static_cast<std::int64_t>(state_.carried[j]) * Prices[j];
        }
        if (value + state_.bank.credits > MaxCredits) return false;
        pickups_.insert(pickup_id);
        state_.carried[i] += count;
        return true;
    }

    bool zone(bool present) {
        if (!live()) return false;
        state_.in_zone = present;
        if (!present && state_.phase == Phase::Extracting) {
            state_.phase = Phase::Running;
            state_.extraction_remaining = 0;
        }
        return true;
    }

    bool extract() {
        if (state_.phase != Phase::Running || !state_.in_zone) return false;
        state_.phase = Phase::Extracting;
        state_.extraction_remaining = tuning_.extraction_seconds;
        return true;
    }

    bool act(Action action) {
        if (!live() || state_.action != Action::None) return false;
        double cost = 0, duration = 0;
        const bool finisher = action == Action::Light && state_.combo_step == 2 && state_.combo_remaining > 0;
        switch (action) {
            case Action::Light:
                cost = finisher ? tuning_.finisher_cost : tuning_.light_cost;
                duration = finisher ? tuning_.finisher_seconds : tuning_.light_seconds;
                break;
            case Action::Heavy: cost = tuning_.heavy_cost; duration = tuning_.heavy_seconds; break;
            case Action::Dodge: cost = tuning_.dodge_cost; duration = tuning_.dodge_seconds; break;
            case Action::Heal:
                if (state_.heals <= 0 || state_.health >= state_.max_health) return false;
                duration = tuning_.heal_seconds;
                break;
            case Action::RelicBurst:
                if (state_.burst_cooldown > 0) return false;
                cost = tuning_.burst_cost; duration = tuning_.burst_seconds;
                break;
            case Action::Rally:
                if (state_.rally_cooldown > 0 || state_.rally_remaining > 0) return false;
                cost = tuning_.rally_cost; duration = tuning_.rally_seconds;
                break;
            default: return false;
        }
        if (state_.stamina < cost) return false;
        state_.stamina -= cost;
        state_.action = action;
        state_.action_remaining = duration;
        state_.finisher = finisher;
        state_.attack_damage = action == Action::Light ? (finisher ? tuning_.finisher_damage : 25) :
            action == Action::Heavy ? 55 : action == Action::RelicBurst ? tuning_.burst_damage : 0;
        if (state_.rally_remaining > 0) state_.attack_damage *= tuning_.rally_damage_multiplier;
        if (action == Action::Light) {
            state_.combo_step = finisher ? 3 : (state_.combo_remaining > 0 && state_.combo_step == 1 ? 2 : 1);
            state_.combo_remaining = duration + tuning_.combo_window;
        } else {
            state_.combo_step = 0; state_.combo_remaining = 0;
        }
        if (action == Action::RelicBurst) state_.burst_cooldown = tuning_.burst_cooldown;
        if (action == Action::Rally) {
            state_.rally_cooldown = tuning_.rally_cooldown;
            state_.rally_remaining = tuning_.rally_duration;
        }
        if (action == Action::Dodge) state_.invulnerable_remaining = tuning_.invulnerable_seconds;
        if (action == Action::Heal) --state_.heals;
        return true;
    }

    bool damage(double amount) {
        if (!live() || !finite(amount) || amount <= 0 || state_.invulnerable_remaining > 0) return false;
        state_.health = std::max(0.0, state_.health - amount * (state_.rally_remaining > 0 ? tuning_.rally_damage_taken : 1));
        if (state_.health == 0) {
            state_.phase = Phase::Dead;
            state_.carried.fill(0);
            end_activity();
        }
        return true;
    }

    bool tick(double seconds) {
        if (!finite(seconds) || seconds < 0 || seconds > 3600) return false;
        if (!live() || seconds == 0) return true;
        const double dt = state_.phase == Phase::Extracting
            ? std::min(seconds, state_.extraction_remaining) : seconds;
        for (double* timer : {&state_.combo_remaining, &state_.burst_cooldown, &state_.rally_cooldown, &state_.rally_remaining}) {
            *timer = std::max(0.0, *timer - dt);
            if (*timer <= 1e-9) *timer = 0;
        }
        if (state_.combo_remaining == 0) state_.combo_step = 0;
        state_.stamina = std::min(tuning_.stamina, state_.stamina + tuning_.stamina_regen * dt);
        state_.invulnerable_remaining = std::max(0.0, state_.invulnerable_remaining - dt);
        state_.action_remaining = std::max(0.0, state_.action_remaining - dt);
        if (state_.action != Action::None && state_.action_remaining <= 1e-9) {
            if (state_.action == Action::Heal) state_.health = std::min(state_.max_health, state_.health + tuning_.heal_amount);
            state_.action = Action::None;
            state_.action_remaining = 0;
            state_.finisher = false;
            state_.attack_damage = 0;
        }
        if (state_.phase == Phase::Extracting) {
            state_.extraction_remaining = std::max(0.0, state_.extraction_remaining - dt);
            if (state_.extraction_remaining <= 1e-9) {
                for (std::size_t i = 0; i < 4; ++i) {
                    state_.bank.items[i] += state_.carried[i];
                    state_.bank.credits += state_.carried[i] * Prices[i];
                }
                state_.carried.fill(0);
                state_.phase = Phase::Escaped;
                end_activity();
            }
        }
        return true;
    }

    bool buy_upgrade() {
        if (live() || state_.bank.upgrade != 0 || state_.bank.credits < UpgradeCost) return false;
        state_.bank.credits -= UpgradeCost;
        state_.bank.upgrade = 1;
        state_.max_health = tuning_.health + 20;
        return true;
    }

    bool load(const Bank& bank) {
        if (live() || bank.version != 1 || bank.credits < 0 || bank.credits > MaxCredits ||
            bank.upgrade < 0 || bank.upgrade > 1) return false;
        for (int count : bank.items) if (count < 0 || count > MaxCount) return false;
        state_ = Snapshot{};
        state_.bank = bank;
        state_.max_health = tuning_.health + bank.upgrade * 20;
        state_.health = state_.max_health;
        state_.stamina = tuning_.stamina;
        state_.heals = tuning_.heal_charges;
        pickups_.clear();
        return true;
    }

private:
    static bool finite(double n) { return std::isfinite(n); }
    void end_activity() {
        state_.action = Action::None;
        state_.action_remaining = 0;
        state_.invulnerable_remaining = 0;
        state_.extraction_remaining = 0;
        state_.in_zone = false;
        state_.combo_step = 0; state_.finisher = false;
        state_.combo_remaining = 0; state_.attack_damage = 0;
        state_.burst_cooldown = 0; state_.rally_cooldown = 0; state_.rally_remaining = 0;
    }
    Tuning tuning_{};
    Snapshot state_{};
    std::unordered_set<std::uint64_t> pickups_{};
};
}
