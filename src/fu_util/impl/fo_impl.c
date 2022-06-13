/* vim: set expandtab autoindent cindent ts=4 sw=4 sts=4 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#ifdef WIN32
#define __thread __declspec(thread)
#endif
#include <pthread.h>

#include <fo_obj.h>

/*
 * We limits total number of methods, klasses and method implementations.
 * Restricted number allows to use uint16_t for id and doesn't bother with
 * smart structures for hashes.
 * If you need more, you have to change the way they are stored.
 */
#define FOBJ_OBJ_MAX_KLASSES (1<<10)
#define FOBJ_OBJ_MAX_METHODS (1<<10)
#define FOBJ_OBJ_MAX_METHOD_IMPLS (1<<15)

enum { FOBJ_DISPOSING = 1, FOBJ_DISPOSED = 2 };

typedef enum {
    FOBJ_RT_NOT_INITIALIZED,
    FOBJ_RT_INITIALIZED,
    FOBJ_RT_FROZEN
} FOBJ_GLOBAL_STATE;

typedef struct fobj_header {
#ifndef NDEBUG
#define FOBJ_HEADER_MAGIC UINT64_C(0x1234567890abcdef)
    uint64_t magic;
#endif
    volatile uint32_t rc;
    volatile uint16_t flags;
    fobj_klass_handle_t klass;
} fobj_header_t;

#define METHOD_PARTITIONS (16)

typedef struct fobj_klass_registration {
    const char *name;
    uint32_t    hash;
    uint32_t    hash_next;

    ssize_t     size;
    fobj_klass_handle_t parent;

    uint32_t    nmethods;

    /* common methods */
    fobj__nm_impl_t(fobjDispose)      dispose;

    volatile uint16_t method_lists[METHOD_PARTITIONS];
} fobj_klass_registration_t;

typedef struct fobj_method_registration {
    const char *name;
    uint32_t    hash;
    uint32_t    hash_next;

    uint32_t    nklasses;
    uint32_t    first;
} fobj_method_registration_t;

typedef struct fobj_method_impl {
    uint16_t    method;
    uint16_t    klass;
    uint16_t    next_for_method;
    uint16_t    next_for_klass;
    void*       impl;
} fobj_method_impl_t;


static fobj_klass_registration_t  fobj_klasses[1<<10] = {{0}};
static fobj_method_registration_t fobj_methods[1<<10] = {{0}};
#define FOBJ_OBJ_HASH_SIZE (FOBJ_OBJ_MAX_METHODS/4)
static volatile uint16_t fobj_klasses_hash[FOBJ_OBJ_HASH_SIZE] = {0};
static volatile uint16_t fobj_methods_hash[FOBJ_OBJ_HASH_SIZE] = {0};
static fobj_method_impl_t fobj_method_impl[FOBJ_OBJ_MAX_METHOD_IMPLS] = {{0}};
static volatile uint32_t fobj_klasses_n = 0;
static volatile uint32_t fobj_methods_n = 0;
static volatile uint32_t fobj_impls_n = 0;

#define FOBJ_OBJ_NAMES_BUF ((FOBJ_OBJ_MAX_KLASSES + FOBJ_OBJ_MAX_METHODS)*64)
static char fobj_names_buf[FOBJ_OBJ_NAMES_BUF] = {0};
static volatile size_t fobj_names_pos = 0;

static const char*
fobj__name_dup(const char *name) {
    size_t  len = strlen(name);
    char   *res = fobj_names_buf + fobj_names_pos;

    ft_assert(fobj_names_pos + len + 1 <= FOBJ_OBJ_NAMES_BUF);
    memcpy(res, name, len);
    res[len] = '\0';
    fobj_names_pos += len + 1;

    return res;
}

static pthread_mutex_t fobj_runtime_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile uint32_t fobj_global_state = FOBJ_RT_NOT_INITIALIZED;

