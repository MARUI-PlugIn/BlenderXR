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

/** \file blender/vr/intern/vr_math.h
*   \ingroup vr
*/

#ifndef __VR_MATH_H__
#define __VR_MATH_H__

/**************************************************************************************************\
|*                                              MACROS                                            *|
\**************************************************************************************************/
#define EIGHTHPI			0.39269908169872415480783042290994	/* Pi/8	(22.5 deg) */
#define QUARTPI				0.78539816339744830961566084581988	/* Pi/4	(45 deg) */
#define HALFPI				1.5707963267948966192313216916398	/* Pi/2	(90 deg) */
#define PI					3.1415926535897932384626433832795	/* Pi	(180 deg) */
#define TWOPI				6.283185307179586476925286766559	/* 2*Pi	(360 deg) */
#define FOURPI				12.566370614359172953850573533118	/* 4*Pi	(720 deg, 2 full turns) */
#define EIGHTPI				25.132741228718345907701147066236	/* 8*Pi	(1440 deg, 4 full turns) */
 
#define DEG_RAD_FACTOR		0.0174532925						/* 1deg = 0.0174532925rad */
#define RAD_DEG_FACTOR		57.2957795							/* 1rad = 57.2957795deg */

#define INCH_MM_FACTOR      25.4                                /* 1 inch = 25.4 mm */
#define MM_INCH_FACTOR      0.0393701                           /* 1 mm = 0.0393701 inch */

#define DEGTORAD(x)			((x)*DEG_RAD_FACTOR)				/* Conversion of degree to radians */
#define RADTODEG(x)			((x)*RAD_DEG_FACTOR)				/* Conversion of radians to degrees */

#define MMTOINCH(x)          ((x)*MM_INCH_FACTOR)				/* Conversion of mm to inch */
#define INCHTOMM(x)          ((x)*INCH_MM_FACTOR)				/* Conversion of mm to inch */

