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
* The Original Code is Copyright (C) 2016 by Mike Erwin.
* All rights reserved.
*
* Contributor(s): Blender Foundation
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_math.cpp
*   \ingroup vr
*
* Collection of VR math-related utility functions.
*/

#include "vr_types.h"

#include "vr_math.h"

#include <Eigen/Dense>

/* Externed in vr_types.h */
bool mat44_inverse(float inv[4][4], const float m[4][4])
{
	Eigen::Map<Eigen::Matrix4f> mat = Eigen::Map<Eigen::Matrix4f>((float*)m);
	Eigen::Matrix4f out;
	bool invertible = true;
	mat.computeInverseWithCheck(out, invertible, 0.0f);
	if (!invertible) {
		out = out.Zero();
	}
	memcpy(inv, out.data(), sizeof(float) * 4 * 4);
	return invertible;
}

/* Externed in vr_types.h */
bool mat44_inverse(double inv[4][4], const double m[4][4])
{
	Eigen::Map<Eigen::Matrix4d> mat = Eigen::Map<Eigen::Matrix4d>((double*)m);
	Eigen::Matrix4d out;
	bool invertible = true;
	mat.computeInverseWithCheck(out, invertible, 0.0f);
	if (!invertible) {
		out = out.Zero();
	}
	memcpy(inv, out.data(), sizeof(double) * 4 * 4);
	return invertible;
}

static void mat44_multiply_unique(float R[4][4], const float A[4][4], const float B[4][4])
{
	/* matrix product: R[j][k] = A[j][i] . B[i][k] */
	__m128 A0 = _mm_loadu_ps(A[0]);
	__m128 A1 = _mm_loadu_ps(A[1]);
	__m128 A2 = _mm_loadu_ps(A[2]);
	__m128 A3 = _mm_loadu_ps(A[3]);

	for (int i = 0; i < 4; i++) {
		__m128 B0 = _mm_set1_ps(B[i][0]);
		__m128 B1 = _mm_set1_ps(B[i][1]);
		__m128 B2 = _mm_set1_ps(B[i][2]);
		__m128 B3 = _mm_set1_ps(B[i][3]);

		__m128 sum = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(B0, A0), _mm_mul_ps(B1, A1)),
			_mm_add_ps(_mm_mul_ps(B2, A2), _mm_mul_ps(B3, A3)));

		_mm_storeu_ps(R[i], sum);
	}
}
static void mat44_pre_multiply(float R[4][4], const float A[4][4])
{
	float B[4][4];
	std::memcpy(B, R, sizeof(float) * 4 * 4);
	mat44_multiply_unique(R, A, B);
}
static void mat44_post_multiply(float R[4][4], const float B[4][4])
{
	float A[4][4];
	std::memcpy(A, R, sizeof(float) * 4 * 4);
	mat44_multiply_unique(R, A, B);
}
/* Externed in vr_types.h */
void mat44_multiply(float R[4][4], const float A[4][4], const float B[4][4])
{
	if (A == R) {
		mat44_post_multiply(R, B);
	}
	else if (B == R) {
		mat44_pre_multiply(R, A);
	}
	else {
		mat44_multiply_unique(R, A, B);
	}
}

static void mat44_multiply_unique(double R[4][4], const double A[4][4], const double B[4][4])
{
	/* matrix product: R[j][k] = A[j][i] . B[i][k] */
	__m128d A0 = _mm_loadu_pd(A[0]);
	__m128d A1 = _mm_loadu_pd(A[1]);
	__m128d A2 = _mm_loadu_pd(A[2]);
	__m128d A3 = _mm_loadu_pd(A[3]);

	for (int i = 0; i < 4; i++) {
		__m128d B0 = _mm_set1_pd(B[i][0]);
		__m128d B1 = _mm_set1_pd(B[i][1]);
		__m128d B2 = _mm_set1_pd(B[i][2]);
		__m128d B3 = _mm_set1_pd(B[i][3]);

		__m128d sum = _mm_add_pd(
			_mm_add_pd(_mm_mul_pd(B0, A0), _mm_mul_pd(B1, A1)),
			_mm_add_pd(_mm_mul_pd(B2, A2), _mm_mul_pd(B3, A3)));

		_mm_storeu_pd(R[i], sum);
	}
}
static void mat44_pre_multiply(double R[4][4], const double A[4][4])
{
	double B[4][4];
	std::memcpy(B, R, sizeof(double) * 4 * 4);
	mat44_multiply_unique(R, A, B);
}
static void mat44_post_multiply(double R[4][4], const double B[4][4])
{
	double A[4][4];
	std::memcpy(A, R, sizeof(double) * 4 * 4);
	mat44_multiply_unique(R, A, B);
}
/* Externed in vr_types.h */
void mat44_multiply(double R[4][4], const double A[4][4], const double B[4][4])
{
	if (A == R) {
		mat44_post_multiply(R, B);
	}
	else if (B == R) {
		mat44_pre_multiply(R, A);
	}
	else {
		mat44_multiply_unique(R, A, B);
	}
}

