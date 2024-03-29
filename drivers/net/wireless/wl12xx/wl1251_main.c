
/*
 * This file is part of wl1251
 *
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Contact: Kalle Valo <kalle.valo@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/spi/spi.h>
#include <linux/crc32.h>
#include <linux/etherdevice.h>
#include <linux/vmalloc.h>
#include <linux/spi/wl12xx.h>
#include <linux/inetdevice.h>

#include "wl1251.h"
#include "wl12xx_80211.h"
#include "wl1251_reg.h"
#include "wl1251_spi.h"
#include "wl1251_event.h"
#include "wl1251_tx.h"
#include "wl1251_rx.h"
#include "wl1251_ps.h"
#include "wl1251_init.h"
#include "wl1251_netlink.h"
#include "wl1251_debugfs.h"
#include "wl1251_boot.h"

static ssize_t wl1251_sysfs_show_tx_mgmt_frm_rate(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	struct wl1251 *wl = dev_get_drvdata(dev);
	ssize_t len;
	int val;

	/* FIXME: what's the maximum length of buf? page size?*/
	len = 500;

	switch (wl->tx_mgmt_frm_rate) {
		/* skip 1 and 12 Mbps because they have same value 0x0a */
	case RATE_2MBPS:
		val = 20;
		break;
	case RATE_5_5MBPS:
		val = 55;
		break;
	case RATE_11MBPS:
		val = 110;
		break;
	case RATE_6MBPS:
		val = 60;
		break;
	case RATE_9MBPS:
		val = 90;
		break;
	case RATE_12MBPS:
		val = 120;
		break;
	case RATE_18MBPS:
		val = 180;
		break;
	case RATE_24MBPS:
		val = 240;
		break;
	case RATE_36MBPS:
		val = 360;
		break;
	case RATE_48MBPS:
		val = 480;
		break;
	case RATE_54MBPS:
		val = 540;
		break;
	default:
		val = 10;
	}

	/* for 1 and 12 Mbps we have to check the modulation */
	if (wl->tx_mgmt_frm_rate == RATE_1MBPS) {
		switch (wl->tx_mgmt_frm_rate) {
		case CCK_LONG:
			val = 10;
			break;
		case OFDM:
			val = 120;
			break;
		default:
			val = 10;
			break;
		}
	}
	len = snprintf(buf, len, "%d", val);

	return len;
}

static ssize_t wl1251_sysfs_store_tx_mgmt_frm_rate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct wl1251 *wl = dev_get_drvdata(dev);
	unsigned long res;
	int ret;

	ret = strict_strtoul(buf, 10, &res);

	if (ret < 0) {
		wl1251_warning("incorrect value written to tx_mgmt_frm_rate");
		return 0;
	}

	switch (res) {
	case 10:
		wl->tx_mgmt_frm_rate = RATE_1MBPS;
		wl->tx_mgmt_frm_mod = CCK_LONG;
		break;
	case 20:
		wl->tx_mgmt_frm_rate = RATE_2MBPS;
		wl->tx_mgmt_frm_mod = CCK_LONG;
		break;
	case 55:
		wl->tx_mgmt_frm_rate = RATE_5_5MBPS;
		wl->tx_mgmt_frm_mod = CCK_LONG;
		break;
	case 110:
		wl->tx_mgmt_frm_rate = RATE_11MBPS;
		wl->tx_mgmt_frm_mod = CCK_LONG;
		break;
	case 60:
		wl->tx_mgmt_frm_rate = RATE_6MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 90:
		wl->tx_mgmt_frm_rate = RATE_9MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 120:
		wl->tx_mgmt_frm_rate = RATE_12MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 180:
		wl->tx_mgmt_frm_rate = RATE_18MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 240:
		wl->tx_mgmt_frm_rate = RATE_24MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 360:
		wl->tx_mgmt_frm_rate = RATE_36MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 480:
		wl->tx_mgmt_frm_rate = RATE_48MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	case 540:
		wl->tx_mgmt_frm_rate = RATE_54MBPS;
		wl->tx_mgmt_frm_mod = OFDM;
		break;
	default:
		wl1251_warning("incorrect value written to tx_mgmt_frm_rate");
		return 0;
	}

	return count;
}

static ssize_t wl1251_sysfs_show_bt_coex_mode(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct wl1251 *wl = dev_get_drvdata(dev);
	ssize_t len;

	/* FIXME: what's the maximum length of buf? page size?*/
	len = 500;

	mutex_lock(&wl->mutex);
	len = snprintf(buf, len, "%d\n\n%d - off\n%d - on\n%d - monoaudio\n",
		       wl->bt_coex_mode,
		       WL1251_BT_COEX_OFF,
		       WL1251_BT_COEX_ENABLE,
		       WL1251_BT_COEX_MONOAUDIO);
	mutex_unlock(&wl->mutex);

	return len;

}

static ssize_t wl1251_sysfs_store_bt_coex_mode(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct wl1251 *wl = dev_get_drvdata(dev);
	unsigned long res;
	int ret;

	ret = strict_strtoul(buf, 10, &res);

	if (ret < 0) {
		wl1251_warning("incorrect value written to bt_coex_mode");
		return count;
	}

	mutex_lock(&wl->mutex);

	if (res == wl->bt_coex_mode)
		goto out;

	switch (res) {
	case WL1251_BT_COEX_OFF:
	case WL1251_BT_COEX_ENABLE:
	case WL1251_BT_COEX_MONOAUDIO:
		wl->bt_coex_mode = res;
		break;
	default:
		wl1251_warning("incorrect value written to bt_coex_mode");
		goto out;
	}

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1251_acx_sg_configure(wl, false);
	wl1251_ps_elp_sleep(wl);

 out:
	mutex_unlock(&wl->mutex);
	return count;
}

static DEVICE_ATTR(tx_mgmt_frm_rate, S_IRUGO | S_IWUSR,
		   wl1251_sysfs_show_tx_mgmt_frm_rate,
		   wl1251_sysfs_store_tx_mgmt_frm_rate);

static DEVICE_ATTR(bt_coex_mode, S_IRUGO | S_IWUSR,
		   wl1251_sysfs_show_bt_coex_mode,
		   wl1251_sysfs_store_bt_coex_mode);

static void wl1251_disable_interrupts(struct wl1251 *wl)
{
	disable_irq(wl->irq);
}

static void wl1251_power_off(struct wl1251 *wl)
{
	wl->set_power(false);
}

static void wl1251_power_on(struct wl1251 *wl)
{
	wl->set_power(true);
}

static irqreturn_t wl1251_irq(int irq, void *cookie)
{
	struct wl1251 *wl;

	wl1251_debug(DEBUG_IRQ, "IRQ");

	wl = cookie;

	queue_work(wl->hw->workqueue, &wl->irq_work);

	return IRQ_HANDLED;
}

static int wl1251_fetch_firmware(struct wl1251 *wl)
{
	const struct firmware *fw;
	int ret;

	ret = request_firmware(&fw, WL1251_FW_NAME, &wl->spi->dev);

	if (ret < 0) {
		wl1251_error("could not get firmware: %d", ret);
		return ret;
	}

	if (fw->size % 4) {
		wl1251_error("firmware size is not multiple of 32 bits: %zu",
			     fw->size);
		ret = -EILSEQ;
		goto out;
	}

	wl->fw_len = fw->size;
	wl->fw = vmalloc(wl->fw_len);

	if (!wl->fw) {
		wl1251_error("could not allocate memory for the firmware");
		ret = -ENOMEM;
		goto out;
	}

	memcpy(wl->fw, fw->data, wl->fw_len);

	ret = 0;

out:
	release_firmware(fw);

	return ret;
}

