/* vim: set expandtab autoindent cindent ts=4 sw=4 sts=4 : */
#ifndef FT_IMPL_H
#define FT_IMPL_H

#ifdef __TINYC__

#if defined(__attribute__)
#undef __attribute__
#define __attribute__ __attribute__
#endif

#include <stdatomic.h>
#define __atomic_add_fetch(x, y, z) ft__atomic_add_fetch((x), (y), z, fm_uniq(y))
#define ft__atomic_add_fetch(x, y_, z, y) ({ \
        __typeof(y_) y = y_; \
        __atomic_fetch_add((x), y, z) + y; \
})
#define __atomic_sub_fetch(x, y, z) ft__atomic_sub_fetch((x), (y), z, fm_uniq(y))
#define ft__atomic_sub_fetch(x, y_, z, y) ({ \
        __typeof(y_) y = y_; \
        __atomic_fetch_sub((x), y, z) - y; \
})
#define __atomic_load_n(x, z) __atomic_load((x), z)
#define __atomic_store_n(x, y, z) __atomic_store((x), (y), z)

#endif /* __TINYC__ */

/* Memory */

/* Logging */

static ft_unused inline const char*
ft_log_level_str(enum FT_LOG_LEVEL level) {
    switch (level) {
        case FT_DEBUG:      return "DEBUG";
        case FT_LOG:        return "LOG";
        case FT_INFO:       return "INFO";
        case FT_WARNING:    return "WARNING";
        case FT_ERROR:      return "ERROR";
        case FT_FATAL:      return "FATAL";
        case FT_OFF:        return "OFF";
        case FT_TRACE:      return "TRACE";
        default:            return "UNKNOWN";
    }
}

struct ft_log_and_assert_level {
    enum FT_LOG_LEVEL       log_level;
    enum FT_ASSERT_LEVEL    assert_level;
};

extern struct ft_log_and_assert_level   ft_log_assert_levels;

/* this variable is duplicated in every source as static variable */
static ft_unused
struct ft_log_and_assert_level *ft_local_lgas_levels = &ft_log_assert_levels;

#define ft_will_log(level) (level >= ft_local_lgas_levels->log_level)
extern void ft__register_source(const char *file,
                                struct ft_log_and_assert_level **local_levels);

#if defined(__GNUC__) || defined(__TINYC__)
#define ft__register_source_impl() \
    static __attribute__((constructor)) void \
    ft__register_source_(void) { \
        ft__register_source(__FILE__, &ft_local_lgas_levels); \
    } \
    fm__dumb_require_semicolon
#else
#define ft_register_source_impl() fm__dumb_require_semicolon
#endif

#define COMPARE_FM_FATAL(x) x
#define ft__log_impl(level, error, fmt_or_msg, ...) \
    fm_if(fm_equal(level, FM_FATAL), \
            ft__log_fatal(ft__srcpos(), error, ft__log_fmt_msg(fmt_or_msg, __VA_ARGS__)), \
            ft__log_common(level, error, fmt_or_msg, __VA_ARGS__))

#define ft__log_common(level, error, fmt_or_msg, ...) do {\
    if (level >= FT_ERROR || ft_unlikely(ft_will_log(level))) \
        ft__log(level, ft__srcpos(), error, ft__log_fmt_msg(fmt_or_msg, __VA_ARGS__)); \
} while(0)

#define ft__log_fmt_msg(fmt, ...) \
    fm_tuple_expand(fm_if(fm_no_va(__VA_ARGS__), ("%s", fmt), (fmt, __VA_ARGS__)))

extern ft_gnu_printf(4, 5)
void ft__log(enum FT_LOG_LEVEL level, ft_source_position_t srcpos, const char* error, const char *fmt, ...);
extern _Noreturn ft_gnu_printf(3, 4)
void ft__log_fatal(ft_source_position_t srcpos, const char* error, const char *fmt, ...);

ft_inline bool ft__dbg_enabled(void) {
    return ft_unlikely(ft_local_lgas_levels->assert_level >= FT_ASSERT_ALL);
}

#define ft__dbg_assert(x, xs, ...) do { \
    if (ft__dbg_enabled() && ft_unlikely(!(x))) \
        ft__log_fatal(ft__srcpos(), xs, ft__assert_arg(__VA_ARGS__)); \
} while(0)

#define ft__assert(x, xs, ...) do { \
    if (ft_unlikely(!(x))) \
        ft__log_fatal(ft__srcpos(), xs, ft__assert_arg(__VA_ARGS__)); \
} while(0)

#define ft__assert_arg(...) \
    fm_if(fm_no_va(__VA_ARGS__), "Asserion failed", \
            ft__log_fmt_msg(__VA_ARGS__))

#define ft__assyscall(syscall, res, ...)  ({ \
        __typeof(syscall) res = (syscall); \
        ft__assert(res >= 0, ft_strerror(errno), #syscall __VA_ARGS__); \
        res; \
    })

/* Comparison */

#define ft__max(a_, b_, a, b) ({ \
        __typeof(a_) a = (a_); \
        __typeof(b_) b = (b_); \
        a < b ? b : a ; \
        })

#define ft__min(a_, b_, a, b) ({ \
        __typeof(a_) a = (a_); \
        __typeof(b_) b = (b_); \
        a > b ? b : a ; \
        })

#define ft__cmp(a_, b_, a, b) ({ \
        __typeof(a_) a = (a_); \
        __typeof(b_) b = (b_); \
        a < b ? FT_CMP_LT : (a > b ? FT_CMP_GT : FT_CMP_EQ); \
        })

