/* vim: set expandtab autoindent cindent ts=4 sw=4 sts=4 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef WIN32
#define __thread __declspec(thread)
#else
#include <pthread.h>
#endif

#include <ft_util.h>

#define FT_LOG_MAX_FILES (1<<12)

static void * (*_ft_realloc) (void *, size_t) = realloc;
static void (*_ft_free) (void *) = free;

void ft_set_allocators(
        void *(*_realloc)(void *, size_t),
        void (*_free)(void*)) {
    _ft_realloc = _realloc ? _realloc : realloc;
    _ft_free = _free ? _free : free;
}

void*
ft_calloc(size_t size) {
    void * res = ft_malloc(size);
    ft_memzero(res, size);
    return res;
}

void*
ft_realloc(void *oldptr, size_t size) {
    if (size) {
        void *res = _ft_realloc(oldptr, size);
        ft_assert(res, "ft_realloc failed: oldptr=%p size=%zd", oldptr, size);
        return res;
    }
    if (oldptr)
        _ft_free(oldptr);
    return NULL;
}

void*
ft_realloc_arr(void* ptr, size_t elem_sz, size_t old_elems, size_t new_elems) {
    ptr = ft_realloc(ptr, ft_mul_size(elem_sz, new_elems));
    if (new_elems > old_elems)
        ft_memzero((char*)ptr + elem_sz * old_elems,
                   elem_sz * (new_elems - old_elems));
    return ptr;
}

#define MEMZERO_BLOCK 4096
static const uint8_t zero[4096] = {0};
void
ft_memzero(void *_ptr, size_t sz) {
    uint8_t*  ptr = _ptr;
    uintptr_t ptri = (uintptr_t)ptr;
    uintptr_t diff;

    if (ptri & (MEMZERO_BLOCK-1)) {
        diff = MEMZERO_BLOCK - (ptri & (MEMZERO_BLOCK-1));
        if (diff > sz)
            diff = sz;
        memset(ptr, 0, diff);
        ptr += diff;
        sz -= diff;
    }

    /* Do not dirty page if it clear */
    while (sz >= MEMZERO_BLOCK) {
        if (memcmp(ptr, zero, MEMZERO_BLOCK) != 0) {
            memset(ptr, 0, MEMZERO_BLOCK);
        }
        ptr += MEMZERO_BLOCK;
        sz -= MEMZERO_BLOCK;
    }

    if (sz)
        memset(ptr, 0, sz);
}

/* String utils */

size_t
ft_strlcat(char *dest, const char* src, size_t dest_size) {
    char*   dest_null = memchr(dest, 0, dest_size);
    size_t  dest_len = dest_null ? dest_null - dest : dest_size;
    ft_assert(dest_null, "destination has no zero byte");
    if (dest_len < dest_size-1) {
        size_t cpy_len = dest_size - dest_len - 1;
        strncpy(dest+dest_len, src, cpy_len);
        dest[dest_len + cpy_len] = '\0';
    }
    return dest_len + strlen(src);
}

char*
ft_vasprintf(const char *fmt, va_list args) {
#define initbuf 256
    int     save_errno = errno;
    char    buf[initbuf] = "";
    char   *str = NULL;
    int     len, need_len;
    va_list argcpy;

    va_copy(argcpy, args);
    need_len = vsnprintf(buf, initbuf, fmt, argcpy);
    va_end(argcpy);

    if (need_len < 0)
        return NULL;

    if (need_len < initbuf)
        return ft_strdup(buf);


    for (;;) {
        len = need_len + 1;
        str = ft_realloc(str, len);

        errno = save_errno;
        va_copy(argcpy, args);
        need_len = vsnprintf(str, len, fmt, argcpy);
        va_end(argcpy);

        if (need_len < 0) {
            ft_free(str);
            return NULL;
        }

        if (need_len < len)
            return str;
    }
#undef initbuf
}

char*
ft_asprintf(const char *fmt, ...) {
    va_list args;
    char*   res;

    va_start(args, fmt);
    res = ft_vasprintf(fmt, args);
    va_end(args);

    return res;
}

