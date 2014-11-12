/*
 * Copyright (c) 2014, ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2014, Hisilicon Ltd.
 * Copyright (c) 2014, Linaro Ltd.
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

#ifndef __LCB_DEF_H__
#define __LCB_DEF_H__

/* The size of DDR RAM is 1GB. */
#define DRAM_BASE			0x00000000
#define DRAM_SIZE			0x40000000

#define DRAM_SEC_SIZE			0x07000000
#define DRAM_SEC_BASE			(DRAM_BASE + DRAM_SIZE - DRAM_SEC_SIZE)

#define DRAM_NS_BASE			DRAM_BASE
#define DRAM_NS_SIZE			(DRAM_SIZE - DRAM_SEC_SIZE)

/*******************************************************************************
 * GIC-400 & interrupt handling related constants
 ******************************************************************************/
#define GICD_BASE			0xF6801000
#define GICC_BASE			0xF6802000
#define GICH_BASE			0xF6803000
#define GICV_BASE			0xF6804000

/*******************************************************************************
 * PL011 related constants
 ******************************************************************************/
#define PL011_UART0_BASE		0xF8015000

#define PL011_BAUDRATE			115200

#define PL011_UART0_CLK_IN_HZ		19200000

#endif /* __LCB_DEF_H__ */