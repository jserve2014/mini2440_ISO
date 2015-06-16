/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"
#include <linux/swap.h>
#include <linux/pci.h>

#define I915_GEM_GPU_DOMAINS	(~(I915_GEM_DOMAIN_CPU | I915_GEM_DOMAIN_GTT))

static void i915_gem_object_flush_gpu_write_domain(struct drm_gem_object *obj);
static void i915_gem_object_flush_gtt_write_domain(struct drm_gem_object *obj);
static void i915_gem_object_flush_cpu_write_domain(struct drm_gem_object *obj);
static int i915_gem_object_set_to_cpu_domain(struct drm_gem_object *obj,
					     int write);
static int i915_gem_object_set_cpu_read_domain_range(struct drm_gem_object *obj,
						     uint64_t offset,
						     uint64_t size);
static void i915_gem_object_set_to_full_cpu_read_domain(struct drm_gem_object *obj);
static int i915_gem_object_wait_rendering(struct drm_gem_object *obj);
static int i915_gem_object_bind_to_gtt(struct drm_gem_object *obj,
					   unsigned alignment);
static void i915_gem_clear_fence_reg(struct drm_gem_object *obj);
static int i915_gem_evict_something(struct drm_device *dev, int min_size);
static int i915_gem_evict_from_inactive_list(struct drm_device *dev);
static int i915_gem_phys_pwrite(struct drm_device *dev, struct drm_gem_object *obj,
				struct drm_i915_gem_pwrite *args,
				struct drm_file *file_priv);

static LIST_HEAD(shrink_list);
static DEFINE_SPINLOCK(shrink_list_lock);

int i915_gem_do_init(struct drm_device *dev, unsigned long start,
		     unsigned long end)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (start >= end ||
	    (start & (PAGE_SIZE - 1)) != 0 ||
	    (end & (PAGE_SIZE - 1)) != 0) {
		return -EINVAL;
	}

	drm_mm_init(&dev_priv->mm.gtt_space, start,
		    end - start);

	dev->gtt_total = (uint32_t) (end - start);

	return 0;
}

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_i915_gem_init *args = data;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = i915_gem_do_init(dev, args->gtt_start, args->gtt_end);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_i915_gem_get_aperture *args = data;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	args->aper_size = dev->gtt_total;
	args->aper_available_size = (args->aper_size -
				     atomic_read(&dev->pin_memory));

	return 0;
}


/**
 * Creates a new mm object and returns a handle to it.
 */
int
i915_gem_create_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_i915_gem_create *args = data;
	struct drm_gem_object *obj;
	int ret;
	u32 handle;

	args->size = roundup(args->size, PAGE_SIZE);

	/* Allocate the new object */
	obj = drm_gem_object_alloc(dev, args->size);
	if (obj == NULL)
		return -ENOMEM;

	ret = drm_gem_handle_create(file_priv, obj, &handle);
	mutex_lock(&dev->struct_mutex);
	drm_gem_object_handle_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	if (ret)
		return ret;

	args->handle = handle;

	return 0;
}

static inline int
fast_shmem_read(struct page **pages,
		loff_t page_base, int page_offset,
		char __user *data,
		int length)
{
	char __iomem *vaddr;
	int unwritten;

	vaddr = kmap_atomic(pages[page_base >> PAGE_SHIFT], KM_USER0);
	if (vaddr == NULL)
		return -ENOMEM;
	unwritten = __copy_to_user_inatomic(data, vaddr + page_offset, length);
	kunmap_atomic(vaddr, KM_USER0);

	if (unwritten)
		return -EFAULT;

	return 0;
}

static int i915_gem_object_needs_bit17_swizzle(struct drm_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		obj_priv->tiling_mode != I915_TILING_NONE;
}

static inline int
slow_shmem_copy(struct page *dst_page,
		int dst_offset,
		struct page *src_page,
		int src_offset,
		int length)
{
	char *dst_vaddr, *src_vaddr;

	dst_vaddr = kmap_atomic(dst_page, KM_USER0);
	if (dst_vaddr == NULL)
		return -ENOMEM;

	src_vaddr = kmap_atomic(src_page, KM_USER1);
	if (src_vaddr == NULL) {
		kunmap_atomic(dst_vaddr, KM_USER0);
		return -ENOMEM;
	}

	memcpy(dst_vaddr + dst_offset, src_vaddr + src_offset, length);

	kunmap_atomic(src_vaddr, KM_USER1);
	kunmap_atomic(dst_vaddr, KM_USER0);

	return 0;
}

static inline int
slow_shmem_bit17_copy(struct page *gpu_page,
		      int gpu_offset,
		      struct page *cpu_page,
		      int cpu_offset,
		      int length,
		      int is_read)
{
	char *gpu_vaddr, *cpu_vaddr;

	/* Use the unswizzled path if this page isn't affected. */
	if ((page_to_phys(gpu_page) & (1 << 17)) == 0) {
		if (is_read)
			return slow_shmem_copy(cpu_page, cpu_offset,
					       gpu_page, gpu_offset, length);
		else
			return slow_shmem_copy(gpu_page, gpu_offset,
					       cpu_page, cpu_offset, length);
	}

	gpu_vaddr = kmap_atomic(gpu_page, KM_USER0);
	if (gpu_vaddr == NULL)
		return -ENOMEM;

	cpu_vaddr = kmap_atomic(cpu_page, KM_USER1);
	if (cpu_vaddr == NULL) {
		kunmap_atomic(gpu_vaddr, KM_USER0);
		return -ENOMEM;
	}

	/* Copy the data, XORing A6 with A17 (1). The user already knows he's
	 * XORing with the other bits (A9 for Y, A9 and A10 for X)
	 */
	while (length > 0) {
		int cacheline_end = ALIGN(gpu_offset + 1, 64);
		int this_length = min(cacheline_end - gpu_offset, length);
		int swizzled_gpu_offset = gpu_offset ^ 64;

		if (is_read) {
			memcpy(cpu_vaddr + cpu_offset,
			       gpu_vaddr + swizzled_gpu_offset,
			       this_length);
		} else {
			memcpy(gpu_vaddr + swizzled_gpu_offset,
			       cpu_vaddr + cpu_offset,
			       this_length);
		}
		cpu_offset += this_length;
		gpu_offset += this_length;
		length -= this_length;
	}

	kunmap_atomic(cpu_vaddr, KM_USER1);
	kunmap_atomic(gpu_vaddr, KM_USER0);

	return 0;
}

/**
 * This is the fast shmem pread path, which attempts to copy_from_user directly
 * from the backing pages of the object to the user's address space.  On a
 * fault, it fails so we can fall back to i915_gem_shmem_pwrite_slow().
 */
static int
i915_gem_shmem_pread_fast(struct drm_device *dev, struct drm_gem_object *obj,
			  struct drm_i915_gem_pread *args,
			  struct drm_file *file_priv)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	ssize_t remain;
	loff_t offset, page_base;
	char __user *user_data;
	int page_offset, page_length;
	int ret;

	user_data = (char __user *) (uintptr_t) args->data_ptr;
	remain = args->size;

	mutex_lock(&dev->struct_mutex);

	ret = i915_gem_object_get_pages(obj);
	if (ret != 0)
		goto fail_unlock;

	ret = i915_gem_object_set_cpu_read_domain_range(obj, args->offset,
							args->size);
	if (ret != 0)
		goto fail_put_pages;

	obj_priv = obj->driver_private;
	offset = args->offset;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * page_base = page offset within aperture
		 * page_offset = offset within page
		 * page_length = bytes to copy for this page
		 */
		page_base = (offset & ~(PAGE_SIZE-1));
		page_offset = offset & (PAGE_SIZE-1);
		page_length = remain;
		if ((page_offset + remain) > PAGE_SIZE)
			page_length = PAGE_SIZE - page_offset;

		ret = fast_shmem_read(obj_priv->pages,
				      page_base, page_offset,
				      user_data, page_length);
		if (ret)
			goto fail_put_pages;

		remain -= page_length;
		user_data += page_length;
		offset += page_length;
	}

fail_put_pages:
	i915_gem_object_put_pages(obj);
fail_unlock:
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

static inline gfp_t
i915_gem_object_get_page_gfp_mask (struct drm_gem_object *obj)
{
	return mapping_gfp_mask(obj->filp->f_path.dentry->d_inode->i_mapping);
}

static inline void
i915_gem_object_set_page_gfp_mask (struct drm_gem_object *obj, gfp_t gfp)
{
	mapping_set_gfp_mask(obj->filp->f_path.dentry->d_inode->i_mapping, gfp);
}

static int
i915_gem_object_get_pages_or_evict(struct drm_gem_object *obj)
{
	int ret;

	ret = i915_gem_object_get_pages(obj);

	/* If we've insufficient memory to map in the pages, attempt
	 * to make some space by throwing out some old buffers.
	 */
	if (ret == -ENOMEM) {
		struct drm_device *dev = obj->dev;
		gfp_t gfp;

		ret = i915_gem_evict_something(dev, obj->size);
		if (ret)
			return ret;

		gfp = i915_gem_object_get_page_gfp_mask(obj);
		i915_gem_object_set_page_gfp_mask(obj, gfp & ~__GFP_NORETRY);
		ret = i915_gem_object_get_pages(obj);
		i915_gem_object_set_page_gfp_mask (obj, gfp);
	}

	return ret;
}

/**
 * This is the fallback shmem pread path, which allocates temporary storage
 * in kernel space to copy_to_user into outside of the struct_mutex, so we
 * can copy out of the object's backing pages while holding the struct mutex
 * and not take page faults.
 */
static int
i915_gem_shmem_pread_slow(struct drm_device *dev, struct drm_gem_object *obj,
			  struct drm_i915_gem_pread *args,
			  struct drm_file *file_priv)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct mm_struct *mm = current->mm;
	struct page **user_pages;
	ssize_t remain;
	loff_t offset, pinned_pages, i;
	loff_t first_data_page, last_data_page, num_pages;
	int shmem_page_index, shmem_page_offset;
	int data_page_index,  data_page_offset;
	int page_length;
	int ret;
	uint64_t data_ptr = args->data_ptr;
	int do_bit17_swizzling;

	remain = args->size;

	/* Pin the user pages containing the data.  We can't fault while
	 * holding the struct mutex, yet we want to hold it while
	 * dereferencing the user data.
	 */
	first_data_page = data_ptr / PAGE_SIZE;
	last_data_page = (data_ptr + args->size - 1) / PAGE_SIZE;
	num_pages = last_data_page - first_data_page + 1;

	user_pages = drm_calloc_large(num_pages, sizeof(struct page *));
	if (user_pages == NULL)
		return -ENOMEM;

	down_read(&mm->mmap_sem);
	pinned_pages = get_user_pages(current, mm, (uintptr_t)args->data_ptr,
				      num_pages, 1, 0, user_pages, NULL);
	up_read(&mm->mmap_sem);
	if (pinned_pages < num_pages) {
		ret = -EFAULT;
		goto fail_put_user_pages;
	}

	do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	mutex_lock(&dev->struct_mutex);

	ret = i915_gem_object_get_pages_or_evict(obj);
	if (ret)
		goto fail_unlock;

	ret = i915_gem_object_set_cpu_read_domain_range(obj, args->offset,
							args->size);
	if (ret != 0)
		goto fail_put_pages;

	obj_priv = obj->driver_private;
	offset = args->offset;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * shmem_page_index = page number within shmem file
		 * shmem_page_offset = offset within page in shmem file
		 * data_page_index = page number in get_user_pages return
		 * data_page_offset = offset with data_page_index page.
		 * page_length = bytes to copy for this page
		 */
		shmem_page_index = offset / PAGE_SIZE;
		shmem_page_offset = offset & ~PAGE_MASK;
		data_page_index = data_ptr / PAGE_SIZE - first_data_page;
		data_page_offset = data_ptr & ~PAGE_MASK;

		page_length = remain;
		if ((shmem_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - shmem_page_offset;
		if ((data_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - data_page_offset;

		if (do_bit17_swizzling) {
			ret = slow_shmem_bit17_copy(obj_priv->pages[shmem_page_index],
						    shmem_page_offset,
						    user_pages[data_page_index],
						    data_page_offset,
						    page_length,
						    1);
		} else {
			ret = slow_shmem_copy(user_pages[data_page_index],
					      data_page_offset,
					      obj_priv->pages[shmem_page_index],
					      shmem_page_offset,
					      page_length);
		}
		if (ret)
			goto fail_put_pages;

		remain -= page_length;
		data_ptr += page_length;
		offset += page_length;
	}

fail_put_pages:
	i915_gem_object_put_pages(obj);
fail_unlock:
	mutex_unlock(&dev->struct_mutex);
fail_put_user_pages:
	for (i = 0; i < pinned_pages; i++) {
		SetPageDirty(user_pages[i]);
		page_cache_release(user_pages[i]);
	}
	drm_free_large(user_pages);

	return ret;
}

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
int
i915_gem_pread_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_i915_gem_pread *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EBADF;
	obj_priv = obj->driver_private;

	/* Bounds check source.
	 *
	 * XXX: This could use review for overflow issues...
	 */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->size) {
		drm_gem_object_unreference(obj);
		return -EINVAL;
	}

	if (i915_gem_object_needs_bit17_swizzle(obj)) {
		ret = i915_gem_shmem_pread_slow(dev, obj, args, file_priv);
	} else {
		ret = i915_gem_shmem_pread_fast(dev, obj, args, file_priv);
		if (ret != 0)
			ret = i915_gem_shmem_pread_slow(dev, obj, args,
							file_priv);
	}

	drm_gem_object_unreference(obj);

	return ret;
}

/* This is the fast write path which cannot handle
 * page faults in the source data
 */

static inline int
fast_user_write(struct io_mapping *mapping,
		loff_t page_base, int page_offset,
		char __user *user_data,
		int length)
{
	char *vaddr_atomic;
	unsigned long unwritten;

	vaddr_atomic = io_mapping_map_atomic_wc(mapping, page_base);
	unwritten = __copy_from_user_inatomic_nocache(vaddr_atomic + page_offset,
						      user_data, length);
	io_mapping_unmap_atomic(vaddr_atomic);
	if (unwritten)
		return -EFAULT;
	return 0;
}

/* Here's the write path which can sleep for
 * page faults
 */

static inline int
slow_kernel_write(struct io_mapping *mapping,
		  loff_t gtt_base, int gtt_offset,
		  struct page *user_page, int user_offset,
		  int length)
{
	char *src_vaddr, *dst_vaddr;
	unsigned long unwritten;

	dst_vaddr = io_mapping_map_atomic_wc(mapping, gtt_base);
	src_vaddr = kmap_atomic(user_page, KM_USER1);
	unwritten = __copy_from_user_inatomic_nocache(dst_vaddr + gtt_offset,
						      src_vaddr + user_offset,
						      length);
	kunmap_atomic(src_vaddr, KM_USER1);
	io_mapping_unmap_atomic(dst_vaddr);
	if (unwritten)
		return -EFAULT;
	return 0;
}

static inline int
fast_shmem_write(struct page **pages,
		 loff_t page_base, int page_offset,
		 char __user *data,
		 int length)
{
	char __iomem *vaddr;
	unsigned long unwritten;

	vaddr = kmap_atomic(pages[page_base >> PAGE_SHIFT], KM_USER0);
	if (vaddr == NULL)
		return -ENOMEM;
	unwritten = __copy_from_user_inatomic(vaddr + page_offset, data, length);
	kunmap_atomic(vaddr, KM_USER0);

	if (unwritten)
		return -EFAULT;
	return 0;
}

/**
 * This is the fast pwrite path, where we copy the data directly from the
 * user into the GTT, uncached.
 */
static int
i915_gem_gtt_pwrite_fast(struct drm_device *dev, struct drm_gem_object *obj,
			 struct drm_i915_gem_pwrite *args,
			 struct drm_file *file_priv)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	drm_i915_private_t *dev_priv = dev->dev_private;
	ssize_t remain;
	loff_t offset, page_base;
	char __user *user_data;
	int page_offset, page_length;
	int ret;

	user_data = (char __user *) (uintptr_t) args->data_ptr;
	remain = args->size;
	if (!access_ok(VERIFY_READ, user_data, remain))
		return -EFAULT;


	mutex_lock(&dev->struct_mutex);
	ret = i915_gem_object_pin(obj, 0);
	if (ret) {
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}
	ret = i915_gem_object_set_to_gtt_domain(obj, 1);
	if (ret)
		goto fail;

	obj_priv = obj->driver_private;
	offset = obj_priv->gtt_offset + args->offset;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * page_base = page offset within aperture
		 * page_offset = offset within page
		 * page_length = bytes to copy for this page
		 */
		page_base = (offset & ~(PAGE_SIZE-1));
		page_offset = offset & (PAGE_SIZE-1);
		page_length = remain;
		if ((page_offset + remain) > PAGE_SIZE)
			page_length = PAGE_SIZE - page_offset;

		ret = fast_user_write (dev_priv->mm.gtt_mapping, page_base,
				       page_offset, user_data, page_length);

		/* If we get a fault while copying data, then (presumably) our
		 * source page isn't available.  Return the error and we'll
		 * retry in the slow path.
		 */
		if (ret)
			goto fail;

		remain -= page_length;
		user_data += page_length;
		offset += page_length;
	}

