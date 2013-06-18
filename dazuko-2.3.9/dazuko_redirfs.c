/* Dazuko RedirFS. Allow Linux 2.6 file access control for 3rd-party applications.
   Written by John Ogness <dazukocode@ogness.net>

   Copyright (c) 2007 Avira GmbH
   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "dazuko_linux26.h"
#include "dazuko_core.h"
#include "dazuko_linux26_device_def.h"

#ifdef USE_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/vermagic.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <redirfs.h>
#include <linux/device.h>
#if !defined(USE_TRYTOFREEZEVOID)
#include <linux/suspend.h>
#endif
#ifdef LINUX_USE_FREEZER_H
#include <linux/freezer.h>
#endif
#include <asm/uaccess.h>


#ifndef DAZUKO_DM
#define DAZUKO_DM 0
#endif


ssize_t linux_dazuko_device_read(struct file *, char __user *, size_t, loff_t *);
ssize_t linux_dazuko_device_write(struct file *, const char __user *, size_t, loff_t *);
int linux_dazuko_device_open(struct inode *, struct file *);
int linux_dazuko_device_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long param);
int linux_dazuko_device_release(struct inode *, struct file *);

#ifndef WITHOUT_UDEV
#ifdef USE_CLASS
static struct class *dazuko_class = NULL;
#else
static struct class_simple *dazuko_class = NULL;
#endif
#endif

static int dev_major = -1;
static int module_disabled = 0;

enum redirfs_rv dazukoflt_open(redirfs_context context, struct redirfs_args *args);

static redirfs_filter dazukoflt;

static struct redirfs_filter_info dazukoflt_info = {
	.owner = THIS_MODULE,
	.name = "dazukoflt",
	.priority = 860000000,
	.active = 1,
};

static struct redirfs_op_info dazukoflt_op_info[] = {
	{ REDIRFS_REG_FOP_OPEN, dazukoflt_open, NULL },
	{ REDIRFS_OP_END, NULL, NULL }
};

static struct file_operations	fops = {
		.owner		= THIS_MODULE,
		.read		= linux_dazuko_device_read,
		.write		= linux_dazuko_device_write,
		.ioctl		= linux_dazuko_device_ioctl,
		.open		= linux_dazuko_device_open,
		.release	= linux_dazuko_device_release,
	};


/* mutex */

inline void xp_init_mutex(struct xp_mutex *mutex)
{
	init_MUTEX(&(mutex->mutex));
}

inline void xp_down(struct xp_mutex *mutex)
{
	down(&(mutex->mutex));
}

inline void xp_up(struct xp_mutex *mutex)
{
	up(&(mutex->mutex));
}

inline void xp_destroy_mutex(struct xp_mutex *mutex)
{
}


/* read-write lock */

inline void xp_init_rwlock(struct xp_rwlock *rwlock)
{
	rwlock_init(&(rwlock->rwlock));
}

inline void xp_write_lock(struct xp_rwlock *rwlock)
{
	write_lock(&(rwlock->rwlock));
}

inline void xp_write_unlock(struct xp_rwlock *rwlock)
{
	write_unlock(&(rwlock->rwlock));
}

inline void xp_read_lock(struct xp_rwlock *rlock)
{
	read_lock(&(rlock->rwlock));
}

inline void xp_read_unlock(struct xp_rwlock *rlock)
{
	read_unlock(&(rlock->rwlock));
}

inline void xp_destroy_rwlock(struct xp_rwlock *rwlock)
{
}


/* wait-notify queue */

inline int xp_init_queue(struct xp_queue *queue)
{
	init_waitqueue_head(&(queue->queue));
	return 0;
}

inline int xp_wait_until_condition(struct xp_queue *queue, int (*cfunction)(void *), void *cparam, int allow_interrupt)
{
	/* wait until cfunction(cparam) != 0 (condition is true) */
	int	ret = 0;

	if (allow_interrupt)
	{
		while (1)
		{
			ret = wait_event_interruptible(queue->queue, cfunction(cparam) != 0);

#if defined (USE_TRYTOFREEZEVOID)
			if (try_to_freeze() == 0)
				break;
#else
			if (current->flags & PF_FREEZE)
			{
				refrigerator(PF_FREEZE);
			}
			else
			{
				break;
			}
#endif
		}
	}
	else
	{
		wait_event(queue->queue, cfunction(cparam) != 0);
	}

	return ret;
}

