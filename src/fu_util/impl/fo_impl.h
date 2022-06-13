/* vim: set expandtab autoindent cindent ts=4 sw=4 sts=4 */
#ifndef FOBJ_OBJ_PRIV_H
#define FOBJ_OBJ_PRIV_H


#define Self_impl(Klass) \
    Klass * self = Vself; fobj_klass_handle_t fobj__klassh ft_unused = fobj__nm_khandle(Klass)()

typedef uint16_t fobj_klass_handle_t;
typedef uint16_t fobj_method_handle_t;

/* Named argument handling tools */
#ifdef __clang__
#define fobj__push_ignore_initializer_overrides \
    _Pragma("clang diagnostic push"); \
    _Pragma("clang diagnostic ignored \"-Winitializer-overrides\"")
#define fobj__pop_ignore_initializer_overrides \
    _Pragma("clang diagnostic pop")
#else
#define fobj__push_ignore_initializer_overrides
#define fobj__pop_ignore_initializer_overrides
#endif

#ifndef NDEBUG
typedef struct { char is_set: 1; } *fobj__missing_argument_detector;
#else
typedef struct fobj__missing_argument_detector {
} fobj__missing_argument_detector;
#endif
#ifndef NDEBUG
#define fobj__dumb_arg ((void*)(uintptr_t)1)
#define fobj__check_arg(name) ft_dbg_assert(fobj__nm_given(name) != NULL);
#else
#define fobj__dumb_arg {}
#define fobj__check_arg(name)
#endif

typedef struct {
    fobj_method_handle_t meth;
    void*                impl;
} fobj__method_impl_box_t;

/* param to tuple coversion */

#define fobj__map_params(...) \
    fm_eval(fm_foreach_comma(fobj__map_param, __VA_ARGS__))
#define fobj__map_param(param) \
    fm_cat(fobj__map_param_, param)
#define fobj__map_param_varsized(...)       (varsized, __VA_ARGS__)
#define fobj__map_param_mth(...)            (mth, __VA_ARGS__)
#define fobj__map_param_opt(...)            (opt, __VA_ARGS__)
#define fobj__map_param_iface(...)          (iface, __VA_ARGS__)
#define fobj__map_param_inherits(parent)    (inherits, parent)

/* Standard naming */

#define fobj__nm_mth(meth)          mth__##meth
#define fobj__nm_mthdflt(meth)      mth__##meth##__optional
#define fobj__nm_kls(klass)         kls__##klass
#define fobj__nm_iface(iface)       iface__##iface
#define fobj__nm_mhandle(meth)      meth##__mh
#define fobj__nm_params_t(meth)     meth##__params_t
#define fobj__nm_invoke(meth)       fobj__invoke_##meth
#define fobj__nm_impl_t(meth)       meth##__impl
#define fobj__nm_cb(meth)           fobj__fetch__##meth
#define fobj__nm_cb_t(meth)         meth##__cb
#define fobj__nm_register(meth)     fobj__register_##meth /* due to tcc bug, we can't use meth##__register */
#define fobj__nm_wrap_decl(meth)    fobj__wrap_decl_##meth
#define fobj__nm_meth_i(meth)       meth##_i
#define fobj__nm_bind(m_or_i)       bind_##m_or_i
#define fobj__nm_bindref(m_or_i)    bindref_##m_or_i
#define fobj__nm_implements(m_or_i) implements_##m_or_i
#define fobj__nm_khandle(klass)     klass##__kh
#define fobj__nm_klass_meth(klass, meth) klass##_##meth
#define fobj__nm_iface_i(iface)     iface##_i
#define fobj__nm_given(param)       param##__given
#define fobj__nm_kvalidate(m_or_i)  fobj__klass_validate_##m_or_i

/* Method definition */

#define fobj__define_method(meth) \
    fobj__method_declare_i(meth, fobj__nm_mth(meth))
#define fobj__method_declare_i(meth, ...) \
    fobj__method_declare(meth, __VA_ARGS__)

#define fobj__method_declare(meth, res, ...) \
    fobj__method_declare_impl(meth, \
            fobj__nm_mhandle(meth), \
            fobj__nm_params_t(meth), \
            fobj__nm_invoke(meth), \
            fobj__nm_impl_t(meth), \
            fobj__nm_cb(meth), \
            fobj__nm_cb_t(meth), \
            fobj__nm_register(meth), \
            fobj__nm_wrap_decl(meth), \
            fobj__nm_meth_i(meth), \
            fobj__nm_bind(meth), \
            fobj__nm_bindref(meth), \
            fobj__nm_implements(meth), \
            fobj__nm_kvalidate(meth), \
            fm_va_comma(__VA_ARGS__), \
            res, __VA_ARGS__)

