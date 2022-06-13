/* vim: set expandtab autoindent cindent ts=4 sw=4 sts=4 */
#ifndef FOBJ_OBJ_H
#define FOBJ_OBJ_H

#include <assert.h>
#include <ft_util.h>

/*
 * Pointer to "object*.
 * In fact, it is just 'void *'.
 */
typedef void* fobj_t;
/*
 * First argument, representing method receiver.
 * Unfortunately, function couldn't have arbitrary typed receiver without issueing
 * compiller warning.
 * Use Self(Klass) to convert to concrete type.
 */
#define VSelf fobj_t Vself
/*
 * Self(Klass) initiate "self" variable with casted pointer.
 */
#define Self(Klass) Self_impl(Klass)

extern void fobj_init(void);
/*
 * fobj_freeze forbids further modifications to runtime.
 * It certainly should be called before additional threads are created.
 */
extern void fobj_freeze(void);

#define fobj_self_klass 0

#include "./impl/fo_impl.h"

/* Generate all method boilerplate. */
#define fobj_method(method) fobj__define_method(method)
/*
 * Ensure method initialized.
 * Calling fobj_method_init is not required,
 * unless you want search method by string name or use `fobj_freeze`
 */
#define fobj_method_init(method) fobj__method_init(method)

/* Declare klass handle */
#define fobj_klass(klass) fobj__klass_declare(klass)
/*
 * Implement klass handle.
 * Here all the binding are done, therefore it should be called
 * after method implementions or at least prototypes.
 * Additional declarations could be passed here.
 */
#define fobj_klass_handle(klass, ...) fobj__klass_handle(klass, __VA_ARGS__)
/*
 * Calling fobj_klass_init is not required,
 * unless you want search klass by string name or use `fobj_freeze`
 */
#define fobj_klass_init(klass) fobj__klass_init(klass)
#define fobj_add_methods(klass, ...) fobj__add_methods(klass, __VA_ARGS__)

#define fobj_iface(iface) fobj__iface_declare(iface)

/*
 * Allocate klass instance, and optionally copy fields.
 *
 * fobj_alloc(klass)
 * $alloc(klass)
 *      allocates instance
 * fobj_alloc(klass, .field1 = val1, .field2 = val2) -
 * $alloc(klass, .field1 = val1, .field2 = val2) -
 *      allocates instance
 *      copies `(klass){.field1 = val1, .field2 = val2}`
 */
#define fobj_alloc(klass, ...) \
    fobj__alloc(klass, __VA_ARGS__)
#define $alloc(klass, ...) \
    fobj__alloc(klass, __VA_ARGS__)

/*
 * Allocate variable sized instance with additional size.
 * Size should be set in bytes, not variable sized field elements count.
 * Don't pass variable sized fields as arguments, they will not be copied.
 * Fill variable sized fields later.
 *
 * fobj_alloc_sized(Klass, size)
 *      allocates instance with custom additional `size`
 *      returns obj
 * fobj_alloc_sized(Klass, size, .field1 = val1, .field2 = val2)
 *      allocates instance with custom additional `size`
 *      copies `(klass){.field1 = val1, .field2 = val2}`
 *      returns obj
 */
#define fobj_alloc_sized(klass, size, ...) \
    fobj__alloc_sized(klass, size, __VA_ARGS__)

/*
 * Manual reference counting.
 *
 * Managing reference from object fields to other objects needs to use
 * automatic reference counting.
 *
 * $ref(obj), fobj_retain(obj)
 *      Manually increment reference count.
 *      It will prevent object from destroying.
 * $del(&var), fobj_del(&var), fobj_release(obj)
 *      Manually decrement reference count and clear variable.
 *      It will destroy object, if its reference count become zero.
 *      `$del(&var)`, `fobj_del` accept address of variable and set it to NULL.
 * $set(&var, obj), fobj_set(&var, obj)
 *      Replace value, pointed by first argument, with new value.
 *      New value will be passed to `fobj_retain` and assigned to ptr.
 *      Then old value will be passed to `fobj_release`.
 */
#define $ref(ptr)               fobj_retain(ptr)
#define $del(ptr)               fobj__del_impl(ptr)
#define fobj_del(ptr)           fobj__del_impl(ptr)
#define $set(ptr, obj)          fobj__set_impl((ptr), (obj))
#define fobj_set(ptr, obj)      fobj__set_impl((ptr), (obj))
/*
 * $idel(iface), fobj_iface_del(iface)
 *      Calls fobj_release(iface.self) and clears iface.
 */
#define $idel(iface)            fobj__idel(iface)
#define fobj_iface_del(iface)   fobj__idel(iface)

