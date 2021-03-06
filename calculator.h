#ifndef CALCULATOR_H
#define CALCULATOR_H
#include "base.h"
#include <stdbool.h>
#include "inventory.h"
#include "recipes.h"
#include "start.h"

// Represent the action at a particular node in the roadmap
enum Action {
	Begin,
	Cook,
	Sort_Alpha_Asc,
	Sort_Alpha_Des,
	Sort_Type_Asc,
	Sort_Type_Des,
	Ch5
};

// What do we do after producing a recipe?
enum HandleOutput  {
	Toss,		// Toss the recipe itself
	Autoplace,	// The recipe is placed in an empty slot
	TossOther	// Toss a different item to make room
};

// Information pertaining to cooking a recipe
struct Cook {
	int numItems;
	enum Type_Sort item1;
	int itemIndex1;
	enum Type_Sort item2;
	int itemIndex2;
	enum Type_Sort output;
	enum HandleOutput handleOutput;
	enum Type_Sort toss;
	int indexToss;
};

// Information pertaining to Chapter 5 evaluation
struct CH5 {
	int indexDriedBouquet;	// index of item we toss to make room for Dried Bouquet
	int indexCoconut;		// index of item we toss to make room for Coconut
	enum Action ch5Sort;	// The sort type we perform after Coconut
	int indexKeelMango;		// index of item we toss to make room for Keel Mango
	int indexCourageShell;	// index of item we toss to make room for Courage Shell
	int indexThunderRage;	// index of Thunder Rage when we use it during Smorg (if 0-9, TR is removed)
	int lateSort;			// 0 - sort after Coconut, 1 - sort after Keel Mango
};

struct CH5_Eval {
	int frames_DB;
	int frames_CO;
	int frames_KM;
	int frames_CS;
	int DB_place_index;
	int CO_place_index;
	int KM_place_index;
	int CS_place_index;
	int TR_use_index;
	int frames_HD;
	int frames_MC;
	int frames_TR;
	int sort_frames;
	enum Action sort;
};

// Overall data pertaining to what we did at a particular point in the roadmap
struct MoveDescription {
	enum Action action;		// Cook, sort, handle CH5,...
	void *data;				// This data may be either of type Cook, CH5, or NULL if we are just sorting
	int framesTaken;		// How many frames were wasted to perform this move
	int totalFramesTaken;	// Cummulative frame loss
};

struct BranchPath {
	int moves;							// Represents how many nodes we've traversed down a particular branch (0 for root, 57 for leaf node)
	struct Inventory inventory;
	struct MoveDescription description;
	struct BranchPath *prev;
	struct BranchPath *next;
	outputCreatedArray_t outputCreated;					// Array of 58 items, true if item was produced, false if not; indexed by recipe ordering
	int numOutputsCreated;				// Number of valid outputCreated entries to eliminate a lengthy for-loop
	struct BranchPath **legalMoves;		// Represents possible next paths to take
	int numLegalMoves;
	ssize_t capacityLegalMoves;
	int totalSorts;
};

// Structure to return the head and tail of an optimized roadmap
struct OptimizeResult {
	struct BranchPath *root;
	struct BranchPath *last;
};

// optimizeRoadmap functions
struct BranchPath* copyAllNodes(struct BranchPath* newNode, const struct BranchPath* oldNode);
struct OptimizeResult optimizeRoadmap(const struct BranchPath* root);
void reallocateRecipes(struct BranchPath *newRoot, const enum Type_Sort *rearranged_recipes, int num_rearranged_recipes);
int removeRecipesForReallocation(struct BranchPath *node, enum Type_Sort *rearranged_recipes);

// Legal move functions

struct BranchPath* createLegalMove(struct BranchPath* node, struct Inventory inventory, struct MoveDescription description, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled);
void filterOut2Ingredients(struct BranchPath* node);
void finalizeChapter5Eval(struct BranchPath* node, struct Inventory inventory, struct CH5* ch5Data, int temp_frame_sum, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled);
void finalizeLegalMove(struct BranchPath* node, int tempFrames, struct MoveDescription useDescription, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, enum HandleOutput tossType, enum Type_Sort toss, int tossIndex);
void freeLegalMove(struct BranchPath* node, int index);
int getInsertionIndex(const struct BranchPath* node, int frames);
void insertIntoLegalMoves(int insertIndex, struct BranchPath* newLegalMove, struct BranchPath* curNode);
void popAllButFirstLegalMove(struct BranchPath* node);
void shiftDownLegalMoves(struct BranchPath *node, int lowerBound, int uppderBound);
void shiftUpLegalMoves(struct BranchPath* node, int startIndex);

// Cooking functions

