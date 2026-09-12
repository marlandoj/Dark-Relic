# Final-day enhancement candidate

Scope authorized September 12, 2026; tracked as ZOU-1634. This branch preserves the merged gameplay core, asset bindings, loot, save slot and extraction rules. Replay variation remains stretch and is not implemented.

- Neutral moon fill, softer directional shadows, reduced fog/cyan, warm route lanterns and eight non-colliding props from the installed CC0 environment kit.
- Landed-hit sparks and layered synthesized impacts; short camera impulse with **M** to toggle it off. Existing controls remain available.
- Extraction bells accelerate from 1.8 to 0.45 seconds between pulses; gold ward rings, rising lines and a three-tone escape chord celebrate completion.
- Original procedural swamp wind/insect texture, movement-driven footsteps and role-specific warning cues. No recordings, downloaded audio or model-generated media are added. Procedural audio components have explicit bounded lifetimes and teardown cleanup.
- Bellkeeper area strike: a fixed 360 cm radius, 1.6-second warning, line-of-sight and vertical range checks. Below half health, enrage reduces warning to 1.25 seconds, speeds movement and reduces cooldown. Area damage rises from 26 to 34. Moving outside, cover or timed dodge prevents damage. Regular hits do not cancel the area telegraph; killing the boss does.

Ground effects are projected by the shipped HUD and remain visible through foreground geometry to prioritize warnings. They are intentionally stylized indicators. They do not use debug-only rendering.

Portable tests invoke the same Bellkeeper timing/range class used by the encounter. The expanded Unreal smoke must exercise area avoidance, dodge protection, one-hit damage, enrage, impacts, extraction and persistence. Source checks alone do not establish packaged readiness.

Protected release: existing character package and shortcut. New candidate uses a separate Windows project, enhanced map and package output. Human start-to-extraction playtest, audio/feel review and performance acceptance remain required. Profiler teardown diagnosis has a 30-minute budget; no engine change or release replacement is implied.
