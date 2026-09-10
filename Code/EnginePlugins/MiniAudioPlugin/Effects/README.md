# MiniAudio effects and groups

`Mini Audio Sound`, sound assets, zones and the listener expose an **Effects**
array. Add an entry, choose **Type**, and edit its settings. Entries can be
reordered, disabled and deleted; unrelated settings are hidden in the editor.

## Groups and routing

A sound asset supplies its default **Group**. The single **Group** on a
`Mini Audio Sound` component overrides it; reset the component property to its
empty default to inherit again. An empty asset group resolves to `Default`.
Group names use the existing editable `MiniAudioSoundGroups` list everywhere.
Two sound components on the same prefab object can use different groups.
Detached sounds retain their group, and a live component's override can change
while the sound is playing.

Zones have **IncludeGroups** and **ExcludeGroups** arrays. Empty IncludeGroups
accepts all groups; otherwise the sound's effective group must be listed.
ExcludeGroups always wins. These filters only control that zone's effects.
Object/component tags no longer participate. Old binary component versions are
readable, but old tag filters must be replaced with the intended group selections.

Processing order is asset effects, component effects, zone effects (ascending
Priority, then component handle), then listener effects on the mixed group bus.
Effects within each chain follow array order. Speed, Pitch and TimeStretch are
source controls, combined before spatialization regardless of their list position.
Other personal/zone effects process each voice after spatialization. Listener
entries select one Group; an empty listener Group processes all group buses.
Group volume is applied before listener processing; ducking follows listener
processing and precedes master volume/mute. Up to 64 distinct group buses can be
created per audio-engine session.

## Effects

| Type | Settings |
| --- | --- |
| Reverb | Mix, Decay (RT60 seconds), RoomSize, Damping |
| Speed | Speed 0.1–4; changes both tempo and pitch |
| Pitch | Pitch -24–24 semitones; preserves duration |
| TimeStretch | Speed 0.1–4; changes tempo while preserving pitch |
| Volume | Volume 0–4 multiplier; 1 is unchanged |
| Delay | Mix, Delay 10–5000 ms, Feedback |
| ParametricEqualizer | Mix, Frequency Hz, Q, Gain dB |
| Chorus / Flanger | Mix, Rate Hz, Depth, Feedback |
| HighPass / LowPass / BandPass | Mix, Frequency Hz, Q |
| Compressor | Threshold -60–0 dBFS, Ratio 1–20, Attack/Release ms |
| Limiter | Threshold -60–0 dBFS, Release ms; immediate peak attack |
| Distortion | Mix, Drive 1–50; normalized soft saturation |
| Bitcrusher | Mix, Bits 1–16, Downsample 1–64 (sample hold) |
| Panner | Pan -1 left to +1 right; stereo balance, center unchanged |
| Muffling | Mix, Frequency Hz, Q, Volume; low-pass plus attenuation |

Mix, Feedback, RoomSize, Damping and Depth use 0–1 values. Feedback is limited to
0.98 for finite decay. Filters clamp frequency below device Nyquist. Dynamics use
linked channels to preserve the stereo image. Panner leaves mono unchanged and
balances the first two channels of multichannel output. Volume, dynamics and
Panner apply fully at weight one and do not use Mix.

TimeStretch uses streaming waveform-similarity overlap-add (WSOLA), with linked
channels, 20 ms windows and 10 ms hops. At tempo one it bypasses to the decoder.
Extreme stretching can smear transients; this is not a studio offline algorithm.
Pitch combines this duration correction with resampling. Combined source pitch
is clamped to 0.01–100 and the stretch ratio to 0.025–16. Existing component/asset
Pitch and global game-speed controls retain their previous resampling behavior.

## Listener zones

Add **Mini Audio Effect Box** or **Mini Audio Effect Sphere** under Sound/MiniAudio.
Edit Extents (full box dimensions) or Radius, or drag the size gizmo. Rotation and
nonuniform/negative object scale are supported; a scaled sphere is an ellipsoid.
Degenerate dimensions disable the zone.

