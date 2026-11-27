module;

#include <algorithm>
#include <cmath>

export module vecmath;

export struct alignas(16) Vec3 {
    float x, y, z;

    bool operator==(const Vec3 &other) const = default;

    float operator[](std::size_t i) const
    {
        switch (i) {
        case 0:
            return x;
        case 1:
            return y;
        case 2:
        default:
            return z;
        }
    };
};
static_assert(sizeof(Vec3) == 4 * sizeof(float));

export Vec3 crossProduct(const Vec3 &a, const Vec3 &b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

export Vec3 operator+(const Vec3 &a, const Vec3 &b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

export Vec3 operator-(const Vec3 &a, const Vec3 &b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

export Vec3 operator*(float s, const Vec3 &v)
{
    return { v.x * s, v.y * s, v.z * s };
}

export Vec3 operator*(const Vec3 &v, float s)
{
    return s * v;
}

export float dot(const Vec3 &a, const Vec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

export float length(const Vec3 &v)
{
    return std::sqrt(dot(v, v));
}

export Vec3 normalize(const Vec3 &v)
{
    const auto l = length(v);
    return { v.x / l, v.y / l, v.z / l };
}

export Vec3 min(const Vec3 &a, const Vec3 &b)
{
    return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

export Vec3 max(const Vec3 &a, const Vec3 &b)
{
    return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}