inline int xp_notify(struct xp_queue *queue)
{
	wake_up(&(queue->queue));
	return 0;
}

inline int xp_destroy_queue(struct xp_queue *queue)
{
	return 0;
}


/* memory */

inline void* xp_malloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

inline int xp_free(void *ptr)
{
	kfree(ptr);
	return 0;
}

inline int xp_copyin(const void *user_src, void *kernel_dest, size_t size)
{
	return copy_from_user(kernel_dest, user_src, size);
}

inline int xp_copyout(const void *kernel_src, void *user_dest, size_t size)
{
	return copy_to_user(user_dest, kernel_src, size);
}

inline int xp_verify_user_writable(const void *user_ptr, size_t size)
{
	return 0;
}

inline int xp_verify_user_readable(const void *user_ptr, size_t size)
{
	return 0;
}


/* path attribute */

inline int xp_is_absolute_path(const char *path)
{
	if (path[0] == '/')
		return 1;

	return 0;
}


/* atomic */

inline int xp_atomic_set(struct xp_atomic *atomic, int value)
{
	atomic_set(&(atomic->atomic), value);
	return 0;
}

inline int xp_atomic_inc(struct xp_atomic *atomic)
{
	atomic_inc(&(atomic->atomic));
	return 0;
}

inline int xp_atomic_dec(struct xp_atomic *atomic)
{
	atomic_dec(&(atomic->atomic));
	return 0;
}

inline int xp_atomic_read(struct xp_atomic *atomic)
{
	return atomic_read(&(atomic->atomic));
}


/* file structure */

static int dazuko_get_full_filename(struct xp_file_struct *xfs)
{
	char *temp;

	if (xfs == NULL)
		return 0;

	if (xfs->inode == NULL)
		return 0;

	if (S_ISDIR(xfs->inode->i_mode))
		return 0;

	if (xfs->nd == NULL || xfs->free_full_filename)
		return 0;
	
#ifdef USE_NDPATH
	if (xfs->nd->path.mnt == NULL || xfs->nd->path.dentry == NULL)
		return 0;
#else
	if (xfs->nd->mnt == NULL || xfs->nd->dentry == NULL)
		return 0;
#endif

	/* check if we need to allocate a buffer */
	if (!xfs->free_page_buffer)
	{
		/* get pre-requisites for d_path function */
		xfs->buffer = (char *)__get_free_page(GFP_USER);

		/* make sure we got a page */
		if (xfs->buffer == NULL)
			return 0;

		/* the buffer will need to be freed */
		xfs->free_page_buffer = 1;
	}

	/* make sure we don't already have a vfsmount */
	if (!xfs->mntput_vfsmount)
	{
#ifdef USE_NDPATH
		xfs->vfsmount = mntget(xfs->nd->path.mnt);
#else
		xfs->vfsmount = mntget(xfs->nd->mnt);
#endif

		/* the vfsmount will need to be put back */
		xfs->mntput_vfsmount = 1;
	}

	/* make sure we don't already have a dentry */
	if (!xfs->dput_dentry)
	{
#ifdef USE_NDPATH
		xfs->dentry = dget(xfs->nd->path.dentry);
#else
		xfs->dentry = dget(xfs->nd->dentry);
#endif

		/* the dentry will need to be put back */
		xfs->dput_dentry = 1;
	}

#ifdef USE_NDPATH
	{
		struct path temp_path = { .mnt = xfs->vfsmount, .dentry = xfs->dentry };
		temp = d_path(&temp_path, xfs->buffer, PAGE_SIZE);
	}
#else
	temp = d_path(xfs->dentry, xfs->vfsmount, xfs->buffer, PAGE_SIZE);
#endif

	/* make sure we really got a new filename */
	if (temp == NULL)
		return 0;
	
	xfs->full_filename_length = dazuko_strlen(temp);

	xfs->full_filename = (char *)xp_malloc(xfs->full_filename_length + 1);
	if (xfs->full_filename == NULL)
		return 0;
	
	/* the char array will need to be freed */
	xfs->free_full_filename = 1;

	memcpy(xfs->full_filename, temp, xfs->full_filename_length + 1);

	/* we have a filename with the full path */

	return 1;
}

