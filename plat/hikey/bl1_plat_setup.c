/*
 * Copyright (c) 2014-2015, Linaro Ltd and Contributors. All rights reserved.
 * Copyright (c) 2014-2015, Hisilicon Ltd and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <cci400.h>
#include <console.h>
#include <debug.h>
#include <errno.h>
#include <gpio.h>
#include <hi6220.h>
#include <hi6553.h>
#include <mmio.h>
#include <partitions.h>
#include <platform.h>
#include <platform_def.h>
#include <sp804_timer.h>
#include <string.h>
#include "../../bl1/bl1_private.h"
#include "hikey_def.h"
#include "hikey_private.h"

/*******************************************************************************
 * Declarations of linker defined symbols which will help us find the layout
 * of trusted RAM
 ******************************************************************************/
extern unsigned long __COHERENT_RAM_START__;
extern unsigned long __COHERENT_RAM_END__;

/*
 * The next 2 constants identify the extents of the coherent memory region.
 * These addresses are used by the MMU setup code and therefore they must be
 * page-aligned.  It is the responsibility of the linker script to ensure that
 * __COHERENT_RAM_START__ and __COHERENT_RAM_END__ linker symbols refer to
 * page-aligned addresses.
 */
#define BL1_COHERENT_RAM_BASE (unsigned long)(&__COHERENT_RAM_START__)
#define BL1_COHERENT_RAM_LIMIT (unsigned long)(&__COHERENT_RAM_END__)

/* Data structure which holds the extents of the trusted RAM for BL1 */
static meminfo_t bl1_tzram_layout;

static void hi6220_pmussi_init(void);
static void hikey_gpio_init(void);
static void hikey_hi6553_init(void);
static int query_boot_mode(void);

meminfo_t *bl1_plat_sec_mem_layout(void)
{
	return &bl1_tzram_layout;
}

/*******************************************************************************
 * Perform any BL1 specific platform actions.
 ******************************************************************************/
void bl1_early_platform_setup(void)
{
	const size_t bl1_size = BL1_RAM_LIMIT - BL1_RAM_BASE;

	/* Initialize the console to provide early debug support */
	console_init(PL011_UART0_BASE, PL011_UART0_CLK_IN_HZ, PL011_BAUDRATE);

	hi6220_timer_init();
	/*
	 * Enable CCI-400 for this cluster. No need for locks as no other cpu is
	 * active at the moment
	 */
	cci_init(CCI400_BASE,
		 CCI400_SL_IFACE3_CLUSTER_IX,
		 CCI400_SL_IFACE4_CLUSTER_IX);
	cci_enable_cluster_coherency(read_mpidr());

	/* Allow BL1 to see the whole Trusted RAM */
	bl1_tzram_layout.total_base = BL1_RW_BASE;
	bl1_tzram_layout.total_size = BL1_RW_SIZE;

	/* Calculate how much RAM BL1 is using and how much remains free */
	bl1_tzram_layout.free_base = BL1_RW_BASE;
	bl1_tzram_layout.free_size = BL1_RW_SIZE;
	reserve_mem(&bl1_tzram_layout.free_base,
		    &bl1_tzram_layout.free_size,
		    BL1_RAM_BASE,
		    bl1_size);

	INFO("BL1: 0x%lx - 0x%lx [size = %u]\n", BL1_RAM_BASE, BL1_RAM_LIMIT,
	     bl1_size);
}

/*******************************************************************************
 * Perform the very early platform specific architecture setup here. At the
 * moment this only does basic initialization. Later architectural setup
 * (bl1_arch_setup()) does not do anything platform specific.
 ******************************************************************************/
void bl1_plat_arch_setup(void)
{
	configure_mmu_el3(bl1_tzram_layout.total_base,
			  bl1_tzram_layout.total_size,
			  BL1_RO_BASE,
			  BL1_RO_LIMIT,
			  BL1_COHERENT_RAM_BASE,
			  BL1_COHERENT_RAM_LIMIT);
}

/*******************************************************************************
 * Function which will perform any remaining platform-specific setup that can
 * occur after the MMU and data cache have been enabled.
 ******************************************************************************/
void bl1_platform_setup(void)
{
	hikey_gpio_init();
	hi6220_pmussi_init();
	hikey_hi6553_init();
	hi6220_pll_init();

	io_setup();
	get_partition();
	if (query_boot_mode()) {
		flush_loader_image();
		usb_download();
	}
}

/* Get the boot mode (normal boot/usb download/uart download) */
static int query_boot_mode(void)
{
	int boot_mode;

	boot_mode = mmio_read_32(ONCHIPROM_PARAM_BASE);
	if ((boot_mode < 0) || (boot_mode > 2)) {
		NOTICE("Invalid boot mode is found:%d\n", boot_mode);
		panic();
	}
	return boot_mode;
}