/* Time */
double
ft_time(void) {
    struct timeval tv = {0, 0};
    ft_assyscall(gettimeofday(&tv, NULL));
    return (double)tv.tv_sec + (double)tv.tv_usec/1e6;
}

/* Logging */

/*
static _Noreturn void
ft_quick_exit(const char* msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    abort();
}
*/

static const char*
ft__truncate_log_filename(const char *file) {
    const char *me = __FILE__;
    const char *he = file;
    for (;*he && *me && *he==*me;he++, me++) {
#ifndef WIN32
        if (*he == '/')
            file = he+1;
#else
        if (*he == '/' || *he == '\\')
            file = he+1;
#endif
    }
    return file;
}

static void ft_gnu_printf(4,0)
ft_default_log(enum FT_LOG_LEVEL level, ft_source_position_t srcpos,
                const char* error, const char *fmt, va_list args) {
#define LOGMSG_SIZE (1<<12)
    char buffer[LOGMSG_SIZE] = {0};
    char *buf   = buffer;
    size_t rest = LOGMSG_SIZE;
    size_t sz;
    double now;

    now = ft_time();
    sz = snprintf(buf, rest, "%.3f %d [%s]",
                    now, getpid(), ft_log_level_str(level));
    buf += sz;
    rest -= sz;

    if (level <= FT_DEBUG || level >= FT_ERROR) {
        sz = snprintf(buf, rest, " (%s@%s:%d)",
                srcpos.func,
                srcpos.file,
                srcpos.line);
        if (sz >= rest)
            goto done;
        buf += sz;
        rest -= sz;
    }

    sz = ft_strlcat(buf, " > ", rest);
    if (sz >= rest)
        goto done;
    buf += sz;
    rest -= sz;

    sz = vsnprintf(buf, rest, fmt, args);
    if (sz >= rest)
        goto done;
    buf += sz;
    rest -= sz;

    if (error != NULL) {
        sz = snprintf(buf, rest, ": %s", error);
        if (sz >= rest)
            goto done;
        buf += sz;
        rest -= sz;
    }

done:
    fprintf(stderr, "%s\n", buffer);
}

void (*ft_log_hook)(enum FT_LOG_LEVEL, ft_source_position_t, const char*, const char *fmt, va_list args) = ft_default_log;

void
ft__log(enum FT_LOG_LEVEL level, ft_source_position_t srcpos,
        const char* error, const char *fmt, ...) {
    va_list args;
    srcpos.file = ft__truncate_log_filename(srcpos.file);
    va_start(args, fmt);
    ft_log_hook(level, srcpos, error, fmt, args);
    va_end(args);
}

extern _Noreturn void
ft__log_fatal(ft_source_position_t srcpos, const char* error,
        const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ft_log_hook(FT_FATAL, srcpos, error, fmt, args);
    va_end(args);
    abort();
}

const char*
ft__strerror(int eno, char *buf, size_t len) {
#if !_GNU_SOURCE && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
    int saveno = errno;
    int e = strerror_r(eno, buf, len);
    if (e != 0) {
        if (e == -1) {
            e = errno;
        }
        if (e == EINVAL) {
            snprintf(buf, len, "Wrong errno %d", eno);
        } else if (e == ERANGE) {
            snprintf(buf, len, "errno = %d has huge message", eno);
        }
    }
    errno = saveno;
    return buf;
#else
    return strerror_r(eno, buf, len);
#endif
}

#ifndef __TINYC__
const char*
ft_strerror(int eno) {
    static __thread char buf[256];
    return ft__strerror(eno, buf, sizeof(buf));
}
#endif

struct ft_log_and_assert_level   ft_log_assert_levels = {
    .log_level = FT_INFO,
#ifndef NDEBUG
    .assert_level = FT_ASSERT_ALL,
#else
    .assert_level = FT_ASSERT_RUNTIME,
#endif
};

typedef struct {
    const char *file;
    uint32_t    next;
    struct ft_log_and_assert_level  local_levels;
} ft_log_file_registration;

