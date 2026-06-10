import os


def add_custom_arguments(parser):
    parser.add_argument(
        "--library",
        default=None,
        help="library name for the given function",
    )
    parser.add_argument(
        "--overlay",
        help="alias for --library",
    )

def apply(config, args):
    lib = args.library
    if not args.library:
        lib = args.overlay
    config["arch"] = "mipsel"
    config["objdump_executable"] = "mipsel-linux-gnu-objdump"
    config["baseimg"] = "build/psyq400_org.elf"
    config["myimg"] = "build/psyq400.elf"
    config["mapfile"] = "build/psyq400.map"
    config["source_directories"] = [
        f"src/{lib}",
    ]
