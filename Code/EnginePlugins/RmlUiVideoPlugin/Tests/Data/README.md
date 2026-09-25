# Video fixtures

These synthetic fixtures contain one second of solid red followed by one second of solid blue, at 96x64 pixels and 10 fps. They contain no third-party media.

Generate the H.264 fixture using an FFmpeg build with OpenH264:

```sh
ffmpeg -f lavfi -i "color=c=red:s=96x64:r=10:d=1" -f lavfi -i "color=c=blue:s=96x64:r=10:d=1" -filter_complex "[0:v][1:v]concat=n=2:v=1:a=0" -c:v libopenh264 -g 10 colors.mp4
ffmpeg -i colors.mp4 -c:v libvpx colors.webm
ffmpeg -i colors.mp4 -c:v mpeg4 -bf 2 -g 100 colors-bframes.mp4
```

`invalid.mp4` is deliberately invalid text for the decoder-error test. The archive test generates its own compressed archive in the test output directory.

`video.rml` is a minimal example that can be copied into a project together with `colors.mp4` and assigned through a regular RmlUi asset to either Canvas.
