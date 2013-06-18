/* Dazuko. Check parameters of XP calls before making real calls.
   Written by John Ogness <dazukocode@ogness.net>

   Copyright (c) 2003, 2004, 2005 H+BEDV Datentechnik GmbH
   Copyright (c) 2006 Avira GmbH
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   3. Neither the name of Dazuko nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DAZUKO_CALL_H
#define DAZUKO_CALL_H

#include "dazuko_platform.h"

#include "dazuko_core.h"

struct xp_mutex;
struct xp_rwlock;
struct xp_queue;
struct xp_atomic;
struct dazuko_file_struct;
struct xp_daemon_id;

#define call_xp_sys_hook xp_sys_hook
#define call_xp_sys_unhook xp_sys_unhook
#define call_xp_print xp_print


/* mutex */

static inline void call_xp_init_mutex(struct xp_mutex *mutex)
{
	if (mutex == NULL)
	{
		xp_print("dazuko: xp_init_mutex(NULL)\n");
		return;
	}

	xp_init_mutex(mutex);
}

static inline void call_xp_down(struct xp_mutex *mutex)
{
	if (mutex == NULL)
	{
		xp_print("dazuko: warning: xp_down(NULL)\n");
		return;
	}

	xp_down(mutex);
}

static inline void call_xp_up(struct xp_mutex *mutex)
{
	if (mutex == NULL)
	{
		xp_print("dazuko: warning: xp_up(NULL)\n");
		return;
	}

	xp_up(mutex);
}

static inline void call_xp_destroy_mutex(struct xp_mutex *mutex)
{
	if (mutex == NULL)
	{
		xp_print("dazuko: warning: xp_destroy_mutex(NULL)\n");
		return;
	}

	xp_destroy_mutex(mutex);
}


/* read-write lock */

static inline void call_xp_init_rwlock(struct xp_rwlock *rwlock)
{
	if (rwlock == NULL)
	{
		xp_print("dazuko: warning: xp_init_rwlock(NULL)\n");
		return;
	}

	xp_init_rwlock(rwlock);
}

static inline void call_xp_write_lock(struct xp_rwlock *rwlock)
{
	if (rwlock == NULL)
	{
		xp_print("dazuko: warning: xp_write_lock(NULL)\n");
		return;
	}

	xp_write_lock(rwlock);
}

static inline void call_xp_write_unlock(struct xp_rwlock *rwlock)
{
	if (rwlock == NULL)
	{
		xp_print("dazuko: warning: xp_write_unlock(NULL)\n");
		return;
	}

	xp_write_unlock(rwlock);
}

static inline void call_xp_read_lock(struct xp_rwlock *rlock)
{
	if (rlock == NULL)
	{
		xp_print("dazuko: warning: xp_read_lock(NULL)\n");
		return;
	}

	xp_read_lock(rlock);
}

static inline void call_xp_read_unlock(struct xp_rwlock *rlock)
{
	if (rlock == NULL)
	{
		xp_print("dazuko: warning: xp_read_unlock(NULL)\n");
		return;
	}

	xp_read_unlock(rlock);
}

static inline void call_xp_destroy_rwlock(struct xp_rwlock *rwlock)
{
	if (rwlock == NULL)
	{
		xp_print("dazuko: warning: xp_destroy_rwlock(NULL)\n");
		return;
	}

	xp_destroy_rwlock(rwlock);
}


/* wait-notify queue */

static inline int call_xp_init_queue(struct xp_queue *queue)
{
	if (queue == NULL)
	{
		xp_print("dazuko: warning: xp_init_queue(NULL)\n");
		return -1;
	}

	return xp_init_queue(queue);
}

static inline int call_xp_wait_until_condition(struct xp_queue *queue, int (*cfunction)(void *), void *cparam, int allow_interrupt)
{
	if (queue == NULL)
	{
		xp_print("dazuko: warning: xp_wait_until_condition(queue=NULL)\n");
		return -1;
	}

	if (cfunction == NULL)
	{
		xp_print("dazuko: warning: xp_wait_until_condition(cfunction=NULL)\n");
		return -1;
	}

	return xp_wait_until_condition(queue, cfunction, cparam, allow_interrupt);
}

static inline int call_xp_notify(struct xp_queue *queue)
{
	if (queue == NULL)
	{
		xp_print("dazuko: warning: xp_notify(NULL)\n");
		return -1;
	}

	return xp_notify(queue);
}

static inline int call_xp_destroy_queue(struct xp_queue *queue)
{
	if (queue == NULL)
	{
		xp_print("dazuko: warning: xp_destroy_queue(NULL)\n");
		return -1;
	}

	return xp_destroy_queue(queue);
}


/* memory */

static inline void* call_xp_malloc(size_t size)
{
	void *ptr;

	if (size < 1)
	{
		xp_print("dazuko: warning: xp_malloc(%lu)\n", (unsigned long)size);
		return NULL;
	}

	ptr = xp_malloc(size);

	if (ptr == NULL)
		xp_print("dazuko: warning: xp_malloc(%lu) -> NULL\n", (unsigned long)size);

	return ptr;
}

static int call_xp_free(void *ptr)
{
	if (ptr == NULL)
	{
		xp_print("dazuko: warning: xp_free(NULL)\n");
		return 0;
	}

	return xp_free(ptr);
}