**Falloff** is the transition width inside the boundary, in local units. Weight
is zero at the boundary and reaches one at Falloff units inside. Falloff 0 gives
a hard boundary. Radius 5 / Falloff 1 gives full effect within radius 4. DSP zone
weights additionally transition over 50 ms; source tempo/pitch controls follow
the world-update weights.

Enable **InterpolateByTime** on a Box or Sphere to replace the Falloff field with
**InterpolationDuration** (seconds, default 1). The geometric boundary then sets
the target to one inside and zero outside. The current weight follows that target
using the same exponential interpolation as Volume Sampler: one duration covers
90% of the remaining difference, two cover 99%. Entry and exit both interpolate;
reversing direction continues from the current weight. Zero duration is immediate.
Time follows the world's simulation clock. The transition progresses even while
no sounds play, and all voices share the zone's current listener weight. Existing
zones keep distance-based Falloff because InterpolateByTime defaults to false.

Zones use the MiniAudio listener position, including the editor camera override.
For game playback place a Mini Audio Listener on the camera. Only zones in the
source's world contribute; the source itself need not be inside the zone.

## Sound Box / Sphere

**Mini Audio Sound Box** and **Mini Audio Sound Sphere** inherit all Mini Audio
Sound properties, including Group, asset effects, personal Effects, random file
selection, playback controls and completion actions. Their volume restricts only
their own sound; it does not apply effects to other sources.

Extents gives full box dimensions; Sphere uses Radius. Falloff and the optional
InterpolateByTime / InterpolationDuration work like the effect volumes described
above. Inside/outside tests use the listener position. Outside the volume the
source becomes inaudible after the chosen transition, but playback continues so
loops resume at their current position on re-entry. Per-voice delay/reverb tails
are gated too. Already mixed listener/group reverb has its own shared tail and
cannot be separated back into individual sources. A short 50 ms DSP gain ramp
avoids clicks when the sampled weight changes abruptly.

**CaptureOffset** is the local position of the emitter, with its own translation
gizmo like a Reflection Probe. Object rotation and scale transform this offset;
the volume stays centered on the object. The sound asset's positional setting and
distance attenuation still apply, so enable positional audio for 3D placement.

## Muffling and occlusion

**Muffling** is a low-pass effect with Frequency, Q, Mix and Volume. For a muted
sound behind a wall, try Frequency 800 Hz, Q 0.707, Mix 1 and Volume 0.25. It can
be used in an asset, a sound, an effect zone or a listener group effect.

**UseOcclusion** is available on ordinary Sound as well as Sound Box/Sphere. It
reveals these controls:

- **OcclusionThreshold**: blocked fraction before attenuation begins (0�1, default 0.5).
- **OcclusionCollisionLayer**: the normal physics collision-layer picker.
- **OcclusionRadius**: world-space source sphere radius in meters (default 1).
  Inside and on this sphere, occlusion is always zero and no rays are cast.
  Outside, occlusion blends in smoothly over one additional radius, reaching its
  full strength at twice the radius. Zero selects a single point-to-point ray.
  Sampling uses the listener-facing part of a fixed 32-point Fibonacci sphere,
  weighted by facing direction, and a small listener aperture (at most 0.2 m).
  With UseOcclusion enabled, click the OcclusionRadius property label to activate
  its sphere gizmo, as with a Point Light's Range. Dragging supports undo/cancel.
  For Sound Box/Sphere it is centered on CaptureOffset. The radius stays in world
  meters even under nonuniform owner scale. It affects occlusion, not spatial
  attenuation or the size of the Sound Box/Sphere playback zone.
- **OcclusionRange**: maximum source-listener distance for occlusion in meters
  (default 50). Attenuation fades over the last 20% of that range, then resets to
  zero and stops raycasts. Zero disables occlusion. This does not limit the sound's
  audible range; asset distance attenuation and Sound Box/Sphere still control it.

