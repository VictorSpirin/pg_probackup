/* vim: set expandtab autoindent cindent ts=4 sw=4 sts=4 */
#ifndef FU_UTIL_H
#define FU_UTIL_H

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
/* trick to find ssize_t even on windows and strict ansi mode */
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif
#include <memory.h>
#include <limits.h>
#include <fm_util.h>
#include <stdarg.h>


#ifdef __GNUC__
#define ft_gcc_const __attribute__((const))
#define ft_gcc_pure __attribute__((pure))
#if __GNUC__ > 10
#define ft_gcc_malloc(free, idx) __attribute__((malloc, malloc(free, idx)))
#else
#define ft_gcc_malloc(free, idx) __attribute__((malloc))
#endif
#define ft_unused __attribute__((unused))
#define ft_gnu_printf(fmt, arg) __attribute__((format(printf,fmt,arg)))
#define ft_likely(x)    __builtin_expect(!!(x), 1)
#define ft_unlikely(x)  __builtin_expect(!!(x), 0)
#else
#define ft_gcc_const
#define ft_gcc_pure
#define ft_gcc_malloc(free, idx)
#define ft_unused
#define ft_gnu_printf(fmt, arg)
#define ft_likely(x)    (x)
#define ft_unlikely(x)  (x)
#endif
#define ft_inline static ft_unused inline

#if defined(__GNUC__) && !defined(__clang__)
#define ft_optimize3    __attribute__((optimize(3)))
#else
#define ft_optimize3
#endif

#if __STDC_VERSION__ >= 201112L
#elif defined(__GNUC__)
#define _Noreturn __attribute__((__noreturn__))
#else
#define _Noreturn
#endif

/* Logging and asserts */

#if defined(__GNUC__) && !defined(__clang__)
#define ft_FUNC __PRETTY_FUNCTION__
#else
#define ft_FUNC __func__
#endif

typedef struct ft_source_position {
    const char *file;
    int line;
    const char *func;
} ft_source_position_t;

#define ft__srcpos() ((ft_source_position_t){.file=__FILE__,.line=__LINE__,.func=ft_FUNC})

enum FT_LOG_LEVEL {
    FT_UNINITIALIZED = -100,
    FT_DEBUG    = -2,
    FT_LOG      = -1,
    FT_INFO     = 0,
    FT_WARNING  = 1,
    FT_ERROR    = 2,
    FT_OFF      = 3,
    FT_FATAL    = 98,
    FT_TRACE    = 100 /* for active debugging only */
};

enum FT_ASSERT_LEVEL { FT_ASSERT_RUNTIME = 0, FT_ASSERT_ALL };

ft_inline const char* ft_log_level_str(enum FT_LOG_LEVEL level);

/*
 * Hook to plugin external logging.
 * Default loggin writes to stderr only.
 */
ft_gnu_printf(4,0)
extern void (*ft_log_hook)(enum FT_LOG_LEVEL, ft_source_position_t srcpos, const char* error, const char *fmt, va_list args);

/* Reset log level for all files */
extern void ft_log_level_reset(enum FT_LOG_LEVEL level);
extern void ft_assert_level_reset(enum FT_ASSERT_LEVEL level);
/* Adjust log level for concrete file or all files */
extern void ft_log_level_set(const char *file, enum FT_LOG_LEVEL level);
extern void ft_assert_level_set(const char *file, enum FT_ASSERT_LEVEL level);

/* register source for fine tuned logging */
#define ft_register_source()   ft__register_source_impl()

/* log simple message */
#define ft_log(level, fmt_or_msg, ...) \
    ft__log_impl(level, NULL, fmt_or_msg, __VA_ARGS__)
/* log message with error. Error will be appended as ": %s". */
#define ft_logerr(level, error, fmt_or_msg, ...) \
    ft__log_impl(level, error, fmt_or_msg, __VA_ARGS__)

/*
 * Assertions uses standard logging for output.
 * Assertions are runtime enabled:
 * - ft_assert is enabled always.
 * - ft_dbg_assert is disabled be default, but will be enabled if `ft_assert_level` is set positive.
 */

#define ft_dbg_enabled()        ft__dbg_enabled()
#define ft_dbg_assert(x, ...)   ft__dbg_assert(x, #x, __VA_ARGS__)
#define ft_assert(x, ...)       ft__assert(x, #x, __VA_ARGS__)
#define ft_assyscall(syscall, ...)  ft__assyscall(syscall, fm_uniq(res), __VA_ARGS__)

/* threadsafe strerror */
extern const char* ft__strerror(int eno, char *buf, size_t len);
#ifndef __TINYC__
extern const char* ft_strerror(int eno);
#else
#define ft_strerror(eno) ft__strerror(eno, (char[256]){0}, 256)
#endif

// Memory

// Standartize realloc(p, 0)
// Realloc implementations differ in handling newsz == 0 case:
// some returns NULL, some returns unique allocation.
// This version always returns NULL.
extern void* ft_realloc(void* ptr, size_t new_sz);
extern void* ft_calloc(size_t sz);
extern void* ft_realloc_arr(void* ptr, size_t elem_sz, size_t old_elems, size_t new_elems);

#define ft_malloc(sz)           ft_realloc(NULL, (sz))
#define ft_malloc_arr(sz, cnt)  ft_realloc(NULL, ft_mul_size((sz), (cnt)))
#define ft_free(ptr)            ft_realloc((ptr), 0)
#define ft_calloc_arr(sz, cnt)  ft_calloc(ft_mul_size((sz), (cnt)))