#define pth_assert(...) do { \
    int rc = __VA_ARGS__; \
    ft_assert(!rc, "fobj_runtime_mutex: %s", ft_strerror(rc)); \
} while(0)

#define atload(v) __atomic_load_n((v), __ATOMIC_ACQUIRE)

bool
fobj_method_init_impl(volatile fobj_method_handle_t *meth, const char *name) {
    uint32_t hash, mh;
    fobj_method_registration_t *reg;

    ft_dbg_assert(meth);

    pth_assert(pthread_mutex_lock(&fobj_runtime_mutex));
    if ((mh = *meth) != 0) {
        reg = &fobj_methods[mh];
        pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));
        ft_assert(mh <= atload(&fobj_methods_n));
        ft_assert(strcmp(reg->name, name) == 0);
        return true;
    }


    hash = ft_small_cstr_hash(name);
    mh = fobj_methods_hash[hash % FOBJ_OBJ_HASH_SIZE];
    for (; mh != 0; mh = reg->hash_next) {
        reg = &fobj_methods[mh];
        if (reg->hash == hash && strcmp(reg->name, name) == 0) {
            __atomic_store_n(meth, mh, __ATOMIC_RELEASE);
            pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));
            return true;
        }
    }

    ft_assert(fobj_global_state == FOBJ_RT_INITIALIZED);

    mh = atload(&fobj_methods_n) + 1;
    ft_dbg_assert(mh > 0);
    ft_assert(*meth < FOBJ_OBJ_MAX_METHODS, "Too many methods defined");
    reg = &fobj_methods[mh];
    reg->name = fobj__name_dup(name);
    reg->hash = hash;
    reg->hash_next = fobj_methods_hash[hash % FOBJ_OBJ_HASH_SIZE];
    fobj_methods_hash[hash % FOBJ_OBJ_HASH_SIZE] = mh;

    __atomic_store_n(&fobj_methods_n, mh, __ATOMIC_RELEASE);
    __atomic_store_n(meth, mh, __ATOMIC_RELEASE);

    pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));

    return false;
}

static inline void*
fobj_search_impl(fobj_method_handle_t meth, fobj_klass_handle_t klass) {
    fobj_klass_registration_t *kreg;
    uint32_t i;

    kreg = &fobj_klasses[klass];

    i = atload(&kreg->method_lists[meth%METHOD_PARTITIONS]);
    while(i != 0 && fobj_method_impl[i].method != meth)
        i = fobj_method_impl[i].next_for_klass;

    return i ? fobj_method_impl[i].impl : NULL;
}

void*
fobj_klass_method_search(fobj_klass_handle_t klass, fobj_method_handle_t meth) {
    fobj_klass_registration_t  *kreg;
    uint32_t i;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    ft_assert(meth > 0 && meth <= atload(&fobj_methods_n));
    ft_assert(meth != fobj__nm_mhandle(fobjDispose)());
    ft_assert(klass > 0 && klass <= atload(&fobj_klasses_n));

    do {
        kreg = &fobj_klasses[klass];

        i = atload(&kreg->method_lists[meth%METHOD_PARTITIONS]);
        while(i != 0 && fobj_method_impl[i].method != meth)
            i = fobj_method_impl[i].next_for_klass;
        if (i)
            return fobj_method_impl[i].impl;

        klass = kreg->parent;
    } while (klass != 0);
    return NULL;
}


