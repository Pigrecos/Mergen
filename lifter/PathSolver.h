#pragma once
#include "includes.h"

void minimal_optpass(Function* clonedFuncx);

void final_optpass(Function* clonedFuncx);

PATH_info solvePath(Function* function, uint64_t& dest, Value* simplifyValue);