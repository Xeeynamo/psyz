from .android import Android
from .native import Native, Wine
from .n64 import N64Emu
from .nds import NdsEmu
from .ps1 import Ps1Emu, Ps1Hw
from .psp import PspEmu, PspHw

DRIVERS = {
    "native": Native,
    "wine": Wine,
    "psp-emu": PspEmu,
    "psp-hw": PspHw,
    "ps1-emu": Ps1Emu,
    "ps1-hw": Ps1Hw,
    "nds-emu": NdsEmu,
    "n64-emu": N64Emu,
    "android": Android,
}
