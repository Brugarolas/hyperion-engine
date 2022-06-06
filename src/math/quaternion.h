#ifndef QUATERNION_H
#define QUATERNION_H

#include "math_util.h"
#include "vector3.h"
#include "matrix4.h"

#include <cmath>
using std::abs;

namespace hyperion {
class Quaternion {
    friend std::ostream &operator<<(std::ostream &out, const Quaternion &rot);
public:
    float x, y, z, w;

    Quaternion();
    Quaternion(float x, float y, float z, float w);
    Quaternion(const Matrix4 &mat);
    Quaternion(const Vector3 &euler);
    Quaternion(const Vector3 &axis, float radians);
    Quaternion(const Quaternion &other);

    float GetX() const { return x; }
    float &GetX()      { return x; }
    void SetX(float x) { this->x = x; }
    float GetY() const { return y; }
    float &GetY()      { return y; }
    void SetY(float y) { this->y = y; }
    float GetZ() const { return z; }
    float &GetZ()      { return z; }
    void SetZ(float z) { this->z = z; }
    float GetW() const { return w; }
    float &GetW()      { return w; }
    void SetW(float w) { this->w = w; }

    Quaternion &operator=(const Quaternion &other);
    Quaternion operator*(const Quaternion &other) const;
    Quaternion &operator*=(const Quaternion &other);
    Quaternion &operator+=(const Vector3 &vec);
    Vector3 operator*(const Vector3 &vec) const;

    float Length() const;
    float LengthSquared() const;
    Quaternion &Normalize();
    Quaternion &Invert();
    Quaternion &Slerp(const Quaternion &to, float amt);

    int GimbalPole() const;
    float Roll() const;
    float Pitch() const;
    float Yaw() const;

    static Quaternion Identity();
};
} // namespace hyperion

#endif
