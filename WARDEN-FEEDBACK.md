# Warden hit and voice feedback

The feedback candidate adds a 55 cm recoil over 0.16 seconds when an enemy lands a damaging hit. It moves away from the attacker or Bellkeeper area center, sweeps the capsule against walls, and adds no upward launch. Dodge immunity prevents pain audio and recoil; a successful dodge cancels any remaining recoil. Damage, combo timing, stamina, Fury mitigation and persistence rules remain unchanged.

Warden uses the installed Paragon Greystone recordings for light strikes, heavy strikes and Sunder, dodges, Relic Burst, Fury, healing effort and recovery, pain, heavy pain, death and extraction celebration. One dedicated audio component replaces the previous vocal on each accepted action. Rapid pain sounds have a 0.18-second guard; each vocal has a five-second maximum. Rejected actions remain silent. Restart and shutdown clear voice playback and recoil.

`integration/bind_warden_feedback.py` verifies the 11 sound assets and persists hard references on the encounter in the enhanced map. These references make the cooker include the cue dependencies. Third-party voice source assets stay in the user's Unreal project, outside this public source repository. This change uses the existing Greystone asset entitlement; it does not resolve outstanding submission-license receipt verification.

Build with `integration/run_feedback_build.ps1` in the separately staged `H:\DarkRelicFeedback-20260912` project. Its output is `H:\DarkRelicFeedbackPackage`; prior packages are preserved. The script is specific to the prepared Windows project and installed engine.

Delivered September 12, 2026: **Dark Relic Warden Feedback** desktop shortcut. Validation: 146 portable checks, Unreal compilation, 11 saved/reloaded voice bindings, 66 rendered audio-enabled editor checks and 66 packaged checks pass. All 11 vocal event types report active playback. A normal launch exits through Escape with code 0. Ten installed source hashes match and all 21 protected prior-package hashes are unchanged. Human listening and combat feel remain final acceptance checks; automated playback does not certify subjective voice balance.
