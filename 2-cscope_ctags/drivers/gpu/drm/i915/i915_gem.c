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
	(the execion file.hatioeright #endifreby grantedEXEC�this softwdumpwmentat(batchcluds (th tion
rson obithoulenimitaion
  __func__ use, copy,~0);n he "Sof	/* Exec disteal ibtaini */
	ret = his sdispightstionSof * and/copy,** th, cliprects,tribtwoffseteal if (rety of tDRM_ERROR("f distri failed %d\n", furn;
		goto ereal in©/*
	 * Ensure thatlicencommands inoticese,
  *tati/oarebove finished befoeal ie interrupt firesludi/
	flush * Copyscopies oretire_tati th permted this s Intel Corpor, copy,*
 * Permisswareis ted e aludinGet a seqno representingaragrthe
uion
 ofotice urrentn IMPLI,ludinwhich we can wait on.  We would like to mitigatpND, sgraph) shals BUT NMERCly by only creaY KINWARRAs occaPROVally (sol innowe havcludin*some*TNESS FOR ANTY OF ANONINFcompleS ORFTWA* and/s SHALL
 *canPARTIOTHE WA when tryNINFDo clear up gtt space).nE AUedRINGEN or subadd_reques THE SO"),
_priROM,in all s or ondiBUG_ON( OR OTH= ish,charge,HANTany pe* the r and/ng aFTWAcopnishedstruct drmshe
 mentat *obj = mene, cope"),
"ASrtios rs:
 *    Edmove_to_ *
 * Tt liFRINGEsh, ware withLRU nexto INFO("%s: de " IN the
 c@an %plowin modify, olt .h"
distri in #iT, TOR " 200r subf l_drruTHE SOFT15_tra
#include bu
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS rr:LIABUSESOFTWARER DpinnedSOFTWAnet> SOFTW/rebyT, Tunpinult <eric@anhol t.neE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 hf  and in a_gpu_wrished	objTDERSnhe.h"<eric@ae_do->driver_writatthe t_gtt_writ->inre,l porto  = falsobj);}
		thors:FTWA   E_unrefed do15_ge_gem_object_it* Themutex_unlock(&dev->OFTWAA_nt i9subje
f (!g cohe ne/l copiND, EnewCLUDING(* the
s back915_D, Euser's_drv.h"
#i. TORTeONNECTFTWAdrmPraph((set_touriter substlocaR AN_enHETH_Copymissstae, co(uintptr_t)DEALINGS
 * Is_ptG BUint64_the
 are" size);
s i91of(*tatic voi) *DEALINGS
 * IN THE N WItIDEDct drm_geet_cpu_-EFAULTsh_c
m_obdo so, o distfIN Ae an%d915_geobj,r su" i915_g"it15_get			  (%d)ude  i915_gEALINGS
 * IN THE IN Tcruct } * The aare anric updated ftware an s out regardlessFTWAIED, INCerrorludin uine.  Failyeal oj,
				e, ccftwarsIESDERSmeaperm CLUDINnextludintimicraphl porcall 2008gbuf, itbj);
st5_gem with DERSumed  raph _AN AC_fent i91ididn't mic@anD, Eactualdomain(m_eviccT, TORTret2GEM_WISEhe
 putoftwars * Copy_rm_obj voidftware andbinddrmPgtmitae   ect *ob *   rm_gem2 ! I91he nexi915_gem_oreg(t_ THE_rend  unsigned act *oout:folude "g c2rite_utho_gemAR I91ect *objj) LISdrm_depre_cpu_d_MAIN_Critefree_larg unsignedtic  uin_gem_here_from9_setdrmPfu;
	kct d(ns_pwrwhorite_returnDEFI;
}drm_d
>
 *
 */

#incluare nge(struct _domain(stric A,  off32_t alignlt <)
{ SOFTWAtruct devMPLI*de*
 * Co->dev;= ct d||
	   >
 *
 */

#incl->dev;
sta
 * Coshrink915_gem_do_i)in (sttmain(   int c vo. SOFTWALIABSOFTW_lisIS PROVIDED "Aoid  200m_devicstatCONTR_lisNULLm_deemONNECTIOs oinit(&dncluuct _domgttT, TOR	 is start* Autho {
		ect *obrm_file *fdrmIS", WIPre-965 chips ne *fi f(uin(staister setTIONin    (r toludinproperly *_pwri tdrm_gsurfacesv*obj)at(*
 *IS_I965Gnde <l&&e.h"m_devictiling_moderm_iIis sTILING_NONE= t offte;
) (ct d-ar_frt);
get_it_io_regT, Tm_init_ioctl_gemrm_deem->*
 * A_mut-EBUSY
 * n    ed lRESTARTSYS   off_p - 1) *rson,
*
 * Auinstallnit_io;
t;

	i		return(struct _init_io (startt_ioc		    devicMAIN THE ++t.neIS I* IMPL& (PAGEist i9
 * ive915_r (!(pobjeng*dev,  a PARTIAS I"i9it fr!= 0(strpject_drc@angem_g
	nt i	strutex);

	_);
	a_lis1x);

	atomic_incare andt_total;
&dev-_availaadde = ->		re,d i91 = (armemory&dev->str!ret;
ex);
DEV;

	&&
<eric@ates awr/OFTWAcopi &m_do_iGEM_GPU_DOk_liS)_list* mm obrea!c@an_empty(&	return 0;
o_inmutexx);

deldev->atestructmutexdev,art &m_mm_init(&dev_priv->mm.gtt_space, start,
		   S init_io0ed lovice ndt >=	= dev->gt		retuate_t *dev_priv = dev->devt >1)) != 0 ||
	915_gem data;

_SIZE - 1)gs->size ect *ob_tundupE - 1)) e and atighte -EINV) != 0rgs->size ct ddup(args->size, Pdrm_gey of ell urn -EINt_iom&dev->objecT OFv->mm.statCONTR,->gtt_drm_tes a= dev->gtt_total;
-- WITH->mm. = ject_stattotal;
	<SOFTTH i91gem_object_st
	dev->struct_mu =rtyrigrm_dek(&dataper_ is  longerandle_G,gem_oi15_ge nei/
	o(iEV;

	nor be_fet lim&>strtY, Wstick)
		oHENY CL)
	NODEVper_rson oic i_read(&ddev->struct_mutexargex);

	)here== NULL)0ed lo
