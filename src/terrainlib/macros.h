#pragma once

#if defined(__has_builtin)
#define HAS_BUILTIN(x) __has_builtin(x)
#else
#define HAS_BUILTIN(x) 0
#endif

#define CAT_IDENT_IMPL(a, b) a##b
#define CAT_IDENT(a, b) CAT_IDENT_IMPL(a, b)
#define UNIQUE_IDENT(prefix) CAT_IDENT(prefix, __COUNTER__)
