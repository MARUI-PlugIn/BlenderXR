/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2018 by Blender Foundation.
* All rights reserved.
*
* Contributor(s): MARUI-PlugIn
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/vr_types.h
*   \ingroup vr
*/

#ifndef __VR_TYPES_H__
#define __VR_TYPES_H__

/**************************************************************************************************\
|*                                        PLATFORM DEFINITIONS                                    *|
\**************************************************************************************************/
#ifdef WIN32
#include <string>	/* required for memcpy */
#else
#include <cmath>	/* required for sqrt, acos */
#include <cstring>	/* required for memcpy */
#endif

/**************************************************************************************************\
|*                                        GLOBAL DEFINITIONS                                      *|
\**************************************************************************************************/
typedef unsigned char		uchar;	/* unsigned 8bit integer (byte). */
typedef unsigned short		ushort;	/* unsigned 16bit (short) integer. */
typedef unsigned int		uint;	/* unsigned 32bit integer. */
typedef unsigned long long	ui64;	/* unsigned 64bit integer. */

/* Global enum for 3 dimension-axis, OR-able, including a "null"-direction. */
typedef enum VR_Axis {
	VR_AXIS_NONE	= 0x0000	/* No axis / direction / null. Binary code: 0000. */
	,
	VR_AXIS_X		= 0x0001	/* X-axis. Binary code: 0001. */
	,
	VR_AXIS_Y		= 0x0002	/* Y-axis. Binary code: 0010. */
	,
	VR_AXIS_Z		= 0x0004	/* Z-axis. Binary code: 0100. */
	,
	VR_AXES_XY		= (VR_AXIS_X| VR_AXIS_Y)	/* X and Y axis. Binary code: 0011. */
	,
	VR_AXES_XZ		= (VR_AXIS_X | VR_AXIS_Z)	/* X and Z axis. Binary code: 0101. */
	,
	VR_AXES_YZ		= (VR_AXIS_Y | VR_AXIS_Z)	/* Y and Z axis. Binary code: 0110. */
	,
	VR_AXES_XYZ		= (VR_AXIS_X | VR_AXIS_Y | VR_AXIS_Z)	/* X, Y, and Z axis. Binary code: 0111. */
} VR_Axis;

inline VR_Axis& operator|=(VR_Axis& a0, const VR_Axis& a1)
{
	return a0 = VR_Axis(a0 | a1);
};
inline VR_Axis operator|(const VR_Axis& a0, const VR_Axis& a1)
{
	return VR_Axis(int(a0) | int(a1));
};

/* Global enum for directions in up to 3 dimensions, OR-able, including a "null"-direction. */
typedef enum VR_Direction {
	VR_DIRECTION_NONE	= 0x00	/* No border / side / null. */
	,
	VR_DIRECTION_LEFT	= 0x01	/* Towards the left side. */
	,
	VR_DIRECTION_RIGHT	= 0x02   /* Towards the right side. */
	,
	VR_DIRECTION_UP		= 0x04	/* Towards the top side. */
	,
	VR_DIRECTION_DOWN	= 0x08  /* Towards the bottom side. */
	,
	VR_DIRECTION_FRONT	= 0x10	/* Towards the front side. */
	,
	VR_DIRECTION_BACK	= 0x20	/* Towards the back side. */
} VR_Direction;

/* Text/object alignment horizontally. */
typedef enum VR_HAlign {
	VR_HALIGN_LEFT		/* Align text left. */
	,
	VR_HALIGN_CENTER	/* Align text centered. */
	,
	VR_HALIGN_RIGHT		/* Align text right. */
} VR_HAlign;

 /* Text/object alignment vertically. */
typedef enum VR_VAlign {
	VR_VALIGN_TOP		/* Align text left. */
	,
	VR_VALIGN_CENTER	/* Align text centered. */
	,
	VR_VALIGN_BOTTOM	/* Align text right. */
} VR_VAlign;

