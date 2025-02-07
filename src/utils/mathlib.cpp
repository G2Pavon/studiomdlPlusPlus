// mathlib.c -- math primitives

#include <cmath>

#include "cmdlib.hpp"
#include "mathlib.hpp"

float to_radians(float deg)
{
	return deg * (Q_PI / 180);
}

float to_degrees(float rad)
{
	return rad * (180 / Q_PI);
}

void matrix_copy(const Matrix3x4 &in, Matrix3x4 &out)
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            out[i][j] = in[i][j];
        }
    }
}

Matrix3x4 angle_matrix(const Vector3 &angles)
{
	
/*

	Z-Y-X intrinsic rotation Tait–Bryan angles

			   yaw ψ
			    ⭯ 
				|
				|
				|
				|_________ ⭯ pitch θ
			   /
			  /
			 ⭯ 
			roll φ

 * Rz(ψ) =  [ cos(ψ)  -sin(ψ)  0   ]
 *          [ sin(ψ)   cos(ψ)  0   ]
 *          [    0        0    1   ]
 * 
 * Ry(θ) =  [ cos(θ)   0    sin(θ) ]
 *          [    0     1       0   ]
 *          [ -sin(θ)  0    cos(θ) ]
 * 
 * Rx(φ) =  [ 1     0        0     ]
 *          [ 0   cos(φ)  -sin(φ)  ]
 *          [ 0   sin(φ)   cos(φ)  ]
 * 
 * 
 * Angle matrix =  Rz(ψ) * Ry(θ) * Rx(φ) + Translate
 * 
*/
 	Matrix3x4 m = {};

	float yaw = to_radians(angles.z);
	float pitch = to_radians(angles.y);
	float roll = to_radians(angles.x);

	m[0][0] = cosf(pitch) * cosf(yaw);
	m[1][0] = cosf(pitch) * sinf(yaw);
	m[2][0] = -sinf(pitch);

	m[0][1] = sinf(roll) * sinf(pitch) * cosf(yaw) + cosf(roll) * -sinf(yaw);
	m[1][1] = sinf(roll) * sinf(pitch) * sinf(yaw) + cosf(roll) * cosf(yaw);
	m[2][1] = sinf(roll) * cosf(pitch);

	m[0][2] = cosf(roll) * sinf(pitch) * cosf(yaw) + -sinf(roll) * -sinf(yaw);
	m[1][2] = cosf(roll) * sinf(pitch) * sinf(yaw) + -sinf(roll) * cosf(yaw);
	m[2][2] = cosf(roll) * cosf(pitch);

	m[0][3] = 0.0;
	m[1][3] = 0.0;
	m[2][3] = 0.0;

	return m;
}


Matrix3x4 angle_i_matrix(const Vector3 &angles)
{
/*
 * 
 * Rx(φ) =  [ 1     0        0     ]
 *          [ 0   cos(φ)  -sin(φ)  ]
 *          [ 0   sin(φ)   cos(φ)  ]
 * 
 * Ry(θ) =  [ cos(θ)   0    sin(θ) ]
 *          [    0     1       0   ]
 *          [ -sin(θ)  0    cos(θ) ]
 * 
 * Rz(ψ) =  [ cos(ψ)  -sin(ψ)  0   ]
 *          [ sin(ψ)   cos(ψ)  0   ]
 *          [    0        0    1   ]
 * 
 * Angle inverse matrix =  Rx(φ) * Ry(θ) * Rz(ψ) + Translate
 * 
*/
 	Matrix3x4 m = {};

	float yaw = to_radians(angles.z);
	float pitch = to_radians(angles.y);
	float roll = to_radians(angles.x);

	m[0][0] = cosf(pitch) * cosf(yaw);
	m[1][0] = sinf(roll) * sinf(pitch) * cosf(yaw) + cosf(roll) * -sinf(yaw);
	m[2][0] = cosf(roll) * sinf(pitch) * cosf(yaw) + -sinf(roll) * -sinf(yaw);

	m[0][1] = cosf(pitch) * sinf(yaw);
	m[1][1] = sinf(roll) * sinf(pitch) * sinf(yaw) + cosf(roll) * cosf(yaw);
	m[2][1] = cosf(roll) * sinf(pitch) * sinf(yaw) + -sinf(roll) * cosf(yaw);

	m[0][2] = -sinf(pitch);
	m[1][2] = sinf(roll) * cosf(pitch);
	m[2][2] = cosf(roll) * cosf(pitch);

	m[0][3] = 0.0;
	m[1][3] = 0.0;
	m[2][3] = 0.0;

	return m;
}

