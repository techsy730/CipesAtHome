#include "base.h"

// Holy crap, getting stack traces is a giant pain.

#ifdef __cplusplus
extern "C" {
#endif

// Provides external definition (function body in header)
ABSL_ATTRIBUTE_UNUSED void handleMallocFailure();
ABSL_ATTRIBUTE_UNUSED void checkMallocFailed(const void* const p);
bool _abrt_from_assert = false;

#ifdef __cplusplus
} // extern "C"
#endif