fail:
	i915_gem_object_unpin(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/**
 * This is the fallback GTT pwrite path, which uses get_user_pages to pin
 * the memory and maps it using kmap_atomic for copying.
 *
 * This code resulted in x11perf -rgb10text consuming about 10% more CPU
 * than using i915_gem_gtt_pwrite_fast on a G45 (32-bit).
 */
static int
i915_gem_gtt_pwrite_slow(struct drm_device *dev, struct drm_gem_object *obj,
			 struct drm_i915_gem_pwrite *args,
			 struct drm_file *file_priv)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	drm_i915_private_t *dev_priv = dev->dev_private;
	ssize_t remain;
	loff_t gtt_page_base, offset;
	loff_t first_data_page, last_data_page, num_pages;
	loff_t pinned_pages, i;
	struct page **user_pages;
	struct mm_struct *mm = current->mm;
	int gtt_page_offset, data_page_offset, data_page_index, page_length;
	int ret;
	uint64_t data_ptr = args->data_ptr;

	remain = args->size;

	/* Pin the user pages containing the data.  We can't fault while
	 * holding the struct mutex, and all of the pwrite implementations
	 * want to hold it while dereferencing the user data.
	 */
	first_data_page = data_ptr / PAGE_SIZE;
	last_data_page = (data_ptr + args->size - 1) / PAGE_SIZE;
	num_pages = last_data_page - first_data_page + 1;

	user_pages = drm_calloc_large(num_pages, sizeof(struct page *));
	if (user_pages == NULL)
		return -ENOMEM;

	down_read(&mm->mmap_sem);
	pinned_pages = get_user_pages(current, mm, (uintptr_t)args->data_ptr,
				      num_pages, 0, 0, user_pages, NULL);
	up_read(&mm->mmap_sem);
	if (pinned_pages < num_pages) {
		ret = -EFAULT;
		goto out_unpin_pages;
	}

	mutex_lock(&dev->struct_mutex);
	ret = i915_gem_object_pin(obj, 0);
	if (ret)
		goto out_unlock;

	ret = i915_gem_object_set_to_gtt_domain(obj, 1);
	if (ret)
		goto out_unpin_object;

	obj_priv = obj->driver_private;
	offset = obj_priv->gtt_offset + args->offset;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * gtt_page_base = page offset within aperture
		 * gtt_page_offset = offset within page in aperture
		 * data_page_index = page number in get_user_pages return
		 * data_page_offset = offset with data_page_index page.
		 * page_length = bytes to copy for this page
		 */
		gtt_page_base = offset & PAGE_MASK;
		gtt_page_offset = offset & ~PAGE_MASK;
		data_page_index = data_ptr / PAGE_SIZE - first_data_page;
		data_page_offset = data_ptr & ~PAGE_MASK;

		page_length = remain;
		if ((gtt_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - gtt_page_offset;
		if ((data_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - data_page_offset;

		ret = slow_kernel_write(dev_priv->mm.gtt_mapping,
					gtt_page_base, gtt_page_offset,
					user_pages[data_page_index],
					data_page_offset,
					page_length);

		/* If we get a fault while copying data, then (presumably) our
		 * source page isn't available.  Return the error and we'll
		 * retry in the slow path.
		 */
		if (ret)
			goto out_unpin_object;

		remain -= page_length;
		offset += page_length;
		data_ptr += page_length;
	}

out_unpin_object:
	i915_gem_object_unpin(obj);
out_unlock:
	mutex_unlock(&dev->struct_mutex);
out_unpin_pages:
	for (i = 0; i < pinned_pages; i++)
		page_cache_release(user_pages[i]);
	drm_free_large(user_pages);

	return ret;
}

/**
 * This is the fast shmem pwrite path, which attempts to directly
 * copy_from_user into the kmapped pages backing the object.
 */
static int
i915_gem_shmem_pwrite_fast(struct drm_device *dev, struct drm_gem_object *obj,
			   struct drm_i915_gem_pwrite *args,
			   struct drm_file *file_priv)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	ssize_t remain;
	loff_t offset, page_base;
	char __user *user_data;
	int page_offset, page_length;
	int ret;

	user_data = (char __user *) (uintptr_t) args->data_ptr;
	remain = args->size;

	mutex_lock(&dev->struct_mutex);

	ret = i915_gem_object_get_pages(obj);
	if (ret != 0)
		goto fail_unlock;

	ret = i915_gem_object_set_to_cpu_domain(obj, 1);
	if (ret != 0)
		goto fail_put_pages;

	obj_priv = obj->driver_private;
	offset = args->offset;
	obj_priv->dirty = 1;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * page_base = page offset within aperture
		 * page_offset = offset within page
		 * page_length = bytes to copy for this page
		 */
		page_base = (offset & ~(PAGE_SIZE-1));
		page_offset = offset & (PAGE_SIZE-1);
		page_length = remain;
		if ((page_offset + remain) > PAGE_SIZE)
			page_length = PAGE_SIZE - page_offset;

		ret = fast_shmem_write(obj_priv->pages,
				       page_base, page_offset,
				       user_data, page_length);
		if (ret)
			goto fail_put_pages;

		remain -= page_length;
		user_data += page_length;
		offset += page_length;
	}

fail_put_pages:
	i915_gem_object_put_pages(obj);
fail_unlock:
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/**
 * This is the fallback shmem pwrite path, which uses get_user_pages to pin
 * the memory and maps it using kmap_atomic for copying.
 *
 * This avoids taking mmap_sem for faulting on the user's address while the
 * struct_mutex is held.
 */
static int
i915_gem_shmem_pwrite_slow(struct drm_device *dev, struct drm_gem_object *obj,
			   struct drm_i915_gem_pwrite *args,
			   struct drm_file *file_priv)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct mm_struct *mm = current->mm;
	struct page **user_pages;
	ssize_t remain;
	loff_t offset, pinned_pages, i;
	loff_t first_data_page, last_data_page, num_pages;
	int shmem_page_index, shmem_page_offset;
	int data_page_index,  data_page_offset;
	int page_length;
	int ret;
	uint64_t data_ptr = args->data_ptr;
	int do_bit17_swizzling;

	remain = args->size;

	/* Pin the user pages containing the data.  We can't fault while
	 * holding the struct mutex, and all of the pwrite implementations
	 * want to hold it while dereferencing the user data.
	 */
	first_data_page = data_ptr / PAGE_SIZE;
	last_data_page = (data_ptr + args->size - 1) / PAGE_SIZE;
	num_pages = last_data_page - first_data_page + 1;

	user_pages = drm_calloc_large(num_pages, sizeof(struct page *));
	if (user_pages == NULL)
		return -ENOMEM;

	down_read(&mm->mmap_sem);
	pinned_pages = get_user_pages(current, mm, (uintptr_t)args->data_ptr,
				      num_pages, 0, 0, user_pages, NULL);
	up_read(&mm->mmap_sem);
	if (pinned_pages < num_pages) {
		ret = -EFAULT;
		goto fail_put_user_pages;
	}

	do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	mutex_lock(&dev->struct_mutex);

	ret = i915_gem_object_get_pages_or_evict(obj);
	if (ret)
		goto fail_unlock;

	ret = i915_gem_object_set_to_cpu_domain(obj, 1);
	if (ret != 0)
		goto fail_put_pages;

	obj_priv = obj->driver_private;
	offset = args->offset;
	obj_priv->dirty = 1;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * shmem_page_index = page number within shmem file
		 * shmem_page_offset = offset within page in shmem file
		 * data_page_index = page number in get_user_pages return
		 * data_page_offset = offset with data_page_index page.
		 * page_length = bytes to copy for this page
		 */
		shmem_page_index = offset / PAGE_SIZE;
		shmem_page_offset = offset & ~PAGE_MASK;
		data_page_index = data_ptr / PAGE_SIZE - first_data_page;
		data_page_offset = data_ptr & ~PAGE_MASK;

		page_length = remain;
		if ((shmem_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - shmem_page_offset;
		if ((data_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - data_page_offset;

		if (do_bit17_swizzling) {
			ret = slow_shmem_bit17_copy(obj_priv->pages[shmem_page_index],
						    shmem_page_offset,
						    user_pages[data_page_index],
						    data_page_offset,
						    page_length,
						    0);
		} else {
			ret = slow_shmem_copy(obj_priv->pages[shmem_page_index],
					      shmem_page_offset,
					      user_pages[data_page_index],
					      data_page_offset,
					      page_length);
		}
		if (ret)
			goto fail_put_pages;

		remain -= page_length;
		data_ptr += page_length;
		offset += page_length;
	}

fail_put_pages:
	i915_gem_object_put_pages(obj);
fail_unlock:
	mutex_unlock(&dev->struct_mutex);
fail_put_user_pages:
	for (i = 0; i < pinned_pages; i++)
		page_cache_release(user_pages[i]);
	drm_free_large(user_pages);

	return ret;
}

/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
int
i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_i915_gem_pwrite *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EBADF;
	obj_priv = obj->driver_private;

	/* Bounds check destination.
	 *
	 * XXX: This could use review for overflow issues...
	 */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->size) {
		drm_gem_object_unreference(obj);
		return -EINVAL;
	}

	/* We can only do the GTT pwrite on untiled buffers, as otherwise
	 * it would end up going through the fenced access, and we'll get
	 * different detiling behavior between reading and writing.
	 * pread/pwrite currently are reading and writing from the CPU
	 * perspective, requiring manual detiling by the client.
	 */
	if (obj_priv->phys_obj)
		ret = i915_gem_phys_pwrite(dev, obj, args, file_priv);
	else if (obj_priv->tiling_mode == I915_TILING_NONE &&
		 dev->gtt_total != 0) {
		ret = i915_gem_gtt_pwrite_fast(dev, obj, args, file_priv);
		if (ret == -EFAULT) {
			ret = i915_gem_gtt_pwrite_slow(dev, obj, args,
						       file_priv);
		}
	} else if (i915_gem_object_needs_bit17_swizzle(obj)) {
		ret = i915_gem_shmem_pwrite_slow(dev, obj, args, file_priv);
	} else {
		ret = i915_gem_shmem_pwrite_fast(dev, obj, args, file_priv);
		if (ret == -EFAULT) {
			ret = i915_gem_shmem_pwrite_slow(dev, obj, args,
							 file_priv);
		}
	}

#if WATCH_PWRITE
	if (ret)
		DRM_INFO("pwrite failed %d\n", ret);
#endif

	drm_gem_object_unreference(obj);

	return ret;
}

/**
 * Called when user space prepares to use an object with the CPU, either
 * through the mmap ioctl's mapping or a GTT mapping.
 */
int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_set_domain *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	uint32_t read_domains = args->read_domains;
	uint32_t write_domain = args->write_domain;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	/* Only handle setting domains to types used by the CPU. */
	if (write_domain & I915_GEM_GPU_DOMAINS)
		return -EINVAL;

	if (read_domains & I915_GEM_GPU_DOMAINS)
		return -EINVAL;

	/* Having something in the write domain implies it's in the read
	 * domain, and only that read domain.  Enforce that in the request.
	 */
	if (write_domain != 0 && read_domains != write_domain)
		return -EINVAL;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EBADF;
	obj_priv = obj->driver_private;

	mutex_lock(&dev->struct_mutex);

	intel_mark_busy(dev, obj);

#if WATCH_BUF
	DRM_INFO("set_domain_ioctl %p(%zd), %08x %08x\n",
		 obj, obj->size, read_domains, write_domain);
#endif
	if (read_domains & I915_GEM_DOMAIN_GTT) {
		ret = i915_gem_object_set_to_gtt_domain(obj, write_domain != 0);

		/* Update the LRU on the fence for the CPU access that's
		 * about to occur.
		 */
		if (obj_priv->fence_reg != I915_FENCE_REG_NONE) {
			list_move_tail(&obj_priv->fence_list,
				       &dev_priv->mm.fence_list);
		}

		/* Silently promote "you're not bound, there was nothing to do"
		 * to success, since the client was just asking us to
		 * make sure everything was done.
		 */
		if (ret == -EINVAL)
			ret = 0;
	} else {
		ret = i915_gem_object_set_to_cpu_domain(obj, write_domain != 0);
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

/**
 * Called when user space has done writes to this buffer
 */
int
i915_gem_sw_finish_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_i915_gem_sw_finish *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret = 0;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	mutex_lock(&dev->struct_mutex);
	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL) {
		mutex_unlock(&dev->struct_mutex);
		return -EBADF;
	}

#if WATCH_BUF
	DRM_INFO("%s: sw_finish %d (%p %zd)\n",
		 __func__, args->handle, obj, obj->size);
#endif
	obj_priv = obj->driver_private;

	/* Pinned buffers may be scanout, so flush the cache */
	if (obj_priv->pin_count)
		i915_gem_object_flush_cpu_write_domain(obj);

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

/**
 * Maps the contents of an object, returning the address it is mapped
 * into.
 *
 * While the mapping holds a reference on the contents of the object, it doesn't
 * imply a ref on the object itself.
 */
int
i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	struct drm_i915_gem_mmap *args = data;
	struct drm_gem_object *obj;
	loff_t offset;
	unsigned long addr;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EBADF;

	offset = args->offset;

	down_write(&current->mm->mmap_sem);
	addr = do_mmap(obj->filp, 0, args->size,
		       PROT_READ | PROT_WRITE, MAP_SHARED,
		       args->offset);
	up_write(&current->mm->mmap_sem);
	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	if (IS_ERR((void *)addr))
		return addr;

	args->addr_ptr = (uint64_t) addr;

	return 0;
}

/**
 * i915_gem_fault - fault a page into the GTT
 * vma: VMA in question
 * vmf: fault info
 *
 * The fault handler is set up by drm_gem_mmap() when a object is GTT mapped
 * from userspace.  The fault handler takes care of binding the object to
 * the GTT (if needed), allocating and programming a fence register (again,
 * only if needed based on whether the old reg is still valid or the object
 * is tiled) and inserting a new PTE into the faulting process.
 *
 * Note that the faulting process may involve evicting existing objects
 * from the GTT and/or fence registers to make room.  So performance may
 * suffer if the GTT working set is large or there are few fence registers
 * left.
 */
int i915_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	pgoff_t page_offset;
	unsigned long pfn;
	int ret = 0;
	bool write = !!(vmf->flags & FAULT_FLAG_WRITE);

	/* We don't use vmf->pgoff since that has the fake offset */
	page_offset = ((unsigned long)vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;

	/* Now bind it into the GTT if needed */
	mutex_lock(&dev->struct_mutex);
	if (!obj_priv->gtt_space) {
		ret = i915_gem_object_bind_to_gtt(obj, 0);
		if (ret)
			goto unlock;

		list_add_tail(&obj_priv->list, &dev_priv->mm.inactive_list);

		ret = i915_gem_object_set_to_gtt_domain(obj, write);
		if (ret)
			goto unlock;
	}

	/* Need a new fence register? */
	if (obj_priv->tiling_mode != I915_TILING_NONE) {
		ret = i915_gem_object_get_fence_reg(obj);
		if (ret)
			goto unlock;
	}

	pfn = ((dev->agp->base + obj_priv->gtt_offset) >> PAGE_SHIFT) +
		page_offset;

	/* Finally, remap it using the new GTT offset */
	ret = vm_insert_pfn(vma, (unsigned long)vmf->virtual_address, pfn);
unlock:
	mutex_unlock(&dev->struct_mutex);

	switch (ret) {
	case 0:
	case -ERESTARTSYS:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
	case -EAGAIN:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

/**
 * i915_gem_create_mmap_offset - create a fake mmap offset for an object
 * @obj: obj in question
 *
 * GEM memory mapping works by handing back to userspace a fake mmap offset
 * it can use in a subsequent mmap(2) call.  The DRM core code then looks
 * up the object based on the offset and sets up the various memory mapping
 * structures.
 *
 * This routine allocates and attaches a fake offset for @obj.
 */
static int
i915_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_gem_mm *mm = dev->mm_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct drm_map_list *list;
	struct drm_local_map *map;
	int ret = 0;

	/* Set the object up for mmap'ing */
	list = &obj->map_list;
	list->map = kzalloc(sizeof(struct drm_map_list), GFP_KERNEL);
	if (!list->map)
		return -ENOMEM;

	map = list->map;
	map->type = _DRM_GEM;
	map->size = obj->size;
	map->handle = obj;

	/* Get a DRM GEM mmap offset allocated... */
	list->file_offset_node = drm_mm_search_free(&mm->offset_manager,
						    obj->size / PAGE_SIZE, 0, 0);
	if (!list->file_offset_node) {
		DRM_ERROR("failed to allocate offset for bo %d\n", obj->name);
		ret = -ENOMEM;
		goto out_free_list;
	}

	list->file_offset_node = drm_mm_get_block(list->file_offset_node,
						  obj->size / PAGE_SIZE, 0);
	if (!list->file_offset_node) {
		ret = -ENOMEM;
		goto out_free_list;
	}

	list->hash.key = list->file_offset_node->start;
	if (drm_ht_insert_item(&mm->offset_hash, &list->hash)) {
		DRM_ERROR("failed to add to map hash\n");
		ret = -ENOMEM;
		goto out_free_mm;
	}

	/* By now we should be all set, any drm_mmap request on the offset
	 * below will get to our mmap & fault handler */
	obj_priv->mmap_offset = ((uint64_t) list->hash.key) << PAGE_SHIFT;

	return 0;

out_free_mm:
	drm_mm_put_block(list->file_offset_node);
out_free_list:
	kfree(list->map);

	return ret;
}

/**
 * i915_gem_release_mmap - remove physical page mappings
 * @obj: obj in question
 *
 * Preserve the reservation of the mmaping with the DRM core code, but
 * relinquish ownership of the pages back to the system.
 *
 * It is vital that we remove the page mapping if we have mapped a tiled
 * object through the GTT and then lose the fence register due to
 * resource pressure. Similarly if the object has been moved out of the
 * aperture, than pages mapped into userspace must be revoked. Removing the
 * mapping will then trigger a page fault on the next user access, allowing
 * fixup by i915_gem_fault().
 */
void
i915_gem_release_mmap(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (dev->dev_mapping)
		unmap_mapping_range(dev->dev_mapping,
				    obj_priv->mmap_offset, obj->size, 1);
}

static void
i915_gem_free_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct drm_gem_mm *mm = dev->mm_private;
	struct drm_map_list *list;

	list = &obj->map_list;
	drm_ht_remove_item(&mm->offset_hash, &list->hash);

	if (list->file_offset_node) {
		drm_mm_put_block(list->file_offset_node);
		list->file_offset_node = NULL;
	}

	if (list->map) {
		kfree(list->map);
		list->map = NULL;
	}

	obj_priv->mmap_offset = 0;
}

/**
 * i915_gem_get_gtt_alignment - return required GTT alignment for an object
 * @obj: object to check
 *
 * Return the required GTT alignment for an object, taking into account
 * potential fence register mapping if needed.
 */
static uint32_t
i915_gem_get_gtt_alignment(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int start, i;

	/*
	 * Minimum alignment is 4k (GTT page size), but might be greater
	 * if a fence register is needed for the object.
	 */
	if (IS_I965G(dev) || obj_priv->tiling_mode == I915_TILING_NONE)
		return 4096;

	/*
	 * Previous chips need to be aligned to the size of the smallest
	 * fence register that can contain the object.
	 */
	if (IS_I9XX(dev))
		start = 1024*1024;
	else
		start = 512*1024;

	for (i = start; i < obj->size; i <<= 1)
		;

	return i;
}

/**
 * i915_gem_mmap_gtt_ioctl - prepare an object for GTT mmap'ing
 * @dev: DRM device
 * @data: GTT mapping ioctl data
 * @file_priv: GEM object info
 *
 * Simply returns the fake offset to userspace so it can mmap it.
 * The mmap call will end up in drm_gem_mmap(), which will set things
 * up so we can get faults in the handler above.
 *
 * The fault handler will take care of binding the object into the GTT
 * (since it may have been evicted to make room for something), allocating
 * a fence register, and mapping the appropriate aperture address into
 * userspace.
 */
int
i915_gem_mmap_gtt_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_i915_gem_mmap_gtt *args = data;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EBADF;

	mutex_lock(&dev->struct_mutex);

	obj_priv = obj->driver_private;

	if (obj_priv->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to mmap a purgeable buffer\n");
		drm_gem_object_unreference(obj);
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}


	if (!obj_priv->mmap_offset) {
		ret = i915_gem_create_mmap_offset(obj);
		if (ret) {
			drm_gem_object_unreference(obj);
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
	}

	args->offset = obj_priv->mmap_offset;

	/*
	 * Pull it into the GTT so that we have a page list (makes the
	 * initial fault faster and any subsequent flushing possible).
	 */
	if (!obj_priv->agp_mem) {
		ret = i915_gem_object_bind_to_gtt(obj, 0);
		if (ret) {
			drm_gem_object_unreference(obj);
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
		list_add_tail(&obj_priv->list, &dev_priv->mm.inactive_list);
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

void
i915_gem_object_put_pages(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int page_count = obj->size / PAGE_SIZE;
	int i;

	BUG_ON(obj_priv->pages_refcount == 0);
	BUG_ON(obj_priv->madv == __I915_MADV_PURGED);

	if (--obj_priv->pages_refcount != 0)
		return;

	if (obj_priv->tiling_mode != I915_TILING_NONE)
		i915_gem_object_save_bit_17_swizzle(obj);

	if (obj_priv->madv == I915_MADV_DONTNEED)
		obj_priv->dirty = 0;

	for (i = 0; i < page_count; i++) {
		if (obj_priv->pages[i] == NULL)
			break;

		if (obj_priv->dirty)
			set_page_dirty(obj_priv->pages[i]);

		if (obj_priv->madv == I915_MADV_WILLNEED)
			mark_page_accessed(obj_priv->pages[i]);

		page_cache_release(obj_priv->pages[i]);
	}
	obj_priv->dirty = 0;

	drm_free_large(obj_priv->pages);
	obj_priv->pages = NULL;
}

static void
i915_gem_object_move_to_active(struct drm_gem_object *obj, uint32_t seqno)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	/* Add a reference if we're newly entering the active list. */
	if (!obj_priv->active) {
		drm_gem_object_reference(obj);
		obj_priv->active = 1;
	}
	/* Move from whatever list we were on to the tail of execution. */
	spin_lock(&dev_priv->mm.active_list_lock);
	list_move_tail(&obj_priv->list,
		       &dev_priv->mm.active_list);
	spin_unlock(&dev_priv->mm.active_list_lock);
	obj_priv->last_rendering_seqno = seqno;
}

static void
i915_gem_object_move_to_flushing(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	BUG_ON(!obj_priv->active);
	list_move_tail(&obj_priv->list, &dev_priv->mm.flushing_list);
	obj_priv->last_rendering_seqno = 0;
}

/* Immediately discard the backing storage */
static void
i915_gem_object_truncate(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct inode *inode;

	inode = obj->filp->f_path.dentry->d_inode;
	if (inode->i_op->truncate)
		inode->i_op->truncate (inode);

	obj_priv->madv = __I915_MADV_PURGED;
}

static inline int
i915_gem_object_is_purgeable(struct drm_i915_gem_object *obj_priv)
{
	return obj_priv->madv == I915_MADV_DONTNEED;
}

static void
i915_gem_object_move_to_inactive(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	i915_verify_inactive(dev, __FILE__, __LINE__);
	if (obj_priv->pin_count != 0)
		list_del_init(&obj_priv->list);
	else
		list_move_tail(&obj_priv->list, &dev_priv->mm.inactive_list);

	obj_priv->last_rendering_seqno = 0;
	if (obj_priv->active) {
		obj_priv->active = 0;
		drm_gem_object_unreference(obj);
	}
	i915_verify_inactive(dev, __FILE__, __LINE__);
}

/**
 * Creates a new sequence number, emitting a write of it to the status page
 * plus an interrupt, which will trigger i915_user_interrupt_handler.
 *
 * Must be called with struct_lock held.
 *
 * Returned sequence numbers are nonzero on success.
 */
static uint32_t
i915_add_request(struct drm_device *dev, struct drm_file *file_priv,
		 uint32_t flush_domains)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_file_private *i915_file_priv = NULL;
	struct drm_i915_gem_request *request;
	uint32_t seqno;
	int was_empty;
	RING_LOCALS;

	if (file_priv != NULL)
		i915_file_priv = file_priv->driver_priv;

	request = kzalloc(sizeof(*request), GFP_KERNEL);
	if (request == NULL)
		return 0;

	/* Grab the seqno we're going to make this request be, and bump the
	 * next (skipping 0 so it can be the reserved no-seqno value).
	 */
	seqno = dev_priv->mm.next_gem_seqno;
	dev_priv->mm.next_gem_seqno++;
	if (dev_priv->mm.next_gem_seqno == 0)
		dev_priv->mm.next_gem_seqno++;

	BEGIN_LP_RING(4);
	OUT_RING(MI_STORE_DWORD_INDEX);
	OUT_RING(I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	OUT_RING(seqno);

	OUT_RING(MI_USER_INTERRUPT);
	ADVANCE_LP_RING();

	DRM_DEBUG("%d\n", seqno);

	request->seqno = seqno;
	request->emitted_jiffies = jiffies;
	was_empty = list_empty(&dev_priv->mm.request_list);
	list_add_tail(&request->list, &dev_priv->mm.request_list);
	if (i915_file_priv) {
		list_add_tail(&request->client_list,
			      &i915_file_priv->mm.request_list);
	} else {
		INIT_LIST_HEAD(&request->client_list);
	}

	/* Associate any objects on the flushing list matching the write
	 * domain we're flushing with our flush.
	 */
	if (flush_domains != 0) {
		struct drm_i915_gem_object *obj_priv, *next;

		list_for_each_entry_safe(obj_priv, next,
					 &dev_priv->mm.flushing_list, list) {
			struct drm_gem_object *obj = obj_priv->obj;

			if ((obj->write_domain & flush_domains) ==
			    obj->write_domain) {
				uint32_t old_write_domain = obj->write_domain;

				obj->write_domain = 0;
				i915_gem_object_move_to_active(obj, seqno);

				trace_i915_gem_object_change_domain(obj,
								    obj->read_domains,
								    old_write_domain);
			}
		}

	}

	if (!dev_priv->mm.suspended) {
		mod_timer(&dev_priv->hangcheck_timer, jiffies + DRM_I915_HANGCHECK_PERIOD);
		if (was_empty)
			queue_delayed_work(dev_priv->wq, &dev_priv->mm.retire_work, HZ);
	}
	return seqno;
}

/**
 * Command execution barrier
 *
 * Ensures that all commands in the ring are finished
 * before signalling the CPU
 */
static uint32_t
i915_retire_commands(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
	uint32_t flush_domains = 0;
	RING_LOCALS;

	/* The sampler always gets flushed on i965 (sigh) */
	if (IS_I965G(dev))
		flush_domains |= I915_GEM_DOMAIN_SAMPLER;
	BEGIN_LP_RING(2);
	OUT_RING(cmd);
	OUT_RING(0); /* noop */
	ADVANCE_LP_RING();
	return flush_domains;
}

/**
 * Moves buffers associated only with the given active seqno from the active
 * to inactive list, potentially freeing them.
 */
static void
i915_gem_retire_request(struct drm_device *dev,
			struct drm_i915_gem_request *request)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	trace_i915_gem_request_retire(dev, request->seqno);

	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.
	 */
	spin_lock(&dev_priv->mm.active_list_lock);
	while (!list_empty(&dev_priv->mm.active_list)) {
		struct drm_gem_object *obj;
		struct drm_i915_gem_object *obj_priv;

		obj_priv = list_first_entry(&dev_priv->mm.active_list,
					    struct drm_i915_gem_object,
					    list);
		obj = obj_priv->obj;

		/* If the seqno being retired doesn't match the oldest in the
		 * list, then the oldest in the list must still be newer than
		 * this seqno.
		 */
		if (obj_priv->last_rendering_seqno != request->seqno)
			goto out;

#if WATCH_LRU
		DRM_INFO("%s: retire %d moves to inactive list %p\n",
			 __func__, request->seqno, obj);
#endif

		if (obj->write_domain != 0)
			i915_gem_object_move_to_flushing(obj);
		else {
			/* Take a reference on the object so it won't be
			 * freed while the spinlock is held.  The list
			 * protection for this spinlock is safe when breaking
			 * the lock like this since the next thing we do
			 * is just get the head of the list again.
			 */
			drm_gem_object_reference(obj);
			i915_gem_object_move_to_inactive(obj);
			spin_unlock(&dev_priv->mm.active_list_lock);
			drm_gem_object_unreference(obj);
			spin_lock(&dev_priv->mm.active_list_lock);
		}
	}
out:
	spin_unlock(&dev_priv->mm.active_list_lock);
}

/**
 * Returns true if seq1 is later than seq2.
 */
bool
i915_seqno_passed(uint32_t seq1, uint32_t seq2)
{
	return (int32_t)(seq1 - seq2) >= 0;
}

uint32_t
i915_get_gem_seqno(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	return READ_HWSP(dev_priv, I915_GEM_HWS_INDEX);
}

/**
 * This function clears the request list as sequence numbers are passed.
 */
void
i915_gem_retire_requests(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t seqno;

	if (!dev_priv->hw_status_page || list_empty(&dev_priv->mm.request_list))
		return;

	seqno = i915_get_gem_seqno(dev);

	while (!list_empty(&dev_priv->mm.request_list)) {
		struct drm_i915_gem_request *request;
		uint32_t retiring_seqno;

		request = list_first_entry(&dev_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   list);
		retiring_seqno = request->seqno;

		if (i915_seqno_passed(seqno, retiring_seqno) ||
		    atomic_read(&dev_priv->mm.wedged)) {
			i915_gem_retire_request(dev, request);

			list_del(&request->list);
			list_del(&request->client_list);
			kfree(request);
		} else
			break;
	}

	if (unlikely (dev_priv->trace_irq_seqno &&
		      i915_seqno_passed(dev_priv->trace_irq_seqno, seqno))) {
		i915_user_irq_put(dev);
		dev_priv->trace_irq_seqno = 0;
	}
}

void
i915_gem_retire_work_handler(struct work_struct *work)
{
	drm_i915_private_t *dev_priv;
	struct drm_device *dev;

	dev_priv = container_of(work, drm_i915_private_t,
				mm.retire_work.work);
	dev = dev_priv->dev;

	mutex_lock(&dev->struct_mutex);
	i915_gem_retire_requests(dev);
	if (!dev_priv->mm.suspended &&
	    !list_empty(&dev_priv->mm.request_list))
		queue_delayed_work(dev_priv->wq, &dev_priv->mm.retire_work, HZ);
	mutex_unlock(&dev->struct_mutex);
}

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 */
static int
i915_wait_request(struct drm_device *dev, uint32_t seqno)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 ier;
	int ret = 0;

	BUG_ON(seqno == 0);

	if (atomic_read(&dev_priv->mm.wedged))
		return -EIO;

	if (!i915_seqno_passed(i915_get_gem_seqno(dev), seqno)) {
		if (IS_IGDNG(dev))
			ier = I915_READ(DEIER) | I915_READ(GTIER);
		else
			ier = I915_READ(IER);
		if (!ier) {
			DRM_ERROR("something (likely vbetool) disabled "
				  "interrupts, re-enabling\n");
			i915_driver_irq_preinstall(dev);
			i915_driver_irq_postinstall(dev);
		}

		trace_i915_gem_request_wait_begin(dev, seqno);

		dev_priv->mm.waiting_gem_seqno = seqno;
		i915_user_irq_get(dev);
		ret = wait_event_interruptible(dev_priv->irq_queue,
					       i915_seqno_passed(i915_get_gem_seqno(dev),
								 seqno) ||
					       atomic_read(&dev_priv->mm.wedged));
		i915_user_irq_put(dev);
		dev_priv->mm.waiting_gem_seqno = 0;

		trace_i915_gem_request_wait_end(dev, seqno);
	}
	if (atomic_read(&dev_priv->mm.wedged))
		ret = -EIO;

	if (ret && ret != -ERESTARTSYS)
		DRM_ERROR("%s returns %d (awaiting %d at %d)\n",
			  __func__, ret, seqno, i915_get_gem_seqno(dev));

	/* Directly dispatch request retiring.  While we have the work queue
	 * to handle this, the waiter on a request often wants an associated
	 * buffer to have made it to the inactive list, and we would need
	 * a separate wait queue to handle that.
	 */
	if (ret == 0)
		i915_gem_retire_requests(dev);

	return ret;
}

static void
i915_gem_flush(struct drm_device *dev,
	       uint32_t invalidate_domains,
	       uint32_t flush_domains)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t cmd;
	RING_LOCALS;

#if WATCH_EXEC
	DRM_INFO("%s: invalidate %08x flush %08x\n", __func__,
		  invalidate_domains, flush_domains);
#endif
	trace_i915_gem_request_flush(dev, dev_priv->mm.next_gem_seqno,
				     invalidate_domains, flush_domains);

	if (flush_domains & I915_GEM_DOMAIN_CPU)
		drm_agp_chipset_flush(dev);

	if ((invalidate_domains | flush_domains) & I915_GEM_GPU_DOMAINS) {
		/*
		 * read/write caches:
		 *
		 * I915_GEM_DOMAIN_RENDER is always invalidated, but is
		 * only flushed if MI_NO_WRITE_FLUSH is unset.  On 965, it is
		 * also flushed at 2d versus 3d pipeline switches.
		 *
		 * read-only caches:
		 *
		 * I915_GEM_DOMAIN_SAMPLER is flushed on pre-965 if
		 * MI_READ_FLUSH is set, and is always flushed on 965.
		 *
		 * I915_GEM_DOMAIN_COMMAND may not exist?
		 *
		 * I915_GEM_DOMAIN_INSTRUCTION, which exists on 965, is
		 * invalidated when MI_EXE_FLUSH is set.
		 *
		 * I915_GEM_DOMAIN_VERTEX, which exists on 965, is
		 * invalidated with every MI_FLUSH.
		 *
		 * TLBs:
		 *
		 * On 965, TLBs associated with I915_GEM_DOMAIN_COMMAND
		 * and I915_GEM_DOMAIN_CPU in are invalidated at PTE write and
		 * I915_GEM_DOMAIN_RENDER and I915_GEM_DOMAIN_SAMPLER
		 * are flushed at any MI_FLUSH.
		 */

		cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
		if ((invalidate_domains|flush_domains) &
		    I915_GEM_DOMAIN_RENDER)
			cmd &= ~MI_NO_WRITE_FLUSH;
		if (!IS_I965G(dev)) {
			/*
			 * On the 965, the sampler cache always gets flushed
			 * and this bit is reserved.
			 */
			if (invalidate_domains & I915_GEM_DOMAIN_SAMPLER)
				cmd |= MI_READ_FLUSH;
		}
		if (invalidate_domains & I915_GEM_DOMAIN_INSTRUCTION)
			cmd |= MI_EXE_FLUSH;

#if WATCH_EXEC
		DRM_INFO("%s: queue flush %08x to ring\n", __func__, cmd);
#endif
		BEGIN_LP_RING(2);
		OUT_RING(cmd);
		OUT_RING(0); /* noop */
		ADVANCE_LP_RING();
	}
}