extern fobj_klass_handle_t fobj_klass_of(fobj_t);
extern fobj_t fobj_retain(fobj_t);
extern void fobj_release(fobj_t);

/*
 * fobjDispose should finish all object's activity and release resources.
 * It is called automatically before destroying object, but could be
 * called manually as well using `fobj_dispose` function. `fobjDispose` could
 * not be called directly.
 * Therefore after fobjDispose object should be accessible, ie call of any
 * method should not be undefined. But it should not be usable, ie should
 * not do any meaningful job.
 */
#define mth__fobjDispose    void
fobj__special_void_method(fobjDispose);
#define $dispose(obj) fobj_dispose(obj)
extern void fobj_dispose(fobj_t);

/* check if object is disposing or was disposed */
extern bool fobj_disposing(fobj_t);
extern bool fobj_disposed(fobj_t);

/*
 * returns globally allocated klass name.
 * DO NOT modify it.
 */
extern const char *fobj_klass_name(fobj_klass_handle_t klass);

/*
 * Call method with named/optional args.
 *
 *      $(someMethod, object)
 *      $(someMethod, object, v1, v2)
 *      $(someMethod, object, .arg1=v1, .arg2=v2)
 *      $(someMethod, object, .arg2=v2, .arg1=v1)
 *      // Skip optional .arg3
 *      $(someMethod, object, v1, v2, .arg4=v4)
 *      $(someMethod, object, .arg1=v1, .arg2=v2, .arg4=v4)
 *      // Order isn't important with named args.
 *      $(someMethod, object, .arg4=v4, .arg1=v1, .arg2=v2)
 *      $(someMethod, object, .arg4=v4, .arg2=v2, .arg1=v1)
 *
 *      fobj_call(someMethod, object)
 *      fobj_call(someMethod, object, v1, v2)
 *      fobj_call(someMethod, object, .arg1=v1, .arg2=v2)
 *      fobj_call(someMethod, object, v1, v2, .arg4=v4)
 *      fobj_call(someMethod, object, .arg1=v1, .arg2=v2, .arg4=v4)
 */
#define $(meth, self, ...) \
    fobj_call(meth, self, __VA_ARGS__)

/*
 * Call parent klass method implementation with named/optional args.
 *
 *      $super(someMethod, object)
 *      $super(someMethod, object, v1, v2, .arg4=v4)
 *      $super(someMethod, object, .arg1=v1, .arg2=v2, .arg4=v4)
 *      fobj_call_super(someMethod, object)
 *      fobj_call_super(someMethod, object, v1, v2)
 *      fobj_call_super(someMethod, object, v1, v2, .arg4=v4)
 *      fobj_call_super(someMethod, object, .arg1=v1, .arg2=v2, .arg4=v4)
 *
 * It uses variable set inside of Self(klass) statement.
 */
#define $super(meth, self, ...) \
    fobj_call_super(meth, fobj__klassh, self, __VA_ARGS__)

/*
 * Call method stored in the interface struct.
 * Interface is passed by value, not pointer.
 *
 *      SomeIface_i someIface = bind_SomeIface(obj);
 *      $i(someMethod, someIface)
 *      $i(someMethod, someIface, v1, v2, .arg4=v4)
 *      $i(someMethod, someIface, .arg1=v1, .arg2=v2, .arg4=v4)
 *      fobj_iface_call(someMethod, someIface)
 *      fobj_iface_call(someMethod, someIface, v1, v2)
 *      fobj_iface_call(someMethod, someIface, v1, v2, .arg4=v4)
 *      fobj_iface_call(someMethod, someIface, .arg1=v1, .arg2=v2, .arg4=v4)
 */
#define $i(meth, iface, ...) \
    fobj_iface_call(meth, iface, __VA_ARGS__)

/*
 * Determine if object implements interface.
 *
 *      if ($implements(someIface, object, &iface_var)) {
 *          $i(someMethod, iface_var);
 *      }
 *
 *      if ($implements(someIface, object)) {
 *          workWith(object);
 *      }
 *
 *      if (fobj_implements(iface, object, &iface_var)) {
 *          fobj_iface_call(someMethod, iface_var);
 *      }
 *
 *      if (fobj_implements(iface, object)) {
 *          workWith(object);
 *      }
 *
 *  And without macroses:
 *
 *      if (implements_someIface(object, &iface_var)) {
 *          $i(someMethod, iface_var);
 *      }
 *
 *      if (implements_someIface(object, NULL)) {
 *          workWith(object);
 *      }
 */
#define $implements(iface, obj, ...) \
    fobj__implements(iface, obj, __VA_ARGS__)
