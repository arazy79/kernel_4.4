/*
 * Author: Park Ju Hyung aka arter97 <qkrwngud825@gmail.com>
 *
 * Copyright 2015 Park Ju Hyung
 * Copyright 2016 Joe Maples
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Adreno idler - Idling algorithm,
 * an efficient workaround for msm-adreno-tz's overheads.
 *
 * Main goal is to lower the power consumptions while maintaining high-performance.
 *
 * Since msm-adreno-tz tends to *not* use the lowest frequency even on idle,
 * Adreno idler replaces msm-adreno-tz's algorithm when it comes to
 * calculating idle frequency(mostly by ondemand's method).
 * The higher frequencies are not touched with this algorithm, so high-demanding
 * games will (most likely) not suffer from worsened performance.
 *
 * TUNED (kernel 4.4): ramp-down dijadikan gradual (1 level per sample)
 * alih-alih loncat langsung ke frek minimum (cliff). Parameter diperlonggar
 * biar gak gampang ke-trigger pas scroll → jitter berkurang drastis.
 */
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/state_notifier.h>
#include <linux/msm_adreno_devfreq.h>

#define ADRENO_IDLER_MAJOR_VERSION 1
#define ADRENO_IDLER_MINOR_VERSION 2

/* stats.busy_time threshold for determining if the given workload is idle.
   Any workload higher than this will be treated as a non-idle workload.
   Adreno idler will more actively try to ramp down the frequency
   if this is set to a higher value.
   TUNED: 8000 -> 6000 (kurang agresif) */
static unsigned long idleworkload = 6000;
module_param_named(adreno_idler_idleworkload, idleworkload, ulong, 0664);

/* Number of events to wait before ramping down the frequency.
   The idlewait'th events before current one must be all idle before
   Adreno idler ramps down the frequency.
   This implementation is to prevent micro-lags on scrolling or playing games.
   Adreno idler will more actively try to ramp down the frequency
   if this is set to a lower value.
   TUNED: 5 -> 8 (butuh lebih banyak sample idle) */
static unsigned int idlewait = 8;
module_param_named(adreno_idler_idlewait, idlewait, uint, 0664);

/* Taken from ondemand.
   TUNED: 25 -> 20 (hanya turun kalo busy% benar-benar rendah) */
static unsigned int downdifferential = 20;
module_param_named(adreno_idler_downdifferential, downdifferential, uint, 0664);

/* Master switch to activate the whole routine.
   Bisa di-disable runtime: echo 0 > /sys/module/adreno_idler/parameters/adreno_idler_active */
static bool adreno_idler_active = true;
module_param_named(adreno_idler_active, adreno_idler_active, bool, 0664);

static unsigned int idlecount = 0;

/* Cari level frek saat ini (self-contained, gak ngandelin export governor) */
static inline int idler_get_level(struct devfreq *devfreq, unsigned long freq)
{
	int i;

	for (i = 0; i < devfreq->profile->max_state; i++)
		if (devfreq->profile->freq_table[i] == freq)
			return i;
	return 0;
}

int adreno_idler(struct devfreq_dev_status stats, struct devfreq *devfreq,
		 unsigned long *freq)
{
	int level;

	if (!adreno_idler_active)
		return 0;

	if (stats.busy_time < idleworkload) {
		/* busy_time >= idleworkload should be considered as a non-idle workload. */
		idlecount++;

		if (*freq == devfreq->profile->freq_table[devfreq->profile->max_state - 1]) {
			/* Frequency is already at its lowest.
			   No need to calculate things, so bail out. */
			return 1;
		}

		if (idlecount >= idlewait &&
		    stats.busy_time * 100 < stats.total_time * downdifferential) {
			/*
			 * We are idle for (idlewait + 1)'th time.
			 * TUNED: turun SATU level saja (gradual), JANGAN loncat
			 * langsung ke minimum. Loncat ke min bikin cliff yang
			 * nyebabin jitter pas load balik naik.
			 */
			level = idler_get_level(devfreq, *freq);
			if (level < devfreq->profile->max_state - 1)
				*freq = devfreq->profile->freq_table[level + 1];
			else
				*freq = devfreq->profile->freq_table[devfreq->profile->max_state - 1];

			idlecount--;
			return 1;
		}
	} else if (state_suspended) {
		/* GPU shouldn't be used for much while display is off, so ramp down the frequency */
		*freq = devfreq->profile->freq_table[devfreq->profile->max_state - 1];
		return 1;
	} else {
		idlecount = 0;
		/* Do not return 1 here and allow rest of the algorithm to
		   figure out the appropriate frequency for current workload.
		   It can even set it back to the lowest frequency. */
	}

	return 0;
}
EXPORT_SYMBOL(adreno_idler);

static int __init adreno_idler_init(void)
{
	pr_info("adreno_idler: version %d.%d by arter97 (tuned, gradual ramp-down)\n",
		 ADRENO_IDLER_MAJOR_VERSION,
		 ADRENO_IDLER_MINOR_VERSION);
	return 0;
}
subsys_initcall(adreno_idler_init);

static void __exit adreno_idler_exit(void)
{
	return;
}
module_exit(adreno_idler_exit);

MODULE_AUTHOR("Park Ju Hyung <qkrwngud825@gmail.com>");
MODULE_DESCRIPTION("'adreno_idler - A powersaver for Adreno TZ"
	"Control idle algorithm for Adreno GPU series (tuned)");
MODULE_LICENSE("GPL");
