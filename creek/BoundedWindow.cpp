/*
 * BoundedWindow.c
 *
 *  Created on: Jun 24, 2016
 *      Author: xzl
 */


#include "BoundedWindow.h"

// xzl: they're ptime. must be initialized out of line. and cannot be in
// the headers

const etime BoundedWindow::TIMESTAMP_MIN_VALUE(etime_min);
const etime BoundedWindow::TIMESTAMP_MAX_VALUE(etime_max);

//etime GlobalWindow::END_OF_GLOBAL_WINDOW = TIMESTAMP_MAX_VALUE - 24 * 3600 * 1000 * 1000; // will overflow
etime GlobalWindow::END_OF_GLOBAL_WINDOW = TIMESTAMP_MAX_VALUE - 1000;

//GlobalWindow INSTANCE;
