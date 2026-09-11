# Dark Relic hackathon source kit

Source-only companion to the Unreal 5.8.1 solo PvE extraction game. The operator owns all Windows editor operations and final packaging. This repository does not contain a playable Unreal project or marketplace assets.

Canonical game brief: ../dark-fantasy-extraction-rpg/hackathon/TASK.md. Frozen Widowfen art and acquired assets remain in that project. Preserve third-person solo, one weapon, light/heavy attack, dodge/heal, two common enemy roles, one elite, three ordinary loot items, Blackbell Relic, extraction countdown, bank-on-success/lose-on-death, one upgrade and restart. No multiplayer, blockchain, new art purchases, or desktop automation.

The factory supplies a portable C++ gameplay core with tests and a thin Blueprint-callable Unreal plugin using that same core. This is an optional integration candidate until Windows UHT/compiler and packaged playtesting pass. Blueprint scene assembly, controls, animation notifies, collision traces, audio and UMG remain in the operator lane. Never silently enable a plugin in an existing project.

Do not mark full game or visual readiness Green from Linux-only tests.