/*
 * Transformation Matrix:
 * A = [ r00  r01  r02 | tax ]        B = [ r00  r01  r02 | tbx ] 
 *     [ r10  r11  r12 | tay ]            [ r10  r11  r12 | tby ]
 *     [ r20  r21  r22 | taz ]            [ r20  r21  r22 | tbz ]
 * 
 * To combine these transformations, we perform the following operations:
 * 
 * 1. Multiply the rotation matrices (3x3):
 *    C_r = A_r * B_r
 *
 * 2. Multiply the rotation part of A with the translation part of B and add the translation part of A:
 *    C_t = A_r * B_t + A_t
 * 
 * The resulting matrix C is a combination of the rotation C_r and translation C_t
 * 
 * C = [ r00  r01  r02 | tcx ]
 *     [ r10  r11  r12 | tcy ] 
 *     [ r20  r21  r22 | tcz ] 
 * 
 */
Matrix3x4 concat_transforms(const Matrix3x4 &A, const Matrix3x4 &B)
{
	Matrix3x4 m = {};

	m[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0] + A[0][2] * B[2][0];
	m[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1] + A[0][2] * B[2][1];
	m[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2] * B[2][2];

	m[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0] + A[1][2] * B[2][0];
	m[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1] + A[1][2] * B[2][1];
	m[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2] * B[2][2];

	m[2][0] = A[2][0] * B[0][0] + A[2][1] * B[1][0] + A[2][2] * B[2][0];
	m[2][1] = A[2][0] * B[0][1] + A[2][1] * B[1][1] + A[2][2] * B[2][1];
	m[2][2] = A[2][0] * B[0][2] + A[2][1] * B[1][2] + A[2][2] * B[2][2];

	m[0][3] = A[0][0] * B[0][3] + A[0][1] * B[1][3] + A[0][2] * B[2][3] + A[0][3];
	m[1][3] = A[1][0] * B[0][3] + A[1][1] * B[1][3] + A[1][2] * B[2][3] + A[1][3];
	m[2][3] = A[2][0] * B[0][3] + A[2][1] * B[1][3] + A[2][2] * B[2][3] + A[2][3];

	return m;
}


Vector3 vector_transform(const Vector3 &in1, const Matrix3x4 &in2)
{
	
/*
 * Applies a 3x4 transformation matrix to a 3D vector
 * The transformation involves both rotation and translation
 * 
 *    out = R * in1 + T
 * 
 * where `R` is the rotation matrix, `in1` is the input vector, and `T` is the translation vector.
 * 
 *    in2 =  [ r00  r01  r02 | tx ]
 *           [ r10  r11  r12 | ty ]
 *           [ r20  r21  r22 | tz ]
 * 
 * 1. Rotation: `in1` is rotated by the 3x3 rotation part of the matrix `in2`:
 * 
 *    Rin2 = [ r00  r01  r02 ]			in1 = [ x1 ]
 *           [ r10  r11  r12 ]			      [ x2 ]
 *           [ r20  r21  r22 ]			      [ x3 ]
 * 	
 * 		in1' = Rin2*in1		
 * 
 * 2. Translation: T is added to the rotated vector:
 * 
 *    outx = x' + tx
 *    outy = y' + ty
 *    outz = z' + tz
 * 
 */
	Vector3 out;

	Vector3 row0(in2[0][0], in2[0][1], in2[0][2]);
	Vector3 row1(in2[1][0], in2[1][1], in2[1][2]);
	Vector3 row2(in2[2][0], in2[2][1], in2[2][2]);

	out.x = in1.dot(row0) + in2[0][3];
	out.y = in1.dot(row1) + in2[1][3];
	out.z = in1.dot(row2) + in2[2][3];

	return out;
}
