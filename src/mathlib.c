// mathlib.c -- math primitives

#include "cmdlib.h"
#include "mathlib.h"

vec3_t vec3_origin = {0, 0, 0};

int VectorCompare(const vec3_t v1, const vec3_t v2)
{
	int i;

	for (i = 0; i < 3; i++)
		if (fabs(v1[i] - v2[i]) > EQUAL_EPSILON)
			return false;

	return true;
}

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

float VectorNormalize(vec3_t v)
{
	int i;
	double length;

	length = 0;
	for (i = 0; i < 3; i++)
		length += v[i] * v[i];
	length = sqrt(length);
	if (length == 0)
		return 0;

	for (i = 0; i < 3; i++)
		v[i] /= length;

	return length;
}

void AngleMatrix(const vec3_t angles, float (*matrix)[4])
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = angles[2] * (Q_PI * 2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[1] * (Q_PI * 2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[0] * (Q_PI * 2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (Z * Y) * X
	matrix[0][0] = cp * cy;
	matrix[1][0] = cp * sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr * sp * cy + cr * -sy;
	matrix[1][1] = sr * sp * sy + cr * cy;
	matrix[2][1] = sr * cp;
	matrix[0][2] = (cr * sp * cy + -sr * -sy);
	matrix[1][2] = (cr * sp * sy + -sr * cy);
	matrix[2][2] = cr * cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

void AngleIMatrix(const vec3_t angles, float matrix[3][4])
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = angles[2] * (Q_PI * 2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[1] * (Q_PI * 2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[0] * (Q_PI * 2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (Z * Y) * X
	matrix[0][0] = cp * cy;
	matrix[0][1] = cp * sy;
	matrix[0][2] = -sp;
	matrix[1][0] = sr * sp * cy + cr * -sy;
	matrix[1][1] = sr * sp * sy + cr * cy;
	matrix[1][2] = sr * cp;
	matrix[2][0] = (cr * sp * cy + -sr * -sy);
	matrix[2][1] = (cr * sp * sy + -sr * cy);
	matrix[2][2] = cr * cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

void R_ConcatTransforms(const float in1[3][4], const float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}

void VectorTransform(const vec3_t in1, const float in2[3][4], vec3_t out)
{
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}