/**
 * Ensures that all rendering to the object has completed and the object is
 * safe to unbind from the GTT or access from the CPU.
 */
static int
i915_gem_object_wait_rendering(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int ret;

	/* This function only exists to support waiting for existing rendering,
	 * not for emitting required flushes.
	 */
	BUG_ON((obj->write_domain & I915_GEM_GPU_DOMAINS) != 0);

	/* If there is rendering queued on the buffer being evicted, wait for
	 * it.
	 */
	if (obj_priv->active) {
#if WATCH_BUF
		DRM_INFO("%s: object %p wait for seqno %08x\n",
			  __func__, obj, obj_priv->last_rendering_seqno);
#endif
		ret = i915_wait_request(dev, obj_priv->last_rendering_seqno);
		if (ret != 0)
			return ret;
	}

	return 0;
}

/**
 * Unbinds an object from the GTT aperture.
 */
int
i915_gem_object_unbind(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int ret = 0;

#if WATCH_BUF
	DRM_INFO("%s:%d %p\n", __func__, __LINE__, obj);
	DRM_INFO("gtt_space %p\n", obj_priv->gtt_space);
#endif
	if (obj_priv->gtt_space == NULL)
		return 0;

	if (obj_priv->pin_count != 0) {
		DRM_ERROR("Attempting to unbind pinned buffer\n");
		return -EINVAL;
	}

	/* blow away mappings if mapped through GTT */
	i915_gem_release_mmap(obj);

	if (obj_priv->fence_reg != I915_FENCE_REG_NONE)
		i915_gem_clear_fence_reg(obj);

	/* Move the object to the CPU domain to ensure that
	 * any possible CPU writes while it's not in the GTT
	 * are flushed when we go to remap it. This will
	 * also ensure that all pending GPU writes are finished
	 * before we unbind.
	 */
	ret = i915_gem_object_set_to_cpu_domain(obj, 1);
	if (ret) {
		if (ret != -ERESTARTSYS)
			DRM_ERROR("set_domain failed: %d\n", ret);
		return ret;
	}

	BUG_ON(obj_priv->active);

	if (obj_priv->agp_mem != NULL) {
		drm_unbind_agp(obj_priv->agp_mem);
		drm_free_agp(obj_priv->agp_mem, obj->size / PAGE_SIZE);
		obj_priv->agp_mem = NULL;
	}

	i915_gem_object_put_pages(obj);
	BUG_ON(obj_priv->pages_refcount);

	if (obj_priv->gtt_space) {
		atomic_dec(&dev->gtt_count);
		atomic_sub(obj->size, &dev->gtt_memory);

		drm_mm_put_block(obj_priv->gtt_space);
		obj_priv->gtt_space = NULL;
	}

	/* Remove ourselves from the LRU list if present. */
	if (!list_empty(&obj_priv->list))
		list_del_init(&obj_priv->list);

	if (i915_gem_object_is_purgeable(obj_priv))
		i915_gem_object_truncate(obj);

	trace_i915_gem_object_unbind(obj);

	return 0;
}

