#include <stddef.h>
#include <reent.h>

void *ytrace_rt_malloc_r(struct _reent *ptr, size_t size);
void *ytrace_rt_realloc_r(struct _reent *ptr, void *old, size_t newlen);
void *ytrace_rt_calloc_r(struct _reent *ptr, size_t size, size_t len);
void ytrace_rt_free_r(struct _reent *ptr, void *addr);

void *__wrap__malloc_r(struct _reent *ptr, size_t size)
{
    return ytrace_rt_malloc_r(ptr, size);
}

void *__wrap__realloc_r(struct _reent *ptr, void *old, size_t newlen)
{
    return ytrace_rt_realloc_r(ptr, old, newlen);
}

void *__wrap__calloc_r(struct _reent *ptr, size_t size, size_t len)
{
    return ytrace_rt_calloc_r(ptr, size, len);
}

void __wrap__free_r(struct _reent *ptr, void *addr)
{
    ytrace_rt_free_r(ptr, addr);
}
