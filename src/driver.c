#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "flash.h"
#include "cartridge_firmware.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx.h"
#include "cartridge_emulation.h"
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include "cartridge_emulation.h"
#include "cartridge_firmware.h"
#include "global.h"



int emulate_ACEROM_cartridge()
{
	// ROM address, clock, and function pointers passed as parameters at the
	// beginning of the buffer memory
	uint32_t* buffer32 = (uint32_t*)0x20000000;

	uint8_t* download = (uint8_t*)*buffer32;
	buffer32++;
	//uint8_t* ccm8 = (uint8_t*)*buffer32;
	buffer32++;
	bool (*reboot_into_cartridge_ptr)() =(bool(*)())(uint32_t)*buffer32;
	buffer32++;
	void (*ReturnVector)() = (void(*)())(uint32_t)*buffer32;
	buffer32++;
	uint32_t PassedSystemCoreClock = (uint32_t)*buffer32;
	//buffer32++;
	//uint32_t PluscartVersion = (uint32_t)*buffer32;
	
	buffer32 = (uint32_t*)0x20000000;
	uint8_t* buffer8 = (uint8_t*)0x20000000;

    uint32_t* ccm32 = (uint32_t*)0x10000000;
    uint8_t* ccm8 = (uint8_t*)0x10000000;
    uint32_t* data32 = (uint32_t*)0x10000800;
    uint8_t* data8 = (uint8_t*)0x10000800;

	// copy buffer and preserving the lowest 16bytes of the buffer
	memcpy(buffer8+0x8000, buffer8+0x1c, 0x10);
	memcpy(buffer8, download+0x100, 0x8000); // 32k and skipping the ACE header
	memcpy(buffer8+0x1c,buffer8+0x8000, 0x10);

	// entry point into the custom program. this should be the _sboot() function
    uint32_t custom_entry_point = 0x20000809;

	// --- now the CDF driver ---

    uint16_t addr = 0, addr_prev = 0, addr_prev2 = 0 ;
    uint8_t data = 0, data_prev = 0;

	// catridge starting at bank 0. skipping driver and custom program
	uint8_t *cartridge = buffer8 + 0x1000;

	// currently selected bank. bank 6 on startup
	uint8_t *bankPtr = cartridge + 0x6000;

   if (!((bool (*)())reboot_into_cartridge_ptr)()) return 1;
    __disable_irq();	// Disable interrupts
						
	bool fast_fetch = false, sample_mode = false;

	while (1) { 
		// wait for stable address
		while (((addr = ADDR_IN) != addr_prev) || (addr != addr_prev2)) {
			   addr_prev2 = addr_prev;
			   addr_prev = addr;
		}

		if (addr & 0x1000) { 
			// bank-switch
			if (addr >= 0x1ff4 && addr <= 0x1ffb) {
				if (addr == 0x1ff4) {
					bankPtr = &cartridge[0x6000];
				} else {
					bankPtr = &cartridge[(addr - 0x1ff5) * 0x1000 ];
				}
				// CDFJ+ requires a bit more fiddling
			}

			#define DSCOMM 32 // datastream used for DSPTR and DSWRITE
			#define DSJMP 33 // datastream used for fast_jmp
			#define FETCHER_SHIFT 20 // 16 for CDFJ+
			#define FETCHER_MASK 0xf0000000 // 0xff000000 for CDFJ+
			#define FETCHER_BASE  0x98 // very early versions of CDF had different fetcher base 

			if (addr == 0x1ff0) {
				// DSWRITE
				while (ADDR_IN == addr);
				data = DATA_IN;

				// index into CCM holding the location of the stream
				int idx = (FETCHER_BASE + (DSCOMM * 4)) >> 2;
				uint32_t v = ccm32[idx];

				// write to stream
				data8[v >> FETCHER_SHIFT] = data;

				// advance address value
				v += 1 << FETCHER_SHIFT;
				ccm32[idx] = v;

			} else if (addr == 0x1ff1) {
				// DSPTR
				while (ADDR_IN == addr);
				data = DATA_IN;

				// index into CCM holding the pointer
				int idx = (FETCHER_BASE + (DSCOMM * 4)) >> 2;

				// read and update pointer value
				uint32_t v = ccm32[idx];
				v <<= 8;
				v &= FETCHER_MASK;
				v |= (uint32_t)data << FETCHER_SHIFT;

				// write new pointer value
				ccm32[idx] = v;

			} else if (addr == 0x1ff2) {
				// SETMODE
				while (ADDR_IN == addr);
				data = DATA_IN;

				fast_fetch = (data&0x0f) != 0x0f;
				sample_mode = (data&0xf0) != 0xf0;

			} else if (addr == 0x1ff3 || addr == 0x1ff4) {
				// CALLFN
				while (ADDR_IN == addr);
				data = DATA_IN;

				addr_prev = ADDR_IN;
				DATA_OUT = 0xea; // NOP
				SET_DATA_MODE_OUT;

				switch(data){
					case 254: 
						break;
					default:
						((int (*)())custom_entry_point)();
						break;
				}

				addr = ADDR_IN;
				while (ADDR_IN == addr);

				addr = ADDR_IN;
				DATA_OUT = 0x4c; // JMP
				SET_DATA_MODE_OUT;
				while (ADDR_IN == addr);

				addr = ADDR_IN;
				DATA_OUT = (uint8_t)(addr_prev & 0x00ff);	// low byte of JMP addr
				SET_DATA_MODE_OUT;
				while (ADDR_IN == addr);

				addr = ADDR_IN;
				DATA_OUT = (uint8_t)(addr_prev >> 8); // high byte of JMP addr
				SET_DATA_MODE_OUT;
				addr_prev = addr;
				while (ADDR_IN == addr);
				SET_DATA_MODE_IN;

			} else {
				#define LDA_IMMEDIATE 0xa9
				#define JMP_ABSOLUTE  0x4c
				#define FASTJMP_MASK 0xfe // very early CDF ROMs had a FASTJMP_MASK of 0xff
				#define DATASTREAM_OFFSET 0 // offset is differnt for CDFJ+ ROMs
				#define INCREMENT_BASE 0x0124 // very early versions of CDF had different increment bases
				#define AMPLITUDE_REGISTER 35 // very early versions of CDF used register 34
				#define INCREMENT_SHIFT 12 // CDFJ+ has an increment shift of 8

				// get opcode
				uint8_t output = bankPtr[addr&0x0fff];

				if (fast_fetch) {
					if (output == LDA_IMMEDIATE) {
						// fast fetch

						// output opcode 
						DATA_OUT = output;
						SET_DATA_MODE_OUT;
						while (ADDR_IN == addr);
						addr = ADDR_IN;
						SET_DATA_MODE_IN;

						// retrieve operand from bank
						output = bankPtr[addr&0x0fff];

						// if operand indicates that the data is a fast fetcha
						// then do replacement 
						if (output >= DATASTREAM_OFFSET && output <= DATASTREAM_OFFSET+DSCOMM) {
							// get address and increment for stream
							int reg = (int)(output - DATASTREAM_OFFSET);
							int dsIdx = (FETCHER_BASE + (reg * 4)) >> 2;
							uint32_t ds = ccm32[dsIdx];
							int incIdx = (INCREMENT_BASE + (reg * 4)) >> 2;
							uint32_t inc = ccm32[incIdx];
							
							// get data
							int dataIdx = (int)(ds >> FETCHER_SHIFT);
							output = data8[dataIdx];

							// write update address
							ds += (inc << INCREMENT_SHIFT);
							ccm32[dsIdx] = ds;
						} else if (data == AMPLITUDE_REGISTER) {
							// * not in collect demo
						}

					} else if (output == JMP_ABSOLUTE) {
						// output opcode 
						DATA_OUT = output;
						SET_DATA_MODE_OUT;
						while (ADDR_IN == addr);
						addr = ADDR_IN;
						SET_DATA_MODE_IN;

						// retrieve operand from bank
						output = bankPtr[addr&0x0fff];
						uint8_t output2 = bankPtr[(addr+1)&0x0fff];

						if ((output&FASTJMP_MASK) == 0x00 && output2 == 0x00) {
							// jmp index depends on register
							int jmpIdx = (FETCHER_BASE + ((int)output * 4)) >> 2;
							uint32_t jmp = ccm32[jmpIdx];

							// read first operand
							int dataIdx = jmp >> FETCHER_SHIFT;
							output = data8[dataIdx];
							jmp += 1 << FETCHER_SHIFT;
							ccm32[jmpIdx] = jmp;

							// output first operand
							DATA_OUT = output;
							SET_DATA_MODE_OUT;
							while (ADDR_IN == addr);
							SET_DATA_MODE_IN;

							// read second operand 
							dataIdx = jmp >> FETCHER_SHIFT;
							output = data8[dataIdx];
							jmp += 1 << FETCHER_SHIFT;
							ccm32[jmpIdx] = jmp;
						}

					} else {
						// * CDFJ+ should also implement ldx immediate and
						// ldy immediate instructions
					}
				}
			
				DATA_OUT = output;
				SET_DATA_MODE_OUT;
				while (ADDR_IN == addr);
				SET_DATA_MODE_IN;
			}
		}
	}

	__enable_irq();

	DATA_OUT = 0xea; // NOP or data for SWCHB
	SET_DATA_MODE_OUT;
	while (ADDR_IN == addr);

	addr = ADDR_IN;
	DATA_OUT = 0x00; // BRK
	while (ADDR_IN == addr);

	((void (*)())ReturnVector)();

	return 0;
}
