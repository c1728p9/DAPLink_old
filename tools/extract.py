import os
import shutil
import argparse

PROJECT_TO_SUFFIX = {
    "k20dx_k22f_if": "_legacy_0x8000",
    "k20dx_k64f_if": "_legacy_0x5000",
    "kl26z_microbit_if": "_legacy_0x8000",
    "lpc11u35_lpc812xpresso_if": "",
    "lpc11u35_lpc824xpresso_if": "",
    "lpc11u35_ssci1114_if": "",
    "lpc11u35_efm32gg_stk_if": "",
    "sam3u2c_nrf51dk_if": "_legacy_0x5000",
    "k20dx_k20dx_if": "_legacy_0x8000",
    "k20dx_k24f_if": "_legacy_0x8000",
    "k20dx_kl02z_if": "_legacy_0x8000",
    "k20dx_kl05z_if": "_legacy_0x8000",
    "k20dx_kl25z_if": "_legacy_0x8000",
    "k20dx_kl26z_if": "_legacy_0x8000",
    "k20dx_kl46z_if": "_legacy_0x8000",
    "sam3u2c_nrf51mkit_if": "_legacy_0x5000",
    "sam3u2c_nrf51dongle_if": "_legacy_0x5000",
    "lpc11u35_archble_if": "",
    "lpc11u35_archpro_if": "",
    "lpc11u35_archmax_if": "",
    "lpc11u35_hrm1017_if": "",
    "lpc11u35_sscity_if": "",
    "lpc11u35_ssci824_if": "",
    "k20dx_rbl_if": "_legacy_0x5000",
    "k20dx_rblnano_if": "_legacy_0x5000",
    #"lpc11u35_tiny_if": ""                     # Unsupported currently
    #"lpc11u35_c027_if": ""                     # Unsupported currently
}

def main():
    parser = argparse.ArgumentParser(description='Package a release for distribution')
    parser.add_argument('source', help='Release directory to grab files from')
    parser.add_argument('dest', help='Directory to create and place files in')
    args = parser.parse_args()

    proj_dir = args.source
    output_dir = args.dest
    build_number = "0240"

    os.mkdir(output_dir)
    for key, value in PROJECT_TO_SUFFIX.iteritems():
        source_path = os.path.join(proj_dir, key, key + "_crc" + value + ".bin")
        items = key.split('_')
        host_mcu = base_name = items[0]
        base_name = items[1]
        if len(value) > 0:
            offset = "_" + value.split('_')[-1]
        else:
            offset = "_0x0000"
        
        dest_path = os.path.join(output_dir, build_number + "_" + host_mcu + "_" + base_name + offset + ".bin")
        shutil.copyfile(source_path, dest_path)

if __name__ == "__main__":
    main()