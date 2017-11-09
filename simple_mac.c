/*
 * simple_mac.c
 *
 *  Created on: Nov 1, 2017
 *      Author: 1109052
 */

#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mman.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/irqflags.h>
#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel

#include "simple.h"

#define  DEVICE_NAME "simpledrv"    ///< The device will appear at /dev/simpledev using this value
#define  CLASS_NAME  "simple"        ///< The device class -- this is a character device driver
static int majorNumber; ///< Stores the device number -- determined automatically
static char message[256] = { 0 }; ///< Memory for the string that is passed from userspace
static short size_of_message; ///< Used to remember the size of the string stored
static int numberOpens = 0; ///< Counts the number of times the device is opened
static struct class* simpledevClass = NULL; ///< The device-driver class struct pointer
static struct device* simpledevDevice = NULL; ///< The device-driver device struct pointer

char simple_mac[6] = { 0xde, 0xad, 0x00, 0x00, 0xbe, 0xef };

struct simple_dev * simpledev;

/*
 * Functions
 * */

static inline struct simple_fpga_intf* vif_to_intf(struct ieee80211_vif *vif) {
	return (struct simple_fpga_intf *) vif->drv_priv;
}

static void __exit simple_mac_exit(void) {
	printk("Goodbye %s from the simpleMac LKM!\n", DEVICE_NAME);
	ieee80211_register_hw(simpledev->hw);
	ieee80211_free_hw(simpledev->hw);
	device_destroy(simpledevClass, MKDEV(majorNumber, 0));
	class_unregister(simpledevClass);
	class_destroy(simpledevClass);
}

static void simple_setup_channel(struct ieee80211_channel *entry,
		const int channel, const int tx_power, const int value) {
	/* XXX: this assumption about the band is wrong for 802.11j */
	entry->band = channel <= 14 ? NL80211_BAND_2GHZ : NL80211_BAND_5GHZ;
	entry->center_freq = ieee80211_channel_to_frequency(channel, entry->band);
	entry->hw_value = value;
	entry->max_power = tx_power;
	entry->max_antenna_gain = 0xff;
}
static void simple_setup_rate(struct ieee80211_rate *entry, const u16 index,
		const struct simple_rate *rate) {
	entry->flags = 0;
	entry->bitrate = rate->bitrate;
	entry->hw_value = index;
	entry->hw_value_short = index;

	if (rate->flags & DEV_RATE_SHORT_PREAMBLE)
		entry->flags |= IEEE80211_RATE_SHORT_PREAMBLE;
}

