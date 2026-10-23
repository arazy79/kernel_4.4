/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * Copyright (c) 2011~2014 Paul Reioux <reioux@gmail.com>
 * Copyright (c)      2018 Ryan Andri <ryanandri@linuxmail.org>
 * 			   [Backport to New Adreno 5xx driver]
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
 * TUNED:
 *  - ret di-init 0 (cegah nilai sampah yg bikin loncat level acak → jitter)
 *  - laziness di-reset pas busy/ramp-up (down-step pertama gak instan lagi)
 *  - default_laziness 2 -> 4 (hold lebih lama sebelum turun, kurang flutter)
 *  - ramp_up_threshold 6000 -> 5000 (naikin frek sedikit lebih responsif)
 *  - batasan level pakai bus.num+2 dihapus (salah, governor yg clamp nanti)
 *
 * CATATAN: default OFF (simple_gpu_active=0). Selama gak diaktifin via
 * /sys/module/.../parameters/simple_gpu_activate, kode ini gak jalan.
 * Semua parameter bisa diubah runtime via sysfs module_param.
 */
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/msm_adreno_devfreq.h>
#include <linux/io.h>

static int default_laziness = 4;
module_param_named(simple_laziness, default_laziness, int, 0664);

static int ramp_up_threshold = 5000;
module_param_named(simple_ramp_threshold, ramp_up_threshold, int, 0664);

int simple_gpu_active = 0;
module_param_named(simple_gpu_activate, simple_gpu_active, int, 0664);

static int laziness;

int simple_gpu_algorithm(int level, int *val,
			struct devfreq_msm_adreno_tz_data *priv)
{
	int ret = 0;

	/* sync memory before sending the commands */
	__iowmb();

	/* it's currently busy */
	if ((unsigned int)priv->bin.busy_time > ramp_up_threshold) {
		/* reset laziness biar down-step pertama setelah busy gak instan */
		laziness = default_laziness;

		if (level == 0)
			ret = 0; /* already maxed, so do nothing */
		else
			ret = -1; /* bump up to next pwrlevel */
	/* idle case */
	} else {
		if (laziness > 0) {
			/* hold off for a while */
			laziness--;
			/* don't change anything yet */
			ret = 0;
		} else {
			/* above min, lower it (governor yg clamp ke batas aman) */
			ret = 1;
			/* reset laziness count */
			laziness = default_laziness;
		}
	}

	*val = ret;
	return ret;
}
EXPORT_SYMBOL(simple_gpu_algorithm);

static int __init simple_gpu_init(void)
{
	return 0;
}
subsys_initcall(simple_gpu_init);

static void __exit simple_gpu_exit(void)
{
	return;
}
module_exit(simple_gpu_exit);

MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_DESCRIPTION("'simple_gpu_algorithm - A Simple user configurable GPU"
	"Control Algorithm for Adreno GPU series (tuned)");
MODULE_LICENSE("GPL");
