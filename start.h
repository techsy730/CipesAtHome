#ifndef START_H
#define START_H

#include "inventory.h"
#include "recipes.h"
#include "config.h"
#include "calculator.h"

struct Result {
	int frames;
	int callNumber;
};

// May get a value <0 if local record was corrupt.
int getLocalRecord();
void setLocalRecord(int frames);
const char* getLocalVersion();

int main(int argc, char **argv); // Main method for entire algorithm

#endif
