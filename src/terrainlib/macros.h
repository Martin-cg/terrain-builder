#pragma once

#if defined(__has_builtin)
#define ALP_HAS_BUILTIN(x) __has_builtin(x)
#else
#define ALP_HAS_BUILTIN(x) 0
#endif

#define ALP_CAT_IDENT_IMPL(a, b) a##b
#define ALP_CAT_IDENT(a, b) ALP_CAT_IDENT_IMPL(a, b)
#define ALP_UNIQUE_IDENT(prefix) ALP_CAT_IDENT(prefix, __COUNTER__)