static struct drm_gem_object *
i915_gem_find_inactive_object(struct drm_device *dev, int min_size)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv;
	struct drm_gem_object *best = NULL;
	struct drm_gem_object *first = NULL;

	/* Try to find the smallest clean object */
	list_for_each_entry(obj_priv, &dev_priv->mm.inactive_list, list) {
		struct drm_gem_object *obj = obj_priv->obj;
		if (obj->size >= min_size) {
			if ((!obj_priv->dirty ||
			     i915_gem_object_is_purgeable(obj_priv)) &&
			    (!best || obj->size < best->size)) {
				best = obj;
				if (best->size == min_size)
					return best;
			}
			if (!first)
			    first = obj;
		}
	}

	return best ? best : first;
}

static int
i915_gem_evict_everything(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t seqno;
	int ret;
	bool lists_empty;

	spin_lock(&dev_priv->mm.active_list_lock);
	lists_empty = (list_empty(&dev_priv->mm.inactive_list) &&
		       list_empty(&dev_priv->mm.flushing_list) &&
		       list_empty(&dev_priv->mm.active_list));
	spin_unlock(&dev_priv->mm.active_list_lock);

	if (lists_empty)
		return -ENOSPC;

	/* Flush everything (on to the inactive lists) and evict */
	i915_gem_flush(dev, I915_GEM_GPU_DOMAINS, I915_GEM_GPU_DOMAINS);
	seqno = i915_add_request(dev, NULL, I915_GEM_GPU_DOMAINS);
	if (seqno == 0)
		return -ENOMEM;

	ret = i915_wait_request(dev, seqno);
	if (ret)
		return ret;

	ret = i915_gem_evict_from_inactive_list(dev);
	if (ret)
		return ret;

	spin_lock(&dev_priv->mm.active_list_lock);
	lists_empty = (list_empty(&dev_priv->mm.inactive_list) &&
		       list_empty(&dev_priv->mm.flushing_list) &&
		       list_empty(&dev_priv->mm.active_list));
	spin_unlock(&dev_priv->mm.active_list_lock);
	BUG_ON(!lists_empty);

	return 0;
}

static int
i915_gem_evict_something(struct drm_device *dev, int min_size)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	int ret;

	for (;;) {
		i915_gem_retire_requests(dev);

		/* If there's an inactive buffer available now, grab it
		 * and be done.
		 */
		obj = i915_gem_find_inactive_object(dev, min_size);
		if (obj) {
			struct drm_i915_gem_object *obj_priv;

#if WATCH_LRU
			DRM_INFO("%s: evicting %p\n", __func__, obj);
#endif
			obj_priv = obj->driver_private;
			BUG_ON(obj_priv->pin_count != 0);
			BUG_ON(obj_priv->active);

			/* Wait on the rendering and unbind the buffer. */
			return i915_gem_object_unbind(obj);
		}

		/* If we didn't get anything, but the ring is still processing
		 * things, wait for the next to finish and hopefully leave us
		 * a buffer to evict.
		 */
		if (!list_empty(&dev_priv->mm.request_list)) {
			struct drm_i915_gem_request *request;

			request = list_first_entry(&dev_priv->mm.request_list,
						   struct drm_i915_gem_request,
						   list);

			ret = i915_wait_request(dev, request->seqno);
			if (ret)
				return ret;

			continue;
		}

		/* If we didn't have anything on the request list but there
		 * are buffers awaiting a flush, emit one and try again.
		 * When we wait on it, those buffers waiting for that flush
		 * will get moved to inactive.
		 */
		if (!list_empty(&dev_priv->mm.flushing_list)) {
			struct drm_i915_gem_object *obj_priv;

			/* Find an object that we can immediately reuse */
			list_for_each_entry(obj_priv, &dev_priv->mm.flushing_list, list) {
				obj = obj_priv->obj;
				if (obj->size >= min_size)
					break;

				obj = NULL;
			}

			if (obj != NULL) {
				uint32_t seqno;

				i915_gem_flush(dev,
					       obj->write_domain,
					       obj->write_domain);
				seqno = i915_add_request(dev, NULL, obj->write_domain);
				if (seqno == 0)
					return -ENOMEM;

				ret = i915_wait_request(dev, seqno);
				if (ret)
					return ret;

				continue;
			}
		}

		/* If we didn't do any of the above, there's no single buffer
		 * large enough to swap out for the new one, so just evict
		 * everything and start again. (This should be rare.)
		 */
		if (!list_empty (&dev_priv->mm.inactive_list))
			return i915_gem_evict_from_inactive_list(dev);
		else
			return i915_gem_evict_everything(dev);
	}
}

int
i915_gem_object_get_pages(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int page_count, i;
	struct address_space *mapping;
	struct inode *inode;
	struct page *page;
	int ret;

	if (obj_priv->pages_refcount++ != 0)
		return 0;

	/* Get the list of pages out of our struct file.  They'll be pinned
	 * at this point until we release them.
	 */
	page_count = obj->size / PAGE_SIZE;
	BUG_ON(obj_priv->pages != NULL);
	obj_priv->pages = drm_calloc_large(page_count, sizeof(struct page *));
	if (obj_priv->pages == NULL) {
		obj_priv->pages_refcount--;
		return -ENOMEM;
	}

	inode = obj->filp->f_path.dentry->d_inode;
	mapping = inode->i_mapping;
	for (i = 0; i < page_count; i++) {
		page = read_mapping_page(mapping, i, NULL);
		if (IS_ERR(page)) {
			ret = PTR_ERR(page);
			i915_gem_object_put_pages(obj);
			return ret;
		}
		obj_priv->pages[i] = page;
	}

	if (obj_priv->tiling_mode != I915_TILING_NONE)
		i915_gem_object_do_bit_17_swizzle(obj);

	return 0;
}

static void i965_write_fence_reg(struct drm_i915_fence_reg *reg)
{
	struct drm_gem_object *obj = reg->obj;
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int regnum = obj_priv->fence_reg;
	uint64_t val;

	val = (uint64_t)((obj_priv->gtt_offset + obj->size - 4096) &
		    0xfffff000) << 32;
	val |= obj_priv->gtt_offset & 0xfffff000;
	val |= ((obj_priv->stride / 128) - 1) << I965_FENCE_PITCH_SHIFT;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I965_FENCE_TILING_Y_SHIFT;
	val |= I965_FENCE_REG_VALID;

	I915_WRITE64(FENCE_REG_965_0 + (regnum * 8), val);
}

static void i915_write_fence_reg(struct drm_i915_fence_reg *reg)
{
	struct drm_gem_object *obj = reg->obj;
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int regnum = obj_priv->fence_reg;
	int tile_width;
	uint32_t fence_reg, val;
	uint32_t pitch_val;

	if ((obj_priv->gtt_offset & ~I915_FENCE_START_MASK) ||
	    (obj_priv->gtt_offset & (obj->size - 1))) {
		WARN(1, "%s: object 0x%08x not 1M or size (0x%zx) aligned\n",
		     __func__, obj_priv->gtt_offset, obj->size);
		return;
	}

	if (obj_priv->tiling_mode == I915_TILING_Y &&
	    HAS_128_BYTE_Y_TILING(dev))
		tile_width = 128;
	else
		tile_width = 512;

	/* Note: pitch better be a power of two tile widths */
	pitch_val = obj_priv->stride / tile_width;
	pitch_val = ffs(pitch_val) - 1;

	val = obj_priv->gtt_offset;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	val |= I915_FENCE_SIZE_BITS(obj->size);
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	if (regnum < 8)
		fence_reg = FENCE_REG_830_0 + (regnum * 4);
	else
		fence_reg = FENCE_REG_945_8 + ((regnum - 8) * 4);
	I915_WRITE(fence_reg, val);
}

static void i830_write_fence_reg(struct drm_i915_fence_reg *reg)
{
	struct drm_gem_object *obj = reg->obj;
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int regnum = obj_priv->fence_reg;
	uint32_t val;
	uint32_t pitch_val;
	uint32_t fence_size_bits;

	if ((obj_priv->gtt_offset & ~I830_FENCE_START_MASK) ||
	    (obj_priv->gtt_offset & (obj->size - 1))) {
		WARN(1, "%s: object 0x%08x not 512K or size aligned\n",
		     __func__, obj_priv->gtt_offset);
		return;
	}

	pitch_val = obj_priv->stride / 128;
	pitch_val = ffs(pitch_val) - 1;
	WARN_ON(pitch_val > I830_FENCE_MAX_PITCH_VAL);

	val = obj_priv->gtt_offset;
	if (obj_priv->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	fence_size_bits = I830_FENCE_SIZE_BITS(obj->size);
	WARN_ON(fence_size_bits & ~0x00000f00);
	val |= fence_size_bits;
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	I915_WRITE(FENCE_REG_830_0 + (regnum * 4), val);
}

/**
 * i915_gem_object_get_fence_reg - set up a fence reg for an object
 * @obj: object to map through a fence reg
 *
 * When mapping objects through the GTT, userspace wants to be able to write
 * to them without having to worry about swizzling if the object is tiled.
 *
 * This function walks the fence regs looking for a free one for @obj,
 * stealing one if it can't find any.
 *
 * It then sets up the reg based on the object's properties: address, pitch
 * and tiling format.
 */
int
i915_gem_object_get_fence_reg(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct drm_i915_fence_reg *reg = NULL;
	struct drm_i915_gem_object *old_obj_priv = NULL;
	int i, ret, avail;

	/* Just update our place in the LRU if our fence is getting used. */
	if (obj_priv->fence_reg != I915_FENCE_REG_NONE) {
		list_move_tail(&obj_priv->fence_list, &dev_priv->mm.fence_list);
		return 0;
	}

	switch (obj_priv->tiling_mode) {
	case I915_TILING_NONE:
		WARN(1, "allocating a fence for non-tiled object?\n");
		break;
	case I915_TILING_X:
		if (!obj_priv->stride)
			return -EINVAL;
		WARN((obj_priv->stride & (512 - 1)),
		     "object 0x%08x is X tiled but has non-512B pitch\n",
		     obj_priv->gtt_offset);
		break;
	case I915_TILING_Y:
		if (!obj_priv->stride)
			return -EINVAL;
		WARN((obj_priv->stride & (128 - 1)),
		     "object 0x%08x is Y tiled but has non-128B pitch\n",
		     obj_priv->gtt_offset);
		break;
	}

	/* First try to find a free reg */
	avail = 0;
	for (i = dev_priv->fence_reg_start; i < dev_priv->num_fence_regs; i++) {
		reg = &dev_priv->fence_regs[i];
		if (!reg->obj)
			break;

		old_obj_priv = reg->obj->driver_private;
		if (!old_obj_priv->pin_count)
		    avail++;
	}

	/* None available, try to steal one or wait for a user to finish */
	if (i == dev_priv->num_fence_regs) {
		struct drm_gem_object *old_obj = NULL;

		if (avail == 0)
			return -ENOSPC;

		list_for_each_entry(old_obj_priv, &dev_priv->mm.fence_list,
				    fence_list) {
			old_obj = old_obj_priv->obj;

			if (old_obj_priv->pin_count)
				continue;

			/* Take a reference, as otherwise the wait_rendering
			 * below may cause the object to get freed out from
			 * under us.
			 */
			drm_gem_object_reference(old_obj);

			/* i915 uses fences for GPU access to tiled buffers */
			if (IS_I965G(dev) || !old_obj_priv->active)
				break;

			/* This brings the object to the head of the LRU if it
			 * had been written to.  The only way this should
			 * result in us waiting longer than the expected
			 * optimal amount of time is if there was a
			 * fence-using buffer later that was read-only.
			 */
			i915_gem_object_flush_gpu_write_domain(old_obj);
			ret = i915_gem_object_wait_rendering(old_obj);
			if (ret != 0) {
				drm_gem_object_unreference(old_obj);
				return ret;
			}

			break;
		}

		/*
		 * Zap this virtual mapping so we can set up a fence again
		 * for this object next time we need it.
		 */
		i915_gem_release_mmap(old_obj);

		i = old_obj_priv->fence_reg;
		reg = &dev_priv->fence_regs[i];

		old_obj_priv->fence_reg = I915_FENCE_REG_NONE;
		list_del_init(&old_obj_priv->fence_list);

		drm_gem_object_unreference(old_obj);
	}

	obj_priv->fence_reg = i;
	list_add_tail(&obj_priv->fence_list, &dev_priv->mm.fence_list);

	reg->obj = obj;

	if (IS_I965G(dev))
		i965_write_fence_reg(reg);
	else if (IS_I9XX(dev))
		i915_write_fence_reg(reg);
	else
		i830_write_fence_reg(reg);

	trace_i915_gem_object_get_fence(obj, i, obj_priv->tiling_mode);

	return 0;
}

/**
 * i915_gem_clear_fence_reg - clear out fence register info
 * @obj: object to clear
 *
 * Zeroes out the fence register itself and clears out the associated
 * data structures in dev_priv and obj_priv.
 */
static void
i915_gem_clear_fence_reg(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (IS_I965G(dev))
		I915_WRITE64(FENCE_REG_965_0 + (obj_priv->fence_reg * 8), 0);
	else {
		uint32_t fence_reg;

		if (obj_priv->fence_reg < 8)
			fence_reg = FENCE_REG_830_0 + obj_priv->fence_reg * 4;
		else
			fence_reg = FENCE_REG_945_8 + (obj_priv->fence_reg -
						       8) * 4;

		I915_WRITE(fence_reg, 0);
	}

	dev_priv->fence_regs[obj_priv->fence_reg].obj = NULL;
	obj_priv->fence_reg = I915_FENCE_REG_NONE;
	list_del_init(&obj_priv->fence_list);
}

/**
 * i915_gem_object_put_fence_reg - waits on outstanding fenced access
 * to the buffer to finish, and then resets the fence register.
 * @obj: tiled object holding a fence register.
 *
 * Zeroes out the fence register itself and clears out the associated
 * data structures in dev_priv and obj_priv.
 */
int
i915_gem_object_put_fence_reg(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (obj_priv->fence_reg == I915_FENCE_REG_NONE)
		return 0;

	/* On the i915, GPU access to tiled buffers is via a fence,
	 * therefore we must wait for any outstanding access to complete
	 * before clearing the fence.
	 */
	if (!IS_I965G(dev)) {
		int ret;

		i915_gem_object_flush_gpu_write_domain(obj);
		i915_gem_object_flush_gtt_write_domain(obj);
		ret = i915_gem_object_wait_rendering(obj);
		if (ret != 0)
			return ret;
	}

	i915_gem_clear_fence_reg (obj);

	return 0;
}

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
static int
i915_gem_object_bind_to_gtt(struct drm_gem_object *obj, unsigned alignment)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	struct drm_mm_node *free_space;
	bool retry_alloc = false;
	int ret;

	if (dev_priv->mm.suspended)
		return -EBUSY;

	if (obj_priv->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to bind a purgeable object\n");
		return -EINVAL;
	}

	if (alignment == 0)
		alignment = i915_gem_get_gtt_alignment(obj);
	if (alignment & (i915_gem_get_gtt_alignment(obj) - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return -EINVAL;
	}

 search_free:
	free_space = drm_mm_search_free(&dev_priv->mm.gtt_space,
					obj->size, alignment, 0);
	if (free_space != NULL) {
		obj_priv->gtt_space = drm_mm_get_block(free_space, obj->size,
						       alignment);
		if (obj_priv->gtt_space != NULL) {
			obj_priv->gtt_space->private = obj;
			obj_priv->gtt_offset = obj_priv->gtt_space->start;
		}
	}
	if (obj_priv->gtt_space == NULL) {
		/* If the gtt is empty and we're still having trouble
		 * fitting our object in, we're out of memory.
		 */
#if WATCH_LRU
		DRM_INFO("%s: GTT full, evicting something\n", __func__);
#endif
		ret = i915_gem_evict_something(dev, obj->size);
		if (ret)
			return ret;

		goto search_free;
	}

#if WATCH_BUF
	DRM_INFO("Binding object of size %zd at 0x%08x\n",
		 obj->size, obj_priv->gtt_offset);
#endif
	if (retry_alloc) {
		i915_gem_object_set_page_gfp_mask (obj,
						   i915_gem_object_get_page_gfp_mask (obj) & ~__GFP_NORETRY);
	}
	ret = i915_gem_object_get_pages(obj);
	if (retry_alloc) {
		i915_gem_object_set_page_gfp_mask (obj,
						   i915_gem_object_get_page_gfp_mask (obj) | __GFP_NORETRY);
	}
	if (ret) {
		drm_mm_put_block(obj_priv->gtt_space);
		obj_priv->gtt_space = NULL;

		if (ret == -ENOMEM) {
			/* first try to clear up some space from the GTT */
			ret = i915_gem_evict_something(dev, obj->size);
			if (ret) {
				/* now try to shrink everyone else */
				if (! retry_alloc) {
				    retry_alloc = true;
				    goto search_free;
				}

				return ret;
			}

			goto search_free;
		}

		return ret;
	}

	/* Create an AGP memory structure pointing at our pages, and bind it
	 * into the GTT.
	 */
	obj_priv->agp_mem = drm_agp_bind_pages(dev,
					       obj_priv->pages,
					       obj->size >> PAGE_SHIFT,
					       obj_priv->gtt_offset,
					       obj_priv->agp_type);
	if (obj_priv->agp_mem == NULL) {
		i915_gem_object_put_pages(obj);
		drm_mm_put_block(obj_priv->gtt_space);
		obj_priv->gtt_space = NULL;

		ret = i915_gem_evict_something(dev, obj->size);
		if (ret)
			return ret;

		goto search_free;
	}
	atomic_inc(&dev->gtt_count);
	atomic_add(obj->size, &dev->gtt_memory);

	/* Assert that the object is not currently in any GPU domain. As it
	 * wasn't in the GTT, there shouldn't be any way it could have been in
	 * a GPU cache
	 */
	BUG_ON(obj->read_domains & I915_GEM_GPU_DOMAINS);
	BUG_ON(obj->write_domain & I915_GEM_GPU_DOMAINS);

	trace_i915_gem_object_bind(obj, obj_priv->gtt_offset);

	return 0;
}

void
i915_gem_clflush_object(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object	*obj_priv = obj->driver_private;

	/* If we don't have a page list set up, then we're not pinned
	 * to GPU, and we can ignore the cache flush because it'll happen
	 * again at bind time.
	 */
	if (obj_priv->pages == NULL)
		return;

	trace_i915_gem_object_clflush(obj);

	drm_clflush_pages(obj_priv->pages, obj->size / PAGE_SIZE);
}

/** Flushes any GPU write domain for the object if it's dirty. */
static void
i915_gem_object_flush_gpu_write_domain(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	uint32_t seqno;
	uint32_t old_write_domain;

	if ((obj->write_domain & I915_GEM_GPU_DOMAINS) == 0)
		return;

	/* Queue the GPU write cache flushing we need. */
	old_write_domain = obj->write_domain;
	i915_gem_flush(dev, 0, obj->write_domain);
	seqno = i915_add_request(dev, NULL, obj->write_domain);
	obj->write_domain = 0;
	i915_gem_object_move_to_active(obj, seqno);

	trace_i915_gem_object_change_domain(obj,
					    obj->read_domains,
					    old_write_domain);
}

/** Flushes the GTT write domain for the object if it's dirty. */
static void
i915_gem_object_flush_gtt_write_domain(struct drm_gem_object *obj)
{
	uint32_t old_write_domain;

	if (obj->write_domain != I915_GEM_DOMAIN_GTT)
		return;

	/* No actual flushing is required for the GTT write domain.   Writes
	 * to it immediately go to main memory as far as we know, so there's
	 * no chipset flush.  It also doesn't land in render cache.
	 */
	old_write_domain = obj->write_domain;
	obj->write_domain = 0;

	trace_i915_gem_object_change_domain(obj,
					    obj->read_domains,
					    old_write_domain);
}

/** Flushes the CPU write domain for the object if it's dirty. */
static void
i915_gem_object_flush_cpu_write_domain(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	uint32_t old_write_domain;

	if (obj->write_domain != I915_GEM_DOMAIN_CPU)
		return;

	i915_gem_clflush_object(obj);
	drm_agp_chipset_flush(dev);
	old_write_domain = obj->write_domain;
	obj->write_domain = 0;

	trace_i915_gem_object_change_domain(obj,
					    obj->read_domains,
					    old_write_domain);
}

/**
 * Moves a single object to the GTT read, and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
int
i915_gem_object_set_to_gtt_domain(struct drm_gem_object *obj, int write)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	uint32_t old_write_domain, old_read_domains;
	int ret;

	/* Not valid to be called on unbound objects. */
	if (obj_priv->gtt_space == NULL)
		return -EINVAL;

	i915_gem_object_flush_gpu_write_domain(obj);
	/* Wait on any GPU rendering and flushing to occur. */
	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return ret;

	old_write_domain = obj->write_domain;
	old_read_domains = obj->read_domains;

	/* If we're writing through the GTT domain, then CPU and GPU caches
	 * will need to be invalidated at next use.
	 */
	if (write)
		obj->read_domains &= I915_GEM_DOMAIN_GTT;

	i915_gem_object_flush_cpu_write_domain(obj);

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	BUG_ON((obj->write_domain & ~I915_GEM_DOMAIN_GTT) != 0);
	obj->read_domains |= I915_GEM_DOMAIN_GTT;
	if (write) {
		obj->write_domain = I915_GEM_DOMAIN_GTT;
		obj_priv->dirty = 1;
	}

	trace_i915_gem_object_change_domain(obj,
					    old_read_domains,
					    old_write_domain);

	return 0;
}