Static/dynamic colliders in the chosen layer participate; triggers do not. Hits
must be finite and strictly between sampled endpoints. The source object's own
actor/shape and ancestors are ignored. Box/Sphere use CaptureOffset. Detached
one-shots query their fixed playback position, not their component's new position.
If that component is deleted, its last occlusion value is cleared.

Stationary sources refresh at 20 Hz. Significant listener/source movement,
changes to radius/range/layer, and re-entry into range invalidate the cached query
immediately. The blocked-ray fraction is remapped as
`(blocked - threshold)/(1 - threshold)` and clamped to 0�1. Threshold 1 disables
attenuation. Full occlusion applies an 800 Hz low-pass and 25% gain; partial
occlusion blends that with the clear signal. Coverage changes smaller than 4%
are ignored; temporal smoothing reaches 90% of increased occlusion in 300 ms and
90% of decreased occlusion in 150 ms. DSP weights also transition smoothly.
Entering the protected sphere or leaving OcclusionRange clears occlusion immediately.
A grille needs a collider with actual openings; a solid collider represents a wall.
Without a physics module the sound stays unoccluded. This is geometric occlusion,
not material-dependent transmission or physical diffraction. The selected layer
must interact with the intended wall colliders.

## Ducker

Enable **Ducker** on the listener, then add entries to **Duckers**. Each entry
has its own Enabled flag, SourceGroups and TargetGroups arrays. Empty arrays do
not match any groups. Settings are:

- **Threshold**: 0–100 percent of full-scale peak amplitude; 10 means 0.1.
- **Reduction**: 0–100 percent removed; 80 leaves 20 percent of target volume.
- **AttackTime**: 0–2000 ms to reach the reduction after a source exceeds threshold.
- **ReleaseTime**: 0–10000 ms to return to full volume once it falls below threshold.

For dialogue over music, use SourceGroups `[Voice]`, TargetGroups `[Music]`,
Threshold 10, Reduction 80, AttackTime 10 and ReleaseTime 250. Multiple rules are
supported; the strongest attenuation wins when their targets overlap. Detection
uses actual group output before any ducking, including source/group gains and
effects. Silent or muted sources do not trigger it. Peaks are detected per audio
block, with sample-by-sample linear attack/release. Rules cannot feed back through
other rules' gain reductions.

## Sound assets and lifetime

**RandomWithoutRepeats** shuffles all file entries, plays each once, then starts a
new shuffled cycle without repeating at the boundary. State is shared by voices
using that resource. One file naturally always selects that file; identical files
listed twice are still separate entries. Without the flag, selection remains
independent random choice. Asset effects are copied to each voice before playback,
including autoplay from dynamically instantiated prefabs and one-shot playback.

Natural source completion preserves personal/zone effect tails. Group effects
keep their own tails independently. Stop/fade completion and world shutdown free
source state. Long feedback delays intentionally retain long tails and buffers.
Resources stay referenced for as long as their decoder reads the source bytes.

WAV and MP3 use MiniAudio's built-in decoders. Ogg Vorbis is expanded once to float
PCM during resource loading by the plugin-local decoder. Previously transformed
Ogg assets also work without rewriting project files or the MiniAudio library.
Expanded Ogg uses more memory while loaded. MP3 stretching seeks within compressed
data and may cost more CPU than PCM. Decoder failures are logged.

Only the existing MiniAudio runtime/editor bundle is needed. The former separate
AudioEcho bundle is superseded; its components are not automatically converted.
After updating binaries, restart the editor/engine process and transform edited
sound assets to include their new settings.

## Validation

Build `MiniAudioPluginTest` with `EZ_BUILD_UNITTESTS`. It renders the real MiniAudio
node graph and tests stereo, group routing, delays/tails, all new DSP types,
listener bus effects and deactivation, group override/inheritance, zone falloff,
component/prefab serialization, shuffled selection, ducking thresholds and timing,
and measured frequency/duration for Speed, Pitch and TimeStretch.