static int wl1251_fetch_nvs(struct wl1251 *wl)
{
	const struct firmware *fw;
	int ret;

	ret = request_firmware(&fw, WL1251_NVS_NAME, &wl->spi->dev);

	if (ret < 0) {
		wl1251_error("could not get nvs file: %d", ret);
		return ret;
	}

	if (fw->size % 4) {
		wl1251_error("nvs size is not multiple of 32 bits: %zu",
			     fw->size);
		ret = -EILSEQ;
		goto out;
	}

	wl->nvs_len = fw->size;
	wl->nvs = kmalloc(wl->nvs_len, GFP_KERNEL);

	if (!wl->nvs) {
		wl1251_error("could not allocate memory for the nvs file");
		ret = -ENOMEM;
		goto out;
	}

	memcpy(wl->nvs, fw->data, wl->nvs_len);

	ret = 0;

out:
	release_firmware(fw);

	return ret;
}

static void wl1251_fw_wakeup(struct wl1251 *wl)
{
	u32 elp_reg;

	elp_reg = ELPCTRL_WAKE_UP;
	wl1251_write32(wl, HW_ACCESS_ELP_CTRL_REG_ADDR, elp_reg);
	elp_reg = wl1251_read32(wl, HW_ACCESS_ELP_CTRL_REG_ADDR);

	if (!(elp_reg & ELPCTRL_WLAN_READY))
		wl1251_warning("WLAN not ready");
}

static int wl1251_chip_wakeup(struct wl1251 *wl)
{
	int ret = 0;

	wl1251_power_on(wl);
	msleep(WL1251_POWER_ON_SLEEP);
	wl1251_spi_reset(wl);
	wl1251_spi_init(wl);

	/* We don't need a real memory partition here, because we only want
	 * to use the registers at this point. */
	wl1251_set_partition(wl,
			     0x00000000,
			     0x00000000,
			     REGISTERS_BASE,
			     REGISTERS_DOWN_SIZE);

	/* ELP module wake up */
	wl1251_fw_wakeup(wl);

	/* whal_FwCtrl_BootSm() */

	/* 0. read chip id from CHIP_ID */
	wl->chip_id = wl1251_reg_read32(wl, CHIP_ID_B);

	/* 1. check if chip id is valid */

	switch (wl->chip_id) {
	case CHIP_ID_1251_PG12:
		wl1251_debug(DEBUG_BOOT, "chip id 0x%x (1251 PG12)",
			     wl->chip_id);
		break;
	case CHIP_ID_1251_PG10:
	case CHIP_ID_1251_PG11:
	default:
		wl1251_error("unsupported chip id: 0x%x", wl->chip_id);
		ret = -ENODEV;
		goto out;
	}

	if (wl->fw == NULL) {
		ret = wl1251_fetch_firmware(wl);
		if (ret < 0)
			goto out;
	}

	/* No NVS from netlink, try to get it from the filesystem */
	if (wl->nvs == NULL) {
		ret = wl1251_fetch_nvs(wl);
		if (ret < 0)
			goto out;
	}

out:
	return ret;
}

#define WL1251_EVENT_TIMEOUT  10000
#define WL1251_IRQ_LOOP_COUNT 10
static void wl1251_irq_work(struct work_struct *work)
{
	u32 intr, ctr = WL1251_IRQ_LOOP_COUNT;
	struct wl1251 *wl =
		container_of(work, struct wl1251, irq_work);
	int ret;

	mutex_lock(&wl->mutex);

	wl1251_debug(DEBUG_IRQ, "IRQ work");

	if (wl->state == WL1251_STATE_OFF)
		goto out;

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl1251_reg_write32(wl, ACX_REG_INTERRUPT_MASK, WL1251_ACX_INTR_ALL);

	intr = wl1251_reg_read32(wl, ACX_REG_INTERRUPT_CLEAR);
	wl1251_debug(DEBUG_IRQ, "intr: 0x%x", intr);

	do {
		if (wl->data_path) {
			wl->rx_counter = wl1251_mem_read32(
				wl, wl->data_path->rx_control_addr);

			/* We handle a frmware bug here */
			switch ((wl->rx_counter - wl->rx_handled) & 0xf) {
			case 0:
				wl1251_debug(DEBUG_IRQ,
					     "RX: FW and host in sync");
				intr &= ~WL1251_ACX_INTR_RX0_DATA;
				intr &= ~WL1251_ACX_INTR_RX1_DATA;
				break;
			case 1:
				wl1251_debug(DEBUG_IRQ, "RX: FW +1");
				intr |= WL1251_ACX_INTR_RX0_DATA;
				intr &= ~WL1251_ACX_INTR_RX1_DATA;
				break;
			case 2:
				wl1251_debug(DEBUG_IRQ, "RX: FW +2");
				intr |= WL1251_ACX_INTR_RX0_DATA;
				intr |= WL1251_ACX_INTR_RX1_DATA;
				break;
			default:
				wl1251_warning(
					"RX: FW and host out of sync: %d",
					wl->rx_counter - wl->rx_handled);
				break;
			}

			wl->rx_handled = wl->rx_counter;

			wl1251_debug(DEBUG_IRQ, "RX counter: %d",
				     wl->rx_counter);
		}

		intr &= wl->intr_mask;

		if (intr == 0) {
			wl1251_debug(DEBUG_IRQ, "INTR is 0");
			goto out_sleep;
		}

		if (intr & WL1251_ACX_INTR_RX0_DATA) {
			wl1251_debug(DEBUG_IRQ, "WL1251_ACX_INTR_RX0_DATA");
			wl1251_rx(wl);
		}

		if (intr & WL1251_ACX_INTR_RX1_DATA) {
			wl1251_debug(DEBUG_IRQ, "WL1251_ACX_INTR_RX1_DATA");
			wl1251_rx(wl);
		}

		if (intr & WL1251_ACX_INTR_TX_RESULT) {
			wl1251_debug(DEBUG_IRQ, "WL1251_ACX_INTR_TX_RESULT");
			wl1251_tx_complete(wl);
		}

		if (intr & (WL1251_ACX_INTR_EVENT_A |
			    WL1251_ACX_INTR_EVENT_B)) {
			wl1251_debug(DEBUG_IRQ, "WL1251_ACX_INTR_EVENT (0x%x)",
				     intr);
			if (intr & WL1251_ACX_INTR_EVENT_A)
				wl1251_event_handle(wl, 0);
			else
				wl1251_event_handle(wl, 1);

			wl->last_event = jiffies +
				msecs_to_jiffies(WL1251_EVENT_TIMEOUT);
		}

		if (intr & WL1251_ACX_INTR_INIT_COMPLETE)
			wl1251_debug(DEBUG_IRQ,
				     "WL1251_ACX_INTR_INIT_COMPLETE");

		if (--ctr == 0)
			break;

		intr = wl1251_reg_read32(wl, ACX_REG_INTERRUPT_CLEAR);
	} while (intr);

out_sleep:
	/* FIXME:
	 * Occasionally the firmware puts mailbox events into the mailbox
	 * for the host to read, but fails to flag the appropriate mailbox
	 * interrupt. This causes the event mailbox to get jammed. This
	 * work-a-round wakes the event queue periodically to avoid the jam.
	 *
	 * The real fix involves a firmware-side and host-side counter
	 * mechanism, similar to the one above for the RX path.
	 */
	if (time_after(jiffies, wl->last_event) && ctr) {
		wl1251_reg_write32(wl, ACX_REG_INTERRUPT_TRIG,
				   INTR_TRIG_EVENT_ACK);
		wl->last_event = jiffies +
			msecs_to_jiffies(WL1251_EVENT_TIMEOUT);
	}
	wl1251_reg_write32(wl, ACX_REG_INTERRUPT_MASK, ~(wl->intr_mask));
	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
}

static int wl1251_join(struct wl1251 *wl, u8 bss_type, u8 channel,
		       u16 beacon_interval, u8 dtim_period)
{
	int ret;

	ret = wl1251_acx_frame_rates(wl, DEFAULT_HW_GEN_TX_RATE,
				     DEFAULT_HW_GEN_MODULATION_TYPE,
				     wl->tx_mgmt_frm_rate,
				     wl->tx_mgmt_frm_mod);
	if (ret < 0)
		goto out;


	ret = wl1251_cmd_join(wl, bss_type, channel, beacon_interval,
			      dtim_period);
	if (ret < 0)
		goto out;

	/*
	 * FIXME: we should wait for JOIN_EVENT_COMPLETE_ID but to simplify
	 * locking we just sleep instead, for now
	 */
	msleep(10);

out:
	return ret;
}
struct wl1251_filter_params {
	unsigned int filters;
	unsigned int changed;
	int mc_list_length;
	u8 mc_list[ACX_MC_ADDRESS_GROUP_MAX][ETH_ALEN];
};