#define FT_LOG_FILES_HASH (FT_LOG_MAX_FILES/4)
static ft_log_file_registration ft_log_file_regs[FT_LOG_MAX_FILES] = {{0}};
static uint32_t ft_log_file_reg_hash[FT_LOG_FILES_HASH] = {0};
static uint32_t ft_log_file_n = 0;

extern void
ft__register_source(
        const char *file,
        struct ft_log_and_assert_level **local_levels) {
    ft_log_file_registration *reg;
    uint32_t hash;

    ft_assert(ft_log_file_n < FT_LOG_MAX_FILES);
    ft_dbg_assert(file != NULL);

    reg = &ft_log_file_regs[ft_log_file_n++];

    reg->file = ft__truncate_log_filename(file);
    reg->local_levels = ft_log_assert_levels;

    *local_levels = &reg->local_levels;

    hash = ft_small_cstr_hash(reg->file);
    reg->next = ft_log_file_reg_hash[hash%FT_LOG_FILES_HASH];
    ft_log_file_reg_hash[hash%FT_LOG_FILES_HASH] = ft_log_file_n;
}

static void
ft__log_level_reset(int what, int level) {
    uint32_t i;

    if (what)
        ft_log_assert_levels.log_level = level;
    else
        ft_log_assert_levels.assert_level = level;

    for (i = 0; i < ft_log_file_n; i++) {
        if (what)
            ft_log_file_regs[i].local_levels.log_level = level;
        else
            ft_log_file_regs[i].local_levels.assert_level = level;
    }
}

static void
ft__log_level_set(const char *file, int what, int level) {
    ft_log_file_registration *reg;
    uint32_t hash, i;

    ft_dbg_assert(file != NULL);

    if (strcmp(file, "ALL") == 0) {
        ft__log_level_reset(what, level);
        return;
    }

    file = ft__truncate_log_filename(file);

    hash = ft_small_cstr_hash(file);
    i = ft_log_file_reg_hash[hash%FT_LOG_FILES_HASH];
    while (i) {
        reg = &ft_log_file_regs[i-1];
        ft_dbg_assert(reg->file != NULL);
        if (strcmp(reg->file, file) == 0) {
            if (what)
                reg->local_levels.log_level = level;
            else
                reg->local_levels.assert_level = level;
            return;
        }
        i = reg->next;
    }
    /*
     * ooops... not found... pity...
     * ok, lets set global one, but without per-file setting
     */
    if (what)
        ft_log_assert_levels.log_level = level;
    else
        ft_log_assert_levels.assert_level = level;
}

void
ft_log_level_reset(enum FT_LOG_LEVEL level) {
    ft__log_level_reset(1, level);
}

void
ft_assert_level_reset(enum FT_ASSERT_LEVEL level) {
    ft__log_level_reset(0, level);
}

void
ft_log_level_set(const char *file, enum FT_LOG_LEVEL level) {
    ft__log_level_set(file, 1, level);
}

void
ft_assert_level_set(const char *file, enum FT_ASSERT_LEVEL level) {
    ft__log_level_set(file, 0, level);
}

uint32_t
ft_rand(void) {
    static volatile uint32_t rstate = 0xbeaf1234;
    uint32_t rand = __atomic_fetch_add(&rstate, 0x11, __ATOMIC_RELAXED);
    rand = ft_mix32(rand);
    return rand;
}

uint32_t
ft_small_cstr_hash(const char *key) {
    unsigned char  *str = (unsigned char *)key;
    uint32_t h1 = 0x3b00;
    uint32_t h2 = 0;
    for (;str[0]; str++) {
        h1 += str[0];
        h1 *= 9;
        h2 += h1;
        h2 = ft_rol32(h2, 7);
        h2 *= 5;
    }
    h1 ^= h2;
    h1 += ft_rol32(h2, 14);
    h2 ^= h1; h2 += ft_ror32(h1, 6);
    h1 ^= h2; h1 += ft_rol32(h2, 5);
    h2 ^= h1; h2 += ft_ror32(h1, 8);
    return h2;
}

