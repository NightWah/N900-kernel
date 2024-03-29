/*
 * linux/drivers/i2c/chips/twl4030_poweroff.c
 *
 * Power off device
 *
 * Copyright (C) 2008 Nokia Corporation
 *
 * Written by Peter De Schrijver <peter.de-schrijver@nokia.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/pm.h>
#include <linux/i2c/twl4030.h>

#define STS_HW_CONDITIONS      0x0f
#define STS_VBUS (1<<7)

#define PWR_P1_SW_EVENTS	0x10
#define PWR_DEVOFF	(1<<0)

#define TWL4030_WATCHDOG_CFG_REG_OFFS   0x3

static u8 twl4030_usb_suspended;

void twl4030_upd_usb_suspended(u8 suspended)
{
	twl4030_usb_suspended = suspended;
}

static void twl4030_poweroff(void)
{
	u8 val;
	int err;

	err = twl4030_i2c_read_u8(TWL4030_MODULE_PM_MASTER, &val,
				  STS_HW_CONDITIONS);
	if (err)
		printk(KERN_WARNING "I2C error %d while reading TWL4030"
				    " PM_MASTER HW_CONDITIONS\n", err);

	if ((val & STS_VBUS) && !twl4030_usb_suspended) {
		printk(KERN_EMERG "twl4030-poweroff: VBUS on,"
				  " forcing restart!\n");
		/* Set watchdog, Triton goes to WAIT-ON state.
		   VBUS will cause start up */
		twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 1,
				     TWL4030_WATCHDOG_CFG_REG_OFFS);
		while (1);
	}

	err = twl4030_i2c_read_u8(TWL4030_MODULE_PM_MASTER, &val,
				  PWR_P1_SW_EVENTS);
	if (err) {
		printk(KERN_WARNING "I2C error %d while reading TWL4030"
					"PM_MASTER P1_SW_EVENTS\n", err);
		return ;
	}

	val |= PWR_DEVOFF;

	err = twl4030_i2c_write_u8(TWL4030_MODULE_PM_MASTER, val,
				   PWR_P1_SW_EVENTS);

	if (err) {
		printk(KERN_WARNING "I2C error %d while writing TWL4030"
					"PM_MASTER P1_SW_EVENTS\n", err);
		return ;
	}

	return;
}

static int __init twl4030_poweroff_init(void)
{
	pm_power_off = twl4030_poweroff;

	return 0;
}

static void __exit twl4030_poweroff_exit(void)
{
	pm_power_off = NULL;
}

module_init(twl4030_poweroff_init);
module_exit(twl4030_poweroff_exit);

MODULE_ALIAS("i2c:twl4030-poweroff");
MODULE_DESCRIPTION("Triton2 device power off");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter De Schrijver");
