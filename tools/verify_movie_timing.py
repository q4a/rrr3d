#!/usr/bin/env python3
"""Check the shipped AVI -> AVFoundation cache boundary, including B-frame PTS."""
import argparse
import concurrent.futures
import fractions
import json
import pathlib
import subprocess


def probe(path, frames=False):
    entries = "stream=codec_type,codec_name,avg_frame_rate,nb_frames,start_time,duration"
    if frames:
        entries += ":frame=best_effort_timestamp_time,media_type"
    command = ["ffprobe", "-v", "error", "-show_entries", entries, "-of", "json"]
    if frames:
        command += ["-select_streams", "v:0"]
    return json.loads(subprocess.check_output(command + [str(path)]))


def verify(pair):
    source, cache = pair
    original = probe(source)
    result = probe(cache)
    source_video = next(s for s in original["streams"] if s["codec_type"] == "video")
    video = next(s for s in result["streams"] if s["codec_type"] == "video")
    audio = next(s for s in result["streams"] if s["codec_type"] == "audio")
    assert video["codec_name"] == "h264" and audio["codec_name"] == "aac", cache
    assert fractions.Fraction(video["avg_frame_rate"]) == fractions.Fraction(source_video["avg_frame_rate"]), cache
    frames = probe(cache, True)["frames"]
    assert len(frames) == int(source_video["nb_frames"]), (cache, len(frames), source_video["nb_frames"])
    step = float(1 / fractions.Fraction(source_video["avg_frame_rate"]))
    for index, frame in enumerate(frames):
        assert "best_effort_timestamp_time" in frame, (cache, index, "missing PTS")
        pts = float(frame["best_effort_timestamp_time"])
        assert abs(pts - index * step) < 0.00001, (cache, index, pts, index * step)
    assert abs(float(audio["start_time"])) < 0.025, (cache, audio)
    return f"{cache.name}: {len(frames)} frames, {video['avg_frame_rate']} fps, ordered PTS, AAC start {audio['start_time']}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_directory", type=pathlib.Path)
    parser.add_argument("cache_directory", type=pathlib.Path)
    args = parser.parse_args()
    movies = sorted(args.cache_directory.glob("*.mp4"))
    if not movies:
        parser.error("no cached movies")
    pairs = [(args.source_directory / (p.stem + ".avi"), p) for p in movies]
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        for report in executor.map(verify, pairs):
            print(report, flush=True)
    print(f"Verified all {len(movies)} original movies without dropping/duplicating frames.")


if __name__ == "__main__":
    main()
