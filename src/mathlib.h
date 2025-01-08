#ifndef __MATHLIB__
#define __MATHLIB__

// mathlib.h

#include <math.h>

typedef float vec3_t[3]; // x,y,z
typedef float vec4_t[4]; // x,y,z,w

#define SIDE_FRONT 0
#define SIDE_ON 2
#define SIDE_BACK 1
#define SIDE_CROSS -2

#define Q_PI 3.14159265358979323846

extern vec3_t vec3_origin;

// Use this definition globally
#define ON_EPSILON 0.01
#define EQUAL_EPSILON 0.001

int VectorCompare(const vec3_t v1, const vec3_t v2);

#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorFill(a, b) \
    {                    \
        (a)[0] = (b);    \
        (a)[1] = (b);    \
        (a)[2] = (b);    \
    }
#define VectorAvg(a) (((a)[0] + (a)[1] + (a)[2]) / 3)
#define VectorSubtract(a, b, c)   \
    {                             \
        (c)[0] = (a)[0] - (b)[0]; \
        (c)[1] = (a)[1] - (b)[1]; \
        (c)[2] = (a)[2] - (b)[2]; \
    }
#define VectorAdd(a, b, c)        \
    {                             \
        (c)[0] = (a)[0] + (b)[0]; \
        (c)[1] = (a)[1] + (b)[1]; \
        (c)[2] = (a)[2] + (b)[2]; \
    }
#define VectorCopy(a, b) \
    {                    \
        (b)[0] = (a)[0]; \
        (b)[1] = (a)[1]; \
        (b)[2] = (a)[2]; \
    }
#define VectorScale(a, b, c)   \
    {                          \
        (c)[0] = (b) * (a)[0]; \
        (c)[1] = (b) * (a)[1]; \
        (c)[2] = (b) * (a)[2]; \
    }

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
float VectorNormalize(vec3_t v);

void AngleMatrix(const vec3_t angles, float matrix[3][4]);
void AngleIMatrix(const vec3_t angles, float matrix[3][4]);
void R_ConcatTransforms(const float in1[3][4], const float in2[3][4], float out[3][4]);

void VectorTransform(const vec3_t in1, const float in2[3][4], vec3_t out);

#endif