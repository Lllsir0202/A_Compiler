#ifndef UTILS_INCLUDE_ROOT_H
#define UTILS_INCLUDE_ROOT_H
// Intentionally empty: exists so a build system that derives -I from header
// directories adds "utils/" to the include path (the code uses includes relative
// to it, e.g. "targets/...", "llvm/...", matching the Makefile's -Iutils).
#endif
