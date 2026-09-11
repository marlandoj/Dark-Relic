#include "Portable/DarkRelicRules.h"
#include <iostream>
int main() {
    dark_relic::Rules game;
    if (!game.start() || !game.pickup(dark_relic::Item::Blackbell, 1, 1) ||
        !game.zone(true) || !game.extract() || !game.tick(20)) return 1;
    std::cout << "Blackbell extracted. Bank credits: " << game.snapshot().bank.credits << '\n';
    if (!game.buy_upgrade() || !game.start()) return 1;
    std::cout << "Next-run health: " << game.snapshot().max_health << '\n';
}