float ident_f[4][4] = { 1.0f, 0.0f, 0.0f, 0.0f,
						0.0f, 1.0f, 0.0f, 0.0f,
						0.0f, 0.0f, 1.0f, 0.0f,
						0.0f, 0.0f, 0.0f, 1.0f };
const Mat44f VR_Math::identity_f = ident_f;

double ident_d[4][4] = { 1.0, 0.0, 0.0, 0.0,
						 0.0, 1.0, 0.0, 0.0,
						 0.0, 0.0, 1.0, 0.0,
						 0.0, 0.0, 0.0, 1.0 };
const Mat44d VR_Math::identity_d = ident_d;

void VR_Math::multiply_mat44_coord3D(Coord3Df& r, const Mat44f& m, const Coord3Df& v)
{
	const float& x = v.x;
	const float& y = v.y;
	const float& z = v.z;

	r.x = x * m.m[0][0] + y * m.m[1][0] + z * m.m[2][0] + m.m[3][0];
	r.y = x * m.m[0][1] + y * m.m[1][1] + z * m.m[2][1] + m.m[3][1];
	r.z = x * m.m[0][2] + y * m.m[1][2] + z * m.m[2][2] + m.m[3][2];
}

float VR_Math::matrix_distance(const Mat44f& a, const Mat44f& b)
{
	float dx = a.m[3][0] - b.m[3][0];
	float dy = a.m[3][1] - b.m[3][1];
	float dz = a.m[3][2] - b.m[3][2];
	return sqrt(dx*dx + dy*dy + dz*dz);
}

float VR_Math::matrix_rotation(const Mat44f& a, const Mat44f& b, Coord3Df* axis)
{
	Coord3Df _axis;
	float angle = 0.0f;
	Quatf a_rotation(a);
	Quatf b_rotation(b);
	(a_rotation.inverse()*b_rotation).to_axis_angle(_axis, angle);
	angle = (float)RADTODEG(angle);
	float counter_angle = 360.0f - angle; /* rotation in opposite direction */
	if (angle > counter_angle) {
		angle = counter_angle;
	}
	if (axis) {
		*axis = _axis;
	}
	return angle;
}

void VR_Math::orient_matrix_z(Mat44f& m, Coord3Df z)
{
	z.normalize_in_place();
	Coord3Df x(m.m[0][0], m.m[0][1], m.m[0][2]); /* x-axis */
	Coord3Df y(m.m[1][0], m.m[1][1], m.m[1][2]); /* y-axis */
	float scale = x.length();
	y = (z ^ x).normalize() * scale; /* rectify y */
	x = (y ^ z).normalize() * scale; /* rectify x */
	z *= scale; /* give z the correct length */
	m.m[0][0] = x.x;    m.m[0][1] = x.y;    m.m[0][2] = x.z;
	m.m[1][0] = y.x;    m.m[1][1] = y.y;    m.m[1][2] = y.z;
	m.m[2][0] = z.x;    m.m[2][1] = z.y;    m.m[2][2] = z.z;
}

Coord2Df VR_Math::project_plane_coordinates(const Mat44f& plane, Coord3Df eye, Coord3Df p, double* distance)
{
	/* Transform the viewpoint and pointer into the plane coordinate system */
	const Mat44f& plane_inv = plane.inverse();
	eye = Coord3Df(plane_inv.m[0][0]*eye.x + plane_inv.m[1][0]*eye.y + plane_inv.m[2][0]*eye.z + plane_inv.m[3][0],
				   plane_inv.m[0][1]*eye.x + plane_inv.m[1][1]*eye.y + plane_inv.m[2][1]*eye.z + plane_inv.m[3][1],
				   plane_inv.m[0][2]*eye.x + plane_inv.m[1][2]*eye.y + plane_inv.m[2][2]*eye.z + plane_inv.m[3][2]);
	
	p = Coord3Df(plane_inv.m[0][0] * p.x + plane_inv.m[1][0] * p.y + plane_inv.m[2][0] * p.z + plane_inv.m[3][0],
				 plane_inv.m[0][1] * p.x + plane_inv.m[1][1] * p.y + plane_inv.m[2][1] * p.z + plane_inv.m[3][1],
				 plane_inv.m[0][2] * p.x + plane_inv.m[1][2] * p.y + plane_inv.m[2][2] * p.z + plane_inv.m[3][2]);

	Coord3Df v = p - eye;
	if (v.z == 0) {
		v.z = 0.000001f;
	}
	/* Normalize */
	v.normalize_in_place();
	/* Get the distance to the projection
	 * = number of unit-length vector steps that have to be taken from p towards the plane
	 * until z becomes zero. */
	float d = p.z / -v.z; /* v is facing down on the plane -> negative z */
	if (distance) {
		*distance = d;
	}
	/* Now get x,y coordinates:
	 * project by taking the calculated number of unit-vector steps until z=0 */
	return Coord2Df(p.x + v.x * d, p.y + v.y * d);
}