/*-------------------------------------------------------------------
 * Function 	: copyCook
 * Inputs	: struct Cook *cookNew
 *		  struct Cook *cookOld
 * 
 * A simple function to copy the data within cookOld to cookNew.
 -------------------------------------------------------------------*/
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void copyCook(struct Cook *cookNew, const struct Cook *cookOld) {
	*cookNew = *cookOld;
	return;
}
void createCookDescription2Items(const struct BranchPath* node, struct Recipe recipe, struct ItemCombination combo, struct Inventory* tempInventory, int* ingredientLoc, int* tempFrames, int viableItems, struct MoveDescription* useDescription);
void createCookDescription1Item(const struct BranchPath* node, struct Recipe recipe, struct ItemCombination combo, struct Inventory* tempInventory, int* ingredientLoc, int* tempFrames, int viableItems, struct MoveDescription* useDescription);
struct MoveDescription createCookDescription(const struct BranchPath* node, struct Recipe recipe, struct ItemCombination combo, struct Inventory *tempInventory, int* tempFrames, int viableItems);
void fulfillRecipes(struct BranchPath* curNode);
void generateCook(struct MoveDescription* description, const struct ItemCombination combo, const struct Recipe recipe, const int* ingredientLoc, int swap);
void handleRecipeOutput(struct BranchPath* curNode, struct Inventory tempInventory, int tempFrames, struct MoveDescription useDescription, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, enum Type_Sort output, int viableItems);
void tryTossInventoryItem(struct BranchPath* curNode, struct Inventory tempInventory, struct MoveDescription useDescription, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, enum Type_Sort output, int tempFrames, int viableItems);

// Chapter 5 functions
void fulfillChapter5(struct BranchPath* curNode);
void handleChapter5Eval(struct BranchPath* node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
void handleChapter5EarlySortEndItems(struct BranchPath* node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
void handleChapter5Sorts(struct BranchPath* node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
void handleChapter5LateSortEndItems(struct BranchPath* node, struct Inventory inventory, const outputCreatedArray_t outputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
void handleDBCOAllocation0Nulls(struct BranchPath* curNode, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
void handleDBCOAllocation1Null(struct BranchPath* curNode, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
void handleDBCOAllocation2Nulls(struct BranchPath* curNode, struct Inventory tempInventory, const outputCreatedArray_t tempOutputsFulfilled, int numOutputsFulfilled, struct CH5_Eval eval);
struct CH5* createChapter5Struct(struct CH5_Eval eval, int lateSort);

// Initialization functions
void initializeInvFrames();
void initializeRecipeList();

// File output functions
void printCh5Data(const struct BranchPath* curNode, struct MoveDescription desc, FILE* fp);
void printCh5Sort(const struct CH5* ch5Data, FILE* fp);
void printCookData(const struct BranchPath* curNode, struct MoveDescription desc, FILE* fp);
void printFileHeader(FILE* fp);
void printInventoryData(const struct BranchPath* curNode, FILE* fp);
void printOutputsCreated(const struct BranchPath* curNode, FILE* fp);
void printNodeDescription(const struct BranchPath * curNode, FILE * fp);
void printResults(const char* filename, const struct BranchPath* path);
void printSortData(FILE* fp, enum Action curNodeAction);

// Select and random methodology functions
void handleSelectAndRandom(struct BranchPath* curNode, int select, int randomise);
void shuffleLegalMoves(struct BranchPath* node);
void softMin(struct BranchPath *node);

// Sorting functions
int alpha_sort(const void* elem1, const void* elem2);
int alpha_sort_reverse(const void* elem1, const void* elem2);
struct Inventory getSortedInventory(struct Inventory inventory, enum Action sort);
int getSortFrames(enum Action action);
void handleSorts(struct BranchPath* curNode);
int type_sort(const void* elem1, const void* elem2);
int type_sort_reverse(const void* elem1, const void* elem2);

// Frame calculation and optimization functions
void applyJumpStorageFramePenalty(struct BranchPath *node);
void generateFramesTaken(struct MoveDescription* description, const struct BranchPath* node, int framesTaken);
int selectSecondItemFirst(int* ingredientLoc, size_t nulls, int viableItems);
void swapItems(int* ingredientLoc);

// General node functions
// No longer public API
// ABSL_MUST_USE_RESULT_INCLUSIVE int *copyOutputsFulfilled(int *oldOutputsFulfilled);
void freeAllNodes(struct BranchPath* node);
void freeNode(struct BranchPath *node);
struct BranchPath* initializeRoot();

// Other
void periodicGithubCheck();
// void logIterations(int ID, int stepIndex, struct BranchPath * curNode, long iterationCount, long iterationLimit, int level);
struct Result calculateOrder(int rawID, long max_branches);

#endif
