#ifndef MACROS_H
#define MACROS_H

#include "platform_info.h"

#ifndef __sgi
#define GLOBAL_ASM(...)
#endif

#if !defined(__sgi) && (!defined(NON_MATCHING) || !defined(AVOID_UB))
// asm-process isn't supported outside of IDO, and undefined behavior causes
// crashes.
#error Matching build is only possible on IDO; please build with NON_MATCHING=1.
#endif

#define ARRAY_COUNT(arr) (s32)(sizeof(arr) / sizeof(arr[0]))

#define GLUE(a, b) a ## b
#define GLUE2(a, b) GLUE(a, b)

// Avoid compiler warnings for unused variables
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

// Avoid undefined behaviour for non-returning functions
#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

// Static assertions
#ifdef __GNUC__
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg) typedef char GLUE2(static_assertion_failed, __LINE__)[(cond) ? 1 : -1]
#endif

// Align to 8-byte boundary for DMA requirements
#ifdef __GNUC__
#define ALIGNED8 __attribute__((aligned(8)))
#else
#define ALIGNED8
#endif

// Align to 16-byte boundary for audio lib requirements
#ifdef __GNUC__
#define ALIGNED16 __attribute__((aligned(16)))
#else
#define ALIGNED16
#endif

#ifndef NO_SEGMENTED_MEMORY
// convert a virtual address to physical.
#define VIRTUAL_TO_PHYSICAL(addr)   ((uintptr_t)(addr) & 0x1FFFFFFF)

// convert a physical address to virtual.
#define PHYSICAL_TO_VIRTUAL(addr)   ((uintptr_t)(addr) | 0x80000000)

// another way of converting virtual to physical
#define VIRTUAL_TO_PHYSICAL2(addr)  ((u8 *)(addr) - 0x80000000U)
#else
// no conversion needed other than cast
#define VIRTUAL_TO_PHYSICAL(addr)   ((uintptr_t)(addr))
#define PHYSICAL_TO_VIRTUAL(addr)   ((uintptr_t)(addr))
#define VIRTUAL_TO_PHYSICAL2(addr)  ((void *)(addr))
#endif

// Repeats the macro `ACTION(N)` `N` times (one per line).
// Needed by DynOS gfx parsing (ported from Co-op DX).
#define REPEAT_0(ACTION)
#define REPEAT_1(ACTION) REPEAT_0(ACTION) ACTION(1)
#define REPEAT_2(ACTION) REPEAT_1(ACTION) ACTION(2)
#define REPEAT_3(ACTION) REPEAT_2(ACTION) ACTION(3)
#define REPEAT_4(ACTION) REPEAT_3(ACTION) ACTION(4)
#define REPEAT_5(ACTION) REPEAT_4(ACTION) ACTION(5)
#define REPEAT_6(ACTION) REPEAT_5(ACTION) ACTION(6)
#define REPEAT_7(ACTION) REPEAT_6(ACTION) ACTION(7)
#define REPEAT_8(ACTION) REPEAT_7(ACTION) ACTION(8)
#define REPEAT_9(ACTION) REPEAT_8(ACTION) ACTION(9)
#define REPEAT(ACTION, N) REPEAT_##N(ACTION)

// Expands to a comma-separated list of arguments.
#define LIST_ARGS_0(ACTION)
#define LIST_ARGS_1(ACTION) LIST_ARGS_0(ACTION) ACTION(1)
#define LIST_ARGS_2(ACTION) LIST_ARGS_1(ACTION), ACTION(2)
#define LIST_ARGS_3(ACTION) LIST_ARGS_2(ACTION), ACTION(3)
#define LIST_ARGS_4(ACTION) LIST_ARGS_3(ACTION), ACTION(4)
#define LIST_ARGS_5(ACTION) LIST_ARGS_4(ACTION), ACTION(5)
#define LIST_ARGS_6(ACTION) LIST_ARGS_5(ACTION), ACTION(6)
#define LIST_ARGS_7(ACTION) LIST_ARGS_6(ACTION), ACTION(7)
#define LIST_ARGS_8(ACTION) LIST_ARGS_7(ACTION), ACTION(8)
#define LIST_ARGS_9(ACTION) LIST_ARGS_8(ACTION), ACTION(9)
#define LIST_ARGS(ACTION, N) LIST_ARGS_##N(ACTION)

#endif // MACROS_H
