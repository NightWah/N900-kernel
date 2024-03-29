/*
 * TSC2005 touchscreen driver
 *
 * Copyright (C) 2006-2008 Nokia Corporation
 *
 * Author: Lauri Leukkunen <lauri.leukkunen@nokia.com>
 * based on TSC2301 driver by Klaus K. Pedersen <klaus.k.pedersen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>

#include <linux/spi/tsc2005.h>

/**
 * The touchscreen interface operates as follows:
 *
 * Initialize:
 *    Request access to GPIO103 (DAV)
 *    tsc2005_ts_irq_handler will trigger when DAV line goes down
 *
 *  1) Pen is pressed against touchscreeen
 *  2) TSC2005 performs AD conversion
 *  3) After the conversion is done TSC2005 drives DAV line down
 *  4) GPIO IRQ is received and tsc2005_ts_irq_handler is called
 *  5) tsc2005_ts_irq_handler queues up an spi transfer to fetch
 *     the x, y, z1, z2 values
 *  6) tsc2005_ts_rx() reports coordinates to input layer and
 *     sets up tsc2005_ts_timer() to be called after TSC2005_TS_SCAN_TIME
 *  7)  When the penup_timer expires, there have not been DAV interrupts
 *     during the last 20ms which means the pen has been lifted.
 */

#define TSC2005_VDD_LOWER_27

#ifdef TSC2005_VDD_LOWER_27
#define TSC2005_HZ     (10000000)
#else
#define TSC2005_HZ     (25000000)
#endif

#define TSC2005_CMD	(0x80)
#define TSC2005_REG	(0x00)

#define TSC2005_CMD_STOP	(1)
#define TSC2005_CMD_10BIT	(0 << 2)
#define TSC2005_CMD_12BIT	(1 << 2)

#define TSC2005_CMD_SCAN_XYZZ	(0 << 3)
#define TSC2005_CMD_SCAN_XY	(1 << 3)
#define TSC2005_CMD_SCAN_X	(2 << 3)
#define TSC2005_CMD_SCAN_Y	(3 << 3)
#define TSC2005_CMD_SCAN_ZZ	(4 << 3)
#define TSC2005_CMD_AUX_SINGLE	(5 << 3)
#define TSC2005_CMD_TEMP1	(6 << 3)
#define TSC2005_CMD_TEMP2	(7 << 3)
#define TSC2005_CMD_AUX_CONT	(8 << 3)
#define TSC2005_CMD_TEST_X_CONN	(9 << 3)
#define TSC2005_CMD_TEST_Y_CONN	(10 << 3)
#define TSC2005_CMD_TEST_SHORT	(11 << 3)
/* command 12 reserved, according to 2008-03 erratum */
#define TSC2005_CMD_DRIVE_XX	(13 << 3)
#define TSC2005_CMD_DRIVE_YY	(14 << 3)
#define TSC2005_CMD_DRIVE_YX	(15 << 3)

#define TSC2005_REG_X		(0 << 3)
#define TSC2005_REG_Y		(1 << 3)
#define TSC2005_REG_Z1		(2 << 3)
#define TSC2005_REG_Z2		(3 << 3)
#define TSC2005_REG_AUX		(4 << 3)
#define TSC2005_REG_TEMP1	(5 << 3)
#define TSC2005_REG_TEMP2	(6 << 3)
#define TSC2005_REG_STATUS	(7 << 3)
#define TSC2005_REG_AUX_HIGH	(8 << 3)
#define TSC2005_REG_AUX_LOW	(9 << 3)
#define TSC2005_REG_TEMP_HIGH	(10 << 3)
#define TSC2005_REG_TEMP_LOW	(11 << 3)
#define TSC2005_REG_CFR0	(12 << 3)
#define TSC2005_REG_CFR1	(13 << 3)
#define TSC2005_REG_CFR2	(14 << 3)
#define TSC2005_REG_FUNCTION	(15 << 3)

#define TSC2005_REG_PND0	(1 << 1)
#define TSC2005_REG_READ	(0x01)
#define TSC2005_REG_WRITE	(0x00)