fobj__method_callback_t
fobj_method_search(const fobj_t self, fobj_method_handle_t meth, fobj_klass_handle_t for_child) {
    fobj_header_t              *h;
    fobj_klass_handle_t         klass;
    fobj_klass_registration_t  *kreg;
    fobj__method_callback_t     cb = {NULL, NULL};

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    if (ft_dbg_enabled()) {
        ft_assert(meth > 0 && meth <= fobj_methods_n);
        ft_assert(meth != fobj__nm_mhandle(fobjDispose)());
    }

    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    klass = h->klass;
    ft_assert(klass > 0 && klass <= atload(&fobj_klasses_n));

    if (for_child != 0) {
        while (klass && klass != for_child) {
            kreg = &fobj_klasses[klass];
            klass = kreg->parent;
        }
        if (klass == 0)
            return cb;
        kreg = &fobj_klasses[klass];
        klass = kreg->parent;
    }

    while (klass) {
        cb.impl = fobj_search_impl(meth, klass);
        if (cb.impl != NULL) {
            cb.self = self;
            return cb;
        }

        kreg = &fobj_klasses[klass];
        klass = kreg->parent;
    }
    return cb;
}

const char *
fobj_klass_name(fobj_klass_handle_t klass) {
    fobj_klass_registration_t *reg;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    ft_dbg_assert(klass && klass <= atload(&fobj_klasses_n));

    reg = &fobj_klasses[klass];

    return reg->name;
}

static void fobj_method_register_priv(fobj_klass_handle_t klass,
                                      fobj_method_handle_t meth,
                                      void* impl);

bool
fobj_klass_init_impl(volatile fobj_klass_handle_t *klass,
                     ssize_t size,
                     fobj_klass_handle_t parent,
                     fobj__method_impl_box_t *methods,
                     const char *name) {
    uint32_t hash, kl;
    fobj_klass_registration_t *reg;

    ft_assert(fobj_global_state == FOBJ_RT_INITIALIZED);
    ft_dbg_assert(klass);

    pth_assert(pthread_mutex_lock(&fobj_runtime_mutex));

    if ((kl = *klass) != 0) {
        reg = &fobj_klasses[kl];
        pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));
        ft_assert(kl <= atload(&fobj_klasses_n));
        ft_assert(strcmp(reg->name, name) == 0);
        ft_assert(reg->size ==  size);
        ft_assert(reg->parent == parent);
        return true;
    }

    hash = ft_small_cstr_hash(name);
    kl = fobj_klasses_hash[hash % FOBJ_OBJ_HASH_SIZE];
    for (; kl != 0; kl = reg->hash_next) {
        reg = &fobj_klasses[kl];
        if (reg->hash == hash && strcmp(reg->name, name) == 0) {
            __atomic_store_n(klass, kl, __ATOMIC_RELEASE);
            pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));
            ft_assert(reg->size == size);
            ft_assert(reg->parent == parent);
            return true;
        }
    }

    kl = atload(&fobj_klasses_n) + 1;
    ft_dbg_assert(kl > 0);
    ft_assert(*klass < FOBJ_OBJ_MAX_KLASSES, "Too many klasses defined");
    reg = &fobj_klasses[kl];
    reg->size = size;
    reg->name = fobj__name_dup(name);
    reg->parent = parent;
    reg->hash = hash;
    reg->hash_next = fobj_klasses_hash[hash % FOBJ_OBJ_HASH_SIZE];
    fobj_klasses_hash[hash % FOBJ_OBJ_HASH_SIZE] = kl;

    __atomic_store_n(&fobj_klasses_n, kl, __ATOMIC_RELEASE);
    /* declare methods before store klass */
    while (methods->meth != 0) {
        fobj_method_register_priv(kl, methods->meth, methods->impl);
        methods++;
    }

    __atomic_store_n(klass, kl, __ATOMIC_RELEASE);

    pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));

    return false;
}

