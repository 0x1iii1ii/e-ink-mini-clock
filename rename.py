Import("env")
import shutil, os


def rename_firmware(source, target, env):
    src = str(target[0])
    project_root = env.subst("$PROJECT_DIR")
    bin_folder = os.path.join(project_root, "bin")
    os.makedirs(bin_folder, exist_ok=True)
    dst = os.path.join(bin_folder, "eink_esp32c3_4mb_fw_v0.0.0.bin")
    # print(">>> Source:", src)
    # print(">>> Dest:  ", dst)
    shutil.copy(src, dst)
    print(">>> Exported binary to:", dst)


firmware = env.File("$BUILD_DIR/${PROGNAME}.bin")
env.AlwaysBuild(firmware)
env.AddPostAction(firmware, rename_firmware)
