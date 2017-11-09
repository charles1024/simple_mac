/*
 * simple.h
 *
 *  Created on: Oct 31, 2017
 *      Author: 1109052
 */

#ifndef SRC_simple_H_
#define SRC_simple_H_

#include <linux/usb.h>
#include <linux/skbuff.h>
#include <net/mac80211.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <linux/scatterlist.h>
#include <net/cfg80211.h>




#define MIN_TXPOWER	31
#define MAX_TXPOWER	62
#define DEFAULT_TXPOWER	39

typedef unsigned int u32;
//typedef unsigned long u64;
/*
 * Interface combinations
 */
enum {
	IF_COMB_AP = 0,
	NUM_IF_COMB,
};

enum rate_modulation {
	RATE_MODE_CCK = 0,
	RATE_MODE_OFDM = 1,
	RATE_MODE_HT_MIX = 2,
	RATE_MODE_HT_GREENFIELD = 3,
};

#define RATE_MCS(__mode, __mcs) \
	((((__mode) & 0x00ff) << 8) | ((__mcs) & 0x00ff))

struct simple_rate {
	unsigned short flags;
#define DEV_RATE_CCK			0x0001
#define DEV_RATE_OFDM			0x0002
#define DEV_RATE_SHORT_PREAMBLE		0x0004

	unsigned short bitrate; /* In 100kbit/s */
	unsigned short ratemask;

	unsigned short plcp;
	unsigned short mcs;
};

/*
 * Driver initialization handlers.
 */
const struct simple_rate simple_supported_rates[12] = {
	{
		.flags = DEV_RATE_CCK,
		.bitrate = 10,
		.ratemask = BIT(0),
		.plcp = 0x00,
		.mcs = RATE_MCS(RATE_MODE_CCK, 0),
	},
	{
		.flags = DEV_RATE_CCK | DEV_RATE_SHORT_PREAMBLE,
		.bitrate = 20,
		.ratemask = BIT(1),
		.plcp = 0x01,
		.mcs = RATE_MCS(RATE_MODE_CCK, 1),
	},
	{
		.flags = DEV_RATE_CCK | DEV_RATE_SHORT_PREAMBLE,
		.bitrate = 55,
		.ratemask = BIT(2),
		.plcp = 0x02,
		.mcs = RATE_MCS(RATE_MODE_CCK, 2),
	},
	{
		.flags = DEV_RATE_CCK | DEV_RATE_SHORT_PREAMBLE,
		.bitrate = 110,
		.ratemask = BIT(3),
		.plcp = 0x03,
		.mcs = RATE_MCS(RATE_MODE_CCK, 3),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 60,
		.ratemask = BIT(4),
		.plcp = 0x0b,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 0),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 90,
		.ratemask = BIT(5),
		.plcp = 0x0f,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 1),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 120,
		.ratemask = BIT(6),
		.plcp = 0x0a,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 2),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 180,
		.ratemask = BIT(7),
		.plcp = 0x0e,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 3),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 240,
		.ratemask = BIT(8),
		.plcp = 0x09,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 4),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 360,
		.ratemask = BIT(9),
		.plcp = 0x0d,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 5),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 480,
		.ratemask = BIT(10),
		.plcp = 0x08,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 6),
	},
	{
		.flags = DEV_RATE_OFDM,
		.bitrate = 540,
		.ratemask = BIT(11),
		.plcp = 0x0c,
		.mcs = RATE_MCS(RATE_MODE_OFDM, 7),
	},
};

static inline const struct simple_rate *simple_get_rate(const u16 hw_value)
{
	return &simple_supported_rates[hw_value & 0xff];
}

/*
 * RF register values that belong to a particular channel.
 */
struct rf_channel {
	int channel;
	u32 rf1;
	u32 rf2;
	u32 rf3;
	u32 rf4;
};

/*
 * Channel information structure
 */
struct channel_info {
	unsigned int flags;
#define GEOGRAPHY_ALLOWED	0x00000001

	short max_power;
	short default_power1;
	short default_power2;
	short default_power3;
};

struct hw_mode_spec {
	unsigned int supported_bands;
#define SUPPORT_BAND_2GHZ	0x00000001
#define SUPPORT_BAND_5GHZ	0x00000002

