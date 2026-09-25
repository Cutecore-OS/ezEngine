# RmlUi Video Plugin

Adds a silent `<video>` element to the existing RmlUi integration. The same RML document works with **RmlUI Canvas 2D** and **RmlUI Canvas 3D**. No engine or RmlUiPlugin sources need modification.

The plugin uses ezEngine data directories, threads, resource management and mutable GAL textures. FFmpeg supplies the missing video demuxer/decoder. There are no Windows-specific runtime APIs; the code is intended for desktop platforms supported by both ezEngine/RmlUi and FFmpeg.

## Build

Provide a **shared FFmpeg 6 or newer development SDK** with `libavformat`, `libavcodec`, `libavutil` and `libswscale`. An FFmpeg command-line executable alone is insufficient. Select a build with the decoders required by your content (for example H.264 and VP8).

Configure your existing engine build with:

```sh
cmake -S . -B <build-directory> -DEZ_BUILD_RMLUI=ON -DEZ_BUILD_RMLUI_VIDEO=ON -DEZ_RMLUI_VIDEO_FFMPEG_ROOT=<ffmpeg-sdk>
cmake --build <build-directory> --config Shipping --target RmlUiVideoPlugin RmlUiVideoPluginTest
```

On Linux, install the development packages for the four libraries, or set the SDK root. With system development packages the root may be omitted. On Windows, the root must contain `include/libavformat`, `lib/avformat.lib` and `bin/avformat-*.dll` (and the other libraries). DLLs from that SDK are copied beside the plugin.

The option defaults to OFF so projects that do not use video do not acquire a new dependency. When enabled, the plugin is included in the editor's runtime dependencies.

FFmpeg is supplied by the application, not vendored into the engine. Obtain binaries/source from the providers linked at [FFmpeg downloads](https://ffmpeg.org/download.html). Preserve the supplied license notices and meet the distribution requirements of the FFmpeg build you choose. The code was built and tested with a shared LGPL FFmpeg 8.1 SDK.

## Use in a project

1. Build the plugin and enable **RML UI Video** in the project's plugin selection. The bundle requires **RML UI**.
2. Place the video in a project data directory, for example `UI/Movies/intro.mp4`.
3. Reference it from an RML document. Create a normal RmlUi asset pointing to that document and assign it to either Canvas component.

```html
<rml>
<head>
  <style>
    body { margin: 0; }
    video { display: block; width: 640px; height: 360px; }
  </style>
</head>
<body>
  <video id="intro" src="Movies/intro.mp4" autoplay="" loop=""></video>
</body>
</rml>
```

Paths follow the existing RmlUi file interface: a valid data-directory path is used directly, otherwise it is resolved relative to the RML document. Loose files and mounted ezArchive data directories are supported. Archive backwards seeks reopen and skip through the stream; large seek-heavy movies perform best as loose files.

Keep **On Demand Update** enabled. The element requests updates during loading, playback, seeking and queued GPU uploads, and stops requesting them when paused or finished. CSS sizing, opacity, transforms and clipping use the regular RmlUi renderer. Natural video dimensions are available after the first frame is decoded; `width` and `height` attributes may override them.

The existing RmlUi asset exporter collects literal `src` references, so video files referenced this way are packaged with the RML asset. If sources are assigned dynamically, explicitly include those files in the project export. Include poster images explicitly as well: the existing exporter scans `src`, not `poster`.

For a custom application that does not consume plugin bundles, load `ezRmlUiVideoPlugin` through `ezPlugin::LoadPlugin` before loading UI documents.

## Attributes and control

| Attribute | Behavior |
| --- | --- |
| `src` | Video path. Changing or removing it closes the old decoder and releases its texture. |
| `autoplay` | Start playing when loaded. Without it, display the first frame and pause. |
| `loop` | Seek to the beginning when playback finishes. |
| `paused` | Pause while present; removing it resumes playback. Suitable for RmlUi `data-attr-paused` bindings. |
| `poster` | Optional image displayed until the first video frame is ready. |
| `width`, `height` | Intrinsic size overrides. CSS may override these. |

Boolean attributes use HTML presence semantics: `paused="false"` is still paused. Remove the attribute to resume.

Include `<RmlUiVideoPlugin/VideoElement.h>` and use RmlUi's RTTI to access the C++ interface:

```cpp
auto* pElement = pDocument->GetElementById("intro");
auto* pVideo = pElement ? rmlui_dynamic_cast<ezRmlUiVideoElement*>(pElement) : nullptr;
if (pVideo != nullptr)
{
  pVideo->Play();
  pVideo->Pause();
  pVideo->Seek(2.5); // seconds; works while paused
}
```

`IsPaused()`, `HasEnded()` and `GetCurrentTime()` expose playback state. Commands before the first update are retained. Call the API on the normal RmlUi update thread, under the same context synchronization used for other RmlUi operations.

The element emits `loadeddata`, `seeked`, `ended` and `error` events. You can use RmlUi listeners or the existing Canvas event-message facility. This is an engine video element, not a full browser media implementation: there are no built-in controls, audio, subtitles, network streams or HDR tone mapping. Playback uses the RmlUi system clock (the engine global clock). SDR video up to 8192 pixels per dimension is accepted. Rotation metadata and non-square sample aspect ratios are not applied.

Each element has its own decoder thread and playback state. Decoding, seeking and pixel conversion run off the UI/render threads; the mailbox retains only the newest due frame. GPU storage is reused for successive frames. Decoder threads are joined when elements are destroyed or their sources change. Destroy UI documents before unloading the runtime plugin, as with other custom RmlUi element plugins.

## Verification

```sh
RmlUiVideoPluginTest -nogui -nosave
RmlUiVideoPluginTest -nogui -nosave -renderer Vulkan
```

Tests decode the small generated MP4/WebM fixtures, check colors, seek backwards, drain delayed frames, restart at EOF, run independent instances, reject invalid media, and decode through an archive data directory.

The GPU test creates contexts using both actual Canvas component types and their shared on-demand update path. It renders using the engine RmlUi renderer, reads pixels back from render targets, and checks pause, seek, source replacement, EOF/restart and looping. It does not require a sample project's asset cache.

Verified locally on Windows / DirectX 11. The local Vulkan runtime could not initialize because `VK_KHR_surface` was unavailable. Linux execution was not available in the development environment.
