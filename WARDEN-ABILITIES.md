# Warden relic arts

Warden remains a sword fighter and now channels relic magic. These abilities are available from the start of each run; they do not consume Blackbell or banked loot.

| Ability | Input | Effect | Limits |
| --- | --- | --- | --- |
| Sunder | Three timed LMB strikes | 25, 25, then 50 damage; third strike uses the heavy animation and wider melee reach | 15, 15, 20 stamina; wait for each swing to recover and continue within 0.8 seconds; finisher recovery 0.7 seconds |
| Relic Burst | F | 45 damage shockwave within 4.5 meters, including enemies behind Warden | 35 stamina; 0.4-second default windup; 0.8-second recovery; 8-second cooldown from activation; cover and vertical separation block hits |
| Warden Fury | R during play | Six seconds of 35% stronger attacks and 25% less incoming damage | 20 stamina; 0.35-second recovery; 20-second cooldown from activation; cannot stack or refresh |

Q still heals, Shift dodges, RMB performs the existing 55-damage heavy attack, and E interacts. R starts a new run on the result screen. Other successful actions interrupt the sword chain. Failed inputs spend no stamina and start no cooldown. Fury attack damage is fixed when a swing/cast starts; incoming mitigation ends immediately when Fury expires. Dodge immunity remains stronger than Fury mitigation.

The HUD displays cooldown seconds, active Fury duration, low stamina/recovery states and the sword-chain window. Sunder uses an existing Greystone attack; relic magic uses procedural sound and a short expanding ring. No new marketplace asset, weapon system, inventory, or player character is required. M disables camera shake.

Death, extraction and restart clear transient ability state. Version-one bank saves remain compatible. All ability tuning is exposed on the run component. The Blueprint action enum appends RelicBurst and Rally, preserving old numeric values.

## Validation

146 portable C++ checks pass. The Unreal runtime smoke now exercises the three-hit combo, live finisher animation and damage, Fury mitigation/expiry, buffed shockwave against front and rear enemies, repeat-cast rejection, and extraction/restart cleanup alongside existing gameplay checks. Windows compilation and packaged verification are pending until a passing warden-build.json receipt is recorded.

Use integration/run_warden_build.ps1 only with the separately prepared H:\DarkRelicWarden-20260912 project. It writes H:\DarkRelicWardenPackage and does not replace the existing character or enhanced releases. Human play/feel review is still required; automated checks do not establish submission readiness.