	unsigned int supported_rates;
#define SUPPORT_RATE_CCK	0x00000001
#define SUPPORT_RATE_OFDM	0x00000002

	unsigned int num_channels;
	const struct rf_channel *channels;
	const struct channel_info *channels_info;

	struct ieee80211_sta_ht_cap ht;
};

/*
 * simple device structure.
 */
struct simple_dev {
	//struct ath_common ath_common;
	struct ieee80211_hw *hw;
	struct ieee80211_ops *ops;
	struct device *dev;
	unsigned char mac_addr[ETH_ALEN];
	void *drv_data;
	struct hw_mode_spec spec;

	struct ieee80211_iface_limit if_limits_ap;
	struct ieee80211_iface_combination if_combinations[NUM_IF_COMB];

	struct ieee80211_supported_band bands[3];
	enum nl80211_band curr_band;
	int curr_freq;
	unsigned int packet_filter;

	unsigned int intf_ap_count;
	unsigned int intf_sta_count;
	unsigned int intf_associated;
	unsigned int intf_beaconing;

};
/*
 * Interface structure
 * Per interface configuration details, this structure
 * is allocated as the private data for ieee80211_vif.
 */
struct simple_intf {
	/*
	 * beacon->skb must be protected with the mutex.
	 */
	struct mutex beacon_skb_mutex;

	/*
	 * Entry in the beacon queue which belongs to
	 * this interface. Each interface has its own
	 * dedicated beacon entry.
	 */
	//struct queue_entry *beacon;
	//bool enable_beacon;
	/*
	 * Actions that needed rescheduling.
	 */
	unsigned long delayed_flags;

/*
 * Software sequence counter, this is only required
 * for hardware which doesn't support hardware
 * sequence counting.
 */
//atomic_t seqno;
};
/*
 * RF value list for RF2528
 * Supports: 2.4 GHz
 */
