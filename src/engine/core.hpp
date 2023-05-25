#pragma once

/* STD CONTAINERS */
#include <vector>
#include <unordered_map>

#define ENGINE_API

#define SAFETY(X) \
  do { X; } while (0);
#ifdef DEBUG
#  include <stdio.h>
#  define ERROR(...) dprintf(2, __VA_ARGS__)
#  define LOG(...)   dprintf(2, __VA_ARGS__)
#  define VERIFY(X, ...)                                             \
    if (!(X)) {                                                      \
      dprintf(2, "%d [ASSERT] ### Assert error " #X ": ", __LINE__); \
      dprintf(2, __VA_ARGS__);                                       \
      fflush(stderr);                                                \
      exit(1);                                                       \
    }

#  define HARD_CHECK(X, ...) VERIFY(X, __VA_ARGS__)
#else
#  define ERROR(...)
#  define LOG(...)
#  define VERIFY(X, ...)
#  define HARD_CHECK(X, ...) \
    do { X } while (0)
#endif