#define TSC2005_CFR0_LONGSAMPLING	(1)
#define TSC2005_CFR0_DETECTINWAIT	(1 << 1)
#define TSC2005_CFR0_SENSETIME_32US	(0)
#define TSC2005_CFR0_SENSETIME_96US	(1 << 2)
#define TSC2005_CFR0_SENSETIME_544US	(1 << 3)
#define TSC2005_CFR0_SENSETIME_2080US	(1 << 4)
#define TSC2005_CFR0_SENSETIME_2656US	(0x001C)
#define TSC2005_CFR0_PRECHARGE_20US	(0x0000)
#define TSC2005_CFR0_PRECHARGE_84US	(0x0020)
#define TSC2005_CFR0_PRECHARGE_276US	(0x0040)
#define TSC2005_CFR0_PRECHARGE_1044US	(0x0080)
#define TSC2005_CFR0_PRECHARGE_1364US	(0x00E0)
#define TSC2005_CFR0_STABTIME_0US	(0x0000)
#define TSC2005_CFR0_STABTIME_100US	(0x0100)
#define TSC2005_CFR0_STABTIME_500US	(0x0200)
#define TSC2005_CFR0_STABTIME_1MS	(0x0300)
#define TSC2005_CFR0_STABTIME_5MS	(0x0400)
#define TSC2005_CFR0_STABTIME_100MS	(0x0700)
#define TSC2005_CFR0_CLOCK_4MHZ		(0x0000)
#define TSC2005_CFR0_CLOCK_2MHZ		(0x0800)
#define TSC2005_CFR0_CLOCK_1MHZ		(0x1000)
#define TSC2005_CFR0_RESOLUTION12	(0x2000)
#define TSC2005_CFR0_STATUS		(0x4000)
#define TSC2005_CFR0_PENMODE		(0x8000)

#define TSC2005_CFR0_INITVALUE	(TSC2005_CFR0_STABTIME_1MS  |	\
				 TSC2005_CFR0_CLOCK_1MHZ    |	\
				 TSC2005_CFR0_RESOLUTION12  |	\
				 TSC2005_CFR0_PRECHARGE_276US | \
				 TSC2005_CFR0_PENMODE)

/* Bits common to both read and write of config register 0 */
#define	TSC2005_CFR0_RW_MASK	0x3fff

#define TSC2005_CFR1_BATCHDELAY_0MS	(0x0000)
#define TSC2005_CFR1_BATCHDELAY_1MS	(0x0001)
#define TSC2005_CFR1_BATCHDELAY_2MS	(0x0002)
#define TSC2005_CFR1_BATCHDELAY_4MS	(0x0003)
#define TSC2005_CFR1_BATCHDELAY_10MS	(0x0004)
#define TSC2005_CFR1_BATCHDELAY_20MS	(0x0005)
#define TSC2005_CFR1_BATCHDELAY_40MS	(0x0006)
#define TSC2005_CFR1_BATCHDELAY_100MS	(0x0007)

#define TSC2005_CFR1_INITVALUE	(TSC2005_CFR1_BATCHDELAY_4MS)

#define TSC2005_CFR2_MAVE_TEMP	(0x0001)
#define TSC2005_CFR2_MAVE_AUX	(0x0002)
#define TSC2005_CFR2_MAVE_Z	(0x0004)
#define TSC2005_CFR2_MAVE_Y	(0x0008)
#define TSC2005_CFR2_MAVE_X	(0x0010)
#define TSC2005_CFR2_AVG_1	(0x0000)
#define TSC2005_CFR2_AVG_3	(0x0400)
#define TSC2005_CFR2_AVG_7	(0x0800)
#define TSC2005_CFR2_MEDIUM_1	(0x0000)
#define TSC2005_CFR2_MEDIUM_3	(0x1000)
#define TSC2005_CFR2_MEDIUM_7	(0x2000)
#define TSC2005_CFR2_MEDIUM_15	(0x3000)

#define TSC2005_CFR2_IRQ_MASK   (0xC000)
#define TSC2005_CFR2_IRQ_DAV	(0x4000)
#define TSC2005_CFR2_IRQ_PEN	(0x8000)
#define TSC2005_CFR2_IRQ_PENDAV	(0x0000)

#define TSC2005_CFR2_INITVALUE	(TSC2005_CFR2_IRQ_PENDAV |	\
				 TSC2005_CFR2_MAVE_X    |	\
				 TSC2005_CFR2_MAVE_Y    |	\
				 TSC2005_CFR2_MAVE_Z    |	\
				 TSC2005_CFR2_MEDIUM_15 |	\
				 TSC2005_CFR2_AVG_7)

