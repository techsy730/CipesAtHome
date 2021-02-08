#ifndef CRIPES_SHUTDOWN_H
#define CRIPES_SHUTDOWN_H

#include <stdbool.h>
#include "base.h"
#include "absl/base/port.h"

// In the header file so we can take the address of it.
bool _askedToShutdownVar;

bool requestShutdown();

ABSL_ATTRIBUTE_ALWAYS_INLINE inline void prefetchShutdown() {
	_PREFETCH_READ_NO_TEMPORAL_LOCALITY(&_askedToShutdownVar);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline bool askedToShutdown() {
	return ABSL_PREDICT_FALSE(_askedToShutdownVar);
}

#endif
