#ifndef INCLUDE_ROOT_H
#define INCLUDE_ROOT_H
// Intentionally empty. This header exists only so that build systems which
// derive their include search path from the directories that contain headers
// (e.g. the competition auto-grader: `-I` = dirname of every *.h) add the
// project root to the include path. The whole codebase uses root-relative
// includes such as "backend/targets/...", "context/...", "middleend/..." which
// require the project root on the include path.
#endif