#define fobj__method_declare_impl(meth, handle, \
        params_t, \
        invoke_methparams, \
        impl_meth_t, \
        cb_meth, cb_meth_t, \
        _register_meth, wrap_decl, \
        meth_i, bind_meth, bindref_meth, implements_meth, \
        kvalidate, comma, res, ...) \
        \
        static ft_unused fobj_method_handle_t handle(void) { \
            static volatile fobj_method_handle_t hndl = 0; \
            fobj_method_handle_t h = hndl; \
            if (h) return h; \
            fobj_method_init_impl(&hndl, fm_str(meth)); \
            return hndl; \
        } \
        \
        typedef res (* impl_meth_t)(fobj_t self comma fobj__mapArgs_toArgs(__VA_ARGS__)); \
        \
        typedef struct params_t { \
            fobj__missing_argument_detector fobj__dumb_first_param; \
            fobj__mapArgs_toFields(__VA_ARGS__) \
        } params_t; \
        \
        typedef struct cb_meth_t { \
            fobj_t      self; \
            impl_meth_t impl; \
        } cb_meth_t; \
        \
        ft_inline cb_meth_t \
        cb_meth(fobj_t self, fobj_klass_handle_t parent) { \
            fobj__method_callback_t fnd = {NULL, NULL}; \
            if (self != NULL) { \
                fnd = fobj_method_search(self, handle(), parent); \
            } \
            return (cb_meth_t){fnd.self, fnd.impl}; \
        } \
        \
        ft_inline res \
        meth(fobj_t self comma fobj__mapArgs_toArgs(__VA_ARGS__)) { \
            cb_meth_t cb = cb_meth(self, fobj_self_klass); \
            ft_assert(cb.impl != NULL && cb.self != NULL); \
            ft_dbg_assert(!fobj_disposed(cb.self)); \
            return cb.impl(cb.self comma fobj__mapArgs_toNames(__VA_ARGS__)); \
        } \
        \
        ft_inline void \
        _register_meth(fobj_klass_handle_t klass, impl_meth_t cb) { \
            fobj_method_register_impl(klass, handle(), (void *)cb); \
        } \
        \
        ft_inline fobj__method_impl_box_t \
        wrap_decl(impl_meth_t cb) { \
            return (fobj__method_impl_box_t) { handle(), cb }; \
        } \
        \
        typedef struct meth_i { \
            fobj_t self; \
            cb_meth_t meth; \
        } meth_i;\
        \
        ft_inline meth_i \
        bind_meth(fobj_t self) { \
            meth_i _iface = (meth_i){.self = self, .meth = cb_meth(self, fobj_self_klass)}; \
            ft_assert(_iface.meth.impl != NULL); \
            return _iface; \
        } \
        \
        ft_inline bool \
        implements_meth(fobj_t self, meth_i *ifacep) { \
            meth_i _iface = (meth_i){.self = self, .meth = cb_meth(self, fobj_self_klass)}; \
            if (ifacep != NULL) \
                *ifacep = _iface.meth.impl != NULL ? _iface : (meth_i){NULL}; \
            return _iface.meth.impl != NULL; \
        } \
        \
        ft_inline meth_i \
        bindref_meth(fobj_t self) { \
            meth_i _iface = (meth_i){.self = self, .meth = cb_meth(self, fobj_self_klass)}; \
            ft_assert(_iface.meth.impl != NULL); \
            fobj_retain(_iface.self); \
            return _iface; \
        } \
        \
        ft_inline void \
        kvalidate(fobj_klass_handle_t khandle) { \
            ft_assert(fobj_klass_method_search(khandle, handle()) != NULL); \
        } \
        \
        ft_inline res \
        invoke_methparams(cb_meth_t cb, params_t params) { \
            ft_assert(cb.impl != NULL && cb.self != NULL); \
            ft_dbg_assert(!fobj_disposed(cb.self)); \
            fobj__assertArgs(__VA_ARGS__) \
            return cb.impl(cb.self comma fobj__mapArgs_toNamedParams(__VA_ARGS__)); \
        } \
        \
        fm__dumb_require_semicolon

