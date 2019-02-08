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

/** \file blender/vr/intern/vr_widget_addprimitive.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_addprimitive.h"
#include "vr_widget_transform.h"

#include "vr_math.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "mesh_intern.h"

#include "WM_api.h"
#include "WM_types.h"

/***********************************************************************************************//**
 * \class                               Widget_AddPrimitive
 ***************************************************************************************************
 * Interaction widget for adding (mesh) primitives.
 *
 **************************************************************************************************/
Widget_AddPrimitive Widget_AddPrimitive::obj;

Widget_AddPrimitive::Primitive Widget_AddPrimitive::primitive(Widget_AddPrimitive::PRIMITIVE_CUBE);

bool Widget_AddPrimitive::calc_uvs(true);
float Widget_AddPrimitive::size(2.0f);
int Widget_AddPrimitive::end_fill_type(0);
int Widget_AddPrimitive::circle_vertices(32);
float Widget_AddPrimitive::radius(1.0f);
float Widget_AddPrimitive::depth(2.0f);
float Widget_AddPrimitive::cone_radius1(1.0f);
float Widget_AddPrimitive::cone_radius2(0.0f);
int Widget_AddPrimitive::grid_subdivx(10);
int Widget_AddPrimitive::grid_subdivy(10);
int Widget_AddPrimitive::sphere_segments(32);
int Widget_AddPrimitive::sphere_rings(16);
int Widget_AddPrimitive::sphere_subdiv(2);

/* Dummy op to pass to WM_operator_view3d_unit_defaults(),
   ED_object_add_generic_get_opts(), and EDBM_op_call_and_selectf() */
static wmOperator primitive_dummy_op;

/* From editmesh_add.c */
typedef struct MakePrimitiveData {
	float mat[4][4];
	bool was_editmode;
} MakePrimitiveData;

static Object *make_prim_init(
	bContext *C, const char *idname,
	const float loc[3], const float rot[3], ushort local_view_bits,
	MakePrimitiveData *r_creation_data)
{
	struct Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);

	r_creation_data->was_editmode = false;
	if (obedit == NULL || obedit->type != OB_MESH) {
		obedit = ED_object_add_type(C, OB_MESH, idname, loc, rot, false, local_view_bits);
		ED_object_editmode_enter_ex(bmain, scene, obedit, 0);

		r_creation_data->was_editmode = true;
	}

	ED_object_new_primitive_matrix(C, obedit, loc, rot, r_creation_data->mat);

	return obedit;
}

static void make_prim_finish(bContext *C, Object *obedit, const MakePrimitiveData *creation_data, int enter_editmode)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const bool exit_editmode = ((creation_data->was_editmode == true) && (enter_editmode == false));

	/* Primitive has all verts selected, use vert select flush
	 * to push this up to edges & faces. */
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	/* only recalc editmode tessface if we are staying in editmode */
	EDBM_update_generic(em, !exit_editmode, true);

	/* userdef */
	if (exit_editmode) {
		ED_object_editmode_exit(C, EM_FREEDATA);
	}
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);
}

static int add_primitive_plane_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::size = 2.0f;

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Plane"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_grid x_segments=%i y_segments=%i size=%f matrix=%m4 calc_uvs=%b",
		1, 1, Widget_AddPrimitive::size / 2.0f, creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_cube_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::size = 2.0f;

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cube"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_cube matrix=%m4 size=%f calc_uvs=%b",
		creation_data.mat, Widget_AddPrimitive::size, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	/* BMESH_TODO make plane side this: M_SQRT2 - plane (diameter of 1.41 makes it unit size) */
	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_circle_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::end_fill_type = 0;
	Widget_AddPrimitive::circle_vertices = 32;
	Widget_AddPrimitive::radius = 1.0f;

	int cap_end = Widget_AddPrimitive::end_fill_type;
	int cap_tri = (cap_end == 2);

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Circle"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_circle segments=%i radius=%f cap_ends=%b cap_tris=%b matrix=%m4 calc_uvs=%b",
		Widget_AddPrimitive::circle_vertices, Widget_AddPrimitive::radius,
		cap_end, cap_tri, creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_cylinder_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::end_fill_type = 1;
	Widget_AddPrimitive::circle_vertices = 32;
	Widget_AddPrimitive::radius = 1.0f;
	Widget_AddPrimitive::depth = 2.0f;

	const bool cap_end = (Widget_AddPrimitive::end_fill_type != 0);
	const bool cap_tri = (Widget_AddPrimitive::end_fill_type == 2);

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cylinder"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b cap_tris=%b depth=%f matrix=%m4 calc_uvs=%b",
		Widget_AddPrimitive::circle_vertices,
		Widget_AddPrimitive::radius,
		Widget_AddPrimitive::radius,
		cap_end, cap_tri,
		Widget_AddPrimitive::depth, creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_cone_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::end_fill_type = 1;
	Widget_AddPrimitive::circle_vertices = 32;
	Widget_AddPrimitive::cone_radius1 = 1.0f;
	Widget_AddPrimitive::cone_radius2 = 0.0f;
	Widget_AddPrimitive::depth = 2.0f;

	const bool cap_end = (Widget_AddPrimitive::end_fill_type != 0);
	const bool cap_tri = (Widget_AddPrimitive::end_fill_type == 2);

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cone"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b cap_tris=%b depth=%f matrix=%m4 calc_uvs=%b",
		Widget_AddPrimitive::circle_vertices, Widget_AddPrimitive::cone_radius1,
		Widget_AddPrimitive::cone_radius2, cap_end, cap_tri, Widget_AddPrimitive::depth,
		creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_grid_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::grid_subdivx = 10;
	Widget_AddPrimitive::grid_subdivy = 10;
	Widget_AddPrimitive::size = 1.0f;

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Grid"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_grid x_segments=%i y_segments=%i size=%f matrix=%m4 calc_uvs=%b",
		Widget_AddPrimitive::grid_subdivx,
		Widget_AddPrimitive::grid_subdivy,
		Widget_AddPrimitive::size / 2.0f, creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_monkey_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	float dia;
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::size = 2.0f;

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Suzanne"),
		loc, rot, local_view_bits, &creation_data);
	dia = Widget_AddPrimitive::size / 2.0f;
	mul_mat3_m4_fl(creation_data.mat, dia);

	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_monkey matrix=%m4 calc_uvs=%b", creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_uvsphere_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::sphere_segments = 32;
	Widget_AddPrimitive::sphere_rings = 16;
	Widget_AddPrimitive::radius = 1.0f;

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Sphere"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_uvsphere u_segments=%i v_segments=%i diameter=%f matrix=%m4 calc_uvs=%b",
		Widget_AddPrimitive::sphere_segments, Widget_AddPrimitive::sphere_rings,
		Widget_AddPrimitive::radius, creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

