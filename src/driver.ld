OUTPUT_ARCH(arm)

SECTIONS
{
    .text.emulate_ACEROM_cartridge 0x08028100 : { *(.text.emulate_ACEROM_cartridge*) }
}
