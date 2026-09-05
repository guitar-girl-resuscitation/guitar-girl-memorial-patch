#!/usr/bin/env python3
"""Build only authored Patch runtime and pinned Dobby; never reads a game APK."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from release_artifacts import digest, verify_archive

ROOT = Path(__file__).resolve().parents[1]
SERVER_REPO = "guitar-girl-resuscitation/guitar-girl-memorial-server"

def run(*args):
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)

def fetch_server(tag, directory):
    if not tag or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-" for c in tag):
        raise SystemExit("invalid Server release tag")
    release = json.loads(subprocess.check_output(
        ["gh", "api", f"repos/{SERVER_REPO}/releases/tags/{tag}"], text=True))
    asset = next(a for a in release["assets"] if a["name"] == "ggfm-server-android-arm64.zip")
    archive = directory / asset["name"]
    # Download by immutable asset ID, not a moving tag URL.
    with archive.open("xb") as out:
        subprocess.run(["gh", "api", f"repos/{SERVER_REPO}/releases/assets/{asset['id']}",
                        "-H", "Accept: application/octet-stream"], stdout=out, check=True)
    github_digest = asset.get("digest", "")
    if github_digest != "sha256:" + digest(archive).lower():
        raise SystemExit("GitHub release asset digest missing/mismatched")
    metadata = verify_archive(archive)
    if metadata["kind"] != "server-android-arm64" or metadata["serverAbi"] != 1:
        raise SystemExit("unsupported Server artifact")
    if release["target_commitish"] != metadata["sourceCommit"]:
        raise SystemExit("release target differs from compiled source")
    with zipfile.ZipFile(archive) as z:
        for name in ("libggfm_server.so", "memorial-policy.json"):
            with (directory / name).open("xb") as stream:
                stream.write(z.read(name))
    return {
        "repository": SERVER_REPO, "releaseId": release["id"], "tag": tag,
        "sourceCommit": metadata["sourceCommit"], "serverAbi": 1,
        "archiveSha256": digest(archive), "sha256": digest(directory / "libggfm_server.so"),
        "policySha256": digest(directory / "memorial-policy.json"),
    }

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sdk", type=Path, required=True)
    p.add_argument("--java-home", type=Path, required=True)
    p.add_argument("--server-tag", default="nightly")
    p.add_argument("--output", type=Path, default=ROOT / "build/release-runtime")
    args = p.parse_args()
    work = args.output.resolve()
    if work.exists():
        raise SystemExit("refusing to overwrite runtime build directory")
    work.mkdir(parents=True)
    deps = json.loads((ROOT / "config/native-dependencies.json").read_text(encoding="utf-8"))
    ndk = args.sdk.resolve() / "ndk" / deps["androidNdk"]
    cmake_bin = args.sdk.resolve() / "cmake/3.22.1/bin"
    suffix = ".exe" if os.name == "nt" else ""
    cmake = cmake_bin / ("cmake" + suffix)
    ninja = cmake_bin / ("ninja" + suffix)
    toolchain = ndk / "build/cmake/android.toolchain.cmake"
    if not all(p.is_file() for p in (toolchain, cmake, ninja)):
        raise SystemExit("required NDK or SDK CMake 3.22.1 is missing")
    server = fetch_server(args.server_tag, work)
    if server["policySha256"] != digest(ROOT / "policy/memorial-policy.v1.json"):
        raise SystemExit("Server/Patch policy mismatch; update both deliberately")
    source = work / "dobby-source"
    run("git", "init", source)
    run("git", "-C", source, "remote", "add", "origin", deps["dobby"]["repository"])
    run("git", "-C", source, "fetch", "--depth", "1", "origin", deps["dobby"]["commit"])
    run("git", "-C", source, "checkout", "--detach", "FETCH_HEAD")
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != deps["dobby"]["commit"]:
        raise SystemExit("Dobby commit mismatch")
    common = ["-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
              f"-DCMAKE_MAKE_PROGRAM={ninja}",
              "-DCMAKE_SYSTEM_NAME=Android", "-DCMAKE_SYSTEM_PROCESSOR=aarch64",
              "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-23",
              "-DCMAKE_BUILD_TYPE=Release"]
    dobby_build = work / "dobby-build"
    run(cmake, "-S", source, "-B", dobby_build, *common,
        "-DDOBBY_GENERATE_SHARED=ON", "-DDOBBY_DEBUG=OFF",
        "-DDOBBY_BUILD_EXAMPLE=OFF", "-DDOBBY_BUILD_TEST=OFF")
    run(cmake, "--build", dobby_build, "--parallel", "2")
    dobby = dobby_build / "libdobby.so"
    native = work / "native"
    run(cmake, "-S", ROOT, "-B", native, *common,
        f"-DGGFM_DOBBY_LIBRARY={dobby}", f"-DGGFM_DOBBY_SHA256={digest(dobby)}",
        f"-DGGFM_SERVER_LIBRARY={work / 'libggfm_server.so'}",
        f"-DGGFM_SERVER_SHA256={server['sha256']}")
    run(cmake, "--build", native, "--target", "ggfm_bootstrap", "--parallel", "2")
    run(sys.executable, ROOT / "tools/build_bootstrap_dex.py",
        "--sdk", args.sdk.resolve(), "--java-home", args.java_home.resolve(),
        "--output", work / "dex")
    # All destinations are new members of the newly created runtime directory.
    shutil.copyfile(dobby, work / "libdobby.so")
    shutil.copyfile(native / "libggfm_bootstrap.so", work / "libggfm_bootstrap.so")
    shutil.copyfile(work / "dex/classes.dex", work / "classes.dex")
    license_file = next((source / n for n in ("LICENSE", "LICENSE.txt") if (source / n).is_file()), None)
    if license_file is None:
        raise SystemExit("Dobby license missing")
    shutil.copyfile(license_file, work / "DOBBY-LICENSE")
    with (work / "dependencies.json").open("x", encoding="utf-8") as stream:
        json.dump({"schema": 1, "server": server,
                   "dobby": {**deps["dobby"], "sha256": digest(dobby)},
                   "policySha256": server["policySha256"]}, stream, indent=2)
    print(f"Runtime built. Server source: {server['sourceCommit']}")

if __name__ == "__main__":
    main()
