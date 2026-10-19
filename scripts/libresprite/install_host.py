"""Install the exact tested official Linux AppImage without system mutation."""
import hashlib
from pathlib import Path
import subprocess
import urllib.request

VERSION = '1.3'
SHA256 = 'fd90bddda8135fe00f4a9a9c8c9ebd48f9b07e8782ab269be5c9efb1a851af30'
URL = 'https://github.com/LibreSprite/LibreSprite/releases/download/v1.3/libresprite-v1.3-linux-x86_64.AppImage'


def install(directory):
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    image = directory / 'libresprite-v1.3.AppImage'
    if not image.exists():
        urllib.request.urlretrieve(URL, image)
    if hashlib.sha256(image.read_bytes()).hexdigest() != SHA256:
        raise ValueError('Official LibreSprite v1.3 AppImage SHA256 mismatch')
    image.chmod(0o755)
    subprocess.run([str(image), '--appimage-extract'], cwd=directory, check=True, stdout=subprocess.DEVNULL)
    executable = directory / 'squashfs-root/AppRun'
    subprocess.run([str(executable), '--version'], check=True)
    return executable


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', nargs='?',
                        default=str(Path(__file__).resolve().parents[2] / '.tools/libresprite'))
    print(install(parser.parse_args().directory))
