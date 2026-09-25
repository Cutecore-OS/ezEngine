# Movies / Render Movie

An optional editor/runtime plugin that renders a scene to a movie with a fixed simulation step and synchronous MiniAudio sound. No engine or existing plugin source files are modified.

## Build

Requires the engine editor dependencies, MiniAudio, and an FFmpeg development SDK with `avcodec`, `avformat`, `avutil`, and `swscale`. The implementation uses the modern FFmpeg channel-layout API; tested with FFmpeg 8.1. Hardware encoders must also be enabled in the FFmpeg build and supported by the installed driver. Respect the licenses of the FFmpeg distribution you ship.

From the SDK root, for example:

```powershell
cmake --preset vs2022x64 -DEZ_BUILD_MOVIES_PLUGIN=ON -DEZ_3RDPARTY_MINIAUDIO_SUPPORT=ON -DEZ_MOVIES_FFMPEG_ROOT=C:/SDK/ffmpeg
cmake --build Workspace/vs2022x64 --config Shipping --target EditorPluginMovies MoviesPlugin Player
```

`EZ_BUILD_MOVIES_PLUGIN` defaults to OFF so a normal engine build does not require FFmpeg. The Windows build copies FFmpeg DLLs from the SDK `bin` directory beside the plugin. Other platforms must provide the corresponding shared libraries on their loader search path. Build the targets and their dependencies from the same engine revision/configuration.

## Use

1. Enable **Movies** and **MiniAudio** in the project's plugin selection and reopen the project if requested. FMOD is not supported by this exporter; existing FMOD sound components must be migrated to MiniAudio if their sound is required.
2. Open a scene, stop editor simulation, and configure its active game camera and cinematic playback to start when the scene starts.
3. Click the clapperboard **Render Movie** button between **Play the Game** and **Export and Run**.
4. Set resolution/aspect preset or custom dimensions, integer FPS, video bitrate, MP4/MKV format, encoder, and track duration. The dialog shows the exact resulting frame count and duration. Fractional frame durations round to the nearest whole frame (at least one).
5. Choose a new output filename. Assets and the scene are exported through the existing editor APIs, then a separate Player performs the render. Use **Cancel** to stop it.

MP4 contains AAC audio. MKV contains float PCM audio without compression, avoiding codec priming delay. Auto encoding tries NVIDIA H.264, AMD H.264, Intel H.264, software H.264 (when present), then software MPEG-4. An explicitly selected encoder fails if unavailable. The actual codec and runtime diagnostics are saved in `<movie>.log`.

A completed temporary file is moved to the destination only after successful finalization. Existing files are never replaced. Cancelled/failed exports are not published. If the destination becomes unavailable after a successful render, the completed temporary file is preserved and its location is shown.

## Timing and performance

- Uses the engine's main view, offscreen GAL render target, render graph transfer pass, and GPU readback helper. Resolution is independent of the monitor and the small Player window.
- The global/world clocks advance at `1 / FPS`. Loading-screen frames are excluded. All frames are rendered; slow hardware changes export time, not movie speed.
- MiniAudio device playback is stopped and its callback is isolated from the node graph while stopped (and restored during shutdown). Even a later device restart cannot consume movie samples. Its node graph supplies an exact integer number of PCM samples per frame. Integer cumulative sample boundaries prevent drift at FPS values such as 29. Master volume, mute, pause, resume, and normal sound cleanup continue to work.
- Full-quality textures and blocking resource acquisition avoid streaming placeholders in the output. Resource errors stop the export and are reported in the log.
- VSync and the renderer's one-frame update/render overlap are disabled for capture. Device rendering and supported video encoding use the GPU. RGBA-to-YUV conversion still uses the CPU and frames pass through GPU staging memory: this is **not a zero-copy GPU encoder**. Memory use is bounded independently of movie duration.
- Standard input actions are suppressed during capture. Runtime settings changed by the plugin are restored before shutdown. Custom game code must use engine clocks rather than wall-clock time to remain deterministic.

## Scope

Captures the active `ezGameState` main view from a fresh scene start, including its game UI. It is not an editor viewport recording, timeline authoring tool, or multi-camera batch exporter. Audio capture currently requires MiniAudio; it does not record desktop audio, microphones, or FMOD. HDR output, fractional FPS, arbitrary timeline seeking, and multi-view/XR capture are not implemented.

DX11 and NVIDIA NVENC were verified on Windows. The local Vulkan renderer crashed during application startup both with and without movie capture; Vulkan export is not verified in this environment. AMD and Intel encoders are selectable but were not available for hardware testing here.

## Verification

Build the self-contained audiovisual fixture:

```powershell
cmake --build Workspace/vs2022x64 --config Shipping --target MoviesPluginTest
python Code/UnitTests/MoviesPluginTest/verify_movies.py --bin Output/Bin/WinVs2022Shipping64 --ffmpeg-bin C:/SDK/ffmpeg/bin --output Workspace/MoviesVerification
```

The script checks exact frame counts, duration, visible motion, a decoded 440 Hz tone, identical frames with deliberate rendering stalls, automatic hardware encoding, both containers, master audio controls, invalid dimensions/encoders, and cancellation. Pass `--vulkan` to exercise Vulkan on a working Vulkan installation. Each run uses a new temporary fixture project and preserves its reports/movies in the output directory.

For automation, the runtime plugin also accepts `-movie-output <absolute path>`, `-movie-width`, `-movie-height`, `-movie-fps`, `-movie-frames`, `-movie-bitrate` (bits/second), `-movie-format` (`mp4` or `matroska`) and `-movie-encoder` (`auto`, `h264_nvenc`, `h264_amf`, `h264_qsv`, `libx264`, `mpeg4`). These are passed together with the normal Player project/scene/profile arguments. Direct CLI callers should also use a temporary output path and publish only after a zero exit code.
