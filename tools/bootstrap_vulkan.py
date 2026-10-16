#!/usr/bin/env python3
"""Install and run the checksum-pinned, repository-local Vulkan developer tools."""

import argparse
import hashlib
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import urllib.request


REPO = pathlib.Path(__file__).resolve().parents[1]
LOCK = REPO / "tools/vulkan.lock.json"
BASE = REPO / ".tools/vulkan"


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def fetch_package(package, cache, offline=False):
    """Return only a digest-verified cache entry; never publish partial downloads."""
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / (package["sha256"] + ".deb")
    if target.exists():
        if sha256(target) == package["sha256"]:
            return target
        if offline:
            raise ValueError("SHA256 mismatch in cached " + package["name"])
    if offline:
        raise ValueError("Missing offline package: " + package["name"])
    pending = target.with_suffix(".partial")
    try:
        print("Downloading " + package["name"], flush=True)
        with urllib.request.urlopen(package["url"], timeout=45) as response:
            with pending.open("wb") as stream:
                shutil.copyfileobj(response, stream)
        if sha256(pending) != package["sha256"]:
            raise ValueError("SHA256 mismatch downloading " + package["name"])
        pending.replace(target)
    finally:
        pending.unlink(missing_ok=True)
    return target


def tool_environment(root, original):
    """Add local headers/tools/layers without choosing or replacing a GPU driver."""
    env = dict(original)
    lib = root / "usr/lib/x86_64-linux-gnu"
    paths = {
        "PATH": [root / "usr/bin"],
        "CPATH": [root / "usr/include"],
        "LIBRARY_PATH": [lib],
        "LD_LIBRARY_PATH": [lib],
        "PKG_CONFIG_PATH": [lib / "pkgconfig"],
        "VK_LAYER_PATH": [root / "usr/share/vulkan/explicit_layer.d"],
    }
    for key, values in paths.items():
        prefix = os.pathsep.join(map(str, values))
        env[key] = prefix + (os.pathsep + env[key] if env.get(key) else "")
    return env


def install_root():
    return BASE / hashlib.sha256(LOCK.read_bytes()).hexdigest()[:16] / "root"


def installed(root):
    required = ("usr/bin/glslangValidator", "usr/bin/spirv-val", "usr/bin/vulkaninfo",
                "usr/include/vulkan/vulkan.h", "usr/lib/x86_64-linux-gnu/libvulkan.so",
                "usr/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json")
    return (root.parent / "complete").is_file() and all((root / p).exists() for p in required)


def bootstrap(offline):
    lock = json.loads(LOCK.read_text())
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
        raise ValueError("Pinned bootstrap supports Linux x86_64; use system tools on other hosts")
    distro = pathlib.Path("/etc/os-release").read_text()
    if 'ID=ubuntu\n' not in distro or 'VERSION_ID="24.04"' not in distro:
        raise ValueError("Pinned packages require Ubuntu 24.04; use system tools on other hosts")
    if not shutil.which("dpkg-deb"):
        raise ValueError("dpkg-deb is required to extract the local toolchain")
    root = install_root()
    if installed(root):
        print("Verified installation already available: " + str(root))
        return
    packages = [fetch_package(p, BASE / "packages", offline) for p in lock["packages"]]
    staging = root.parent / "root.staging"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    for package in packages:
        subprocess.run(["dpkg-deb", "-x", str(package), str(staging)], check=True)
    # Ubuntu's .pc file uses a system prefix; point only this extracted copy at its root.
    pc = staging / "usr/lib/x86_64-linux-gnu/pkgconfig/vulkan.pc"
    pc.write_text(pc.read_text().replace("prefix=/usr", "prefix=" + str(root / "usr")))
    if root.exists():
        shutil.rmtree(root)
    staging.replace(root)
    env = tool_environment(root, os.environ)
    # Compiler/version checks submit no GPU work.
    subprocess.run([str(root / "usr/bin/glslangValidator"), "--version"], env=env, check=True)
    subprocess.run([str(root / "usr/bin/spirv-val"), "--version"], env=env, check=True)
    (root.parent / "complete").write_text(LOCK.read_text())
    if not installed(root):
        raise ValueError("Toolchain installation is incomplete")
    print("Installed persistent Vulkan tools: " + str(root))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    setup = sub.add_parser("bootstrap")
    setup.add_argument("--offline", action="store_true")
    run = sub.add_parser("run")
    run.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.action == "bootstrap":
            bootstrap(args.offline)
            return 0
        root = install_root()
        if not installed(root):
            raise ValueError("Run make tools first to install the pinned repository toolchain")
        command = args.command
        if command[:1] == ["--"]:
            command = command[1:]
        if not command:
            raise ValueError("Supply a command, e.g. tools/with-vulkan make test")
        os.execvpe(command[0], command, tool_environment(root, os.environ))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("Vulkan toolchain: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
