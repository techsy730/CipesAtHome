#include <stdio.h>
#include <stdlib.h>
#include <libconfig.h>
#include <time.h>
#if AGGRESSIVE_0_ALLOCATING || VERIFYING_SHIFTING_FUNCTIONS
// Pull in the "_s" bounds checking functions
#define __STDC_WANT_LIB_EXT1__ 1
#endif
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <omp.h>
#include "base.h"
#include "calculator.h"
#include "FTPManagement.h"
#include "recipes.h"
#include "start.h"
#include "shutdown.h"
#include "logger.h"
#include "rand_replace.h"

#include "absl/base/port.h"

// DON'T TOUCH: These reflect the logic of Paper Mario TTYD itself. Changing these will result in invalid plans.
#define CHOOSE_2ND_INGREDIENT_FRAMES 56 	// Penalty for choosing a 2nd item
#define TOSS_FRAMES 32				// Penalty for having to toss an item
#define ALPHA_SORT_FRAMES 38			// Penalty to perform alphabetical ascending sort
#define REVERSE_ALPHA_SORT_FRAMES 40		// Penalty to perform alphabetical descending sort
#define TYPE_SORT_FRAMES 39			// Penalty to perform type ascending sort
#define REVERSE_TYPE_SORT_FRAMES 41		// Penalty to perform type descending sort
#define JUMP_STORAGE_NO_TOSS_FRAMES 5		// Penalty for not tossing the last item (because we need to get Jump Storage)

// User configurable tunables
#define WEAK_PB_FLOOR 4500		// If PB is above this value, consider it a "weak" PB and don't increase iteration limit as much.
// Just so first runs (like from non-existant results dir) don't spend so long trying to "optimize" a "not all that great" branch.
#define WEAK_PB_FLOOR_DIVISIOR 15 	// Divide limit increase by this if a "weak" PB.
#define BUFFER_SEARCH_FRAMES 150		// Threshold to try optimizing a roadmap to attempt to beat the current record
#define BUFFER_SEARCH_FRAMES_KIND_OF_CLOSE 3 * BUFFER_SEARCH_FRAMES // Threshold for closeness to the current record to try spending more time on the branch
#define VERBOSE_ITERATION_LOG_RATE 100000    // How many iterations before logging iteration progress verbosely (level 6 logging)
#define DEFAULT_TERATION_LOG_BRANCH_INTERVAL_MAX_BEFORE_SLOWER_SCALING  50 // If the branch rate is above this, then additional values above this will be divided by DEFAULT_TERATION_LOG_BRANCH_RATE_DIVISOR for deciding when to log iteration progress incrementally.
#define DEFAULT_TERATION_LOG_BRANCH_INTERVAL_DIVISOR  4 // How much to divide the branch rate additional value above the DEFAULT_TERATION_LOG_BRANCH_RATE_MAX_BEFORE_SLOWER_SCALING for deciding when to log iteration progress incrementally.
#define DEFAULT_ITERATION_LIMIT 150000l // Cutoff for iterations explored before resetting
#define DEFAULT_ITERATION_LIMIT_SHORT 75000l // Cutoff for iterations explored before resetting when short branches is randomly selected
#define SHORT_ITERATION_LIMIT_CHANCE 40 // Chance (out of 100) for a thread to choose DEFAULT_ITERATION_LIMIT_SHORT instead of DEFAULT_ITERATION_LIMIT when starting a new branch
#define ITERATION_LIMIT_INCREASE 75000000l // Amount to increase the iteration limit by when finding a new PB
// Basically 2.5*ITERATION_LIMIT_INCREASE, but keeps floats out of it so we can static_assert on it
#define ITERATION_LIMIT_INCREASE_FIRST ((ITERATION_LIMIT_INCREASE << 1) + (ITERATION_LIMIT_INCREASE >> 1)) // Amount to increase the iteration limit by when finding a new PB for the first time in this branch
#define ITERATION_LIMIT_MAX (12*ITERATION_LIMIT_INCREASE) // Maxumum iteration limit before increases shrink drastically (a soft maximum)
#define ITERATION_LIMIT_INCREASE_PAST_MAX (ITERATION_LIMIT_INCREASE/500) // Amount to increase the iteration limit by when finding a new record when past the max
#define ITERATION_LIMIT_INCREASE_GETTING_CLOSE ITERATION_LIMIT_INCREASE_FIRST / 4
#define ITERATION_LIMIT_INCREASE_GETTING_KINDOF_CLOSE ITERATION_LIMIT_INCREASE_GETTING_CLOSE / 2
#define SELECT_CHANCE_TO_SKIP_SEEMINGLY_GOOD_MOVE 25 // Chance (out of 100) for the select strategy to skip a seemingly good next move
#define DEFAULT_CAPACITY_FOR_EMPTY 8 // When initializing a dynamically sized array, an empty/NULL array will be initialized to an Array of this size on a new element add
#define CAPACITY_INCREASE_FACTOR 1.5 // When a dynamically sized array is full, increase capacity by this factor
#define CAPACITY_DECREASE_THRESHOLD 0.25 // When a dynamically sized array element count is below this fraction of the capacity, shrink it
#define CAPACITY_DECREASE_FACTOR 0.35 // When a dynamically sized array is shrunk, shrink it below this factor
#define CAPACITY_DECREASE_FLOOR (2*DEFAULT_CAPACITY_FOR_EMPTY) // Never shrink a dynamically sized array below this capacity
#define CHECK_SHUTDOWN_INTERVAL 30000

#define NEW_BRANCH_LOG_LEVEL 3

#define INDEX_ITEM_UNDEFINED -1

// Sanity tests for constants.
_CIPES_STATIC_ASSERT(VERBOSE_ITERATION_LOG_RATE > 0, "Log rates must be > 0");
_CIPES_STATIC_ASSERT(BUFFER_SEARCH_FRAMES < BUFFER_SEARCH_FRAMES_KIND_OF_CLOSE, "The 'close to PB' threshold must be <= 'kind of close to PB' threshold");
_CIPES_STATIC_ASSERT(DEFAULT_ITERATION_LIMIT_SHORT <= DEFAULT_ITERATION_LIMIT, "Short iteration limit must be <= then default iteration limit");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE <= ITERATION_LIMIT_MAX, "Default iteration limit must be <= then iteration limit maximum");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE <= ITERATION_LIMIT_MAX, "Iteration limit increase must be <= then iteration limit maximum");
_CIPES_STATIC_ASSERT(SHORT_ITERATION_LIMIT_CHANCE < 100, "Chance to use short iteration limit must be < 100");
_CIPES_STATIC_ASSERT(SHORT_ITERATION_LIMIT_CHANCE >= 0, "Chance to use short iteration limit must be >= 0");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE_FIRST <= ITERATION_LIMIT_MAX, "Iteration limit increase must be <= then iteration limit maximum");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE_PAST_MAX <= ITERATION_LIMIT_MAX, "Iteration limit increase must be <= then iteration limit maximum");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE_PAST_MAX <= ITERATION_LIMIT_INCREASE, "The small Iteration limit (for when past ITERATION_LIMIT_MAX) must be <= the normal iteration limit increase");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE_GETTING_CLOSE <= ITERATION_LIMIT_INCREASE_FIRST, "The 'getting close to PB' iteration limit increase must be less then the first PB increase.");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE_GETTING_CLOSE <= ITERATION_LIMIT_MAX, "The 'getting close to PB' iteration limit increase must be <= then iteration limit maximum");
_CIPES_STATIC_ASSERT(ITERATION_LIMIT_INCREASE_GETTING_KINDOF_CLOSE <= ITERATION_LIMIT_INCREASE_GETTING_CLOSE,
		"The 'getting kind of close to PB' iteration limit increase must be <= The 'getting close to PB' iteration limit increase");

_CIPES_STATIC_ASSERT(SELECT_CHANCE_TO_SKIP_SEEMINGLY_GOOD_MOVE < 100, "Chance to skip greedy, seemingly best move must be < 100");
_CIPES_STATIC_ASSERT(SELECT_CHANCE_TO_SKIP_SEEMINGLY_GOOD_MOVE >= 0, "Chance to skip greedy, seemingly best move must be >= 0");
_CIPES_STATIC_ASSERT(CHECK_SHUTDOWN_INTERVAL > 0, "Check for shutdown interval must be > 0");
_CIPES_STATIC_ASSERT(CHECK_SHUTDOWN_INTERVAL < DEFAULT_ITERATION_LIMIT, "Check for shutdown interval must be < the default iteration limit");

// Only GCC can assure that floating point constant expressions can be evaluated at compile time.
#if defined(__GNUC__) && !defined(__clang__)
_CIPES_STATIC_ASSERT(CAPACITY_DECREASE_THRESHOLD <= CAPACITY_DECREASE_FACTOR, "The decrease threshold must be <= the decrease factor");
_CIPES_STATIC_ASSERT(CAPACITY_INCREASE_FACTOR >= 1, "The increase factor must be >= 1");
#endif
_CIPES_STATIC_ASSERT(DEFAULT_CAPACITY_FOR_EMPTY > 0, "The default capacity must be > 0");
_CIPES_STATIC_ASSERT(CAPACITY_DECREASE_FLOOR > 0, "The floor for capacity must be > 0");

#define NOISY_DEBUG_FLAG 0
// Only uncomment the below if you are really using NOISY_DEBUG_FLAG
//#if NOISY_DEBUG_FLAG
//#define NOISY_DEBUG(...) printf(__VA_ARGS__)
//#else
#define NOISY_DEBUG(...) _REQUIRE_SEMICOLON
//#endif

#if VERIFYING_SHIFTING_FUNCTIONS
#define _assert_for_shifting_function(condition) _assert_with_stacktrace(condition)
// #elif defined(FAST_BUT_NO_VERIFY)
#else
#define _assert_for_shifting_function(condition) ABSL_INTERNAL_ASSUME_NO_ASSERT(condition)
// #define _assert_for_shifting_function(condition)
#endif

static const int INT_OUTPUT_ARRAY_SIZE_BYTES = sizeof(outputCreatedArray_t);
static const outputCreatedArray_t EMPTY_RECIPES = {0};

typedef enum Alpha_Sort Alpha_Sort;
typedef enum Type_Sort Type_Sort;
typedef struct MoveDescription MoveDescription;

int **invFrames;
struct Recipe *recipeList;

// Harmless race; if multiple threads try to initialize this they will
// all initialize to the same thing.
static const struct Cook EMPTY_COOK = {0};

ABSL_ATTRIBUTE_UNUSED ABSL_ATTRIBUTE_ALWAYS_INLINE
static inline bool checkShutdownOnIndex(int i) {
	return i % CHECK_SHUTDOWN_INTERVAL == 0 && askedToShutdown();
}

ABSL_ATTRIBUTE_UNUSED ABSL_ATTRIBUTE_ALWAYS_INLINE
static inline bool prefetchShutdownOnIndex(int i) {
#if ENABLE_PREFETCHING
	if (i % CHECK_SHUTDOWN_INTERVAL == (CHECK_SHUTDOWN_INTERVAL - 1)) {
		prefetchShutdown();
		return true;
	}
#endif
	return false;
}

ABSL_ATTRIBUTE_UNUSED ABSL_ATTRIBUTE_ALWAYS_INLINE
static inline bool checkShutdownOnIndexWithPrefetch(int i) {
#if ENABLE_PREFETCHING
	int modulo = i % CHECK_SHUTDOWN_INTERVAL;
	switch (modulo) {
		case 0:
			return askedToShutdown();
		case (CHECK_SHUTDOWN_INTERVAL - 1):
			prefetchShutdown();
			return false;
		default:
			return false;
	}
#else
	return checkShutdownOnIndex(i);
#endif
}

ABSL_ATTRIBUTE_UNUSED ABSL_ATTRIBUTE_ALWAYS_INLINE
static inline bool checkShutdownOnIndexLong(long i) {
	return i % CHECK_SHUTDOWN_INTERVAL == 0 && askedToShutdown();
}

ABSL_ATTRIBUTE_UNUSED ABSL_ATTRIBUTE_ALWAYS_INLINE
static inline bool prefetchShutdownOnIndexLong(long i) {
#if ENABLE_PREFETCHING
	if (i % CHECK_SHUTDOWN_INTERVAL == (CHECK_SHUTDOWN_INTERVAL - 1)) {
		prefetchShutdown();
		return true;
	}
#endif
	return false;

}

ABSL_ATTRIBUTE_UNUSED ABSL_ATTRIBUTE_ALWAYS_INLINE
static inline bool checkShutdownOnIndexLongWithPrefetch(long i) {
#if ENABLE_PREFETCHING
	int modulo = i % CHECK_SHUTDOWN_INTERVAL;
	switch (modulo) {
		case 0:
			return askedToShutdown();
		case (CHECK_SHUTDOWN_INTERVAL - 1):
			prefetchShutdown();
			return false;
		default:
			return false;
	}
#else
	return checkShutdownOnIndexLong(i);
#endif
}

/*-------------------------------------------------------------------
 * Function 	: initializeInvFrames
 *
 * Initializes global variable invFrames, which is used to calculate
 * the number of frames it takes to navigate to an item in the menu.
 -------------------------------------------------------------------*/
void initializeInvFrames() {
	invFrames = getInventoryFrames();
}

/*-------------------------------------------------------------------
 * Function 	: initializeRecipeList
 *
 * Initializes global variable recipeList, which stores data pertaining
 * to each of the 57 recipes, plus a representation of Chapter 5. This
 * data consists of recipe outputs, number of different ways to cook the
 * recipe, and the items required for each recipe combination.
 -------------------------------------------------------------------*/
void initializeRecipeList() {
	recipeList = getRecipeList();
}

/*-------------------------------------------------------------------
 * Function 	: applyJumpStorageFramePenalty
 * Inputs	: struct BranchPath *node
 *
 * Looks at the node's Cook data. If the item is autoplaced, then add
 * a penalty for not tossing the item. Adjust framesTaken and
 * totalFramesTaken to reflect this change.
 -------------------------------------------------------------------*/
void applyJumpStorageFramePenalty(struct BranchPath *node) {
	if (((struct Cook *) node->description.data)->handleOutput == Autoplace) {
		node->description.framesTaken += JUMP_STORAGE_NO_TOSS_FRAMES;
		node->description.totalFramesTaken += JUMP_STORAGE_NO_TOSS_FRAMES;
	}

	return;
}

// This defines it, but the body is in the header for inlining.
ABSL_ATTRIBUTE_ALWAYS_INLINE void copyCook(struct Cook *cookNew, const struct Cook *cookOld);

/*-------------------------------------------------------------------
 * Function 	: copyOutputsFulfilled
 * Inputs	: struct BranchPath *node
 *		  int *oldOutputsFulfilled
 * Outputs	:
 * A simple memcpy to duplicate oldOutputsFulfilled to a new array
 -------------------------------------------------------------------*/
/*ABSL_MUST_USE_RESULT_INCLUSIVE static int *copyOutputsFulfilled(int *oldOutputsFulfilled) {
	int *newOutputsFulfilled = malloc(INT_OUTPUT_ARRAY_SIZE_BYTES);

	checkMallocFailed(newOutputsFulfilled);

	memcpy((void *)newOutputsFulfilled, (void *)oldOutputsFulfilled, INT_OUTPUT_ARRAY_SIZE_BYTES);
	return newOutputsFulfilled;
}*/

/*-------------------------------------------------------------------
 * Function 	: copyOutputsFulfilledNoAlloc
 * Inputs	: int *destOutputsFulfilled, int *srcOutputsFulfilled
 * Outputs	:
 * A simple memcpy to duplicate srcOutputsFulfilled into destOutputsFulfilled
 * Basically copyOutputsFulfilled, but no allocation (both arrays must already be allocated).
 * NOTE: Destination is FIRST
 -------------------------------------------------------------------*/
ABSL_ATTRIBUTE_ALWAYS_INLINE static inline void copyOutputsFulfilledNoAlloc(outputCreatedArray_t destOutputsFulfilled, const outputCreatedArray_t srcOutputsFulfilled) {
	memcpy((void *)destOutputsFulfilled, (const void*)srcOutputsFulfilled, INT_OUTPUT_ARRAY_SIZE_BYTES);
}