static void
fobj_method_register_priv(fobj_klass_handle_t klass, fobj_method_handle_t meth, void* impl) {
    fobj_method_registration_t *mreg;
    fobj_klass_registration_t *kreg;
    void    *existed;
    uint32_t nom;

    mreg = &fobj_methods[meth];
    kreg = &fobj_klasses[klass];

    existed = fobj_search_impl(meth, klass);
    ft_dbg_assert(existed == NULL || existed == impl,
                "Method %s.%s is redeclared with different implementation",
                kreg->name, mreg->name);

    if (existed == impl) {
        return;
    }

    nom = atload(&fobj_impls_n) + 1;
    ft_assert(nom < FOBJ_OBJ_MAX_METHOD_IMPLS);
    fobj_method_impl[nom].method = meth;
    fobj_method_impl[nom].klass = klass;
    fobj_method_impl[nom].next_for_method = mreg->first;
    fobj_method_impl[nom].next_for_klass = kreg->method_lists[meth%METHOD_PARTITIONS];
    fobj_method_impl[nom].impl = impl;
    mreg->first = nom;
    __atomic_store_n(&kreg->method_lists[meth%METHOD_PARTITIONS], nom,
                    __ATOMIC_RELEASE);

    if (meth == fobj__nm_mhandle(fobjDispose)())
        kreg->dispose = (fobj__nm_impl_t(fobjDispose)) impl;

    nom = __atomic_add_fetch(&fobj_impls_n, 1, __ATOMIC_ACQ_REL);
}

void
fobj_method_register_impl(fobj_klass_handle_t klass, fobj_method_handle_t meth, void* impl) {
    ft_assert(fobj_global_state == FOBJ_RT_INITIALIZED);
    ft_dbg_assert(meth > 0 && meth <= atload(&fobj_methods_n));
    ft_dbg_assert(klass > 0 && klass <= atload(&fobj_klasses_n));

    pth_assert(pthread_mutex_lock(&fobj_runtime_mutex));

    fobj_method_register_priv(klass, meth, impl);

    pth_assert(pthread_mutex_unlock(&fobj_runtime_mutex));
}

void*
fobj__allocate(fobj_klass_handle_t klass, void *init, ssize_t size) {
    fobj_klass_registration_t *kreg;
    fobj_header_t  *hdr;
    fobj_t          self;
    ssize_t         copy_size;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    ft_dbg_assert(klass > 0 && klass <= atload(&fobj_klasses_n));

    kreg = &fobj_klasses[klass];
    copy_size = kreg->size >= 0 ? kreg->size : -1-kreg->size;
    if (size < 0) {
        size = copy_size;
    } else {
        ft_assert(kreg->size < 0);
        size += copy_size;
    }
    hdr = ft_calloc(sizeof(fobj_header_t) + size);
#ifndef NDEBUG
    hdr->magic = FOBJ_HEADER_MAGIC;
#endif
    hdr->klass = klass;
    hdr->rc = 1;
    self = (fobj_t)(hdr + 1);
    if (init != NULL)
        memcpy(self, init, copy_size);
    fobj_autorelease(self);
    return self;
}

fobj_t
fobj_retain(fobj_t self) {
    fobj_header_t *h;
    if (self == NULL)
        return NULL;
    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    ft_assert(h->klass > 0 && h->klass <= atload(&fobj_klasses_n));
    __atomic_fetch_add(&h->rc, 1, __ATOMIC_ACQ_REL);
    return self;
}

fobj_t
fobj__set(fobj_t *ptr, fobj_t val) {
    fobj_t oldval = *ptr;
    *ptr = $ref(val);
    fobj_release(oldval);
    return val;
}

static void
fobj__dispose_req(fobj_t self, fobj_klass_registration_t *kreg) {
    if (kreg->dispose)
        kreg->dispose(self);
    if (kreg->parent) {
        fobj_klass_registration_t *preg;

        preg = &fobj_klasses[kreg->parent];
        fobj__dispose_req(self, preg);
    }
}

static void
fobj__do_dispose(fobj_t self, fobj_header_t *h, fobj_klass_registration_t *kreg) {
    uint32_t old = __atomic_fetch_or(&h->flags, FOBJ_DISPOSING, __ATOMIC_ACQ_REL);
    if (old & FOBJ_DISPOSING)
        return;
    fobj__dispose_req(self, kreg);
    __atomic_fetch_or(&h->flags, FOBJ_DISPOSED, __ATOMIC_ACQ_REL);

    if (atload(&h->rc) == 0)
        ft_free(h);
}

