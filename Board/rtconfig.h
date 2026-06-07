/* RT-Thread config file */

#ifndef __RTTHREAD_CFG_H__
#define __RTTHREAD_CFG_H__

#include <rtthread.h>
#include "finsh_config.h"

//rtthread线程管理最大优先级
#define RT_THREAD_PRIORITY_MAX  32

//rtthread 系统时钟频率
#define RT_TICK_PER_SECOND  1000

//rtthread字节对齐大小
#define RT_ALIGN_SIZE   4

//rtthread设备名最大长度
#define RT_NAME_MAX    8

//rtthread使能自动初始化
#define RT_USING_COMPONENTS_INIT

//用户使用main线程
#define RT_USING_USER_MAIN

//main线程堆栈大小
#define RT_MAIN_THREAD_STACK_SIZE     1024

//使能rtthread的内核LOG功能
//#define RT_DEBUG

//输出自动初始化的组件信息
#define RT_DEBUG_INIT 0

//使能rtthread的堆栈溢出检测
//#define RT_USING_OVERFLOW_CHECK

//使能rtthread的钩子函数
//#define RT_USING_HOOK

//使能rtthread的空闲线程钩子函数
//#define RT_USING_IDLE_HOOK

//软件定时器
#define RT_USING_TIMER_SOFT         0
#if RT_USING_TIMER_SOFT == 0
    #undef RT_USING_TIMER_SOFT
#endif

//软件定时器线程信息
#define RT_TIMER_THREAD_PRIO        4
#define RT_TIMER_THREAD_STACK_SIZE  512


//rtthread ICP内容
#define RT_USING_SEMAPHORE	//信号量
#define RT_USING_MUTEX	//互斥量
//#define RT_USING_EVENT	//事件
#define RT_USING_MAILBOX	//邮箱
//#define RT_USING_MESSAGEQUEUE //消息队列

#define RT_USING_HEAP		//堆内存
#define RT_USING_SMALL_MEM //小内存
//#define RT_USING_TINY_SIZE

#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE          1024

#if defined(RT_USING_FINSH)
    #define FINSH_USING_MSH
    #define FINSH_USING_MSH_ONLY
    //#define __FINSH_THREAD_PRIORITY     5
    //#define FINSH_THREAD_PRIORITY       (RT_THREAD_PRIORITY_MAX / 8 * __FINSH_THREAD_PRIORITY + 1)
    #define FINSH_THREAD_STACK_SIZE     8192

    #define FINSH_HISTORY_LINES         1

    #define FINSH_USING_SYMTAB

#endif

#define RT_USING_ULOG
#define DBG_ENABLE

//Enable J-Link RTT output for RT-Thread console and ulog.
#define Y_TRACE_USING_RTT_CONSOLE
#ifndef Y_TRACE_RTT_UP_BUFFER_SIZE
#define Y_TRACE_RTT_UP_BUFFER_SIZE 1024U
#endif

#endif
