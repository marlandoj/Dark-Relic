# Warden relic arts

Warden remains a sword fighter and now channels relic magic. These abilities are available from the start of each run; they do not consume Blackbell or banked loot.

| Ability | Input | Effect | Limits |
| --- | --- | --- | --- |
| Sunder | Three timed LMB strikes | 25, 25, then 50 damage; third strike uses the heavy animation and wider melee reach | 15, 15, 20 stamina; wait for each swing to recover and continue within 0.8 seconds; finisher recovery 0.7 seconds |
| Relic Burst | F | 45 damage shockwave within 4.5 meters, including enemies behind Warden | 35 stamina; 0.4-second default windup; 0.8-second recovery; 8-second cooldown from activation; cover and vertical separation block hits |
| Warden Fury | R during play | Six seconds of 35% stronger attacks and 25% less incoming damage | 20 stamina; 0.35-second recovery; 20-second cooldown from activation; cannot stack or refresh |

Q still heals, Shift dodges, RMB performs the existing 55-damage heavy attack, and E interacts. R starts a new run on the result screen. Other successful actions interrupt the sword chain. Failed inputs spend no stamina and start no cooldown. Fury attack damage is fixed when a swing/cast starts; incoming mitigation ends immediately when Fury expires. Dodge immunity remains stronger than Fury mitigation.

The HUD displays cooldown seconds, active Fury duration, low stamina/recovery states and the sword-chain window. Sunder uses an existing Greystone attack; relic magic uses procedural sound and a short expanding ring. No new marketplace asset, weapon system, inventory, or player character is required. M disables camera shake.

Death, extraction and restart clear transient ability state. Version-one bank saves remain compatible. Damage, stamina costs, combo window, recovery, Fury multipliers and cooldowns are exposed on the run component. Burst radius and vertical tolerance are fixed at 450 and 180 centimeters in the encounter. The Blueprint action enum appends RelicBurst and Rally, preserving old numeric values.

## Validation

146 portable C++ checks pass, including AddressSanitizer and UndefinedBehaviorSanitizer. Source revision 69f9f72 passed Unreal compilation, character bindings, rendering and packaging on September 12, 2026. Both editor and packaged runtime smoke passed all 47 checks, covering the three-hit combo, live finisher animation and damage, Fury mitigation/expiry, buffed shockwave against front and rear enemies, repeat-cast rejection, and extraction/restart cleanup alongside existing gameplay checks.

The separate Warden package passed a fresh ordinary launch and foreground Escape exit with code 0 at 7:41 AM Arizona. The Dark Relic Warden Candidate desktop shortcut targets that package. Ten plugin source hashes match the installed Windows source; fourteen protected files in the earlier Enhanced and Character releases remain unchanged at release verification. Seven candidate executable/container files have recorded hashes. The first automated Escape attempt did not close the game; a later attached-process check could not recover an exit code. Both failed receipts are retained; the fresh launch with a retained process handle provides the successful exit evidence. No game-code change was required to complete delivery verification.

This validates the built candidate, not human play feel, worst-case performance, or hackathon submission readiness. The separate CSV-profiler shutdown diagnostic remains open. PR #4 stacks on PR #3; these abilities are not on main until that source is merged.

Use integration/run_warden_build.ps1 only with the separately prepared H:\DarkRelicWarden-20260912 project. It writes H:\DarkRelicWardenPackage and does not replace the existing character or enhanced releases. Human play/feel review is still required; automated checks do not establish submission readiness.