void
fobj_dispose(fobj_t self) {
    fobj_header_t *h;
    fobj_klass_handle_t klass;
    fobj_klass_registration_t *kreg;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);

    if (self == NULL)
        return;

    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    klass = h->klass;
    ft_dbg_assert(klass > 0 && klass <= atload(&fobj_klasses_n));
    kreg = &fobj_klasses[klass];

    fobj__do_dispose(self, h, kreg);
}

void
fobj_release(fobj_t self) {
    fobj_header_t *h;
    fobj_klass_handle_t klass;
    fobj_klass_registration_t *kreg;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);

    if (self == NULL)
        return;

    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    klass = h->klass;
    ft_dbg_assert(klass > 0 && klass <= atload(&fobj_klasses_n));
    kreg = &fobj_klasses[klass];


    if (__atomic_sub_fetch(&h->rc, 1, __ATOMIC_ACQ_REL) != 0)
        return;
    if ((atload(&h->flags) & FOBJ_DISPOSING) != 0)
        return;
    fobj__do_dispose(self, h, kreg);
}

bool
fobj_disposing(fobj_t self) {
    fobj_header_t *h;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    ft_assert(self != NULL);

    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    return (atload(&h->flags) & FOBJ_DISPOSING) != 0;
}

bool
fobj_disposed(fobj_t self) {
    fobj_header_t *h;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    ft_assert(self != NULL);

    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    return (atload(&h->flags) & FOBJ_DISPOSED) != 0;
}

static fobj_klass_handle_t
fobjBase_fobjKlass(fobj_t self) {
    fobj_header_t *h;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);
    ft_assert(self != NULL);

    h = ((fobj_header_t*)self - 1);
    assert(h->magic == FOBJ_HEADER_MAGIC);
    return h->klass;
}

static struct fobjStr*
fobjBase_fobjRepr(VSelf) {
    Self(fobjBase);
    fobj_klass_handle_t klass = fobjKlass(self);
    return fobj_sprintf("%s@%p", fobj_klass_name(klass), self);
}

static void
fobjErr_fobjDispose(VSelf) {
    Self(fobjErr);
    if (self->cause)
        $del(&self->cause);
    if (self->sibling)
        $del(&self->sibling);
    if (self->message) {
        ft_free((void*)self->message);
        self->message = NULL;
    }
}

static const char*
fobjErr_fobjErrMsg(VSelf) {
    Self(fobjErr);
    return self->message ?: "Unspecified Error";
}

fobjErr*
fobj_err_combine(fobjErr* first, fobjErr* second) {
    fobjErr   **tail;
    if (first == NULL)
        return second;
    if (second == NULL)
        return first;
    if (first->sibling != NULL) {
        tail = &second->sibling;
        while (*tail != NULL) tail = &(*tail)->sibling;
        /* owner ship is also transferred */
        *tail = first->sibling;
    }
    first->sibling = $ref(second);
    return first;
}

fobjStr*
fobj_newstrn(const char* s, size_t len) {
    fobjStr *str;
    ft_assert(len < UINT32_MAX-2);
    str = fobj_alloc_sized(fobjStr, len+1, .len = len);
    memcpy(str->s, s, len);
    str->s[len] = '\0';
    return str;
}

fobjStr*
fobj_strncat(fobjStr *self, const char* str, size_t add_len) {
    fobjStr *newstr;
    size_t alloc_len = self->len + add_len + 1;
    ft_assert(alloc_len < UINT32_MAX-2);

    newstr = fobj_alloc_sized(fobjStr, alloc_len, .len = alloc_len-1);
    memcpy(newstr->s, self->s, self->len);
    memcpy(newstr->s + self->len, str, add_len);
    newstr->s[newstr->len] = '\0';
    return newstr;
}

