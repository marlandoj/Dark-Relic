# Warden body glow

Fury now lights the Warden's animated skeletal mesh with an additive crimson-and-gold material overlay. The separate sphere and eight orbiting ember meshes are removed. The original armor materials remain in place beneath the glow. The production Fury timer controls the existing charge and fade curve; expiry, death, extraction completion, restart and encounter teardown restore the previous overlay. Local light intensity is reduced from 850 to 350.

The six-second ability, cooldown, damage bonuses, animation, voice and Realistic Widowfen scene remain unchanged.

## Integration

Use the verified `H:\DarkRelicRealistic-20260913` project as the baseline. Stage the changed plugin source and `integration` scripts beneath `H:\DarkRelicBodyGlowDelivery-20260913`. Exit the running game before starting the build.

Run `integration/run_body_glow_build.ps1` from that delivery. It copies the baseline into `H:\DarkRelicBodyGlow-20260913`, compiles the editor plugin, creates the skeletal-mesh-compatible material, binds it in the realistic map, runs gameplay checks and captures charge, peak, fade and off states. The driver preserves existing releases and refuses to reuse an existing receipt without inspection. Use `-PrepareOnly` to copy, compile and bind while an existing packaged game remains open, then `-ResumeRuntime` once it closes.

Inspect all four captures and the DX11 material log before writing a passing `visual-review.json` in the delivery directory. Then run the same driver with `-Package`. The separate output is `H:\DarkRelicBodyGlowPackage`. Review packaged captures and normal Escape exit before creating a shortcut or reporting deployment complete.

## Current verification

169 portable gameplay/presentation checks pass. Python binding syntax and Git whitespace checks pass. Unreal compilation, editor rendering, packaged gameplay and deployment are pending. This source revision is not a verified Windows release.