#define fobj__mapArgs_toArgs_do(x, y, ...) x y
#define fobj__mapArgs_toArgs(...) \
    fm_eval(fm_foreach_tuple_comma(fobj__mapArgs_toArgs_do, __VA_ARGS__))

#define fobj__mapArgs_toFields_do(x, y, ...)  \
    x y; \
    fobj__missing_argument_detector fobj__nm_given(y);
#define fobj__mapArgs_toFields(...) \
    fm_eval(fm_foreach_tuple(fobj__mapArgs_toFields_do, __VA_ARGS__))

#define fobj__mapArgs_toNames_do(x, y, ...) y
#define fobj__mapArgs_toNames(...) \
    fm_eval(fm_foreach_tuple_comma(fobj__mapArgs_toNames_do, __VA_ARGS__))

#define fobj__mapArgs_toNamedParams_do(x, y, ...) params.y
#define fobj__mapArgs_toNamedParams(...) \
    fm_eval(fm_foreach_tuple_comma(fobj__mapArgs_toNamedParams_do, __VA_ARGS__))

#define fobj__assertArgs_do(x, y, ...) fobj__check_arg(params.y)
#define fobj__assertArgs(...) \
    fm_eval(fm_foreach_tuple(fobj__assertArgs_do, __VA_ARGS__))

#define fobj__special_void_method(meth) \
        \
        static ft_unused fobj_method_handle_t fobj__nm_mhandle(meth) (void) { \
            static volatile fobj_method_handle_t hndl = 0; \
            fobj_method_handle_t h = hndl; \
            if (h) return h; \
            fobj_method_init_impl(&hndl, fm_str(meth)); \
            return hndl; \
        } \
        \
        typedef void (* fobj__nm_impl_t(meth))(fobj_t self); \
        \
        ft_inline void \
        fobj__nm_register(meth)(fobj_klass_handle_t klass, fobj__nm_impl_t(meth) cb) { \
            fobj_method_register_impl(klass, fobj__nm_mhandle(meth)(), (void *)cb); \
        } \
        \
        ft_inline fobj__method_impl_box_t \
        fobj__nm_wrap_decl(meth)(fobj__nm_impl_t(meth) cb) { \
            return (fobj__method_impl_box_t) { fobj__nm_mhandle(meth)(), cb }; \
        } \
        \
        fm__dumb_require_semicolon

/* Klass declarations */

#define fobj__klass_declare(klass) \
    extern fobj_klass_handle_t fobj__nm_khandle(klass)(void) ft_gcc_const; \
    fm__dumb_require_semicolon


#define fobj__klass_handle(klass, ...) \
    fobj__klass_handle_i(klass, \
            fobj__map_params(fobj__nm_kls(klass)) \
            fm_va_comma(__VA_ARGS__) fobj__map_params(__VA_ARGS__))
#define fobj__klass_handle_i(klass, ...) \
    fobj__klass_handle_impl(klass, __VA_ARGS__)
#define fobj__klass_handle_impl(klass, ...) \
    fobj_klass_handle_t fobj__nm_khandle(klass) (void) { \
        static volatile fobj_klass_handle_t hndl = 0; \
        fobj_klass_handle_t khandle = hndl; \
        fobj_klass_handle_t kparent = fobjBase__kh(); \
        ssize_t kls_size = sizeof(klass); \
        if (khandle) return khandle; \
        fm_eval_tuples_arg(fobj__klass_detect_size, klass, __VA_ARGS__) \
        { \
            fobj__method_impl_box_t methods[] = { \
                fobj__klass_decl_methods(klass, __VA_ARGS__) \
                { 0, NULL } \
            }; \
            if (fobj_klass_init_impl(&hndl, kls_size, kparent, methods, fm_str(klass))) \
                return hndl; \
        } \
        khandle = hndl; \
        fm_when(fm_isnt_empty(fobj__klass_has_iface(__VA_ARGS__))) ( \
            fobj__klass_check_iface(klass, __VA_ARGS__) \
        ) \
        return khandle; \
    } \
    fm__dumb_require_semicolon

#define fobj__klass_detect_size_varsized_1(klass, fld, ...) \
    kls_size = -1-offsetof(klass,fld);
#define fobj__klass_detect_size_varsized_0(klass, ...) \
    kls_size = -1-sizeof(klass);