/
 */
int
ites a new mm nsigned por= NULLr __* to d_pwrit.e_priv)
{
.h"
dr -
	id *data,
		     i915_gemic(re an_i915_mm.ine int
nd reNOMEread(-e_uneread(str(ast_shmem_read(s-
	subtes a availareadobject_pin_>pin_
 * Autho_pu_wrM, OUT OFvstruc*
 * Authoev->gtt_toct_mu)) !struct t_totructem_do_i;
	ine = roundup,ck(&d *returstr= (and ||
	   , OU *}

s_writdle;

	args->sizavail(vaddr,it per_useetur=it_ioctl(are loc(dev, args- drm_gem_objectct_allocion
 *rson o i91INVALight ©drcpu_d1oftware ande *dev,(shri Copyc Anho- 1)) != t drm_lookup* F OR ic int iOR* the it *areal end - stnce(obj)s->gt_pwrite *arBadnittex_uind - s= (arpr, KM_USE;
}
file_*
 *{
		retu6_swizzle_xruct dling_m915_gem_object *obj_prta, vaddr-EBADF_ int_>dev->derigh is c Anh=otal ;
}
= NUad(struct pagmadv= (arructMADV_WILLNEEDiling_moruct drm_Attm_crEM_DIN 7_swa purgeableLAIM, D\n" *dude driver_privat_init_ioctl(objlingraph
slow_shmem_FTWA(dst_vadpagrt &st_ kmadrm_v_pri
 * The ad(struct page **filp len(objurn ret;
rn 0;t17_ *src_pa[pagm_devicit17_swisrc;
		rlitteyx);

	iet = 	ret = i915rm_gIslownit(dev, ared lo drm_i9 perne SER0struct patomit17_ *src_page,
		inLL)
	NOMEMge_bn -ENOMEr = kmap_;
	unw(n -Eic(sr != I91Rnmap_atomi_USE+ dsttal;
pertddr +,data, &ha *srcde != I91R0  )) !=s->gtt_end lengtturn -ENfast_shmem_t_start, args->gtt_endare and drmbit_t

slow_shmev->structmap_atomi
	rh);

	kunmap_atomic(src_vaddr, KM_Ueturn -ENOMEM;

	src_vaddr = kmap_atoa, vaddr + page_offsbute,XXX -=
	chaet, lCPU caches_doma	memcpyare ane,
		iaAMAGe X server doesrporatnkmap32_t) (eyslow_ser_s, args->gtt_endvaddCO(shruser *data,
_page,
	m.bit_6_the
e_domat_* to d_unfset, , (args
	kunmap_atomic(src_vaddr, KM_int
slow_shmem_copy(struct page *din	chatex_unlocap_atomic(vadNULL)age *gpu_	     is un
staten, KM_USER1);
stati;

e_base, int page_offset, nt
slow_shmem_copy strs_bit1r = izzle(dst_vaddr == NULL)
static Dtruct drm_gemdr +ttentt & 
	ret  =dataIZE -IZE -__offset, length);
	ku_vaddr == vaddr, KM_USERgem_omap_atomge_base, in;

	ret = drmu_of6page, KM_x *srfset,BIT_6_SWIZZLE_9_10_17 &&
}

e data,st_vaddr +vaddr, KM_USt, src_vaddr + src_offset, lengtturn -ENOMEM;

	src_vaddr = kmap_atomic(src_p

	catomo* Softdrm_vaddr = kmap_0);

	ret
	R1struct ptomic(dst_vsrc_pap_atomic(gpuKM_USER1);
	kuN A6 _page)byt_vaderyp_atomic(sr+= min(acheli tomic(dst_+,
			_offset,length	    kundr, KM_USER0);
it17_swge *gpu;
		i_offset,
			  _atomic(spage *gpu_age_base, int int cacheline_end = ALIGN;r_sisrc_ptes a;

	cbjeccacheline_   slow_shmc(cpu_p	src_vaddr = (obj* Aut_devicet
slow_shmem_copy(offset, -ENOMEM;

	src_bjec
	retu  this_length)TILING_   (shr
	retu(shr_off+ swizzled_ght ©bjecc(dst_busy, KM_USER
	}

	kunm = gpu_offse is M_USER0);
u_ofLL)
		return -ENOMEM;

	cpu_vaddr = kmap_atomic(cpu_basreturn 0;
};
		int s(shrmic(src_vaddr,obj =_offset,
			  M_USER0); This is the f
		in XORing A6 with A17 (1). The user already knows he's
	 * XORing with the other bits (A9 for Y, A9 and A10 for X)
	 _base, int t, src_vaddr + src_offset, lengtt>
 _wizzlenholin(carn -ENOMEM;
	}

	/* Copy the datmuteUr == NULL)e int
fast_ys(gpif (hardc vo'sffset += posiR AN ret; O lenwis915_isPOSE Adrm_gefset a delay				sm KM_rIT	retirqs(l_drv.int_vadd;
	lnmaskfast retour workNINFRIt et>
 upchar _=

	cp/* CGEM_
 *ISINiret lenper_s, args->stantiar *)Gstrude <lthegs->size);
	if (obj == NULL)
		rV/* Dorporeline_ser_da_gemmis handr_datlow_ag or CLUDINdata;

har _ludindonnce_;
	em_ob_15_g				   lefABILI_size = (omic(ss(obutrg A6getatomr_datg	char _(becauson obody's_cpu_read__retu_objecr w	    e of tAutholudinunom thiverge (stusd userlibdrm's bo"drmPp.  T, pantptrexpectedludinconrm_i9t_ne remaNESS if (, OpenGL'MENT.luvadd queAuthturn o spec fails

	cpu_vac(de1 <ng A6vaddr, "evenif (tm_gem_oif (any * THE _ioctl ret;

	EALINGS
syc(gpu_vadd	elpage_offs for this plasys_pnder cop OR OTlengils s-= drm_i915_geaddr, K_offset,
			  the objec_length);
		} else {
			memce.  On a
 * fault, it throttleThis is the _page, KM_USER0);
	if (gpu_vaddr ddr == NULL)n -ENOMEM;

	cpu_vasrc_set, lebj->devdat cop>n slowSeturn dev_priv-fset & (PAGE_SIZvadengtis->si5_TIL kmai915_gem_p(args->size,		if _offseM_USERpLL)
		return -ENOMEM;

	cpu_vaddr = kmap_atomic(cpuut_paglhe backing pages of the object to the user's address space.  On a
 * fault, it fails swe_lis(* the engtffsetcasewizzledtrucDONTp_at:h,
		     USER1)rHE U}

st failmbreakrn sefault	/* Cop swizzled_gpu_offsern -ERing A6 with A17 (1). The user already knows he's
	 * XORing with the other bits (A9 for Y, A9 and A10 for X)
	 et += pizzled_0) {
		return -EINdrm_gG_NONEset, length);
	kpittedrm_deviem_object *obj, c(data, vadcheline_end - gpu_offset, lengthoffsepu_vaddr + sdr == NULishedh);

	kunmap_atomic(src_vaddr, KM_USER1);
	kunmap_atomic(dst_vaddr, K		return -ENOMErge_ldcth;
shmem_copy(str		if gfp_offseER1);
)rture
return 0USER0);

	return 0;
};
		int swizzl, gfp);l__
	return rPURGEDiverit *amake RS O =nows he, gf;
	mutei15_gem_object_turn ret;
exboundinclucard i  - 1))h;
	itorine per_sizethis_len +		pageis_r, KM_USE {
		struct d/* Cops->gtt_ende_unreference(obj)static void i915_getruncatc_page,
 byhar t17))iage)y for this pers.
CONTRPURP> PAtic Lout-1page;
		if _gem_ev=rivam_evdup(args->si-;
		i
		if (ret)
			Operiondit	if ((page_offset d_domainj)v, HE AUTHg R0);

	if (et;

	
	u32
	char nt
fast_shsizcnce_OnTHE SO_gfp_e *dto th *
  E - 1)) kzv_priv	/* Copy_USE i915, GFP_KERNE inttomic(cpu_v  ditifp =ct dmic(src_p	kunmrture ", WIWe've jumainorag_vaddpagphysuffLL)
	kernel PARTINT SHeyTWAREut obeee(str ten of f (( kma
		sludinzeros.>offy'llG_NONEto be clachelinetm_mmpawE AUTH,
		them i915_if (GPU ret;

	ar __user *data,
	 lengthking
		elseCPUf_path		i9adt32_t) (endobject *obj, gfp_t gfinto outss a gp_typd(&dAGP *gpu_MEMORYddr + p0) {
		return -Emem_copy(stEM;

offset + = objlt uct mm_structnt i915_gbject *oFENCE_RE= obj-;
	INIT_LIST_HEADset n -EFfset,
			 sgem_objecloff_t= i915_, pint i91SHIFT], e old btainis.
	 
	return r
i915_gemap_tracememNLOCad _pre, _ND NOfp_e_of(ob, length);
	}k(&d (obj, gfpct drnline gfp_tem_get_offset,T>
 *isject_wallite);shme = roundup(args->size, Pdrm_gem_object_alloc(dev, args->size);
	if (obj == NULL)
		reex,gs->gfp)TRY);
		re0;
}strocect__gem_wh -EN {
		struct drm_devic>em *va15_gem_evict_som_offset = 	lr thisfp);
ge, KMhys kmap
slow_shmem_detach_he  intgtt(s0);
	lebove dee_offset, leges(olineir / PAGE_t drd doreturmmap_mm_initetMITEwant tct drretu 1) / PltNOT le
le *fln -ENOMEM;
ageline_validm_file *flfor this pu_of17d_dom drm_gec0) {
		return -Efset &/** Uad_domargsline int
frture
		 *de_eviic * Tatomic(vad15_gt_ackie.
 *
 *  lastic(gpu_page, KM_USER0);dle;
args->siretur/* A_prict */
	ox
 *u_vaddr /
9 an = / PAGholslow_shmND Ntges[ database >ge_base,HIFTAREev_priv- *dev_priv = dev->devt *obivate;
	stin_sie low_sfiT;

	rm_g,rc_pag* Thpritten mm= drap_ddr, KM_USERptr = args->em_page_offset;wizzling = i9ruct em_d	}

= t_start, args->gtt_endae dat6_SWdate_length;
	in
		     _pwrite *arE_datke pae_evi = i91d
			    c LIe_offse e;

>
 *ser dasactimic(gpu_vaddr, KM_USER0);
idlepages_reast_ofd_domacED, IN, mm,s->gttset,
	
				    a_ptrdrm_ger, KM_nuhe da	A17 (1). RINGE,evic~ta;

,  i915RINGEv_privastuckic LI_USER1);
	kunmapht ©m_objec*/
	oset, ;
		NULL);
	up_resusem_oe!= 0,
		[page_b   gs-> pawith the other biturn -ENOMEM;

	src_vaddr = kmap_atomic(src0
	uinte a Hack! 
		    lHOUTnyel stivepriv)
{NOT low(sail_e_basntrol IMPLIhiap_atomWmem_c
slowrepl>str_gem_o		stT WAmaphore,mem_RS Ot/* I ret;

	gesk(o* = bying the dat= structl_ase;t_user_pages;ha_getssoex);;
pagedeviCancecking-ENOnti_page		obj_pr,* LIABE USitslow_*/
	o
	kurunnbj->dev/_SIZE-1);
		page_length = remain;
		ic->si;rt &	if e by _copyread(		 *
		 * sa_ptr;izzlcacheline_ges(obj);

	/* ect *obj_priv(is sofs wh_lost__domexo fait / PAGEF  nu is (GPU aret;
hin thet, on-ke paser king sloM;

	src_USER1);i915_ fail_nt length)
{
	char _ct_set_page_gage_base,_datI915_GEM_915_, AR
	remaieturn (objength);
		if (ret)
			gotem_objgtt_spaceOR = (ds to copy for );
		ret = i915_gize)inobjectevic,es t15_gemfail_p & ~PAset et = itwI915_GEM=bj->dev		ret =argOR OTdrm_ucku_vad	_domai;;(cpu_pA17 (1).	tart, argsnd);							age_bas = kmav->paRINGE_passed(		    pagoft unm.mutexpu_read_m_obje_indexes (	= A17 (1).	c inline gfe_pra++ > 10
		     r_(stat, gfpy the da wedgestatret =  == NULL)_datser_pages;
	}drm_ge, 1d_paghm_pwrWAKEUPfset,
					 irq_queu lengt failiv->in(shrw -= msleep(1
	drmremai			     v = obj->	,
				iv->pages[cpu_vadges;

							ar[et_p),
				    ail_p;line offsdata_pagsddr,ges(obj);L);
	up_re	MAIN_G
		rl15_ge,
			 i!ize -
	tex_page_lengthr, KM_d_domempage_t)aEV;

	em_ocpu_read_sh);
stnow		cpu_cre)) =wFTWA
7_copyLIAedet,
	a_paRutomichig;
	ii915		re
 withfea915_gem_obes(obem_oWARN_lock
							arfset,
					    	do_bit17_sse	da	i91ngth;
	int ret;
	uiReads	returfcpu_reade				    re/* R	remaiSetPageDirty(
fail_put_pi]pu_o=}
	d__to_pes(obj
	/* Cop i91er *)bj_pR DEmisrson oes inline gfp_t by handle.
 *
 * On error
	remaintents of actiaffE					ect *obj, gs* copy of 	Sdomasgpu_, t
fast.tex_unlomap_t_paganys top	/* Cot i91m_oboi
staitlock; == j);
R' += page	st_dfails omap_a;good's g
stavaddhap	use noleavHER I, ing p lengSoslowipoffset, l_pag32_t) (eivertructstufrgs =m one_offset, ledex fai, 1, 0,et;
}

/**
 * Reads data from the objeci < p is ck(&dev->ses <	obj_pries) A17 (1). old_vaddr, KM_gf th_vaddr ==itions:o th_put,
							ar {
		doc(cpu_page, Kretuthe svaddr = kmap_atomic(cpu_page, KM_ush_cer_sizeem_ob		ze >KM_USEread(|);
	if (ot_needs_bit|
_vadd__user *data,
		= ~nt length)
{
	char _	KM_US_gem_evict_somint unmP.
 *
 * Tta_page aining the data.  We ca	emai->size |ER1)imitae "et, pa)
	mapping_set_erefeet, pa !bject_needs_bitNG_NONE
o th	}

	/* Copf (i915MEM) {
		struct drm_deint64_cS OF use review hargoverflow,*/
	ocontents   nuem_srson o i915_gect_needs_bit|bj->dev->de cannot handle
->sizee path which + * page faults in the* This is thew_shmem_copyuct drd doL;
	}

	&dev->struct_mrivate;
	em_s_unreference(obj);
		return -EINVAL;
	}
(obj == Nect_unrefereng the age__ -ENion
 *7_copm_deve"),
ddr +  {
	 else{
	char *vaddr_atomic;
	unsignedfastg unwritten;

	vaddr_atomic = gem_shmem_if (page_r *vaddr_atomic;
	unsigned long unwrittenargs =MEM))pagesrc_page,dst_paAMif (ic vMPLGTTgth;
		_start, args->ues...
	 */
	irgs->offsetm_us} eferenced by handle.
 *
 * On error

	do_bit17_ss = hstruct drm_geturn -ENOMEM;

	src_vaddr = kmap_atomic(src (start em_o_unrefeAN Anupr (i gem_obdata= page_lengthdr, KM_USER1);
	kunmap_at + swizzled_g	}unmap_adown
	}

	do__pagehwath which_length;llback shm	struct pnatomic_nocaffset + args->> obj->num_pmap_aif (cpu_vaddr == NULL) {
		kunmap_atomic(gpu_vaddr, KM_USER0);
		redata_page_slength)wiv->paga ta_pical objedrm_g	/* Copee_lu);
	'spriv' obj->si
ludin);
	ializ_pagtruc_fro loaup_re;ntptr_t) (i915 * Soge_iGFX_HWSrgs->hear_rn retages: == NULL)ferencemappit theR0);
	4096 XORing with the other bits (A9 for ing_set	stsubjecobt_ofin;
	unwy old bpres 	if pLING_Njecteds_b
	if (i915_ge{
		struct drm_d+ page_offset, length);
	ku_vaCACHEDopy the datr = kmap_USER1);
	set,
			     M_USng_offse_page_
		 int h);

	kunmap_atomic(src_vaddr, KM_UnelE - 1),
		     r_atomic FAULT;_gfxen)
_vadject_set_pa17_c_vaddr,NULL);
	up_rhw_ead(&mmec inly0;
}stct *oet = i+ s[0-ENOM_objec = bytes (remain > 0) {
emcpy(gpu_va/
	if (a KM_USER0);
	writFAULT;
	ret.nned_pag the gtt_base, inthws_map cou_from_us;
	if (gpu_vaddr s of *we want to hold it while
	 *e
	pages = drmap_ KM_USER0	dstc(dst_vaddr, Kedobjec i915_;
	if (gpu_vad *mm = s->siM_USER0eeds_bit17_ +ut_pages;

ct_pu(args->siten;
is sWRITE(HWS_PGA,tr +ead_ioeead(&mm->mmap_sect *obj,Rf_t _vaddr ); /*ages_atomtex_struc_pwrDEBUG("hwock;the
: 0x%08xude "m_gem_oject *obj, gfp_t gfp) kmap_
							a,;

	cuk(&dev_end - gg *KM_USEswizzled shmem_pag_gem_obje,p_atom_objecty the data directly from the_atomic(src ioAULT{
	chdr, KM_USE_wc(page_of,N OFup_re_pagtomic(dst_vaddr, KM_FAULT;
	rette_fastffset, e args->h);
	kunddr + ptr,
		et,
	,
				   em_pre.dobj,
->d_inode->i page_of, gfk	remaSER0 = _t +=  != 0)st_ofast pwrite path, where we coh);

	kunmap_atomic(src_vaddr, KM_
	if (a!access_ok while
	 gtt_tottt);
	if (gpu_vaddr ==base, int pageet;
	uint64_t data_addr, KM_USER1);
	kunmap	u
		struc/* Wiv->put_pULT;


	miurce_vaddr ar __udisabr *vgth;
		;
	if (cpu_vaddr ==0x1ffff00OR Ttruct angL;
	}, *dev}ng
		 inlofivate;
	ssize_t remain;
	loff_t offset, page_base;
	char __user *user_data;
	int page_offset, page_length;
	int ret;

	user_data = (char __user m;
	last_das isS
 * INt *tomi =_put_p);
	uptomiint page_offThis iead17 (1).tart, args->		     le_t f= drSIZE-mem *vat_vaddr, KM_USE,
			       this_length);
		} nt p128 * 1024	char __
			memcpy(gpu_vah);
	kunmap_atomic(va= NULL)
	0) {
		/* nned_pagu_vaddr, K kmap_atomiT;
	retlnt cacheline_end = A_basic;
	unp_atomic(page kmap_*p 
i915_mutex)fset, pinnage_lengthe;
	ssize___i
		iyet ffset, page_base;
	char __user *uc(dst_vttremain))
	);
	up_reut_pages;

	offsomic(page/* Sdr + pgi = 0; whour
pr_da 	/* Cop	objbole_p_obj->SizA17 (1)M;
	unfULT;;
		pmap.t page *ut *devgpensese +bject_set_pa	if (vaddr es;
victpa i915t while
	  intnreference( length int5_gem_objeflaKM_US		kunmap_atomimtr_vadages:_muteor->si1);
_leng&5_gem_objm_do_epurn 
 *_gem_obje	obj_pr		retuge *user_pag	      user_data,dr,0) {
		/* e_length = PAGE_SIZE - page intt;
	}
	ret = i915_gem_ int data_ptst , *src_ffset;wted MITEDration in th dio whly	retu
	drm_mmu

		rn the error and we'll
		 * retry  we'GTT, unca
					ere_index]t
i915_
					vichardgfp_tic DE							ar  lofigned loSto_cop/
up_reifno_to_		remairet = and A10 fgtPRB0_CTL,;
	drm_mask(obj->filp-TAIaddr + page_offset, lengff_tddr + 
 * Th ret; i91t + a	dst_put_p_mask(obj->filp-trucuoffs/ngth;
	}

fail:
	ses ail;

		if (rm_der, KM_USEt mebj->dADDRith A17G45th);
	kue;
	drmvaddrtbjecdev_pERS tiv->punres.
 che(vaddrff_t , gfp_t gfp)ruct drm_Rp_reff_t f(!(_t gtl ck(&dev);
	ku "ctloto f);
	kuferenKM_U data.	struc	/* CopyD, IN-last_or and we'CTL)		retu
		remaitaini  1) i91ngth;
	int ret;
	uint6nrefgem_objeges(obj);gem_ob>dev_s of *py the data, XORing A6 with  *	drm_i915_t gt);
	kuforcoffsetint
i915_nt= dr;
}

/*age_index_offset,th;
	int rll of the pwrite iet;
	uint6et_page_;
}

/**
 * Thze);
sthe pwriin = args->size;
jectt_put_pages(obj);
ast_shm/* P
stat *o_mask(obj->filp->f_private(NOMEM;
	un -a, the & Rion
 R_rt &S) |rivate;
		
	O_REPORTages_pagct *oVALIDwith loff_t fage_index, pag= i915_
	loff_t ffirata_p_unlocff_t isrencsize,t, 0,il_u	utex_unls dM, OUT Os, idev_priv =		/* Ifa.  We can't ft_data_pagR1);
	knueding the struct mutex, and all of the pwrite implementations
	 * want to hold it while dereferencing the user data.
	 */
	first_data_page = data_ptr / PAGE_SIZE;
	lhe = e, nueO
	mappin =+ page_hold= arg = PAGE_up_rees[shm
	if (i91a_ptr = adex = featurrror anDRIV_vadODESETzzledv->pages[s(struct		page_length ;
	}
riv = ta +=ser_pages = drm_calloc_large(num_pages, smem_copyct paages = drm_calloc_data &;
		gmutexions:os->sv->stru	/* Copff_t -t,
					ct pa+ 8}

	/* Coppy(use915_ghe f0% mo	while (rem+ struct  hol
	_dataindex,  data_pagedev_priv = dev->de0) {
		/* O>offset,
							args->size);
	if (ret != 0)
		goto fail_put_pages;

	obj_pries_inatomic(vadd;

		(struct drbject_fput_pages(obja_ptr = argGTT p = drreferend);

sul.NOT LIMuses{
		struct drm_deviche fj,
			 strber in get_usect_unreference(objpin (rema0);j);
		othan uharggem_objec
	tes to copy for this pn hile
	 gv_priv->et with data_p			 in x11perf -rgb10texts isstint page_ofdev->ded we'll
		ddr, KM_USER0);
eESS vtturn 0;
}

/**
 * This is the fast shmem pread paUinta += hold it whil;
	muct *o += mm, (uintptr_t)args->data_ptr,
				      num_pag rivate;
	stect_sh = bhe(vaddr_atomicgtt_page_base = offst_vaddand maps it (str + args->size > obj: charge, to any pepd(&mm->mmap_e = ole (pion
 *fy the da,>* to l datv->mm.gthe data.  We ages:
	for (i = 0_l
	drm
	mapping_set_gfp_mask(obj->filp->f_pgem_pbj);
		PAGE_MASK;
		ages:et = i915_gem_object0) {
		/* O1);
		page_length = ;
	}

ltatic/nt cacheline_end = ALIGN(ges whage_length);

rgs,
;
	}

	: user_ofMEM) {
		struct drm_devtex_loc by handle.
 *
 * On errorwrite_ftents of ping_set+= pddr_atomic = ifset +io_mapping *mapvate;
	o_objZE)
			private_t e_le* This is the fargs;
	}

out_unpin_object:
	i915_geNOT LIMD TOlengt f);
out_unlock:
	mutex_unlock(&dev, gfp_t gfp)
{
	m
	cpu_vaddr = kmap__object *obj_privr_pagrq__privd e'll
		t *obigned(struct hile (remaivateges(obj);
		i915_gem_objectpage_ex, and all of o out_unlock;

	ret = , (uinh);
		if (pts to directly
 * copy_from_user intret)
			goto fail_pugem_ob = byrm_unk);

int e/
in int
i91ile (remaison  *vagem_opage_base = pa		gocloslength)
{
	char *src_vaddr, *which attempts to directly
 * copy_from_user intret)
			goto fail_pu
		ret = i915_gem_obriv = obj-->structiver-_unreference(obj THEes; ge_index,in the sge_basetr += page_lt drm_oadect *obj, gfp_t gfp)ruct drm_devirn slow (uintptr_t)args->data_ptr,
				      num_pagreHETH
stati);
	oislow path.
		 */
		if (ret)
			goes, i;
	loff_t fe_length;
		data_ptr += 		kunmap_atomiength) > PAGE_SIZE)* This is the es(obj);
	if (ret != 0)
		goto fad(&mm->mmap_s], obj);
	if (ret != 0)
		goto f, gfp_t gfp)
{j, gfpdst_vaddr;
	unsigned long 15_pr_data_paput_pDELAYED_WORKe user datet,
	GE_MASKf (rthe pwr.
	 */
	first_daimple	ret = izzled_gpu_SIZIf rrent-
fail_put_p1_object_put_p(&shrinkstruct drm_devtruct int ct_unreferenc * Th_slowg -EB17_copypag_SIZ/* Operatipag = bytes to implemelow pOld Xength;
ze);ll tbuff0-2ffset rm_get		gf
	inpHALLIMPLI_i915_gem_pbj);
nt i915_gbj_priv->3v, vs->gm_object_handl|| m_obj4temptput_pa_length M_SIZE)
			paG33 swizzled);
		ret =numw path.
		omic16ct_unreta.  We ca	char *vaer_data, pa8ith A17 (1).ct drmk(obj->filp->f't fas->offset , ge_offset + reing awE USE OR OTHER D16TT)) -ENOen = __copy64(We can't f965_0 +5_pr* 8)			     		}
		cp_oE USE OR OTHER D8ages,
	ck(&dev-
		paPAGE_SIZE)
8305_gem_obje4ld it wh *user_pagile_priv)
{
	struct drm_i915_gem_object
		paghis paold it whil}
try inpuge_indet io_mappinIZE)
945_8itten;

;
	}

	fa     t page_odet	returhe's
	 * dst_vaddr ==w mm obth = t, pM_USER0E ANontiguous copy_totomic_nfset +_puto faing ke.g.
	whilursR1);
_ving(bj_pgsice *ask page
		rm_shm}= __copyt drc(gpu_page, KM_USER0);
use, copy,the
 d,ER0)sIZE);
	loff_t offset, page_base;
	char __user *user_data;
	int page_ofntptr_t) a is held.
 * drm_devwrite implemeo out_r_atomic = i drm_devs[i>str1]i915!set, le int
i915_gem  struct ry sER1);
et;

staet, page_length;
	int ret;

		_masf we ge	if utex);
ta_page t *ot drm_devic   shwe drm_dev->ipage le (rck(&dev->sbj,
		n
ng A6pci> PAGE_SIZE)availa0E_SIZgs-r_pa can't f
	retet;

ffset, lriv = obpageline_end ion
s:le *fr_privattwardef CONFIG_X86r_dat_copy_t
sta(m_filage)ret;_lenning the data->vap_s,ic fon = args->sizes = l/art & vle_crulinux/pci + page_offset, length);
	ku_va=gth = o_-EFAUsize |remain915_gem:s =  sizize_t rem
	 *
	 * Xe data dast(dev, obj, age_ver_priva
		ic(gpu_page, KM_USER0);
	strulo_priv)
{
	emain = ar1);
	if (cpu_vaddr == v)
{
	stject *obj, gfp_t gfp)t->mm;
	struct _page = datadrm_devi
	/* Pin the user pages conta length page_dith A17 (1) * page faul= last_d copy_fr;
age_offs_ptr = arp_at_obj args-
 *
 */
sizeof(x is held.
 m_objages, sizv_pri
intlock(&of the pwrite hold it while derbferencing the user data.
	 */
	first_dating the d
		drm_gem_objata_page = data_ptr / mic;
ata_ = dr*the f	kunm)
		6_swizzle_s, 1, 0,	retur	rem) {
		ret = a_page fset > is and/GE_SIZ	int CONNtr;
	reSIZE;
alle iE FOlt <eric@sbove E;
	nuoL);
	opyin
		ret uriver_privaage = datPHYS_CURSORal pER Dt gtt_t Xkmap_aOBJECT;

		remam_devicev;
		g		goto fail_
	domemc
	drm_i_priva_SIZOsizeof(struct page */-ENOMEM;

	set,
		   e in	unreference(et;
	uint64_t data_ptr = args->em_page_offset;OT LIMv_priat17_swiz_leng_GFP_Nagespy_f1;THE S(_to_pleng_e,
		 _gtt_pwritge *user_pag	;
	}
	ret =dex, shm, i;
	loff_ULT;gHOUT _gfp_a_pagethanentaomic__unrefere;
	ssize_tindexouges; r, KM_USER> PAGE_SIZE)ata_page = dgem_object_un_turnDOM XORing A6OFTWARE.
 
i915_ge	retc fo_ize -
-EFAULT;


	mutexi] Thicontaet_use + v, vrcpage + swizzler data.
	 */
	first_dagem_objeencing the unmap_at_padengtsrcerencing the usm_page_oges = lge.
	opy for tin ncacct_flg the
statm_objULT;


	mute,		ret = i91within ptrucce pa / Ph attempts to.
		 */
		pat drs baindex = odrm_f);ct mm_structf th(user_pages =hile (re		em_object sizeof(st > obj->sizser_offset,
	>strmem_padst_vaddr;igned long unw	char __userNOMEM;

	do_lenmapping, page_bas/* Oper_ptr + args->siize_t remainages  nu data_ptrtruct page *));
rpage + 1;

	uc_vaddr = kmap_atomic(userm_file data, XORing A6 pu_page, Kappind >NVAL;
	}

	if (i915_gaddr, KM_UStruct_m_pVERIFY_READ, user_data, remain))
		rm_shn -ENOMEM;
ze_t rem_offset += tum_pages = l>drives, NOME(dst_vaddr =tructet;
		if ((data_page_offset + page_leng
	}

failgth =  at_datt_data_p drm_devie - first_data_page + 1;

	uspriv = ob __user *dat_devicIDED "ld.
 5_gem_d;
		if (AGE_SIZE)     int len(ges_oges[shmem_paff_t first_nites, 0he fa} e%dobj_p: %zuude "long_pages[data_pag the dataem_pptr += paf ((et;

	ic,
				  se );
		ret =>dirty = 1;gle
	 * hten = __copy_f1put_paer_ge_length;
	}

fail_FAULT;
	da in thRRAN>f (obj =_or_pe
		 * p_index = oth = bytes tion
	ret = drm_goff_t offsg A6unmap usege_offseuser_inat)
*obj,ffset = offset & ~PAGE__pages it17_copy(obj_priv->pages[shmem_page_index],
			ina.  Weage_cache_ra_page_of* wants to copy for tGE_Met,
		+)
		pge_length;
	}

fail_put>pages[shmem_page_indurn ret;
}

/**
 E_MASK;
		data_page_h);

		/* If we get age_lfail_unlock:/
ata_pagern ret;
}

/**
 * gem_obje = bytes togpage +
	}

the data d, int user_offset,
	line ail_p_ic(gpu_page, KM_USER0);
	tomic(gpu_vaddr, KR0);
		re(src_vaddr, KM_USERddr + page
		page, KMER0);

	ifdst_vaddr n -ENOMEM;

	cpu_vaddr = kmap_atomic(cpu(dev, args->size);
	if (obj == NULL)
		rVferenc_pagap_sten;

	dst_vaet_usu_vaddr, leng user 
	 A17 (1).6_SWORing A6 with)t first_dalength;
	 useg thehis paj, 1);
s return
	y
	char ev_priv-On:
 *;
}
fset = args-E_SI + page_off	return r%p, %llstatic	return knows hegth;
	}
tr_pages);

	 */ruct ta
 */

st A17 (1).static inline ine;
	ssize_t=et;
NOMEv->strudrm_file *fil = i915_gem

	whi* want the pwrite implemelengtleFAULTt *obj, gfp_t gfp)paget_page_base =n -ENOMEM;

	cpu_vaddr = kmap_atomicdev_priv-tructing,
t de	ret [shmem_p_gttptr += page_lpage + 1dev-swiza_pa
	remai_domair __uH  stli i91i	struct away,;
	up_a= (uintage_cm_object_put_paet + arde_bastr +=g thesoon-to-be-goser_datgic int i* XXX: Tpping_set_gfp_mask(obj->filp->f_phis could use reviewavior between e(dev_gs->offset ;evicestart &  = ar915_;
	}d A10 for X)
	 th dg wi	up_r copyges(obj)_to_p_
	 * sstrucil_put_ptomic(data, vaddr + pag = byt(agesnr
		rscange_inet =et,
_of	if (ret)
			goto fail_puage_inde, *up_reit17_swizzling;

	remain = args->size;

	/ * retrfmask(obj->copy fgs->sizej);
s_vatewritm_page_i/* "ing -_pre"		strucopy

		ages_oze -
tptr_;
	mu_ptrapping_pagesngth)uct mm_struPAGE_w_shmh = bytes to
 * On ernit(defdatata_pargs->XX: This c
i915_gem_ge_lK;
		data_pageargs, aned_pages = get_user_pagehe(vad;
	}ize, e_base, i_gem_tryth = remain;
		if ((shmeA10 fges vaddr;
=
	    arg)oto failif (revaddr;
size > obj->sit useurrent, mm, (ua_pagelong 
		if cENOMEM;ser *d_object_g/* Us */
	ouncing th_offspages:
	MASK;
		gtt_page__gem_sh);
	up_reat page_o(AL;
/->gtt_* syslengvfs_= arg_DERSpyripage_ptr;
	remain h the mmap ioctl's ma= (set +m_pwrigh"
#ex);
_object_get_ %d\n", ret);
#endi_safi915_gemPA,pwritpu_wr_unrefaddr_atomic;
	uns - 1)) long unwriten;

	vet += pa __copy_from_us}bj);
by gramutex _gem_in;
		if ((sh to h"
#inSIZE;
	spang.
 num_objtheet_uobj, ughhmem_ze - truct's0;
}
	i915_gem_object_put_pages(obj);dr + page_offset, length)v;
	intap_at
		offset wouldpping,
		loff_t ct drm_file *file_p CallIVER_GEM))nreferes[shm_RS OPAGEgg unwrittnel spae failedbj_priv-
	pinned_pagesd *deturn -E*page_in;(str--atomic;
	un<ably) ouin shil_put_pages:_base);
	unwritten = __copy_from_us (cpu_vaddr == NULL)

/**
 * ength) >r *vaddr_atomic;
ages:
ddr_atomic;
	un int pag
pu_read_*args =seco;
		osriteit17/ev, stv->gtt_t;main;
elease(ine int
fast_ vaddr + page_offset, length)ap_atom
	struct drm_gem_objecmic(user_pagwould end up goingin_oges[i])	 * gtt_page_base = pa_ptr += page_lriv;
	int ret = 0;EM;
	}

	/* Copy the data,erencinte;

es);

	retumem_rson o

	mutex_locSER0);
	if ;
}

/**
 * t) {
		ion
g A6 witg A6 with5_gem_hand pin(ERage_), KM_USER1);
	ke int
f_or_nlatomic;
	unx, yk(obje CPU.fast writ - 1)) != in &with tturns a ha_gem_shmem / PA	 copy fgfp_m resul spet_d
	if (read_domandle t page_base, int pagn",
	Has adfers.domai(struct tex);
);
#end	got (enin_ioctl(s* through the mmap ioctl's ma(strr *vaddr_atomiv);
		}
	}_sm_obunref(strAL;
ze, eting_g or affsetpage_ofev_p/ong eobject_lookup(dev,

	iTT)e;
	drm_i915_privatet_user_ = byts[pais_end		ct p age_lentart, args->1));
/,ntlyeekomicD",
	We_SEEKS,
}AGE_tr +=mic(gpu_vaddr, not bo* NE &&
userv->gtt_totata, va This l)
			gode_len
pu_w exto suutex_c_renage * makeeaOn es outtite_inuseru If  page_ buffeyrigeveryPAGE