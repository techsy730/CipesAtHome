/*
 * A definition for a once construct for OpenMP.
 * Similar to pthread_once, except takes a block instead of taking a function pointer.
 *
 * Example usage:
 *	_cipes_openmp_once_t is_thing_done = OPENMP_ONCE_INIT
 *
 *	bool doTheThingButOnce() {
 *		OPENMP_ONCE(is_thing_done, thing_section) {
 *			bool succeeded = tryTheThing;
 *			thing_section->succeeded = succeeded;
 *		}
 *		OPENMP_ONCE_CLOSE
 *		return is_thing_done->succeeded;
 *	}
 *
 * Here is to hoping in the future openmp adds a '#pragma omp once' directive, and then this won't be needed anymore.
 *
 * All parameters to the public macros MUST be side effect free (this is repeated in the documentation for each macro).
 *
 * NOTE: These macros rely on threadprivate semantics and omp critical sections, which may have unexpected results
 * when used outside of a parallel region. It is recommended you use a separate "once API" to handle calls
 * outside a parallel region. An alternative, if there will be no threads calling from outside of a parallel region except one,
 * is a dedicated openmp_once_t for the outside of the parallel block case.
 * You can tell the difference at runtime by calling "omp_in_parallel()" from omp.h.
 */

#ifndef OPENMP_ONCE_H_
#define OPENMP_ONCE_H_

#include <stdbool.h>
#include "absl/base/port.h"