#define fobj__klass_detect_size_varsized(klass, ...) \
    fm_cat(fobj__klass_detect_size_varsized_, fm_va_01(__VA_ARGS__))(klass, __VA_ARGS__)
#define fobj__klass_detect_size_mth(...)
#define fobj__klass_detect_size_opt(...)
#define fobj__klass_detect_size_inherits(klass, parent) \
    kparent = fobj__nm_khandle(parent)();
#define fobj__klass_detect_size_iface(...)
#define fobj__klass_detect_size(klass, tag, ...) \
    fobj__klass_detect_size_##tag (klass, __VA_ARGS__)

#define fobj__method_init(meth) \
    fobj__consume(fobj__nm_mhandle(meth)())
#define fobj__klass_init(klass) \
    fobj__consume(fobj__nm_khandle(klass)())

#define fobj__klass_decl_method(klass, meth, ...) \
    fobj__nm_wrap_decl(meth)(fobj__nm_klass_meth(klass, meth)),
#define fobj__klass_decl_method_loop(klass, ...) \
    fm_foreach_arg(fobj__klass_decl_method, klass, __VA_ARGS__)

#define fobj__klass_decl_methods_mth(klass, ...) \
    fm_recurs(fobj__klass_decl_method_loop)(klass, __VA_ARGS__)
#define fobj__klass_decl_methods_opt(klass, ...) \
    fm_recurs(fobj__klass_decl_method_loop)(klass, __VA_ARGS__)
#define fobj__klass_decl_methods_varsized(...)
#define fobj__klass_decl_methods_inherits(klass, parent)
#define fobj__klass_decl_methods_iface(...)
#define fobj__klass_decl_methods_dispatch(klass, tag, ...) \
    fobj__klass_decl_methods_##tag(klass, __VA_ARGS__)
#define fobj__klass_decl_methods(klass, ...) \
    fm_eval(fm_foreach_tuple_arg(\
                fobj__klass_decl_methods_dispatch, klass, __VA_ARGS__))

#define fobj__klass_has_iface_varsized
#define fobj__klass_has_iface_mth
#define fobj__klass_has_iface_opt
#define fobj__klass_has_iface_inherits
#define fobj__klass_has_iface_iface 1
#define fobj__klass_has_iface_impl(tag, ...) \
    fobj__klass_has_iface_##tag
#define fobj__klass_has_iface(...) \
    fm_eval_tuples(fobj__klass_has_iface_impl, __VA_ARGS__)

#define fobj__klass_check_dispatch_varsized(...)
#define fobj__klass_check_dispatch_mth(...)
#define fobj__klass_check_dispatch_opt(...)
#define fobj__klass_check_dispatch_inherits(...)
#define fobj__klass_check_dispatch_iface(klass, ...) \
    fm_recurs(fobj__klass_check_dispatch_iface_i)(klass, __VA_ARGS__)
#define fobj__klass_check_dispatch_iface_i(klass, ...) \
    fm_foreach_arg(fobj__klass_check_one_iface, klass, __VA_ARGS__)
#define fobj__klass_check_one_iface(klass, iface) \
    fobj__nm_kvalidate(iface)(khandle);
#define fobj__klass_check_dispatch(klass, tag, ...) \
    fobj__klass_check_dispatch_##tag(klass, __VA_ARGS__)
#define fobj__klass_check_iface(klass, ...) \
    fm_eval_tuples_arg(fobj__klass_check_dispatch, klass, __VA_ARGS__)

#define fobj__add_methods_loop(klass, ...) \
    fm_foreach_arg(fobj__add_methods_do, klass, __VA_ARGS__)
#define fobj__add_methods_do(klass, meth, ...) \
    fm_recurs(fobj__add_methods_do_)(klass, meth, ...)
#define fobj__add_methods_do_(klass, meth, ...) \
    fobj__nm_register(meth)(\
            fobj__nm_khandle(klass)(), \
            fobj__nm_klass_meth(klass, meth));

/* add methods after class declaration */

#define fobj__add_methods(klass, ...) do { \
    fobj_klass_handle_t khandle = fobj__nm_khandle(klass)(); \
    fm_eval(fobj__add_methods_loop(klass, __VA_ARGS__)) \
} while (0)

/* Instance creation */
#define fobj__alloc(klass, ...) \
    fm_cat(fobj__alloc_, fm_va_01(__VA_ARGS__))(klass, fobj__nm_khandle(klass), -1, __VA_ARGS__)
