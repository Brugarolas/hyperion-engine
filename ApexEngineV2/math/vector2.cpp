#include "vector2.h"

namespace apex {
Vector2::Vector2()
    : x(0.0), y(0.0)
{
}

Vector2::Vector2(float x, float y)
    : x(x), y(y)
{
}

Vector2::Vector2(float xy)
    : x(xy), y(xy)
{
}

Vector2::Vector2(const Vector2 &other)
    : x(other.x), y(other.y)
{
}

Vector2 &Vector2::operator=(const Vector2 &other)
{
    x = other.x;
    y = other.y;
    return *this;
}

Vector2 Vector2::operator+(const Vector2 &other) const
{
    return Vector2(x + other.x, y + other.y);
}

Vector2 &Vector2::operator+=(const Vector2 &other)
{
    x += other.x;
    y += other.y;
    return *this;
}

Vector2 Vector2::operator-(const Vector2 &other) const
{
    return Vector2(x - other.x, y - other.y);
}

Vector2 &Vector2::operator-=(const Vector2 &other)
{
    x -= other.x;
    y -= other.y;
    return *this;
}

Vector2 Vector2::operator*(const Vector2 &other) const
{
    return Vector2(x * other.x, y * other.y);
}

Vector2 &Vector2::operator*=(const Vector2 &other)
{
    x *= other.x;
    y *= other.y;
    return *this;
}

Vector2 Vector2::operator/(const Vector2 &other) const
{
    return Vector2(x / other.x, y / other.y);
}

Vector2 &Vector2::operator/=(const Vector2 &other)
{
    x /= other.x;
    y /= other.y;
    return *this;
}

bool Vector2::operator==(const Vector2 &other) const
{
    return x == other.x && y == other.y;
}

bool Vector2::operator!=(const Vector2 &other) const
{
    return !((*this) == other);
}

float Vector2::Length() const
{
    return sqrt(x * x + y * y);
}

Vector2 &Vector2::Normalize()
{
    float len = Length();
    float len_sqr = len * len;
    if (len_sqr == 0 || len_sqr == 1) {
        return *this;
    }

    (*this) *= (1.0f / len);
    return *this;
}

Vector2 &Vector2::Lerp(const Vector2 &to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    return *this;
}

float Vector2::DistanceSquared(const Vector2 &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    return dx * dx + dy * dy;
}

float Vector2::Distance(const Vector2 &other) const
{
    return sqrt(DistanceSquared(other));
}

Vector2 Vector2::Round(const Vector2 &vec)
{
    return Vector2(std::round(vec.x), std::round(vec.y));
}

Vector2 Vector2::Clamp(const Vector2 &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vector2 Vector2::Min(const Vector2 &a, const Vector2 &b)
{
    return Vector2(MathUtil::Min(a.x, b.x), MathUtil::Min(a.y, b.y));
}

Vector2 Vector2::Max(const Vector2 &a, const Vector2 &b)
{
    return Vector2(MathUtil::Max(a.x, b.x), MathUtil::Max(a.y, b.y));
}

Vector2 Vector2::Zero()
{
    return Vector2(0, 0);
}

Vector2 Vector2::One()
{
    return Vector2(1, 1);
}

Vector2 Vector2::UnitX()
{
    return Vector2(1, 0);
}

Vector2 Vector2::UnitY()
{
    return Vector2(0, 1);
}

std::ostream &operator<<(std::ostream &out, const Vector2 &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}
}