#define MAX_12BIT					((1 << 12) - 1)
#define TS_SAMPLES					4
#define TSC2005_TS_PENUP_TIME				40

static const u32 tsc2005_read_reg[] = {
	(TSC2005_REG | TSC2005_REG_X | TSC2005_REG_READ) << 16,
	(TSC2005_REG | TSC2005_REG_Y | TSC2005_REG_READ) << 16,
	(TSC2005_REG | TSC2005_REG_Z1 | TSC2005_REG_READ) << 16,
	(TSC2005_REG | TSC2005_REG_Z2 | TSC2005_REG_READ) << 16,
};
#define NUM_READ_REGS	(sizeof(tsc2005_read_reg)/sizeof(tsc2005_read_reg[0]))

struct tsc2005 {
	struct spi_device	*spi;

	struct input_dev	*idev;
	char			phys[32];
	struct timer_list	penup_timer;

	/* ESD recovery via a hardware reset if the tsc2005
	 * doesn't respond after a configurable period (in ms) of
	 * IRQ/SPI inactivity. If esd_timeout is 0, timer and work
	 * fields are used.
	 */
	u32			esd_timeout;
	struct timer_list	esd_timer;
	struct work_struct	esd_work;

	spinlock_t		lock;
	struct mutex		mutex;

	struct spi_message	read_msg;
	struct spi_transfer	read_xfer[NUM_READ_REGS];
	u32                     data[NUM_READ_REGS];

	/* previously reported x,y,p (if pen_down) */
	int			out_x;
	int			out_y;
	int			out_p;
	/* fudge parameters - changes must exceed one of these. */
	int			fudge_x;
	int			fudge_y;
	int			fudge_p;
	/* raw copy of previous x,y,z */
	int			in_x;
	int			in_y;
	int			in_z1;
	int			in_z2;
	/* average accumulators for each component */
	int			sample_cnt;
	int			avg_x;
	int			avg_y;
	int			avg_z1;
	int			avg_z2;
	/* configuration */
	int			x_plate_ohm;
	int			hw_avg_max;
	int			stab_time;
	int			p_max;
	int			touch_pressure;
	/* status */
	u8			sample_sent;
	u8			pen_down;
	u8			disabled;
	u8			disable_depth;
	u8			spi_pending;

	void (*set_reset)(bool enable);
};

static void tsc2005_cmd(struct tsc2005 *ts, u8 cmd)
{
	u8 data = TSC2005_CMD | TSC2005_CMD_12BIT | cmd;
	struct spi_message msg;
	struct spi_transfer xfer = { 0 };

	xfer.tx_buf = &data;
	xfer.rx_buf = NULL;
	xfer.len = 1;
	xfer.bits_per_word = 8;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	spi_sync(ts->spi, &msg);
}

static void tsc2005_write(struct tsc2005 *ts, u8 reg, u16 value)
{
	u32 tx;
	struct spi_message msg;
	struct spi_transfer xfer = { 0 };

	tx = (TSC2005_REG | reg | TSC2005_REG_PND0 |
	       TSC2005_REG_WRITE) << 16;
	tx |= value;

	xfer.tx_buf = &tx;
	xfer.rx_buf = NULL;
	xfer.len = 4;
	xfer.bits_per_word = 24;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	spi_sync(ts->spi, &msg);
}

static void tsc2005_read(struct tsc2005 *ts, u8 reg, u16 *value)
{
	u32 tx;
	u32 rx = 0;
	struct spi_message msg;
	struct spi_transfer xfer = { 0 };

	tx = (TSC2005_REG | reg | TSC2005_REG_READ) << 16;

	xfer.tx_buf = &tx;
	xfer.rx_buf = &rx;
	xfer.len = 4;
	xfer.bits_per_word = 24;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	spi_sync(ts->spi, &msg);
	*value = rx;
}