/*-------------------------------------------------------------------
 * Function 	: createChapter5Struct
 * Inputs	: int				DB_place_index
 *		  int				CO_place_index
 *		  int				KM_place_index
 *		  int				CS_place_index
 *		  int				TR_use_index
 *		  int				lateSort
 * Outputs	: struct CH5			*ch5
 *
 * Compartmentalization of setting struct CH5 attributes
 * lateSort tracks whether we performed the sort before or after the
 * Keel Mango, for printing purposes
 -------------------------------------------------------------------*/
ABSL_MUST_USE_RESULT_INCLUSIVE struct CH5 *createChapter5Struct(struct CH5_Eval eval, int lateSort) {
	struct CH5 *ch5 = malloc(sizeof(struct CH5));

	checkMallocFailed(ch5);

	ch5->indexDriedBouquet = eval.DB_place_index;
	ch5->indexCoconut = eval.CO_place_index;
	ch5->ch5Sort = eval.sort;
	ch5->indexKeelMango = eval.KM_place_index;
	ch5->indexCourageShell = eval.CS_place_index;
	ch5->indexThunderRage = eval.TR_use_index;
	ch5->lateSort = lateSort;
	return ch5;
}

/*-------------------------------------------------------------------
 * Function 	: createCookDescription
 * Inputs	: struct BranchPath 	  *node
 *		  struct Recipe 	  recipe
 *		  struct ItemCombination combo
 *		  enum Type_Sort	  *tempInventory
 *		  int 			  *tempFrames
 *		  int 			  viableItems
 * Outputs	: MoveDescription useDescription
 *
 * Compartmentalization of generating a MoveDescription struct
 * based on various parameters dependent on what recip we're cooking
 -------------------------------------------------------------------*/
MoveDescription createCookDescription(const struct BranchPath *node, struct Recipe recipe, struct ItemCombination combo, struct Inventory *tempInventory, int *tempFrames, int viableItems) {
	MoveDescription useDescription;
	useDescription.action = Cook;

	int ingredientLoc[2] = {INDEX_ITEM_UNDEFINED, INDEX_ITEM_UNDEFINED};

	// Determine the locations of both ingredients
	ingredientLoc[0] = indexOfItemInInventory(*tempInventory, combo.item1);

	if (combo.numItems == 1) {
		createCookDescription1Item(node, recipe, combo, tempInventory, ingredientLoc, tempFrames, viableItems, &useDescription);
	}
	else {
		ingredientLoc[1] = indexOfItemInInventory(*tempInventory, combo.item2);
		createCookDescription2Items(node, recipe, combo, tempInventory, ingredientLoc, tempFrames, viableItems, &useDescription);
	}

	return useDescription;
}

/*-------------------------------------------------------------------
 * Function 	: createCookDescription1Item
 * Inputs	: struct BranchPath 		*node
 *		  struct Recipe 		recipe
 *		  struct ItemCombination 	combo
 *		  enum Type_Sort		*tempInventory
 *		  int 				*ingredientLoc
 *		  int 				*ingredientOffset
 *		  int 				*tempFrames
 *		  int 				viableItems
 		  MoveDescription	*useDescription
 *
 * Handles inventory management and frame calculation for recipes of
 * length 1. Generates Cook structure and points to this structure
 * in useDescription.
 -------------------------------------------------------------------*/
void createCookDescription1Item(const struct BranchPath *node, struct Recipe recipe, struct ItemCombination combo, struct Inventory *tempInventory, int *ingredientLoc, int *tempFrames, int viableItems, MoveDescription *useDescription) {
	// This is a potentially viable recipe with 1 ingredient
	// Determine how many frames will be needed to select that item
	*tempFrames = invFrames[viableItems - 1][ingredientLoc[0] - tempInventory->nulls];

	// Modify the inventory if the ingredient was in the first 10 slots
	if (ingredientLoc[0] < 10) {
		// Shift the inventory towards the front of the array to fill the null
		*tempInventory = removeItem(*tempInventory, ingredientLoc[0]);
	}

	generateCook(useDescription, combo, recipe, ingredientLoc, 0);
	generateFramesTaken(useDescription, node, *tempFrames);
}

/*-------------------------------------------------------------------
 * Function 	: createCookDescription2Items
 * Inputs	: struct BranchPath 		*node
 *		  struct Recipe 		recipe
 *		  struct ItemCombination 	combo
 *		  enum Type_Sort		*tempInventory
 *		  int 				*ingredientLoc
 *		  int 				*ingredientOffset
 *		  int 				*tempFrames
 *		  int 				viableItems
 		  MoveDescription	*useDescription
 *
 * Handles inventory management and frame calculation for recipes of
 * length 2. Swaps items if it's faster to choose the second item first.
 * Generates Cook structure and points to this structure in useDescription.
 -------------------------------------------------------------------*/
void createCookDescription2Items(const struct BranchPath *node, struct Recipe recipe, struct ItemCombination combo, struct Inventory *tempInventory, int *ingredientLoc, int *tempFrames, int viableItems, MoveDescription *useDescription) {
	// This is a potentially viable recipe with 2 ingredients
	//Baseline frames based on how many times we need to access the menu
	*tempFrames = CHOOSE_2ND_INGREDIENT_FRAMES;

	int swap = 0;

	// Due to weird side-effects from Inventory Overload, choosing the item in index inventory.length - 1 will
	// always cause the item in index inventory.length - 1 - inventory.nulls to disappear. In this case, we need
	// to be super careful about trying to swap the ingredient order, as certain orders may be impossible.
	int lastVisibleSlot = tempInventory->length - 1;
	if (tempInventory->nulls) {
		// Determine if it's faster to select the second item first
		if (selectSecondItemFirst(ingredientLoc, tempInventory->nulls, viableItems)) {
			swapItems(ingredientLoc);
			swap = 1;
		}

		// This will cause the item in index length - 1 - nulls to disappear
		// Verify that the second ingredient is NOT in this index
		if (ingredientLoc[0] == lastVisibleSlot && ingredientLoc[1] == lastVisibleSlot - tempInventory->nulls) {
			// This item will disappear. We will need to swap the order of the items
			swapItems(ingredientLoc);
			swap = swap ? 0 : 1;
		}

		// Calculate the number of frames to grab the first item
		*tempFrames += invFrames[viableItems - 1][ingredientLoc[0] - tempInventory->nulls];

		// Based on the index of the first item, calculate the frames to grab the second item

		// If the first ingredient is in slots 1-10, the item is removed.
		// We only care about this in order to adjust the index of the second item,
		// which decreases by 1 in this scenario.
		if (ingredientLoc[0] < 10 && ingredientLoc[1] > ingredientLoc[0]) {
			*tempFrames += invFrames[viableItems - 2][ingredientLoc[1] - tempInventory->nulls - 1];
		}
		// The anomaly occurs
		else if (ingredientLoc[0] == lastVisibleSlot) {
			// We do not need to adjust the index of the item, as the index is before the removed item
			if (ingredientLoc[1] < lastVisibleSlot - tempInventory->nulls) {
				*tempFrames += invFrames[viableItems - 2][ingredientLoc[1] - tempInventory->nulls];
			}
			// Adjust the index because this index occurs after the index of the removed item
			else {
				*tempFrames += invFrames[viableItems - 2][ingredientLoc[1] - tempInventory->nulls - 1];
			}
		}
		else {
			// The first item will not disappear, OR it will not affect the index of the second item
			if (ingredientLoc[0] >= 10) {
				*tempFrames += invFrames[viableItems - 1][ingredientLoc[1] - tempInventory->nulls];
			}
			else {
				*tempFrames += invFrames[viableItems - 2][ingredientLoc[1] - tempInventory->nulls];
			}
		}
	}
	else {
		// Determine which order of ingredients to take
		// The first picked item always vanishes from the list of ingredients when picking the 2nd ingredient
		// There are some configurations where it is 2 frames faster to pick the ingredients in the reverse order
		if (selectSecondItemFirst(ingredientLoc, tempInventory->nulls, viableItems)) {
			// It's faster to select the 2nd item, so make it the priority and switch the order
			swapItems(ingredientLoc);
			swap = 1;
		}

		// Calculate the number of frames needed to grab the first item
		*tempFrames += invFrames[viableItems - 1][ingredientLoc[0] - tempInventory->nulls];

		// Determine the frames needed for the 2nd ingredient
		// First ingredient is always removed from the menu, so there is always 1 less viable item
		if (ingredientLoc[1] > ingredientLoc[0]) {
			// In this case, the 2nd ingredient has "moved up" one slot since the 1st ingredient vanishes
			*tempFrames += invFrames[viableItems - 2][ingredientLoc[1] - tempInventory->nulls - 1];
		}
		else {
			// In this case, the 2nd ingredient was found earlier on than the 1st ingredient, so no change to index
			*tempFrames += invFrames[viableItems - 2][ingredientLoc[1] - tempInventory->nulls];
		}
	}

	// Set each inventory index to null if the item was in the first 10 slots
	// To reduce complexity, remove the items in ascending order of index
	if (ingredientLoc[0] < ingredientLoc[1]) {
		if (ingredientLoc[0] < 10) {
			*tempInventory = removeItem(*tempInventory, ingredientLoc[0]);
		}
		if (ingredientLoc[1] < 10) {
			*tempInventory = removeItem(*tempInventory, ingredientLoc[1]);
		}
	}
	else {
		if (ingredientLoc[1] < 10) {
			*tempInventory = removeItem(*tempInventory, ingredientLoc[1]);
		}
		if (ingredientLoc[0] < 10) {
			*tempInventory = removeItem(*tempInventory, ingredientLoc[0]);
		}
	}

	// Describe what items were used
	generateCook(useDescription, combo, recipe, ingredientLoc, swap);
	generateFramesTaken(useDescription, node, *tempFrames);
}


/*-------------------------------------------------------------------
 * Function 	: createMoveQuick
 *
 * Outputs	: struct BranchPath *newMoveNode
 *
 * Allocates a BranchPath struct on the heap, with the assurance that
 * the callee will either initialize the strcut's value to sane values,
 * or doesn't care in it's usage case.
 */
static struct BranchPath *createMoveQuick() {
	struct BranchPath *node = malloc(sizeof(struct BranchPath));
	checkMallocFailed(node);
	return node;
}


/*-------------------------------------------------------------------
 * Function 	: createLegalMove
 * Inputs	: struct BranchPath		*node
 *		  enum Type_Sort		*inventory
 *		  MoveDescription	description
 *		  int				*outputsFulfilled
 *		  int				numOutputsFulfilled
 * Outputs	: struct BranchPath		*newLegalMove
 *
 * Given the input parameters, allocate and set attributes for a legalMove node
 * Note: Although node is never modified by this function, it will be the
 * new {return}->prev node of the returned BranchPath, thus it is not const
 -------------------------------------------------------------------*/
struct BranchPath *createLegalMove(struct BranchPath *mutableNode, struct Inventory inventory, MoveDescription description, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled) {
  // Prefer to work with the const version when possible to ensure we really don't modify it.
  const struct BranchPath *node = mutableNode;
	struct BranchPath *newLegalMove = createMoveQuick();

	checkMallocFailed(newLegalMove);

	newLegalMove->moves = node->moves + 1;
	newLegalMove->inventory = inventory;
	newLegalMove->description = description;
	newLegalMove->prev = mutableNode;
	newLegalMove->next = NULL;
	copyOutputsFulfilledNoAlloc(newLegalMove->outputCreated, outputsFulfilled);
	newLegalMove->numOutputsCreated = numOutputsFulfilled;
	newLegalMove->legalMoves = NULL;
	newLegalMove->numLegalMoves = 0;
	newLegalMove->capacityLegalMoves = 0;
	if (description.action >= Sort_Alpha_Asc && description.action <= Sort_Type_Des) {
		newLegalMove->totalSorts = node->totalSorts + 1;
	}
	else {
		newLegalMove->totalSorts = node->totalSorts;
	}

	return newLegalMove;
}

/*-------------------------------------------------------------------
 * Function 	: createMoveRaw
 *
 * Outputs	: struct BranchPath *newMoveNode
 *
 * Allocates a BranchPath struct on the heap, setting some critical values
 * to sane initial values.
 */
static struct BranchPath *createMoveZeroed() {
	struct BranchPath *node = calloc(sizeof(struct BranchPath), 1);
	checkMallocFailed(node);
	return node;
}

/*-------------------------------------------------------------------
 * Function 	: filterOut2Ingredients
 * Inputs	: struct BranchPath		*node
 *
 * For the first node's legal moves, we cannot cook a recipe which
 * contains two items. Thus, we need to remove any legal moves
 * which require two ingredients
 -------------------------------------------------------------------*/