static int dazuko_fill_file_struct_cleanup(struct dazuko_file_struct *dfs)
{
	if (dfs == NULL)
		return 0;

	if (dfs->extra_data == NULL)
		return 0;

	if (dfs->extra_data->free_page_buffer)
	{
		free_page((unsigned long)dfs->extra_data->buffer);
		dfs->extra_data->free_page_buffer = 0;
	}

	if (dfs->extra_data->dput_dentry)
	{
		dput(dfs->extra_data->dentry);
		dfs->extra_data->dput_dentry = 0;
	}

	if (dfs->extra_data->mntput_vfsmount)
	{
		mntput(dfs->extra_data->vfsmount);
		dfs->extra_data->mntput_vfsmount = 0;
	}

	return 0;
}

int xp_fill_file_struct(struct dazuko_file_struct *dfs)
{
	int	error = -1;

	if (dfs == NULL)
		return error;

	/* check if filename has already been filled in */
	if (dfs->filename != NULL)
		return 0;

	/* make sure we can get the full path */
	if (dazuko_get_full_filename(dfs->extra_data))
	{
		/* reference copy of full path */
		dfs->filename = dfs->extra_data->full_filename;

		dfs->filename_length = dfs->extra_data->full_filename_length;

		dfs->file_p.size = dfs->extra_data->inode->i_size;
		dfs->file_p.set_size = 1;
		dfs->file_p.uid = dfs->extra_data->inode->i_uid;
		dfs->file_p.set_uid = 1;
		dfs->file_p.gid = dfs->extra_data->inode->i_gid;
		dfs->file_p.set_gid = 1;
		dfs->file_p.mode = dfs->extra_data->inode->i_mode;
		dfs->file_p.set_mode = 1;
		dfs->file_p.device_type = dfs->extra_data->inode->i_rdev;
		dfs->file_p.set_device_type = 1;

		error = 0;
	}

	dazuko_fill_file_struct_cleanup(dfs);

	return error;
}

static int dazuko_file_struct_cleanup(struct dazuko_file_struct **dfs)
{
	if (dfs == NULL)
		return 0;

	if (*dfs == NULL)
		return 0;

	if ((*dfs)->extra_data)
	{
		if ((*dfs)->extra_data->free_full_filename)
			xp_free((*dfs)->extra_data->full_filename);

		xp_free((*dfs)->extra_data);
	}

	xp_free(*dfs);

	*dfs = NULL;

	return 0;
}


/* daemon id */

static inline int check_parent(struct task_struct *parent, struct task_struct *child)
{
	struct task_struct	*ts = child;

	if (parent == NULL || child == NULL)
		return -1;

	while (1)
	{
		if (ts == parent)
			return 0;

		if (ts->parent == NULL)
			break;

		if (ts == ts->parent)
			break;

		ts = ts->parent;
	}

	return -1;
}

inline int xp_id_compare(struct xp_daemon_id *id1, struct xp_daemon_id *id2, int check_related)
{
	if (id1 == NULL || id2 == NULL)
		return DAZUKO_DIFFERENT;

	/* If file's are available we do a special
	 * check ("file"'s are only used by daemons).
	 * Here we allow threads to look like one
	 * instance, if they pass around the handle.
	 * Note: this is a Linux-only "hack" */
	if (id1->file != NULL && id2->file != NULL)
	{
		if (id1->tgid == id2->tgid && id1->files == id2->files && id1->file == id2->file)
			return DAZUKO_SAME;
	}

	if (id1->pid == id2->pid && id1->current_p == id2->current_p && id1->files == id2->files)
		return DAZUKO_SAME;

	if (check_related)
	{
		/* Same thread id and same file descriptors,
		 * looks like they could be the same process...
		 * We will treat two threads of the same process
		 * as the same (for relation checks). This is
		 * useful for the Trusted Application Framework,
		 * if we trust one thread, we can trust them all.*/
		if (id1->tgid == id2->tgid && id1->files == id2->files)
		{
			/* Two different threads of the same process will have different current pointers,
			 * but if process ids match, current pointers must too. */

			if (id1->pid == id2->pid && id1->current_p == id2->current_p)
				return DAZUKO_SAME;

			if (id1->pid != id2->pid && id1->current_p != id2->current_p)
				return DAZUKO_SAME;
		}

		if (check_parent(id1->current_p, id2->current_p) == 0)
		{
			return DAZUKO_CHILD;
		}
		else if (id1->pid == id2->pid || id1->current_p == id2->current_p || id1->files == id2->files)
		{
			return DAZUKO_SUSPICIOUS;
		}
		else if (id1->tgid == id2->tgid)
		{
			return DAZUKO_SUSPICIOUS;
		}
	}

	return DAZUKO_DIFFERENT;
}