static void tsc2005_ts_update_pen_state(struct tsc2005 *ts,
					int x, int y, int pressure)
{
	if (pressure) {
		input_report_abs(ts->idev, ABS_X, x);
		input_report_abs(ts->idev, ABS_Y, y);
		input_report_abs(ts->idev, ABS_PRESSURE, pressure);
		if (!ts->pen_down) {
			input_report_key(ts->idev, BTN_TOUCH, 1);
			ts->pen_down = 1;
		}
	} else {
		input_report_abs(ts->idev, ABS_PRESSURE, 0);
		if (ts->pen_down) {
			input_report_key(ts->idev, BTN_TOUCH, 0);
			ts->pen_down = 0;
		}
	}

	input_sync(ts->idev);
}

/*
 * This function is called by the SPI framework after the coordinates
 * have been read from TSC2005
 */
static void tsc2005_ts_rx(void *arg)
{
	struct tsc2005 *ts = arg;
	unsigned long flags;
	int inside_rect, pressure_limit;
	int x, y, z1, z2, pressure;

	spin_lock_irqsave(&ts->lock, flags);

	if (ts->disable_depth) {
		ts->spi_pending = 0;
		goto out;
	}

	x = ts->data[0];
	y = ts->data[1];
	z1 = ts->data[2];
	z2 = ts->data[3];

	/* validate pressure and position */
	if (x > MAX_12BIT || y > MAX_12BIT)
		goto out;

	/* skip coords if the pressure-components are out of range */
	if (z1 < 100 || z2 > MAX_12BIT || z1 >= z2)
		goto out;

	/* skip point if this is a pen down with the exact same values as
	 * the value before pen-up - that implies SPI fed us stale data
	 */
	if (!ts->pen_down &&
	    ts->in_x == x &&
	    ts->in_y == y &&
	    ts->in_z1 == z1 &&
	    ts->in_z2 == z2)
		goto out;

	/* At this point we are happy we have a valid and useful reading.
	 * Remember it for later comparisons. We may now begin downsampling
	 */
	ts->in_x = x;
	ts->in_y = y;
	ts->in_z1 = z1;
	ts->in_z2 = z2;

	/* don't run average on the "pen down" event */
	if (ts->sample_sent) {
		ts->avg_x += x;
		ts->avg_y += y;
		ts->avg_z1 += z1;
		ts->avg_z2 += z2;

		if (++ts->sample_cnt < TS_SAMPLES)
			goto out;

		x = ts->avg_x / TS_SAMPLES;
		y = ts->avg_y / TS_SAMPLES;
		z1 = ts->avg_z1 / TS_SAMPLES;
		z2 = ts->avg_z2 / TS_SAMPLES;
	}

	ts->sample_cnt = 0;
	ts->avg_x = 0;
	ts->avg_y = 0;
	ts->avg_z1 = 0;
	ts->avg_z2 = 0;

	pressure = x * (z2 - z1) / z1;
	pressure = pressure * ts->x_plate_ohm / 4096;

	pressure_limit = ts->sample_sent? ts->p_max: ts->touch_pressure;
	if (pressure > pressure_limit)
		goto out;

	/* Discard the event if it still is within the previous rect -
	 * unless the pressure is clearly harder, but then use previous
	 * x,y position. If any coordinate deviates enough, fudging
	 * of all three will still take place in the input layer.
	 */
	inside_rect = (ts->sample_sent &&
		x > (int)ts->out_x - ts->fudge_x &&
		x < (int)ts->out_x + ts->fudge_x &&
		y > (int)ts->out_y - ts->fudge_y &&
		y < (int)ts->out_y + ts->fudge_y);
	if (inside_rect)
		x = ts->out_x, y = ts->out_y;

	if (!inside_rect || pressure < (ts->out_p - ts->fudge_p)) {
		tsc2005_ts_update_pen_state(ts, x, y, pressure);
		ts->sample_sent = 1;
		ts->out_x = x;
		ts->out_y = y;
		ts->out_p = pressure;
	}
out:
	if (ts->spi_pending > 1) {
		/* One or more interrupts (sometimes several dozens)
		 * occured while waiting for the SPI read - get
		 * another read going.
		 */
		ts->spi_pending = 1;
		if (spi_async(ts->spi, &ts->read_msg)) {
			dev_err(&ts->spi->dev, "ts: spi_async() failed");
			ts->spi_pending = 0;
		}
	} else
		ts->spi_pending = 0;

	/* kick pen up timer - to make sure it expires again(!) */
	if (ts->sample_sent) {
		mod_timer(&ts->penup_timer,
			  jiffies + msecs_to_jiffies(TSC2005_TS_PENUP_TIME));
		/* Also kick the watchdog, as we still think we're alive */
		if (ts->esd_timeout && ts->disable_depth == 0) {
			unsigned long wdj = msecs_to_jiffies(ts->esd_timeout);
			mod_timer(&ts->esd_timer, round_jiffies(jiffies+wdj));
		}
	}
	spin_unlock_irqrestore(&ts->lock, flags);
}