void filterOut2Ingredients(struct BranchPath *node) {
	for (int i = 0; i < node->numLegalMoves; i++) {
		if (node->legalMoves[i]->description.action == Cook) {
			struct Cook *cook = node->legalMoves[i]->description.data;
			if (cook->numItems == 2) {
				freeLegalMove(node, i);
				i--; // Update i so we don't skip over the newly moved legalMoves
			}
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: finalizeChapter5Eval
 * Inputs	: struct BranchPath		*node
 *		  enum Type_Sort		*inventory
 *		  enum Action			sort
 *		  struct CH5			*ch5Data
 *		  int 				temp_frame_sum
 *		  int				*outputsFulfilled
 *		  int				numOutputsFulfilled
 *
 * Given input parameters, construct a new legal move to represent CH5
 -------------------------------------------------------------------*/
void finalizeChapter5Eval(struct BranchPath *node, struct Inventory inventory, struct CH5 *ch5Data, int temp_frame_sum, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled) {
	// Get the index of where to insert this legal move to
	int insertIndex = getInsertionIndex(node, temp_frame_sum);

	MoveDescription description;
	description.action = Ch5;
	description.data = ch5Data;
	description.framesTaken = temp_frame_sum;
	description.totalFramesTaken = node->description.totalFramesTaken + temp_frame_sum;

	// Create the legalMove node
	struct BranchPath *legalMove = createLegalMove(node, inventory, description, outputsFulfilled, numOutputsFulfilled);

	// Apend the legal move
	insertIntoLegalMoves(insertIndex, legalMove, node);
}

/*-------------------------------------------------------------------
 * Function 	: finalizeLegalMove
 * Inputs	: struct BranchPath		*node
 *		  int				tempFrames
 *		  MoveDescription	useDescription
 *		  enum Type_Sort		*tempInventory
 *		  int				*tempOutputsFulfilled
 *		  int				numOutputsFulfilled
 *		  enum HandleOutput		tossType
 *		  enum Type_Sort		toss
 *		  int				tossIndex
 *
 * Given input parameters, construct a new legal move to represent
 * a valid recipe move. Also checks to see if the legal move exceeds
 * the frame limit
 -------------------------------------------------------------------*/
void finalizeLegalMove(struct BranchPath *node, int tempFrames, MoveDescription useDescription, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, enum HandleOutput tossType, enum Type_Sort toss, int tossIndex) {
	// Determine if the legal move exceeds the frame limit. If so, return out
	if (useDescription.totalFramesTaken > getLocalRecord() + BUFFER_SEARCH_FRAMES) {
		return;
	}

	// Determine where to insert this legal move into the list of legal moves (sorted by frames taken)
	int insertIndex = getInsertionIndex(node, tempFrames);

	struct Cook *cookNew = malloc(sizeof(struct Cook));

	checkMallocFailed(cookNew);

	copyCook(cookNew, (struct Cook *)useDescription.data);
	cookNew->handleOutput = tossType;
	cookNew->toss = toss;
	cookNew->indexToss = tossIndex;
	useDescription.data = cookNew;

	// Create the legalMove node
	struct BranchPath *newLegalMove = createLegalMove(node, tempInventory, useDescription, tempOutputsFulfilled, numOutputsFulfilled);

	// Insert this new move into the current node's legalMove array
	insertIntoLegalMoves(insertIndex, newLegalMove, node);
}

/*-------------------------------------------------------------------
 * Function 	: freeAllNodes
 * Inputs	: struct BranchPath	*node
 *
 * We've reached the iteration limit, so free all nodes in the roadmap
 * We additionally need to delete node from the previous node's list of
 * legalMoves to prevent a double-free
 -------------------------------------------------------------------*/
void freeAllNodes(struct BranchPath *node) {
	struct BranchPath *prevNode = NULL;

	do {
		prevNode = node->prev;
		freeNode(node);

		// Delete node in nextNode's list of legal moves to prevent a double free
		if (prevNode != NULL && prevNode->legalMoves != NULL) {
			if (prevNode->capacityLegalMoves > 0) {
				prevNode->legalMoves[0] = NULL;
			}
			prevNode->numLegalMoves--;
			shiftUpLegalMoves(prevNode, 1);
		}

		// Traverse to the previous node
		node = prevNode;
	} while (node != NULL);
}

/*-------------------------------------------------------------------
 * Function 	: freeLegalMoveOnly
 * Inputs	: struct BranchPath	*node
 *		  int			index
 *
 * Free the legal move at index in the node's array of legal moves,
 * but unlike freeLegalMove(), this does NOT shift the existing legal
 * moves to fill the gap. This is useful in cases where where the
 * caller can assure such consistency is not needed (For example,
 * freeing the last legal move or freeing all legal moves).
 -------------------------------------------------------------------*/
static void freeLegalMoveOnly(struct BranchPath *node, int index) {
	freeNode(node->legalMoves[index]);
	node->legalMoves[index] = NULL;
	node->numLegalMoves--;
	node->next = NULL;
	_assert_with_stacktrace(node->numLegalMoves >= 0);
}

/*-------------------------------------------------------------------
 * Function 	: freeLegalMove
 * Inputs	: struct BranchPath	*node
 *		  int			index
 *
 * Free the legal move at index in the node's array of legal moves
 -------------------------------------------------------------------*/

void freeLegalMove(struct BranchPath *node, int index) {
	_assert_with_stacktrace(index < node->capacityLegalMoves);
	_assert_with_stacktrace(index < node->numLegalMoves);
	_assert_for_shifting_function(node->numLegalMoves <= node->capacityLegalMoves);

	freeLegalMoveOnly(node, index);

	// Shift up the rest of the legal moves
	shiftUpLegalMoves(node, index + 1);
}

/*-------------------------------------------------------------------
 * Function 	: freeNode
 * Inputs	: struct BranchPath	*node
 *
 * Free the current node and all legal moves within the node
 -------------------------------------------------------------------*/
void freeNode(struct BranchPath *node) {
	if (node == NULL) {
		return;
	}
	if (node->description.data != NULL) {
		free(node->description.data);
	}
	if (node->legalMoves != NULL) {
		const int max = node->numLegalMoves;
		int i = 0;
		while (i < max) {
			// Don't need to worry about shifting up when we do this.
			// Or resetting slots to NULL.
			// We are blowing it all away anyways.
			freeLegalMoveOnly(node, i++);
		}
		free(node->legalMoves);
	}
	free(node);
}

/*-------------------------------------------------------------------
 * Function 	: fulfillChapter5
 * Inputs	: struct BranchPath	*curNode
 *
 * A preliminary step to determine Dried Bouquet and Coconut placement
 * before calling handleChapter5Eval
 -------------------------------------------------------------------*/
void fulfillChapter5(struct BranchPath *curNode) {
	// Create an outputs chart but with the Dried Bouquet collected
	// to ensure that the produced inventory can fulfill all remaining recipes
  outputCreatedArray_t tempOutputsFulfilled;
	copyOutputsFulfilledNoAlloc(tempOutputsFulfilled, curNode->outputCreated);
	tempOutputsFulfilled[getIndexOfRecipe(Dried_Bouquet)] = true;
	int numOutputsFulfilled = curNode->numOutputsCreated + 1;

	struct Inventory newInventory = curNode->inventory;

	int mousse_cake_index = indexOfItemInInventory(newInventory, Mousse_Cake);

	// Create the CH5 eval struct
	struct CH5_Eval eval;

	// Calculate frames it takes the navigate to the Mousse Cake and the Hot Dog for the trade
	eval.frames_HD = 2 * invFrames[newInventory.length - 2 * newInventory.nulls - 1][indexOfItemInInventory(newInventory, Hot_Dog) - newInventory.nulls];
	eval.frames_MC = invFrames[newInventory.length - 2 * newInventory.nulls - 1][mousse_cake_index - newInventory.nulls];

	// If the Mousse Cake is in the first 10 slots, change it to NULL
	if (mousse_cake_index < 10) {
		newInventory = removeItem(newInventory, mousse_cake_index);
	}

	// Handle allocation of the first 2 CH5 items (Dried Bouquet and Coconut)
	switch (newInventory.nulls) {
		case 0 :
			handleDBCOAllocation0Nulls(curNode, newInventory, tempOutputsFulfilled, numOutputsFulfilled, eval);
			break;
		case 1 :
			handleDBCOAllocation1Null(curNode, newInventory, tempOutputsFulfilled, numOutputsFulfilled, eval);
			break;
		default :
			handleDBCOAllocation2Nulls(curNode, newInventory, tempOutputsFulfilled, numOutputsFulfilled, eval);
	}

	// We know tempOutputsFulfilled does not escape this scope, so safe to be unallocated on return.
}

/*-------------------------------------------------------------------
 * Function 	: fulfillRecipes
 * Inputs	: struct BranchPath	*curNode
 * 		  int			recipeIndex
 *
 * Iterate through all possible combinations of cooking different
 * recipes and create legal moves for them
 -------------------------------------------------------------------*/
void fulfillRecipes(struct BranchPath *curNode) {
	// For debugging stacktraces
	/*if (omp_get_thread_num() == 0) {
		_assert_with_stacktrace(false);
		// checkMallocFailed(NULL);
	}*/
	// Only evaluate the 57th recipe (Mistake) when it's the last recipe to fulfill
	// This is because it is relatively easy to craft this output with many of the previous outputs, and will take minimal frames
	int upperOutputLimit = (curNode->numOutputsCreated == NUM_RECIPES - 1) ? NUM_RECIPES : (NUM_RECIPES - 1);

	// Iterate through all recipe ingredient combos
	for (int recipeIndex = 0; recipeIndex < upperOutputLimit; recipeIndex++) {
		// Only want recipes that haven't been fulfilled
		if (curNode->outputCreated[recipeIndex]) {
			continue;
		}

		// Dried Bouquet (Recipe index 56) represents the Chapter 5 intermission
		// Don't actually use the specified recipe, as it is handled later
		if (recipeIndex == getIndexOfRecipe(Dried_Bouquet)) {
			continue;
		}

		// Only want ingredient combos that can be fulfilled right now!
		struct Recipe recipe = recipeList[recipeIndex];
		struct ItemCombination *combos = recipe.combos;
		for (int comboIndex = 0; comboIndex < recipe.countCombos; comboIndex++) {
			struct ItemCombination combo = combos[comboIndex];
			if (!itemComboInInventory(combo, curNode->inventory)) {
				continue;
			}

			// This is a recipe that can be fulfilled right now!

			// Copy the inventory
			struct Inventory newInventory = curNode->inventory;

			// Mark that this output has been fulfilled for viability determination
			outputCreatedArray_t tempOutputsFulfilled;
			copyOutputsFulfilledNoAlloc(tempOutputsFulfilled, curNode->outputCreated);
			tempOutputsFulfilled[recipeIndex] = true;
			int numOutputsFulfilled = curNode->numOutputsCreated + 1;

			// How many items there are to choose from (Not NULL or hidden)
			int viableItems = newInventory.length - 2 * newInventory.nulls;

			int tempFrames;

			struct MoveDescription useDescription = createCookDescription(curNode, recipe, combo, &newInventory, &tempFrames, viableItems);

			// Store the base useDescription's cook pointer to be freed later
			struct Cook *cookBase = (struct Cook *)useDescription.data;

			// Handle allocation of the output
			handleRecipeOutput(curNode, newInventory, tempFrames, useDescription, tempOutputsFulfilled, numOutputsFulfilled, recipe.output, viableItems);

			free(cookBase);
			// We know tempOutputsFulfilled does not escape this scope, so safe to be unallocated on return.
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: generateCook
 * Inputs	: MoveDescription	*description
 * 		  struct ItemCombination	combo
 *		  struct Recipe		recipe
 *		  int				*ingredientLoc
 *		  int				swap
 *
 * Given input parameters, generate Cook structure
 -------------------------------------------------------------------*/
void generateCook(MoveDescription *description, const struct ItemCombination combo, const struct Recipe recipe, const int *ingredientLoc, int swap) {
	struct Cook *cook = malloc(sizeof(struct Cook));

	checkMallocFailed(cook);

	description->action = Cook;
	cook->numItems = combo.numItems;
	if (swap) {
		cook->item1 = combo.item2;
		cook->item2 = combo.item1;
	}
	else {
		cook->item1 = combo.item1;
		cook->item2 = combo.item2;
	}

	cook->itemIndex1 = ingredientLoc[0];
	cook->itemIndex2 = ingredientLoc[1];
	cook->output = recipe.output;
	description->data = cook;
}

/*-------------------------------------------------------------------
 * Function 	: generateFramesTaken
 * Inputs	: MoveDescription	*description
 * 		  struct BranchPath		*node
 *		  int				framesTaken
 *
 * Assign frame duration to description structure and reference the
 * previous node to find the total frame duration for the roadmap thus far
 -------------------------------------------------------------------*/
void generateFramesTaken(MoveDescription *description, const struct BranchPath *node, int framesTaken) {
	description->framesTaken = framesTaken;
	description->totalFramesTaken = node->description.totalFramesTaken + framesTaken;
}

/*-------------------------------------------------------------------
 * Function 	: getInsertionIndex
 * Inputs	: struct BranchPath	*curNode
 *		  int			frames
 *
 * Based on the frames it takes to complete a new legal move, find out
 * where to insert it in the current node's array of legal moves, which
 * is ordered based on frame count ascending
 -------------------------------------------------------------------*/
int getInsertionIndex(const struct BranchPath *curNode, int frames) {
	if (curNode->legalMoves == NULL) {
		return 0;
	}
	int tempIndex = 0;
	while (tempIndex < curNode->numLegalMoves && frames > curNode->legalMoves[tempIndex]->description.framesTaken) {
		tempIndex++;
	}

	return tempIndex;
}

/*-------------------------------------------------------------------
 * Function : getSortFrames
 * Inputs	: enum Action action
 * Outputs	: int frames
 *
 * Depending on the type of sort, return the corresponding frame cost.
 -------------------------------------------------------------------*/
int getSortFrames(enum Action action) {
	switch (action) {
		case Sort_Alpha_Asc:
			return ALPHA_SORT_FRAMES;
		case Sort_Alpha_Des:
			return REVERSE_ALPHA_SORT_FRAMES;
		case Sort_Type_Asc:
			return TYPE_SORT_FRAMES;
		case Sort_Type_Des:
			return REVERSE_TYPE_SORT_FRAMES;
		default:
			// Critical error if we reach this point...
			// action should be some type of sort
			exit(-2);
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleChapter5EarlySortEndItems
 * Inputs	: struct BranchPath	*node
 *		  enum Type_Sort	*inventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			sort_frames
 *		  enum Action		sort
 *		  int			frames_DB
 *		  int			frames_CO
 *		  int			DB_place_index
 *		  int			CO_place_index
 *
 * Evaluate Chapter 5 such that a sort occurs between grabbing the
 * Coconut and the Keel Mango. Place the Keel Mango and Courage Shell
 * in various inventory locations. Determine if the move is legal.
 -------------------------------------------------------------------*/
void handleChapter5EarlySortEndItems(struct BranchPath *node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	for (eval.KM_place_index = 0; eval.KM_place_index < 10; eval.KM_place_index++) {
		// Don't allow current move to remove Thunder Rage or previously
		// obtained items
		if (inventory.inventory[eval.KM_place_index] == Thunder_Rage
			|| inventory.inventory[eval.KM_place_index] == Dried_Bouquet) {
			continue;
		}

		// Replace the chosen item with the Keel Mango
		struct Inventory km_temp_inventory = replaceItem(inventory, eval.KM_place_index, Keel_Mango);
		// Calculate the frames for this action
		eval.frames_KM = TOSS_FRAMES + invFrames[inventory.length][eval.KM_place_index + 1];

		for (eval.CS_place_index = 1; eval.CS_place_index < 10; eval.CS_place_index++) {
			// Don't allow current move to remove Thunder Rage or previously
			// obtained items
			if (eval.CS_place_index == eval.KM_place_index
				|| km_temp_inventory.inventory[eval.CS_place_index] == Thunder_Rage
				|| inventory.inventory[eval.KM_place_index] == Dried_Bouquet) {
				continue;
			}

			// Replace the chosen item with the Courage Shell
			struct Inventory kmcs_temp_inventory = replaceItem(km_temp_inventory, eval.CS_place_index, Courage_Shell);
			// Calculate the frames for this action
			eval.frames_CS = TOSS_FRAMES + invFrames[kmcs_temp_inventory.length][eval.CS_place_index + 1];

			// The next event is using the Thunder Rage item before resuming the 2nd session of recipe fulfillment
			eval.TR_use_index = indexOfItemInInventory(kmcs_temp_inventory, Thunder_Rage);
			if (eval.TR_use_index < 10) {
				kmcs_temp_inventory = removeItem(kmcs_temp_inventory, eval.TR_use_index);
			}
			// Calculate the frames for this action
			eval.frames_TR = invFrames[kmcs_temp_inventory.length - 1][eval.TR_use_index];

			// Calculate the frames of all actions done
			int temp_frame_sum = eval.frames_DB + eval.frames_CO + eval.frames_KM + eval.frames_CS + eval.frames_TR + eval.frames_HD + eval.frames_MC + eval.sort_frames;

			// Determine if the remaining inventory is sufficient to fulfill all remaining recipes
			if (stateOK(kmcs_temp_inventory, outputsFulfilled, recipeList)) {
				struct CH5 *ch5Data = createChapter5Struct(eval, 0);
				finalizeChapter5Eval(node, kmcs_temp_inventory, ch5Data, temp_frame_sum, outputsFulfilled, numOutputsFulfilled);
			}
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleChapter5Eval
 * Inputs	: struct BranchPath	*node
 *		  enum Type_Sort	*inventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			frames_DB
 *		  int			frames_CO
 *		  int			DB_place_index
 *		  int			CO_place_index
 *
 * Main Chapter 5 evaluation function. After allocating Dried Bouquet
 * and Coconut in the caller function, try performing a sort before
 * grabbing the Keel Mango and evaluate legal moves. Afterwards, try
 * placing the Keel Mango by tossing various inventory items and
 * evaluate legal moves.
 -------------------------------------------------------------------*/
void handleChapter5Eval(struct BranchPath *node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	// Evaluate sorting before the Keel Mango
	// Use -1 to identify that we are not collecting the Keel Mango until after the sort
	eval.frames_KM = -1;
	eval.KM_place_index = -1;
	handleChapter5Sorts(node, inventory, outputsFulfilled, numOutputsFulfilled, eval);

	// Place the Keel Mango in a null spot if one is available.
	if (inventory.nulls >= 1) {
		// Making a copy of the temp inventory for what it looks like after the allocation of the KM
		struct Inventory km_temp_inventory = addItem(inventory, Keel_Mango);
		eval.frames_KM = 0;
		eval.KM_place_index = 0;

		// Perform all sorts
		handleChapter5Sorts(node, km_temp_inventory, outputsFulfilled, numOutputsFulfilled, eval);

	}
	else {
		// Place the Keel Mango starting after the other placed items.
		for (eval.KM_place_index = 2; eval.KM_place_index < 10; eval.KM_place_index++) {
			// Don't allow current move to remove Thunder Rage
			if (inventory.inventory[eval.KM_place_index] == Thunder_Rage) {
				continue;
			}

			// Making a copy of the temp inventory for what it looks like after the allocation of the KM
			struct Inventory km_temp_inventory = replaceItem(inventory, eval.KM_place_index, Keel_Mango);
			// Calculate the frames for this action
			eval.frames_KM = TOSS_FRAMES + invFrames[inventory.length][eval.KM_place_index + 1];

			// Perform all sorts
			handleChapter5Sorts(node, km_temp_inventory, outputsFulfilled, numOutputsFulfilled, eval);
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleChapter5LateSortEndItems
 * Inputs	: struct BranchPath	*node
 *		  enum Type_Sort	*inventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			sort_frames
 *		  enum Action		sort
 *		  int			frames_DB
 *		  int			frames_CO
 *		  int			frames_KM
 *		  int			DB_place_index
 *		  int			CO_place_index
 *		  int			KM_place_index
 *
 * Evaluate Chapter 5 such that a sort occurs after grabbing the
 * Keel Mango. Place the Courage Shell in various inventory locations.
 * Determine if a move is legal.
 -------------------------------------------------------------------*/
void handleChapter5LateSortEndItems(struct BranchPath *node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	// Place the Courage Shell
	for (eval.CS_place_index = 0; eval.CS_place_index < 10; eval.CS_place_index++) {
		// Don't allow current move to remove Thunder Rage
		if (inventory.inventory[eval.CS_place_index] == Thunder_Rage) {
			continue;
		}

		// Replace the chosen item with the Courage Shell
		struct Inventory cs_temp_inventory = replaceItem(inventory, eval.CS_place_index, Courage_Shell);
		// Calculate the frames for this action
		eval.frames_CS = TOSS_FRAMES + invFrames[cs_temp_inventory.length][eval.CS_place_index + 1];

		// The next event is using the Thunder Rage
		eval.TR_use_index = indexOfItemInInventory(cs_temp_inventory, Thunder_Rage);
		// Using the Thunder Rage in slots 1-10 will cause a NULL to appear in that slot
		if (eval.TR_use_index < 10) {
			cs_temp_inventory = removeItem(cs_temp_inventory, eval.TR_use_index);
		}
		// Calculate the frames for this action
		eval.frames_TR = invFrames[cs_temp_inventory.length - 1][eval.TR_use_index];

		// Calculate the frames of all actions done
		int temp_frame_sum = eval.frames_DB + eval.frames_CO + eval.frames_KM + eval.frames_CS + eval.frames_TR + eval.frames_HD + eval.frames_MC + eval.sort_frames;

		if (stateOK(cs_temp_inventory, outputsFulfilled, recipeList)) {
			struct CH5 *ch5Data = createChapter5Struct(eval, 1);
			finalizeChapter5Eval(node, cs_temp_inventory, ch5Data, temp_frame_sum, outputsFulfilled, numOutputsFulfilled);
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleChapter5Sorts
 * Inputs	: struct BranchPath	*node
 *		  enum Type_Sort	*inventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			sort_frames
 *		  enum Action		sort
 *		  int			frames_DB
 *		  int			frames_CO
 *		  int			frames_KM
 *		  int			DB_place_index
 *		  int			CO_place_index
 *		  int			KM_place_index
 *
 * Perform various sorts on the inventory during Chapter 5 evaluation.
 * Only continue if a sort places the Coconut in slots 11-20.
 * Then, call an EndItems function to finalize the CH5 evaluation.
 -------------------------------------------------------------------*/
void handleChapter5Sorts(struct BranchPath *node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	for (eval.sort = Sort_Alpha_Asc; eval.sort <= Sort_Type_Des; eval.sort++) {
		struct Inventory sorted_inventory = getSortedInventory(inventory, eval.sort);

		// Only bother with further evaluation if the sort placed the Coconut in the latter half of the inventory
		// because the Coconut is needed for duplication
		if (indexOfItemInInventory(sorted_inventory, Coconut) < 10) {
			continue;
		}

		// Handle all placements of the Keel Mango, Courage Shell, and usage of the Thunder Rage
		eval.sort_frames = getSortFrames(eval.sort);

		if (eval.frames_KM == -1) {
			handleChapter5EarlySortEndItems(node, sorted_inventory, outputsFulfilled, numOutputsFulfilled, eval);
			continue;
		}

		handleChapter5LateSortEndItems(node, sorted_inventory, outputsFulfilled, numOutputsFulfilled, eval);
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleDBCOAllocation0Nulls
 * Inputs	: struct BranchPath	*curNode
 *		  enum Type_Sort	*tempInventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			viableItems
 *
 * Preliminary function to allocate Dried Bouquet and Coconut before
 * evaluating the rest of Chapter 5. There are no nulls in the inventory.
 -------------------------------------------------------------------*/
void handleDBCOAllocation0Nulls(struct BranchPath *curNode, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	// No nulls to utilize for Chapter 5 intermission
	// Both the DB and CO can only replace items in the first 10 slots
	// The remaining items always slide down to fill the vacancy
	// The DB will eventually end up in slot #2 and
	// the CO will eventually end up in slot #1
	for (eval.DB_place_index = 0; eval.DB_place_index < 10; eval.DB_place_index++) {
		// Don't allow current move to remove Thunder Rage
		if (tempInventory.inventory[eval.DB_place_index] == Thunder_Rage) {
			continue;
		}

		// Replace the chosen item with the Dried Bouquet
		struct Inventory db_temp_inventory = replaceItem(tempInventory, eval.DB_place_index, Dried_Bouquet);
		// Calculate the frames for this action
		eval.frames_DB = TOSS_FRAMES + invFrames[tempInventory.length][eval.DB_place_index + 1];

		for (eval.CO_place_index = 1; eval.CO_place_index < 10; eval.CO_place_index++) {
			// Don't allow current move to remove needed items
			if (eval.CO_place_index == eval.DB_place_index
				|| db_temp_inventory.inventory[eval.CO_place_index] == Thunder_Rage) {
				continue;
			}

			// Replace the chosen item with the Coconut
			struct Inventory dbco_temp_inventory = replaceItem(db_temp_inventory, eval.CO_place_index, Coconut);

			// Calculate the frames of this action
			eval.frames_CO = TOSS_FRAMES + invFrames[tempInventory.length][eval.CO_place_index + 1];

			// Handle the allocation of the Coconut sort, Keel Mango, and Courage Shell
			handleChapter5Eval(curNode, dbco_temp_inventory, tempOutputsFulfilled, numOutputsFulfilled, eval);
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleDBCOAllocation1Null
 * Inputs	: struct BranchPath	*curNode
 *		  enum Type_Sort		*tempInventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			viableItems
 *
 * Preliminary function to allocate Dried Bouquet and Coconut before
 * evaluating the rest of Chapter 5. There is 1 null in the inventory.
 -------------------------------------------------------------------*/
void handleDBCOAllocation1Null(struct BranchPath *curNode, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	// The Dried Bouquet gets auto-placed in the 1st slot,
	// and everything else gets shifted down one to fill the first NULL
	tempInventory = addItem(tempInventory, Dried_Bouquet);
	eval.DB_place_index = 0;
	eval.frames_DB = 0;

	// Dried Bouquet will always be in the first slot
	for (eval.CO_place_index = 1; eval.CO_place_index < 10; eval.CO_place_index++) {
		// Don't waste time replacing the Thunder Rage with the Coconut
		if (tempInventory.inventory[eval.CO_place_index] == Thunder_Rage) {
			continue;
		}

		// Replace the item with the Coconut
		struct Inventory co_temp_inventory = replaceItem(tempInventory, eval.CO_place_index, Coconut);
		// Calculate the number of frames needed to pick this slot for replacement
		eval.frames_CO = TOSS_FRAMES + invFrames[tempInventory.length][eval.CO_place_index + 1];

		// Handle the allocation of the Coconut sort, Keel Mango, and Courage Shell
		handleChapter5Eval(curNode, co_temp_inventory, tempOutputsFulfilled, numOutputsFulfilled, eval);
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleDBCOAllocation2Nulls
 * Inputs	: struct BranchPath	*curNode
 *		  enum Type_Sort	*tempInventory
 *		  int			*outputsFulfilled
 *		  int			numOutputsFulfilled
 *		  int			viableItems
 *
 * Preliminary function to allocate Dried Bouquet and Coconut before
 * evaluating the rest of Chapter 5. There are >=2 nulls in the inventory.
 -------------------------------------------------------------------*/
void handleDBCOAllocation2Nulls(struct BranchPath *curNode, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval) {
	// The Dried Bouquet gets auto-placed due to having nulls
	tempInventory = addItem(tempInventory, Dried_Bouquet);
	eval.DB_place_index = 0;
	eval.frames_DB = 0;

	// The Coconut gets auto-placed due to having nulls
	tempInventory = addItem(tempInventory, Coconut);
	eval.CO_place_index = 0;
	eval.frames_CO = 0;

	// Handle the allocation of the Coconut, Sort, Keel Mango, and Courage Shell
	handleChapter5Eval(curNode, tempInventory, tempOutputsFulfilled, numOutputsFulfilled, eval);
}

/*-------------------------------------------------------------------
 * Function 	: handleRecipeOutput
 * Inputs	: struct BranchPath		*curNode
 *		  enum Type_Sort		*tempInventory
 *		  int				tempFrames
 *		  MoveDescription	useDescription
 *		  int				*tempOutputsFulfilled
 *		  int				numOutputsFulfilled
 *		  enum Type_Sortf		output
 *		  int				viableItems
 *
 * After detecting that a recipe can be satisfied, see how we can handle
 * the output (either tossing the output, auto-placing it if there is a
 * null slot, or tossing a different item in the inventory)
 -------------------------------------------------------------------*/
void handleRecipeOutput(struct BranchPath *curNode, struct Inventory tempInventory, int tempFrames, MoveDescription useDescription, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, enum Type_Sort output, int viableItems) {
	// Options vary by whether there are NULLs within the inventory
	if (tempInventory.nulls >= 1) {
		tempInventory = addItem(tempInventory, ((struct Cook*)useDescription.data)->output);

		// Check to see if this state is viable
		if(stateOK(tempInventory, tempOutputsFulfilled, recipeList)) {
			finalizeLegalMove(curNode, tempFrames, useDescription, tempInventory, tempOutputsFulfilled, numOutputsFulfilled, Autoplace, -1, -1);
		}
	}
	else {
		// There are no NULLs in the inventory. Something must be tossed
		// Total number of frames increased by forcing to toss something
		tempFrames += TOSS_FRAMES;
		useDescription.framesTaken += TOSS_FRAMES;
		useDescription.totalFramesTaken += TOSS_FRAMES;

		// Evaluate viability of tossing the output item itself
		if (stateOK(tempInventory, tempOutputsFulfilled, recipeList)) {
			finalizeLegalMove(curNode, tempFrames, useDescription, tempInventory, tempOutputsFulfilled, numOutputsFulfilled, Toss, output, -1);
		}

		// Evaluate the viability of tossing all current inventory items
		// Assumed that it is impossible to toss and replace any items in the last 10 positions
		tryTossInventoryItem(curNode, tempInventory, useDescription, tempOutputsFulfilled, numOutputsFulfilled, output, tempFrames, viableItems);
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleSelectAndRandom
 * Inputs	: struct BranchPath	*curNode
 *		  int 			select
 *		  int 			randomise
 *
 * Based on configuration parameters select and randomise within config.txt,
 * manage the array of legal moves based on the designated behavior of the parameters.
 -------------------------------------------------------------------*/
void handleSelectAndRandom(struct BranchPath *curNode, int select, int randomise) {
	/*if (select && curNode->moves < 55 && curNode->numLegalMoves > 0) {
		softMin(curNode);
	}*/

	// Old method of handling select
	// Somewhat random process of picking the quicker moves to recurse down
	// Arbitrarily skip over the fastest legal move with a given probability
	if (select && curNode->moves < 55 && curNode->numLegalMoves > 0) {
		int nextMoveIndex = 0;
		while (nextMoveIndex < curNode->numLegalMoves - 1 && randint(0, 99) < SELECT_CHANCE_TO_SKIP_SEEMINGLY_GOOD_MOVE) {
			if (checkShutdownOnIndexWithPrefetch(nextMoveIndex)) {
				break;
			}
			nextMoveIndex++;
		}

		if (askedToShutdown()) {
			return;
		}

		// Take the legal move at nextMoveIndex and move it to the front of the array
		struct BranchPath *nextMove = curNode->legalMoves[nextMoveIndex];
		curNode->legalMoves[nextMoveIndex] = NULL;
		shiftDownLegalMoves(curNode, 0, nextMoveIndex);
		_assert_with_stacktrace(curNode != nextMove);
		curNode->legalMoves[0] = nextMove;
	}

	// When not doing the select methodology, and opting for randomize
	// just shuffle the entire list of legal moves and pick the new first item
	else if (randomise) {
		if (askedToShutdown()) {
			return;
		}
		shuffleLegalMoves(curNode);
	}
}

/*-------------------------------------------------------------------
 * Function 	: handleSorts
 * Inputs	: struct BranchPath	*curNode
 *
 * Perform the 4 different sorts, determine if they changed the inventory,
 * and if so, generate a legal move to represent the sort.
 -------------------------------------------------------------------*/
void handleSorts(struct BranchPath *curNode) {
	// Limit the number of sorts allowed in a roadmap
	if (curNode->totalSorts < 10) {
		// Perform the 4 different sorts
		for (enum Action sort = Sort_Alpha_Asc; sort <= Sort_Type_Des; sort++) {
			struct Inventory sorted_inventory = getSortedInventory(curNode->inventory, sort);

			// Only add the legal move if the sort actually changes the inventory
			if (compareInventories(sorted_inventory, curNode->inventory) == 0) {
				MoveDescription description;
				description.action = sort;
				description.data = NULL;
				int sortFrames = getSortFrames(sort);
				generateFramesTaken(&description, curNode, sortFrames);
				description.framesTaken = sortFrames;

				// Create the legalMove node
				struct BranchPath *newLegalMove = createLegalMove(curNode, sorted_inventory, description, curNode->outputCreated, curNode->numOutputsCreated);

				// Insert this new move into the current node's legalMove array
				insertIntoLegalMoves(curNode->numLegalMoves, newLegalMove, curNode);
			}
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: initializeRoot
 * Inputs	: struct Job		job
 * Outputs	: struct BranchPath	*root
 *
 * Generate the root of the tree graph
 -------------------------------------------------------------------*/
struct BranchPath *initializeRoot() {
	struct BranchPath *root = createMoveQuick();

	checkMallocFailed(root);

	root->moves = 0;
	root->inventory = getStartingInventory();
	root->description.action = Begin;
	root->description.data = NULL;
	root->description.framesTaken = 0;
	root->description.totalFramesTaken = 0;
	root->prev = NULL;
	root->next = NULL;
	// This will also 0 out all the other elements
	copyOutputsFulfilledNoAlloc(root->outputCreated, EMPTY_RECIPES);
	root->numOutputsCreated = 0;
	root->legalMoves = NULL;
	root->numLegalMoves = 0;
	root->capacityLegalMoves = 0;
	root->totalSorts = 0;
	return root;
}

struct CapacityComputationResult {
	ssize_t newCapacity;
	ssize_t capacityIncreasedBy;
	bool needsRealloc;
	bool needsShrink;
};

static struct CapacityComputationResult capacityCompute(const ssize_t currentCapacity, int minNewSize) {
	ssize_t newCapacity = currentCapacity;
	bool needsRealloc = false;
	bool needsShrink = false;
	// Always allocate at least one more then what the user asked for.
	++minNewSize;
	ssize_t shrinkThreshold = (ssize_t)(CAPACITY_DECREASE_THRESHOLD * newCapacity);
	if (currentCapacity == 0) {
		newCapacity = DEFAULT_CAPACITY_FOR_EMPTY;
		needsRealloc = true;
	} else if (minNewSize > currentCapacity) {
		// Thus + 1 is to assure increase even if the factor rounds to a 0 increase
		newCapacity = (ssize_t)(CAPACITY_INCREASE_FACTOR * (newCapacity + 1));
		if (newCapacity < DEFAULT_CAPACITY_FOR_EMPTY) {
			newCapacity = DEFAULT_CAPACITY_FOR_EMPTY;
		}
		needsRealloc = true;
	} else if (currentCapacity > CAPACITY_DECREASE_FLOOR && minNewSize <= shrinkThreshold) {
		ssize_t shrunkCapacity = (ssize_t)(CAPACITY_DECREASE_FACTOR * currentCapacity);
		if (shrunkCapacity < CAPACITY_DECREASE_FLOOR) {
			shrunkCapacity = CAPACITY_DECREASE_FLOOR;
		}
		if (shrunkCapacity < minNewSize) {
			// Shrunk too much, just fit the array exactly (always leave at least one slot after)
			shrunkCapacity = minNewSize;
		}
		if (shrunkCapacity < currentCapacity) {
			needsRealloc = true;
			needsShrink = true;
			newCapacity = shrunkCapacity;
		}
	}
	if (needsRealloc) {
		// Safeguard, make sure we are at least increasing it by a little.
		_assert_with_stacktrace(newCapacity >= minNewSize);
		// The above logic should _never_ shrink when we were expecting increase.
		_assert_with_stacktrace(needsShrink || newCapacity > currentCapacity);
	}
	ssize_t capacityIncreasedBy = newCapacity - currentCapacity;
	return (struct CapacityComputationResult){newCapacity, capacityIncreasedBy, needsRealloc, needsShrink};
}

/*-------------------------------------------------------------------
 * Function 	: insertIntoLegalMoves
 * Inputs	: int			insertIndex
 *		  struct BranchPath	*newLegalMove
 *		  struct BranchPath	*curNode
 *
 * Determine where in curNode's legalmove array the new legal move should
 * be inserted. This is necessary because our current implementation
 * arranges legal moves in ascending order based on the number of frames
 * it takes to complete the legal move.
 * Note: Even though newLegalMove is never modified in this function,
 * it maybe be set as a pointer in the curNode->legalMoves array, and thus it is not const
 -------------------------------------------------------------------*/
void insertIntoLegalMoves(int insertIndex, struct BranchPath *mutableNewLegalMove, struct BranchPath *curNode) {
  // Prefer to work with the const version when possible to ensure we really don't modify it.
  const struct BranchPath *newLegalMove = mutableNewLegalMove;
	struct CapacityComputationResult capacityChanges = capacityCompute(
		curNode->capacityLegalMoves, curNode->numLegalMoves + 1);
	if (capacityChanges.needsRealloc) {
		// Failsafes. Ensure we are at least reaching the target of new size
		// Reallocate the legalMove array to make room for a new legal move
		struct BranchPath **temp = realloc(curNode->legalMoves, sizeof(curNode->legalMoves[0]) * (capacityChanges.newCapacity));
		checkMallocFailed(temp);
#if AGGRESSIVE_0_ALLOCATING
		// Zero out the new parts of the array so viewing array contents doesn't cause dereferencing of invalid pointers,
		// which may mess up debuggers.
#if defined(__STDC_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__
		_assert_with_stacktrace(0 == memset_s(
				temp + curNode->capacityLegalMoves,
				capacityChanges.newCapacity - curNode->capacityLegalMoves,
				0,
				sizeof(curNode->legalMoves[0]) * capacityChanges.capacityIncreasedBy));
#else
		memset(
				temp + curNode->capacityLegalMoves,
				0,
				sizeof(curNode->legalMoves[0]) * capacityChanges.capacityIncreasedBy);
#endif
#endif

		curNode->legalMoves = temp;
		curNode->capacityLegalMoves = capacityChanges.newCapacity;
	}

	// Shift all legal moves further down the array to make room for a new legalMove
	shiftDownLegalMoves(curNode, insertIndex, curNode->numLegalMoves);
	/*for (int i = curNode->numLegalMoves - 1; i >= insertIndex; i--) {
		curNode->legalMoves[i+1] = curNode->legalMoves[i];
	}*/

	// Place newLegalMove in index insertIndex
	_assert_with_stacktrace(curNode != newLegalMove);
	curNode->legalMoves[insertIndex] = mutableNewLegalMove;

	// Increase numLegalMoves
	curNode->numLegalMoves++;

	return;
}

/*-------------------------------------------------------------------
 * Function 	: copyAllNodes
 * Inputs	: struct BranchPath	*newNode
 *		  struct BranchPath	*oldNode
 * Outputs	: struct BranchPath	*newNode
 *
 * Duplicate all the contents of a roadmap to a new memory region.
 * This is used for optimizeRoadmap.
 -------------------------------------------------------------------*/
struct BranchPath *copyAllNodes(struct BranchPath *newNode, const struct BranchPath *oldNode) {
	do {
		newNode->moves = oldNode->moves;
		newNode->inventory = oldNode->inventory;
		newNode->description = oldNode->description;
		switch (newNode->description.action) {
			case (Begin) :
				newNode->description.data = NULL;
				break;
			case (Sort_Alpha_Asc) :
				newNode->description.data = NULL;
				break;
			case (Sort_Alpha_Des) :
				newNode->description.data = NULL;
				break;
			case (Sort_Type_Asc) :
				newNode->description.data = NULL;
				break;
			case (Sort_Type_Des) :
				newNode->description.data = NULL;
				break;
			case (Cook) :
				newNode->description.data = malloc(sizeof(struct Cook));

				checkMallocFailed(newNode->description.data);

				copyCook((struct Cook *)newNode->description.data, (struct Cook *)oldNode->description.data);
				break;
			case (Ch5) :
				newNode->description.data = malloc(sizeof(struct CH5));

				checkMallocFailed(newNode->description.data);

				struct CH5 *newData = (struct CH5 *)newNode->description.data;
				struct CH5 *oldData = (struct CH5 *)oldNode->description.data;
				newData->indexDriedBouquet = oldData->indexDriedBouquet;
				newData->indexCoconut =  oldData->indexCoconut;
				newData->ch5Sort = oldData->ch5Sort;
				newData->indexKeelMango = oldData->indexKeelMango;
				newData->indexCourageShell = oldData->indexCourageShell;
				newData->indexThunderRage = oldData->indexThunderRage;
				newData->lateSort = oldData->lateSort;
				break;
			default :
				break;
		}

		copyOutputsFulfilledNoAlloc(newNode->outputCreated, oldNode->outputCreated);
		newNode->numOutputsCreated = oldNode->numOutputsCreated;
		newNode->legalMoves = NULL;
		newNode->numLegalMoves = 0;
		newNode->capacityLegalMoves = 0;
		if (newNode->numOutputsCreated < NUM_RECIPES) {
			newNode->next = createMoveZeroed();

			checkMallocFailed(newNode->next);

			newNode->next->prev = newNode;
			newNode = newNode->next;
		}
		else {
			newNode->next = NULL;
		}

		oldNode = oldNode->next;
	} while (oldNode != NULL);

	// Returns the deepest node in the tree after copying everything
	return newNode;
}

/*-------------------------------------------------------------------
 * Function 	: optimizeRoadmap
 * Inputs	: struct BranchPath		*root
 * Outputs	: struct OptimizeResult	result
 *
 * Given a complete roadmap, attempt to rearrange recipes such that they
 * are placed in more efficient locations in the roadmap. This is effective
 * in shaving off upwards of 100 frames off of a roadmap.
 -------------------------------------------------------------------*/
struct OptimizeResult optimizeRoadmap(const struct BranchPath *root) {
	// First copy all nodes to new memory locations so we can begin rearranging nodes
	struct BranchPath *newRoot = createMoveZeroed();

	checkMallocFailed(newRoot);

	newRoot->prev = NULL;

	// newNode is now the leaf of the tree (for easy list manipulation later)
	struct BranchPath *newNode = copyAllNodes(newRoot, root);

	// List of recipes that can be potentially rearranged into a better location within the roadmap
	enum Type_Sort rearranged_recipes[NUM_RECIPES];

	// Ignore the last recipe as the mistake can almost always be cooked last
	newNode = newNode->prev;

	// Determine which steps can be rearranged
	int num_rearranged_recipes = removeRecipesForReallocation(newNode, rearranged_recipes);

	// Now that all rearranged items have been removed,
	// find the optimal place they can be inserted again, such that they don't affect the inventory
	reallocateRecipes(newRoot, rearranged_recipes, num_rearranged_recipes);

	// All items have been rearranged and placed into a new roadmap
	// Recalculate the total frame count
	for (newNode = newRoot; newNode->next != NULL; newNode = newNode->next) {
		newNode->next->description.totalFramesTaken = newNode->next->description.framesTaken + newNode->description.totalFramesTaken;
	}

	struct OptimizeResult result;
	result.root = newRoot;
	result.last = newNode;
	return result;
}

/*-------------------------------------------------------------------
 * Function : periodicGithubCheck
 * Inputs	:
 *
 * Check for the most recent Github repository release version. If there
 * is a newer version, alert the user.
 -------------------------------------------------------------------*/
void periodicGithubCheck() {
	// Double check the latest release on Github
	int update = checkForUpdates(getLocalVersion());
	if (update == -1) {
		printf("Could not check version on Github. Please check your internet connection.\n");
		printf("Otherwise, completed roadmaps may be inaccurate!\n");
	}
	else if (update == 1) {
		printf("Please visit https://github.com/SevenChords/CipesAtHome/releases to download the newest version of this program!\n");
		printf("Press ENTER to exit the program.\n");
		ABSL_ATTRIBUTE_UNUSED char exitChar = getchar();
		exit(1);
	}
}

/*-------------------------------------------------------------------
 * Function 	: popAllButFirstLegalMove
 * Inputs	: struct BranchPath	*node
 *
 * In the event we are within the last few nodes of the roadmap, get rid
 * of all but the fastest legal move.
 -------------------------------------------------------------------*/
void popAllButFirstLegalMove(struct BranchPath *node) {
	// First case, we may need to set the final slot to NULL.
	int i = node->numLegalMoves - 1;
	freeLegalMove(node, i--);
	for (; i > 0 ; --i) {
		// Now we don't need to check final slot.
		freeLegalMoveOnly(node, i);
	}

	return;
}

/*-------------------------------------------------------------------
 * Function 	: printCh5Data
 * Inputs	: struct BranchPath		*curNode
 *		  MoveDescription 	desc
 *		  FILE				*fp
 *
 * Print to a txt file the data which pertains to Chapter 5 evaluation
 * (where to place Dried Bouquet, Coconut, etc.)
 -------------------------------------------------------------------*/
void printCh5Data(const struct BranchPath *curNode, MoveDescription desc, FILE *fp) {
	struct CH5 *ch5Data = desc.data;

	// Determine how many nulls there are when allocations start
	size_t nulls = curNode->prev->inventory.nulls;
	if (indexOfItemInInventory(curNode->prev->inventory, Mousse_Cake) < 10) {
		++nulls;
	}

	fprintf(fp, "Ch.5 Break: ");
	if (nulls) {
		fprintf(fp, "DB filling null, ");
		--nulls;
	}
	else {
		fprintf(fp, "DB replacing #%d, ", ch5Data->indexDriedBouquet + 1);
	}

	if (nulls) {
		fprintf(fp, "CO filling null, ");
		--nulls;
	}
	else {
		fprintf(fp, "CO replacing #%d, ", ch5Data->indexCoconut + 1);
	}
	if (ch5Data->lateSort) {
		if (nulls) {
			fprintf(fp, "KM filling null, ");
		}
		else {
			fprintf(fp, "KM replacing #%d, ", ch5Data->indexKeelMango + 1);
		}
		printCh5Sort(ch5Data, fp);
	}
	else {
		printCh5Sort(ch5Data, fp);
		fprintf(fp, "KM replacing #%d, ", ch5Data->indexKeelMango + 1);
	}
	fprintf(fp, "CS replacing #%d, use TR in #%d",
		ch5Data->indexCourageShell + 1, ch5Data->indexThunderRage + 1);
}

/*-------------------------------------------------------------------
 * Function 	: printCh5Sort
 * Inputs	: struct CH5	*ch5Data
 *		  FILE		*fp
 *
 * Print to a txt file the data which pertains to Chapter 5 sorting
 -------------------------------------------------------------------*/
void printCh5Sort(const struct CH5 *ch5Data, FILE *fp) {
	fprintf(fp, "sort ");
	switch (ch5Data->ch5Sort) {
		case Sort_Alpha_Asc :
			fprintf(fp, "(Alpha), ");
			break;
		case Sort_Alpha_Des :
			fprintf(fp, "(Reverse-Alpha), ");
			break;
		case Sort_Type_Asc :
			fprintf(fp, "(Type), ");
			break;
		case Sort_Type_Des :
			fprintf(fp, "(Reverse-Type), ");
			break;
		default :
			fprintf(fp, "ERROR IN CH5SORT SWITCH CASE");
	};
}

/*-------------------------------------------------------------------
 * Function 	: printCookData
 * Inputs	: struct BranchPath 		*curNode
 *		  MoveDescription 	desc
 *		  FILE				*fp
 *
 * Print to a txt file the data which pertains to cooking a recipe,
 * which includes what items were used and what happens to the output.
 -------------------------------------------------------------------*/
void printCookData(const struct BranchPath *curNode, const MoveDescription desc, FILE *fp) {
	struct Cook *cookData = desc.data;
	size_t nulls = curNode->prev->inventory.nulls;
	fprintf(fp, "Use [%s] in slot " _zuf " ", getItemName(cookData->item1),
		cookData->itemIndex1 - (cookData->itemIndex1 < 10 ? nulls : 0) + 1);

	if (cookData->numItems == 2) {
		fprintf(fp, "and [%s] in slot " _zuf " ", getItemName(cookData->item2),
			cookData->itemIndex2 - (cookData->itemIndex2 < 10 ? nulls : 0) + 1);
	}

	fputs("to make ", fp);

	if (cookData->handleOutput == Toss) {
		fputs("(and toss) ", fp);
	}
	else if (cookData->handleOutput == Autoplace) {
		fputs("(and auto-place) ", fp);
	}

	fprintf(fp, "<%s>", getItemName(cookData->output));

	if (cookData->handleOutput == TossOther) {
		fprintf(fp, ", toss [%s] in slot %d", getItemName(cookData->toss), cookData->indexToss + 1);
	}

	if (curNode->numOutputsCreated == NUM_RECIPES) {
		if (((struct Cook *) curNode->description.data)->handleOutput == Autoplace) {
			fputs(" (No-Toss 5 Frame Penalty for Jump Storage)", fp);
		}
		else {
			fputs(" (Jump Storage on Tossed Item)", fp);
		}
	}
}

/*-------------------------------------------------------------------
 * Function 	: printFileHeader
 * Inputs	: FILE				*fp
 *
 * Print to a txt file the header information for the file.
 -------------------------------------------------------------------*/
void printFileHeader(FILE *fp) {
	fputs("Description\tFrames Taken\tTotal Frames", fp);
	for (int i = 0; i < 20; i++) {
		fprintf(fp, "\tSlot #%d", i+1);
	}
	for (int i = 0; i < NUM_RECIPES; i++) {
		fprintf(fp, "\t%s", getItemName(recipeList[i].output));
	}
	fprintf(fp, "\n");
	recipeLog(5, "Calculator", "File", "Write", "Header for new output written");
}

/*-------------------------------------------------------------------
 * Function 	: printInventoryData
 * Inputs	: struct BranchPath	*curNode
 *		  FILE			*fp
 *
 * Print to a txt file the header information for the file.
 -------------------------------------------------------------------*/
void printInventoryData(const struct BranchPath *curNode, FILE *fp) {
	size_t nulls = curNode->inventory.nulls;
	size_t i;
	for (i = nulls; i < 10; ++i) {
		fprintf(fp, "\t%s", getItemName(curNode->inventory.inventory[i]));
	}
	for (i = 0; i < nulls; ++i) {
		fprintf(fp, "\tNULL");
	}
	for (i = 10; i < curNode->inventory.length - nulls; ++i) {
		fprintf(fp, "\t%s", getItemName(curNode->inventory.inventory[i]));
	}
	for (; i < curNode->inventory.length; ++i) {
		fprintf(fp, "\t(%s)", getItemName(curNode->inventory.inventory[i]));
	}
	for (; i < 20; ++i) {
		fprintf(fp, "\tBLOCKED");
	}
}

/*-------------------------------------------------------------------
 * Function 	: printOutputsCreated
 * Inputs	: struct BranchPath	*curNode
 *		  FILE			*fp
 *
 * Print to a txt file data pertaining to which recipes
 * have been cooked thus far.
 -------------------------------------------------------------------*/
void printOutputsCreated(const struct BranchPath *curNode, FILE *fp) {
	for (int i = 0; i < NUM_RECIPES; i++) {
		if (curNode->outputCreated[i]) {
			fprintf(fp, "\tTrue");
		}
		else {
			fprintf(fp, "\tFalse");
		}
	}
}

void printNodeDescription(const struct BranchPath * curNode, FILE * fp)
{
	MoveDescription desc = curNode->description;
	enum Action curNodeAction = desc.action;
	switch (curNodeAction) {
	case Cook:
		printCookData(curNode, desc, fp);
		break;
	case Ch5:
		printCh5Data(curNode, desc, fp);
		break;
	case Begin:
		fputs("Begin", fp);
		break;
	default:
		// Some type of sorting
		printSortData(fp, curNodeAction);
	}
}

/*-------------------------------------------------------------------
 * Function 	: printResults
 * Inputs	: char			*filename
 *		  struct BranchPath	*path
 *
 * Parent function for children print functions. This parent function
 * is called when a roadmap has been found which beats the current
 * local record.
 -------------------------------------------------------------------*/
void printResults(const char *filename, const struct BranchPath *path) {
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		printf("Could not locate %s... This is a bug.\n", filename);
		printf("Press ENTER to exit.\n");
		ABSL_ATTRIBUTE_UNUSED char exitChar = getchar();
		exit(1);
	}
	// Write header information
	printFileHeader(fp);

	// Print data information
	const struct BranchPath *curNode = path;
	do {
		printNodeDescription(curNode, fp);

		// Print out frames taken
		fprintf(fp, "\t%d", curNode->description.framesTaken);
		// Print out total frames taken
		fprintf(fp, "\t%d", curNode->description.totalFramesTaken);

		// Print out inventory
		printInventoryData(curNode, fp);

		// Print out whether or not all 58 items were created
		printOutputsCreated(curNode, fp);

		// Add newline character to put next node on new line
		fprintf(fp, "\n");
	} while ((curNode = curNode->next) != NULL);

	fclose(fp);

	recipeLog(5, "Calculator", "File", "Write", "Data for roadmap written.");
}

/*-------------------------------------------------------------------
 * Function 	: printSortData
 * Inputs	: FILE 	*fp
 *		  enum Action 	curNodeAction
 *
 * Print to a file data which pertains to sorting the inventory.
 -------------------------------------------------------------------*/
void printSortData(FILE *fp, enum Action curNodeAction) {
	fprintf(fp, "Sort - ");
	switch (curNodeAction) {
		case Sort_Alpha_Asc :
			fputs("Alphabetical", fp);
			break;
		case Sort_Alpha_Des :
			fputs("Reverse Alphabetical", fp);
			break;
		case Sort_Type_Asc :
			fputs("Type", fp);
			break;
		case Sort_Type_Des :
			fputs("Reverse Type", fp);
			break;
		default :
			fputs("ERROR IN HANDLING OF SORT", fp);
	};
}

/*-------------------------------------------------------------------
 * Function : reallocateRecipes
 * Inputs	: struct BranchPath	*newRoot
 *			  enum Type_Sort	*rearranged_recipes
 *			  int				num_rearranged_recipes
 *
 * Given a set of recipes, find alternative places in the roadmap to
 * cook these recipes such that we minimize the frame cost.
 * Note: Even though newRoot is never modified in this function,
 * it may be set as the pointer to newly allocated nodes, and thus is not const
 -------------------------------------------------------------------*/
void reallocateRecipes(struct BranchPath* mutableNewRoot, const enum Type_Sort* rearranged_recipes, int num_rearranged_recipes) {
  // Prefer to work with the const version when possible to ensure we really don't modify it.
  ABSL_ATTRIBUTE_UNUSED const struct BranchPath* newRoot = mutableNewRoot;
	for (int recipe_offset = 0; recipe_offset < num_rearranged_recipes; recipe_offset++) {
		// Establish a default bound for the optimal place for this item
		int record_frames = 9999;
		struct BranchPath *record_placement_node = NULL;
		struct Cook *record_description = NULL;
		struct Cook temp_description = {0};

		// Evaluate all recipes and determine the optimal recipe and location
		int recipe_index = getIndexOfRecipe(rearranged_recipes[recipe_offset]);
		struct Recipe recipe = recipeList[recipe_index];
		for (int recipe_combo_index = 0; recipe_combo_index < recipe.countCombos; recipe_combo_index++) {
			struct ItemCombination combo = recipe.combos[recipe_combo_index];

			// Evaluate placing after each node where it can be placed
			for (struct BranchPath *mutablePlacement = combo.numItems == 2 ? mutableNewRoot->next : mutableNewRoot;
			    mutablePlacement != NULL; mutablePlacement = mutablePlacement->next) {
			  // Prefer to work with the const version when possible to ensure we really don't modify it.
			  const struct BranchPath *placement = mutablePlacement;
				// Only want moments when there are no NULLs in the inventory
				if (placement->inventory.nulls) {
					continue;
				}

				// Only want recipes where all ingredients are in the last 10 slots of the evaluated inventory
				int indexItem1 = indexOfItemInInventory(placement->inventory, combo.item1);
				int indexItem2 = INDEX_ITEM_UNDEFINED;
				if (indexItem1 < 10) {
					continue;
				}
				if (combo.numItems == 2) {
					indexItem2 = indexOfItemInInventory(placement->inventory, combo.item2);
					if (indexItem2 < 10) {
						continue;
					}
				}

				// This is a valid recipe and location to fulfill (and toss) the output
				// Calculate the frames needed to produce this step
				int temp_frames = TOSS_FRAMES;
				temp_description = EMPTY_COOK;
				temp_description.output = recipe.output;
				temp_description.handleOutput = Toss;

				if (combo.numItems == 1) {
					// Only one ingredient to navigate to
					temp_frames += invFrames[placement->inventory.length - 1][indexItem1];
					temp_description.numItems = 1;
					temp_description.item1 = combo.item1;
					temp_description.itemIndex1 = indexItem1;
					temp_description.item2 = -1;
					temp_description.itemIndex2 = -1;
				}
				else {
					_assert_with_stacktrace(indexItem2 != INDEX_ITEM_UNDEFINED);

					// Two ingredients to navigate to, but order matters
					// Pick the larger-index number ingredient first, as it will reduce
					// the frames needed to reach the other ingredient
					temp_frames += CHOOSE_2ND_INGREDIENT_FRAMES;
					temp_description.numItems = 2;

					if (indexItem1 > indexItem2) {
						temp_frames += invFrames[placement->inventory.length - 1][indexItem1];
						temp_frames += invFrames[placement->inventory.length - 2][indexItem2];
						temp_description.item1 = combo.item1;
						temp_description.itemIndex1 = indexItem1;
						temp_description.item2 = combo.item2;
						temp_description.itemIndex2 = indexItem2;
					}
					else {
						temp_frames += invFrames[placement->inventory.length - 1][indexItem2];
						temp_frames += invFrames[placement->inventory.length - 2][indexItem1];
						temp_description.item1 = combo.item2;
						temp_description.itemIndex1 = indexItem2;
						temp_description.item2 = combo.item1;
						temp_description.itemIndex2 = indexItem1;
					}
				}

				// Compare the current placement to the current record
				if (temp_frames < record_frames) {
					// Update the record information
					record_frames = temp_frames;
					record_placement_node = mutablePlacement;

					if (record_description == NULL) {
						record_description = malloc(sizeof(struct Cook));
						checkMallocFailed(record_description);
					}
					copyCook(record_description, &temp_description);
				}
			}
		}

		// All recipe combos and intervals have been evaluated
		// Insert the optimized output in the designated interval
		if (record_placement_node == NULL) {
			// This is an error
			recipeLog(7, "Calculator", "Roadmap", "Optimize", "OptimizeRoadmap couldn't find a valid placement...");
			exit(1);
		}

		struct BranchPath *insertNode = createMoveZeroed();
		checkMallocFailed(insertNode);

		// Set pointers to and from surrounding structs
		insertNode->prev = record_placement_node;
		record_placement_node->next->prev = insertNode;
		insertNode->next = record_placement_node->next;
		record_placement_node->next = insertNode;

		// Initialize the new node
		insertNode->moves = record_placement_node->moves + 1;
		insertNode->inventory = record_placement_node->inventory;
		insertNode->description.action = Cook;
		insertNode->description.data = (void *)record_description;
		insertNode->description.framesTaken = record_frames;
		copyOutputsFulfilledNoAlloc(insertNode->outputCreated, record_placement_node->outputCreated);
		insertNode->outputCreated[recipe_index] = true;
		insertNode->numOutputsCreated = record_placement_node->numOutputsCreated + 1;
		insertNode->legalMoves = NULL;
		insertNode->numLegalMoves = 0;
		insertNode->capacityLegalMoves = 0;

		// Update all subsequent nodes with
		for (struct BranchPath *node = insertNode->next; node!= NULL; node = node->next) {
			node->outputCreated[recipe_index] = true;
			++node->numOutputsCreated;
			++node->moves;
		}
	}
}

/*-------------------------------------------------------------------
 * Function : removeRecipesForReallocation
 * Inputs	: struct BranchPath	*node
 *			  enum Type_Sort	*rearranged_recipes
 *
 * Look through a completed roadmap to find recipes which can be
 * performed elsewhere in the roadmap without affecting the inventory.
 * Store these recipes in rearranged_recipes for later.
 -------------------------------------------------------------------*/
int removeRecipesForReallocation(struct BranchPath* node, enum Type_Sort *rearranged_recipes) {
	int num_rearranged_recipes = 0;
	while (node->moves > 1) {
		// Ignore sorts/CH5
		if (node->description.action != Cook) {
			node = node->prev;
			continue;
		}

		// Ignore recipes which do not toss the output
		struct Cook* cookData = (struct Cook*)node->description.data;
		if (cookData->handleOutput != Toss) {
			node = node->prev;
			continue;
		}

		// At this point, we have a recipe which tosses the output
		// This output can potentially be relocated to a quicker time
		enum Type_Sort tossed_item = cookData->output;
		rearranged_recipes[num_rearranged_recipes] = tossed_item;
		num_rearranged_recipes++;

		// First update subsequent nodes to remove this item from outputCreated
		struct BranchPath* newNode = node->next;
		while (newNode != NULL) {
			newNode->outputCreated[getIndexOfRecipe(tossed_item)] = 0;
			newNode->numOutputsCreated--;
			newNode = newNode->next;
		}

		// Now, get rid of this current node
		checkMallocFailed(node->next);
		node->prev->next = node->next;
		node->next->prev = node->prev;
		newNode = node->prev;
		freeNode(node);
		node = newNode;
	}

	return num_rearranged_recipes;
}

/*-------------------------------------------------------------------
 * Function 	: selectSecondItemFirst
 * Inputs	: struct BranchPath 		*node
 *		  struct ItemCombination 	combo
 *		  int 				*ingredientLoc
 *		  int 				nulls
 *		  int 				viableItems
 * Outputs	: int (0 or 1)
 *
 * Calculates a boolean expression which determines whether it is faster
 * to select the second item before the first item originally listed in
 * the recipe combo.
 -------------------------------------------------------------------*/
int selectSecondItemFirst(int *ingredientLoc, size_t nulls, int viableItems) {
	return (ingredientLoc[0] - (int)nulls) >= 2
		   && ingredientLoc[0] > ingredientLoc[1]
		   && (ingredientLoc[0] - (int)nulls) <= viableItems/2
		   || (ingredientLoc[0] - (int)nulls) < ingredientLoc[1]
		   && (ingredientLoc[0] - (int)nulls) >= viableItems/2;
}

/*-------------------------------------------------------------------
 * Function 	: shiftDownLegalMoves
 * Inputs	: struct BranchPath	*node
 *		  int			lowerBound
 *		  int			upperBound
 *
 * If this function is called, we want to make room in the legal moves
 * array to place a new legal move. Shift all legal moves starting at
 * lowerBound one index towards the end of the list, ending at upperBound.
 * NOTE: Although a total of (upperBound - lowerBound) total elements are moved,
 * both lowerBound and upperBound (both inclusive) are referenced, unless
 * they are equal, in which case this function does nothing.
 *
 * This function does NOT update node->numLegalMoves. Caller should do
 * it themselves. (Unless the function is only
 * moving existing legal moves around, in which case it would be correct
 * to not increment it)
 -------------------------------------------------------------------*/
void shiftDownLegalMoves(struct BranchPath *node, int lowerBound, int uppderBound) {
	if (uppderBound == lowerBound) return;
	_assert_for_shifting_function(node != NULL);
	_assert_for_shifting_function(node->numLegalMoves <= node->capacityLegalMoves);
	_assert_for_shifting_function(lowerBound >= 0);
	_assert_for_shifting_function(uppderBound >= 0);
	_assert_for_shifting_function(uppderBound >= lowerBound);
	_assert_for_shifting_function(lowerBound < node->numLegalMoves);
	_assert_for_shifting_function(uppderBound <= node->numLegalMoves);
  struct BranchPath **legalMoves = node->legalMoves;
  memmove(&legalMoves[lowerBound + 1], &legalMoves[lowerBound], sizeof(&legalMoves[0])*(uppderBound - lowerBound));
	/*for (int i = uppderBound - 1; i >= lowerBound; i--) {
		legalMoves[i+1] = legalMoves[i];
	}*/
}

/*-------------------------------------------------------------------
 * Function 	: shiftUpLegalMoves
 * Inputs	: struct BranchPath	*node
 *		  int			index
 *
 * There is a NULL in the array of legal moves. The first valid legal
 * move AFTER the null is index. Iterate starting at the index of the
 * NULL legal moves and shift all subsequent legal moves towards the
 * front of the array.
 * WARNING: node->numLegalMove MUST already be decremented before
 * calling this function. Thus this function will actually edit
 * node->legalModes[node->numLegalMoves] on the assumption that
 * numLegalMoves is actually now below the allocated region.
 * NOTE: the destination actually starts with index - 1. If index == 0,
 * then you are asking to shift down index 0, which is already at the top,
 * so this function will do nothing.
 *
 * This function does NOT update node->numLegalMoves. Caller should
 * do it themselves.
 -------------------------------------------------------------------*/
void shiftUpLegalMoves(struct BranchPath *node, int startIndex) {
	_assert_with_stacktrace(startIndex > 0);
	if (node->numLegalMoves == 0) {
		// Nothing to shift
		_assert_for_shifting_function(startIndex == 0 || startIndex == 1);
#if VERIFYING_SHIFTING_FUNCTIONS || AGGRESSIVE_0_ALLOCATING
			if (node->capacityLegalMoves > 0) {
				node->legalMoves[0] = NULL;
			}
#endif
		return;
	}
	_assert_for_shifting_function(node != NULL);
	struct BranchPath **legalMoves = node->legalMoves;
	_assert_for_shifting_function(node->numLegalMoves >= 0);
	_assert_for_shifting_function(startIndex >= 1);
	if (startIndex == node->numLegalMoves + 1) {
		// We just freed the last element of the previous array.
#if VERIFYING_SHIFTING_FUNCTIONS || AGGRESSIVE_0_ALLOCATING
		// Null where the last entry was before shifting
		legalMoves[node->numLegalMoves] = NULL;
#endif
		return;
	}
	_assert_for_shifting_function(startIndex <= node->numLegalMoves);
	_assert_for_shifting_function(node->numLegalMoves < node->capacityLegalMoves);
	// Make sure we are actually shifting into a NULL slot.
	_assert_for_shifting_function(node->legalMoves[startIndex - 1] == NULL);
	memmove(&legalMoves[startIndex - 1], &legalMoves[startIndex], sizeof(&legalMoves[0])*(node->numLegalMoves - startIndex + 1));
	/*for (int i = startIndex; i <= node->numLegalMoves; i++) {
		node->legalMoves[i-1] = node->legalMoves[i];
	}*/
	// Null where the last entry was before shifting
	legalMoves[node->numLegalMoves] = NULL;
}

/*-------------------------------------------------------------------
 * Function 	: softMin
 * Inputs	: struct BranchPath	*node
 *
 * This is an experimental function which may substitute the original
 * "select" methodology when determining what next node to explore.
 * This is a variation of the Softmax function. Essentially, the
 * original methodology uses an arbitrary distribution when determining
 * which legal move to take. softMin uses the framecount of each legal
 * move to construct a distribution for which a given node will have a
 * higher probability than subsequent nodes depending on how much faster
 * the given node is compared to the subsequent nodes.
 * For more information on Softmax: https://en.wikipedia.org/wiki/Softmax_function
 -------------------------------------------------------------------*/
void softMin(struct BranchPath *node) {
	// If numLegalMoves is 0 or 1, we will get an error when trying to do x % 0
	if (node->numLegalMoves < 2) {
		return;
	}

	// Calculate the sum of framecount for all legalMoves
	int frameCountSum = 0;
	for (int i = 0; i < node->numLegalMoves; i++) {
		frameCountSum += node->legalMoves[i]->description.framesTaken;
	}

	// Do some janky shit to recalculate the sum such that the node with the
	// lowest framecount has a higher "value"
	int weightSum = 0;
	for (int i = 0; i < node->numLegalMoves; i++) {
		weightSum += (frameCountSum - node->legalMoves[i]->description.framesTaken);
	}

	// Generate a random number between 0 and weightSum
	int modSum = randint(0, weightSum);

	// Find the legal move that corresponds to the modSum
	int index;
	weightSum = 0;
	for (index = 0; index < node->numLegalMoves; index++) {
		weightSum += (frameCountSum - node->legalMoves[index]->description.framesTaken);
		if (modSum < weightSum) {
			// We have found the corresponding legal move
			break;
		}
	}

	// Move the indexth legal move to the front of the array
	// First store the indexth legal move in a separate pointer
	struct BranchPath *softMinNode = node->legalMoves[index];
	node->legalMoves[index] = NULL;

	// Make room at the beginning of the legal moves array for the softMinNode
	shiftDownLegalMoves(node, 0, index);

	// Set first index in array to the softMinNode
	_assert_with_stacktrace(node != softMinNode);
	node->legalMoves[0] = softMinNode;
}

/*-------------------------------------------------------------------
 * Function 	: shuffleLegalMoves
 * Inputs	: struct BranchPath	*node
 *
 * Randomize the order of legal moves by switching two legal moves
 * numlegalMoves times.
 -------------------------------------------------------------------*/
void shuffleLegalMoves(struct BranchPath *node) {
	// Swap 2 legal moves a variable number of times
	for (int i = 0; i < node->numLegalMoves; i++) {
		if (checkShutdownOnIndexWithPrefetch(i)) {
			break;
		}
		int index1 = randint(0, node->numLegalMoves);
		int index2 = randint(0, node->numLegalMoves);
		struct BranchPath *temp = node->legalMoves[index1];
		node->legalMoves[index1] = node->legalMoves[index2];
		node->legalMoves[index2] = temp;
	}

	return;
}

/*-------------------------------------------------------------------
 * Function 	: swapItems
 * Inputs	: int	*ingredientLoc
 *		  int 	*ingredientOffset
 *
 * After determining that it is faster to pick the second item before
 * the first for a given recipe (based on the state of our inventory),
 * swap the item slot locations and offsets (for printing purposes).
 -------------------------------------------------------------------*/
void swapItems(int *ingredientLoc) {
	int locTemp;

	locTemp = ingredientLoc[0];
	ingredientLoc[0] = ingredientLoc[1];
	ingredientLoc[1] = locTemp;

	return;
}

/*-------------------------------------------------------------------
 * Function 	: tryTossInventoryItem
 * Inputs	: struct BranchPath 	  *curNode
 *		  enum Type_Sort	  *tempInventory
 *		  MoveDescription useDescription
 *		  int 			  *tempOutputsFulfilled
 *		  int 			  numOutputsFulfilled
 *		  int 			  tossedIndex
 *		  enum Type_Sort	  output
 *		  int 			  tempFrames
 *		  int 			  viableItems
 *
 * For the given recipe, try to toss items in the inventory in order
 * to make room for the recipe output.
 -------------------------------------------------------------------*/
void tryTossInventoryItem(struct BranchPath *curNode, struct Inventory tempInventory, MoveDescription useDescription, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, enum Type_Sort output, int tempFrames, int viableItems) {
	for (int tossedIndex = 0; tossedIndex < 10; tossedIndex++) {
		enum Type_Sort tossedItem = tempInventory.inventory[tossedIndex];

		// Make a copy of the tempInventory with the replaced item
		struct Inventory replacedInventory = replaceItem(tempInventory, tossedIndex, output);

		if (!stateOK(replacedInventory, tempOutputsFulfilled, recipeList)) {
			continue;
		}

		// Calculate the additional tossed frames.
		int tossFrames = invFrames[viableItems][tossedIndex + 1];
		int replacedFrames = tempFrames + tossFrames;

		useDescription.framesTaken += tossFrames;
		useDescription.totalFramesTaken += tossFrames;

		finalizeLegalMove(curNode, replacedFrames, useDescription, replacedInventory, tempOutputsFulfilled, numOutputsFulfilled, TossOther, tossedItem, tossedIndex);
	}

	return;
}

/*-------------------------------------------------------------------
 * Function 	: alpha_sort
 * Inputs	: const void	*elem1
 *		  const void	*elem2
 * Outputs	: int		comparisonValue
 *
 * Evaluate an alphabetical ascending sort. This is a comparator function
 * called by stdlib's qsort.
 -------------------------------------------------------------------*/
int alpha_sort(const void *elem1, const void *elem2) {
	enum Type_Sort item1 = *((enum Type_Sort*)elem1);
	enum Type_Sort item2 = *((enum Type_Sort*)elem2);

	return getAlphaKey(item1) - getAlphaKey(item2);
}

/*-------------------------------------------------------------------
 * Function 	: alpha_sort_reverse
 * Inputs	: const void	*elem1
 *		  const void	*elem2
 * Outputs	: int		comparisonValue
 *
 * Evaluate an alphabetical descending sort. This is a comparator function
 * called by stdlib's qsort.
 -------------------------------------------------------------------*/
int alpha_sort_reverse(const void *elem1, const void *elem2) {
	enum Type_Sort item1 = *((enum Type_Sort*)elem1);
	enum Type_Sort item2 = *((enum Type_Sort*)elem2);

	return getAlphaKey(item2) - getAlphaKey(item1);
}

/*-------------------------------------------------------------------
 * Function 	: type_sort
 * Inputs	: const void	*elem1
 *		  const void	*elem2
 * Outputs	: int		comparisonValue
 *
 * Evaluate an type ascending sort. This is a comparator function
 * called by stdlib's qsort.
 -------------------------------------------------------------------*/
int type_sort(const void *elem1, const void *elem2) {
	enum Type_Sort item1 = *((enum Type_Sort*)elem1);
	enum Type_Sort item2 = *((enum Type_Sort*)elem2);

	return item1 - item2;
}

/*-------------------------------------------------------------------
 * Function 	: type_sort_reverse
 * Inputs	: const void	*elem1
 *		  const void	*elem2
 * Outputs	: int		comparisonValue
 *
 * Evaluate an type descending sort. This is a comparator function
 * called by stdlib's qsort.
 -------------------------------------------------------------------*/
int type_sort_reverse(const void *elem1, const void *elem2) {
	enum Type_Sort item1 = *((enum Type_Sort*)elem1);
	enum Type_Sort item2 = *((enum Type_Sort*)elem2);

	return item2 - item1;
}

/*-------------------------------------------------------------------
 * Function 	: getSortedInventory
 * Inputs	: enum Type_Sort *inventory
 *		  enum Action 	  sort
 * Outputs	: enum Type_Sort *sorted_inventory
 *
 * Given the type of sort, use qsort to create a new sorted inventory.
 -------------------------------------------------------------------*/
struct Inventory getSortedInventory(struct Inventory inventory, enum Action sort) {
	// Set up the inventory for sorting
	memmove(inventory.inventory, inventory.inventory + inventory.nulls,
		(inventory.length - inventory.nulls) * sizeof(enum Type_Sort));
	inventory.length -= inventory.nulls;
	inventory.nulls = 0;

	// Use qsort and execute sort function depending on sort type
	switch(sort) {
		case Sort_Alpha_Asc :
			qsort((void*)inventory.inventory, inventory.length, sizeof(enum Type_Sort), alpha_sort);
			return inventory;
		case Sort_Alpha_Des :
			qsort((void*)inventory.inventory, inventory.length, sizeof(enum Type_Sort), alpha_sort_reverse);
			return inventory;
		case Sort_Type_Asc :
			qsort((void*)inventory.inventory, inventory.length, sizeof(enum Type_Sort), type_sort);
			return inventory;
		case Sort_Type_Des :
			qsort((void*)inventory.inventory, inventory.length, sizeof(enum Type_Sort), type_sort_reverse);
			return inventory;
		default :
			printf("Error in sorting inventory.\n");
			exit(2);
	}
}

static void logWithThreadInfo(int ID, char* toLogStr, int level) {
	if (will_log_level(level)) {
		char callString[30];
		sprintf(callString, "Thread %d", ID);
		recipeLog(level, "Calculator", "Info", callString, toLogStr);
	}
}

static void logIterations(int ID, int stepIndex, const struct BranchPath * curNode, long iterationCount, long iterationLimit, int level)
{
	if (will_log_level(level)) {
		char callString[30];
		char iterationString[200];
		sprintf(callString, "Thread %d", ID);
		sprintf(iterationString, "%d steps currently taken, %d frames accumulated so far; %ldk iterations (%ldk current iteration max)",
			stepIndex, curNode->description.totalFramesTaken, iterationCount / 1000, iterationLimit / 1000);
		recipeLog(level, "Calculator", "Info", callString, iterationString);
	}
}

// destText is a pointer to a char array of size 50.
static bool handleAfterOptimizing(char (*destText)[50], int afterOptimizingFrames) {
	if (afterOptimizingFrames <= 0 || afterOptimizingFrames > UNSET_FRAME_RECORD) {
		*destText[0] = 0;
		return false;
	} else {
		sprintf(*destText, " (%d after optimizing)", afterOptimizingFrames);
		return true;
	}
}

static void logCloseToPb(int ID, size_t preambleTextLength, const char preambleText[preambleTextLength], const struct BranchPath * curNode, int pbFrames, int afterOptimizingFrames, int level) {
	if (will_log_level(level)) {
		_assert_with_stacktrace(preambleTextLength < 80);
		char callString[30];
		char outText[200];
		char afterOptimizing[50];
		sprintf(callString, "Thread %d", ID);
		handleAfterOptimizing(&afterOptimizing, afterOptimizingFrames);
		const int currentFrames = curNode->description.totalFramesTaken;
		sprintf(outText, "%s: Current %d%s, PB: %d, difference %d",
				preambleText,
				currentFrames,
				afterOptimizing,
				pbFrames,
				currentFrames - pbFrames);
		recipeLog(level, "Calculator", "Info", callString, outText);
	}
}

static void logIterationsAfterLimitIncrease(int ID, int stepIndex, const struct BranchPath * curNode, int afterOptimizingFrames, long iterationCount, long oldIterationLimit, long iterationLimit, int level)
{
	if (will_log_level(level)) {
		char callString[30];
		char afterOptimizing[50];
		char iterationString[300];
		sprintf(callString, "Thread %d", ID);
		handleAfterOptimizing(&afterOptimizing, afterOptimizingFrames);
		sprintf(iterationString, "%d steps currently taken, %d%s; %ldk iterations (%ldk previous iteration max, %ldk new iteration max)",
			stepIndex, curNode->description.totalFramesTaken, afterOptimizing, iterationCount / 1000, oldIterationLimit / 1000, iterationLimit / 1000);
		recipeLog(level, "Calculator", "Info", callString, iterationString);
	}
}

static int iterationDefaultLogBranchValCompute(int branchInterval) {
	return branchInterval <= DEFAULT_TERATION_LOG_BRANCH_INTERVAL_MAX_BEFORE_SLOWER_SCALING ? branchInterval :
		DEFAULT_TERATION_LOG_BRANCH_INTERVAL_MAX_BEFORE_SLOWER_SCALING + ((branchInterval - DEFAULT_TERATION_LOG_BRANCH_INTERVAL_MAX_BEFORE_SLOWER_SCALING)/DEFAULT_TERATION_LOG_BRANCH_INTERVAL_DIVISOR);
}

static long iterationDefaultLogInterval(int branchInterval) {
	return iterationDefaultLogBranchValCompute(branchInterval) * DEFAULT_ITERATION_LIMIT;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE static inline int getLimitIncreaseDivisor(int currentPb) {
	// After the flurry of it being true in the beginning, this will almost always be false once PB gets to a somewhat optimized level.
	return ABSL_PREDICT_FALSE(currentPb >= WEAK_PB_FLOOR) ? WEAK_PB_FLOOR_DIVISIOR : 1;
}

/*-------------------------------------------------------------------
 * Function 	: calculateOrder
 * Inputs	: int ID
 * Outputs	: struct Result	result
 *
 * This is the main roadmap evaluation function. This calls various
 * child functions to generate a sequence of legal moves. It then
 * uses parameters to determine which legal move to traverse to. Once
 * a roadmap is found, the data is printed to a .txt file, and the result
 * is passed back to start.c to try submitting to the server.
 -------------------------------------------------------------------*/
struct Result calculateOrder(const int rawID, long max_branches) {
	// For debugging stacktraces
	// _assert_with_stacktrace(false);
	const int displayID = rawID + 1;
	const int randomise = getConfigInt("randomise");
	const int select = getConfigInt("select");
	const int debug = getConfigInt("debug");
	// The user may disable all randomization but not be debugging.
	int freeRunning = !debug && !randomise && !select;
	const int branchInterval = getConfigInt("branchLogInterval");
	const int defaultIterationLogInterval = iterationDefaultLogInterval(branchInterval);

	long total_dives = 0;
	struct BranchPath *curNode = NULL; // Deepest node at any particular point
	struct BranchPath *root;

	struct Result result_cache = (struct Result) {-1, -1};

	//Start main loop
	while (1) {
		if (askedToShutdown()) {
			break;
		}
		if (max_branches > 0 && total_dives >= max_branches) {
			break;
		}

		int stepIndex = 0;
		long iterationCount = 0;
		bool useShortIterationLimit = (rand() % 100) < SHORT_ITERATION_LIMIT_CHANCE;
		long iterationLimit = useShortIterationLimit ? DEFAULT_ITERATION_LIMIT_SHORT : DEFAULT_ITERATION_LIMIT;
		bool iterationLimitIncreased = false;
		bool iterationLimitIncreasedFromPB = false;
		bool iterationLimitIncreasedFromGettingClose = false;
		bool iterationLimitIncreasedFromGettingKindOfClose = false;

		// Create root of tree path
		curNode = initializeRoot();
		root = curNode; // Necessary when printing results starting from root

		total_dives++;

		if (total_dives % branchInterval == 0 && will_log_level(NEW_BRANCH_LOG_LEVEL)) {
			char temp1[30];
			char temp2[50];
			sprintf(temp1, "Thread %d", displayID);
			sprintf(temp2, "Searching New Branch %ld", total_dives);
			recipeLog(NEW_BRANCH_LOG_LEVEL, "Calculator", "Info", temp1, temp2);
		}

		// If the user is not exploring only one branch, reset when it is time
		// Start iteration loop
		while (iterationCount < iterationLimit || freeRunning) {
			if (checkShutdownOnIndexLong(iterationCount)) {
				break;
			}
			// In the rare occassion that the root node runs out of legal moves due to "select",
			// exit out of the while loop to restart
			if (curNode == NULL) {
				NOISY_DEBUG("No current node. Breaking for next branch.\n");
				if (iterationLimitIncreased) {
					logWithThreadInfo(displayID, "No more legal moves at all: moving to next branch", 4);
					logIterations(displayID, stepIndex, curNode, iterationCount, iterationLimit, 4);
				}
				break;
			}

			// Check for end condition (57 recipes + the Chapter 5 intermission)
			if(curNode->numOutputsCreated == NUM_RECIPES) {
				NOISY_DEBUG("End condition\n");
				const long oldIterationLimit = iterationLimit;
				// All recipes have been fulfilled!
				// Check that the total time taken is strictly less than the current observed record.
				// Apply a frame penalty if the final move did not toss an item.
				applyJumpStorageFramePenalty(curNode);

				const int currentPb = curNode->description.totalFramesTaken;
				const int limitIncreaseDivisor = getLimitIncreaseDivisor(currentPb);

				if (currentPb < getLocalRecord() + BUFFER_SEARCH_FRAMES) {
					// A finished roadmap has been generated
					// We are getting close enough to spend extra time on this branch.

					// Rearrange the roadmap to save frames
					struct OptimizeResult optimizeResult = optimizeRoadmap(root);
					if (optimizeResult.last->description.totalFramesTaken < getLocalRecord()) {
						NOISY_DEBUG("New PB!\n");
						#pragma omp critical(optimize)
						{
							setLocalRecord(optimizeResult.last->description.totalFramesTaken);
							char *filename = malloc(sizeof(char) * 17);
							sprintf(filename, "results/%d.txt", optimizeResult.last->description.totalFramesTaken);
							printResults(filename, optimizeResult.root);
							if (will_log_level(1)) {
								char tmp[200];
								sprintf(tmp, "Thread %d][New local fastest roadmap found! %d frames, saved %d after rearranging", displayID, optimizeResult.last->description.totalFramesTaken, curNode->description.totalFramesTaken - optimizeResult.last->description.totalFramesTaken);
								recipeLog(1, "Calculator", "Info", "Roadmap", tmp);
							}
							free(filename);
							if (debug) {
								testRecord(result_cache.frames);
							}
						}
						result_cache = (struct Result){ optimizeResult.last->description.totalFramesTaken, rawID };

						// Reset the iteration count so we continue to explore near this record
						if (ABSL_PREDICT_TRUE(iterationLimit < ITERATION_LIMIT_MAX)) {
							if (!iterationLimitIncreasedFromPB) {
								// On the first time with PB increase, use ITERATION_LIMIT_INCREASE_FIRST.
								if (iterationLimitIncreasedFromGettingClose) {
									iterationLimit = MAX(
										// If ITERATION_LIMIT_INCREASE_GETTING_CLOSE has already been applied, subtract that to get to ITERATION_LIMIT_INCREASE_FIRST.
										iterationLimit + (ITERATION_LIMIT_INCREASE_FIRST/limitIncreaseDivisor) - ITERATION_LIMIT_INCREASE_GETTING_CLOSE,
										iterationCount + (ITERATION_LIMIT_INCREASE_FIRST/limitIncreaseDivisor));
								} else {
									iterationLimit = MAX(
										iterationCount + (ITERATION_LIMIT_INCREASE_FIRST/limitIncreaseDivisor),
										iterationLimit + 2 * ITERATION_LIMIT_INCREASE_PAST_MAX);
								}
							} else {
								iterationLimit = MAX(iterationCount + (ITERATION_LIMIT_INCREASE/limitIncreaseDivisor), iterationLimit + ITERATION_LIMIT_INCREASE_PAST_MAX);
							}
							if (ABSL_PREDICT_FALSE(iterationLimit > ITERATION_LIMIT_MAX)) {
								iterationLimit = ITERATION_LIMIT_MAX;
							}
						} else {
							iterationLimit = MAX(iterationCount + (ITERATION_LIMIT_INCREASE_PAST_MAX/limitIncreaseDivisor), iterationLimit + ITERATION_LIMIT_INCREASE_PAST_MAX/50);
						}
						if (iterationLimit > oldIterationLimit) {
							iterationLimitIncreased = true;
							iterationLimitIncreasedFromPB = true;
						}
						logIterationsAfterLimitIncrease(displayID, stepIndex, curNode, optimizeResult.last->description.totalFramesTaken, iterationCount, oldIterationLimit, iterationLimit, 3);
					} else {  // Close enough to optimizeRoadmap but not quite PB
						NOISY_DEBUG("Not new PB but close\n");
						// Close enough to optimize but not to PB, still worth spending more time on the branch.
						if (!iterationLimitIncreasedFromGettingClose && !iterationLimitIncreased && ABSL_PREDICT_TRUE(iterationLimit < ITERATION_LIMIT_MAX)) {
							if (iterationLimitIncreasedFromGettingKindOfClose) {
								iterationLimit = MAX(
									// If ITERATION_LIMIT_INCREASE_GETTING_KINDOF_CLOSE has already been applied, subtract that to get to ITERATION_LIMIT_INCREASE_GETTING_CLOSE.
									iterationLimit + (ITERATION_LIMIT_INCREASE_GETTING_CLOSE/limitIncreaseDivisor) - ITERATION_LIMIT_INCREASE_GETTING_KINDOF_CLOSE,
									iterationCount + (ITERATION_LIMIT_INCREASE_GETTING_CLOSE/limitIncreaseDivisor));
							} else {
								iterationLimit = MAX(
									iterationCount + ITERATION_LIMIT_INCREASE_GETTING_CLOSE/limitIncreaseDivisor,
									iterationLimit + ITERATION_LIMIT_INCREASE_GETTING_CLOSE/50);
							}
							if (ABSL_PREDICT_FALSE(iterationLimit > ITERATION_LIMIT_MAX)) {
								iterationLimit = ITERATION_LIMIT_MAX;
							}
							if (iterationLimit > oldIterationLimit) {
								static const char closeAndOptimizePreamble[] = "Close enough to PB to spend more time on this branch and optimize";
								// Only log this once
								int afterOptimizing = optimizeResult.last->description.totalFramesTaken;
								logCloseToPb(displayID, sizeof(closeAndOptimizePreamble), closeAndOptimizePreamble, curNode, getLocalRecord(), afterOptimizing, 4);
								logIterationsAfterLimitIncrease(displayID, stepIndex, curNode, afterOptimizing, iterationCount, oldIterationLimit, iterationLimit, 4);
								iterationLimitIncreased = true;
								iterationLimitIncreasedFromGettingClose = true;
								iterationLimitIncreasedFromGettingKindOfClose = true;
							}
						}
					}
					freeAllNodes(optimizeResult.last);
				} else if (curNode->description.totalFramesTaken < getLocalRecord() + BUFFER_SEARCH_FRAMES_KIND_OF_CLOSE) {
					// Close enough to look harder but not enough to put in the optimizeRoadmap overhead yet.
					NOISY_DEBUG("Kind of close\n");
					if (!iterationLimitIncreased && !iterationLimitIncreasedFromGettingKindOfClose && ABSL_PREDICT_TRUE(iterationLimit < ITERATION_LIMIT_MAX)) {
						iterationLimit = MAX(
															iterationCount + (ITERATION_LIMIT_INCREASE_GETTING_KINDOF_CLOSE/limitIncreaseDivisor),
															iterationLimit + ITERATION_LIMIT_INCREASE_GETTING_KINDOF_CLOSE/50);
						if (iterationLimit > oldIterationLimit) {
							static const char closePreamble[] = "Close enough to PB to spend more time on this branch";
							logCloseToPb(displayID, sizeof(closePreamble), closePreamble, curNode, getLocalRecord(), 0, 4);
							logIterationsAfterLimitIncrease(displayID, stepIndex, curNode, -1, iterationCount, oldIterationLimit, iterationLimit, 4);
							iterationLimitIncreasedFromGettingKindOfClose = true;
							// This is such a tiny increase we aren't even bothering to set iterationLimitIncreased.
						}
					}
				}

				// Regardless of record status, it's time to go back up and find new endstates
				curNode = curNode->prev;
				freeLegalMove(curNode, 0);
				curNode->next = NULL;
				stepIndex--;

				continue;
			}
			// End condition not met. Check if this current level has something in the event queue
			else if (curNode->legalMoves == NULL) {
				NOISY_DEBUG("End condition not met. Check if this current level has something in the event queue\n");
				// This node has not yet been assigned an array of legal moves.
				// Generate the list of all possible recipes
				fulfillRecipes(curNode);

				// Special handling of the 56th recipe, which is representative of the Chapter 5 intermission

				// The first item is trading the Mousse Cake and 2 Hot Dogs for a Dried Bouquet
				// Inventory must contain both items, and Hot Dog must be in a slot such that it can be duplicated
				// The Mousse Cake and Hot Dog cannot be in a slot such that it is "hidden" due to NULLs in the inventory
				if (!curNode->outputCreated[getIndexOfRecipe(Dried_Bouquet)]
					&& indexOfItemInInventory(curNode->inventory, Mousse_Cake) != -1
					&& indexOfItemInInventory(curNode->inventory, Hot_Dog) >= 10) {
					fulfillChapter5(curNode);
				}

				// Special handling of inventory sorting
				// Avoid redundant searches
				if (curNode->description.action == Begin || curNode->description.action == Cook || curNode->description.action == Ch5) {
					handleSorts(curNode);
				}

				// All legal moves evaluated and listed!

				if (curNode->moves == 0) {
					// Filter out all legal moves that use 2 ingredients in the very first legal move
					filterOut2Ingredients(curNode);
				}

				// Special filtering if we only had one recipe left to fulfill
				if (curNode->numOutputsCreated == NUM_RECIPES-1 && curNode->numLegalMoves > 0 && curNode->legalMoves != NULL && curNode->legalMoves[0]->description.action == Cook) {
					// If there are any legal moves that satisfy this final recipe,
					// strip out everything besides the fastest legal move
					// This saves on recursing down pointless states
					popAllButFirstLegalMove(curNode);
				}
				// Apply randomization when not debugging or when done
				// choosing moves
				else if (!debug || freeRunning) {
					handleSelectAndRandom(curNode, select, randomise);
				}

				if (curNode->numLegalMoves == 0) {
					// There are no legal moves to iterate on
					// Go back up!

					// Handle the case where the root node runs out of legal moves
					if (curNode->prev == NULL) {
						freeNode(curNode);
						return (struct Result) {-1, -1};
					}

					struct BranchPath* curNodePrev = curNode->prev;
					freeLegalMove(curNodePrev, 0);
					curNodePrev->next = NULL;
					curNode = curNodePrev;
					stepIndex--;
					continue;
				}

				// Allow the user to choose their path when in debugging mode
				else if (debug && !freeRunning) {
					FILE *fp = stdout;
					for (int move = 0; move < curNode->numLegalMoves; ++move) {
						fprintf(fp, "%d - ", move);
						printNodeDescription(curNode->legalMoves[move], fp);
						fprintf(fp, "\n");
					}

					fprintf(fp, "%d - Run freely\n", curNode->numLegalMoves);

					printf("Which move would you like to perform? ");
					int moveToExplore;
					scanf("%d", &moveToExplore);
					fprintf(fp, "\n");

					if (moveToExplore == curNode->numLegalMoves) {
						freeRunning = 1;
						handleSelectAndRandom(curNode, select, randomise);
					}
					else {
						// Take the legal move at nextMoveIndex and move it to the front of the array
						struct BranchPath *nextMove = curNode->legalMoves[0];
						curNode->legalMoves[0] = curNode->legalMoves[moveToExplore];
						curNode->legalMoves[moveToExplore] = nextMove;
					}
				}

				// Once the list is generated choose the top-most path and iterate downward

				checkMallocFailed(curNode->legalMoves);

				curNode->next = curNode->legalMoves[0];
				curNode = curNode->next;
				stepIndex++;

			}
			else {
				NOISY_DEBUG("Normal end\n");
				if (curNode->numLegalMoves == 0) {
					NOISY_DEBUG("No moves left\n");
					// No legal moves are left to evaluate, go back up...
					// Wipe away the current node

					// Handle the case where the root node runs out of legal moves
					if (curNode->prev == NULL) {
						freeNode(curNode);
						return (struct Result) {-1, -1};
					}

					curNode = curNode->prev;
					freeLegalMove(curNode, 0);
					curNode->next = NULL;
					stepIndex--;
					continue;
				}

				prefetchShutdownOnIndexLong(iterationCount);

				// Moves would already be shuffled with randomise, but select
				// would always choose the first one unless we select here
				handleSelectAndRandom(curNode, select, 0);

				// Once the list is generated, choose the top-most (quickest) path and iterate downward
				curNode->next = curNode->legalMoves[0];
				curNode = curNode->legalMoves[0];
				stepIndex++;

				// Logging for progress display
				iterationCount++;
				if ((iterationCount % defaultIterationLogInterval) == 0
					&& (freeRunning || iterationLimit != DEFAULT_ITERATION_LIMIT)) {
					logIterations(displayID, stepIndex, curNode, iterationCount, iterationLimit, 3);
				}
				else if ((iterationCount % VERBOSE_ITERATION_LOG_RATE) == 0) {
					logIterations(displayID, stepIndex, curNode, iterationCount, iterationLimit, 6);
				}
			}
		}

		// We have passed the iteration maximum
		// Free everything before reinitializing
		freeAllNodes(curNode);
		curNode = NULL;

		// Check the cache to see if a result was generated
		if (result_cache.frames > -1) {

			// Enter critical section to prevent corrupted file
			#pragma omp critical(pb)
			{
				// Prevent slower threads from overwriting a faster record in PB.txt
				// by first checking the current record
				FILE* fp = NULL;
				if ((fp = fopen("results/PB.txt", "r+")) == NULL) {
					// The file has not been created
					fp = fopen("results/PB.txt", "w");
				}
				else {
					// Verify that this new roadmap is faster than PB
					int pb_record;
					fscanf(fp, "%d", &pb_record);
					fclose(fp);
					fp = NULL;
					if (result_cache.frames > pb_record) {
						// This is a slower thread and a faster record was already found
						result_cache = (struct Result) { -1, -1 };
					}
					else {
						if (ABSL_PREDICT_TRUE(result_cache.frames > -1)) {
							fp = fopen("results/PB.txt", "w");
						}
					}
				}

				// Modify PB.txt
				if (fp != NULL) {
					if (ABSL_PREDICT_FALSE(result_cache.frames < 0)) {
						// Somehow got invalid number of frames.
						// Fetch the current known max from the global.
						int localRecord = getLocalRecord();
						if (ABSL_PREDICT_FALSE(localRecord < 0)) {
							recipeLog(1, "Calculator", "Roadmap", "Error", "Current cached local record is corrupt (less then 0 frames). Not writing invalid PB file but your PB may be lost.");
						} else if (ABSL_PREDICT_FALSE(localRecord > UNSET_FRAME_RECORD)) {
							char reportStr[200];
							sprintf(reportStr, "Current cached local record is corrupt (current [%d] greater then nonsense upper bound of [%d]). Not writing invalid PB file but your PB may be lost.",
								localRecord, UNSET_FRAME_RECORD);
							recipeLog(1, "Calculator", "Roadmap", "Error", reportStr);
						} else {
							fprintf(fp, "%d", localRecord);
						}
					} else {
						fprintf(fp, "%d", result_cache.frames);
					}
					fclose(fp);
				}
			}

			// Return the cached result
			return result_cache;
		}

		// Period check for Github update (only perform on thread 0)
		if (total_dives % 10000 == 0 && rawID == 0) {
			periodicGithubCheck();
		}

		// For profiling
		/*if (total_dives >= 100) {
			return (struct Result) { -1, -1 };
		}*/

	}

	// Unexpected break out of loop. Return the nothing results.
	return (struct Result) { -1, -1 };
}

