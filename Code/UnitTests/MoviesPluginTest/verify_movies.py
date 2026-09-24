"""End-to-end tests: requires MoviesPluginTest plus ffmpeg and ffprobe.

Example:
  python verify_movies.py --bin H:/ezEngine/Output/Bin/WinVs2022Shipping64 \
    --ffmpeg-bin H:/ffmpeg/bin --output H:/ezEngine/Workspace/MoviesTests
"""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--bin", required=True, type=Path)
parser.add_argument("--ffmpeg-bin", required=True, type=Path)
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("--vulkan", action="store_true")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
run_dir = Path(tempfile.mkdtemp(prefix="movies-", dir=args.output.resolve()))
project = run_dir / "project"
(project / "RuntimeConfigs").mkdir(parents=True)
(project / "ezProject").write_text("")
(project / "RuntimeConfigs/DataDirectories.ddl").write_text(
    'DataDir { string %Path{">sdk/Data/Base"} string %RootName{"base"} bool %Writable{false} }\n'
    'DataDir { string %Path{">project/"} string %RootName{"project"} bool %Writable{true} }\n')
exe = args.bin.resolve() / "MoviesPluginTest.exe"
ffmpeg = args.ffmpeg_bin.resolve() / "ffmpeg.exe"
ffprobe = args.ffmpeg_bin.resolve() / "ffprobe.exe"
if not exe.exists():
    exe = exe.with_suffix("")
if not ffmpeg.exists():
    ffmpeg = ffmpeg.with_suffix("")
    ffprobe = ffprobe.with_suffix("")


def command(output, encoder="mpeg4", renderer="DX11", stall=0, fmt="mp4", width=320, frames=31):
    return [str(exe), "-project", str(project), "-movie-output", str(output),
            "-movie-width", str(width), "-movie-height", "180", "-movie-fps", "29",
            "-movie-frames", str(frames), "-movie-encoder", encoder, "-movie-format", fmt,
            "-renderer", renderer, "-movie-test-stall", str(stall)]


def run(name, silent=False, **options):
    output = run_dir / name
    argv = command(output, **options)
    if silent:
        argv.append("-movie-test-silent")
    result = subprocess.run(argv, cwd=args.bin, capture_output=True, timeout=90)
    (run_dir / (name + ".process.log")).write_bytes(result.stdout + result.stderr)
    assert result.returncode == 0, f"{name}: exit {result.returncode}; see {output}.log"
    probe = json.loads(subprocess.check_output([str(ffprobe), "-v", "error", "-count_frames", "-show_streams", "-show_format", "-of", "json", str(output)]))
    (run_dir / (name + ".json")).write_text(json.dumps(probe, indent=2))
    video = next(s for s in probe["streams"] if s["codec_type"] == "video")
    audio = next(s for s in probe["streams"] if s["codec_type"] == "audio")
    assert (video["width"], video["height"]) == (320, 180)
    assert video["r_frame_rate"] == "29/1"
    assert int(video["nb_read_frames"]) == 31
    assert abs(float(probe["format"]["duration"]) - 31 / 29) < 0.002, probe
    assert audio["codec_name"] == ("pcm_f32le" if options.get("fmt") == "matroska" else "aac")
    pixels = subprocess.check_output([str(ffmpeg), "-v", "error", "-i", str(output), "-map", "0:v:0", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"])
    frame_size = 320 * 180 * 3
    hashes = [hashlib.sha256(pixels[i:i+frame_size]).hexdigest() for i in range(0, len(pixels), frame_size)]
    assert len(hashes) == 31
    assert len(set(hashes)) >= 10, "Moving object did not advance with simulation time"
    pcm = array.array("f", subprocess.check_output([str(ffmpeg), "-v", "error", "-i", str(output), "-map", "0:a:0", "-af", "pan=mono|c0=c0", "-f", "f32le", "pipe:1"]))
    sample_rate = int(audio["sample_rate"])
    assert abs(len(pcm) - 31 * sample_rate // 29) <= 1024, "Audio sample count drift"
    samples = pcm[2048:-1024]
    rms = math.sqrt(sum(x*x for x in samples) / len(samples))
    crossings = sum(a <= 0 < b for a, b in zip(samples, samples[1:]))
    frequency = crossings * sample_rate / len(samples)
    if silent:
        assert all(x == 0 for x in pcm), "Empty scene must produce silence"
        print(name, "31 frames with silent audio", flush=True)
        return hashes, hashlib.sha256(pcm.tobytes()).hexdigest()
    assert 0.12 < rms < 0.22, f"Audio missing or distorted: RMS {rms}"
    assert abs(frequency - 440) < 3, f"Wrong audio speed: {frequency} Hz"
    print(name, video["codec_name"], "31 frames, tone", round(frequency, 2), "Hz", flush=True)
    return hashes, hashlib.sha256(pcm.tobytes()).hexdigest()


run("silent.mp4", silent=True)
run("silent.mkv", silent=True, fmt="matroska")
baseline = run("baseline.mp4")
slow = run("slow.mp4", stall=25)
assert baseline == slow, "Render speed changed the movie frames or audio samples"
run("auto.mp4", encoder="auto")
run("matroska.mkv", fmt="matroska")
if args.vulkan:
    run("vulkan.mp4", renderer="Vulkan")
# Dynamic master controls must also use simulation time, without restarting device playback.
controlled = run_dir / "controls.mkv"
control_args = command(controlled, fmt="matroska", frames=32)
control_args[control_args.index("-movie-fps") + 1] = "32"
control_args.append("-movie-test-controls")
result = subprocess.run(control_args, cwd=args.bin, capture_output=True, timeout=90)
assert result.returncode == 0
pcm = array.array("f", subprocess.check_output([str(ffmpeg), "-v", "error", "-i", str(controlled), "-map", "0:a:0", "-af", "pan=mono|c0=c0", "-f", "f32le", "pipe:1"]))
quarter = len(pcm) // 4
levels = []
for i in range(4):
    samples = pcm[i*quarter+256:(i+1)*quarter-256]
    levels.append(math.sqrt(sum(x*x for x in samples) / len(samples)))
assert 0.07 < levels[0] < 0.10 and levels[1] < 0.0001 and levels[2] < 0.0001 and 0.15 < levels[3] < 0.20, levels
print("volume, mute, pause and resume:", levels, flush=True)
for name, options in [("invalid-size.mp4", {"width": 319}), ("invalid-encoder.mp4", {"encoder": "not-an-encoder"})]:
    result = subprocess.run(command(run_dir / name, **options), cwd=args.bin, capture_output=True, timeout=30)
    assert result.returncode != 0, f"{name}: invalid request was accepted"
    print(name, "correctly rejected", flush=True)
# A normal window close or cancellation must never report successful completion.
cancel = run_dir / "cancel.mp4"
process = subprocess.Popen(command(cancel, frames=2000, stall=30), cwd=args.bin, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline and not Path(str(cancel) + ".progress").exists():
        assert process.poll() is None, "Cancellation fixture exited early"
        time.sleep(0.1)
    assert Path(str(cancel) + ".progress").exists(), "Cancellation fixture did not start"
finally:
    process.terminate()
    process.wait(timeout=10)
assert process.returncode != 0
print("PASS. Results:", run_dir)
