# ACE CDFJ driver

It allows games that use the CDFJ banking to be run on the PlusCart or the UnoCart. The custom code needs to be recompiled to the memory map of the STM32 MCU used by the PlusCart and UnoCart.

## Compilation

The ACE driver is built with the GNU Arm Embedded Toolchain version of GCC. Full version banner of the compiler below.

```
arm-none-eabi-gcc (Arm GNU Toolchain 12.2.Rel1 (Build arm-12.24)) 12.2.1 20221205
Copyright (C) 2022 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

Reference the `build.sh` file in the `ace/` directory for which compile time options were used.

The build process requires access to the [United Carts of Atari](https://github.com/Al-Nafuur/United-Carts-of-Atari) SDK. Download or `git clone` the repository into the `ace/` directory.

#### ACE Driver Versions

The ACE driver is versioned and any substantial change to the driver code *must* be accompanied by a change in the version number. The version number is in the driver asm file located at `ace/src/ace.asm`

The relevant section of the ace.asm file is. Please follow the directives in the commentary.

```
ACE-Driver:
; driver name should always begin with the string "CDJF " (including the space)
;
; it should also include a version string of the form "vX.Y" where X and Y are
; integers
;
; nothing else should be included in the driver name apart from whitespace
;
; finally, the driver name should be exactly string 16 chars wide exactly
;                "                "
			dc.b "CDJF v1.0       "
```

**NOTE**: The version string is `CDJF` and not `CDFJ` in order to distinguish it from CDFJ binaries compiled for the Harmony type cartridges. Failure to distinguish the two types of binary would likely result in confusion. 

## Custom Code

#### Compilation

The custom code should be built with the same version of the GNU Arm Embedded Toolchain.

#### Changes from the Original

The custom code of your CDFj project is mostly untouched. However, there are some significant changes.

The most significant change is to the address space the program operates with. The first group of addresses are found in the `custom.S` and `custom.boot.S` files. The table below summarises how address ranges are changed.

| Old Origin   | Old Memtop   | New Origin   | New Memtop   |
|--------------|--------------|--------------|--------------|
| `0x00000000` | `0x0fffffff` | `0x20000000` | `0x2000ffff` |
| `0x40000000` | `0x4fffffff` | `0x20010000` | `0x20017fff` |

Addresses found in the source code of the CDFJ project need to be changed in the same way.

Finally, there are changes to the `Makefile`. The target architecture has to be changed to `armv6-m` and the optimisation has to be set to `-Oz`. This is a relatively new flag to GCC and requests more aggressive size optimisation than `-Os`. Indeed, the principle reason for the GCC v12.0 requirement is access to the `-Oz` flag.

The GCC debugging flags have also been ammended. The singular `-g` flag has been replaced with `-g3 -gdwarf-4` `-gstrict-dwarf`. The debugging information isn't included in the final custom binary but it is included in the interim elf file. Also for debugging purposes the Makefile now also produces a `objdump` file.
