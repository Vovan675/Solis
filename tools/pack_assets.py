import os
import zipfile

archives = ["bistro", "cerberus"]

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
models = os.path.join(root, "Engine", "assets", "models")
download_temp = os.path.join(root, "download_temp")

os.makedirs(download_temp, exist_ok=True)
packed = []

for name in archives:
    folder = os.path.join(models, name)
    if not os.path.isdir(folder):
        raise SystemExit(name + ": " + folder + " does not exist")

    archive = os.path.join(download_temp, name + ".zip")
    source_bytes = 0
    count = 0
    print(name + ": packing")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as archive_file:
        for directory, directories, files in os.walk(folder):
            directories[:] = sorted(entry for entry in directories if entry != ".runtimes")
            for file_name in sorted(files):
                path = os.path.join(directory, file_name)
                archive_file.write(path, os.path.relpath(path, models))
                source_bytes += os.path.getsize(path)
                count += 1

    packed.append(archive)
    print("  %d files, %.0f MB -> %.0f MB" % (count, source_bytes / (1024 * 1024), os.path.getsize(archive) / (1024 * 1024)))
for archive in packed:
    print("  " + archive)