/**
 * Moves a single object to the CPU read, and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
static int
i915_gem_object_set_to_cpu_domain(struct drm_gem_object *obj, int write)
{
	uint32_t old_write_domain, old_read_domains;
	int ret;

	i915_gem_object_flush_gpu_write_domain(obj);
	/* Wait on any GPU rendering and flushing to occur. */
	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return ret;

	i915_gem_object_flush_gtt_write_domain(obj);

	/* If we have a partially-valid cache of the object in the CPU,
	 * finish invalidating it and free the per-page flags.
	 */
	i915_gem_object_set_to_full_cpu_read_domain(obj);

	old_write_domain = obj->write_domain;
	old_read_domains = obj->read_domains;

	/* Flush the CPU cache if it's still invalid. */
	if ((obj->read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		i915_gem_clflush_object(obj);

		obj->read_domains |= I915_GEM_DOMAIN_CPU;
	}

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	BUG_ON((obj->write_domain & ~I915_GEM_DOMAIN_CPU) != 0);

	/* If we're writing through the CPU, then the GPU read domains will
	 * need to be invalidated at next use.
	 */
	if (write) {
		obj->read_domains &= I915_GEM_DOMAIN_CPU;
		obj->write_domain = I915_GEM_DOMAIN_CPU;
	}

	trace_i915_gem_object_change_domain(obj,
					    old_read_domains,
					    old_write_domain);

	return 0;
}

/*
 * Set the next domain for the specified object. This
 * may not actually perform the necessary flushing/invaliding though,
 * as that may want to be batched with other set_domain operations
 *
 * This is (we hope) the only really tricky part of gem. The goal
 * is fairly simple -- track which caches hold bits of the object
 * and make sure they remain coherent. A few concrete examples may
 * help to explain how it works. For shorthand, we use the notation
 * (read_domains, write_domain), e.g. (CPU, CPU) to indicate the
 * a pair of read and write domain masks.
 *
 * Case 1: the batch buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Mapped to GTT
 *	4. Read by GPU
 *	5. Unmapped from GTT
 *	6. Freed
 *
 *	Let's take these a step at a time
 *
 *	1. Allocated
 *		Pages allocated from the kernel may still have
 *		cache contents, so we set them to (CPU, CPU) always.
 *	2. Written by CPU (using pwrite)
 *		The pwrite function calls set_domain (CPU, CPU) and
 *		this function does nothing (as nothing changes)
 *	3. Mapped by GTT
 *		This function asserts that the object is not
 *		currently in any GPU-based read or write domains
 *	4. Read by GPU
 *		i915_gem_execbuffer calls set_domain (COMMAND, 0).
 *		As write_domain is zero, this function adds in the
 *		current read domains (CPU+COMMAND, 0).
 *		flush_domains is set to CPU.
 *		invalidate_domains is set to COMMAND
 *		clflush is run to get data out of the CPU caches
 *		then i915_dev_set_domain calls i915_gem_flush to
 *		emit an MI_FLUSH and drm_agp_chipset_flush
 *	5. Unmapped from GTT
 *		i915_gem_object_unbind calls set_domain (CPU, CPU)
 *		flush_domains and invalidate_domains end up both zero
 *		so no flushing/invalidating happens
 *	6. Freed
 *		yay, done
 *
 * Case 2: The shared render buffer
 *
 *	1. Allocated
 *	2. Mapped to GTT
 *	3. Read/written by GPU
 *	4. set_domain to (CPU,CPU)
 *	5. Read/written by CPU
 *	6. Read/written by GPU
 *
 *	1. Allocated
 *		Same as last example, (CPU, CPU)
 *	2. Mapped to GTT
 *		Nothing changes (assertions find that it is not in the GPU)
 *	3. Read/written by GPU
 *		execbuffer calls set_domain (RENDER, RENDER)
 *		flush_domains gets CPU
 *		invalidate_domains gets GPU
 *		clflush (obj)
 *		MI_FLUSH and drm_agp_chipset_flush
 *	4. set_domain (CPU, CPU)
 *		flush_domains gets GPU
 *		invalidate_domains gets CPU
 *		wait_rendering (obj) to make sure all drawing is complete.
 *		This will include an MI_FLUSH to get the data from GPU
 *		to memory
 *		clflush (obj) to invalidate the CPU cache
 *		Another MI_FLUSH in i915_gem_flush (eliminate this somehow?)
 *	5. Read/written by CPU
 *		cache lines are loaded and dirtied
 *	6. Read written by GPU
 *		Same as last GPU access
 *
 * Case 3: The constant buffer
 *
 *	1. Allocated
 *	2. Written by CPU
 *	3. Read by GPU
 *	4. Updated (written) by CPU again
 *	5. Read by GPU
 *
 *	1. Allocated
 *		(CPU, CPU)
 *	2. Written by CPU
 *		(CPU, CPU)
 *	3. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 *	4. Updated (written) by CPU again
 *		(CPU, CPU)
 *		flush_domains = 0 (no previous write domain)
 *		invalidate_domains = 0 (no new read domains)
 *	5. Read by GPU
 *		(CPU+RENDER, 0)
 *		flush_domains = CPU
 *		invalidate_domains = RENDER
 *		clflush (obj)
 *		MI_FLUSH
 *		drm_agp_chipset_flush
 */
static void
i915_gem_object_set_to_gpu_domain(struct drm_gem_object *obj)
{
	struct drm_device		*dev = obj->dev;
	struct drm_i915_gem_object	*obj_priv = obj->driver_private;
	uint32_t			invalidate_domains = 0;
	uint32_t			flush_domains = 0;
	uint32_t			old_read_domains;

	BUG_ON(obj->pending_read_domains & I915_GEM_DOMAIN_CPU);
	BUG_ON(obj->pending_write_domain == I915_GEM_DOMAIN_CPU);

	intel_mark_busy(dev, obj);

#if WATCH_BUF
	DRM_INFO("%s: object %p read %08x -> %08x write %08x -> %08x\n",
		 __func__, obj,
		 obj->read_domains, obj->pending_read_domains,
		 obj->write_domain, obj->pending_write_domain);
#endif
	/*
	 * If the object isn't moving to a new write domain,
	 * let the object stay in multiple read domains
	 */
	if (obj->pending_write_domain == 0)
		obj->pending_read_domains |= obj->read_domains;
	else
		obj_priv->dirty = 1;

	/*
	 * Flush the current write domain if
	 * the new read domains don't match. Invalidate
	 * any read domains which differ from the old
	 * write domain
	 */
	if (obj->write_domain &&
	    obj->write_domain != obj->pending_read_domains) {
		flush_domains |= obj->write_domain;
		invalidate_domains |=
			obj->pending_read_domains & ~obj->write_domain;
	}
	/*
	 * Invalidate any read caches which may have
	 * stale data. That is, any new read domains.
	 */
	invalidate_domains |= obj->pending_read_domains & ~obj->read_domains;
	if ((flush_domains | invalidate_domains) & I915_GEM_DOMAIN_CPU) {
#if WATCH_BUF
		DRM_INFO("%s: CPU domain flush %08x invalidate %08x\n",
			 __func__, flush_domains, invalidate_domains);
#endif
		i915_gem_clflush_object(obj);
	}

	old_read_domains = obj->read_domains;

	/* The actual obj->write_domain will be updated with
	 * pending_write_domain after we emit the accumulated flush for all
	 * of our domain changes in execbuffers (which clears objects'
	 * write_domains).  So if we have a current write domain that we
	 * aren't changing, set pending_write_domain to that.
	 */
	if (flush_domains == 0 && obj->pending_write_domain == 0)
		obj->pending_write_domain = obj->write_domain;
	obj->read_domains = obj->pending_read_domains;

	dev->invalidate_domains |= invalidate_domains;
	dev->flush_domains |= flush_domains;
#if WATCH_BUF
	DRM_INFO("%s: read %08x write %08x invalidate %08x flush %08x\n",
		 __func__,
		 obj->read_domains, obj->write_domain,
		 dev->invalidate_domains, dev->flush_domains);
#endif

	trace_i915_gem_object_change_domain(obj,
					    old_read_domains,
					    obj->write_domain);
}

/**
 * Moves the object from a partially CPU read to a full one.
 *
 * Note that this only resolves i915_gem_object_set_cpu_read_domain_range(),
 * and doesn't handle transitioning from !(read_domains & I915_GEM_DOMAIN_CPU).
 */
static void
i915_gem_object_set_to_full_cpu_read_domain(struct drm_gem_object *obj)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;

	if (!obj_priv->page_cpu_valid)
		return;

	/* If we're partially in the CPU read domain, finish moving it in.
	 */
	if (obj->read_domains & I915_GEM_DOMAIN_CPU) {
		int i;

		for (i = 0; i <= (obj->size - 1) / PAGE_SIZE; i++) {
			if (obj_priv->page_cpu_valid[i])
				continue;
			drm_clflush_pages(obj_priv->pages + i, 1);
		}
	}

	/* Free the page_cpu_valid mappings which are now stale, whether
	 * or not we've got I915_GEM_DOMAIN_CPU.
	 */
	kfree(obj_priv->page_cpu_valid);
	obj_priv->page_cpu_valid = NULL;
}

/**
 * Set the CPU read domain on a range of the object.
 *
 * The object ends up with I915_GEM_DOMAIN_CPU in its read flags although it's
 * not entirely valid.  The page_cpu_valid member of the object flags which
 * pages have been flushed, and will be respected by
 * i915_gem_object_set_to_cpu_domain() if it's called on to get a valid mapping
 * of the whole object.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
static int
i915_gem_object_set_cpu_read_domain_range(struct drm_gem_object *obj,
					  uint64_t offset, uint64_t size)
{
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	uint32_t old_read_domains;
	int i, ret;

	if (offset == 0 && size == obj->size)
		return i915_gem_object_set_to_cpu_domain(obj, 0);

	i915_gem_object_flush_gpu_write_domain(obj);
	/* Wait on any GPU rendering and flushing to occur. */
	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return ret;
	i915_gem_object_flush_gtt_write_domain(obj);

	/* If we're already fully in the CPU read domain, we're done. */
	if (obj_priv->page_cpu_valid == NULL &&
	    (obj->read_domains & I915_GEM_DOMAIN_CPU) != 0)
		return 0;

	/* Otherwise, create/clear the per-page CPU read domain flag if we're
	 * newly adding I915_GEM_DOMAIN_CPU
	 */
	if (obj_priv->page_cpu_valid == NULL) {
		obj_priv->page_cpu_valid = kzalloc(obj->size / PAGE_SIZE,
						   GFP_KERNEL);
		if (obj_priv->page_cpu_valid == NULL)
			return -ENOMEM;
	} else if ((obj->read_domains & I915_GEM_DOMAIN_CPU) == 0)
		memset(obj_priv->page_cpu_valid, 0, obj->size / PAGE_SIZE);

	/* Flush the cache on any pages that are still invalid from the CPU's
	 * perspective.
	 */
	for (i = offset / PAGE_SIZE; i <= (offset + size - 1) / PAGE_SIZE;
	     i++) {
		if (obj_priv->page_cpu_valid[i])
			continue;

		drm_clflush_pages(obj_priv->pages + i, 1);

		obj_priv->page_cpu_valid[i] = 1;
	}

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	BUG_ON((obj->write_domain & ~I915_GEM_DOMAIN_CPU) != 0);

	old_read_domains = obj->read_domains;
	obj->read_domains |= I915_GEM_DOMAIN_CPU;

	trace_i915_gem_object_change_domain(obj,
					    old_read_domains,
					    obj->write_domain);

	return 0;
}

/**
 * Pin an object to the GTT and evaluate the relocations landing in it.
 */
static int
i915_gem_object_pin_and_relocate(struct drm_gem_object *obj,
				 struct drm_file *file_priv,
				 struct drm_i915_gem_exec_object *entry,
				 struct drm_i915_gem_relocation_entry *relocs)
{
	struct drm_device *dev = obj->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj_priv = obj->driver_private;
	int i, ret;
	void __iomem *reloc_page;

	/* Choose the GTT offset for our buffer and put it there. */
	ret = i915_gem_object_pin(obj, (uint32_t) entry->alignment);
	if (ret)
		return ret;

	entry->offset = obj_priv->gtt_offset;