fobjStr*
fobj_sprintf(const char *fmt, ...) {
#define initbuf 128
    int     save_errno = errno;
    char    buf[initbuf] = "";
    fobjStr *str;
    int     len, need_len;
    va_list args;

    va_start(args, fmt);
    need_len = vsnprintf(buf, initbuf, fmt, args);
    va_end(args);

    if (need_len < 0)
        return NULL;

    if (need_len < initbuf)
        return fobj_newstrn(buf, need_len);

    for (;;) {
        len = need_len + 1;
        str = fobj_alloc_sized(fobjStr, len, .len = need_len);

        errno = save_errno;
        va_start(args, fmt);
        need_len = vsnprintf(str->s, len, fmt, args);
        va_end(args);

        if (need_len < 0)
            return NULL;

        if (need_len < len)
            return str;
    }
#undef initbuf
}

fobjStr*
fobj_strcatf(fobjStr *ostr, const char *fmt, ...) {
    int     save_errno = errno;
    fobjStr *str;
    int     len, need_len;
    va_list args;

    va_start(args, fmt);
    need_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (need_len < 0)
        return NULL;
    if (need_len == 0)
        return ostr;

    for (;;) {
        len = ostr->len + need_len + 1;
        str = fobj_alloc_sized(fobjStr, len, .len = len-1);

        errno = save_errno;
        va_start(args, fmt);
        need_len = vsnprintf(str->s + ostr->len, need_len + 1, fmt, args);
        va_end(args);

        if (need_len < 0)
            return NULL;

        if (ostr->len + need_len < len) {
            memcpy(str->s, ostr->s, ostr->len);
            return str;
        }
    }
}


#ifndef WIN32
static pthread_key_t fobj_AR_current_key = 0;
static void fobj_destroy_thread_AR(void *arg);
#endif

/* Custom fobjBase implementation */
fobj_klass_handle_t fobjBase__kh(void) {
    static volatile fobj_klass_handle_t hndl = 0;
    fobj_klass_handle_t khandle = hndl;
    ssize_t kls_size = sizeof(fobjBase);
    if (khandle) return khandle;
    {
        fobj__method_impl_box_t methods[] = {
            fobj__klass_decl_methods(fobjBase, fobj__map_params(kls__fobjBase))
            { 0, NULL }
        };
        if (fobj_klass_init_impl(&hndl, kls_size, 0, methods, "fobjBase"))
            return hndl;
    }
    khandle = hndl;
#define force_expand(klass, ...) \
    _force_expand(klass, fobj__map_params(__VA_ARGS__))
#define _force_expand(klass, ...) \
    fm_when(fm_isnt_empty(fobj__klass_has_iface(__VA_ARGS__))) ( \
        fobj__klass_check_iface(klass, __VA_ARGS__); \
    )
    force_expand(fobjBase, kls__fobjBase)
    return khandle;
}

fobj_klass_handle(fobjErr);
fobj_klass_handle(fobjStr);

void
fobj_init(void) {
    ft_assert(fobj_global_state == FOBJ_RT_NOT_INITIALIZED);

#ifndef WIN32
    {
        int res = pthread_key_create(&fobj_AR_current_key, fobj_destroy_thread_AR);
        if (res != 0) {
            fprintf(stderr, "could not initialize autorelease thread key: %s",
                    strerror(res));
            abort();
        }
    }
#endif

    fobj_global_state = FOBJ_RT_INITIALIZED;

    fobj__consume(fobjDispose__mh());
    fobj_klass_init(fobjBase);
    fobj_klass_init(fobjErr);
    fobj_klass_init(fobjStr);
}

void
fobj_freeze(void) {
    fobj_global_state = FOBJ_RT_FROZEN;
}

/* Without this function clang could commit initialization of klass without methods */
volatile uint16_t fobj__FAKE__x;
void
fobj__consume(uint16_t _) {
    fobj__FAKE__x += _;
}

