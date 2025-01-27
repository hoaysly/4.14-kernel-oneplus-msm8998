/*
 * Copyright (C) 2015 Etnaviv Project
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ETNAVIV_GPU_H__
#define __ETNAVIV_GPU_H__

#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include "etnaviv_drv.h"
#include "common.xml.h"

struct etnaviv_gem_submit;
struct etnaviv_vram_mapping;

struct etnaviv_chip_identity {
	/* Chip model. */
	u32 model;

	/* Revision value.*/
	u32 revision;

	/* Supported feature fields. */
	u32 features;

	/* Supported minor feature fields. */
	u32 minor_features0;

	/* Supported minor feature 1 fields. */
	u32 minor_features1;

	/* Supported minor feature 2 fields. */
	u32 minor_features2;

	/* Supported minor feature 3 fields. */
	u32 minor_features3;

	/* Supported minor feature 4 fields. */
	u32 minor_features4;

	/* Supported minor feature 5 fields. */
	u32 minor_features5;

	/* Number of streams supported. */
	u32 stream_count;

	/* Total number of temporary registers per thread. */
	u32 register_max;

	/* Maximum number of threads. */
	u32 thread_count;

	/* Number of shader cores. */
	u32 shader_core_count;

	/* Size of the vertex cache. */
	u32 vertex_cache_size;

	/* Number of entries in the vertex output buffer. */
	u32 vertex_output_buffer_size;

	/* Number of pixel pipes. */
	u32 pixel_pipes;

	/* Number of instructions. */
	u32 instruction_count;

	/* Number of constants. */
	u32 num_constants;

	/* Buffer size */
	u32 buffer_size;

	/* Number of varyings */
	u8 varyings_count;
};

struct etnaviv_event {
	bool used;
	struct dma_fence *fence;
};

struct etnaviv_cmdbuf_suballoc;
struct etnaviv_cmdbuf;

struct etnaviv_gpu {
	struct drm_device *drm;
	struct thermal_cooling_device *cooling;
	struct device *dev;
	struct mutex lock;
	struct etnaviv_chip_identity identity;
	struct etnaviv_file_private *lastctx;
	bool switch_context;

	/* 'ring'-buffer: */
	struct etnaviv_cmdbuf *buffer;
	int exec_state;

	/* bus base address of memory  */
	u32 memory_base;

	/* event management: */
	struct etnaviv_event event[30];
	struct completion event_free;
	spinlock_t event_spinlock;

	/* list of currently in-flight command buffers */
	struct list_head active_cmd_list;

	u32 idle_mask;

	/* Fencing support */
	u32 next_fence;
	u32 active_fence;
	u32 completed_fence;
	u32 retired_fence;
	wait_queue_head_t fence_event;
	u64 fence_context;
	spinlock_t fence_spinlock;

	/* worker for handling active-list retiring: */
	struct work_struct retire_work;

	void __iomem *mmio;
	int irq;

	struct etnaviv_iommu *mmu;
	struct etnaviv_cmdbuf_suballoc *cmdbuf_suballoc;
	unsigned int flush_seq;

	/* Power Control: */
	struct clk *clk_bus;
	struct clk *clk_core;
	struct clk *clk_shader;

	/* Hang Detction: */
#define DRM_ETNAVIV_HANGCHECK_PERIOD 500 /* in ms */
#define DRM_ETNAVIV_HANGCHECK_JIFFIES msecs_to_jiffies(DRM_ETNAVIV_HANGCHECK_PERIOD)
	struct timer_list hangcheck_timer;
	u32 hangcheck_fence;
	u32 hangcheck_dma_addr;
	struct work_struct recover_work;
	unsigned int freq_scale;
	unsigned long base_rate_core;
	unsigned long base_rate_shader;
};

static inline void gpu_write(struct etnaviv_gpu *gpu, u32 reg, u32 data)
{
	etnaviv_writel(data, gpu->mmio + reg);
}

static inline u32 gpu_read(struct etnaviv_gpu *gpu, u32 reg)
{
	return etnaviv_readl(gpu->mmio + reg);
}

static inline bool fence_retired(struct etnaviv_gpu *gpu, u32 fence)
{
	return fence_after_eq(gpu->retired_fence, fence);
}

static inline u32 gpu_fix_power_address(struct etnaviv_gpu *gpu, u32 reg)
{
	/* Power registers in GC300 < 2.0 are offset by 0x100 */
	if (gpu->identity.model == chipModel_GC300 &&
	    gpu->identity.revision < 0x2000)
		reg += 0x100;

	return reg;
}

static inline void gpu_write_power(struct etnaviv_gpu *gpu, u32 reg, u32 data)
{
	writel(data, gpu->mmio + gpu_fix_power_address(gpu, reg));
}

static inline u32 gpu_read_power(struct etnaviv_gpu *gpu, u32 reg)
{
	return readl(gpu->mmio + gpu_fix_power_address(gpu, reg));
}

int etnaviv_gpu_get_param(struct etnaviv_gpu *gpu, u32 param, u64 *value);

int etnaviv_gpu_init(struct etnaviv_gpu *gpu);

#ifdef CONFIG_DEBUG_FS
int etnaviv_gpu_debugfs(struct etnaviv_gpu *gpu, struct seq_file *m);
#endif

int etnaviv_gpu_fence_sync_obj(struct etnaviv_gem_object *etnaviv_obj,
	unsigned int context, bool exclusive, bool implicit);

void etnaviv_gpu_retire(struct etnaviv_gpu *gpu);
int etnaviv_gpu_wait_fence_interruptible(struct etnaviv_gpu *gpu,
	u32 fence, struct timespec *timeout);
int etnaviv_gpu_wait_obj_inactive(struct etnaviv_gpu *gpu,
	struct etnaviv_gem_object *etnaviv_obj, struct timespec *timeout);
int etnaviv_gpu_submit(struct etnaviv_gpu *gpu,
	struct etnaviv_gem_submit *submit, struct etnaviv_cmdbuf *cmdbuf);
int etnaviv_gpu_pm_get_sync(struct etnaviv_gpu *gpu);
void etnaviv_gpu_pm_put(struct etnaviv_gpu *gpu);
int etnaviv_gpu_wait_idle(struct etnaviv_gpu *gpu, unsigned int timeout_ms);
void etnaviv_gpu_start_fe(struct etnaviv_gpu *gpu, u32 address, u16 prefetch);

extern struct platform_driver etnaviv_gpu_driver;

#endif /* __ETNAVIV_GPU_H__ */
