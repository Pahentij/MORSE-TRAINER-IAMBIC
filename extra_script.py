Import("env")

from pathlib import Path
import subprocess
import sys


def merge_firmware(source, target, env):
    """
    Run merge_bin_esp.py after PlatformIO has created firmware.bin.

    ESP32-C3 flash layout:
      0x0000  bootloader.bin
      0x8000  partitions.bin
      0x10000 firmware.bin
    """

    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))

    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"

    merger = project_dir / "merge_bin_esp.py"

    release_dir = project_dir / "release"
    output_name = "MORSE_TRAINER_IAMBIC_lolin_c3_mini_merged.bin"

    required = {
        "bootloader.bin": bootloader,
        "partitions.bin": partitions,
        "firmware.bin": firmware,
        "merge_bin_esp.py": merger,
    }

    missing = [
        name
        for name, path in required.items()
        if not path.is_file()
    ]

    if missing:
        details = "\n".join(
            "  {} -> {}".format(name, path)
            for name, path in required.items()
        )

        raise RuntimeError(
            "Cannot create merged firmware.\n"
            "Missing: "
            + ", ".join(missing)
            + "\n\nExpected files:\n"
            + details
        )

    cmd = [
        sys.executable,
        str(merger),

        "--output_name",
        output_name,

        "--output_folder",
        str(release_dir),

        "--bin_path",
        str(bootloader),
        str(partitions),
        str(firmware),

        "--bin_address",
        "0x0000",
        "0x8000",
        "0x10000",
    ]

    print("")
    print("============================================================")
    print("Creating merged ESP32-C3 firmware")
    print("============================================================")
    print("Bootloader : " + str(bootloader))
    print("Partitions : " + str(partitions))
    print("Firmware   : " + str(firmware))
    print("Output     : " + str(release_dir / output_name))
    print("============================================================")

    result = subprocess.run(
        cmd,
        cwd=str(project_dir)
    )

    if result.returncode != 0:
        raise RuntimeError(
            "merge_bin_esp.py failed."
        )

    print("")
    print("============================================================")
    print("Merged firmware created successfully:")
    print(str(release_dir / output_name))
    print("Flash address: 0x0000")
    print("============================================================")
    print("")


# Execute the merger after PlatformIO has built firmware.bin.
env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.bin",
    merge_firmware
)