/* This penup timer is very forgiving of delayed SPI reads. The
 * (ESD) watchdog will rescue us if spi_pending remains set, unless
 * we are enterring the disabled state. In that case we must just
 * handle the pen up, and let disabling complete.
 */
static void tsc2005_ts_penup_timer_handler(unsigned long data)
{
	struct tsc2005 *ts = (struct tsc2005 *)data;
	if ((!ts->spi_pending || ts->disable_depth) &&
	    ts->sample_sent) {
		tsc2005_ts_update_pen_state(ts, 0, 0, 0);
		ts->sample_sent = 0;
	}
}

/*
 * This interrupt is called when pen is down and coordinates are
 * available. That is indicated by a either:
 * a) a rising edge on PINTDAV or (PENDAV mode)
 * b) a falling edge on DAV line (DAV mode)
 * depending on the setting of the IRQ bits in the CFR2 setting above.
 */
static irqreturn_t tsc2005_ts_irq_handler(int irq, void *dev_id)
{
	struct tsc2005 *ts = dev_id;
	if (ts->disable_depth)
		goto out;

	if (!ts->spi_pending) {
		if (spi_async(ts->spi, &ts->read_msg)) {
			dev_err(&ts->spi->dev, "ts: spi_async() failed");
			goto out;
		}
	}
	/* By shifting in 1s we can never wrap */
	ts->spi_pending = (ts->spi_pending<<1)+1;

	/* Kick pen up timer only if it's not been started yet. Strictly,
	 * it isn't even necessary to start it at all here,  but doing so
	 * keeps an equivalence between pen state and timer state.
	 * The SPI read loop will keep pushing it into the future.
	 * If it times out with an SPI pending, it's ignored anyway.
	 */
	if (!timer_pending(&ts->penup_timer)) {
		unsigned long pu = msecs_to_jiffies(TSC2005_TS_PENUP_TIME);
		ts->penup_timer.expires = jiffies + pu;
		add_timer(&ts->penup_timer);
	}
out:
	return IRQ_HANDLED;
}

static void tsc2005_ts_setup_spi_xfer(struct tsc2005 *ts)
{
	struct spi_message *m = &ts->read_msg;
	struct spi_transfer *x = &ts->read_xfer[0];
	int i;

	spi_message_init(m);

	for (i = 0; i < NUM_READ_REGS; i++, x++) {
		x->tx_buf = &tsc2005_read_reg[i];
		x->rx_buf = &ts->data[i];
		x->len = 4;
		x->bits_per_word = 24;
		x->cs_change = i < (NUM_READ_REGS - 1);
		spi_message_add_tail(x, m);
	}

	m->complete = tsc2005_ts_rx;
	m->context = ts;
}

static ssize_t tsc2005_ts_pen_down_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tsc2005 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->pen_down);
}

static DEVICE_ATTR(pen_down, S_IRUGO, tsc2005_ts_pen_down_show, NULL);

static int tsc2005_configure(struct tsc2005 *ts, int flags)
{
	tsc2005_write(ts, TSC2005_REG_CFR0, TSC2005_CFR0_INITVALUE);
	tsc2005_write(ts, TSC2005_REG_CFR1, TSC2005_CFR1_INITVALUE);
	tsc2005_write(ts, TSC2005_REG_CFR2, TSC2005_CFR2_INITVALUE);
	tsc2005_cmd(ts, flags);

	return 0;
}

static void tsc2005_start_scan(struct tsc2005 *ts)
{
	tsc2005_configure(ts, TSC2005_CMD_SCAN_XYZZ);
}

static void tsc2005_stop_scan(struct tsc2005 *ts)
{
	tsc2005_cmd(ts, TSC2005_CMD_STOP);
}