inline int xp_id_free(struct xp_daemon_id *id)
{
	xp_free(id);

	return 0;
}

inline struct xp_daemon_id* xp_id_copy(struct xp_daemon_id *id)
{
	struct xp_daemon_id	*ptr;

	if (id == NULL)
		return NULL;

	ptr = (struct xp_daemon_id *)xp_malloc(sizeof(struct xp_daemon_id));

	if (ptr != NULL)
	{
		ptr->pid = id->pid;
		ptr->tgid = id->tgid;
		ptr->file = id->file;
		ptr->current_p = id->current_p;
		ptr->files = id->files;
	}

	return ptr;
}


/* event */

int xp_set_event_properties(struct event_properties *event_p, struct xp_daemon_id *xp_id)
{
	event_p->pid = xp_id->pid;
	event_p->set_pid = 1;

	return 0;
}


/* cache settings */

int xp_init_cache(unsigned long ttl)
{
	return -1;
}


/* include/exclude paths */

int xp_set_path(const char *path, int type)
{
	int err = 0;
	struct redirfs_path_info path_info;
	redirfs_path rfs_path;
	struct nameidata nd;

	switch (type)
	{
		case ADD_INCLUDE_PATH:
			path_info.flags = REDIRFS_PATH_INCLUDE;
			break;

		case ADD_EXCLUDE_PATH:
			path_info.flags = REDIRFS_PATH_EXCLUDE;
			break;

		default:
			return -1;
	}

	if (path_lookup(path, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &nd) != 0)
		return -1;

	path_info.dentry = nd.path.dentry;
	path_info.mnt = nd.path.mnt;
	rfs_path = redirfs_add_path(dazukoflt, &path_info);
	if (IS_ERR(rfs_path)) {
		xp_print("dazuko: failed to set RedirFS path, err=%d\n", err);
		err = -1;
	}

	path_put(&nd.path);
	redirfs_put_path(rfs_path);

	return err;
}


/* system hooks */

int dazuko_sys_generic(struct inode *inode, struct nameidata *nd)
{
	struct dazuko_file_struct *dfs = NULL;
	int error = 0;
	int check_error = 0;
	struct event_properties event_p;
	struct xp_daemon_id xp_id;
	struct slot_list *sl = NULL;
	int event = DAZUKO_ON_OPEN;
	int daemon_is_allowed = 1;

	if (nd == NULL || inode == NULL)
		return 0;

	dazuko_bzero(&event_p, sizeof(event_p));

	xp_id.pid = current->pid;
	xp_id.tgid = current->tgid;
	xp_id.file = NULL;
	xp_id.current_p = current;
	xp_id.files = current->files;

	check_error = dazuko_check_access(event, daemon_is_allowed, &xp_id, &sl);

	if (!check_error)
	{
		event_p.mode = inode->i_mode;
		event_p.set_mode = 1;
		event_p.pid = current->pid;
		event_p.set_pid = 1;
#ifdef task_uid
		event_p.uid = task_uid(current);
#else
		event_p.uid = current->uid;
#endif
		event_p.set_uid = 1;

		dfs = (struct dazuko_file_struct *)xp_malloc(sizeof(struct dazuko_file_struct));
		if (dfs != NULL)
		{
			dazuko_bzero(dfs, sizeof(struct dazuko_file_struct));

			dfs->extra_data = (struct xp_file_struct *)xp_malloc(sizeof(struct xp_file_struct));
			if (dfs->extra_data != NULL)
			{
				dazuko_bzero(dfs->extra_data, sizeof(struct xp_file_struct));

				dfs->extra_data->nd = nd;
				dfs->extra_data->inode = inode;

				error = dazuko_process_access(event, dfs, &event_p, sl);
			}
			else
			{
				xp_free(dfs);
				dfs = NULL;
			}

			dazuko_file_struct_cleanup(&dfs);
		}
	}

	if (error)
		return XP_ERROR_PERMISSION;

	return 0;
}