/* Generic 2D coordinates. */
template <typename T = float> struct Coord2D {
	T	x;	/* x-coordinate */
	T   y;  /* y-coordinate */
	Coord2D() : x(0), y(0) {};
	Coord2D(T x, T y) : x(x), y(y) {};
	/* Length (magnitude) function */
	T length() const {
		return (T)std::sqrt(x*x + y * y);
	};
	/* Normalization function */
	Coord2D normalize() const {
		T len = length();
		if (len == 0) return Coord2D(0, 0);
		return Coord2D(x / len, y / len);
	};
	/* Normalization function (in-place) */
	Coord2D& normalize_in_place() {
		T len = length();
		if (len == 0) return *this;
		x /= len;
		y /= len;
		return *this;
	};
	/* Angle function */
	T angle(const Coord2D& other) const {
		T n = this->normalize() * other.normalize();
		n = (n < (T)-1.0 ? (T)-1.0 : (n > (T)1.0 ? (T)1.0 : n));
		return std::acos(n);
	};
	/* Addition operator */
	Coord2D operator+(const Coord2D& other) const {
		return Coord2D(x + other.x, y + other.y);
	};
	/* Subtraction operator */
	Coord2D operator-(const Coord2D& other) const {
		return Coord2D(x - other.x, y - other.y);
	};
	/* Multiplication operator */
	Coord2D operator*(const T& c) const {
		return Coord2D(x*c, y*c);
	};
	/* Multiplication (dot product) function */
	T operator*(const Coord2D& other) const {
		return x * other.x + y * other.y;
	};
	/* Division operator */
	Coord2D operator/(const T& c) const {
		return Coord2D(x / c, y / c);
	};
	/* Addition operator */
	Coord2D& operator+=(const Coord2D& other) {
		this->x += other.x;
		this->y += other.y;
		return *this;
	};
	/* Subtraction operator */
	Coord2D& operator-=(const Coord2D& other) {
		this->x -= other.x;
		this->y -= other.y;
		return *this;
	};
	/* Multiplication operator */
	Coord2D& operator*=(const T& c) {
		x *= c;
		y *= c;
		return *this;
	};
	/* Division operator */
	Coord2D& operator/=(const T& c) {
		x /= c;
		y /= c;
		return *this;
	};
};

typedef Coord2D<float>  Coord2Df;	/* Generic 2D single-precision coordinates. */
typedef Coord2D<double> Coord2Dd;	/* Generic 2D double-precision coordinates. */

/* Generic 3D coordinates. */
template <typename T = float> struct Coord3D {
	T   x;  /* x-coordinate */
	T   y;	/* y-coordinate */
	T   z;  /* z-coordinate */
	Coord3D() : x(0), y(0), z(0) {};
	Coord3D(T x, T y, T z) : x(x), y(y), z(z) {};
	/* Length (magnitude) function */
	T length() const {
		return (T)std::sqrt(x*x + y * y + z * z);
	};
	/* Normalization function */
	Coord3D normalize() const {
		T len = length();
		if (len == 0) return Coord3D(0, 0, 0);
		return Coord3D(x / len, y / len, z / len);
	};
	/* Normalization function (in-place) */
	Coord3D& normalize_in_place() {
		T len = length();
		if (len == 0) return *this;
		x /= len;
		y /= len;
		z /= len;
		return *this;
	};
	/* Angle function */
	T angle(const Coord3D& other) const {
		T n = this->normalize() * other.normalize();
		n = (n < (T)-1.0 ? (T)-1.0 : (n > (T)1.0 ? (T)1.0 : n));
		return std::acos(n);
	};
	/* Addition operator */
	Coord3D operator+(const Coord3D& other) const {
		return Coord3D(x + other.x, y + other.y, z + other.z);
	};
	/* Subtraction operator */
	Coord3D operator-(const Coord3D& other) const {
		return Coord3D(x - other.x, y - other.y, z - other.z);
	};
	/* Multiplication operator */
	Coord3D operator*(const T& c) const {
		return Coord3D(x*c, y*c, z*c);
	};
	/* Multiplication (dot product) function */
	T operator*(const Coord3D& other) const {
		return x * other.x + y * other.y + z * other.z;
	};
	/* Division operator */
	Coord3D operator/(const T& c) const {
		return Coord3D(x / c, y / c, z / c);
	};
	/* Cross product operator */
	Coord3D operator^(const Coord3D& other) const {
		return Coord3D(y*other.z - z * other.y,
			z*other.x - x * other.z,
			x*other.y - y * other.x);
	};
	/* Equals operator */
	int operator==(const Coord3D& other) {
		if (this->x == other.x && this->y == other.y && this->z == other.z) {
			return 1;
		}
		else {
			return 0;
		}
	};
	/* Addition operator */
	Coord3D& operator+=(const Coord3D& other) {
		this->x += other.x;
		this->y += other.y;
		this->z += other.z;
		return *this;
	};
	/* Subtraction operator */
	Coord3D& operator-=(const Coord3D& other) {
		this->x -= other.x;
		this->y -= other.y;
		this->z -= other.z;
		return *this;
	};
	/* Multiplication operator */
	Coord3D& operator*=(const T& c) {
		x *= c;
		y *= c;
		z *= c;
		return *this;
	};
	/* Division operator */
	Coord3D& operator/=(const T& c) {
		x /= c;
		y /= c;
		z /= c;
		return *this;
	};
};

