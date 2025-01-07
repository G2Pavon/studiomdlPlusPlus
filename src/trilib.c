// trilib.c: library for loading triangles from an Alias triangle file
#include <stdio.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "trilib.h"

// on disk representation of a face
#define FLOAT_START 99999.0
#define FLOAT_END -FLOAT_START
#define MAGIC 123322

// #define NOISY 1

typedef struct
{
	float v[3];
} vector;

typedef struct
{
	vector n; /* normal */
	vector p; /* point */
	vector c; /* color */
	float u;  /* u */
	float v;  /* v */
} aliaspoint_t;

typedef struct
{
	aliaspoint_t pt[3];
} tf_triangle;