#define fobj__alloc_sized(klass, size, ...) \
    fm_cat(fobj__alloc_, fm_va_01(__VA_ARGS__))(\
            klass, fobj__nm_khandle(klass),  (size), __VA_ARGS__)
#define fobj__alloc_0(klass, khandle, size, ...) \
    ((klass *)fobj__allocate(khandle(), NULL, size))
#define fobj__alloc_1(klass, khandle, size, ...) \
    ((klass *)fobj__allocate(khandle(), &(klass){__VA_ARGS__}, size))

/* Interface declaration */

#define fobj__iface_declare(iface) \
    fobj__iface_declare_i(iface, fobj__map_params(fobj__nm_iface(iface)))
#define fobj__iface_declare_i(iface, ...) \
    fobj__iface_declare_impl(iface, \
            fobj__nm_iface_i(iface), fobj__nm_bind(iface), \
            fobj__nm_bindref(iface), fobj__nm_implements(iface), \
            fobj__nm_kvalidate(iface), __VA_ARGS__)

#define fobj__iface_declare_impl(iface, iface_i, \
                                bind_iface, bindref_iface, implements_iface, \
                                kvalidate, ...) \
    typedef struct iface_i { \
        fobj_t self; \
        fobj__mapMethods_toFields(__VA_ARGS__) \
    } iface_i; \
    \
    static ft_unused inline iface_i \
    bind_iface(fobj_t self) { \
        iface_i _iface = (iface_i){ .self = self }; \
        fobj__mapMethods_toSetters(__VA_ARGS__) \
        return _iface; \
    } \
    \
    static ft_unused inline bool \
    implements_iface(fobj_t self, iface_i *ifacep) { \
        iface_i _iface = (iface_i){ .self = self }; \
        bool    all_ok = true; \
        fobj__mapMethods_toIfSetters(__VA_ARGS__) \
        if (ifacep != NULL) \
            *ifacep = all_ok ? _iface : (iface_i){NULL}; \
        return all_ok; \
    } \
    \
    static ft_unused inline iface_i \
    bindref_iface(fobj_t self) { \
        iface_i _iface = bind_iface(self); \
        fobj_retain(_iface.self); \
        return _iface; \
    } \
    \
    ft_inline void \
    kvalidate(fobj_klass_handle_t khandle) { \
        fobj__kvalidateMethods(__VA_ARGS__) \
    } \
    \
    fm__dumb_require_semicolon

#define fobj__mapMethods_toFields_do_do(m) fobj__nm_cb_t(m) m;
#define fobj__mapMethods_toFields_loop(...) \
    fm_foreach(fobj__mapMethods_toFields_do_do, __VA_ARGS__)
#define fobj__mapMethods_toFields_do(tag, ...) \
    fm_recurs(fobj__mapMethods_toFields_loop)(__VA_ARGS__)
#define fobj__mapMethods_toFields(...) \
    fm_eval_tuples(fobj__mapMethods_toFields_do, __VA_ARGS__)

#define fobj__mapMethods_toSetters_do_opt(meth) \
    _iface.meth = fobj__nm_cb(meth)(self, fobj_self_klass);
#define fobj__mapMethods_toSetters_do_mth(meth) \
    _iface.meth = fobj__nm_cb(meth)(self, fobj_self_klass); \
    ft_assert(_iface.meth.impl != NULL);