static inline int call_xp_copyin(const void *user_src, void *kernel_dest, size_t size)
{
	if (user_src == NULL)
	{
		xp_print("dazuko: warning: xp_copyin(user_src=NULL)\n");
		return -1;
	}

	if (kernel_dest == NULL)
	{
		xp_print("dazuko: warning: xp_copyin(kernel_dest=NULL)\n");
		return -1;
	}

	if (size < 1)
	{
		xp_print("dazuko: warning: xp_copyin(size=%lu)\n", (unsigned long)size);
		return 0;
	}

	return xp_copyin(user_src, kernel_dest, size);
}

static inline int call_xp_copyout(const void *kernel_src, void *user_dest, size_t size)
{
	if (kernel_src == NULL)
	{
		xp_print("dazuko: warning: xp_copyout(kernel_src=NULL)\n");
		return -1;
	}

	if (user_dest == NULL)
	{
		xp_print("dazuko: warning: xp_copyout(user_dest=NULL)\n");
		return -1;
	}

	if (size < 1)
	{
		xp_print("dazuko: warning: xp_copyout(size=%lu)\n", (unsigned long)size);
		return 0;
	}

	return xp_copyout(kernel_src, user_dest, size);
}

static inline int call_xp_verify_user_writable(const void *user_ptr, size_t size)
{
	if (user_ptr == NULL)
	{
		xp_print("dazuko: warning: xp_verify_user_writable(user_ptr=NULL)\n");
		return -1;
	}

	if (size < 1)
	{
		xp_print("dazuko: warning: xp_verify_user_writable(size=%lu)\n", (unsigned long)size);
		return -1;
	}

	return xp_verify_user_writable(user_ptr, size);
}

static inline int call_xp_verify_user_readable(const void *user_ptr, size_t size)
{
	if (user_ptr == NULL)
	{
		xp_print("dazuko: warning: xp_verify_user_readable(user_ptr=NULL)\n");
		return -1;
	}

	if (size < 1)
	{
		xp_print("dazuko: warning: xp_verify_user_readable(size=%lu)\n", (unsigned long)size);
		return -1;
	}

	return xp_verify_user_readable(user_ptr, size);
}


/* path attribute */

static inline int call_xp_is_absolute_path(const char *path)
{
	if (path == NULL)
	{
		xp_print("dazuko: warning: xp_is_absolute_path(NULL)\n");
		return 0;
	}

	return xp_is_absolute_path(path);
}


/* atomic */

static inline int call_xp_atomic_set(struct xp_atomic *atomic, int value)
{
	if (atomic == NULL)
	{
		xp_print("dazuko: warning: xp_atomic_set(atomic=NULL)\n");
		return -1;
	}

	return xp_atomic_set(atomic, value);
}

static inline int call_xp_atomic_inc(struct xp_atomic *atomic)
{
	if (atomic == NULL)
	{
		xp_print("dazuko: warning: xp_atomic_inc(NULL)\n");
		return -1;
	}

	return xp_atomic_inc(atomic);
}

static inline int call_xp_atomic_dec(struct xp_atomic *atomic)
{
	if (atomic == NULL)
	{
		xp_print("dazuko: warning: xp_atomic_dec(NULL)\n");
		return -1;
	}

	return xp_atomic_dec(atomic);
}

static inline int call_xp_atomic_read(struct xp_atomic *atomic)
{
	if (atomic == NULL)
	{
		xp_print("dazuko: warning: xp_atomic_read(NULL)\n");
		return -1;
	}

	return xp_atomic_read(atomic);
}


/* file structure */

static inline int call_xp_fill_file_struct(struct dazuko_file_struct *dfs)
{
	if (dfs == NULL)
	{
		xp_print("dazuko: warning: xp_fill_file_struct(NULL)\n");
		return -1;
	}

	return xp_fill_file_struct(dfs);
}


/* daemon id */

static inline int call_xp_id_compare(struct xp_daemon_id *id1, struct xp_daemon_id *id2, int check_related)
{
	if (id1 == NULL)
	{
		xp_print("dazuko: warning: xp_id_compare(id1=NULL)\n");
		return -1;
	}

	if (id2 == NULL)
	{
		xp_print("dazuko: warning: xp_id_compare(id2=NULL)\n");
		return -1;
	}

	return xp_id_compare(id1, id2, check_related);
}

static inline int call_xp_id_free(struct xp_daemon_id *id)
{
	if (id == NULL)
	{
		xp_print("dazuko: warning: xp_id_free(NULL)\n");
		return 0;
	}

	return xp_id_free(id);
}

static inline struct xp_daemon_id* call_xp_id_copy(struct xp_daemon_id *id)
{
	struct xp_daemon_id *ptr;

	if (id == NULL)
	{
		xp_print("dazuko: warning: xp_id_copy(NULL)\n");
		return NULL;
	}

	ptr = xp_id_copy(id);

	if (ptr == NULL)
		xp_print("dazuko: warning: xp_id_copy() -> NULL\n");

	return ptr;
}


/* event */

static inline int call_xp_set_event_properties(struct event_properties *event_p, struct xp_daemon_id *xp_id)
{
	if (event_p == NULL)
	{
		xp_print("dazuko: warning: xp_set_event_properties(event_p=NULL)\n");
		return -1;
	}

	if (xp_id == NULL)
	{
		xp_print("dazuko: warning: xp_set_event_properties(xp_id=NULL)\n");
		return -1;
	}

	return xp_set_event_properties(event_p, xp_id);
}


/* cache settings */

static int call_xp_init_cache(unsigned long ttl)
{
	return xp_init_cache(ttl);
}


/* include/exclude paths */

static int call_xp_set_path(const char *path, int type)
{
	if (path == NULL)
	{
		xp_print("dazuko: warning: call_xp_set_path(path=NULL)\n");
		return -1;
	}

	return xp_set_path(path, type);
}

#endif
