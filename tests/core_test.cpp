#include "Portable/DarkRelicRules.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace dark_relic;
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void win(Rules& game) {
    check(game.zone(true), "enter zone");
    check(game.extract(), "begin extraction");
    check(game.tick(20), "advance countdown");
    check(game.snapshot().phase == Phase::Escaped, "escaped");
}
int main() {
    Rules g;
    check(!g.extract(), "cannot extract before run");
    check(!g.pickup(Item::Iron, 1, 1), "no loot before run");
    check(g.start(), "start");
    check(!g.start(), "cannot reset active run");
    check(!g.extract(), "requires presence");
    check(g.pickup(Item::Iron, 2, 1), "iron pickup");
    check(!g.pickup(Item::Iron, 2, 1), "repeated overlap rejected");
    check(g.pickup(Item::Blackbell, 1, 2), "relic pickup");
    check(!g.pickup(Item::Blackbell, 1, 3), "one relic per run");
    check(!g.pickup(static_cast<Item>(255), 1, 4), "invalid item");
    check(!g.pickup(Item::Salt, -1, 5), "negative pickup");
    check(!g.pickup(Item::Salt, 1, 0), "zero identity rejected");
    check(g.zone(true) && g.extract(), "countdown starts");
    check(g.tick(19), "partial countdown");
    check(!g.extract(), "cannot rearm extracting state");
    check(g.zone(false), "exit zone");
    check(g.tick(100) && g.snapshot().phase == Phase::Running, "leaving cancels win");
    win(g);
    check(g.snapshot().bank.credits == 120, "banked correct value");
    check(g.snapshot().carried == std::array<int,4>{}, "cleared carried");
    check(!g.extract() && g.tick(60) && g.snapshot().bank.credits == 120, "bank once");
    check(g.buy_upgrade(), "affordable upgrade");
    check(!g.buy_upgrade() && g.snapshot().bank.credits == 20, "spend upgrade once");
    Bank saved = g.snapshot().bank;
    Rules restored;
    check(restored.load(saved), "load persistent bank");
    check(restored.start() && restored.snapshot().max_health == 120, "upgrade applies to next run");
    check(restored.pickup(Item::Salt, 1, 1), "new run pickup identity reusable");
    check(!restored.load(saved), "no mid-run save rollback");
    check(restored.damage(1000), "lethal damage");
    check(restored.snapshot().phase == Phase::Dead && restored.snapshot().carried[2] == 0, "death loses loot");
    check(restored.snapshot().bank.credits == 20 && restored.snapshot().bank.items[3] == 1, "death preserves bank");
    check(!restored.damage(1) && !restored.act(Action::Heal), "dead cannot act");
    check(restored.start() && restored.snapshot().health == 120, "restart after death");
    check(restored.act(Action::Dodge), "dodge accepted");
    check(!restored.damage(10), "dodge invulnerability");
    check(!restored.act(Action::Heavy), "no overlapping actions");
    check(restored.tick(0.31) && restored.damage(10), "invulnerability expires before recovery");
    check(restored.tick(1) && restored.act(Action::Heal), "heal starts");
    check(restored.snapshot().health == 110, "heal has windup");
    check(restored.tick(0.8) && restored.snapshot().health == 120, "heal clamped at cap");
    check(!restored.act(Action::Heal), "full health rejects heal without charge");
    check(restored.snapshot().heals == 1, "charge spent exactly once");
    const double health = restored.snapshot().health;
    check(!restored.damage(-1) && !restored.damage(std::numeric_limits<double>::infinity()), "invalid damage rejected");
    check(!restored.tick(-1) && !restored.tick(std::numeric_limits<double>::quiet_NaN()), "invalid clock rejected");
    check(restored.snapshot().health == health, "invalid requests do not mutate health");
    check(restored.act(Action::Heavy), "heavy accepted");
    check(restored.tick(0.9) && restored.act(Action::Light), "heavy recovery opens light");
    check(restored.damage(1000), "death cancels attack");
    check(restored.snapshot().action == Action::None, "no lingering attack on death");
    saved.version = 9;
    check(!restored.load(saved), "unknown save version rejected");
    saved.version = 1; saved.credits = -1;
    check(!restored.load(saved), "negative bank rejected");
    saved.credits = 0; saved.items[0] = Rules::MaxCount + 1;
    check(!restored.load(saved), "oversized bank rejected");
    check(restored.snapshot().bank.credits == 20, "invalid save preserves current bank");
    Rules capped;
    Bank cap; cap.credits = Rules::MaxCredits;
    check(capped.load(cap) && capped.start(), "credit cap fixture");
    check(!capped.pickup(Item::Iron, 1, 1), "prevent bank overflow before pickup");
    Rules no_regen;
    Tuning tune; tune.stamina_regen = 0;
    check(no_regen.configure(tune) && no_regen.start(), "custom tuning");
    for (int i=0;i<3;++i) check(no_regen.act(Action::Heavy) && no_regen.tick(1), "heavy drains stamina");
    check(!no_regen.act(Action::Heavy), "insufficient stamina");
    tune.health = std::numeric_limits<double>::quiet_NaN();
    Rules invalid;
    check(!invalid.configure(tune) && invalid.snapshot().health == 100, "bad config rejected atomically");
    Rules before, after;
    check(before.start() && after.start(), "time comparison setup");
    check(before.damage(50) && after.damage(50), "time comparison damage");
    check(before.act(Action::Heal) && after.act(Action::Heal), "time comparison heal");
    check(before.tick(1), "single tick");
    for(int i=0;i<10;++i) check(after.tick(0.1), "partition tick");
    check(before.snapshot().health == after.snapshot().health && before.snapshot().action == after.snapshot().action, "partitioned time same outcome");
    Rules fatal;
    check(fatal.start() && fatal.pickup(Item::Blackbell,1,1) && fatal.zone(true) && fatal.extract(), "death during extraction setup");
    check(fatal.tick(19) && fatal.damage(1000) && fatal.tick(10), "death before timer");
    check(fatal.snapshot().phase == Phase::Dead && fatal.snapshot().bank.credits == 0, "dead cannot extract later");
    std::cout << "PASS: " << checks << " checks\n";
}