static int simple_probe_hw_mode(struct simple_dev *ad) {
	printk("In %s\r\n", __FUNCTION__);
	struct hw_mode_spec *spec = &ad->spec;
	struct ieee80211_hw *hw = ad->hw;
	struct ieee80211_channel *channels;
	struct ieee80211_rate *rates;
	struct channel_info *info;
	char *tx_power;
	unsigned int i;
	unsigned int num_rates;

	/*
	 * Initialize all hw fields.
	 *
	 * Don't set IEEE80211_HW_HOST_BROADCsimple_PS_BUFFERING unless we are
	 * capable of sending the buffered frames out after the DTIM
	 * transmission using rt2x00lib_beacondone. This will send out
	 * multicsimple and broadcsimple traffic immediately instead of buffering it
	 * infinitly and thus dropping it after some time.
	 */
	ieee80211_hw_set(ad->hw, PS_NULLFUNC_STACK);
	ieee80211_hw_set(ad->hw, SUPPORTS_PS);
	ieee80211_hw_set(ad->hw, RX_INCLUDES_FCS);
	ieee80211_hw_set(ad->hw, SIGNAL_DBM);
	/*
	 * Disable powersaving as default.
	 */
	ad->hw->wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;

	/*
	 * Initialize hw_mode information.
	 */
	SET_IEEE80211_DEV(ad->hw, ad->dev);
	SET_IEEE80211_PERM_ADDR(ad->hw, simple_mac);

	spec->supported_bands = SUPPORT_BAND_2GHZ;
	spec->supported_rates = SUPPORT_RATE_CCK | SUPPORT_RATE_OFDM;

	spec->num_channels = ARRAY_SIZE(rf_vals_bg_2528);
	spec->channels = rf_vals_bg_2528;

	/*
	 * Create channel information array
	 */
	info = kcalloc(spec->num_channels, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	spec->channels_info = info;

	for (i = 0; i < 14; i++) {
		info[i].max_power = MAX_TXPOWER;
		info[i].default_power1 = DEFAULT_TXPOWER;
	}

	if (spec->num_channels > 14) {
		for (i = 14; i < spec->num_channels; i++) {
			info[i].max_power = MAX_TXPOWER;
			info[i].default_power1 = DEFAULT_TXPOWER;
		}
	}

	num_rates = 0;
	if (spec->supported_rates & SUPPORT_RATE_CCK)
		num_rates += 4;
	if (spec->supported_rates & SUPPORT_RATE_OFDM)
		num_rates += 8;

	channels = kcalloc(spec->num_channels, sizeof(*channels), GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	rates = kcalloc(num_rates, sizeof(*rates), GFP_KERNEL);
	if (!rates)
		goto exit_free_channels;

	/*
	 * Initialize Rate list.
	 */
	for (i = 0; i < num_rates; i++)
		simple_setup_rate(&rates[i], i, simple_get_rate(i));

	/*
	 * Initialize Channel list.
	 */
	for (i = 0; i < spec->num_channels; i++) {
		simple_setup_channel(&channels[i], spec->channels[i].channel,
				spec->channels_info[i].max_power, i);
	}

	/*
	 * Intitialize 802.11b, 802.11g
	 * Rates: CCK, OFDM.
	 * Channels: 2.4 GHz
	 */
	if (spec->supported_bands & SUPPORT_BAND_2GHZ) {
		ad->bands[NL80211_BAND_2GHZ].n_channels = 14;
		ad->bands[NL80211_BAND_2GHZ].n_bitrates = num_rates;
		ad->bands[NL80211_BAND_2GHZ].channels = channels;
		ad->bands[NL80211_BAND_2GHZ].bitrates = rates;
		hw->wiphy->bands[NL80211_BAND_2GHZ] = &ad->bands[NL80211_BAND_2GHZ];
		memcpy(&ad->bands[NL80211_BAND_2GHZ].ht_cap, &spec->ht,
				sizeof(spec->ht));
	}

	ad->hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION);
	ad->hw->wiphy->interface_modes |= BIT(NL80211_IFTYPE_ADHOC)
			| BIT(NL80211_IFTYPE_AP) | BIT(NL80211_IFTYPE_WDS);

	ad->hw->wiphy->flags |= WIPHY_FLAG_IBSS_RSN;

	return 0;

	exit_free_channels: kfree(channels);
	printk("Allocation ieee80211 modes failed\n");
	return -ENOMEM;
}

static inline void simple_mac_set_if_combinations(struct simple_dev *ad) {
	struct ieee80211_iface_limit *if_limit;
	struct ieee80211_iface_combination *if_combination;

	/*
	 * Build up AP interface limits structure.
	 */
	if_limit = &ad->if_limits_ap;
	if_limit->max = 4;
	if_limit->types = BIT(NL80211_IFTYPE_AP);

	/*
	 * Build up AP interface combinations structure.
	 */
	if_combination = &ad->if_combinations[IF_COMB_AP];
	if_combination->limits = if_limit;
	if_combination->n_limits = 1;
	if_combination->max_interfaces = if_limit->max;
	if_combination->num_different_channels = 1;

	/*
	 * Finally, specify the possible combinations to mac80211.
	 */
	ad->hw->wiphy->iface_combinations = ad->if_combinations;
	ad->hw->wiphy->n_iface_combinations = 1;
}

void simple_mac_destroy(struct simple_dev *ad) {
	printk("In %s\r\n", __FUNCTION__);
	struct ieee80211_ops *ops = ad->ops;

	ieee80211_free_hw(ad->hw);
}

void simple_mac_tx(struct ieee80211_hw *hw,
		struct ieee80211_tx_control *control, struct sk_buff *skb) {
	printk("In %s\r\n", __FUNCTION__);
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);
}

