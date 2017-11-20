/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Meterfs - STM32L1x SPI routines
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/threads.h>
#include <sys/msg.h>
#include <sys/pwman.h>
#include <sys/interrupt.h>
#include <unistd.h>

#include "spi.h"


typedef struct {
	int mask;
	int state;
} __attribute__((packed)) gpioset_t;


typedef struct {
	char pin;
	char mode;
	char af;
	char otype;
	char ospeed;
	char pupd;
} __attribute__((packed)) gpioconfig_t;


typedef struct {
	char pin;
	char state;
	char edge;
} __attribute__((packed)) gpiointerrupt_t;


typedef struct {
	int len;
} __attribute__((packed)) gpiodelay_t;


typedef struct {
	char type;
	int port;

	union {
		gpioset_t set;
		gpioconfig_t config;
		gpiointerrupt_t interrupt;
		gpiodelay_t delay;
	};
} __attribute__((packed)) gpiomsg_t;


struct {
	volatile unsigned int *base;
	volatile unsigned int *rcc;

	volatile char spi_ready;
	handle_t mutex;
	handle_t cond;

	unsigned int gpio;
} spi_common;


enum { cr1 = 0, cr2, sr, dr, crcpr, rxcrcr, txcrcr, i2scfgr, i2spr };


enum { rcc_apb2enr = 8 };


enum { GPIO_CONFIG, GPIO_INTERRUPT, GPIO_GET, GPIO_SET, GPIO_DELAY };


enum { GPIOA = 0, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH };


static int spi_irqHandler(unsigned int n, void *arg)
{
	*(spi_common.base + cr2) &= ~(1 << 7);
	spi_common.spi_ready = 1;

	return spi_common.cond;
}


static void gpio_pinSet(int port, int pin, int state)
{
	gpiomsg_t devctl;

	devctl.port = port;
	devctl.type = GPIO_SET;
	devctl.set.mask = 1 << pin;
	devctl.set.state = !!state << pin;

	send(spi_common.gpio, DEVCTL, &devctl, sizeof(devctl), NORMAL, NULL, 0);
}


static void gpio_pinConfig(int port, char pin, char mode, char af, char ospeed, char otype, char pupd)
{
	gpiomsg_t devctl;

	devctl.port = port;
	devctl.type = GPIO_CONFIG;
	devctl.config.pin = pin;
	devctl.config.mode = mode;
	devctl.config.af = af;
	devctl.config.otype = otype;
	devctl.config.ospeed = ospeed;
	devctl.config.pupd = pupd;

	send(spi_common.gpio, DEVCTL, &devctl, sizeof(devctl), NORMAL, NULL, 0);
}


static void spi_powerCtrl(int state)
{
	if (!state)
		gpio_pinSet(GPIOE, 12, 1);

	gpio_pinSet(GPIOA, 4, state);

	if (state) {
		usleep(1000);
		gpio_pinSet(GPIOE, 12, 0);
	}
}


static unsigned char spi_readwrite(unsigned char txd)
{
	unsigned char rxd;

	mutexLock(spi_common.mutex);
	spi_common.spi_ready = 0;

	/* Initiate transmission */
	*(spi_common.base + dr) = txd;
	*(spi_common.base + cr2) |= 1 << 7;

	while (!spi_common.spi_ready)
		condWait(spi_common.cond, spi_common.mutex, 0);

	rxd = *(spi_common.base + dr);
	mutexUnlock(spi_common.mutex);

	return rxd;
}


void spi_transaction(unsigned char cmd, unsigned int addr, unsigned char flags, unsigned char *buff, size_t bufflen)
{
	int i;

	keepidle(1);
	spi_powerCtrl(1);

	spi_readwrite(cmd);

	if (flags & spi_address) {
		for (i = 0; i < 3; ++i) {
			spi_readwrite((addr >> 16) & 0xff);
			addr <<= 8;
		}
	}

	if (flags & spi_dummy)
		spi_readwrite(0);

	if (flags & spi_read) {
		for (i = 0; i < bufflen; ++i)
			buff[i] = spi_readwrite(0);
	}
	else {
		for (i = 0; i < bufflen; ++i)
			spi_readwrite(buff[i]);
	}

	spi_powerCtrl(0);
	keepidle(0);
}


int spi_init(void)
{
	spi_common.base = (void *)0x40013000; /* SPI1 */
	spi_common.rcc = (void *)0x40023800;  /* RCC */

	spi_common.spi_ready = 1;

	if (lookup("/gpiodrv", &spi_common.gpio))
		return -1;

	mutexCreate(&spi_common.mutex);
	condCreate(&spi_common.cond);

	/* Enable SPI1 clock */
	*(spi_common.rcc + rcc_apb2enr) |= 1 << 12;
	__asm__ volatile ("dmb");

	/* Disable SPI */
	*(spi_common.base + cr1) &= 1 << 6;
	__asm__ volatile ("dmb");

	/* 1 MHz baudrate, master, mode 0 */
	*(spi_common.base + cr1) = (1 << 2);

	/* Motorola frame format, SS output enable */
	*(spi_common.base + cr2) = (1 << 2);

	/* SPI mode enabled */
	*(spi_common.base + i2scfgr) = 0;

	/* Enable SPI */
	*(spi_common.base + cr1) |= 1 << 6;

	interrupt(16 + 35, spi_irqHandler, NULL, spi_common.cond);

	gpio_pinConfig(GPIOA, 4, 1, 0, 1, 0, 0);  /* SPI PWEN */
	gpio_pinConfig(GPIOE, 12, 1, 0, 1, 0, 0); /* SPI /CS */
	gpio_pinConfig(GPIOE, 13, 2, 5, 1, 0, 0); /* SPI SCK */
	gpio_pinConfig(GPIOE, 14, 2, 5, 1, 0, 0); /* SPI MISO */
	gpio_pinConfig(GPIOE, 15, 2, 5, 1, 0, 0); /* SPI MOSI */

	spi_powerCtrl(0);

	return 0;
}