enum redirfs_rv dazukoflt_open(redirfs_context context, struct redirfs_args *args)
{
	struct nameidata nd;

	nd.path.dentry = args->args.f_open.file->f_dentry;
	nd.path.mnt = args->args.f_open.file->f_vfsmnt;

	if (dazuko_sys_generic(args->args.f_open.file->f_dentry->d_inode, &nd) != 0)
	{
		args->rv.rv_int = -EACCES;
		return REDIRFS_STOP;
	}

	return REDIRFS_CONTINUE;
}

inline int xp_sys_hook()
{
	/* Make sure we have a valid task_struct. */

	if (current == NULL)
	{
		xp_print("dazuko: panic (current == NULL)\n");
		return -1;
	}
	if (current->fs == NULL)
	{
		xp_print("dazuko: panic (current->fs == NULL)\n");
		return -1;
	}

	{
		int err;

		dazukoflt = redirfs_register_filter(&dazukoflt_info);
		if (IS_ERR(dazukoflt))
		{
			xp_print("dazuko: unable to register with RedirFS, err=%d\n", PTR_ERR(dazukoflt));
			return -1;
		}

		err = redirfs_set_operations(dazukoflt, dazukoflt_op_info);
		if (err)
		{
			xp_print("dazuko: unable to set RedirFS options, err=%d\n", err);
			err = redirfs_unregister_filter(dazukoflt);
			if (err)
			{
				xp_print("dazuko: unable to unregister from RedirFS, err=%d\n", err);
				module_disabled = 1;
				xp_print("dazuko: WARNING: dazuko is loaded, but disabled\n");
				return 0;
			}

			redirfs_delete_filter(dazukoflt);
			return -1;
		}
	}

	dev_major = register_chrdev(DAZUKO_DM, DEVICE_NAME, &fops);
	if (dev_major < 0)
	{
		xp_print("dazuko: unable to register device, err=%d\n", dev_major);
		{
			int err;

			err = redirfs_unregister_filter(dazukoflt);
			if (err)
			{
				xp_print("dazuko: unable to unregister from RedirFS, err=%d\n", err);
				module_disabled = 1;
				xp_print("dazuko: WARNING: dazuko is loaded, but disabled\n");
				return 0;
			}
		}
		redirfs_delete_filter(dazukoflt);
		return dev_major;
	}

	dazuko_class = class_create(THIS_MODULE, DEVICE_NAME);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	device_create(dazuko_class, NULL, MKDEV(dev_major, 0), DEVICE_NAME);
#else
	device_create(dazuko_class, NULL, MKDEV(dev_major, 0), NULL, DEVICE_NAME);
#endif

	return 0;
}

inline int xp_sys_unhook()
{
	if (!module_disabled) {
		device_destroy(dazuko_class, MKDEV(dev_major, 0));
		class_destroy(dazuko_class);
		unregister_chrdev(dev_major, DEVICE_NAME);
	}

	redirfs_delete_filter(dazukoflt);

	return 0;
}


/* output */

int xp_print(const char *fmt, ...)
{
	va_list args;
	char *p;
	size_t size = 1024;
	int length;

	p = (char *)xp_malloc(size);
	if (p == NULL)
		return -1;

	length = dazuko_strlen(KERN_INFO);

	memcpy(p, KERN_INFO, length);

	va_start(args, fmt);
	vsnprintf(p + length, size - length, fmt, args);
	va_end(args);

	p[size-1] = 0;

	printk(p);

	xp_free(p);

	return 0;
}