int simple_mac_start(struct ieee80211_hw *hw) {
	printk("In %s\r\n", __FUNCTION__);
	struct simple_dev *ad = hw->priv;

	return 0;
}

void simple_mac_stop(struct ieee80211_hw *hw) {
	printk("In %s\r\n", __FUNCTION__);
}

int simple_mac_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif) {
	printk("In %s\r\n", __FUNCTION__);
	struct simple_dev *ad = hw->priv;

	return 0;
}

void simple_mac_remove_interface(struct ieee80211_hw *hw,
		struct ieee80211_vif *vif) {
	printk("In %s\r\n", __FUNCTION__);

}

int simple_mac_config(struct ieee80211_hw *hw, u32 changed) {
	printk("In %s\r\n", __FUNCTION__);
	struct simple_dev *ad = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;

	return 0;
}

void simple_mac_configure_filter(struct ieee80211_hw *hw,
		unsigned int changed_flags, unsigned int *total_flags, u64 multicast) {
	printk("In %s\r\n", __FUNCTION__);

	struct simple_dev *ad = hw->priv;
	*total_flags &= FIF_ALLMULTI | FIF_FCSFAIL | FIF_PLCPFAIL | FIF_CONTROL
			| FIF_PSPOLL | FIF_OTHER_BSS;
	*total_flags |= FIF_ALLMULTI;

	*total_flags |= FIF_PSPOLL;

}

static const struct ieee80211_ops simple_mac80211_ops = { //
		.tx = simple_mac_tx, //
				.start = simple_mac_start, //
				.stop = simple_mac_stop, //
				.add_interface = simple_mac_add_interface, //
				.remove_interface = simple_mac_remove_interface, //
				.config = simple_mac_config, //
				.configure_filter = simple_mac_configure_filter, //
		};

struct simple_dev *simple_mac_create(size_t priv_size) {
	printk("In %s\r\n", __FUNCTION__);
	struct ieee80211_hw *hw;
	struct simple_dev *ad;

	int ret;

	hw = ieee80211_alloc_hw(sizeof(struct simple_dev) + priv_size,
			&simple_mac80211_ops);
	if (!hw) {
		printk("Failed to allocate hardware\n");
		return NULL;
	}

	ad = hw->priv;
	ad->hw = hw;
	ad->ops = &simple_mac80211_ops;

	simpledevClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(simpledevClass)) {    // Check for error and clean up if there is
		printk("Failed to register device class\n");
		return PTR_ERR(simpledevClass); // Correct way to return an error on a pointer
	}
	printk("simpledev: device class registered correctly\n");

	ad->dev = device_create(simpledevClass, NULL, MKDEV(majorNumber, 0), NULL,
	DEVICE_NAME);
	if (IS_ERR(simpledevDevice)) {              // Clean up if there is an error
		class_destroy(simpledevClass); // Repeated code but the alternative is goto statements
		printk("Failed to create the device\n");
		return PTR_ERR(simpledevDevice);
	}

	simple_probe_hw_mode(ad);
	simple_mac_set_if_combinations(ad);

	printk("registering hw\n");
	ret = ieee80211_register_hw(hw);
	if (ret != 0) {
		printk("Failed to register hardware: %d\n", ret);
		return NULL;
	}

	return ad;
}

static int __init simple_mac_init(void) {
	printk("Hello %s from the simple driver module!\n", DEVICE_NAME);

	simpledev = simple_mac_create(sizeof(struct simple_dev));

	if (simpledev ==NULL) {
		printk("Failed to create hw interface, aborting\n");
	}
	return 0;
}
MODULE_LICENSE("GPL");
module_init( simple_mac_init);
module_exit( simple_mac_exit);
