// mathlib.c -- math primitives

#include <cmath>
#include <numbers>

constexpr float Q_PI = std::numbers::pi_v<float>;
constexpr float EPSILON = 0.001f;

struct Vector3
{
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float new_x, float new_y, float new_z)
    {
        x = new_x;
        y = new_y;
        z = new_z;
    }

    float &operator[](int i)
    {
        if (i == 0)
            return x;
        else if (i == 1)
            return y;
        else
            return z;
    }

    const float &operator[](int i) const
    {
        if (i == 0)
            return x;
        else if (i == 1)
            return y;
        else
            return z;
    }

    Vector3 operator+(const Vector3 &other) const
    {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator-(const Vector3 &other) const
    {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator*(float scalar) const
    {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    Vector3 operator/(float scalar) const
    {
        if (scalar == 0)
        {
            return Vector3();
        }
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    Vector3 &operator+=(const Vector3 &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vector3 &operator-=(const Vector3 &other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vector3 &operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    Vector3 &operator/=(float scalar)
    {
        if (scalar == 0)
        {
            return *this;
        }
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    bool operator==(const Vector3 &other) const
    {
        return fabs(x - other.x) <= EPSILON && fabs(y - other.y) <= EPSILON && fabs(z - other.z) <= EPSILON;
    }

    bool operator!=(const Vector3 &other) const
    {
        return !(*this == other);
    }

    float magnitude() const
    {
        return std::sqrtf(x * x + y * y + z * z);
    }

    void normalize()
    {
        float len = magnitude();
        if (len != 0)
        {
            *this /= len;
        }
    }

    float dot(const Vector3 &other) const
    {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3 cross(const Vector3 &other) const
    {
        return Vector3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
    }
};

float to_radians(float deg);
float to_degrees(float rad);

void matrix_copy(float in[3][4], float out[3][4]);

void angle_matrix(const Vector3 &angles, float matrix[3][4]);
void angle_i_matrix(const Vector3 &angles, float matrix[3][4]);

void r_concat_transforms(const float in1[3][4], const float in2[3][4], float out[3][4]);
void vector_transform(const Vector3 &in1, const float in2[3][4], Vector3 &out);