/* ioctl's */

int linux_dazuko_device_open(struct inode *inode, struct file *file)
{
	DPRINT(("dazuko: linux_dazuko_device_open() [%d]\n", current->pid));

	return 0;
}

ssize_t linux_dazuko_device_read(struct file *file, char *buffer, size_t length, loff_t *pos)
{
	/* Reading from the dazuko device simply
	 * returns the device number. This is to
	 * help out the daemon. */

	char    tmp[20];
	size_t  dev_major_len;

	DPRINT(("dazuko: linux_dazuko_device_read() [%d]\n", current->pid));
	
	if (*pos != 0)
		return 0;

	if (dev_major < 0)
		return XP_ERROR_NODEVICE;

	/* print dev_major to a string
	 * and get length (with terminator) */
	dazuko_bzero(tmp, sizeof(tmp));

	dev_major_len = dazuko_snprintf(tmp, sizeof(tmp), "%d", dev_major) + 1;

	if (tmp[sizeof(tmp)-1] != 0)
	{
		xp_print("dazuko: failing device_read, device number overflow for dameon %d (dev_major=%d)\n", current->pid, dev_major);
		return XP_ERROR_FAULT;
	}

	if (length < dev_major_len)
		return XP_ERROR_INVALID;

	/* copy dev_major string to userspace */
	if (xp_copyout(tmp, buffer, dev_major_len) != 0)
		return XP_ERROR_FAULT;

	*pos += dev_major_len;

	return dev_major_len;
}

ssize_t linux_dazuko_device_write(struct file *file, const char *buffer, size_t length, loff_t *pos)
{
	struct xp_daemon_id	xp_id;
	char			tmpbuffer[32];
	int			size;

	size = length;
	if (length >= sizeof(tmpbuffer))
		size = sizeof(tmpbuffer) -1;

	/* copy request pointer string to kernelspace */
	if (xp_copyin(buffer, tmpbuffer, size) != 0)
		return XP_ERROR_FAULT;

	tmpbuffer[size] = 0;

	xp_id.pid = current->pid;
	xp_id.tgid = current->tgid;
	xp_id.file = file;
	xp_id.current_p = current;
	xp_id.files = current->files;

	if (dazuko_handle_user_request(tmpbuffer, &xp_id) == 0)
		return size;

	return XP_ERROR_INTERRUPT;
}

int linux_dazuko_device_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long param)
{
	/* A daemon uses this function to interact with
	 * the kernel. A daemon can set scanning parameters,
	 * give scanning response, and get filenames to scan. */

	struct xp_daemon_id	xp_id;
	int			error = 0;

	if (param == 0)
	{
		xp_print("dazuko: error: linux_dazuko_device_ioctl(..., 0)\n");
		return XP_ERROR_INVALID;
	}

	xp_id.pid = current->pid;
	xp_id.tgid = current->tgid;
	xp_id.file = file;
	xp_id.current_p = current;
	xp_id.files = current->files;

	error = dazuko_handle_user_request_compat1((void *)param, _IOC_NR(cmd), &xp_id);

	if (error != 0)
	{
		/* general error occurred */

		return XP_ERROR_PERMISSION;
	}

	return error;
}

int linux_dazuko_device_release(struct inode *inode, struct file *file)
{
	struct xp_daemon_id	xp_id;

	DPRINT(("dazuko: dazuko_device_release() [%d]\n", current->pid));

	xp_id.pid = current->pid;
	xp_id.tgid = current->tgid;
	xp_id.file = file;
	xp_id.current_p = current;
	xp_id.files = current->files;

	return dazuko_unregister_daemon(&xp_id);
}


/* init/exit */

static int __init linux_dazuko_init(void)
{
	return dazuko_init();
}

static void __exit linux_dazuko_exit(void)
{
	dazuko_exit();
}


MODULE_AUTHOR("John Ogness <dazukocode@ogness.net>");
MODULE_DESCRIPTION("allow 3rd-party file access control");
MODULE_LICENSE("GPL");
MODULE_INFO(vermagic, VERMAGIC_STRING);

module_init(linux_dazuko_init);
module_exit(linux_dazuko_exit);
