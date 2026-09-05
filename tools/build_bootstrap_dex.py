#!/usr/bin/env python3
"""Bounded local bootstrap compilation; no game classes/resources are inputs."""
import argparse
from pathlib import Path
import subprocess
import os

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--java-home", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sources = sorted((root / "bootstrap/src/main/java").rglob("*.java"))
    android = args.sdk / "platforms/android-35/android.jar"
    d8 = args.sdk / "build-tools/35.0.0/lib/d8.jar"
    for path in [android, d8, *sources]:
        if not path.is_file(): raise SystemExit(f"missing input: {path}")
    if args.output.exists(): raise SystemExit("refusing to overwrite bootstrap output")
    classes = args.output / "classes"
    classes.mkdir(parents=True)
    executable_suffix = ".exe" if os.name == "nt" else ""
    subprocess.run([str(args.java_home / ("bin/javac" + executable_suffix)), "-J-Xmx256m",
                    "--release", "8", "-encoding", "UTF-8", "-classpath", str(android),
                    "-d", str(classes), *map(str, sources)], check=True)
    subprocess.run([str(args.java_home / ("bin/java" + executable_suffix)), "-Xmx384m", "-cp", str(d8),
                    "com.android.tools.r8.D8", "--release", "--min-api", "28",
                    "--lib", str(android), "--output", str(args.output),
                    *map(str, sorted(classes.rglob("*.class")))], check=True)
    print(args.output / "classes.dex")

if __name__ == "__main__": main()
