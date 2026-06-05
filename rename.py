Import("env")
import shutil, os


def rename_firmware(source, target, env):
    src = str(target[0])
    project_root = env.subst("$PROJECT_DIR")
    bin_folder = os.path.join(project_root, "bin")
    os.makedirs(bin_folder, exist_ok=True)

    fw_version = env.GetProjectOption("build_flags")
    version = "unknown"
    for f in fw_version:
        if "FW_VERSION" in f:
            version = f.split("=")[1].replace('"', "").strip()
            version = version.replace("'", "")
            break

    filename = f"eink_esp32c3_4mb_fw_v{version}.bin"
    dst = os.path.join(bin_folder, filename)

    if not os.path.isfile(src):
        print(f">>> ERROR: source not found: {src}")
        return

    shutil.copy(src, dst)
    print(f">>> Exported binary to: {dst}")


firmware = env.File("$BUILD_DIR/${PROGNAME}.bin")
env.AlwaysBuild(firmware)
env.AddPostAction(firmware, rename_firmware)