/* Must be called with mutex held */
static void tsc2005_disable(struct tsc2005 *ts)
{
	if (ts->disable_depth++ != 0)
		return;

	disable_irq(ts->spi->irq);
	if (ts->esd_timeout)
		del_timer(&ts->esd_timer);

	/* wait until penup timer expire normally */
	do {
		msleep(4);
	} while (ts->sample_sent);

	tsc2005_stop_scan(ts);
}

static void tsc2005_enable(struct tsc2005 *ts)
{
	if (ts->disable_depth != 1)
		goto out;

	if (ts->esd_timeout) {
		unsigned long wdj = msecs_to_jiffies(ts->esd_timeout);
		ts->esd_timer.expires = round_jiffies(jiffies+wdj);
		add_timer(&ts->esd_timer);
	}
	tsc2005_start_scan(ts);
	enable_irq(ts->spi->irq);
out:
	--ts->disable_depth;
}

static ssize_t tsc2005_disable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct tsc2005 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->disabled);
}

static ssize_t tsc2005_disable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct tsc2005		*ts = dev_get_drvdata(dev);
	unsigned long res;
	int i;

	if (strict_strtoul(buf, 10, &res) < 0)
		return -EINVAL;
	i = res ? 1 : 0;

	mutex_lock(&ts->mutex);
	if (i == ts->disabled)
		goto out;
	ts->disabled = i;

	if (i)
		tsc2005_disable(ts);
	else
		tsc2005_enable(ts);
out:
	mutex_unlock(&ts->mutex);
	return count;
}

static DEVICE_ATTR(disable_ts, 0664, tsc2005_disable_show,
		   tsc2005_disable_store);

static ssize_t tsc2005_ctrl_selftest_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	u16 temp_high_orig, temp_high_test, temp_high;
	unsigned int result = 1;
	struct tsc2005 *ts = dev_get_drvdata(dev);

	if (!ts->set_reset) {
		dev_warn(&ts->spi->dev,
			 "unable to selftest: reset not configured\n");
		result = 0;
		goto out;
	}

	mutex_lock(&ts->mutex);
	tsc2005_disable(ts);

	/* Test ctrl communications via temp high / low registers */
	tsc2005_read(ts, TSC2005_REG_TEMP_HIGH, &temp_high_orig);

	temp_high_test = (temp_high_orig - 1) & 0x0FFF;

	tsc2005_write(ts, TSC2005_REG_TEMP_HIGH, temp_high_test);

	tsc2005_read(ts, TSC2005_REG_TEMP_HIGH, &temp_high);

	if (temp_high != temp_high_test) {
		result = 0;
		dev_warn(dev, "selftest failed: %d != %d\n",
			 temp_high, temp_high_test);
	}

	/* HW Reset */
	ts->set_reset(0);
	msleep(1); /* only 10us required */
	ts->set_reset(1);

	tsc2005_enable(ts);

	/* Test that reset really happened */
	tsc2005_read(ts, TSC2005_REG_TEMP_HIGH, &temp_high);

	if (temp_high != temp_high_orig) {
		result = 0;
		dev_warn(dev, "selftest failed after reset: "
			 "%d != %d\n",
			 temp_high, temp_high_orig);
	}

	mutex_unlock(&ts->mutex);

out:
	return sprintf(buf, "%u\n", result);
}

static DEVICE_ATTR(ts_ctrl_selftest, S_IRUGO, tsc2005_ctrl_selftest_show, NULL);

static void tsc2005_esd_timer_handler(unsigned long data)
{
	struct tsc2005 *ts = (struct tsc2005 *)data;
	if (!ts->disable_depth)
		schedule_work(&ts->esd_work);
}