	/* Apply the relocations, using the GTT aperture to avoid cache
	 * flushing requirements.
	 */
	for (i = 0; i < entry->relocation_count; i++) {
		struct drm_i915_gem_relocation_entry *reloc= &relocs[i];
		struct drm_gem_object *target_obj;
		struct drm_i915_gem_object *target_obj_priv;
		uint32_t reloc_val, reloc_offset;
		uint32_t __iomem *reloc_entry;

		target_obj = drm_gem_object_lookup(obj->dev, file_priv,
						   reloc->target_handle);
		if (target_obj == NULL) {
			i915_gem_object_unpin(obj);
			return -EBADF;
		}
		target_obj_priv = target_obj->driver_private;

#if WATCH_RELOC
		DRM_INFO("%s: obj %p offset %08x target %d "
			 "read %08x write %08x gtt %08x "
			 "presumed %08x delta %08x\n",
			 __func__,
			 obj,
			 (int) reloc->offset,
			 (int) reloc->target_handle,
			 (int) reloc->read_domains,
			 (int) reloc->write_domain,
			 (int) target_obj_priv->gtt_offset,
			 (int) reloc->presumed_offset,
			 reloc->delta);
#endif

		/* The target buffer should have appeared before us in the
		 * exec_object list, so it should have a GTT space bound by now.
		 */
		if (target_obj_priv->gtt_space == NULL) {
			DRM_ERROR("No GTT space found for object %d\n",
				  reloc->target_handle);
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}

		/* Validate that the target is in a valid r/w GPU domain */
		if (reloc->write_domain & I915_GEM_DOMAIN_CPU ||
		    reloc->read_domains & I915_GEM_DOMAIN_CPU) {
			DRM_ERROR("reloc with read/write CPU domains: "
				  "obj %p target %d offset %d "
				  "read %08x write %08x",
				  obj, reloc->target_handle,
				  (int) reloc->offset,
				  reloc->read_domains,
				  reloc->write_domain);
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}
		if (reloc->write_domain && target_obj->pending_write_domain &&
		    reloc->write_domain != target_obj->pending_write_domain) {
			DRM_ERROR("Write domain conflict: "
				  "obj %p target %d offset %d "
				  "new %08x old %08x\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset,
				  reloc->write_domain,
				  target_obj->pending_write_domain);
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}

		target_obj->pending_read_domains |= reloc->read_domains;
		target_obj->pending_write_domain |= reloc->write_domain;

		/* If the relocation already has the right value in it, no
		 * more work needs to be done.
		 */
		if (target_obj_priv->gtt_offset == reloc->presumed_offset) {
			drm_gem_object_unreference(target_obj);
			continue;
		}

		/* Check that the relocation address is valid... */
		if (reloc->offset > obj->size - 4) {
			DRM_ERROR("Relocation beyond object bounds: "
				  "obj %p target %d offset %d size %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset, (int) obj->size);
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}
		if (reloc->offset & 3) {
			DRM_ERROR("Relocation not 4-byte aligned: "
				  "obj %p target %d offset %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->offset);
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}

		/* and points to somewhere within the target object. */
		if (reloc->delta >= target_obj->size) {
			DRM_ERROR("Relocation beyond target object bounds: "
				  "obj %p target %d delta %d size %d.\n",
				  obj, reloc->target_handle,
				  (int) reloc->delta, (int) target_obj->size);
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}

		ret = i915_gem_object_set_to_gtt_domain(obj, 1);
		if (ret != 0) {
			drm_gem_object_unreference(target_obj);
			i915_gem_object_unpin(obj);
			return -EINVAL;
		}

		/* Map the page containing the relocation we're going to
		 * perform.
		 */
		reloc_offset = obj_priv->gtt_offset + reloc->offset;
		reloc_page = io_mapping_map_atomic_wc(dev_priv->mm.gtt_mapping,
						      (reloc_offset &
						       ~(PAGE_SIZE - 1)));
		reloc_entry = (uint32_t __iomem *)(reloc_page +
						   (reloc_offset & (PAGE_SIZE - 1)));
		reloc_val = target_obj_priv->gtt_offset + reloc->delta;

#if WATCH_BUF
		DRM_INFO("Applied relocation: %p@0x%08x %08x -> %08x\n",
			  obj, (unsigned int) reloc->offset,
			  readl(reloc_entry), reloc_val);
#endif
		writel(reloc_val, reloc_entry);
		io_mapping_unmap_atomic(reloc_page);

		/* The updated presumed offset for this entry will be
		 * copied back out to the user.
		 */
		reloc->presumed_offset = target_obj_priv->gtt_offset;

		drm_gem_object_unreference(target_obj);
	}

#if WATCH_BUF
	if (0)
		i915_gem_dump_object(obj, 128, __func__, ~0);
#endif
	return 0;
}

/** Dispatch a batchbuffer to the ring
 */
static int
i915_dispatch_gem_execbuffer(struct drm_device *dev,
			      struct drm_i915_gem_execbuffer *exec,
			      struct drm_clip_rect *cliprects,
			      uint64_t exec_offset)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int nbox = exec->num_cliprects;
	int i = 0, count;
	uint32_t exec_start, exec_len;
	RING_LOCALS;

	exec_start = (uint32_t) exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t) exec->batch_len;

	trace_i915_gem_request_submit(dev, dev_priv->mm.next_gem_seqno + 1);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			int ret = i915_emit_box(dev, cliprects, i,
						exec->DR1, exec->DR4);
			if (ret)
				return ret;
		}

		if (IS_I830(dev) || IS_845G(dev)) {
			BEGIN_LP_RING(4);
			OUT_RING(MI_BATCH_BUFFER);
			OUT_RING(exec_start | MI_BATCH_NON_SECURE);
			OUT_RING(exec_start + exec_len - 4);
			OUT_RING(0);
			ADVANCE_LP_RING();
		} else {
			BEGIN_LP_RING(2);
			if (IS_I965G(dev)) {
				OUT_RING(MI_BATCH_BUFFER_START |
					 (2 << 6) |
					 MI_BATCH_NON_SECURE_I965);
				OUT_RING(exec_start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START |
					 (2 << 6));
				OUT_RING(exec_start | MI_BATCH_NON_SECURE);
			}
			ADVANCE_LP_RING();
		}
	}

	/* XXX breadcrumb */
	return 0;
}

/* Throttle our rendering by waiting until the ring has completed our requests
 * emitted over 20 msec ago.
 *
 * Note that if we were to use the current jiffies each time around the loop,
 * we wouldn't escape the function with any frames outstanding if the time to
 * render a frame was over 20ms.
 *
 * This should get us reasonable parallelism between CPU and GPU but also
 * relatively low latency when blocking on a particular request to finish.
 */
static int
i915_gem_ring_throttle(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_i915_file_private *i915_file_priv = file_priv->driver_priv;
	int ret = 0;
	unsigned long recent_enough = jiffies - msecs_to_jiffies(20);

	mutex_lock(&dev->struct_mutex);
	while (!list_empty(&i915_file_priv->mm.request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&i915_file_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   client_list);

		if (time_after_eq(request->emitted_jiffies, recent_enough))
			break;

		ret = i915_wait_request(dev, request->seqno);
		if (ret != 0)
			break;
	}
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

static int
i915_gem_get_relocs_from_user(struct drm_i915_gem_exec_object *exec_list,
			      uint32_t buffer_count,
			      struct drm_i915_gem_relocation_entry **relocs)
{
	uint32_t reloc_count = 0, reloc_index = 0, i;
	int ret;

	*relocs = NULL;
	for (i = 0; i < buffer_count; i++) {
		if (reloc_count + exec_list[i].relocation_count < reloc_count)
			return -EINVAL;
		reloc_count += exec_list[i].relocation_count;
	}

	*relocs = drm_calloc_large(reloc_count, sizeof(**relocs));
	if (*relocs == NULL)
		return -ENOMEM;

	for (i = 0; i < buffer_count; i++) {
		struct drm_i915_gem_relocation_entry __user *user_relocs;

		user_relocs = (void __user *)(uintptr_t)exec_list[i].relocs_ptr;

		ret = copy_from_user(&(*relocs)[reloc_index],
				     user_relocs,
				     exec_list[i].relocation_count *
				     sizeof(**relocs));
		if (ret != 0) {
			drm_free_large(*relocs);
			*relocs = NULL;
			return -EFAULT;
		}

		reloc_index += exec_list[i].relocation_count;
	}

	return 0;
}

static int
i915_gem_put_relocs_to_user(struct drm_i915_gem_exec_object *exec_list,
			    uint32_t buffer_count,
			    struct drm_i915_gem_relocation_entry *relocs)
{
	uint32_t reloc_count = 0, i;
	int ret = 0;

	for (i = 0; i < buffer_count; i++) {
		struct drm_i915_gem_relocation_entry __user *user_relocs;
		int unwritten;

		user_relocs = (void __user *)(uintptr_t)exec_list[i].relocs_ptr;

		unwritten = copy_to_user(user_relocs,
					 &relocs[reloc_count],
					 exec_list[i].relocation_count *
					 sizeof(*relocs));

		if (unwritten) {
			ret = -EFAULT;
			goto err;
		}

		reloc_count += exec_list[i].relocation_count;
	}

err:
	drm_free_large(relocs);

	return ret;
}

static int
i915_gem_check_execbuffer (struct drm_i915_gem_execbuffer *exec,
			   uint64_t exec_offset)
{
	uint32_t exec_start, exec_len;

	exec_start = (uint32_t) exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t) exec->batch_len;

	if ((exec_start | exec_len) & 0x7)
		return -EINVAL;

	if (!exec_start)
		return -EINVAL;

	return 0;
}

