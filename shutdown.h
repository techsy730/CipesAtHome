#include <stdbool.h>
#include "absl/base/port.h"

bool _askedToShutdownVar;

bool requestShutdown();

ABSL_ATTRIBUTE_ALWAYS_INLINE inline bool askedToShutdown() {
	return ABSL_PREDICT_FALSE(_askedToShutdownVar);
}