static void tsc2005_rst_handler(struct work_struct *work)
{
	u16 reg_val;
	struct tsc2005 *ts = container_of(work, struct tsc2005, esd_work);
	unsigned long wdj;

	mutex_lock(&ts->mutex);

	/* If we are disabled, or the a touch has been detected,
	 * then ignore this timeout. The enable will restart the
	 * watchdog, as it restarts scanning
	 */
	if (ts->disable_depth)
		goto out;

	/* If we cannot read our known value from configuration register 0
	 * then reset the controller as if from power-up and start
	 * scanning again. Always re-arm the watchdog.
	 */
	tsc2005_read(ts, TSC2005_REG_CFR0, &reg_val);
	if ((reg_val ^ TSC2005_CFR0_INITVALUE) & TSC2005_CFR0_RW_MASK) {
		dev_info(&ts->spi->dev, "TSC not responding, resetting.\n");
		/* If this timer kicked in, the penup timer, if ever active
		 * at all, must have expired ages ago, so no need to del it.
		 */
		ts->set_reset(0);
		if (ts->sample_sent) {
			tsc2005_ts_update_pen_state(ts, 0, 0, 0);
			ts->sample_sent = 0;
		}
		ts->spi_pending = 0;
		msleep(1); /* only 10us required */
		ts->set_reset(1);
		tsc2005_start_scan(ts);
	}
	wdj = msecs_to_jiffies(ts->esd_timeout);
	mod_timer(&ts->esd_timer, round_jiffies(jiffies+wdj));

out:
	mutex_unlock(&ts->mutex);
}

static int __devinit tsc2005_ts_init(struct tsc2005 *ts,
				     struct tsc2005_platform_data *pdata)
{
	struct input_dev *idev;
	int r;
	int x_max, y_max;

	init_timer(&ts->penup_timer);
	setup_timer(&ts->penup_timer, tsc2005_ts_penup_timer_handler,
			(unsigned long)ts);

	spin_lock_init(&ts->lock);
	mutex_init(&ts->mutex);

	ts->x_plate_ohm		= pdata->ts_x_plate_ohm ? : 280;
	ts->hw_avg_max		= pdata->ts_hw_avg;
	ts->stab_time		= pdata->ts_stab_time;
	x_max			= pdata->ts_x_max ? : 4096;
	ts->fudge_x		= pdata->ts_x_fudge ? : 4;
	y_max			= pdata->ts_y_max ? : 4096;
	ts->fudge_y		= pdata->ts_y_fudge ? : 8;
	ts->p_max		= pdata->ts_pressure_max ? : MAX_12BIT;
	ts->touch_pressure	= pdata->ts_touch_pressure ? : ts->p_max;
	ts->fudge_p		= pdata->ts_pressure_fudge ? : 2;

	ts->set_reset		= pdata->set_reset;

	idev = input_allocate_device();
	if (idev == NULL) {
		r = -ENOMEM;
		goto err1;
	}

	idev->name = "TSC2005 touchscreen";
	snprintf(ts->phys, sizeof(ts->phys), "%s/input-ts",
		 ts->spi->dev.bus_id);
	idev->phys = ts->phys;

	idev->evbit[0] = BIT(EV_ABS) | BIT(EV_KEY);
	idev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);
	idev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->idev = idev;

	tsc2005_ts_setup_spi_xfer(ts);

	input_set_abs_params(idev, ABS_X, 0, x_max, ts->fudge_x, 0);
	input_set_abs_params(idev, ABS_Y, 0, y_max, ts->fudge_y, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, ts->p_max, ts->fudge_p, 0);

	tsc2005_start_scan(ts);

	r = request_irq(ts->spi->irq, tsc2005_ts_irq_handler,
			(((TSC2005_CFR2_INITVALUE & TSC2005_CFR2_IRQ_MASK) ==
			  TSC2005_CFR2_IRQ_PENDAV)
			 ? IRQF_TRIGGER_RISING
			 : IRQF_TRIGGER_FALLING) |
			IRQF_DISABLED | IRQF_SAMPLE_RANDOM, "tsc2005", ts);
	if (r < 0) {
		dev_err(&ts->spi->dev, "unable to get DAV IRQ");
		goto err2;
	}

	set_irq_wake(ts->spi->irq, 1);

	r = input_register_device(idev);
	if (r < 0) {
		dev_err(&ts->spi->dev, "can't register touchscreen device\n");
		goto err3;
	}

	/* We can tolerate these failing */
	r = device_create_file(&ts->spi->dev, &dev_attr_ts_ctrl_selftest);
	if (r < 0)
		dev_warn(&ts->spi->dev, "can't create sysfs file for %s: %d\n",
			 dev_attr_ts_ctrl_selftest.attr.name, r);

	r = device_create_file(&ts->spi->dev, &dev_attr_pen_down);
	if (r < 0)
		dev_warn(&ts->spi->dev, "can't create sysfs file for %s: %d\n",
			 dev_attr_pen_down.attr.name, r);

	r = device_create_file(&ts->spi->dev, &dev_attr_disable_ts);
	if (r < 0)
		dev_warn(&ts->spi->dev, "can't create sysfs file for %s: %d\n",
			 dev_attr_disable_ts.attr.name, r);

	/* Finally, configure and start the optional EDD watchdog. */
	ts->esd_timeout = pdata->esd_timeout;
	if (ts->esd_timeout && ts->set_reset) {
		unsigned long wdj;
		setup_timer(&ts->esd_timer, tsc2005_esd_timer_handler,
			    (unsigned long)ts);
		INIT_WORK(&ts->esd_work, tsc2005_rst_handler);
		wdj = msecs_to_jiffies(ts->esd_timeout);
		ts->esd_timer.expires = round_jiffies(jiffies+wdj);
		add_timer(&ts->esd_timer);
	}

	return 0;