static int add_primitive_icosphere_exec(bContext *C, VR_UI::Cursor& c)
{
	MakePrimitiveData creation_data;
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3];
	bool enter_editmode;
	ushort local_view_bits;

	/* TODO_XR */
	Widget_AddPrimitive::calc_uvs = true;
	Widget_AddPrimitive::sphere_subdiv = 2;
	Widget_AddPrimitive::radius = 1.0f;

	if (VR_UI::ctrl_key_get()) {
		/* Create at Blender 3D cursor */
		Scene *scene = CTX_data_scene(C);
		float m[4][4];
		ED_view3d_cursor3d_calc_mat4(scene, m);
		mat4_to_eul(rot, m);
		memcpy(loc, m[3], sizeof(float) * 3);
	}
	else {
		/* Create at VR controller / cursor */
		const Mat44f& m = c.position.get(VR_SPACE_BLENDER);
		mat4_to_eul(rot, (float(*)[4])m.m);
		memcpy(loc, m.m[3], sizeof(float) * 3);
	}

	enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		local_view_bits = v3d->local_view_uuid;
	}
	else {
		local_view_bits = 0;
	}

	obedit = make_prim_init(
		C, CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Icosphere"),
		loc, rot, local_view_bits, &creation_data);
	em = BKE_editmesh_from_object(obedit);

	if (Widget_AddPrimitive::calc_uvs) {
		ED_mesh_uv_texture_ensure((Mesh*)obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
		em, &primitive_dummy_op, "verts.out", false,
		"create_icosphere subdivisions=%i diameter=%f matrix=%m4 calc_uvs=%b",
		Widget_AddPrimitive::sphere_subdiv,
		Widget_AddPrimitive::radius, creation_data.mat, Widget_AddPrimitive::calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, &creation_data, enter_editmode);

	return OPERATOR_FINISHED;
}

bool Widget_AddPrimitive::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_AddPrimitive::click(VR_UI::Cursor& c)
{
	/* Create primitive */
	bContext *C = vr_get_obj()->ctx;
	int ret;
	switch (primitive) {
	case PRIMITIVE_PLANE: {
		ret = add_primitive_plane_exec(C, c);
		break;
	}
	case PRIMITIVE_CUBE: {
		ret = add_primitive_cube_exec(C, c);
		break;
	}
	case PRIMITIVE_CIRCLE: {
		ret = add_primitive_circle_exec(C, c);
		break;
	}
	case PRIMITIVE_CYLINDER: {
		ret = add_primitive_cylinder_exec(C, c);
		break;
	}
	case PRIMITIVE_CONE: {
		ret = add_primitive_cone_exec(C, c);
		break;
	}
	case PRIMITIVE_GRID: {
		ret = add_primitive_grid_exec(C, c);
		break;
	}
	case PRIMITIVE_MONKEY: {
		ret = add_primitive_monkey_exec(C, c);
		break;
	}
	case PRIMITIVE_UVSPHERE: {
		ret = add_primitive_uvsphere_exec(C, c);
		break;
	}
	case PRIMITIVE_ICOSPHERE: {
		ret = add_primitive_icosphere_exec(C, c);
		break;
	}
	default: {
		return;
	}
	}

	if (ret == OPERATOR_FINISHED) {
		ED_undo_push(C, "Primitive");

		/* Update manipulators */
		Widget_Transform::update_manipulator();
	}
}

bool Widget_AddPrimitive::has_drag(VR_UI::Cursor& c) const
{
	return false;
}