/**************************************************************************************************\
|*                                         DATA STRUCTURES                                        *|
\**************************************************************************************************/
/* Generic quaternion for 3D rotations. */
template <typename T = float> struct Quat {
	T   x;
	T   y;
	T   z;
	T	w;
	Quat() : x(0), y(0), z(0), w(1) {};
	Quat(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {};
	/* "Two vectors" constructor */
	Quat(const Coord3Df& from, const Coord3Df& to) { /* TODO_XR: Use a generic Coord3D */
		Coord3Df v = from ^ to;
		x = v.x; y = v.y; z = v.z;
		w = std::sqrt((from.x*from.x + from.y*from.y + from.z*from.z)
			* (to.x*to.x + to.y*to.y + to.z*to.z))
			+ (from*to);
	};
	/* "Rotation matrix" constructor */
	Quat(const Mat44f& m) { /* TODO_XR: Use a generic Mat44 */
		T trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
		if (trace > 0) {
			T s = (T)0.5 / std::sqrt(trace + (T)1.0);
			x = (m.m[2][1] - m.m[1][2])*s;
			y = (m.m[0][2] - m.m[2][0])*s;
			z = (m.m[1][0] - m.m[0][1])*s;
			w = (T)0.25 / s;
		}
		else {
			if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
				T s = (T)2.0 * std::sqrt((T)1.0 + m.m[0][0] - m.m[1][1] - m.m[2][2]);
				x = (T)0.25 * s;
				y = (m.m[0][1] + m.m[1][0]) / s;
				z = (m.m[0][2] + m.m[2][0]) / s;
				w = (m.m[2][1] - m.m[1][2]) / s;
			}
			else if (m.m[1][1] > m.m[2][2]) {
				T s = (T)2.0 * std::sqrt((T)1.0 + m.m[1][1] - m.m[0][0] - m.m[2][2]);
				x = (m.m[0][1] + m.m[1][0]) / s;
				y = (T)0.25 * s;
				z = (m.m[1][2] + m.m[2][1]) / s;
				w = (m.m[0][2] - m.m[2][0]) / s;
			}
			else {
				T s = (T)2.0 * std::sqrt((T)1.0 + m.m[2][2] - m.m[0][0] - m.m[1][1]);
				x = (m.m[0][2] + m.m[2][0]) / s;
				y = (m.m[1][2] + m.m[2][1]) / s;
				z = (T)0.25 * s;
				w = (m.m[1][0] - m.m[0][1]) / s;
			}
		}
	};
	/* "Axis,angle" constructor */
	Quat(const Coord3Df& axis, const T& angle) { /* TODO_XR: Use a generic Coord3D */
		T s = sin(angle / (T)2);
		T c = cos(angle / (T)2);
		x = axis.x*s;
		y = axis.y*s;
		z = axis.z*s;
		w = c;
	}
	/* "Matrix" function */
	Mat44f to_matrix() { /* TODO_XR: Use a generic Mat44 */
		Mat44f out;
		out.m[0][0] = 1 - 2 * y*y - 2 * z*z;
		out.m[0][1] = 2 * x*y - 2 * z*w;
		out.m[0][2] = 2 * x*z + 2 * y*w;
		out.m[0][3] = 0;
		out.m[1][0] = 2 * x*y + 2 * z*w;
		out.m[1][1] = 1 - 2 * x*x - 2 * z*z;
		out.m[1][2] = 2 * y*z - 2 * x*w;
		out.m[1][3] = 0;
		out.m[2][0] = 2 * x*z - 2 * y*w;
		out.m[2][1] = 2 * y*z + 2 * x*w;
		out.m[2][2] = 1 - 2 * x*x - 2 * y*y;
		out.m[2][3] = 0;
		out.m[3][0] = 0;
		out.m[3][1] = 0;
		out.m[3][2] = 0;
		out.m[3][3] = 1;
		return out;
	};
	/* "Axis,angle" function */
	void to_axis_angle(Coord3Df& axis, T& angle) { /* TODO_XR: Use a generic Coord3D */
		if (w > (T)1) {
			Quat q = this->normalize();
			angle = (T)2 * std::acos(q.w);
			T s = std::sqrt(1 - q.w*q.w);
			if (s == 0) { axis.x = q.x; axis.y = q.y; axis.z = q.z; }
			else { axis.x = q.x / s; axis.y = q.y / s; axis.z = q.z / s; }
			return;
		}
		else {
			angle = (T)2 * std::acos(w);
			T s = std::sqrt(1 - w * w);
			if (s == 0) { axis.x = x; axis.y = y; axis.z = z; }
			else { axis.x = x / s; axis.y = y / s; axis.z = z / s; }
			return;
		}
	};
	/* Conjugate function */
	Quat conjugate() const {
		return Quat(-x, -y, -z, w);
	};
	/* Inverse function */
	Quat inverse() const {
		T d = x * x + y * y + z * z + w * w;
		if (d == 0) return Quat(0, 0, 0, 0);
		return Quat(-x / d, -y / d, -z / d, w / d);
	};
	/* Normalization function */
	Quat normalize() const {
		T d = std::sqrt(x*x + y * y + z * z + w * w);
		if (d == 0) return Quat(0, 0, 0, 0);
		return Quat(x / d, y / d, z / d, w / d);
	};
	/* Quaternion multiplication operator */
	Quat operator*(const Quat& other) const {
		return Quat(x*other.w + y * other.z - z * other.y + w * other.x,
					-x * other.z + y * other.w + z * other.x + w * other.y,
					x*other.y - y * other.x + z * other.w + w * other.z,
					-x * other.x - y * other.y - z * other.z + w * other.w);
	};
	/* Vector multiplication operator (NOTE: This drops the w component of the vector) */
	Coord3Df operator*(const Coord3Df& c) const {  /* TODO_XR: Use a generic Coord3D */
		T n0 = x * (T)2; T n1 = y * (T)2; T n2 = z * (T)2;
		T n3 = x * n0; T n4 = y * n1; T n5 = z * n2;
		T n6 = x * n1; T n7 = x * n2; T n8 = y * n2;
		T n9 = w * n0; T n10 = w * n1; T n11 = w * n2;
		Coord3Df out;
		out.x = ((T)1 - (n4 + n5))*x + (n7 - n11)*y + (n7 + n10)*z;
		out.y = (n6 + n11)*x + ((T)1 - (n3 + n5))*y + (n8 - n9)*z;
		out.z = (n7 - n10)*x + (n8 + n9)*y + ((T)1 - (n3 + n4))*z;
		//out.w = (T)1;
		return out;
	};
};

typedef Quat<float>  Quatf; /* Generic quaternion for 3D rotations (single-precision). */
typedef Quat<double> Quatd; /* Generic quaternion for 3D rotations (double-precision). */

/* VR math utility collection class. */
class VR_Math
{
public:
	static const Mat44f identity_f; /* 4x4 identity matrix (float). */
	static const Mat44d identity_d; /* 4x4 identity matrix (double). */
	
	static void		multiply_mat44_coord3D(Coord3Df& r, const Mat44f& m, const Coord3Df& v);	/* Multiply a Coord3Df by a Mat44f (drops the w component). */
	static float	matrix_distance(const Mat44f& a, const Mat44f& b);	/* Extract the distance between two Mat44f transformations. */
	static float	matrix_rotation(const Mat44f& a, const Mat44f& b, Coord3Df* axis = 0);	/* Get the relative rotation bewteen two transformation matrices. */
	static void		orient_matrix_z(Mat44f& m, Coord3Df z);	/* Rotate transformation matrix to align it with given z-vector. */
};

#endif /* __VR_MATH_H__ */
