#include <reent.h>
#include <rthw.h>
#include <rtthread.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>

#ifdef RT_USING_HEAP /* Memory routine */
void *_malloc_r(struct _reent *ptr, size_t size)
{
    void *result;

    result = (void*)rt_malloc(size);
    if (result == RT_NULL)
    {
        ptr->_errno = ENOMEM;
    }

    return result;
}

void *_realloc_r(struct _reent *ptr, void *old, size_t newlen)
{
    void *result;

    result = (void*)rt_realloc(old, newlen);
    if (result == RT_NULL)
    {
        ptr->_errno = ENOMEM;
    }

    return result;
}

void *_calloc_r(struct _reent *ptr, size_t size, size_t len)
{
    void *result;

    result = (void*)rt_calloc(size, len);
    if (result == RT_NULL)
    {
        ptr->_errno = ENOMEM;
    }

    return result;
}

void _free_r(struct _reent *ptr, void *addr)
{
    rt_free(addr);
}

#else
void *
_sbrk_r(struct _reent *ptr, ptrdiff_t incr)
{
    LOG_E("Please enable RT_USING_HEAP");
    RT_ASSERT(0);
    return RT_NULL;
}
#endif /*RT_USING_HEAP*/

/* Stub implementations for newlib syscalls */
__attribute__((used)) int _close(int file)
{
    return -1;
}

__attribute__((used)) int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

__attribute__((used)) int _isatty(int file)
{
    return 1;
}

__attribute__((used)) int _lseek(int file, int ptr, int dir)
{
    return 0;
}

__attribute__((used)) int _read(int file, char *ptr, int len)
{
    return 0;
}

__attribute__((used)) int _write(int file, char *ptr, int len)
{
    int i;

    (void)file;
    if (ptr == RT_NULL || len <= 0)
    {
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        char ch[2] = {ptr[i], '\0'};
        rt_hw_console_output(ch);
    }

    return len;
}

__attribute__((used)) void _exit(int status)
{
    while (1);
}

__attribute__((used)) int _kill(int pid, int sig)
{
    return -1;
}

__attribute__((used)) int _getpid(void)
{
    return 1;
}