#define fobj_implements(iface, obj, ...) \
    fobj__implements(iface, obj, __VA_ARGS__)

/*
 * Determine if optional method is filled in interface.
 * Note: required methods are certainly filled.
 *
 *      if ($ifilled(someMethod, iface)) {
 *          $i(someMethod, iface);
 *      }
 *
 *      if (fobj_iface_filled(someMethod, iface)) {
 *          fobj_iface_call(someMethod, iface);
 *      }
 */
#define $ifilled(meth, iface) \
    fobj_iface_filled(meth, iface)

/*
 * Call method if it is defined, and assign result.
 *
 *      value_t val;
 *      if ($ifdef(val =, someMethod, self, v1, v2, .arg4=v4)) {
 *          ...
 *      }
 *
 *  or doesn't assign result
 *
 *      if ($ifdef(, someMethod, self, v1, v2, .arg4=v4)) {
 *          ...
 *      }
 */
#define $ifdef(assignment, meth, self, ...) \
    fobj_ifdef(assignment, meth, (self), __VA_ARGS__)

#define $iifdef(assignment, meth, iface, ...) \
    fobj_iface_ifdef(assignment, meth, iface, __VA_ARGS__)

/* Autorelease pool */
#define FOBJ_FUNC_ARP() FOBJ_ARP_POOL(fobj__func_ar_pool)
#define FOBJ_LOOP_ARP() FOBJ_ARP_POOL(fobj__loop_ar_pool)
#define FOBJ_BLOCK_ARP() FOBJ_ARP_POOL(fobj__block_ar_pool)

/* put into current autorelease pool */
#define $adel(obj) fobj_autorelease(obj)
/* increment reference and put into current autorelease pool */
#define $aref(obj) fobj_autorelease($ref(obj))
/* increment reference and store object in parent autorelease pool */
#define $save(obj) fobj_store_to_parent_pool($ref(obj), NULL)
/* increment reference and store object in autorelease pool of calling function */
#define $returning(obj) fobj_store_to_parent_pool($ref(obj), &fobj__func_ar_pool)
#define $return(obj)    return $returning(obj)

/*
 * Base type
 */
#define mth__fobjRepr   struct fobjStr*
fobj_method(fobjRepr);
#define mth__fobjKlass  fobj_klass_handle_t
fobj_method(fobjKlass);

typedef struct fobjBase {
    char __base[0];
} fobjBase;
#define kls__fobjBase mth(fobjKlass, fobjRepr)
fobj_klass(fobjBase);

/*
 * Base type for error.
 * Makes strdup on message in initialization.
 */
typedef struct fobjErr fobjErr;
struct fobjErr {
    const char* message;
    fobjErr*    cause;   /* cause: wrapped error */
    fobjErr*    sibling; /* sibling error */
};
#define kls__fobjErr mth(fobjDispose, fobjErrMsg)
fobj_klass(fobjErr);

/* returns string owned by error object(?) */
#define mth__fobjErrMsg       const char*
fobj_method(fobjErrMsg);

static inline fobjErr*
fobj_error(const char *message) {
    return fobj_alloc(fobjErr, .message = ft_strdup(message));
}

/*
 * Combines two error by placing second into single linked list of siblings.
 * If either of error is NULL, other error is returned.
 * If both errors are NULL, then NULL is returned.
 * If second already has siblings, first's list of siblings is appended to
 * second's list, then second becames first sibling of first.
 */
extern fobjErr* fobj_err_combine(fobjErr* first, fobjErr* second);

#define fobj_reset_err(err) do { ft_dbg_assert(err); *err = NULL; } while(0)

/* Varsized null terminated string */
typedef struct fobjStr {
    uint32_t    len;
    char        s[1];
} fobjStr;

extern fobjStr*     fobj_newstrn(const char* s, size_t len);
static ft_unused inline fobjStr*
fobj_newcstr(const char* s) {
    return fobj_newstrn(s, strlen(s));
}

ft_gnu_printf(1, 2)
extern fobjStr* fobj_sprintf(const char* fmt, ...);
extern fobjStr* fobj_strncat(fobjStr *ostr, const char* str, size_t add_len);
static ft_unused inline fobjStr*
fobj_cstrcat(fobjStr *ostr, const char *str) {
    return fobj_strncat(ostr, str, strlen(str));
}
static ft_unused inline fobjStr*
fobj_strcat(fobjStr *ostr, fobjStr *other) {
    return fobj_strncat(ostr, other->s, other->len);
}
ft_gnu_printf(2, 3)
extern fobjStr* fobj_strcatf(fobjStr *str, const char* fmt, ...);

#define kls__fobjStr varsized(s)
fobj_klass(fobjStr);

#endif