err3:
	free_irq(ts->spi->irq, ts);
err2:
	tsc2005_stop_scan(ts);
	input_free_device(idev);
err1:
	return r;
}

static int __devinit tsc2005_probe(struct spi_device *spi)
{
	struct tsc2005			*ts;
	struct tsc2005_platform_data	*pdata = spi->dev.platform_data;
	int r;

	if (spi->irq < 0) {
		dev_dbg(&spi->dev, "no irq?\n");
		return -ENODEV;
	}
	if (!pdata) {
		dev_dbg(&spi->dev, "no platform data?\n");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL)
		return -ENOMEM;

	dev_set_drvdata(&spi->dev, ts);
	ts->spi = spi;
	spi->dev.power.power_state = PMSG_ON;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	/* The max speed might've been defined by the board-specific
	 * struct */
	if (!spi->max_speed_hz)
		spi->max_speed_hz = TSC2005_HZ;

	spi_setup(spi);

	r = tsc2005_ts_init(ts, pdata);
	if (r)
		goto err1;

	return 0;

err1:
	kfree(ts);
	return r;
}

static int __devexit tsc2005_remove(struct spi_device *spi)
{
	struct tsc2005 *ts = dev_get_drvdata(&spi->dev);

	mutex_lock(&ts->mutex);
	tsc2005_disable(ts);
	mutex_unlock(&ts->mutex);

	device_remove_file(&ts->spi->dev, &dev_attr_disable_ts);
	device_remove_file(&ts->spi->dev, &dev_attr_pen_down);
	device_remove_file(&ts->spi->dev, &dev_attr_ts_ctrl_selftest);

	free_irq(ts->spi->irq, ts);
	input_unregister_device(ts->idev);

	if (ts->esd_timeout)
		del_timer(&ts->esd_timer);
	kfree(ts);

	return 0;
}

#ifdef CONFIG_PM
static int tsc2005_suspend(struct spi_device *spi, pm_message_t mesg)
{
	struct tsc2005 *ts = dev_get_drvdata(&spi->dev);

	mutex_lock(&ts->mutex);
	tsc2005_disable(ts);
	mutex_unlock(&ts->mutex);

	return 0;
}

static int tsc2005_resume(struct spi_device *spi)
{
	struct tsc2005 *ts = dev_get_drvdata(&spi->dev);

	mutex_lock(&ts->mutex);
	tsc2005_enable(ts);
	mutex_unlock(&ts->mutex);

	return 0;
}
#endif

static struct spi_driver tsc2005_driver = {
	.driver = {
		.name = "tsc2005",
		.owner = THIS_MODULE,
	},
#ifdef CONFIG_PM
	.suspend = tsc2005_suspend,
	.resume = tsc2005_resume,
#endif
	.probe = tsc2005_probe,
	.remove = __devexit_p(tsc2005_remove),
};

static int __init tsc2005_init(void)
{
	printk(KERN_INFO "TSC2005 driver initializing\n");

	return spi_register_driver(&tsc2005_driver);
}
module_init(tsc2005_init);

static void __exit tsc2005_exit(void)
{
	spi_unregister_driver(&tsc2005_driver);
}
module_exit(tsc2005_exit);

MODULE_AUTHOR("Lauri Leukkunen <lauri.leukkunen@nokia.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tsc2005");