extern void ft_set_allocators(
        void *(*_realloc)(void *, size_t),
        void (*_free)(void*));

/* overflow checking size addition and multiplication */
ft_inline size_t ft_add_size(size_t a, size_t b);
ft_inline size_t ft_mul_size(size_t a, size_t b);

#define ft_new(type)        ft_calloc(sizeof(type))
#define ft_newar(type, cnt) ft_calloc(ft_mul_size(sizeof(type), (cnt)))

// Function to clear freshly allocated memory
extern void  ft_memzero(void* ptr, size_t sz);

// Comparison

/* ft_max - macro-safe calculation of maximum */
#define ft_max(a_, b_) ft__max((a_), (b_), fm_uniq(a), fm_uniq(b))
/* ft_min - macro-safe calculation of minimum */
#define ft_min(a_, b_) ft__min((a_), (b_), fm_uniq(a), fm_uniq(b))

typedef enum FT_CMP_RES {
    FT_CMP_LT  = -1,
    FT_CMP_EQ  =  0,
    FT_CMP_GT  =  1,
    FT_CMP_NE  =  2,
} FT_CMP_RES;
/* ft_cmp - macro-safe comparison */
#define ft_cmp(a_, b_) ft__cmp((a_), (b_), fm_uniq(a), fm_uniq(b))
/* ft_swap - macro-safe swap of variables */
#define ft_swap(a_, b_) ft__swap((a_), (b_), fm_uniq(ap), fm_uniq(bp), fm_uniq(t))

/* ft_arrsz - geterminze size of static array */
#define ft_arrsz(ar) (sizeof(ar)/sizeof(ar[0]))

/* used in ft_*_foreach iterations to close implicit scope */
#define ft_end_foreach } while(0)

// String utils

/* dup string using ft_malloc */
ft_inline char * ft_strdup(const char *str);
/* print string into ft_malloc-ed buffer */
extern char* ft_asprintf(const char *fmt, ...) ft_gnu_printf(1,2);
extern char* ft_vasprintf(const char *fmt, va_list args) ft_gnu_printf(1,0);
/*
 * Concat strings regarding destination buffer size.
 * Note: if dest already full and doesn't contain \0n character, then fatal log is issued.
 */
extern size_t ft_strlcat(char *dest, const char* src, size_t dest_size);

// Some Numeric Utils

ft_inline uint32_t ft_rol32(uint32_t x, unsigned n);
ft_inline uint32_t ft_ror32(uint32_t x, unsigned n);

/*
 * Simple inline murmur hash implementation hashing a 32 bit integer, for
 * performance.
 */
ft_inline uint32_t ft_mix32(uint32_t data);


/* Dumb quality random */
extern uint32_t ft_rand(void);
/* Dumb quality random 0<=r<mod */
ft_inline uint32_t ft_randn(uint32_t mod);

/* hash for small c strings */
extern uint32_t ft_small_cstr_hash(const char *key);

/* Time */
extern double ft_time(void);

/* ARGUMENT */

/*
 * Type for *_r callback functions argument.
 * Could be one of type:
 *  z - value-less
 *  p - `void*` pointer
 *  s - `char*` pointer
 *  i - `int64_t`
 *  u - `uint64_t`
 *  d - `double`
 *  b - `bool`
 */
typedef struct ft_arg ft_arg_t;

/* make value */
ft_inline ft_arg_t ft_mka_z(void);
ft_inline ft_arg_t ft_mka_p(void*    p);
ft_inline ft_arg_t ft_mka_s(char*    s);
ft_inline ft_arg_t ft_mka_i(int64_t  i);
ft_inline ft_arg_t ft_mka_u(uint64_t u);
ft_inline ft_arg_t ft_mka_d(double   d);
ft_inline ft_arg_t ft_mka_b(bool     b);

/* type of value */
ft_inline char     ft_arg_type(ft_arg_t v);

/* get value */
ft_inline void     ft_arg_z(ft_arg_t v);
ft_inline void*    ft_arg_p(ft_arg_t v);
ft_inline char*    ft_arg_s(ft_arg_t v);
ft_inline int64_t  ft_arg_i(ft_arg_t v);
ft_inline uint64_t ft_arg_u(ft_arg_t v);
ft_inline double   ft_arg_d(ft_arg_t v);
ft_inline bool     ft_arg_b(ft_arg_t v);

/* SLICES */

/* Value to indicate end of slice in _slice operations */
static const ssize_t FT_SLICE_END = (-SSIZE_MAX-1);

/* Action in walk callback */
typedef enum FT_WALK_ACT {
    FT_WALK_CONT        = 0,
    FT_WALK_BREAK       = 1,
    FT_WALK_DEL         = 2,
    FT_WALK_DEL_BREAK   = FT_WALK_BREAK | FT_WALK_DEL,
} FT_WALK_ACT;

/* Variable initialization fields */
#define ft_slc_init() { .ptr = NULL, .len = 0 }
#define ft_arr_init() { .ptr = NULL, .len = 0, .cap = 0 }

/*
 * Transform slice or array into pointer and length pair.
 * It evaluates argument twice, so use it carefully.
 */
#define ft_2ptrlen(slice_or_array) (slice_or_array).ptr, (slice_or_array).len

/* Result if binary search */
typedef struct ft_bsearch_result {
    size_t  ix; /* index of first greater or equal element */
    bool    eq; /* is element equal or not */
} ft_bsres_t;

#include "./impl/ft_impl.h"

/* Include some examples for search and sort usages */
//#include "./ft_ss_examples.h"
//#include "./ft_ar_examples.h"

#endif