typedef Coord3D<float>  Coord3Df;	/* Generic 3D single-precision coordinates. */
typedef Coord3D<double> Coord3Dd;	/* Generic 3D double-precision coordinates. */

/* Defined in vr_math.cpp */
extern bool mat44_inverse(float inv[4][4], const float m[4][4]);		/* Fast matrix inversion using Eigen. */
extern bool mat44_inverse(double inv[4][4], const double m[4][4]);	/* Fast matrix inversion using Eigen. */
extern void mat44_multiply(float R[4][4], const float A[4][4], const float B[4][4]);		/* Fast matrix multiplication using SSE2 operations. */
extern void mat44_multiply(double R[4][4], const double A[4][4], const double B[4][4]);	/* Fast matrix multiplication using SSE2 operations. */

/* Generic 4x4 3D transformation matrix. */
template <typename T = float> struct Mat44 {
	T	m[4][4];	/* Actual matrix data. */
	Mat44() {};
	Mat44(const T arr[4][4]) { std::memcpy(m, arr, sizeof(T) * 4 * 4); };
	/* "Identity" function */
	Mat44& set_to_identity() {
		m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1;
		m[0][1] = m[0][2] = m[0][3] = m[1][0] = m[1][2] = m[1][3] = m[2][0] = m[2][1] = m[2][3] = m[3][0] = m[3][1] = m[3][2] = 0;
		return *this;
	};
	/* Determinant function */
	T det() const {
		return m[0][0] * m[1][1] * m[2][2] * m[3][3] + m[0][0] * m[1][2] * m[2][3] * m[3][1] + m[0][0] * m[1][3] * m[2][1] * m[3][2]
			 + m[0][1] * m[1][0] * m[2][3] * m[3][2] + m[0][1] * m[1][2] * m[2][0] * m[3][3] + m[0][1] * m[1][3] * m[2][2] * m[3][0]
			 + m[0][2] * m[1][0] * m[2][1] * m[3][3] + m[0][2] * m[1][1] * m[2][3] * m[3][0] + m[0][2] * m[1][3] * m[2][0] * m[3][1]
			 + m[0][3] * m[1][0] * m[2][2] * m[3][1] + m[0][3] * m[1][1] * m[2][0] * m[3][2] + m[0][3] * m[1][2] * m[2][1] * m[3][0]
			 - m[0][0] * m[1][1] * m[2][3] * m[3][2] - m[0][0] * m[1][2] * m[2][1] * m[3][3] - m[0][0] * m[1][3] * m[2][2] * m[3][1]
			 - m[0][1] * m[1][0] * m[2][2] * m[3][3] - m[0][1] * m[1][2] * m[2][3] * m[3][0] - m[0][1] * m[1][3] * m[2][0] * m[3][2]
			 - m[0][2] * m[1][0] * m[2][3] * m[3][1] - m[0][2] * m[1][1] * m[2][0] * m[3][3] - m[0][2] * m[1][3] * m[2][1] * m[3][0]
			 - m[0][3] * m[1][0] * m[2][1] * m[3][2] - m[0][3] * m[1][1] * m[2][2] * m[3][0] - m[0][3] * m[1][2] * m[2][0] * m[3][1];
	};
	/* Inverse function */
	Mat44 inverse() const {
		Mat44 out;
		mat44_inverse(out.m, m);
		return out;
	};
	/* Copy operator */
	Mat44& operator=(const Mat44 &other) {
		std::memcpy(m, other.m, sizeof(T) * 4 * 4);
		return *this;
	};
	/* Copy operator */
	Mat44& operator=(const T(other)[4][4]) {
		std::memcpy(m, other, sizeof(T) * 4 * 4);
		return *this;
	};
	/* Multiplication operator */
	Mat44 operator*(const Mat44& other) const {
		Mat44 out;
		mat44_multiply(out.m, other.m, m);
		return out;
	};
	/* Multiplication operator */
	Mat44 operator*(const T(&other)[4][4]) const {
		Mat44 out;
		mat44_multiply(out.m, other.m, m);
		return out;
	};
};

typedef Mat44<float>  Mat44f;	/* Generic 4x4 3D transformation matrix (single-precision). */
typedef Mat44<double> Mat44d;	/* Generic 4x4 3D transformation matrix (double-precision). */

/**************************************************************************************************\
|*                                              MACROS                                            *|
\**************************************************************************************************/
#define STRING(X)	#X	/* Multi-line string. */

/**************************************************************************************************\
|*                                         GLOBAL VARIABLES										  *|
\**************************************************************************************************/
extern ui64	VR_t_now;	/* Current (most recent) timestamp. This will be updated (1) when updating tracking (2) when starting rendering a new frame (3) before executing UI operations. */

#endif /* __VR_TYPES_H__ */