static const struct rf_channel rf_vals_bg_2528[] = {
	{ 1,  0x00002c0c, 0x00000786, 0x00068255, 0x000fea0b },
	{ 2,  0x00002c0c, 0x00000786, 0x00068255, 0x000fea1f },
	{ 3,  0x00002c0c, 0x0000078a, 0x00068255, 0x000fea0b },
	{ 4,  0x00002c0c, 0x0000078a, 0x00068255, 0x000fea1f },
	{ 5,  0x00002c0c, 0x0000078e, 0x00068255, 0x000fea0b },
	{ 6,  0x00002c0c, 0x0000078e, 0x00068255, 0x000fea1f },
	{ 7,  0x00002c0c, 0x00000792, 0x00068255, 0x000fea0b },
	{ 8,  0x00002c0c, 0x00000792, 0x00068255, 0x000fea1f },
	{ 9,  0x00002c0c, 0x00000796, 0x00068255, 0x000fea0b },
	{ 10, 0x00002c0c, 0x00000796, 0x00068255, 0x000fea1f },
	{ 11, 0x00002c0c, 0x0000079a, 0x00068255, 0x000fea0b },
	{ 12, 0x00002c0c, 0x0000079a, 0x00068255, 0x000fea1f },
	{ 13, 0x00002c0c, 0x0000079e, 0x00068255, 0x000fea0b },
	{ 14, 0x00002c0c, 0x000007a2, 0x00068255, 0x000fea13 },
};
static const struct rf_channel rf_vals_5226[] = {
	{ 1,  0x00002c0c, 0x00000786, 0x00068255, 0x000fea0b },
	{ 2,  0x00002c0c, 0x00000786, 0x00068255, 0x000fea1f },
	{ 3,  0x00002c0c, 0x0000078a, 0x00068255, 0x000fea0b },
	{ 4,  0x00002c0c, 0x0000078a, 0x00068255, 0x000fea1f },
	{ 5,  0x00002c0c, 0x0000078e, 0x00068255, 0x000fea0b },
	{ 6,  0x00002c0c, 0x0000078e, 0x00068255, 0x000fea1f },
	{ 7,  0x00002c0c, 0x00000792, 0x00068255, 0x000fea0b },
	{ 8,  0x00002c0c, 0x00000792, 0x00068255, 0x000fea1f },
	{ 9,  0x00002c0c, 0x00000796, 0x00068255, 0x000fea0b },
	{ 10, 0x00002c0c, 0x00000796, 0x00068255, 0x000fea1f },
	{ 11, 0x00002c0c, 0x0000079a, 0x00068255, 0x000fea0b },
	{ 12, 0x00002c0c, 0x0000079a, 0x00068255, 0x000fea1f },
	{ 13, 0x00002c0c, 0x0000079e, 0x00068255, 0x000fea0b },
	{ 14, 0x00002c0c, 0x000007a2, 0x00068255, 0x000fea13 },

	/* 802.11 UNI / HyperLan 2 */
	{ 36, 0x00002c0c, 0x0000099a, 0x00098255, 0x000fea23 },
	{ 40, 0x00002c0c, 0x000009a2, 0x00098255, 0x000fea03 },
	{ 44, 0x00002c0c, 0x000009a6, 0x00098255, 0x000fea0b },
	{ 48, 0x00002c0c, 0x000009aa, 0x00098255, 0x000fea13 },
	{ 52, 0x00002c0c, 0x000009ae, 0x00098255, 0x000fea1b },
	{ 56, 0x00002c0c, 0x000009b2, 0x00098255, 0x000fea23 },
	{ 60, 0x00002c0c, 0x000009ba, 0x00098255, 0x000fea03 },
	{ 64, 0x00002c0c, 0x000009be, 0x00098255, 0x000fea0b },

	/* 802.11 HyperLan 2 */
	{ 100, 0x00002c0c, 0x00000a2a, 0x000b8255, 0x000fea03 },
	{ 104, 0x00002c0c, 0x00000a2e, 0x000b8255, 0x000fea0b },
	{ 108, 0x00002c0c, 0x00000a32, 0x000b8255, 0x000fea13 },
	{ 112, 0x00002c0c, 0x00000a36, 0x000b8255, 0x000fea1b },
	{ 116, 0x00002c0c, 0x00000a3a, 0x000b8255, 0x000fea23 },
	{ 120, 0x00002c0c, 0x00000a82, 0x000b8255, 0x000fea03 },
	{ 124, 0x00002c0c, 0x00000a86, 0x000b8255, 0x000fea0b },
	{ 128, 0x00002c0c, 0x00000a8a, 0x000b8255, 0x000fea13 },
	{ 132, 0x00002c0c, 0x00000a8e, 0x000b8255, 0x000fea1b },
	{ 136, 0x00002c0c, 0x00000a92, 0x000b8255, 0x000fea23 },

	/* 802.11 UNII */
	{ 140, 0x00002c0c, 0x00000a9a, 0x000b8255, 0x000fea03 },
	{ 149, 0x00002c0c, 0x00000aa2, 0x000b8255, 0x000fea1f },
	{ 153, 0x00002c0c, 0x00000aa6, 0x000b8255, 0x000fea27 },
	{ 157, 0x00002c0c, 0x00000aae, 0x000b8255, 0x000fea07 },
	{ 161, 0x00002c0c, 0x00000ab2, 0x000b8255, 0x000fea0f },
	{ 165, 0x00002c0c, 0x00000ab6, 0x000b8255, 0x000fea17 },

	/* MMAC(Japan)J52 ch 34,38,42,46 */
	{ 34, 0x00002c0c, 0x0008099a, 0x000da255, 0x000d3a0b },
	{ 38, 0x00002c0c, 0x0008099e, 0x000da255, 0x000d3a13 },
	{ 42, 0x00002c0c, 0x000809a2, 0x000da255, 0x000d3a1b },
	{ 46, 0x00002c0c, 0x000809a6, 0x000da255, 0x000d3a23 },
};


struct simple_dev *simple_mac_create(size_t priv_size);
void simple_mac_destroy(struct simple_dev *ad);


int ksocket_init(void);
void ksocket_exit(void);
int ksocket_send(unsigned char *buf, int len);
static void ksocket_start(void);
static void ksocket_listen(void) ;

#endif /* SRC_simple_H_ */