#define fobj__mapMethods_toSetters_loop(tag, ...) \
    fm_foreach(fobj__mapMethods_toSetters_do_##tag, __VA_ARGS__)
#define fobj__mapMethods_toSetters_do(tag, ...) \
    fm_recurs(fobj__mapMethods_toSetters_loop)(tag, __VA_ARGS__)
#define fobj__mapMethods_toSetters(...) \
    fm_eval_tuples(fobj__mapMethods_toSetters_do, __VA_ARGS__)

#define fobj__mapMethods_toIfSetters_do_opt(meth) \
    _iface.meth = fobj__nm_cb(meth)(self, fobj_self_klass);
#define fobj__mapMethods_toIfSetters_do_mth(meth) \
    _iface.meth = fobj__nm_cb(meth)(self, fobj_self_klass); \
    if (_iface.meth.impl == NULL) all_ok = false;
#define fobj__mapMethods_toIfSetters_loop(tag, ...) \
    fm_foreach(fobj__mapMethods_toIfSetters_do_##tag, __VA_ARGS__)
#define fobj__mapMethods_toIfSetters_do(tag, ...) \
    fm_recurs(fobj__mapMethods_toIfSetters_loop)(tag, __VA_ARGS__)
#define fobj__mapMethods_toIfSetters(...) \
    fm_eval_tuples(fobj__mapMethods_toIfSetters_do, __VA_ARGS__)

#define fobj__kvalidateMethods_do_opt(meth)
#define fobj__kvalidateMethods_do_mth(meth) \
    fobj__nm_kvalidate(meth)(khandle);
#define fobj__kvalidateMethods_loop(tag, ...) \
    fm_foreach(fobj__kvalidateMethods_do_##tag, __VA_ARGS__)
#define fobj__kvalidateMethods_do(tag, ...) \
    fm_recurs(fobj__kvalidateMethods_loop)(tag, __VA_ARGS__)
#define fobj__kvalidateMethods(...) \
    fm_eval_tuples(fobj__kvalidateMethods_do, __VA_ARGS__)

/* Method invocation */

#define fobj_call(meth, self, ...) \
    fobj__nm_invoke(meth)(fobj__nm_cb(meth)(self, fobj_self_klass), fobj_pass_params(meth, __VA_ARGS__))

#define fobj_call_super(meth, _klassh, self, ...) \
    fobj__nm_invoke(meth)(fobj__nm_cb(meth)(self, _klassh), fobj_pass_params(meth, __VA_ARGS__))

#define fobj_iface_call(meth, iface, ...) \
    fobj__nm_invoke(meth)((iface).meth, fobj_pass_params(meth, __VA_ARGS__))

#define fobj__implements(iface, self, ...) \
    (fobj__nm_implements(iface)(self, fm_if(fm_no_va(__VA_ARGS__), NULL, __VA_ARGS__)))

#define fobj_iface_filled(meth, iface) \
    ((iface).meth.impl != NULL)

#define fobj_ifdef(assignment, meth, self, ...) \
    fobj__ifdef_impl(assignment, meth, (self), \
            fm_uniq(cb), fobj__nm_cb(meth), fobj__nm_cb_t(meth), \
            fobj__nm_invoke(meth), __VA_ARGS__)
#define fobj__ifdef_impl(assignment, meth, self_, cb, cb_meth, cb_meth_t, \
            invoke_meth__params, ...) ({ \
            cb_meth_t cb = cb_meth(self_, fobj_self_klass); \
            if (cb.impl != NULL) { \
                assignment invoke_meth__params(cb, fobj_pass_params(meth, __VA_ARGS__)); \
            } \
            cb.impl != NULL; \
            })

#define fobj_iface_ifdef(assignment, meth, iface, ...) \
    fobj__iface_ifdef_impl(assignment, meth, (iface), \
            fm_uniq(cb), fobj__nm_cb_t(meth), \
            fobj__nm_invoke(meth), __VA_ARGS__)
#define fobj__iface_ifdef_impl(assignment, meth, iface, cb, cb_meth_t, \
            invoke_meth__params, ...) ({ \
            cb_meth_t cb = iface.meth; \
            if (cb.impl != NULL) { \
                assignment invoke_meth__params(cb, fobj_pass_params(meth, __VA_ARGS__)); \
            } \
            cb.impl != NULL; \
            })

/* Named params passing hazzles with optional and defaults */

#define fobj_pass_params(meth, ...) \
    fm_cat(fobj__pass_params_impl_, fm_no_va(__VA_ARGS__))( \
            meth, fobj__nm_params_t(meth), __VA_ARGS__)
#define fobj__pass_params_impl_1(meth, meth__params_t, ...) \
    ((meth__params_t){fobj__params_defaults(meth)})
#ifndef __clang__
#define fobj__pass_params_impl_0(meth, meth__params_t, ...) \
    ((meth__params_t){\
     fobj__params_defaults(meth), \
     fm_eval(fm_foreach_comma(fobj__pass_params_each, __VA_ARGS__)) \
     })
#else
#define fobj__pass_params_impl_0(meth, meth__params_t, ...) \
    ({ \
     fobj__push_ignore_initializer_overrides; \
     meth__params_t _this_is_params = { \
     fobj__params_defaults(meth), \
     fm_eval(fm_foreach_comma(fobj__pass_params_each, __VA_ARGS__)) \
     }; \
     fobj__pop_ignore_initializer_overrides; \
     _this_is_params; \
     })
#endif

#define fobj__pass_params_each(param) \
    param, fobj__dumb_arg

#define fobj__params_defaults(meth) \
    fobj__params_defaults_i(meth, fobj__nm_mthdflt(meth)()) \
    .fobj__dumb_first_param = fobj__dumb_arg
#define fobj__params_defaults_i(meth, ...) \
    fm_when(fm_is_tuple(fm_head(__VA_ARGS__))) ( \
        fobj__params_defaults_impl(__VA_ARGS__) \
    )
#define fobj__params_defaults_impl(...) \
    fm_eval(fm_foreach_tuple(fobj__params_defaults_each, __VA_ARGS__))
#define fobj__params_defaults_each(x, ...) \
    fm_when(fm_isnt_empty(__VA_ARGS__))( .x = __VA_ARGS__, )\
    .fobj__nm_given(x) = fobj__dumb_arg,


/* Declarations "private" implementation functions */
extern bool fobj_method_init_impl(volatile fobj_method_handle_t *meth,
                                  const char *name);
extern void fobj_method_register_impl(fobj_klass_handle_t klass,
                                      fobj_method_handle_t meth,
                                      void* impl);
extern bool fobj_klass_init_impl(volatile fobj_klass_handle_t *klass,
                                 ssize_t size,
                                 fobj_klass_handle_t parent,
                                 fobj__method_impl_box_t *methods,
                                 const char *name);
extern void* fobj__allocate(fobj_klass_handle_t klass,
                            void *init,
                            ssize_t size);

/* helper function to consume value to disable compiler optimizations */
extern void fobj__consume(uint16_t);

typedef struct fobj__method_callback {
    fobj_t  self;
    void*   impl;
} fobj__method_callback_t;
extern fobj__method_callback_t fobj_method_search(const fobj_t self, fobj_method_handle_t meth, fobj_klass_handle_t for_child_take_parent);

extern void* fobj_klass_method_search(fobj_klass_handle_t klass, fobj_method_handle_t meth);

/* Variable set helpers */

extern fobj_t fobj__set(fobj_t *ptr, fobj_t val);
#define fobj__set_impl(ptr, obj) do { \
    __typeof(&(**ptr)) fm_uniq(_validate_ptrptr_) ft_unused = NULL; \
    fobj__set((void**)(ptr), (obj)); \
} while (0)
#define fobj__del_impl(ptr) do { \
    __typeof(&(**ptr)) fm_uniq(_validate_ptrptr_) ft_unused = NULL; \
    fobj__set((void**)(ptr), NULL); \
} while (0)
#define fobj__idel(iface) \
    fobj__idel_impl((iface), fm_uniq(iface))