/* PMU SSI is the device that could map external PMU register to IO */
static void hi6220_pmussi_init(void)
{
	uint32_t data;

	/*
	 * After reset, PMUSSI stays in reset mode.
	 * Now make it out of reset.
	 */
	mmio_write_32(AO_SC_PERIPH_RSTDIS4,
		AO_SC_PERIPH_RSTDIS4_PRESET_PMUSSI_N);
	do {
		data = mmio_read_32(AO_SC_PERIPH_RSTSTAT4);
	} while (data & AO_SC_PERIPH_RSTDIS4_PRESET_PMUSSI_N);

	/* set PMU SSI clock latency for read operation */
	data = mmio_read_32(AO_SC_MCU_SUBSYS_CTRL3);
	data &= ~AO_SC_MCU_SUBSYS_CTRL3_RCLK_MASK;
	data |= AO_SC_MCU_SUBSYS_CTRL3_RCLK_3;
	mmio_write_32(AO_SC_MCU_SUBSYS_CTRL3, data);

	/* enable PMUSSI clock */
	data = AO_SC_PERIPH_CLKEN5_PCLK_PMUSSI_CCPU |
	       AO_SC_PERIPH_CLKEN5_PCLK_PMUSSI_MCU;
	mmio_write_32(AO_SC_PERIPH_CLKEN5, data);
	data = AO_SC_PERIPH_CLKEN4_PCLK_PMUSSI;
	mmio_write_32(AO_SC_PERIPH_CLKEN4, data);

	/* output high on gpio0 */
	gpio_direction_output(0);
	gpio_set_value(0, 1);
}

static void hikey_hi6553_init(void)
{
	int data;

	hi6553_write_8(PERI_EN_MARK, 0x1e);
	hi6553_write_8(NP_REG_ADJ1, 0);
	data = DISABLE6_XO_CLK_CONN | DISABLE6_XO_CLK_NFC |
		DISABLE6_XO_CLK_RF1 | DISABLE6_XO_CLK_RF2;
	hi6553_write_8(DISABLE6_XO_CLK, data);

	/* configure BUCK0 & BUCK1 */
	hi6553_write_8(BUCK01_CTRL2, 0x5e);
	hi6553_write_8(BUCK0_CTRL7, 0x10);
	hi6553_write_8(BUCK1_CTRL7, 0x10);
	hi6553_write_8(BUCK0_CTRL5, 0x1e);
	hi6553_write_8(BUCK1_CTRL5, 0x1e);
	hi6553_write_8(BUCK0_CTRL1, 0xfc);
	hi6553_write_8(BUCK1_CTRL1, 0xfc);

	/* configure BUCK2 */
	hi6553_write_8(BUCK2_REG1, 0x4f);
	hi6553_write_8(BUCK2_REG5, 0x99);
	hi6553_write_8(BUCK2_REG6, 0x45);

	/* configure BUCK3 */
	hi6553_write_8(BUCK3_REG3, 0x02);
	hi6553_write_8(BUCK3_REG5, 0x99);
	hi6553_write_8(BUCK3_REG6, 0x41);

	/* configure BUCK4 */
	hi6553_write_8(BUCK4_REG2, 0x9a);
	hi6553_write_8(BUCK4_REG5, 0x99);
	hi6553_write_8(BUCK4_REG6, 0x45);

	/* configure LDO20 */
	hi6553_write_8(LDO20_REG_ADJ, 0x50);

	hi6553_write_8(NP_REG_CHG, 0x0f);
	hi6553_write_8(CLK_TOP0, 0x06);
	hi6553_write_8(CLK_TOP3, 0xc0);
	hi6553_write_8(CLK_TOP4, 0x00);

	/* select 32.764KHz */
	hi6553_write_8(CLK19M2_600_586_EN, 0x01);
}

static void hikey_gpio_init(void)
{
	gpio_register_device(GPIO0_BASE);
	gpio_register_device(GPIO1_BASE);
	gpio_register_device(GPIO2_BASE);
	gpio_register_device(GPIO3_BASE);
	gpio_register_device(GPIO4_BASE);
	gpio_register_device(GPIO5_BASE);
	gpio_register_device(GPIO6_BASE);
	gpio_register_device(GPIO7_BASE);
	gpio_register_device(GPIO8_BASE);
	gpio_register_device(GPIO9_BASE);
	gpio_register_device(GPIO10_BASE);
	gpio_register_device(GPIO11_BASE);
	gpio_register_device(GPIO12_BASE);
	gpio_register_device(GPIO13_BASE);
	gpio_register_device(GPIO14_BASE);
	gpio_register_device(GPIO15_BASE);
	gpio_register_device(GPIO16_BASE);
	gpio_register_device(GPIO17_BASE);
	gpio_register_device(GPIO18_BASE);
	gpio_register_device(GPIO19_BASE);

	/* Power on indicator LED (User LED0). */
	gpio_direction_output(32);
	gpio_set_value(32, 1);
	gpio_direction_output(33);
	gpio_direction_output(34);
	gpio_direction_output(35);
}

/*******************************************************************************
 * Before calling this function BL2 is loaded in memory and its entrypoint
 * is set by load_image. This is a placeholder for the platform to change
 * the entrypoint of BL2 and set SPSR and security state.
 * On Juno we are only setting the security state, entrypoint
 ******************************************************************************/
void bl1_plat_set_bl2_ep_info(image_info_t *bl2_image,
			      entry_point_info_t *bl2_ep)
{
	SET_SECURITY_STATE(bl2_ep->h.attr, SECURE);
	bl2_ep->spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
}
