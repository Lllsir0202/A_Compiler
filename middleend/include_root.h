#ifndef MIDDLEEND_INCLUDE_ROOT_H
#define MIDDLEEND_INCLUDE_ROOT_H
// Intentionally empty: exists so a build system that derives -I from header
// directories adds "middleend/" to the include path (the code uses includes relative
// to it, e.g. "targets/...", "llvm/...", matching the Makefile's -Imiddleend).
#endif
