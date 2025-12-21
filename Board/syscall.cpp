#include <rtthread.h>
#include <new>
#include <stddef.h>

void* operator new(size_t size)
{
    return rt_malloc(size);
}

void* operator new[](size_t size)
{
    return rt_malloc(size);
}

void operator delete(void* ptr) noexcept
{
    rt_free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    rt_free(ptr);
}

void operator delete(void* ptr, size_t size) noexcept
{
    rt_free(ptr);
}

void operator delete[](void* ptr, size_t size) noexcept
{
    rt_free(ptr);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
    return rt_malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
    return rt_malloc(size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    rt_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    rt_free(ptr);
}