int
i915_gem_execbuffer(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_execbuffer *args = data;
	struct drm_i915_gem_exec_object *exec_list = NULL;
	struct drm_gem_object **object_list = NULL;
	struct drm_gem_object *batch_obj;
	struct drm_i915_gem_object *obj_priv;
	struct drm_clip_rect *cliprects = NULL;
	struct drm_i915_gem_relocation_entry *relocs;
	int ret, ret2, i, pinned = 0;
	uint64_t exec_offset;
	uint32_t seqno, flush_domains, reloc_index;
	int pin_tries;

#if WATCH_EXEC
	DRM_INFO("buffers_ptr %d buffer_count %d len %08x\n",
		  (int) args->buffers_ptr, args->buffer_count, args->batch_len);
#endif

	if (args->buffer_count < 1) {
		DRM_ERROR("execbuf with %d buffers\n", args->buffer_count);
		return -EINVAL;
	}
	/* Copy in the exec list from userland */
	exec_list = drm_calloc_large(sizeof(*exec_list), args->buffer_count);
	object_list = drm_calloc_large(sizeof(*object_list), args->buffer_count);
	if (exec_list == NULL || object_list == NULL) {
		DRM_ERROR("Failed to allocate exec or object list "
			  "for %d buffers\n",
			  args->buffer_count);
		ret = -ENOMEM;
		goto pre_mutex_err;
	}
	ret = copy_from_user(exec_list,
			     (struct drm_i915_relocation_entry __user *)
			     (uintptr_t) args->buffers_ptr,
			     sizeof(*exec_list) * args->buffer_count);
	if (ret != 0) {
		DRM_ERROR("copy %d exec entries failed %d\n",
			  args->buffer_count, ret);
		goto pre_mutex_err;
	}

	if (args->num_cliprects != 0) {
		cliprects = kcalloc(args->num_cliprects, sizeof(*cliprects),
				    GFP_KERNEL);
		if (cliprects == NULL)
			goto pre_mutex_err;

		ret = copy_from_user(cliprects,
				     (struct drm_clip_rect __user *)
				     (uintptr_t) args->cliprects_ptr,
				     sizeof(*cliprects) * args->num_cliprects);
		if (ret != 0) {
			DRM_ERROR("copy %d cliprects failed: %d\n",
				  args->num_cliprects, ret);
			goto pre_mutex_err;
		}
	}

	ret = i915_gem_get_relocs_from_user(exec_list, args->buffer_count,
					    &relocs);
	if (ret != 0)
		goto pre_mutex_err;

	mutex_lock(&dev->struct_mutex);

	i915_verify_inactive(dev, __FILE__, __LINE__);

	if (atomic_read(&dev_priv->mm.wedged)) {
		DRM_ERROR("Execbuf while wedged\n");
		mutex_unlock(&dev->struct_mutex);
		ret = -EIO;
		goto pre_mutex_err;
	}

	if (dev_priv->mm.suspended) {
		DRM_ERROR("Execbuf while VT-switched.\n");
		mutex_unlock(&dev->struct_mutex);
		ret = -EBUSY;
		goto pre_mutex_err;
	}

	/* Look up object handles */
	for (i = 0; i < args->buffer_count; i++) {
		object_list[i] = drm_gem_object_lookup(dev, file_priv,
						       exec_list[i].handle);
		if (object_list[i] == NULL) {
			DRM_ERROR("Invalid object handle %d at index %d\n",
				   exec_list[i].handle, i);
			ret = -EBADF;
			goto err;
		}

		obj_priv = object_list[i]->driver_private;
		if (obj_priv->in_execbuffer) {
			DRM_ERROR("Object %p appears more than once in object list\n",
				   object_list[i]);
			ret = -EBADF;
			goto err;
		}
		obj_priv->in_execbuffer = true;
	}

	/* Pin and relocate */
	for (pin_tries = 0; ; pin_tries++) {
		ret = 0;
		reloc_index = 0;

		for (i = 0; i < args->buffer_count; i++) {
			object_list[i]->pending_read_domains = 0;
			object_list[i]->pending_write_domain = 0;
			ret = i915_gem_object_pin_and_relocate(object_list[i],
							       file_priv,
							       &exec_list[i],
							       &relocs[reloc_index]);
			if (ret)
				break;
			pinned = i + 1;
			reloc_index += exec_list[i].relocation_count;
		}
		/* success */
		if (ret == 0)
			break;

		/* error other than GTT full, or we've already tried again */
		if (ret != -ENOSPC || pin_tries >= 1) {
			if (ret != -ERESTARTSYS) {
				unsigned long long total_size = 0;
				for (i = 0; i < args->buffer_count; i++)
					total_size += object_list[i]->size;
				DRM_ERROR("Failed to pin buffer %d of %d, total %llu bytes: %d\n",
					  pinned+1, args->buffer_count,
					  total_size, ret);
				DRM_ERROR("%d objects [%d pinned], "
					  "%d object bytes [%d pinned], "
					  "%d/%d gtt bytes\n",
					  atomic_read(&dev->object_count),
					  atomic_read(&dev->pin_count),
					  atomic_read(&dev->object_memory),
					  atomic_read(&dev->pin_memory),
					  atomic_read(&dev->gtt_memory),
					  dev->gtt_total);
			}
			goto err;
		}

		/* unpin all of our buffers */
		for (i = 0; i < pinned; i++)
			i915_gem_object_unpin(object_list[i]);
		pinned = 0;

		/* evict everyone we can from the aperture */
		ret = i915_gem_evict_everything(dev);
		if (ret && ret != -ENOSPC)
			goto err;
	}

	/* Set the pending read domains for the batch buffer to COMMAND */
	batch_obj = object_list[args->buffer_count-1];
	if (batch_obj->pending_write_domain) {
		DRM_ERROR("Attempting to use self-modifying batch buffer\n");
		ret = -EINVAL;
		goto err;
	}
	batch_obj->pending_read_domains |= I915_GEM_DOMAIN_COMMAND;

	/* Sanity check the batch buffer, prior to moving objects */
	exec_offset = exec_list[args->buffer_count - 1].offset;
	ret = i915_gem_check_execbuffer (args, exec_offset);
	if (ret != 0) {
		DRM_ERROR("execbuf with invalid offset/length\n");
		goto err;
	}

	i915_verify_inactive(dev, __FILE__, __LINE__);

	/* Zero the global flush/invalidate flags. These
	 * will be modified as new domains are computed
	 * for each object
	 */
	dev->invalidate_domains = 0;
	dev->flush_domains = 0;

	for (i = 0; i < args->buffer_count; i++) {
		struct drm_gem_object *obj = object_list[i];

		/* Compute new gpu domains and update invalidate/flush */
		i915_gem_object_set_to_gpu_domain(obj);
	}

	i915_verify_inactive(dev, __FILE__, __LINE__);

	if (dev->invalidate_domains | dev->flush_domains) {
#if WATCH_EXEC
		DRM_INFO("%s: invalidate_domains %08x flush_domains %08x\n",
			  __func__,
			 dev->invalidate_domains,
			 dev->flush_domains);
#endif
		i915_gem_flush(dev,
			       dev->invalidate_domains,
			       dev->flush_domains);
		if (dev->flush_domains)
			(void)i915_add_request(dev, file_priv,
					       dev->flush_domains);
	}

	for (i = 0; i < args->buffer_count; i++) {
		struct drm_gem_object *obj = object_list[i];
		uint32_t old_write_domain = obj->write_domain;

		obj->write_domain = obj->pending_write_domain;
		trace_i915_gem_object_change_domain(obj,
						    obj->read_domains,
						    old_write_domain);
	}

	i915_verify_inactive(dev, __FILE__ *
 LINE__);

#if WATCH_COHERENCY
	for (i = 0; i < args->buffer_count; i++) {
	� 2008gem_object_check_coherency(are andlist[i],
	(the execion file.handleright #endifreby grantedEXEC© 2008oftwdumpware an(batchwares (th tion
rson obithoulenimitation
 __func__imitation
 ~0);n the Sof	/* Exec the rightbtaini */
	ret =  2008dispithouoftwSoftbtainiion
 *rson, cliprects, Softwoffsetrighif (rety of tDRM_ERROR("f the So failed %d\n", furn;
		goto erright ©/*
	 * Ensure thatlicencommands inlicense,
  * and/oarebove finished beforighte interrupt firesbove/
	flush * Copyscopies oretire_ and thiion
here� 2008 Intel Corporation
 *
 * Permission is heree above Get a seqno representingaragrSoftution ofotice urrentnotice ,bove which we can wait on.  We would like to mitigatparagsgraph) shals BUT NMERCly by only creaY KINWARRAs occasionally (soght nowe havcludin*some*TNESS FOR ANTY OF ANY KINcompleS OR
 * btainis SHALL
 *canBUT NOTHE WA when try KINDo clear up gtt space).ncludedWARRANopies oadd_requestion
 *file_priROM,in all copieondiBUG_ON( OR OTH= ish,charge, to any person obtaining a
 * copy of tstruct drmsoftware an *obj = mentation file"AS this software andmove_to_rporatit liFRINGEsh, y grantedLRUhed to INFO("%s: de " IN Soft on f %plowin modify, olt sh, distri in #include "i915ies of inclruion
 *
 odify,sh, distribu 2008 Intel Corporation
 *
 * Permission is hereerr:THE USE OR OTHER Dpinned* copynet>
 *
 */

#incluunpinumentation file "AS harge, to any person obtaining a
 * copy of thf ect_flush_gpu_wr of t	objT OF nholt <eric@anhol->driverT OF atendit_gtt_writ->inre, and to  = falsobj);}
		thors:
 *    E_unrefed doincluflush_gpu_writht ©mutex_unlock(&dev->*
 * A_nt i9f the
f (!furnished/* Copyaragrnewnotice (* Softs back IN ragruser's_drv.h"
#i.ludedell copcopydrmP int((*
 * Authorr substlocaS OR_entry _main_ *
statatio(uintptr_t)erson obtainis_ptG BUint64_Software" size);
ssizeof(*Software") *erson obtaining a
 ondit is furnishedell cop-EFAULTbj);
 to do so, o the fIN Aject%d_drv.hobj,ies "size);
"ite);
st			  (%d)lowisize);
rson obtaining a
 ing condit} * The aobject *obupdated em_object s out regardless
 * IED, INCerrorbove state.  Failyrighoj,
				taticem_obsIES OF meaperm noticenextbove timic intland calli915_gbuf, itIES OF do so with  OF umed   int _clear_fen int ididn't mion nragractualolt <erar_fencncludedret2THERWISEoftwputgem_obs_domain_rtatic voidem_object_bind_to_gt (the   ject *oware is fur2 !E ORished to do so, reg(t_wait_rend  unsigned aite);out:following c2 "AS  drm_gemARE ORject *obj) LIStatic pre_cpu_d_MAIN_Cthorfree_largm_object *objstat_lock);

int i9_set_to_fu;
	kk);
(ns to who "AS returnDEFI;
}

int
his software andobje*
 * Authors:
 *    Eric A,  off32_t alignment)
{
 *
 * Authordevice *dete_doma->dev;= end ||
	   his software anric A_write_domarm_gem_object *obj)ingnmet the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "Aoid i915tic voigtt_CONTRARE NULL15_gemll copies ooftware andbinddrmPgttnclude	if (startuct drm_gem_ject *o unsigned drm above Pre-965 chips need a f_gemnmenister setTIONin order tobove properly * to d t,
			surfacesv);
stat(struIS_I965Gns of &&olt tic voitiling_moderm_iI2008TILING_NONE= (uint32_t) (end - start);
get_ drm__regnclum_init_ioctlrm_i915_gem->struct_mut-EBUSYtrucn ret;
}
RESTARTSYS  uint_pwrite *args,
struct installt drm_;

statim_objectg condituct drm_device drm_g
static voipinng a
 ++"AS IS I* IMPL& (PAGEis nottrucive (str (!(pthe nguct in a BUT Nree "i9it fromaragrapdev->dron fret;

	mute drm_i915_gem_get_aARE 115_gematomic_incobject_gem_get_am_ini_availaadde = ->m_ob, size = (armemorym_init_io!t_mutex);
dev->dr&&
tation     awr/*
 * Copy &m_do_iGEM_GPU_DOMAINS)ARE O**
 * Crea!on f_empty(&t_mutex);
do_in(stru15_gedel_initate_ioctl(struct ice *d
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS ct drm_0ed lovoidend)
{
	drm_i915_m_obje*
 * Authors:
 *    Eric At >= end ||
	    (start & (PAGE_SIZE - 1)||
	    (ject *o_tt & (_write_dject_cate the *obj)) != 0 ||
	    (end & (PAGE_SIZE - 1)) != 0) {
		return -EINdrm_mm_init(&dev_priv->mm.gtt_space, start,
		     drm_i915_gem_get_a-- WITH THE  = dev->gtt_total;
	< OR THtex_lock(&dev->st
	dev->gtt_total =rture *args = data;

	if ( longerOMAIN_G,riveriinclu neithe (iev->drnor be_feares &	if tY, Wstick)
		oHER
 *n -ENODEV;

	args->aper_size = dev->gtt_total;
	arg915_gem));

	return 0;
}


/**
 * Creates a new mm object and returns a handle to it.drm_devicede "drailate_ioctl(struct size);
smic(bjectic voimm.NODEV;

_do_init(size -
	dee_size = (args->aper_size -
	sub     atomic_read(&dev->pin_memortruct drm_file *file_priv)
{
	struct drm_i915_gem_ long end)
{
	drgem_ioctlct *obj;
	in (start & (, data *data str_gem
 * Author, OU *, OUT OF t >= end ||
	   tomic(vaddr, *rson newata = drm_gem_objend & (PAGE_SIZ = drm_gem_object_alloc(dev, args->sizeINVAL;
	}

	drnt i91gem_object_set_to_cpu_domainc Anhorite_domain(strlookup* FROM, OUT OF ORrson o* to deal id i915t_total = (uin to do so, Badnit *argin i915_gem_pr, KM_USE)
			    strugem_obje* to deal int i915_gem_object_set_to_cpu_doma *file_pr-EBADF_user_gs->size);
	if (obj == NULL)
		retuize = dev->gtmadv_gem_do_iMADV_WILLNEED15_gem_pwrite *arAttm_crHER IN 7_swa purgeableLAIM, D\n" *dstrite_domain(struct drm_gem_objline int
slow_shmem_copy(struct page *dst_page,
		INVALight © ize = dev->gtt_tofilprm_ital truct_mutex);
addr == NULLtic int i9_vaddr, *src_vaddlreadyOMAIN_Griv->tiling_mode != I915_TILING_NONE;
}

static inline SER0);
	if (dst_vaddr == NULL)
		return -ENOMEM;

	src_vaddr = kmap_atomic(src_page, KM_USERt_mutex);
 intmode get_apert_priv, obj, &ha == Nr, KM_USER0   end - start);
nline int
slow	args->apert32_t) (end - start);
object_>mm.bit_t
i915_gem_init_ioctl_mutex);

	rSER0);
	if (dst_vaddr == NULL)
		re int
slow_shmem_copy(struct page *dst*file_priv)
{
	strucbute,XXX -= handc inlCPU caches harg	memcpyobject
		retaAMAGe X server doesactivenage l copiesy915_ge

	m(end - start);
 IN COcpu_new mm objecULL)
		rrson o* Softnholt t_handle_un* Soft, PAGE_);
	if (dst_vaddr == NULL)
		rnt i915_gem_object_set_to_cpu_domaineate *args = ng end)
{
	drm_obj KM_USER0);

	if (unwritten)
		return -EFAULT;



	return 0;
}

static int i915_gem_object_needs_bit17_swizzle(struct drm_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->dev->dev_struct drm_i915_gem_object *obj_priv = obj->driver_private;

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		obj_priv->tiling_m= kmap_atomi5_TILING_NONE;
}

static inline int
slow_shmem_copy(struct page *dst_page,
		int dst_ooffset,
		struct page *src_page,
	R1);
	if (src_vaddr == NULL KM_USER0);
		return -ENOMN>drimemcpybyruct ery(dst_vaddr + dst_offset, src_vaddr + src_offset, length);

	kunmap_atomic(src_vaddr, KM_USER1);
	kunmap_atomic(dst_vaddr, KM_USER0);

	return 0;}

static inline int
slow;
	mue,
		      int gpu_offset,
		   915_gemm_bit17_copy(struct ptal uct dic void i915_gem_object_static islow_shmem_copy(gpu_page, gpu_offset,
					       cpu_page, cpu_offst, length);
	}

	gpu_vaddr busyp_atomic(gpu_page, KM_USER0);
	if (gpu_vaddr == N	return 0;
}

static int i915_gem_object_needs_bit1
	rege, KM_USER1);
	if (cpu_vaddr == NULL) {
		kunmap_atomic(gpu_vaddr, KM_USER0);
		retu = obj->driver_private;

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		obj_priv->tiling_m
	return 0;5_TILING_NONE;
}

static inline this_length = min(caruct drm_i915_gem_object *obj_prre *Urm_gem_objDEV;

	argsys(gp= 0)hardware's void i91posiS ORv);
s O retwisct_fisPOSE A,
				main a delayt_waim    rITY, Wirqs(includinruct d;
	lnmask	if (retour workNINFRIt ethisupandle =int i_objTHER
 *ISINired ret;

	m(end - sstantiaISING
 ons of theSIZE - 1)) != 0) {
		return -EINV/* Doactiet,
		ndle =opermisres &r_dat15_gagopienotice& (PAGEndle bove donnce_;
	ssize_, anotice (lefABILIobject_get_pages(obutr->drgetdst_ludinghandle (becausgs->body'sect_get_pa_from_ Copyr w);

	eif (ct drmbove unom th(strgegnmeusd) {
	libdrm's boe_to_p.  T(ret ret;expectedbove conatic    t remaaph) 
	in, OpenGL'MENT.lu IN  quet drint
so spec
		retint i915_ & (1 <j->dr_priv ="even
	int "size)lignanyration rm_gemv);
statrson obtsylength);
		el
}


/**
ngth);
		ellasys_pndert = WARRANm_i9retu -= this_length;
	}

	kunmap_atomic(cpu_vaddr, KM_USER1);
	kunmap_atomic(gpu_vaddr, KM_USER0);
throttle KM_USER0);

	if (unwritten)
		return -EFAULT drm_gem_obj}

static int i915_= NUct drm_ args->datt = > PAGE_S* FROM, OUT OF 
	kunmap_atomic(vad
		iisSIZE)
			page_length = PAGE_SIZE - page_offsettomic(p	return 0;
}

static int i915_gem_object_needs_bit1 page_lge, KM_USER1);
	if (cpu_vaddr == NULL) {
		kunmap_atomic(gpu_vaddr, KM_USER0);
		retuswion n(rson o
		i);
		caselength)
{
	DONT *ds:utex);

	return rchar *ds:
		rembreak, PAefaultm_objecUSER0);

	return 0;
}

sobj->driver_private;

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		obj_priv->tiling_m page_length);rm_gem_object *obj,
			  struct drm_i915_gem_pread *args,
			  struct drm_file *file_fset,
		struct page *src_page,
		int src_offset, (args->ap of tSER0);
	if (dst_vaddr == NULL)
		return -ENOMEM;

	src_vaddr = kmap_aaddr, *src_vaddr;

	dcpy(gem_object_set_page_gfpoffsetu_page) & (1 ge, KM_Umic(src_page, KM_USER1);
	if (src_v
		int l__ength)
{
	PURGED(str* to make some =mm.bit_
		irture *i_unlock(&dev->struct_mutexbound, discard i write)NINFRtor	ret;

	muteu_offset += thisis_ap_atomicck(&dev->stru_objec- start);

	dev->gtt_total =net>
 *
 */

#inclutruncatNULL)
		 bytes tretaimcpyength);
		elsome space by throwing out-1));
		page_offset = offset & (PAGE_SIZE-1);
		page_length = remain;
		tomic(gpu_vaddr, Kpages(obj)v, vcluding t *obj;
	int ret;
	u32 handle;

	args->sizce.  On a
 * fault, it fails so _write_dkzalloc(m_objectbj->size), GFP_KERNEj);
offset,
			  
		gfp = i91_page,
		NOMEM"AS IS", WIWe've jus(oborag				 pagphyseturn -Ekernel BUT NNT SHeyopy out obeen new ten {
	f ((pageize)bove zeros.>offy'll  struto be clffset,
	t
 * pawcludin	argthemsize);= 0)GPUv);
states a new mm objectem_do_iturnhandle_CPUf_path		i9adll copies o  struct drm_i915_gemet,
			     agp_type = AGP_USER_MEMORY_priv)
rm_gem_object *o_object_sethmem_bit17_coc Anholt hmem_bit17_comutex_unl struct FENCE_REev, ar;
	INIT_LIST_HEADoid *data,
		      sremain;
	loff_t offset, pimutex_do_init(e old buffers.
	 ength)
{
	char *ds_pritracemem pread path, _ND NOfp_mask(obeate *args = datapages(obj)k);



	return ret;
}

/**
 * This is the fallback shm (start & (PAGE_SIZE - 1)) != 0 ||
	    (end & (PAGE_SIZE - 1)) != 0) {
		return -EINdex, shmem_page_offset;
destrocumenect_swh

stck(&dev->struct_mutex>em *vau_offset += this_length;
		lh);
		int swizzlehyspage i915_gem_objdetach_he user ing n)
		le
	 * de	struct drm_gem_o
	reile
	 * dereferencing tmmap * Softwet we want tk);

ze - 1) / Plt while
gned l}

static iaget,
		validunsigned lgth);
		elbit_17pages = drm_crm_gem_object *o
	kunm/** U_pagesof tENODEV;

	 & (1 <ject_r_feicratiend)
{
	dr(stat_acki Corporat lastER0);

	if (unwritten)
t >= AGE_SIZE);

	/* Allocate the new object */
	obj =e
	 * hol915_gem_creatges[page_base >> PAGE_SHIFTARE.
 *
 * Authors:
 *    Eric Auct dL;
	}

	dr so we 15_gefir_gembj,
, NULL);
	up_read(&mm->mmap_kmap_atomic(pallback shmem pread path, kmap_atomic(pdo_inct *met = t32_t) (end - start);
a_page = (dat	return ret;
ex);

	r to do so, E_gempageier_feage = d;

static LIed path if this page isn't , length);
	}

	gpu_vaddr idlees = get_user_pages(current, mm, (uintptr_t)args->data_ptr,
				      num_pag	rivate;

WARRA, voi~(PAGE, e = (WARRAINVAL;
stucking creturn -ENOMEM;
	}

	/* Copy the data,1);
ges[page_basesusivered ||ew otic voi   p.   pa I915_BIT_6_SWIZZint
slow_shmem_copy(struct page *dst_page,
0
 * The a Hack! ex);

	lHOUTny>sizt mim_devicNOT low(sail_

	rentrolotice hiprivateW5_ge
i915_repl>gtt remaize);T WAmaphore,	chaRS Otet_pv);
statge
		 *
		 * shmem_page= 1_pagel_ase;
, NULL);
	uphang assocase;
);
	muteCanceeturn
stantilengtnit *arr,* LIABhargit915_g the ENOMrunn args->/mic(cpu_vaddr, KM_USER1);
	kunmap_atocSIZE;e *dage__engt_syle_size[page_basestantiaengtoffset,
		gem_object_set_to_cpu_domain(2008ges wh_lost_agesexdata_);
	muteF*/
	if ((GPU at_muth datac inon-ke paew mturn slomem_copy(cpu_pagffsetata_ptnd returns a handle  page_length) > PAGE_S_gem OR OTHERWISE, ARISING
 * FROMtal IZE)
			page_length = PAGhis paSOFTWARE ORile
		 * shmem_page_offset = offset within page utex, so ength ta_ptr & ~PA LIAt = oftw OR OTHE= args->fset = arg to a= druck						 harge;;bit17_rivate;
	_t) (end -nd);ser_pag > PAGEt pagege_ofWARRA_passed(rivate;
	of "drm.(strut_get_pais pae_index],
		=private;
	x);

	retur   da++ > 10ex);

	rr_evict(obj)t *obj_p wedgellowed pat		return et, NULL);
	up_re,
				, 1    shm to WAKEUP, NULL);
	upirq_queunline ata_page_incpu_wpu_wmsleep(1OR THge_index],
			rivate;
	_user_age_offset,
						    user_pages[E-1)) args->data_ptr;
	remain = args->skmapgem_objec[page_base	pinned_pagl_unlo_user i!_availa *arfset,
					      pagesem);
	/* Aev->driverct_get_pashS OF now_gemm_cre)) =wcopy

		 * LIAed& ~PAT WARudrm_fhigret;har page
iver_feae, and to 		pagt_seWARNTHE  user_pages, NULL);
	up_re(&mm->mmap_se	datreturn ret;
}

/**
 * Reads data fct_get_pe object re/* RSING
 SetPageDirty(user_pages[i]);
 = pag_cache		pageem_objece = er *)e un permisargs->es);

	return ret;
}

/**
 * Reads data fISING
 e object rn't affEr_pagtruct drm_is; i++) {
		Sges(s*dev, 

	arg. *args =_prir_datany	 * pm_obje notiis poi_gttittaticAMAGES OR'ex],
				rm_g		retuo_priv;good's goHER IN happand noleav KIND, urn -rm_i9So915_iptatic inllengl copies(strout ostuf* IMPm on	struct drm_dex = d, 1, 0, user_pages, NULL);
	up_re(&mm->mmap_sem);
	if (pinned_pages < num_pages) rivate;

old				       gp = -EFAULT;
		goto fail_put_user_pages;
	}	do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	mutex_loc		ze > obj->size |)) != 0) obj->size ||
-EFAUa new mm object = ~nd returns a handle 		gpu_offset += thisde "drmPCorporatit while
ex, shmem_page_offset;
	e_in      gpu_pas (the "f (ret)ad *args,
			 ;
		if (ret !e > obj->size |   stru
fail5_gem_objec	mutex_unlock(&dev->struct_mu This could use review for overflow, the contents */
	if (args->offset > obj->size || args->size > obj->size ||
	    args->offset + args->size > obj->si, the content_gem_object_unreference(obj);
		return -EINVAL;
	}

	if (i915_gem_object_needs_bit17_swizzle(obj)) {
		ret = i915_gem_shmem_pread_slow(dev, obj, args, file_priv);
	} else {
		ret = i915_gem_shmem_pread_fast(dev, obj, args, file_priv);
		if (ret != 0)
			ret = i915_gem_shmem_pread_slow(dev, obj, 't affM "i9es == NULL)
	AIM, DAMlign * IMPLGTTject_st32_t) (end - s->mmap_sem);
	pinned_page;
		} ereturn ret;
}

