import os
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zipfile

release = "assets-v1"
models_names = ["bistro", "cerberus"]
default_repository = "Vovan675/RenderingEngine"

root_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
models_path = os.path.join(root_path, "Engine", "assets", "models")
download_temp_path = os.path.join(root_path, "download_temp")

# Get remote repository from git + default
repositories = []
try:
    remote = subprocess.run(["git", "-C", root_path, "remote", "get-url", "origin"], capture_output=True, text=True, check=True).stdout.strip()
    match = re.search(r"github\.com[:/]+([^/]+)/(.+?)(?:\.git)?$", remote)
    if match:
        repositories.append("%s/%s" % (match.group(1), match.group(2)))
except (OSError, subprocess.CalledProcessError):
    pass

if default_repository not in repositories:
    repositories.append(default_repository)

force = "--force" in sys.argv

# Download all assets
for model_name in models_names:
    target = os.path.join(models_path, model_name)
    if os.path.isdir(target) and os.listdir(target) and not force:
        print(model_name + ": already installed")
        continue

    response = None
    for repository in repositories:
        url = "https://github.com/%s/releases/download/%s/%s.zip" % (repository, release, model_name)
        try:
            response = urllib.request.urlopen(url)
            break
        except urllib.error.HTTPError as error:
            print("  %s -> HTTP %d" % (url, error.code))
    if response is None:
        raise SystemExit(model_name + ": not found in " + ", ".join(repositories))

    print(model_name + ": " + url)
    os.makedirs(download_temp_path, exist_ok=True)
    archive = os.path.join(download_temp_path, model_name + ".zip")
    total = int(response.headers.get("Content-Length", 0))
    done = 0
    started = time.monotonic()
    with response, open(archive, "wb") as target_file:
        while True:
            chunk = response.read(1 << 20)
            if not chunk:
                break
            target_file.write(chunk)
            done += len(chunk)
            elapsed = time.monotonic() - started
            speed = done / elapsed if elapsed > 0 else 0
            if total and speed > 0:
                print("\r  %5.1f%% of %.0f MB at %5.1f MB/s" % (done * 100.0 / total, total / (1024 * 1024), speed / (1024 * 1024)), end="")
    print()
    if done != total:
        os.remove(archive)
        raise SystemExit(model_name + ": failed download, downloaded %d of %d bytes" % (done, total))

    if os.path.isdir(target):
        shutil.rmtree(target)
    with zipfile.ZipFile(archive) as archive_file:
        archive_file.extractall(models_path)
    os.remove(archive)
    print(model_name + ": installed")