#define WL1251_SUPPORTED_FILTERS (FIF_PROMISC_IN_BSS | \
				  FIF_ALLMULTI | \
				  FIF_FCSFAIL | \
				  FIF_BCN_PRBRESP_PROMISC | \
				  FIF_CONTROL | \
				  FIF_OTHER_BSS)

static void wl1251_filter_work(struct work_struct *work)
{
	struct wl1251 *wl =
		container_of(work, struct wl1251, filter_work);
	struct wl1251_filter_params *fp;
	unsigned long flags;
	bool enabled = true;
	int ret;

	/* first, get the filter parameters */
	spin_lock_irqsave(&wl->wl_lock, flags);
	fp = wl->filter_params;
	wl->filter_params = NULL;
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	if (!fp)
		return;

	/* then, lock the mutex without risk of lock-up */
	mutex_lock(&wl->mutex);

	if (wl->state == WL1251_STATE_OFF)
		goto out;

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	/* configure the mc filter regardless of the changed flags */
	if (fp->filters & FIF_ALLMULTI)
		enabled = false;

	ret = wl1251_acx_group_address_tbl(wl, fp->mc_list, fp->mc_list_length,
					   enabled);
	if (ret < 0)
		goto out_sleep;

	/* determine, whether supported filter values have changed */
	if (fp->changed == 0)
		goto out;

	/* apply configured filters */
	ret = wl1251_acx_rx_config(wl, wl->rx_config, wl->rx_filter);
	if (ret < 0)
		goto out_sleep;

out_sleep:
	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
	kfree(fp);
}

static int wl1251_plt_init(struct wl1251 *wl)
{
	int ret;

	ret = wl1251_hw_init_mem_config(wl);
	if (ret < 0)
		return ret;

	ret = wl1251_cmd_data_path(wl, wl->channel, 1);
	if (ret < 0)
		return ret;

	return 0;
}

int wl1251_plt_start(struct wl1251 *wl)
{
	int ret;

	mutex_lock(&wl->mutex);

	wl1251_notice("power up");

	if (wl->state != WL1251_STATE_OFF) {
		wl1251_error("cannot go into PLT state because not "
			     "in off state: %d", wl->state);
		ret = -EBUSY;
		goto out;
	}

	wl->state = WL1251_STATE_PLT;

	ret = wl1251_chip_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1251_boot(wl);
	if (ret < 0)
		goto out;

	wl1251_notice("firmware booted in PLT mode (%s)", wl->fw_ver);

	ret = wl1251_plt_init(wl);
	if (ret < 0)
		goto out;

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

int wl1251_plt_stop(struct wl1251 *wl)
{
	int ret = 0;

	mutex_lock(&wl->mutex);

	wl1251_notice("power down");

	if (wl->state != WL1251_STATE_PLT) {
		wl1251_error("cannot power down because not in PLT "
			     "state: %d", wl->state);
		ret = -EBUSY;
		goto out;
	}

	wl1251_disable_interrupts(wl);
	wl1251_power_off(wl);

	wl->state = WL1251_STATE_OFF;

out:
	mutex_unlock(&wl->mutex);

	return ret;
}


static int wl1251_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct wl1251 *wl = hw->priv;

	skb_queue_tail(&wl->tx_queue, skb);

	/*
	 * The chip specific setup must run before the first TX packet -
	 * before that, the tx_work will not be initialized!
	 */

	queue_work(wl->hw->workqueue, &wl->tx_work);

	/*
	 * The workqueue is slow to process the tx_queue and we need stop
	 * the queue here, otherwise the queue will get too long.
	 */
	if (skb_queue_len(&wl->tx_queue) >= WL1251_TX_QUEUE_MAX_LENGTH) {
		wl1251_debug(DEBUG_TX, "op_tx: tx_queue full, stop queues");
		ieee80211_stop_queues(wl->hw);

		/*
		 * FIXME: this is racy, the variable is not properly
		 * protected. Maybe fix this by removing the stupid
		 * variable altogether and checking the real queue state?
		 */
		wl->tx_queue_stopped = true;
	}

	return NETDEV_TX_OK;
}

