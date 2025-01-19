// mathlib.c -- math primitives

#include <cmath>

constexpr double Q_PI = 3.14159265358979323846;
constexpr float EPSILON = 0.001;

struct vec3_t
{
    float x, y, z;

    vec3_t() : x(0), y(0), z(0) {}
    vec3_t(float x, float y, float z) : x(x), y(y), z(z) {}

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

    vec3_t operator+(const vec3_t &other) const
    {
        return vec3_t(x + other.x, y + other.y, z + other.z);
    }

    vec3_t operator-(const vec3_t &other) const
    {
        return vec3_t(x - other.x, y - other.y, z - other.z);
    }

    vec3_t operator*(float scalar) const
    {
        return vec3_t(x * scalar, y * scalar, z * scalar);
    }

    vec3_t operator/(float scalar) const
    {
        if (scalar == 0)
        {
            return vec3_t();
        }
        return vec3_t(x / scalar, y / scalar, z / scalar);
    }

    vec3_t &operator+=(const vec3_t &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    vec3_t &operator-=(const vec3_t &other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    vec3_t &operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    vec3_t &operator/=(float scalar)
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

    bool operator==(const vec3_t &other) const
    {
        return fabs(x - other.x) <= EPSILON && fabs(y - other.y) <= EPSILON && fabs(z - other.z) <= EPSILON;
    }

    bool operator!=(const vec3_t &other) const
    {
        return !(*this == other);
    }

    float magnitude() const
    {
        return sqrt(x * x + y * y + z * z);
    }

    void normalize()
    {
        float len = magnitude();
        if (len != 0)
        {
            *this /= len;
        }
    }

    float dot(const vec3_t &other) const
    {
        return x * other.x + y * other.y + z * other.z;
    }

    vec3_t cross(const vec3_t &other) const
    {
        return vec3_t(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
    }
};

void MatrixCopy(float in[3][4], float out[3][4]);

void AngleMatrix(const vec3_t &angles, float matrix[3][4]);
void AngleIMatrix(const vec3_t &angles, float matrix[3][4]);

void R_ConcatTransforms(const float in1[3][4], const float in2[3][4], float out[3][4]);
void VectorTransform(const vec3_t &in1, const float in2[3][4], vec3_t &out);