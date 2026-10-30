/*
 * Copyright (c) 2026 Razvan Lazar
 */

#ifndef _E1000_REGS_H_
#define _E1000_REGS_H_

/**
 * Registers.
 */
/*****************************************************************************/

// generic
#define E1000_CTRL						0x0000
#define E1000_STATUS					0x0008


/*****************************************************************************/

// interrupts
#define E1000_ICR						0x00C0	// Interrupt Cause
#define E1000_IMS						0x00D0	// Interrupt Mask Set
#define E1000_IMC						0x00D8	// Interrupt Mask Clear

/*****************************************************************************/

// rx ring
#define E1000_RCTL						0x0100	// rx control
#define E1000_RDBAL						0x2800	// ring iova (low)
#define E1000_RDBAH						0x2804	// ring iova (high)
#define E1000_RDLEN						0x2808	// ring size (bytes)
#define E1000_RDH						0x2810	// head (NIC moves it)
#define E1000_RDT						0x2818	// tail (software moves it)
#define E1000_MTA						0x5200	// mcast table


/*****************************************************************************/

/**
 * Reg values.
 */
// generic values
#define E1000_STATUS_FD					(1 << 0)	// full duplex
#define E1000_STATUS_LU					(1 << 1)	// link up

#define E1000_CTRL_SLU					(1 << 6)	// Set Link Up
#define E1000_CTRL_LRST					(1 << 3)	// Link Reset
#define E1000_CTRL_RST					(1 << 26)	// Global device Reset


/*****************************************************************************/

// rx control values
#define E1000_RCTL_EN					(1 << 1)	// enable rx
#define E1000_RCTL_UPE					(1 << 3)	// accept all unicast
#define E1000_RCTL_MPE					(1 << 4)	// accept multicast
#define E1000_RCTL_LPE					(1 << 5)	// accept jumbo frames
#define E1000_RCTL_BAM					(1 << 15)	// accept broadcast
#define E1000_RCTL_BSIZE				((1 << 16) | (1 << 17))	// buf size
#define E1000_RCTL_BSEX					(1 << 25)	// buffer size * 16
#define E1000_RCTL_SECRC				(1 << 26)	// strip the CRC


/*****************************************************************************/

#endif	// _E1000_REGS_H_