static int wl1251_dev_notify(struct notifier_block *me, unsigned long what,
			     void *arg)
{
	struct net_device *dev;
	struct wireless_dev *wdev;
	struct wiphy *wiphy;
	struct ieee80211_hw *hw;
	struct wl1251 *wl;
	struct in_ifaddr *ifa = arg;
	int ret = 0;

	dev = ifa->ifa_dev->dev;

	wdev = dev->ieee80211_ptr;
	if (wdev == NULL)
		return -ENODEV;

	wiphy = wdev->wiphy;
	if (wiphy == NULL)
		return -ENODEV;

	hw = wiphy_priv(wiphy);
	if (hw == NULL)
		return -ENODEV;

	/* FIXME, we assume here that the notification was for wl12xx.
	   That is not true if there are multiple WLAN adapters in the device.
	   FIXME, we should probably not install ARP filter if the interface
	   has multiple addresses.
	*/
	wl = hw->priv;

	mutex_lock(&wl->mutex);

	if (wl->state == WL1251_STATE_OFF)
		goto out;

	/* FIXME, add support for IPv6 */
	if (what == NETDEV_UP) {
		ret = wl1251_ps_elp_wakeup(wl);
		if (ret < 0)
			goto out;

		ret = wl1251_acx_ip_config(wl, true, (u8 *)&ifa->ifa_address,
					   IPV4_VERSION);
		wl1251_ps_elp_sleep(wl);

	} else if (what == NETDEV_DOWN) {
		ret = wl1251_ps_elp_wakeup(wl);
		if (ret < 0)
			goto out;
		ret = wl1251_acx_ip_config(wl, false, NULL, IPV4_VERSION);
		wl1251_ps_elp_sleep(wl);
	}
out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static struct notifier_block wl1251_dev_notifier = {
	.notifier_call = wl1251_dev_notify,
};

static int wl1251_op_start(struct ieee80211_hw *hw)
{
	struct wl1251 *wl = hw->priv;
	int ret = 0;

	wl1251_debug(DEBUG_MAC80211, "mac80211 start");

	mutex_lock(&wl->mutex);

	if (wl->state != WL1251_STATE_OFF) {
		wl1251_error("cannot start because not in off state: %d",
			     wl->state);
		ret = -EBUSY;
		goto out;
	}

	ret = wl1251_chip_wakeup(wl);
	if (ret < 0)
		return ret;

	ret = wl1251_boot(wl);
	if (ret < 0)
		goto out;

	ret = wl1251_hw_init(wl);
	if (ret < 0)
		goto out;

	ret = wl1251_acx_station_id(wl);
	if (ret < 0)
		goto out;

	wl->state = WL1251_STATE_ON;

	wl1251_info("firmware booted (%s)", wl->fw_ver);

out:
	if (ret < 0)
		wl1251_power_off(wl);

	mutex_unlock(&wl->mutex);

	register_inetaddr_notifier(&wl1251_dev_notifier);

	return ret;
}

static void wl1251_op_stop(struct ieee80211_hw *hw)
{
	struct wl1251 *wl = hw->priv;
	unsigned long flags;

	wl1251_info("down");

	wl1251_debug(DEBUG_MAC80211, "mac80211 stop");

	/* complete/cancel ongoing work */
	cancel_work_sync(&wl->filter_work);
	spin_lock_irqsave(&wl->wl_lock, flags);
	kfree(wl->filter_params);
	wl->filter_params = NULL;
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	unregister_inetaddr_notifier(&wl1251_dev_notifier);

	mutex_lock(&wl->mutex);

	WARN_ON(wl->state != WL1251_STATE_ON);

	if (wl->scanning) {
		mutex_unlock(&wl->mutex);
		ieee80211_scan_completed(wl->hw);
		mutex_lock(&wl->mutex);
		wl->scanning = false;
	}

	wl->state = WL1251_STATE_OFF;

	wl1251_disable_interrupts(wl);

	mutex_unlock(&wl->mutex);

	cancel_work_sync(&wl->irq_work);
	cancel_work_sync(&wl->tx_work);
	cancel_work_sync(&wl->filter_work);

	mutex_lock(&wl->mutex);

	/* let's notify MAC80211 about the remaining pending TX frames */
	wl1251_tx_flush(wl);
	wl1251_power_off(wl);

	memset(wl->bssid, 0, ETH_ALEN);
	memset(wl->ssid, 0, IW_ESSID_MAX_SIZE + 1);
	wl->ssid_len = 0;
	wl->listen_int = 1;
	wl->bss_type = MAX_BSS_TYPE;

	wl->data_in_count = 0;
	wl->rx_counter = 0;
	wl->rx_handled = 0;
	wl->rx_current_buffer = 0;
	wl->rx_last_id = 0;
	wl->next_tx_complete = 0;
	wl->elp = false;
	wl->psm = 0;
	wl->ps_entry_retry = 0;
	wl->tx_queue_stopped = false;
	wl->power_level = WL1251_DEFAULT_POWER_LEVEL;
	wl->channel = WL1251_DEFAULT_CHANNEL;
	wl->last_event = 0;

	wl1251_debugfs_reset(wl);

	mutex_unlock(&wl->mutex);
}

static int wl1251_op_add_interface(struct ieee80211_hw *hw,
				   struct ieee80211_if_init_conf *conf)
{
	struct wl1251 *wl = hw->priv;
	DECLARE_MAC_BUF(mac);
	int ret = 0;

	wl1251_debug(DEBUG_MAC80211, "mac80211 add interface type %d mac %s",
		     conf->type, print_mac(mac, conf->mac_addr));

	mutex_lock(&wl->mutex);
	if (wl->vif) {
		ret = -EBUSY;
		goto out;
	}

	wl->vif = conf->vif;

	switch (conf->type) {
	case NL80211_IFTYPE_STATION:
		wl->bss_type = BSS_TYPE_STA_BSS;
		break;
	case NL80211_IFTYPE_ADHOC:
		wl->bss_type = BSS_TYPE_IBSS;
		break;
	default:
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (memcmp(wl->mac_addr, conf->mac_addr, ETH_ALEN)) {
		memcpy(wl->mac_addr, conf->mac_addr, ETH_ALEN);
		SET_IEEE80211_PERM_ADDR(wl->hw, wl->mac_addr);
		ret = wl1251_acx_station_id(wl);
		if (ret < 0)
			goto out;
	}

out:
	mutex_unlock(&wl->mutex);
	return ret;
}

static void wl1251_op_remove_interface(struct ieee80211_hw *hw,
					 struct ieee80211_if_init_conf *conf)
{
	struct wl1251 *wl = hw->priv;

	mutex_lock(&wl->mutex);
	wl1251_debug(DEBUG_MAC80211, "mac80211 remove interface");
	wl->vif = NULL;
	mutex_unlock(&wl->mutex);
}

static int wl1251_build_null_data(struct wl1251 *wl)
{
	struct wl12xx_null_data_template template;

	if (!is_zero_ether_addr(wl->bssid)) {
		memcpy(template.header.da, wl->bssid, ETH_ALEN);
		memcpy(template.header.bssid, wl->bssid, ETH_ALEN);
	} else {
		memset(template.header.da, 0xff, ETH_ALEN);
		memset(template.header.bssid, 0xff, ETH_ALEN);
	}

	memcpy(template.header.sa, wl->mac_addr, ETH_ALEN);
	template.header.frame_ctl = cpu_to_le16(IEEE80211_FTYPE_DATA |
						IEEE80211_STYPE_NULLFUNC |
						IEEE80211_FCTL_TODS);

	return wl1251_cmd_template_set(wl, CMD_NULL_DATA, &template,
				       sizeof(template));

}

static int wl1251_build_ps_poll(struct wl1251 *wl, u16 aid)
{
	struct wl12xx_ps_poll_template template;

	memcpy(template.bssid, wl->bssid, ETH_ALEN);
	memcpy(template.ta, wl->mac_addr, ETH_ALEN);

	/* aid in PS-Poll has its two MSBs each set to 1 */
	template.aid = cpu_to_le16(1 << 15 | 1 << 14 | aid);

	template.fc = cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_PSPOLL);

	return wl1251_cmd_template_set(wl, CMD_PS_POLL, &template,
				       sizeof(template));

}

static void wl1251_update_support_rates(struct wl12xx_beacon_template *beacon)
{
	int index, rate_len;
	u16 size;
	struct wl12xx_ie_rates *rates;
	struct wl12xx_ie_ssid *ssid;
	u8 *ptr;

	ptr = (u8 *)beacon;
	size = sizeof(struct ieee80211_header);

	ptr += size;
	/* Pass through time stamp, beacon interval and capability */
	ptr += (6 * sizeof(u16));

	ssid = (struct wl12xx_ie_ssid *)ptr;
	size = sizeof(struct wl12xx_ie_header) + ssid->header.len;
	ptr += size;

	rates = (struct wl12xx_ie_rates *)ptr;
	rate_len = rates->header.len;
	size = sizeof(struct wl12xx_ie_header) + rate_len;
	for (index = 0; index < rate_len; index++) {
		if (rates->rates[index] == IEEE80211_CCK_RATE_1MB) {
			rates->rates[index] = IEEE80211_BASIC_RATE_MASK |
					      IEEE80211_CCK_RATE_1MB;
		}
		if (rates->rates[index] == IEEE80211_CCK_RATE_2MB) {
			rates->rates[index] = IEEE80211_BASIC_RATE_MASK |
					      IEEE80211_CCK_RATE_2MB;
		}
		if (rates->rates[index] == IEEE80211_CCK_RATE_5MB) {
			rates->rates[index] = IEEE80211_BASIC_RATE_MASK |
					      IEEE80211_CCK_RATE_5MB;
		}
		if (rates->rates[index] == IEEE80211_CCK_RATE_11MB) {
			rates->rates[index] = IEEE80211_BASIC_RATE_MASK |
					      IEEE80211_CCK_RATE_11MB;
		}
	}
}


static int wl1251_op_config_interface(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      struct ieee80211_if_conf *conf)
{
	struct wl1251 *wl = hw->priv;
	struct sk_buff *beacon;
	DECLARE_MAC_BUF(mac);
	bool do_join = false;
	int ret;

	wl1251_debug(DEBUG_MAC80211, "mac80211 config_interface bssid %s",
		     print_mac(mac, conf->bssid));
	wl1251_dump_ascii(DEBUG_MAC80211, "ssid: ", conf->ssid,
			  conf->ssid_len);

	mutex_lock(&wl->mutex);

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	if (!is_zero_ether_addr(conf->bssid))
		do_join = true;

	memcpy(wl->bssid, conf->bssid, ETH_ALEN);

	if (do_join) {
		ret = wl1251_build_null_data(wl);
		if (ret < 0)
			goto out_sleep;
	}

	wl->ssid_len = conf->ssid_len;
	if (wl->ssid_len)
		memcpy(wl->ssid, conf->ssid, wl->ssid_len);

	if (conf->changed & IEEE80211_IFCC_BEACON) {
		beacon = ieee80211_beacon_get(hw, vif);
		wl1251_update_support_rates((struct wl12xx_beacon_template *)
					     beacon->data);

		ret = wl1251_cmd_template_set(wl, CMD_BEACON, beacon->data,
					      beacon->len);

		if (ret < 0) {
			dev_kfree_skb(beacon);
			goto out_sleep;
		}

		ret = wl1251_cmd_template_set(wl, CMD_PROBE_RESP, beacon->data,
					      beacon->len);

		dev_kfree_skb(beacon);

		if (ret < 0)
			goto out_sleep;
	}

	if (do_join) {
		ret = wl1251_join(wl, wl->bss_type, wl->channel,
				  wl->beacon_int, wl->dtim_period);
		if (ret < 0)
			goto out_sleep;
	}

out_sleep:
	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl1251_op_config(struct ieee80211_hw *hw,
			    struct ieee80211_conf *conf)
{
	struct wl1251 *wl = hw->priv;
	int channel, ret = 0;

	channel = ieee80211_frequency_to_channel(conf->channel->center_freq);

	wl1251_debug(DEBUG_MAC80211, "mac80211 config ch %d psm %s power %d",
		     channel,
		     conf->flags & IEEE80211_CONF_PS ? "on" : "off",
		     conf->power_level);

	mutex_lock(&wl->mutex);

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	wl->channel = channel;

	if (conf->flags & IEEE80211_CONF_PS && !wl->psm_requested) {
		wl1251_debug(DEBUG_PSM, "psm enabled");

		wl->psm_requested = true;

		/*
		 * We enter PSM only if we're already associated.
		 * If we're not, we'll enter it when joining an SSID,
		 * through the bss_info_changed() hook.
		 */

		ret = wl1251_ps_set_mode(wl, STATION_POWER_SAVE_MODE);
	} else if (!(conf->flags & IEEE80211_CONF_PS) &&
		   wl->psm_requested) {
		wl1251_debug(DEBUG_PSM, "psm disabled");

		wl->psm_requested = false;

		if (wl->psm)
			ret = wl1251_ps_set_mode(wl, STATION_ACTIVE_MODE);
	}

	if (conf->power_level != wl->power_level) {
		ret = wl1251_acx_tx_power(wl, conf->power_level);
		if (ret < 0)
			goto out_sleep;

		wl->power_level = conf->power_level;
	}

out_sleep:
	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static void wl1251_op_configure_filter(struct ieee80211_hw *hw,
				       unsigned int changed,
				       unsigned int *total,
				       int mc_count,
				       struct dev_addr_list *mc_list)
{
	struct wl1251 *wl = hw->priv;
	struct wl1251_filter_params *fp;
	struct dev_addr_list *mc;
	unsigned long flags;
	int i;
	DECLARE_MAC_BUF(mac);

	wl1251_debug(DEBUG_MAC80211, "mac80211 configure filter");

	*total &= WL1251_SUPPORTED_FILTERS;
	changed &= WL1251_SUPPORTED_FILTERS;

	fp = kzalloc(sizeof(*fp), GFP_ATOMIC);
	if (!fp) {
		wl1251_error("Out of memory setting filters.");
		return;
	}

	/* store current filter config */
	fp->filters = *total;
	fp->changed = changed;

	/* update multicast filtering parameters */
	if (mc_count > ACX_MC_ADDRESS_GROUP_MAX) {
		mc_count = 0;
		fp->filters |= FIF_ALLMULTI;
	}

	fp->mc_list_length = 0;
	mc = mc_list;
	for (i = 0; i < mc_count; i++) {
		if (mc->da_addrlen == ETH_ALEN) {
			wl1251_debug(DEBUG_MAC80211, "multicast mac %s",
				     print_mac(mac, mc->da_addr));
			memcpy(fp->mc_list[fp->mc_list_length],
			       mc->da_addr, ETH_ALEN);
			fp->mc_list_length++;
		} else {
			wl1251_warning("Unknown mc address length.");
		}
		mc = mc->next;
	}

	spin_lock_irqsave(&wl->wl_lock, flags);
	kfree(wl->filter_params);
	wl->filter_params = fp;
	spin_unlock_irqrestore(&wl->wl_lock, flags);

	if (changed == 0)
		/* no filters which we support changed */
		goto out;

	/* FIXME: wl->rx_config and wl->rx_filter are not protected */

	wl->rx_config = WL1251_DEFAULT_RX_CONFIG;
	wl->rx_filter = WL1251_DEFAULT_RX_FILTER;

	if (*total & FIF_PROMISC_IN_BSS) {
		wl->rx_config |= CFG_BSSID_FILTER_EN;
		wl->rx_config |= CFG_RX_ALL_GOOD;
	}
	if (*total & FIF_ALLMULTI)
		/*
		 * CFG_MC_FILTER_EN in rx_config needs to be 0 to receive
		 * all multicast frames
		 */
		wl->rx_config &= ~CFG_MC_FILTER_EN;
	if (*total & FIF_FCSFAIL)
		wl->rx_filter |= CFG_RX_FCS_ERROR;
	if (*total & FIF_BCN_PRBRESP_PROMISC) {
		wl->rx_config &= ~CFG_BSSID_FILTER_EN;
		wl->rx_config &= ~CFG_SSID_FILTER_EN;
	}
	if (*total & FIF_CONTROL)
		wl->rx_filter |= CFG_RX_CTL_EN;
	if (*total & FIF_OTHER_BSS)
		wl->rx_filter &= ~CFG_BSSID_FILTER_EN;

out:
	queue_work(wl->hw->workqueue, &wl->filter_work);
}

/* HW encryption */
static int wl1251_set_key_type(struct wl1251 *wl,
			       struct wl1251_cmd_set_keys *key,
			       enum set_key_cmd cmd,
			       struct ieee80211_key_conf *mac80211_key,
			       const u8 *addr)
{
	switch (mac80211_key->alg) {
	case ALG_WEP:
		if (is_broadcast_ether_addr(addr))
			key->key_type = KEY_WEP_DEFAULT;
		else
			key->key_type = KEY_WEP_ADDR;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
	case ALG_TKIP:
		if (is_broadcast_ether_addr(addr))
			key->key_type = KEY_TKIP_MIC_GROUP;
		else
			key->key_type = KEY_TKIP_MIC_PAIRWISE;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
	case ALG_CCMP:
		if (is_broadcast_ether_addr(addr))
			key->key_type = KEY_AES_GROUP;
		else
			key->key_type = KEY_AES_PAIRWISE;
		mac80211_key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV;
		break;
	default:
		wl1251_error("Unknown key algo 0x%x", mac80211_key->alg);
		return -EOPNOTSUPP;
	}

	return 0;
}

static int wl1251_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			     const u8 *local_addr, const u8 *addr,
			     struct ieee80211_key_conf *key)
{
	struct wl1251 *wl = hw->priv;
	struct wl1251_cmd_set_keys *wl_cmd;
	int ret;

	wl1251_debug(DEBUG_MAC80211, "mac80211 set key");

	wl_cmd = kzalloc(sizeof(*wl_cmd), GFP_KERNEL);
	if (!wl_cmd) {
		ret = -ENOMEM;
		goto out;
	}

	wl1251_debug(DEBUG_CRYPT, "CMD: 0x%x", cmd);
	wl1251_dump(DEBUG_CRYPT, "ADDR: ", addr, ETH_ALEN);
	wl1251_dump(DEBUG_CRYPT, "LOCAL_ADDR: ", local_addr, ETH_ALEN);
	wl1251_debug(DEBUG_CRYPT, "Key: algo:0x%x, id:%d, len:%d flags 0x%x",
		     key->alg, key->keyidx, key->keylen, key->flags);
	wl1251_dump(DEBUG_CRYPT, "KEY: ", key->key, key->keylen);

	if (is_zero_ether_addr(addr)) {
		/* We dont support TX only encryption */
		ret = -EOPNOTSUPP;
		goto out;
	}

	mutex_lock(&wl->mutex);

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out_unlock;

	switch (cmd) {
	case SET_KEY:
		wl_cmd->key_action = KEY_ADD_OR_REPLACE;
		break;
	case DISABLE_KEY:
		wl_cmd->key_action = KEY_REMOVE;
		break;
	default:
		wl1251_error("Unsupported key cmd 0x%x", cmd);
		break;
	}

	ret = wl1251_set_key_type(wl, wl_cmd, cmd, key, addr);
	if (ret < 0) {
		wl1251_error("Set KEY type failed");
		goto out_sleep;
	}

	if (wl_cmd->key_type != KEY_WEP_DEFAULT)
		memcpy(wl_cmd->addr, addr, ETH_ALEN);

	if ((wl_cmd->key_type == KEY_TKIP_MIC_GROUP) ||
	    (wl_cmd->key_type == KEY_TKIP_MIC_PAIRWISE)) {
		/*
		 * We get the key in the following form:
		 * TKIP (16 bytes) - TX MIC (8 bytes) - RX MIC (8 bytes)
		 * but the target is expecting:
		 * TKIP - RX MIC - TX MIC
		 */
		memcpy(wl_cmd->key, key->key, 16);
		memcpy(wl_cmd->key + 16, key->key + 24, 8);
		memcpy(wl_cmd->key + 24, key->key + 16, 8);

	} else {
		memcpy(wl_cmd->key, key->key, key->keylen);
	}
	wl_cmd->key_size = key->keylen;

	wl_cmd->id = key->keyidx;
	wl_cmd->ssid_profile = 0;

	wl1251_dump(DEBUG_CRYPT, "TARGET KEY: ", wl_cmd, sizeof(*wl_cmd));

	ret = wl1251_cmd_send(wl, CMD_SET_KEYS, wl_cmd, sizeof(*wl_cmd));
	if (ret < 0) {
		wl1251_warning("could not set keys");
		goto out_sleep;
	}

out_sleep:
	wl1251_ps_elp_sleep(wl);

out_unlock:
	mutex_unlock(&wl->mutex);

out:
	kfree(wl_cmd);

	return ret;
}

static int wl1251_build_basic_rates(char *rates)
{
	u8 index = 0;

	rates[index++] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_1MB;
	rates[index++] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_2MB;
	rates[index++] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_5MB;
	rates[index++] = IEEE80211_BASIC_RATE_MASK | IEEE80211_CCK_RATE_11MB;

	return index;
}

static int wl1251_build_extended_rates(char *rates)
{
	u8 index = 0;

	rates[index++] = IEEE80211_OFDM_RATE_6MB;
	rates[index++] = IEEE80211_OFDM_RATE_9MB;
	rates[index++] = IEEE80211_OFDM_RATE_12MB;
	rates[index++] = IEEE80211_OFDM_RATE_18MB;
	rates[index++] = IEEE80211_OFDM_RATE_24MB;
	rates[index++] = IEEE80211_OFDM_RATE_36MB;
	rates[index++] = IEEE80211_OFDM_RATE_48MB;
	rates[index++] = IEEE80211_OFDM_RATE_54MB;

	return index;
}


static int wl1251_build_probe_req(struct wl1251 *wl, u8 *ssid, size_t ssid_len)
{
	struct wl12xx_probe_req_template template;
	struct wl12xx_ie_rates *rates;
	char *ptr;
	u16 size;

	ptr = (char *)&template;
	size = sizeof(struct ieee80211_header);

	memset(template.header.da, 0xff, ETH_ALEN);
	memset(template.header.bssid, 0xff, ETH_ALEN);
	memcpy(template.header.sa, wl->mac_addr, ETH_ALEN);
	template.header.frame_ctl = cpu_to_le16(IEEE80211_STYPE_PROBE_REQ);

	/* IEs */
	/* SSID */
	template.ssid.header.id = WLAN_EID_SSID;
	template.ssid.header.len = ssid_len;
	if (ssid_len && ssid)
		memcpy(template.ssid.ssid, ssid, ssid_len);
	size += sizeof(struct wl12xx_ie_header) + ssid_len;
	ptr += size;

	/* Basic Rates */
	rates = (struct wl12xx_ie_rates *)ptr;
	rates->header.id = WLAN_EID_SUPP_RATES;
	rates->header.len = wl1251_build_basic_rates(rates->rates);
	size += sizeof(struct wl12xx_ie_header) + rates->header.len;
	ptr += sizeof(struct wl12xx_ie_header) + rates->header.len;

	/* Extended rates */
	rates = (struct wl12xx_ie_rates *)ptr;
	rates->header.id = WLAN_EID_EXT_SUPP_RATES;
	rates->header.len = wl1251_build_extended_rates(rates->rates);
	size += sizeof(struct wl12xx_ie_header) + rates->header.len;

	wl1251_dump(DEBUG_SCAN, "PROBE REQ: ", &template, size);

	return wl1251_cmd_template_set(wl, CMD_PROBE_REQ, &template,
				      size);
}

static int wl1251_hw_scan(struct wl1251 *wl, u8 *ssid, size_t len,
			  u8 active_scan, u8 high_prio, u8 num_channels,
			  u8 probe_requests)
{
	struct wl1251_cmd_trigger_scan_to *trigger = NULL;
	struct cmd_scan *params = NULL;
	int i, ret;
	u16 scan_options = 0;

	if (wl->scanning)
		return -EINVAL;

	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	params->params.rx_config_options = cpu_to_le32(CFG_RX_ALL_GOOD);
	params->params.rx_filter_options =
		cpu_to_le32(CFG_RX_PRSP_EN | CFG_RX_MGMT_EN | CFG_RX_BCN_EN);

	/* High priority scan */
	if (!active_scan)
		scan_options |= SCAN_PASSIVE;
	if (high_prio)
		scan_options |= SCAN_PRIORITY_HIGH;
	params->params.scan_options = scan_options;

	params->params.num_channels = num_channels;
	params->params.num_probe_requests = probe_requests;
	params->params.tx_rate = cpu_to_le16(1 << 1); /* 2 Mbps */
	params->params.tid_trigger = 0;

	for (i = 0; i < num_channels; i++) {
		params->channels[i].min_duration = cpu_to_le32(30000);
		params->channels[i].max_duration = cpu_to_le32(60000);
		memset(&params->channels[i].bssid_lsb, 0xff, 4);
		memset(&params->channels[i].bssid_msb, 0xff, 2);
		params->channels[i].early_termination = 0;
		params->channels[i].tx_power_att = 0;
		params->channels[i].channel = i + 1;
		memset(params->channels[i].pad, 0, 3);
	}

	for (i = num_channels; i < SCAN_MAX_NUM_OF_CHANNELS; i++)
		memset(&params->channels[i], 0,
		       sizeof(struct basic_scan_channel_parameters));

	if (len && ssid) {
		params->params.ssid_len = len;
		memcpy(params->params.ssid, ssid, len);
	} else {
		params->params.ssid_len = 0;
		memset(params->params.ssid, 0, 32);
	}

	ret = wl1251_build_probe_req(wl, ssid, len);
	if (ret < 0) {
		wl1251_error("PROBE request template failed");
		goto out;
	}

	trigger = kzalloc(sizeof(*trigger), GFP_KERNEL);
	if (!trigger) {
		ret = -ENOMEM;
		goto out;
	}

	trigger->timeout = 0;

	ret = wl1251_cmd_send(wl, CMD_TRIGGER_SCAN_TO, trigger,
			      sizeof(*trigger));
	if (ret < 0) {
		wl1251_error("trigger scan to failed for hw scan");
		goto out;
	}

	wl1251_dump(DEBUG_SCAN, "SCAN: ", params, sizeof(*params));

	wl->scanning = true;

	ret = wl1251_cmd_send(wl, CMD_SCAN, params, sizeof(*params));
	if (ret < 0)
		wl1251_error("SCAN failed");

	wl1251_spi_mem_read(wl, wl->cmd_box_addr, params, sizeof(*params));

	if (params->header.status != CMD_STATUS_SUCCESS) {
		wl1251_error("TEST command answer error: %d",
			     params->header.status);
		wl->scanning = false;
		ret = -EIO;
		goto out;
	}

out:
	kfree(trigger);
	kfree(params);
	return ret;

}

static int wl1251_op_hw_scan(struct ieee80211_hw *hw, u8 *ssid, size_t len)
{
	struct wl1251 *wl = hw->priv;
	int ret;

	wl1251_debug(DEBUG_MAC80211, "mac80211 hw scan");

	mutex_lock(&wl->mutex);

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1251_hw_scan(hw->priv, ssid, len, 1, 0, 13, 3);

	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static int wl1251_op_set_rts_threshold(struct ieee80211_hw *hw, u32 value)
{
	struct wl1251 *wl = hw->priv;
	int ret;

	mutex_lock(&wl->mutex);

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	ret = wl1251_acx_rts_threshold(wl, (u16) value);
	if (ret < 0)
		wl1251_warning("wl1251_op_set_rts_threshold failed: %d", ret);

	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

static void wl1251_op_bss_info_changed(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_bss_conf *bss_conf,
				       u32 changed)
{
	enum wl1251_cmd_ps_mode mode;
	struct wl1251 *wl = hw->priv;
	int ret;

	wl1251_debug(DEBUG_MAC80211, "mac80211 bss info changed");

	mutex_lock(&wl->mutex);

	ret = wl1251_ps_elp_wakeup(wl);
	if (ret < 0)
		goto out;

	if (changed & BSS_CHANGED_ASSOC) {
		if (bss_conf->assoc) {
			wl->beacon_int = bss_conf->beacon_int;
			wl->dtim_period = bss_conf->dtim_period;

			ret = wl1251_acx_wr_tbtt_and_dtim(wl, wl->beacon_int,
							  wl->dtim_period);
			wl->aid = bss_conf->aid;

			ret = wl1251_build_ps_poll(wl, wl->aid);
			if (ret < 0)
				goto out_sleep;

			ret = wl1251_acx_aid(wl, wl->aid);
			if (ret < 0)
				goto out_sleep;

			/* If we want to go in PSM but we're not there yet */
			if (wl->psm_requested && !wl->psm) {
				mode = STATION_POWER_SAVE_MODE;
				ret = wl1251_ps_set_mode(wl, mode);
				if (ret < 0)
					goto out_sleep;
			}
		} else {
			/* use defaults when not associated */
			wl->beacon_int = WL1251_DEFAULT_BEACON_INT;
			wl->dtim_period = WL1251_DEFAULT_DTIM_PERIOD;
		}
	}
	if (changed & BSS_CHANGED_ERP_SLOT) {
		if (bss_conf->use_short_slot)
			ret = wl1251_acx_slot(wl, SLOT_TIME_SHORT);
		else
			ret = wl1251_acx_slot(wl, SLOT_TIME_LONG);
		if (ret < 0) {
			wl1251_warning("Set slot time failed %d", ret);
			goto out_sleep;
		}
	}

	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		if (bss_conf->use_short_preamble)
			wl1251_acx_set_preamble(wl, ACX_PREAMBLE_SHORT);
		else
			wl1251_acx_set_preamble(wl, ACX_PREAMBLE_LONG);
	}

	if (changed & BSS_CHANGED_ERP_CTS_PROT) {
		if (bss_conf->use_cts_prot)
			ret = wl1251_acx_cts_protect(wl, CTSPROTECT_ENABLE);
		else
			ret = wl1251_acx_cts_protect(wl, CTSPROTECT_DISABLE);
		if (ret < 0) {
			wl1251_warning("Set ctsprotect failed %d", ret);
			goto out_sleep;
		}
	}

out_sleep:
	wl1251_ps_elp_sleep(wl);

out:
	mutex_unlock(&wl->mutex);
}

static int wl1251_op_conf_tx(struct ieee80211_hw *hw, u16 queue,
			     const struct ieee80211_tx_queue_params *params)
{
	struct wl1251 *wl = hw->priv;
	int ret;

	mutex_lock(&wl->mutex);

	wl1251_debug(DEBUG_MAC80211, "mac80211 conf tx %d", queue);

	/* mac80211 txop unit is 32 usecs */
	ret = wl1251_acx_ac_cfg(wl, wl1251_tx_get_queue(queue),
				params->cw_min, params->cw_max,
				params->aifs, params->txop * 32);
	if (ret < 0)
		goto out;

	ret = wl1251_acx_tid_cfg(wl, wl1251_tx_get_queue(queue),
				 CHANNEL_TYPE_EDCF,
				 wl1251_tx_get_queue(queue),
				 WL1251_ACX_PS_SCHEME_LEGACY,
				 WL1251_ACX_ACK_POLICY_LEGACY);
	if (ret < 0)
		goto out;

out:
	mutex_unlock(&wl->mutex);

	return ret;
}

/* can't be const, mac80211 writes to this */
static struct ieee80211_rate wl1251_rates[] = {
	{ .bitrate = 10,
	  .hw_value = 0x1,
	  .hw_value_short = 0x1, },
	{ .bitrate = 20,
	  .hw_value = 0x2,
	  .hw_value_short = 0x2,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55,
	  .hw_value = 0x4,
	  .hw_value_short = 0x4,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110,
	  .hw_value = 0x20,
	  .hw_value_short = 0x20,
	  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 60,
	  .hw_value = 0x8,
	  .hw_value_short = 0x8, },
	{ .bitrate = 90,
	  .hw_value = 0x10,
	  .hw_value_short = 0x10, },
	{ .bitrate = 120,
	  .hw_value = 0x40,
	  .hw_value_short = 0x40, },
	{ .bitrate = 180,
	  .hw_value = 0x80,
	  .hw_value_short = 0x80, },
	{ .bitrate = 240,
	  .hw_value = 0x200,
	  .hw_value_short = 0x200, },
	{ .bitrate = 360,
	 .hw_value = 0x400,
	 .hw_value_short = 0x400, },
	{ .bitrate = 480,
	  .hw_value = 0x800,
	  .hw_value_short = 0x800, },
	{ .bitrate = 540,
	  .hw_value = 0x1000,
	  .hw_value_short = 0x1000, },
};

/* can't be const, mac80211 writes to this */
static struct ieee80211_channel wl1251_channels[] = {
	{ .hw_value = 1, .center_freq = 2412},
	{ .hw_value = 2, .center_freq = 2417},
	{ .hw_value = 3, .center_freq = 2422},
	{ .hw_value = 4, .center_freq = 2427},
	{ .hw_value = 5, .center_freq = 2432},
	{ .hw_value = 6, .center_freq = 2437},
	{ .hw_value = 7, .center_freq = 2442},
	{ .hw_value = 8, .center_freq = 2447},
	{ .hw_value = 9, .center_freq = 2452},
	{ .hw_value = 10, .center_freq = 2457},
	{ .hw_value = 11, .center_freq = 2462},
	{ .hw_value = 12, .center_freq = 2467},
	{ .hw_value = 13, .center_freq = 2472},
};

/* can't be const, mac80211 writes to this */
static struct ieee80211_supported_band wl1251_band_2ghz = {
	.channels = wl1251_channels,
	.n_channels = ARRAY_SIZE(wl1251_channels),
	.bitrates = wl1251_rates,
	.n_bitrates = ARRAY_SIZE(wl1251_rates),
};

static const struct ieee80211_ops wl1251_ops = {
	.start = wl1251_op_start,
	.stop = wl1251_op_stop,
	.add_interface = wl1251_op_add_interface,
	.remove_interface = wl1251_op_remove_interface,
	.config = wl1251_op_config,
	.config_interface = wl1251_op_config_interface,
	.configure_filter = wl1251_op_configure_filter,
	.tx = wl1251_op_tx,
	.set_key = wl1251_op_set_key,
	.hw_scan = wl1251_op_hw_scan,
	.bss_info_changed = wl1251_op_bss_info_changed,
	.set_rts_threshold = wl1251_op_set_rts_threshold,
	.conf_tx = wl1251_op_conf_tx,
};

static int wl1251_register_hw(struct wl1251 *wl)
{
	int ret;

	if (wl->mac80211_registered)
		return 0;

	SET_IEEE80211_PERM_ADDR(wl->hw, wl->mac_addr);

	ret = ieee80211_register_hw(wl->hw);
	if (ret < 0) {
		wl1251_error("unable to register mac80211 hw: %d", ret);
		return ret;
	}

	wl->mac80211_registered = true;

	wl1251_notice("loaded");

	return 0;
}

static int wl1251_init_ieee80211(struct wl1251 *wl)
{
	/* The tx descriptor buffer and the TKIP space */
	wl->hw->extra_tx_headroom = sizeof(struct tx_double_buffer_desc)
		+ WL1251_TKIP_IV_SPACE;

	/* unit us */
	/* FIXME: find a proper value */
	wl->hw->channel_change_time = 10000;

	wl->hw->flags = IEEE80211_HW_SIGNAL_DBM |
		IEEE80211_HW_NOISE_DBM |
		IEEE80211_HW_BEACON_FILTER;

	wl->hw->wiphy->bands[IEEE80211_BAND_2GHZ] = &wl1251_band_2ghz;

	wl->hw->queues = 4;

	SET_IEEE80211_DEV(wl->hw, &wl->spi->dev);

	return 0;
}

static void wl1251_device_release(struct device *dev)
{

}

static struct platform_device wl1251_device = {
	/* FIXME: use wl12xx name to not break the user space */
	.name		= "wl12xx",
	.id		= -1,

	/* device model insists to have a release function */
	.dev            = {
		.release = wl1251_device_release,
	},
};

static int __devinit wl1251_probe(struct spi_device *spi)
{
	struct wl12xx_platform_data *pdata;
	struct ieee80211_hw *hw;
	struct wl1251 *wl;
	int ret, i;
	static const u8 nokia_oui[3] = {0x00, 0x1f, 0xdf};

	pdata = spi->dev.platform_data;
	if (!pdata) {
		wl1251_error("no platform data");
		return -ENODEV;
	}

	hw = ieee80211_alloc_hw(sizeof(*wl), &wl1251_ops);
	if (!hw) {
		wl1251_error("could not alloc ieee80211_hw");
		return -ENOMEM;
	}

	wl = hw->priv;
	memset(wl, 0, sizeof(*wl));

	wl->hw = hw;
	dev_set_drvdata(&spi->dev, wl);
	wl->spi = spi;

	wl->data_in_count = 0;

	skb_queue_head_init(&wl->tx_queue);

	INIT_WORK(&wl->filter_work, wl1251_filter_work);
	INIT_DELAYED_WORK(&wl->elp_work, wl1251_elp_work);
	wl->channel = WL1251_DEFAULT_CHANNEL;
	wl->scanning = false;
	wl->default_key = 0;
	wl->listen_int = 1;
	wl->rx_counter = 0;
	wl->rx_handled = 0;
	wl->rx_current_buffer = 0;
	wl->rx_last_id = 0;
	wl->rx_config = WL1251_DEFAULT_RX_CONFIG;
	wl->rx_filter = WL1251_DEFAULT_RX_FILTER;
	wl->elp = false;
	wl->psm = 0;
	wl->psm_requested = false;
	wl->ps_entry_retry = 0;
	wl->tx_queue_stopped = false;
	wl->power_level = WL1251_DEFAULT_POWER_LEVEL;
	wl->beacon_int = WL1251_DEFAULT_BEACON_INT;
	wl->dtim_period = WL1251_DEFAULT_DTIM_PERIOD;
	wl->vif = NULL;
	wl->bt_coex_mode = WL1251_BT_COEX_OFF;
	wl->last_event = 0;

	for (i = 0; i < FW_TX_CMPLT_BLOCK_SIZE; i++)
		wl->tx_frames[i] = NULL;

	wl->next_tx_complete = 0;

	INIT_WORK(&wl->irq_work, wl1251_irq_work);
	INIT_WORK(&wl->tx_work, wl1251_tx_work);

	spin_lock_init(&wl->wl_lock);

	/*
	 * In case our MAC address is not correctly set,
	 * we use a random but Nokia MAC.
	 */
	memcpy(wl->mac_addr, nokia_oui, 3);
	get_random_bytes(wl->mac_addr + 3, 3);

	wl->state = WL1251_STATE_OFF;
	mutex_init(&wl->mutex);

	wl->tx_mgmt_frm_rate = DEFAULT_HW_GEN_TX_RATE;
	wl->tx_mgmt_frm_mod = DEFAULT_HW_GEN_MODULATION_TYPE;

	wl->rx_descriptor = kmalloc(sizeof(*wl->rx_descriptor), GFP_KERNEL);
	if (!wl->rx_descriptor) {
		wl1251_error("could not allocate memory for rx descriptor");
		ret = -ENOMEM;
		goto out_free;
	}

	/* This is the only SPI value that we need to set here, the rest
	 * comes from the board-peripherals file */
	spi->bits_per_word = 32;

	ret = spi_setup(spi);
	if (ret < 0) {
		wl1251_error("spi_setup failed");
		goto out_free;
	}

	wl->set_power = pdata->set_power;
	if (!wl->set_power) {
		wl1251_error("set power function missing in platform data");
		ret = -ENODEV;
		goto out_free;
	}

	wl->irq = spi->irq;
	if (wl->irq < 0) {
		wl1251_error("irq missing in platform data");
		ret = -ENODEV;
		goto out_free;
	}

	ret = request_irq(wl->irq, wl1251_irq, 0, DRIVER_NAME, wl);
	if (ret < 0) {
		wl1251_error("request_irq() failed: %d", ret);
		goto out_free;
	}

	set_irq_type(wl->irq, IRQ_TYPE_EDGE_RISING);

	disable_irq(wl->irq);

	ret = platform_device_register(&wl1251_device);
	if (ret) {
		wl1251_error("couldn't register platform device");
		goto out_irq;
	}
	dev_set_drvdata(&wl1251_device.dev, wl);

	ret = wl1251_init_ieee80211(wl);
	if (ret)
		goto out_platform;

	ret = wl1251_register_hw(wl);
	if (ret)
		goto out_platform;

	ret = wl1251_nl_register();
	if (ret)
		goto out_register_hw;

	ret = device_create_file(&wl1251_device.dev,
				 &dev_attr_tx_mgmt_frm_rate);
	if (ret < 0) {
		wl1251_error("failed to create sysfs file tx_mgmt_frm_rate");
		goto out_register_hw;
	}

	ret = device_create_file(&wl1251_device.dev, &dev_attr_bt_coex_mode);
	if (ret < 0) {
		wl1251_error("failed to create sysfs file bt_coex_mode");
		goto out_register_hw;
	}

	wl1251_debugfs_init(wl);

	wl1251_notice("initialized");

	return 0;

 out_register_hw:
	ieee80211_unregister_hw(hw);
	wl->mac80211_registered = false;

 out_platform:
	platform_device_unregister(&wl1251_device);

 out_irq:
	free_irq(wl->irq, wl);

 out_free:
	kfree(wl->rx_descriptor);
	wl->rx_descriptor = NULL;

	ieee80211_free_hw(hw);

	return ret;
}

static int __devexit wl1251_remove(struct spi_device *spi)
{
	struct wl1251 *wl = dev_get_drvdata(&spi->dev);

	ieee80211_unregister_hw(wl->hw);

	wl1251_debugfs_exit(wl);
	platform_device_unregister(&wl1251_device);
	free_irq(wl->irq, wl);
	kfree(wl->target_mem_map);
	kfree(wl->data_path);
	vfree(wl->fw);
	wl->fw = NULL;
	kfree(wl->nvs);
	wl->nvs = NULL;

	kfree(wl->rx_descriptor);
	wl->rx_descriptor = NULL;

	kfree(wl->fw_status);

	ieee80211_free_hw(wl->hw);
	wl1251_nl_unregister();

	return 0;
}


static struct spi_driver wl1251_spi_driver = {
	.driver = {
		/* FIXME: use wl12xx name to not break the user space */
		.name		= "wl12xx",
		.bus		= &spi_bus_type,
		.owner		= THIS_MODULE,
	},

	.probe		= wl1251_probe,
	.remove		= __devexit_p(wl1251_remove),
};

static int __init wl1251_init(void)
{
	int ret;

	ret = spi_register_driver(&wl1251_spi_driver);
	if (ret < 0) {
		wl1251_error("failed to register spi driver: %d", ret);
		goto out;
	}

out:
	return ret;
}

static void __exit wl1251_exit(void)
{
	spi_unregister_driver(&wl1251_spi_driver);

	wl1251_notice("unloaded");
}

module_init(wl1251_init);
module_exit(wl1251_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kalle Valo <kalle.valo@nokia.com>");