#ifdef __cplusplus
extern "C" {
#endif

// The "tried" member gets set and read by the ONCE macros.
// The "succeeded" member is free for use by the program.
// It has no special meaning to OPENMP_ONCE.
// Typically intended to be set while under the inner critical section block the user provides.
typedef struct openmp_once_t {
	// DO NOT WRITE TO THIS VARIABLE YOURSELF. The OPENMP_ONCE macros take care of it.
	// Must be written only under a critical section (usually handled by _CIPES_OPENMP_ONCE). Can be read outside of it just fine.
	bool tried;
	bool succeeded; // Usually the idea is that this gets written under block the user supplies if the underlying action succeeded, but using it is optional.
} openmp_once_t;

#define OPENMP_ONCE_INIT (openmp_once_t){false, false}

// Internal helper macro, do not use externally
// Short for CIPES_ONCE_MAKE_LOCAL_NAME
#define _CIPES_OMLN(x) _##x##_tried_local

// @formatter:off

//printf("Thread: %d Section: %s Local var: %d once_struct.tried: %d once_struct.succeeded: %d\n", omp_in_parallel() ? omp_get_thread_num() : -1, _XSTR(critical_section_name), (local_var), (once_struct).tried, (once_struct).succeeded);

// Internal helper macro, do not use externally
#define _OPENMP_ONCE_IMPL(local_var, once_struct, once_struct_volatile, critical_section_name) \
	if (ABSL_PREDICT_FALSE(!(local_var) || !((local_var) = (once_struct).tried))) \
	_Pragma(_XSTR(omp critical(critical_section_name))) \
	{ \
		/* Force read as volatile so we don't get the compiler using a cached read from above. */ \
		(local_var) = (once_struct_volatile).tried; \
		(once_struct).tried = 1; \
		if (!(local_var))


// Runs the specified block of code only once based on the state of the given state struct.
// The block is responsible for setting succeeded to true if it did succeed based on its definition
// (or leave it untouched if you don't care, you just want it run once).
// As the block following this macro runs under a OpenMP critical section, you cannot return out of it.
//
// All parameters MUST be side effect free.
//
// After closing the block, you must follow it with OPENMP_ONCE_CLOSE (either on the same line or the next, your choice)
//
// once_struct_var: An *identifier* to the appropriate openmp_once_t used as backing state (and not a pointer to it).
//   This is because it names the static variable to store thread local state based on this name.
//   If you cannot have a simple identifier for whatever reason, use OPENMP_ONCE_WITH_LOCAL_NAME.
//     If it is threadprivate, then the block will happen at most once among all threads.
//     If it is not threadprivate, then the block will be run once for every thread, but
//       all of them will be guarded by the same critical section so only one thread can
//       can have the block run at once (useful for APIs that require at most one thread call them)
//     If you need it once for every thread and no critical block, then you aren't using OpenMP's
//       tooling at all, and thus this API isn't approriate for you.
// critical_section_name: the name of the OpenMP critical section. Do not surround with quotes.
#define OPENMP_ONCE(once_struct_var, critical_section_name) \
    static bool _CIPES_OMLN(once_struct_var) = false; \
    _Pragma(_XSTR(omp threadprivate(_CIPES_OMLN(once_struct_var)))) \
    _OPENMP_ONCE_IMPL(_CIPES_OMLN(once_struct_var), once_struct_var, (volatile openmp_once_t)once_struct_var, critical_section_name)

// Runs the specified block of code only once based on the state of the given state struct.
// The block is responsible for setting succeeded to true if it did succeed based on its definition
// (or leave it untouched if you don't care, you just want it run once).
// As the block following this macro runs under a OpenMP critical section, you cannot return out of it.
//
// All parameters MUST be side effect free.
//
// After closing the block, you must follow it with OPENMP_ONCE_CLOSE (either on the same line or the next, your choice)
//
// local_name: the name of the *already defined* thread local variable to use to prevent the having to read under
//   critical section every time (once a thread sees "tried" as true, it doesn't need to take the lock anymore).
//   Must be a variable declared (and not a pointer to such) that was made thread local
//    using '#pragma omp threadprivate(threadprivate_local_name)
//    If you must have a pointer to an already declared one use OPENMP_ONCE_USE_PTRS.
// once_struct: the openmp_once_t struct to use as the backing state (not a pointer to it)
//   If you need a pointer, use OPENMP_ONCE_USE_PTRS.
//   This can be either threadprivate or not.
//     If it is threadprivate, then the block will happen at most once among all threads.
//     If it is not threadprivate, then the block will be run once for every thread, but
//       all of them will be guarded by the same critical section so only one thread can
//       can have the block run at once (useful for APIs that require at most one thread call them)
//     If you need it once for every thread and no critical block, then you aren't using OpenMP's
//       tooling at all, and thus this API isn't approriate for you.
// critical_section_name: the name of the OpenMP critical section. Do not surround with quotes.
#define OPENMP_ONCE_WITH_LOCAL_NAME(threadprivate_local_name, once_struct, critical_section_name) \
    _OPENMP_ONCE_IMPL(threadprivate_local_name, once_struct, (volatile openmp_once_t)once_struct, critical_section_name)

// Runs the specified block of code only once based on the state of the given state struct.
// The block is responsible for setting succeeded to true if it did succeed based on its definition
// (or leave it untouched if you don't care, you just want it run once).
// As the block following this macro runs under a OpenMP critical section, you cannot return out of it.
//
// All parameters MUST be side effect free.
//
// After closing the block, you must follow it with OPENMP_ONCE_CLOSE (either on the same line or the next, your choice)
//
// local_ptr: A pointer to an *already defined* thread local variable to use to prevent the having to read under
//   critical section every time (once a thread sees "tried" as true, it doesn't need to take the lock anymore).
//   Must be a variable declared made thread local using '#pragma omp threadprivate(local_name)
//     If the passed in pointer was from a bool* variable itself, it *also* must be declared threadprivate.
// once_struct_ptr: the pointer to an openmp_once_t to use as the backing state.
//     If it is threadprivate, then the block will happen at most once among all threads.
//     If it is not threadprivate, then the block will be run once for every thread, but
//       all of them will be guarded by the same critical section so only one thread can
//       can have the block run at once (useful for APIs that require at most one thread call them)
//     If you need it once for every thread and no critical block, then you aren't using OpenMP's
//       tooling at all, and thus this API isn't approriate for you.
// critical_section_name: the name of the OpenMP critical section. Do not surround with quotes.
#define OPENMP_ONCE_WITH_PTRS(local_ptr, once_struct_ptr, critical_section_name) \
    _OPENMP_ONCE_IMPL(*local_ptr, *once_struct_ptr, *((volatile openmp_once_t *)once_struct_ptr), critical_section_name)

#define OPENMP_ONCE_CLOSE }

// @formatter:on

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* OPENMP_ONCE_H_ */