// AUTORELEASE POOL

static void fobj_autorelease_pool_release_till(fobj_autorelease_pool **from, fobj_autorelease_pool *till);

#ifndef __TINYC__
static __thread fobj_autorelease_pool *fobj_AR_current = NULL;
#ifndef WIN32
static __thread bool fobj_AR_current_set = false;
#endif
static inline fobj_autorelease_pool**
fobj_AR_current_ptr(void) {
    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);

#ifndef WIN32
    if (!fobj_AR_current_set)
        pthread_setspecific(fobj_AR_current_key, &fobj_AR_current);
#endif
    return &fobj_AR_current;
}
#ifndef WIN32
static void
fobj_destroy_thread_AR(void *arg) {
    ft_assert(arg == &fobj_AR_current);
    fobj_autorelease_pool_release_till(&fobj_AR_current, NULL);
}
#endif
#else
static fobj_autorelease_pool**
fobj_AR_current_ptr(void) {
    fobj_autorelease_pool **current;

    ft_assert(fobj_global_state != FOBJ_RT_NOT_INITIALIZED);

    current = pthread_getspecific(fobj_AR_current_key);
    if (current == NULL) {
        current = ft_calloc(sizeof(fobj_autorelease_pool*));
        pthread_setspecific(fobj_AR_current_key, current);
    }
    return current;
}
static void
fobj_destroy_thread_AR(void *arg) {
    fobj_autorelease_pool **current = arg;

    fobj_autorelease_pool_release_till(current, NULL);
    ft_free(current);
}
#endif

fobj__autorelease_pool_ref
fobj_autorelease_pool_init(fobj_autorelease_pool *pool) {
    fobj_autorelease_pool **parent = fobj_AR_current_ptr();
    pool->ref.parent = *parent;
    pool->ref.root = parent;
    pool->last = &pool->first;
    pool->first.prev = NULL;
    pool->first.cnt = 0;
    *parent = pool;
    return pool->ref;
}

void
fobj_autorelease_pool_release(fobj_autorelease_pool *pool) {
    fobj_autorelease_pool_release_till(pool->ref.root, pool->ref.parent);
}

static void
fobj_autorelease_pool_release_till(fobj_autorelease_pool **from, fobj_autorelease_pool *till) {
    fobj_autorelease_pool   *current;
    fobj_autorelease_chunk  *chunk;
    fobj_autorelease_chunk  *prev_chunk;

    while (*from != till) {
        current = *from;
        chunk = current->last;
        for (;;) {
            current->last = chunk;
            while (chunk->cnt) {
                $del(&chunk->refs[--chunk->cnt]);
            }
            if (chunk == &current->first)
                break;
            prev_chunk = chunk->prev;
            ft_free(chunk);
            chunk = prev_chunk;
        }
        *from = (*from)->ref.parent;
    }
}

static fobj_t
fobj_autorelease_impl(fobj_t obj, fobj_autorelease_pool *pool) {
    fobj_autorelease_chunk  *chunk, *new_chunk;

    ft_assert(pool != NULL);

    chunk = pool->last;
    if (chunk->cnt == FOBJ_AR_CHUNK_SIZE) {
        new_chunk = ft_calloc(sizeof(fobj_autorelease_chunk));
        new_chunk->prev = chunk;
        pool->last = chunk = new_chunk;
    }
    chunk->refs[chunk->cnt] = obj;
    chunk->cnt++;
    return obj;
}

fobj_t
fobj_autorelease(fobj_t obj) {
    return fobj_autorelease_impl(obj, *fobj_AR_current_ptr());
}

fobj_t
fobj_store_to_parent_pool(fobj_t obj, fobj_autorelease_pool *child_pool_or_null) {
    return fobj_autorelease_impl(obj,
            (child_pool_or_null ?: *fobj_AR_current_ptr())->ref.parent);
}
