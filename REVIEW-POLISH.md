# Dark Relic reviewer polish

This candidate applies the game-development persona recommendations after the operator accepted gameplay and audio in the preceding Enemy Feedback build.

- HUD warnings have dark backing and draw above the hit tint. Bellkeeper danger uses radial spikes, while the ward retains its smooth gold ring.
- Extraction prompts require Blackbell. Results show credits earned this run, available bank, Resolve cost, +20 health benefit and ownership. Only restart, eligible upgrade and quit remain on results.
- Hexbound shows a diamond cast tell only while its live windup has range and line of sight. A short cross-shaped impact follows successful damage. There is no projectile and no change to timing, damage or dodge rules.
- Procedural combat sounds use spatial attenuation once. Warning sounds can displace lower-priority cues at the 16-cue limit. Pain and terminal Warden voices survive routine exertion. Death and restart clear old cues.
- The prepared Widowfen map reduces incidental doorway lights, retains neutral hero fill and ward lanterns, moves selected non-colliding decoration away from the main fight, and adds shallow wet ground patches. No new marketplace dependency is introduced.

Sword timing and extraction duration are preserved following operator acceptance. Replay variation, additional weapons, multiplayer and the parked profiler-shutdown fault are outside this pass.

Validation: 169 portable checks passed. Unreal compilation and packaged presentation verification are pending. A separate candidate at H:\DarkRelicPolishPackage is being built; the accepted package and submission ZIP are protected by hash checks. Do not claim the new candidate is delivered until the terminal build and release receipts pass.

`integration/polish_review.py` edits only a copy of the already prepared enhanced map. It is not a blank-project installer. The Windows installer and verification scripts use project-specific candidate paths and fail on existing targets. Inspect receipts before retrying. Presentation capture uses an isolated save and deliberately staged states; those screenshots are not a human full-run recording.