/**
 * Reads data fad(&mm->mmap_seare is furnishedint
slow_shmem_copy(struct page *dst_page,
device *s:
	i915_geAN Anup    pnd to permoffset,
					       cpu_page, cpu_offset, length);
	}MEM;

	down_read(&mm);
	}hws->offset,
							args->size);
	if (ret != 0)
		goto fail_put_pages;

	obj_privm_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->dev->dev_private;
	ste *argwage_offa he uical addrstatem_objecr_feusect's drm'ages;
	}
bove v, vializ);
	Authem_o loa_base; ret;

	mutex_2008 *ds_GFX_HWSLT;
	he struct_mE-1))*obj)
{
	return mappitoragen)
		4096zle_x == I915_BIT_6_SWIZZLE_9_10_17 gs,
				stf the obser_inatomicy to map in the p	    shmem>size;

	mutex_lock(&dev->struct_mv)
{
	struct drm_i915_gem_objeCACHEDect *obj_prct page *cpu_page,
		      int apping_unmaplength,
		    SER0);
	if (dst_vaddr == NULL)
		renel_write(struct ile_priv);er_ina_gfx				bjecgth);
		else
			returnges[page_bashw_e >> PAe + ary mapst_data_page + s[0
stats page
		 *
		eturn -ENOMEM;
	ic(dst_vaddr);
	if (unwritten)
		mapser_inatomic.	      smem_offset,
					 hws_map, 0,em_objec
		return -EFAULTect reu_offset += this_length;
		le
	unsigned long unwritten;

	vaddr = kmap_aed_gpu_offset
		return -EFA *mm = currewrittentomic(vaddr + page_offse;
	rePAGE_SIZE_page2008WRITE(HWS_PGA,ndex = pagee >> PAGE_SHIFTtruct drmRff_tbject *); /*>dridst_v *ars);

 to DEBUG("hwaticSoft: 0x%08xlowinj,
			 struct drm_i915_gem_ppage *user_page, int udata;
	struct g *mappit length)
{
	char *src_vaddr, *dst_vaddr;
	unsigned long unwritten;

	dst_vaddr = io_mapping_map_atomic_wc(mapping, gtt_base);
	src_vaddr = kmap_atoer_inatomic(vaddr atic ine of the struct__priv)
 new otr_t) args->daf_path.dentry->d_inode->i_mapping, gfkage_itten = __copy_from_user__offset += this_length;
		leSER0);
	if (dst_vaddr == NULL)
		r;
	if (!access_okength;
		5_gem_gtt)
		return -EFAULT;
	return 0;
}

/**
 * This is the NULL)
		return -ENOMEM;
	u(&dev->s/* Wage_oges[ = __copyiurcebject *ITY, Wdisabet =ject_st drm_gem_object *ob0x1ffff00ish,main_range(obj, a;
	}ng,
		  lofngth)
{
	char *src_vaddr, *dst_vaddr;
	unsigned long unwritten;

	dst_vaddr = io_mapping_map_atomic_wc(mapping, gtt_base);
	src_vaddr = kmap_atomuct drm_ge conbtainint *ge
	 =pages[page_bge
	INVAL;
	}

		u32 headivate;
_t) (end - s	  int lef_t gttint length,
	r = kmap_atomicatomic(src_vaddr, KM_USER1);
	io_m128 * 1024ng_unmap_atomic(dst_vaddr);
	if (unwritten)
		return -Eng,
		  lo	      sev_priv = dev->dev_pr;
		} el

static inline int
fast_shmem_write(struct page **p char __user *data,
		 int length)
{
	char __iomem *vaddr;
	unsigned long unwritten;

	vaddr =tt_mapping, page_base,
				       pate(struct /* Spriv)
g pages whaddrple = m_objec numbor seling->Sizrivate; atomif we  -= pmap.fset, lenject_agpe rise +ngth);
		else
			return ata += pagm_oblength;
		user15_gem_objerm_i915			 ata += pagflale(sttruct_mutex);
mtrbjecE-1));
		corSIZErage__wc(&ata += pat *obep for
 *ta += pagit *arg, data, length);
	kunmap_atomic(vaddr,ng,
		  lo

	if (unwritten)
		return user;
	return 0;
}

/**
 *users the fast pwrite path, where we copy the data directly from the
 * usett_mapping, page_base,
				       page_gpu_offsetuser_per within = curreuser_pvirct durn ric DEser_pages to pi
		gotoSto		 */
_baseifnocachpage_inet = obj_priv->gtPRB0_CTL, OR THruct drm_file *fTAIe_priv)
{
	struct drm_i9off__priv)age, K gtt_offs fail;

		remairuct drm_file *fpertuptr /);
		else
			retuep fge_oges;
	in*argsv = obj->) & off__ADDRer_privG4515_gem_ gtt_offject to th;
	stOF Atage_o915_s.
 	ret = i9off_tm_i915_gem_pwrite *arRbaseoff_t (!(es;
	l pinned__gem_  "ctl m_obstruct	int nwripage_o drm_gm_objectrrent- gtt_page_base,CTL), data_page_index, pag offs, data_page_index, pag15_gdata_ptr = args->data_ppertuect rect *obj_priv = obj->driver_p **user_pages;
	structforc
i915_m = current->mm;
	int gtt_page_offset, data_page_offset, data_page_index, page_length;
	int ret;
	uint64_t data_ptr = args->data_ptr;

	remain = args->size;

	/* Pin tuct ruct drm_file *file_

		re(     atomi -h)
{
	 & R(dev,R_e *dS) |

		reIZE;
	O_REPORTs = last_dataVALIDver_p	loff_t gtt_page_base, offset;
	loff_t first_dargs = off_tisturnZE - t hold;

	r15_gem_s d*file_prs, i;
	struct page **user_pages;
	s last_data_page, nuedcurrent->mm;
	int gtt_page_offset, data_page_offset, data_page_index, page_length;
	int ret;
	uint64_t data_ptr = args->data_ptr;

	remain = args->size;

	/* Pin thr into theOad *args =v)
{
	sge_l_to_pmap_atom_base_evict;

	mutexe fallbac assocfeatur, page_DRIVbjecODESETngth)ge_offset + page_length) > PAGE	else struc -= p	loff_t gtt_page_base, offset;
	loff_t fi_object_ffset_t gtt_page_base,tr;

 & 15_gt)
		goto out_uv->gtt_m_objecoff_t-t_user_pffset+ 8_gem_objece;
	offset );
	 pathe;
	offset += obj_prage_
	last_eate *args = data;
	struct g *mapping,
		  lofs = get_user_pages(current, mm, (uintptr_t)args->data_ptr,
				      num_pagess page
		 *
		 number within shmem fremain = argse fallback GTT pned lThis code resul. which usesk(&dev->struct_mutex);
	dex = page number withi = i915_gem_object_pin(obj, 0);ytes to copy for this page
	ex = page number within gth;
		g
 *
 * This code resulted in x11perf -rgb10text const io_mapping *mappibase,
				
	}

	gpu_vaddr eph) vtp_atomic(gpu_page, KM_USER0);
	if (gpu_vaddr == NUin -= page_length;
		user_data +=AGE_SIZE);

	/* Allocate the new object */
	obj = AL;
	}

	drm pagx);
	ret = i915_gem_object_pin(obj, 0);
	if (r;
	kunmap_at+ pafail_put_user_pages:
	for (i = 0; i < p**user_pageseen
	offsp(dev, ft *obj_p,>hand luck	      sem_page_offset,
					      page_lOR THad *args,
			  struct drm_file *file_th = bytes to copy for thE-1))t = offset & (PAGE_Sng,
		  loff_t gttint length,
		    ults
 */

static inline int
slow_kernel_write(struct 
fail_unlock:
	mutex_unlock(&dev->struct_mutTH THE et;
}

/**
 * Reads data from the object rrgs,
							file_priv);
	}

	drm_gem_object_unrto out_unpin_object;

		remain -=, the contents of to out_unpin_object;

		remain -= which can sleep fto out_unpin_object;

		remain -=m_i915_gem_pread nt i915_gem_object_set_to_cpu_domain shmerq_, void e,
				_cpu_read_domain_range(obj, 
		re
		page_length = remain;
		if ((gtt_page_offset + page_length) > PAGE_SIZE)
			page_l+ page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - data_p);
	drm_unfree_large(useSIZE - dnge(obj, args-t = data_data;
	struct e = closs->offset,
							args->size)age_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - ffset = offset & (PA struct drit_ioctl(str i915_gem_object_waitrivett_page_bl_unlock;

	retm_gem_object *obj,oadtruct drm_i915_gem_pwrite *args,i, PAGE_SIZE);

	/* Allocate the new object */
	obj =retry in thev, voik:
	mutex_unlock(&dev->struct_mutremain;
	loff_t ile_priv);
	}

	drm_gem_truct_mutex);

	ret = i915_gem_ob, the contentsruct_mutex);

	ret = i915_gem_ob >> PAGE_SHIFT], t_mutex);

	ret = i915_gem_obm_i915_gem_pres(obj);
	if (ret != 0)
		goto faage, last_datremaiDELAYED_WORK data_ptr & ~PAGE_MASK;

	, data_ args->data_ptr;_offsset = oength);

		/* If gem_  user_pages[1a_ptr;
	remai(&shrinkev->struct_mutv->st			  = i915_gem_obre
		 * pag, ure
		 * pag;
		offset += pagre
		 * page_offsetlow pOld X						 ith ll take 0-2goto fro_gttite)t *opt notice s_length = bytesmutex_unlt drm_gem3r *) (ui_lock(&dev->st|| _lock4et + remain) > PAGEM + remain) >G33  length)_offset = num	mutex_unle(st16= i915_e_offset;

		ret = fast_shmem_8er_private;
	drm_i drm_file *fil;
	stnned_pages, i_lock(&dev->sfast wharge, to any pe16TT))

sta_data_page 64(_pages;
	s965_0 +ge, * 8)dex],
		 915_gem_oharge, to any pe8;

		remain -= page_ngth;
		use830ata += pag4_length;length);
	AGE_SIZE)
			page_length = PAGE_SIZE - page_o+= page_length;
	}

fail_putt_pages:
	i915_gem_obj945_8ut_pages(obj);
fauct io_mappindett);

	t_6_swizzstruct drm_g/*
 * Cint d
	unwrittenE ANontiguous >pin_met != 0)oto faemaie = dng ke.g.king cursorage_ving(t drgs
struask (obj, gfp);
	}ta_page = daR0);

	if (unwritten)
	mitation
 ask (d,ite_sm_obdr, *dst_vaddr;
	unsigned long unwritten;

	dst_vaddr = io_mappingtomic(vadda_page = d *e *args,a_page_offset + pagle_priv);
	}e *args,s[i>gtt1]main!ct drm_SIZE - data_pe *args,ary storage
 * in t drm_i915_gem_pwrite *args,
		ace to copy_to_user i!he user datatruct_mutex, so wee *args,->iff_tioffsepinned_pag to pin
j->drpciSER1);
	io_mtomic_0t + rgs- shmages;
	ssize_t retatic inl struct pagc inline itions:gned ruct drinteldef CONFIG_X86GE_St->pin_mwrit(unsigmcpyt_mu)dex, shmem_page_->vHIFT,unwrir = args->datae - 1/ce *dev, stru distribuiv)
{
	struct drm_i915_gem_obje=	int do_bser_pages);

	,  data_p:e - firshe user duct drm_deigned lopage_offset;
	int pe *args,
		ER0);

	if (unwritten)
		te_slorm_device *dev, struct drm_gem_object *obj,
			   struct drm_i915_gem_pwrite *args,
			   struct duct_mutev)
{
	struct drm_i915_gem_obj return
		 * der_private; args->size - 1) / PAGE_SIZE;
vate;
	dex, shmemrivas pa of this softwirst_data_page = data_pts = drm_calloc_lar_user_ffset;
	int page_length;
	int rbt;
	uint64_t data_ptr = args->data_ptr;
	int do_bit17_swizzling;

	remain = args->sizm_shmst_dned l*));
	if (user_* to deal 
	 * holding the s

	/* Pin the user pages containith;
		 and all of the pwritealle implementations
	 * want to hold int ret;

	ue_domain(st  struct PHYS_CURSOR_ any pes;
	int Xject_nOBJECTTT))

static void write implementatld it er *user_d) {
		/* Oirst_data_page = da*/
static int
i915_gem_shme	915_gem_obje/**
 * This is the fallback shmem pread path, which allocaret;

	usSIZE-1));
{
		e + 1; a
 *(cacheline_end - gpu_offset, length);
		
	return 0;ize_t remain;
	loff we get a fault while cop,
		ct's i915_gemt_ioctl(strions:oudrivev = obj->dlength;
		us;

	remain =izzling = i91_GEM_DOM = obj->dr copy of tchar *ds numnwri__availtten = __copy_froi], KMm_obj);
faiage insrc], KM_USER0);a_ptr = args->data_ptr;a += page *dev, stru(unwricpy(dengtsrcice *dev, strucreturn e
		 * dge.
	e number in ncac falshmem_in this pa = __copy_fr,fset = offs = i915_ct d
		  ;
	moffset + page.
		 * page_lengphysn this page
iv);mem_bit17_cop = drm_calloc_laE_MASK;
		E_SIZE - first_dates < num_pagdown_read(&mmatict(obj);
	if (ret)
		goto fail_unlock;

	repage_length) > priv = dev->dev_p while dereferencing the user data.
	 */
	first_data_page = data_ptr / PAGE_SIZE;v = obj->dev->dev_private;urn ret_priv = obj->drit17_swizzlse {
d >zzle(obj);

	mutex_lopriv = obj-gem_gtt_p_path.dentry->d_inode->i_mapping, gfp);
}

static ie user dic void i915eturn
		 * data_pes,  i;
(struct drm_			 a.
	 */
	first_data_page = data_ptr / PA    user_	int d abj,
	gs,
			 ruct_muteargs->size - 1) / PAGE_SIZE;
	 struct page *cpu_pa_mutex is held.
 ld it ds (the "gth;
		usm_init_ioctl(ges_or_evict(obj)off_t offsenit
	if );
		} e%dnum_p: %zulowinlow(et,
					     hmem_page_it drm_gem_obet)
;
static;
		} else _offset = data_ptr & ge - first_data_page + 1;

	user_et,
						    user_ppage;
		dae data.  W> 0) {
		/* Operation in this page
		 *
		 *(dev_priv->mm.gtbject_wait->drMEM;
ges(	      s		if (ret)
uct ex = page number within shmem file
		 * shmem_page_offset = offset within page inr_page file
		 * data_page_index = page number in get_use shmemet,
						    user_page_offset = offset with data_page_index page.
		 * page_length = bytes to copy f	 * pis page
		 */
MASK;
		data_page_index = data_ptr
		 *
		 * g / PAG    unsigned loMEM;

	down_read(&mm   shppage_ER0);

	if (unwritten)
		ate_t *dev_priv = dev->dev_dr == NULL)
		retur_priv)
{
	page_oizzlect *obj;
	struct drm}

static int i915_gem_object_needs_bit1& (PAGE_SIZE - 1)) != 0) {
		return -EINVreturnet,
HIFT_pages;

	objage i					   nlineruct d
	private;
 = ( obj->driver_)_t offset,
						  ruct64_t += pagHIFT], KM_USER0);y handle.
 *
 * On error, gpu_offset, 
	/*iv)
{
	struview for %p, %llllowinview for>mm.bit_				    t_cpu_read__sem)ain_r args->offprivate;
ffset + args->sit_ioctl(str= slow_shstatic _page_offset = offset & ~PAGE__index,  data_page_offset;
	relea struct drm_i915_gem_pta_p= data;
	stru}

static int i915_gem_object_needs_, OUT OF v->stferent detilint page *gpu_rm_gem_object / PAGE_ *ma

	uge_lISING
 ages(oTY, WHice li INCie);
	if away,oldinga15_gemlafileta_ptr;
	remaino fail_det drm_gemrrentsoon-to-be-goncluding OUT OF * XXX: Targs,
			  struct drm_file *file_, 1, 0, user_pages, ferent detilin i < pinned_pages; datavice *dev, vo if (obj_priv->tiling_mode == Ie_bas i++)
		page_cache_release(user_pages[drm_file *file_priv)
{
re
		 ({
		nrdrmPscan, gfp_t i915e_ofage_length = PAGE_SIZE - gtt_page, *_baseE - 1)) != 0 ||
	    (end & (PAGE_SIZE - 1       fuct drm_fice numE_SIZE -ES OF_		regem_ffset wi/* "fast-path"wait_rge nnumb{
		/*availomic(rture
		lse {
		 {
			ret hmem_bit17_thin aperture
		 * page_offset TILINGfor_et_dal_put_,
						  age
		 */
		, 
		 * page_lenages[datr = args->data_ptr;
	intret = slowE - 1

	returnt i91trygem_object_set_to_cpu_doriv->pagif (ret == -EFAULT) m_object_
		if (ret user_pages;
	}

	do_bit17_swizzlige_lenlow(d (the cow_shme *cpu_vaddr;

	/* Use the unswizzled pat -= pagey for this page
		 */
		page_base VAL;
	}
(le(o/bj_pri* sysctl_vfs__to_p_ OF pyri		 * retry in the ge
		 */
		page_base = (to fa hmem_g on rite (PAGE_SIZE-1if (ret == -EFAULT)_safffset & PA,_gem_file i915_g i915_gem_shmem_pwrite_slow(dev, oj, args,
							 file_priv);
		}
	}

#if WA(stru WRITE
	if (ret)
		DRM_INFO("pwrite spang.
 nuta_ptther
 * through the mmap ioctl's map args->data_ptr;
	remain = args->priv)
{
	struct drm_i915_pm_object_neds_bit1 struct reference(obj);

	return ret;
}

/* Call);

	retur15_gem_evict_something(dev, obj->size)riv->pag_SIZE;
	last_data_page = (_struct *m_pages;+ pa--_gem_shmem_<mem *vads;

		remain -= pafast(dev, obj, args, file_priv);
		gem_object *obj)
{
	int ret;

	ret = et = i915_gem_shmE-1)) i915_gem_shmem_EINVAL;

t_get_pan't affseconjectsom t>mma/, argsobj_priv;);
	ifct_set_NODEV;

	argsile_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_set_domain *args = data;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	uint32_t read_domains = args->read_domainsrite_domain;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	/* Onl_gem_shmem_x, yby the CPU. */
	if (write_domain & I915_GEM_GPU_DO {
			ret ;
	mu	;
		off spacn user sps & I915_GEM_GPU_DOMAINS)
		return -EINVAL;

	/* Having something in the write domain implies  * retry i this page
		 */
		page_base + paet = i915_gem_ret = slow_sis p915_g+ pale(ox, yet ping or a GTT mapping.
 */
int
i915_gem_set_domaiIN_GTT)ge *user_page, int u	returnre
		 ile_ist);
		= of 	 * pag_t) (end - s}

		/,ntlyeeke(stD	/* We_SEEKS,
};

_ndex]e_t *dev_priv =}

		/* ev, vodataobj_priv->e *file the clit drm_depage
}
		 exto success, since the clieas das just askinuct us to
		 * make sure everythin