#define ft__swap(a_, b_, ap, bp, t) do { \
    __typeof(a_) ap = a_; \
    __typeof(a_) bp = b_; \
    __typeof(*ap) t = *ap; \
    *ap = *bp; \
    *bp = t; \
} while (0)

#if defined(__has_builtin) || defined(__clang__)
#   if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_mul_overflow)
#       define ft__has_builtin_int_overflow
#   endif
#elif __GNUC__ > 4 && !defined(__clang__)
#       define ft__has_builtin_int_overflow
#endif

ft_inline size_t ft_add_size(size_t a, size_t b) {
    size_t r;
#ifdef ft__has_builtin_int_overflow
    if (ft_unlikely(__builtin_add_overflow(a, b, &r)))
        ft_assert(r >= a && r >= b);
#else
    r = a + b;
    ft_assert(r >= a && r >= b);
#endif
    return r;
}

ft_inline size_t ft_mul_size(size_t a, size_t b) {
    size_t r;
#ifdef ft__has_builtin_int_overflow
    if (ft_unlikely(__builtin_mul_overflow(a, b, &r)))
        ft_assert(r / a == b);
#else
    r = a * b;
    ft_assert(r / a == b);
#endif
    return r;
}

extern ft_gcc_malloc(ft_realloc, 1) void* ft_realloc(void* ptr, size_t new_sz);
extern ft_gcc_malloc(ft_realloc, 1) void* ft_calloc(size_t sz);

// String utils
ft_inline char *
ft_strdup(const char *str) {
    size_t sz = strlen(str);
    char *mem = ft_malloc(sz + 1);
    memcpy(mem, str, sz+1);
    return mem;
}

extern ft_gcc_malloc(ft_realloc, 1) char* ft_asprintf(const char *fmt, ...) ft_gnu_printf(1,2);

// Some Numeric Utils

ft_inline uint32_t
ft_rol32(uint32_t x, unsigned n) {
    return n == 0 ? x : n >= 32 ? 0 : (x << n) | (x >> (32 - n));
}

ft_inline uint32_t
ft_ror32(uint32_t x, unsigned n) {
    return n == 0 ? x : n >= 32 ? 0 : (x << (32 - n)) | (x >> n);
}

ft_inline uint32_t
ft_mix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

ft_inline uint32_t
ft_fast_randmod(uint32_t v, uint32_t mod) {
    return (uint32_t)(((uint64_t)v * mod) >> 32);
}

ft_inline uint32_t ft_randn(uint32_t mod) {
    return ft_fast_randmod(ft_rand(), mod);
}

/* ft_val_t */
struct ft_arg {
    union {
        void       *p;
        char       *s;
        int64_t     i;
        uint64_t    u;
        double      d;
        bool        b;
    } v;
    char t;
};

ft_inline ft_arg_t ft_mka_z(void)       { return (ft_arg_t){.v={.u = 0}, .t='z'};}
ft_inline ft_arg_t ft_mka_p(void*    p) { return (ft_arg_t){.v={.p = p}, .t='p'};}
ft_inline ft_arg_t ft_mka_s(char*    s) { return (ft_arg_t){.v={.s = s}, .t='s'};}
ft_inline ft_arg_t ft_mka_i(int64_t  i) { return (ft_arg_t){.v={.i = i}, .t='i'};}
ft_inline ft_arg_t ft_mka_u(uint64_t u) { return (ft_arg_t){.v={.u = u}, .t='u'};}
ft_inline ft_arg_t ft_mka_d(double   d) { return (ft_arg_t){.v={.d = d}, .t='d'};}
ft_inline ft_arg_t ft_mka_b(bool     b) { return (ft_arg_t){.v={.b = b}, .t='b'};}

ft_inline char     ft_arg_type(ft_arg_t v) { return v.t; }

ft_inline void     ft_arg_z(ft_arg_t v) { ft_dbg_assert(v.t=='z'); }
ft_inline void*    ft_arg_p(ft_arg_t v) { ft_dbg_assert(v.t=='p'); return v.v.p; }
ft_inline char*    ft_arg_s(ft_arg_t v) { ft_dbg_assert(v.t=='s'); return v.v.s; }
ft_inline int64_t  ft_arg_i(ft_arg_t v) { ft_dbg_assert(v.t=='i'); return v.v.i; }
ft_inline uint64_t ft_arg_u(ft_arg_t v) { ft_dbg_assert(v.t=='u'); return v.v.u; }
ft_inline double   ft_arg_d(ft_arg_t v) { ft_dbg_assert(v.t=='d'); return v.v.d; }
ft_inline bool     ft_arg_b(ft_arg_t v) { ft_dbg_assert(v.t=='b'); return v.v.b; }

/* slices and arrays */

ft_inline size_t
ft__index_unify(ssize_t at, size_t len) {
    if (at >= 0) {
        ft_assert(at < len);
        return at;
    } else {
        ft_assert((size_t)(-at) <= len);
        return (size_t)(len - (size_t)(-at));
    }
}

ft_inline size_t
ft__slcindex_unify(ssize_t end, size_t len) {
    if (end >= 0) {
        ft_assert(end <= len);
        return end;
    } else if (end == FT_SLICE_END) {
        return len;
    } else {
        ft_assert((size_t)(-end) <= len);
        return (size_t)(len - (size_t)(-end));
    }
}

#endif