#define fobj__idel_impl(iface_, iface) do { \
    __typeof(&(*iface_.self)) fm_uniq(_validate_ptrptr_) ft_unused = NULL; \
    __typeof(&iface_) iface = &iface_; \
    fobj__set((void**)&iface->self, NULL); \
    memset(iface, 0, sizeof(*iface)); \
} while (0)


/* Autorelease pool handling */

#define FOBJ_AR_CHUNK_SIZE 14
typedef struct fobj_autorelease_chunk fobj_autorelease_chunk;
struct fobj_autorelease_chunk {
    fobj_autorelease_chunk  *prev;
    uint32_t    cnt;
    fobj_t      refs[FOBJ_AR_CHUNK_SIZE];
};
typedef struct fobj__autorelease_pool_ref fobj__autorelease_pool_ref;
typedef struct fobj_autorelease_pool fobj_autorelease_pool;
struct fobj__autorelease_pool_ref {
    fobj_autorelease_pool  *parent;
    fobj_autorelease_pool **root;
};
struct fobj_autorelease_pool {
    struct fobj__autorelease_pool_ref ref;
    fobj_autorelease_chunk *last;
    fobj_autorelease_chunk  first;
};

extern fobj__autorelease_pool_ref fobj_autorelease_pool_init(fobj_autorelease_pool *pool);
extern void fobj_autorelease_pool_release(fobj_autorelease_pool *pool);
extern fobj_t fobj_autorelease(fobj_t);
extern fobj_t fobj_store_to_parent_pool(fobj_t obj,
        fobj_autorelease_pool *child_pool_or_null);

#define FOBJ_ARP_POOL(name) \
    fobj_autorelease_pool __attribute__((cleanup(fobj_autorelease_pool_release))) \
    name = {fobj_autorelease_pool_init(&name), &name.first}

#endif
