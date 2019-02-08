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

/** \file blender/vr/intern/vr_widget_transform.cpp
*   \ingroup vr
* 
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_transform.h"
#include "vr_widget_extrude.h"

#include "vr_math.h"
#include "vr_draw.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"
#include "ED_undo.h"

#include "WM_gizmo_types.h"
#include "gizmo_library_intern.h"

#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_types.h"

#include "vr_util.h"

/***********************************************************************************************//**
 * \class									Widget_Transform
 ***************************************************************************************************
 * Interaction widget for the Transform tool.
 *
 **************************************************************************************************/
Widget_Transform Widget_Transform::obj;

Widget_Transform::TransformMode Widget_Transform::transform_mode(Widget_Transform::TRANSFORMMODE_OMNI);
bool Widget_Transform::omni(true);
VR_UI::ConstraintMode Widget_Transform::constraint_mode(VR_UI::CONSTRAINTMODE_NONE);
int Widget_Transform::constraint_flag[3] = { 0 };
VR_UI::SnapMode Widget_Transform::snap_mode(VR_UI::SNAPMODE_TRANSLATION);
int Widget_Transform::snap_flag[3] = { 1, 1, 1 };
std::vector<Mat44f*> Widget_Transform::nonsnap_t;
bool Widget_Transform::snapped(false);

/* Multiplier for one and two-handed scaling transformations. */
#define WIDGET_TRANSFORM_SCALING_SENSITIVITY 0.5f

/* Precision multipliers. */
#define WIDGET_TRANSFORM_TRANS_PRECISION 0.1f
#define WIDGET_TRANSFORM_ROT_PRECISION (PI/36.0f)
#define WIDGET_TRANSFORM_SCALE_PRECISION 0.005f

//bool Widget_Transform::edit(false);
VR_UI::TransformSpace Widget_Transform::transform_space(VR_UI::TRANSFORMSPACE_GLOBAL);
bool Widget_Transform::is_dragging(false);

bool Widget_Transform::manipulator(false);
Mat44f Widget_Transform::manip_t = VR_Math::identity_f;
Mat44f Widget_Transform::manip_t_orig;
Mat44f Widget_Transform::manip_t_snap;
Coord3Df Widget_Transform::manip_angle[VR_UI::TRANSFORMSPACES];
float Widget_Transform::manip_scale_factor(2.0f);

Mat44f Widget_Transform::obmat_inv;

/* Manipulator colors. */
static const float c_manip[4][4] = { 1.0f, 0.2f, 0.322f, 0.4f,
									 0.545f, 0.863f, 0.0f, 0.4f,
								     0.157f, 0.565f, 1.0f, 0.4f,
									 1.0f, 1.0f, 1.0f, 0.4f };
static const float c_manip_select[4][4] = { 1.0f, 0.2f, 0.322f, 1.0f,
											0.545f, 0.863f, 0.0f, 1.0f,
											0.157f, 0.565f, 1.0f, 1.0f,
											1.0f, 1.0f, 1.0f, 1.0f };
/* Scale factors for manipulator rendering. */
#define WIDGET_TRANSFORM_ARROW_SCALE_FACTOR	0.1f
#define WIDGET_TRANSFORM_BOX_SCALE_FACTOR 0.05f
#define WIDGET_TRANSFORM_BALL_SCALE_FACTOR 0.08f
#define WIDGET_TRANSFORM_DIAL_RESOLUTION 100

/* Select a manipulator component with raycast selection. */
void Widget_Transform::raycast_select_manipulator(const Coord3Df& p, bool *extrude)
{
	bContext *C = vr_get_obj()->ctx;
	ARegion *ar = CTX_wm_region(C);
	/* TODO_XR: Use rv3d->persmat of dominant eye. */
	RegionView3D *rv3d = (RegionView3D*)ar->regiondata;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	int mval[2];
	float screen_co[2];

	VR_Side side = VR_UI::eye_dominance_get();
	VR_UI::get_pixel_coordinates(p, mval[0], mval[1], side);
	const float mval_fl[2] = { (float)mval[0], (float)mval[1] };

	static Coord3Df axis[3];
	static float axis_length[3];

	static Coord3Df pos;
	float length;

	bool hit = false;

	for (int i = 0; i < 3; ++i) {
		axis[i] = (*(Coord3Df*)manip_t.m[i]).normalize();
		axis_length[i] = (*(Coord3Df*)manip_t.m[i]).length();
	}
	const Coord3Df& manip_pos = *(Coord3Df*)manip_t.m[3];

	/* Do hit / selection test for shared manipulator. */
	for (int i = 0; i < 16; ++i) {
		switch (i) {
		case 0: { /* z extrude ball */
			if (!extrude) {
				i += 2;
				continue;
			}
			length = axis_length[2] * manip_scale_factor * 1.6f;
			pos = manip_pos + axis[2] * length;
			break;
		}
		case 1: { /* x extrude ball */
			if (Widget_Extrude::extrude_mode != Widget_Extrude::EXTRUDEMODE_REGION) {
				++i;
				continue;
			}
			length = axis_length[0] * manip_scale_factor * 1.6f;
			pos = manip_pos + axis[0] * length;
			break;
		}
		case 2: { /* y extrude ball */
			length = axis_length[1] * manip_scale_factor * 1.6f;
			pos = manip_pos + axis[1] * length;
			break;
		}
		case 3: { /* x-axis arrow */
			if (transform_mode != TRANSFORMMODE_MOVE && !omni) {
				i += 2;
				continue;
			}
			length = axis_length[0] * manip_scale_factor;
			pos = manip_pos + axis[0] * length;
			break;
		}
		case 4: { /* y-axis arrow */
			length = axis_length[1] * manip_scale_factor;
			pos = manip_pos + axis[1] * length;
			break;
		}
		case 5: { /* z-axis arrow */
			length = axis_length[2] * manip_scale_factor;
			pos = manip_pos + axis[2] * length;
			break;
		}
		case 6: { /* x-axis box */
			if (transform_mode != TRANSFORMMODE_SCALE && !omni) {
				i += 2;
				continue;
			}
			length = axis_length[0] * manip_scale_factor / 2.0f;
			pos = manip_pos + axis[0] * length;
			break;
		}
		case 7: { /* y-axis box */
			length = axis_length[1] * manip_scale_factor / 2.0f;
			pos = manip_pos + axis[1] * length;
			break;
		}
		case 8: { /* z-axis box */
			length = axis_length[2] * manip_scale_factor / 2.0f;
			pos = manip_pos + axis[2] * length;
			break;
		}
		case 9: { /* x-rotation ball */
			if (transform_mode != TRANSFORMMODE_ROTATE && !omni) {
				i += 2;
				continue;
			}
			rotate_v3_v3v3fl((float*)&pos, (float*)&axis[1], (float*)&axis[0], PI / 4.0f);
			length = axis_length[1] * manip_scale_factor / 2.0f;
			pos = manip_pos + pos * length;
			break;
		}
		case 10: { /* y-rotation ball */
			rotate_v3_v3v3fl((float*)&pos, (float*)&axis[2], (float*)&axis[1], PI / 4.0f);
			length = axis_length[2] * manip_scale_factor / 2.0f;
			pos = manip_pos + pos * length;
			break;
		}
		case 11: { /* z-rotation ball */
			rotate_v3_v3v3fl((float*)&pos, (float*)&axis[0], (float*)&axis[2], PI / 4.0f);
			length = axis_length[0] * manip_scale_factor / 2.0f;
			pos = manip_pos + pos * length;
			break;
		}
		case 12: { /* xy plane */
			if (omni || (transform_mode != TRANSFORMMODE_MOVE && transform_mode != TRANSFORMMODE_SCALE)) {
				i += 2;
				continue;
			};
			pos = manip_pos + (axis[0] * axis_length[0] + axis[1] * axis_length[1]) * manip_scale_factor / 2.0f;
			break;
		}
		case 13: { /* yz plane */
			pos = manip_pos + (axis[1] * axis_length[1] + axis[2] * axis_length[2]) * manip_scale_factor / 2.0f;
			break;
		}
		case 14: { /* zx plane */
			pos = manip_pos + (axis[0] * axis_length[0] + axis[2] * axis_length[2]) * manip_scale_factor / 2.0f;
			break;
		}
		case 15: { /* center box */
			if (!omni) {
				continue;
			}
			pos = manip_pos;
			break;
		}
		}

		if (VR_Util::view3d_project(
			ar, rv3d->persmat, false, (float*)&pos, screen_co,
			(eV3DProjTest)(V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR)) == V3D_PROJ_RET_OK)
		{
			float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
			dist_temp += 150.0f; //50.0f;
			if (dist_temp < dist) {
				hit = true;
				switch (i) {
				case 0: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Z;
					*extrude = true;
					return;
				}
				case 1: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_X;
					*extrude = true;
					return;
				}
				case 2: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
					*extrude = true;
					return;
				}
				case 3: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_X;
					return;
				}
				case 4: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Y;
					return;
				}
				case 5: {
					constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_Z;
					return;
				}
				case 6: {
					constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_X;
					return;
				}
				case 7: {
					constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Y;
					return;
				}
				case 8: {
					constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_Z;
					return;
				}
				case 9: {
					constraint_mode = VR_UI::CONSTRAINTMODE_ROT_X;
					return;
				}
				case 10: {
					constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Y;
					return;
				}
				case 11: {
					constraint_mode = VR_UI::CONSTRAINTMODE_ROT_Z;
					return;
				}
				case 12: {
					if (transform_mode == TRANSFORMMODE_SCALE) {
						constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_XY;
					}
					else {
						constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_XY;
					}
					return;
				}
				case 13: {
					if (transform_mode == TRANSFORMMODE_SCALE) {
						constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_YZ;
					}
					else {
						constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_YZ;
					}
					return;
				}
				case 14: {
					if (transform_mode == TRANSFORMMODE_SCALE) {
						constraint_mode = VR_UI::CONSTRAINTMODE_SCALE_ZX;
					}
					else {
						constraint_mode = VR_UI::CONSTRAINTMODE_TRANS_ZX;
					}
					return;
				}
				case 15: {
					transform_mode = TRANSFORMMODE_SCALE;
					snap_mode = VR_UI::SNAPMODE_SCALE;
					constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
					return;
				}
				}
			}
		}
	}

	if (!hit) {
		constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
	}
}

void Widget_Transform::update_manipulator()
{
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
	Object *obact = NULL;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		if (!ctx_link) {
			memset(manip_t.m, 0, sizeof(float) * 4 * 4);
			return;
		}
		obact = (Object*)ctx_link->ptr.data;
	}

	static float rot[3][3];
	static float z_axis[3] = { 0.0f, 0.0f, 1.0f };
	if (obedit && obedit->type == OB_MESH) {
		/* Edit mode */
		Scene *scene = CTX_data_scene(C);
		ToolSettings *ts = scene->toolsettings;
		BMesh *bm = ((Mesh*)obedit->data)->edit_btmesh->bm;
		if (bm) {
			BMIter iter;
			int count;

			const Mat44f& offset = *(Mat44f*)obedit->obmat;
			static Mat44f offset_no;
			offset_no = offset;
			memset(offset_no.m[3], 0, sizeof(float) * 3);
			static Coord3Df pos, no, temp;
			memset(&no, 0, sizeof(float) * 3);
			memset(&pos, 0, sizeof(float) * 3);

			manip_t.set_to_identity();

			switch (transform_space) {
			case VR_UI::TRANSFORMSPACE_NORMAL: {
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					count = 0;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							no += *(Coord3Df*)v->no;

							pos += *(Coord3Df*)v->co;
							++count;
						}
					}

					no /= (float)count;
					VR_Math::multiply_mat44_coord3D(temp, offset_no, no);
					temp.normalize_in_place();
					rotation_between_vecs_to_mat3(rot, z_axis, (float*)&temp);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
					}

					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					count = 0;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							no += *(Coord3Df*)e->v1->no + *(Coord3Df*)e->v2->no;

							pos += *(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co;
							count += 2;
						}
					}
					no /= (float)count;
					VR_Math::multiply_mat44_coord3D(temp, offset_no, no);
					temp.normalize_in_place();
					rotation_between_vecs_to_mat3(rot, z_axis, (float*)&temp);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
					}

					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					count = 0;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								no += *(Coord3Df*)l->v->no;

								pos += *(Coord3Df*)l->v->co;
								++count;
							}
						}
					}
					no /= (float)count;
					VR_Math::multiply_mat44_coord3D(temp, offset_no, no);
					temp.normalize_in_place();
					rotation_between_vecs_to_mat3(rot, z_axis, (float*)&temp);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
					}

					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				break;
			}
			case VR_UI::TRANSFORMSPACE_LOCAL: {
				const Mat44f& obmat = *(Mat44f*)obedit->obmat;
				for (int i = 0; i < 3; ++i) {
					memcpy(manip_t.m[i], obmat.m[i], sizeof(float) * 3);
				}
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					count = 0;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)v->co;
							++count;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					count = 0;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co;
							count += 2;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					count = 0;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								pos += *(Coord3Df*)l->v->co;
								++count;
							}
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				break;
			}
			case VR_UI::TRANSFORMSPACE_GLOBAL:
			default: {
				manip_t.set_to_identity();
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					count = 0;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)v->co;
							++count;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					count = 0;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							pos += *(Coord3Df*)e->v1->co + *(Coord3Df*)e->v2->co;
							count += 2;
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					count = 0;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								pos += *(Coord3Df*)l->v->co;
								++count;
							}
						}
					}
					pos /= (float)count;
					VR_Math::multiply_mat44_coord3D(*(Coord3Df*)manip_t.m[3], offset, pos);
				}
				break;
			}
			}
		}
		return;
	} /* else, object mode */

	manip_t.set_to_identity();
	if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
		manip_t.m[0][0] = manip_t.m[1][1] = manip_t.m[2][2] = 0.0f;
	}
	float manip_length = 0.0f;
	int num_objects = 0;
	for (; ctx_link; ctx_link = ctx_link->next) {
		obact = (Object*)ctx_link->ptr.data;
		if (!obact) {
			continue;
		}

		if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
			/* Average object rotations (z-axis). */
			*(Coord3Df*)manip_t.m[2] += *(Coord3Df*)obact->obmat[2];
		}
		/* Average object positions for manipulator location */
		*(Coord3Df*)manip_t.m[3] += *(Coord3Df*)obact->obmat[3];
		/* Use largest axis size (across all objects) for manipulator size */
		for (int i = 0; i < 3; ++i) {
			const float& len = (*(Coord3Df*)obact->obmat[i]).length();
			if (len > manip_length) {
				manip_length = len;
			}
		}
		++num_objects;
	}

	*(Coord3Df*)manip_t.m[3] /= num_objects;
	if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
		*(Coord3Df*)manip_t.m[2] /= num_objects;
		(*(Coord3Df*)manip_t.m[2]).normalize_in_place();
		rotation_between_vecs_to_mat3(rot, z_axis, manip_t.m[2]);
		for (int i = 0; i < 3; ++i) {
			memcpy(manip_t.m[i], rot[i], sizeof(float) * 3);
		}
		/* Apply uniform scaling to manipulator */
		for (int i = 0; i < 3; ++i) {
			*(Coord3Df*)manip_t.m[i] *= manip_length;
		}
	}
	else {
		/* Apply uniform scaling to manipulator */
		for (int i = 0; i < 3; ++i) {
			(*(Coord3Df*)manip_t.m[i]).normalize_in_place() *= manip_length;
		}
	}
}

bool Widget_Transform::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Transform::click(VR_UI::Cursor& c)
{
	const Mat44f& m = c.position.get();
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		VR_Util::raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	else {
		VR_Util::raycast_select_single(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}
	/* Update manipulator transform. */
	update_manipulator();
	
	if (manipulator) {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = true;
		}
	}
}	

void Widget_Transform::drag_start(VR_UI::Cursor& c)
{
	/* If other hand is already dragging, don't change the current state of the Transform tool. */
	if (c.bimanual) {
		return;
	}

	if (manipulator) {
		/* Test for manipulator selection and set constraints. */
		const Mat44f& m = c.position.get();
		raycast_select_manipulator(*(Coord3Df*)m.m[3]);
	}

	/* Set transform/snapping modes based on constraints */
	memset(constraint_flag, 0, sizeof(int) * 3);
	if (constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		switch (constraint_mode) {
		case VR_UI::CONSTRAINTMODE_TRANS_X: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Y: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_Z: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_XY: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[0] = constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[1] = constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
			transform_mode = TRANSFORMMODE_MOVE;
			snap_mode = VR_UI::SNAPMODE_TRANSLATION;
			constraint_flag[0] = constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			transform_mode = TRANSFORMMODE_ROTATE;
			snap_mode = VR_UI::SNAPMODE_ROTATION;
			constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			transform_mode = TRANSFORMMODE_ROTATE;
			snap_mode = VR_UI::SNAPMODE_ROTATION;
			constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			transform_mode = TRANSFORMMODE_ROTATE;
			snap_mode = VR_UI::SNAPMODE_ROTATION;
			constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_X: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[0] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Y: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_Z: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_XY: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[0] = constraint_flag[1] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_YZ: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[1] = constraint_flag[2] = 1;
			break;
		}
		case VR_UI::CONSTRAINTMODE_SCALE_ZX: {
			transform_mode = TRANSFORMMODE_SCALE;
			snap_mode = VR_UI::SNAPMODE_SCALE;
			constraint_flag[0] = constraint_flag[2] = 1;
			break;
		}
		default: {
			break;
		}
		}
		memcpy(snap_flag, constraint_flag, sizeof(int) * 3);
	}
	else {
		memset(snap_flag, 1, sizeof(int) * 3);
	}

	/* Set up snapping positions vector */
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
	Object *obedit = CTX_data_edit_object(C);
	if (!ctx_link && !obedit) {
		return;
	}
	for (int i = 0; i < nonsnap_t.size(); ++i) {
		delete nonsnap_t[i];
	}
	nonsnap_t.clear();
	for (; ctx_link; ctx_link = ctx_link->next) {
		nonsnap_t.push_back(new Mat44f());
	}
	snapped = false;

	/* Reset manipulator angles */
	memset(&manip_angle, 0, sizeof(float) * 9);
	/* Save original manipulator transformation */
	if (obedit) {
		obmat_inv = (*(Mat44f*)obedit->obmat).inverse();
		manip_t_orig = manip_t * obmat_inv;
	}
	else {
		manip_t_orig = manip_t;
	}

	if (manipulator || constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = true;
		}
	}

	//is_dragging = true;

	/* Call drag_contd() immediately? */
	Widget_Transform::obj.drag_contd(c);
}

void Widget_Transform::drag_contd(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	ListBase ctx_data_list;
	CTX_data_selected_objects(C, &ctx_data_list);
	CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
	Object *obedit = CTX_data_edit_object(C);
	if (!ctx_link && !obedit) {
		return;
	}
	ToolSettings *ts = NULL;
	BMesh *bm = NULL;
	if (obedit) {
		/* Edit mode */
		ts = CTX_data_scene(C)->toolsettings;
		if (!ts) {
			return;
		}
		if (obedit->type == OB_MESH) {
			bm = ((Mesh*)obedit->data)->edit_btmesh->bm;
			if (!bm) {
				return;
			}
		}
	}

	static Mat44f curr;
	static Mat44f prev;
	/* Check if we're two-hand dragging */
	if (c.bimanual) {
		if (c.bimanual == VR_UI::Cursor::BIMANUAL_SECOND)
			return; /* calculations are only performed by first hand */

		const Mat44f& curr_h = VR_UI::cursor_position_get(VR_SPACE_BLENDER, c.side);
		const Mat44f& curr_o = VR_UI::cursor_position_get(VR_SPACE_BLENDER, (VR_Side)(1 - c.side));
		const Mat44f& prev_h = c.interaction_position.get(VR_SPACE_BLENDER);
		const Mat44f& prev_o = c.other_hand->interaction_position.get(VR_SPACE_BLENDER);

		/* Rotation
		/* x-axis is the base line between the two pointers */
		Coord3Df x_axis_prev(prev_h.m[3][0] - prev_o.m[3][0],
			prev_h.m[3][1] - prev_o.m[3][1],
			prev_h.m[3][2] - prev_o.m[3][2]);
		Coord3Df x_axis_curr(curr_h.m[3][0] - curr_o.m[3][0],
			curr_h.m[3][1] - curr_o.m[3][1],
			curr_h.m[3][2] - curr_o.m[3][2]);
		/* y-axis is the average of the pointers y-axis */
		Coord3Df y_axis_prev((prev_h.m[1][0] + prev_o.m[1][0]) / 2.0f,
			(prev_h.m[1][1] + prev_o.m[1][1]) / 2.0f,
			(prev_h.m[1][2] + prev_o.m[1][2]) / 2.0f);
		Coord3Df y_axis_curr((curr_h.m[1][0] + curr_o.m[1][0]) / 2.0f,
			(curr_h.m[1][1] + curr_o.m[1][1]) / 2.0f,
			(curr_h.m[1][2] + curr_o.m[1][2]) / 2.0f);

		/* z-axis is the cross product of the two */
		Coord3Df z_axis_prev = x_axis_prev ^ y_axis_prev;
		Coord3Df z_axis_curr = x_axis_curr ^ y_axis_curr;
		/* fix the y-axis to be orthogonal */
		y_axis_prev = z_axis_prev ^ x_axis_prev;
		y_axis_curr = z_axis_curr ^ x_axis_curr;
		/* normalize and apply */
		x_axis_prev.normalize_in_place();
		x_axis_curr.normalize_in_place();
		y_axis_prev.normalize_in_place();
		y_axis_curr.normalize_in_place();
		z_axis_prev.normalize_in_place();
		z_axis_curr.normalize_in_place();
		prev.m[0][0] = x_axis_prev.x;    prev.m[0][1] = x_axis_prev.y;    prev.m[0][2] = x_axis_prev.z;
		prev.m[1][0] = y_axis_prev.x;    prev.m[1][1] = y_axis_prev.y;    prev.m[1][2] = y_axis_prev.z;
		prev.m[2][0] = z_axis_prev.x;    prev.m[2][1] = z_axis_prev.y;    prev.m[2][2] = z_axis_prev.z;
		curr.m[0][0] = x_axis_curr.x;    curr.m[0][1] = x_axis_curr.y;    curr.m[0][2] = x_axis_curr.z;
		curr.m[1][0] = y_axis_curr.x;    curr.m[1][1] = y_axis_curr.y;    curr.m[1][2] = y_axis_curr.z;
		curr.m[2][0] = z_axis_curr.x;    curr.m[2][1] = z_axis_curr.y;    curr.m[2][2] = z_axis_curr.z;

		/* Translation: translation of the averaged pointer positions */
		prev.m[3][0] = (prev_h.m[3][0] + prev_o.m[3][0]) / 2.0f;    prev.m[3][1] = (prev_h.m[3][1] + prev_o.m[3][1]) / 2.0f;    prev.m[3][2] = (prev_h.m[3][2] + prev_o.m[3][2]) / 2.0f;	prev.m[3][3] = 1;
		curr.m[3][0] = (curr_h.m[3][0] + curr_o.m[3][0]) / 2.0f;    curr.m[3][1] = (curr_h.m[3][1] + curr_o.m[3][1]) / 2.0f;    curr.m[3][2] = (curr_h.m[3][2] + curr_o.m[3][2]) / 2.0f;	curr.m[3][3] = 1;

		if (transform_mode != TRANSFORMMODE_ROTATE) {
			/* Scaling: distance between pointers */
			float curr_s = sqrt(((curr_h.m[3][0] - curr_o.m[3][0])*(curr_h.m[3][0] - curr_o.m[3][0])) + ((curr_h.m[3][1]) - curr_o.m[3][1])*(curr_h.m[3][1] - curr_o.m[3][1])) + ((curr_h.m[3][2] - curr_o.m[3][2])*(curr_h.m[3][2] - curr_o.m[3][2]));
			float start_s = sqrt(((prev_h.m[3][0] - prev_o.m[3][0])*(prev_h.m[3][0] - prev_o.m[3][0])) + ((prev_h.m[3][1]) - prev_o.m[3][1])*(prev_h.m[3][1] - prev_o.m[3][1])) + ((prev_h.m[3][2] - prev_o.m[3][2])*(prev_h.m[3][2] - prev_o.m[3][2]));

			prev.m[0][0] *= start_s; prev.m[1][0] *= start_s; prev.m[2][0] *= start_s;
			prev.m[0][1] *= start_s; prev.m[1][1] *= start_s; prev.m[2][1] *= start_s;
			prev.m[0][2] *= start_s; prev.m[1][2] *= start_s; prev.m[2][2] *= start_s;

			curr.m[0][0] *= curr_s; curr.m[1][0] *= curr_s; curr.m[2][0] *= curr_s;
			curr.m[0][1] *= curr_s; curr.m[1][1] *= curr_s; curr.m[2][1] *= curr_s;
			curr.m[0][2] *= curr_s; curr.m[1][2] *= curr_s; curr.m[2][2] *= curr_s;
		}

		c.interaction_position.set(curr_h.m, VR_SPACE_BLENDER);
		c.other_hand->interaction_position.set(curr_o.m, VR_SPACE_BLENDER);
	}
	else { /* one-handed drag */
		curr = c.position.get(VR_SPACE_BLENDER);
		prev = c.interaction_position.get(VR_SPACE_BLENDER);
		c.interaction_position.set(curr.m, VR_SPACE_BLENDER);
	}

	if (obedit) {
		curr = curr * obmat_inv;
		prev = prev * obmat_inv;
	}

	/* Calculate delta based on transform mode. */
	static Mat44f delta;
	if (c.bimanual) {
		delta = prev.inverse() * curr;
	}
	else {
		switch (transform_mode) {
		case TRANSFORMMODE_MOVE: {
			delta = VR_Math::identity_f;
			*(Coord3Df*)delta.m[3] = *(Coord3Df*)curr.m[3] - *(Coord3Df*)prev.m[3];
			break;
		}
		case TRANSFORMMODE_SCALE: {
			delta = VR_Math::identity_f;
			if (constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				/* Scaling based on distance from manipulator center. */
				static Coord3Df prev_d, curr_d;		
				prev_d = *(Coord3Df*)prev.m[3] - *(Coord3Df*)manip_t_orig.m[3];
				curr_d = *(Coord3Df*)curr.m[3] - *(Coord3Df*)manip_t_orig.m[3];
				float p_len = prev_d.length();
				float s = (p_len == 0.0f) ? 1.0f : curr_d.length() / p_len;
				if (s > 1.0f) {
					s = 1.0f + (s - 1.0f) * WIDGET_TRANSFORM_SCALING_SENSITIVITY;
				}
				else if (s < 1.0f) {
					s = 1.0f - (1.0f - s) * WIDGET_TRANSFORM_SCALING_SENSITIVITY;
				}
				delta.m[0][0] = delta.m[1][1] = delta.m[2][2] = s;
			}
			else {
				*(Coord3Df*)delta.m[3] = *(Coord3Df*)curr.m[3] - *(Coord3Df*)prev.m[3];
				float s = (*(Coord3Df*)delta.m[3]).length();
				(*(Coord3Df*)delta.m[3]).normalize_in_place() *= s * WIDGET_TRANSFORM_SCALING_SENSITIVITY;
			}
			break;
		}
		case TRANSFORMMODE_ROTATE:
		case TRANSFORMMODE_OMNI:
		default: {
			delta = prev.inverse() * curr;
			break;
		}
		}
	}

	static Mat44f delta_orig;
	static float scale[3];
	static float eul[3];
	static float rot[3][3];
	static Coord3Df temp1, temp2;

	/* Precision */
	if (VR_UI::shift_key_get()) {
		/* Translation */
		for (int i = 0; i < 3; ++i) {
			scale[i] = (*(Coord3Df*)delta.m[i]).length();
		}
		*(Coord3Df*)delta.m[3] *= WIDGET_TRANSFORM_TRANS_PRECISION;
		/*for (int i = 0; i < 3; ++i) {
			delta.m[3][i] *= (scale[i] * WIDGET_TRANSFORM_TRANS_PRECISION);
		}*/

		/* Rotation */
		mat4_to_eul(eul, delta.m);
		for (int i = 0; i < 3; ++i) {
			eul[i] *= WIDGET_TRANSFORM_ROT_PRECISION;
		}
		eul_to_mat3(rot, eul);
		for (int i = 0; i < 3; ++i) {
			memcpy(delta.m[i], rot[i], sizeof(float) * 3);
		}

		/* Scale */
		for (int i = 0; i < 3; ++i) {
			if (scale[i] > 1.0001f) { /* Take numerical instability into account */
				*(Coord3Df*)delta.m[i] *= (1.0f + WIDGET_TRANSFORM_SCALE_PRECISION);
			}
			else if (scale[i] < 0.9999f) {
				*(Coord3Df*)delta.m[i] *= (1.0f - WIDGET_TRANSFORM_SCALE_PRECISION);
			}
		}
	}

	/* Constraints */
	bool constrain = false;
	if (constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		delta_orig = delta;
		delta = VR_Math::identity_f;
		constrain = true;
	}

	/* Snapping */
	bool snap = false;
	if (VR_UI::ctrl_key_get()) {
		snap = true;
	}

	for (int index = 0; ctx_link; ctx_link = ctx_link->next, ++index) {
		Object *obact = (Object*)ctx_link->ptr.data;
		if (!obact) {
			continue;
		}

		/* Constraints */
		if (constrain) {
			static float axis[3];
			static float angle;
			static Coord3Df temp3;
			switch (constraint_mode) {
			case VR_UI::CONSTRAINTMODE_TRANS_X: {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], manip_t_orig.m[0]);
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Y: {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], manip_t_orig.m[1]);
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_Z: {
				project_v3_v3v3(delta.m[3], delta_orig.m[3], manip_t_orig.m[2]);
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_XY: {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
				project_v3_v3v3(&temp2.x, delta_orig.m[3], manip_t_orig.m[1]);
				*(Coord3Df*)delta.m[3] = temp1 + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[1]);
				project_v3_v3v3(&temp2.x, delta_orig.m[3], manip_t_orig.m[2]);
				*(Coord3Df*)delta.m[3] = temp1 + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
				project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
				project_v3_v3v3(&temp2.x, delta_orig.m[3], manip_t_orig.m[2]);
				*(Coord3Df*)delta.m[3] = temp1 + temp2;
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_X: {
				mat4_to_axis_angle(axis, &angle, delta_orig.m);
				if ((*(Coord3Df*)axis) * (*(Coord3Df*)manip_t_orig.m[0]) < 0) {
					angle = -angle;
				}
				axis_angle_to_mat4(delta.m, manip_t_orig.m[0], angle);
				if (VR_UI::shift_key_get()) {
					manip_angle[transform_space].x += angle * WIDGET_TRANSFORM_ROT_PRECISION;
				}
				else {
					manip_angle[transform_space].x += angle;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Y: {
				mat4_to_axis_angle(axis, &angle, delta_orig.m);
				if ((*(Coord3Df*)axis) * (*(Coord3Df*)manip_t_orig.m[1]) < 0) {
					angle = -angle;
				}
				axis_angle_to_mat4(delta.m, manip_t_orig.m[1], angle);
				if (VR_UI::shift_key_get()) {
					manip_angle[transform_space].y += angle * WIDGET_TRANSFORM_ROT_PRECISION;
				}
				else {
					manip_angle[transform_space].y += angle;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Z: {
				mat4_to_axis_angle(axis, &angle, delta_orig.m);
				if ((*(Coord3Df*)axis) * (*(Coord3Df*)manip_t_orig.m[2]) < 0) {
					angle = -angle;
				}
				axis_angle_to_mat4(delta.m, manip_t_orig.m[2], angle);
				if (VR_UI::shift_key_get()) {
					manip_angle[transform_space].z += angle * WIDGET_TRANSFORM_ROT_PRECISION;
				}
				else {
					manip_angle[transform_space].z += angle;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_X: {
				float length;
				*(Coord3Df*)scale = (*(Coord3Df*)manip_t_orig.m[0]).normalize();
				if (c.bimanual) {
					length = -delta_orig.m[3][0];
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
					length = temp1.length();
					temp2 = (*(Coord3Df*)delta_orig.m[3]).normalize();
					if (dot_v3v3((float*)&temp2, scale) < 0) {
						length = -length;
					}
				}
				for (int i = 0; i < 3; ++i) {
					delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_Y: {
				float length;
				*(Coord3Df*)scale = (*(Coord3Df*)manip_t_orig.m[1]).normalize();
				if (c.bimanual) {
					length = -delta_orig.m[3][1];
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[1]);
					length = temp1.length();
					temp2 = (*(Coord3Df*)delta_orig.m[3]).normalize();
					if (dot_v3v3((float*)&temp2, scale) < 0) {
						length = -length;
					}
				}
				for (int i = 0; i < 3; ++i) {
					delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_Z: {
				float length;
				*(Coord3Df*)scale = (*(Coord3Df*)manip_t_orig.m[2]).normalize();
				if (c.bimanual) {
					length = -delta_orig.m[3][2];
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[2]);
					length = temp1.length();
					temp2 = (*(Coord3Df*)delta_orig.m[3]).normalize();
					if (dot_v3v3((float*)&temp2, scale) < 0) {
						length = -length;
					}
				}
				for (int i = 0; i < 3; ++i) {
					delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_XY: {
				float length;
				if (c.bimanual) {
					length = -(delta_orig.m[3][0] + delta_orig.m[3][1]) / 2.0f;
					*(Coord3Df*)scale = ((*(Coord3Df*)manip_t_orig.m[0]).normalize() + (*(Coord3Df*)manip_t_orig.m[1]).normalize()) / 2.0f;
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
					length = temp1.length();
					(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
					temp1 = (*(Coord3Df*)manip_t_orig.m[0]).normalize();
					if (dot_v3v3((float*)&temp1, scale) < 0) {
						length = -length;
					}
					project_v3_v3v3(&temp3.x, delta_orig.m[3], manip_t_orig.m[1]);
					temp2 = (*(Coord3Df*)manip_t_orig.m[1]).normalize();
					if (dot_v3v3((float*)&temp2, scale) < 0) {
						length -= temp3.length();
					}
					else {
						length += temp3.length();
					}
					length /= 2.0f;
				}
				*(Coord3Df*)scale = (temp1 + temp2) / 2.0f;
				for (int i = 0; i < 3; ++i) {
					delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_YZ: {
				float length;
				if (c.bimanual) {
					length = -(delta_orig.m[3][1] + delta_orig.m[3][2]) / 2.0f;
					*(Coord3Df*)scale = ((*(Coord3Df*)manip_t_orig.m[1]).normalize() + (*(Coord3Df*)manip_t_orig.m[2]).normalize()) / 2.0f;
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[1]);
					length = temp1.length();
					(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
					temp1 = (*(Coord3Df*)manip_t_orig.m[1]).normalize();
					if (dot_v3v3((float*)&temp1, scale) < 0) {
						length = -length;
					}
					project_v3_v3v3(&temp3.x, delta_orig.m[3], manip_t_orig.m[2]);
					temp2 = (*(Coord3Df*)manip_t_orig.m[2]).normalize();
					if (dot_v3v3((float*)&temp2, scale) < 0) {
						length -= temp3.length();
					}
					else {
						length += temp3.length();
					}
					length /= 2.0f;
				}
				*(Coord3Df*)scale = (temp1 + temp2) / 2.0f;
				for (int i = 0; i < 3; ++i) {
					delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
				}
				break;
			}
			case VR_UI::CONSTRAINTMODE_SCALE_ZX: {
				float length;
				if (c.bimanual) {
					length = -(delta_orig.m[3][0] + delta_orig.m[3][2]) / 2.0f;
					*(Coord3Df*)scale = ((*(Coord3Df*)manip_t_orig.m[0]).normalize() + (*(Coord3Df*)manip_t_orig.m[2]).normalize()) / 2.0f;
				}
				else {
					project_v3_v3v3(&temp1.x, delta_orig.m[3], manip_t_orig.m[0]);
					length = temp1.length();
					(*(Coord3Df*)scale = (*(Coord3Df*)delta_orig.m[3]).normalize());
					temp1 = (*(Coord3Df*)manip_t_orig.m[0]).normalize();
					if (dot_v3v3((float*)&temp1, scale) < 0) {
						length = -length;
					}
					project_v3_v3v3(&temp3.x, delta_orig.m[3], manip_t_orig.m[2]);
					temp2 = (*(Coord3Df*)manip_t_orig.m[2]).normalize();
					if (dot_v3v3((float*)&temp2, scale) < 0) {
						length -= temp3.length();
					}
					else {
						length += temp3.length();
					}
					length /= 2.0f;
				}
				for (int i = 0; i < 3; ++i) {
					delta.m[i][i] = 1.0f + fabsf(scale[i]) * length;
				}
				break;
			}
			default: {
				break;
			}
			}
		}

		/* Snapping */
		static Mat44f m;
		if (snap) {
			if (obedit) { /* Edit mode */
				Mat44f& nonsnap_m = *nonsnap_t[index];
				if (!snapped) {
					nonsnap_m = manip_t * obmat_inv;
					manip_t_snap = manip_t * obmat_inv;
				}
				else {
					m = nonsnap_m;
					nonsnap_m = m * delta;
				}
				static Mat44f manip_t_prev;
				manip_t_prev = manip_t_snap;

				/* Apply snapping. */
				float precision, iter_fac, val;
				for (int i = 0; i < 3; ++i) {
					scale[i] = (*(Coord3Df*)nonsnap_m.m[i]).length();
				}
				switch (snap_mode) {
				case VR_UI::SNAPMODE_TRANSLATION: {
					/* Translation */
					if (VR_UI::shift_key_get()) {
						precision = WIDGET_TRANSFORM_TRANS_PRECISION;
					}
					else {
						precision = 1.0f;
					}
					float *pos = (float*)nonsnap_m.m[3];
					for (int i = 0; i < 3; ++i, ++pos) {
						if (!snap_flag[i]) {
							continue;
						}
						iter_fac = precision * scale[i];
						val = roundf(*pos / iter_fac);
						manip_t_snap.m[3][i] = iter_fac * val;
					}
					switch (constraint_mode) {
					case VR_UI::CONSTRAINTMODE_TRANS_X: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[0]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_Y: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[1]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_Z: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[2]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_XY: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[0]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[1]);
						*(Coord3Df*)manip_t_snap.m[3] += temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[1]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[2]);
						*(Coord3Df*)manip_t_snap.m[3] += temp2;
						break;
					}
					case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
						temp1 = *(Coord3Df*)manip_t_snap.m[3] - *(Coord3Df*)nonsnap_m.m[3];
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[0]);
						*(Coord3Df*)manip_t_snap.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
						project_v3_v3v3(&temp2.x, &temp1.x, manip_t_orig.m[2]);
						*(Coord3Df*)manip_t_snap.m[3] += temp2;
						break;
					}
					default: {
						/* TODO_XR: Local / normal translation snappping (no constraints) */
						break;
					}
					}
					break;
				}
				case VR_UI::SNAPMODE_ROTATION: {
					/* Rotation */
					if (VR_UI::shift_key_get()) {
						precision = PI / 180.0f;
					}
					else {
						precision = WIDGET_TRANSFORM_ROT_PRECISION;
					}
					/* TODO_XR: Local / normal rotation snapping (no constraints). */
					mat4_to_eul(eul, nonsnap_m.m);
					//static float eul_orig[3];
					//memcpy(eul_orig, eul, sizeof(float) * 3);
					for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						val = roundf(eul[i] / precision);
						eul[i] = precision * val;
					}
					eul_to_mat3(rot, eul);
					for (int i = 0; i < 3; ++i) {
						memcpy(manip_t_snap.m[i], rot[i], sizeof(float) * 3);
						*(Coord3Df*)manip_t_snap.m[i] *= scale[i];
					}
					/* Update manipulator angles. */
					/* TODO_XR */
					//for (int i = 0; i < 3; ++i) {
					//	if (!snap_flag[i]) {
					//		continue;
					//	}
					//	switch (i) {
					//	case 0: {
					//		float& m_angle = manip_angle[transform_space].x;
					//		m_angle += eul[i] - eul_orig[i];
					//		val = roundf(m_angle / precision);
					//		m_angle = precision * val;
					//		break;
					//	}
					//	case 1: {
					//		float& m_angle = manip_angle[transform_space].y;
					//		m_angle += eul[i] - eul_orig[i];
					//		val = roundf(m_angle / precision);
					//		m_angle = precision * val;
					//		break;
					//	}
					//	case 2: {
					//		float& m_angle = manip_angle[transform_space].z;
					//		m_angle += eul[i] - eul_orig[i];
					//		val = roundf(m_angle / precision);
					//		m_angle = precision * val;
					//		break;
					//	}
					//	}
					//}
					break;
				}
				case VR_UI::SNAPMODE_SCALE: {
					/* Scale */
					/* TODO_XR */
					//if (transform_space == VR_UI::TRANSFORMSPACE_GLOBAL && constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
					//	/* TODO_XR */
					//	break;
					//	/*for (int i = 0; i < 3; ++i) {
					//		if (snap_flag[i]) {
					//			continue;
					//		}
					//		(*(Coord3Df*)manip_t_snap.m[i]).normalize_in_place() *= (*(Coord3Df*)nonsnap_m.m[i]).length();
					//	}
					//	static Mat44f t;
					//	transpose_m4_m4(t.m, nonsnap_m.m);
					//	for (int i = 0; i < 3; ++i) {
					//		scale[i] = (*(Coord3Df*)t.m[i]).length();
					//	}*/
					//}
					//for (int i = 0; i < 3; ++i) {
					//	if (!snap_flag[i]) {
					//		continue;
					//	}
					//	if (VR_UI::shift_key_get()) {
					//		/* Snap scale based on the power of ten magnitude of the curent scale */
					//		precision = 0.1f * powf(10.0f, floor(log10(scale[i])));
					//	}
					//	else {
					//		precision = 0.5f * powf(10.0f, floor(log10(scale[i])));
					//	}
					//	val = roundf(scale[i] / precision);
					//	if (val == 0.0f) {
					//		val = 1.0f;
					//	}
					//	(*(Coord3Df*)manip_t_snap.m[i]).normalize_in_place() *= (precision * val);
					//}
					break;
				}
				default: {
					break;
				}
				}

				delta = manip_t_prev.inverse() * manip_t_snap;
				if (snap_mode == VR_UI::SNAPMODE_ROTATION) {
					memset(delta.m[3], 0, sizeof(float) * 3);
				}
				BMIter iter;

				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							float *co = v->co;
							memcpy((float*)&temp1, co, sizeof(float) * 3);
							mul_v3_m4v3(co, delta.m, (float*)&temp1);
						}
					}
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							float *co1 = e->v1->co;
							float *co2 = e->v2->co;
							memcpy((float*)&temp1, co1, sizeof(float) * 3);
							memcpy((float*)&temp2, co2, sizeof(float) * 3);
							mul_v3_m4v3(co1, delta.m, (float*)&temp1);
							mul_v3_m4v3(co2, delta.m, (float*)&temp2);
						}
					}
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								float *co = l->v->co;
								memcpy((float*)&temp1, co, sizeof(float) * 3);
								mul_v3_m4v3(co, delta.m, (float*)&temp1);
							}
						}
					}
				}

				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obedit->data, 0);
				/* Exit object iteration loop. */
				break;
			}
			else { /* Object mode */
				/* Save actual position. */
				Mat44f& nonsnap_m = *nonsnap_t[index];
				Mat44f& obmat = *(Mat44f*)obact->obmat;
				if (!snapped) {
					nonsnap_m = obmat;
				}
				else {
					m = nonsnap_m;
					nonsnap_m = m * delta;
				}

				/* Apply snapping. */
				float precision, iter_fac, val;
				for (int i = 0; i < 3; ++i) {
					scale[i] = (*(Coord3Df*)nonsnap_m.m[i]).length();
				}
				switch (snap_mode) {
				case VR_UI::SNAPMODE_TRANSLATION: {
					/* Translation */
					if (VR_UI::shift_key_get()) {
						precision = WIDGET_TRANSFORM_TRANS_PRECISION;
					}
					else {
						precision = 1.0f;
					}
					float *pos = (float*)nonsnap_m.m[3];
					for (int i = 0; i < 3; ++i, ++pos) {
						if (!snap_flag[i]) {
							continue;
						}
						iter_fac = precision * scale[i];
						val = roundf(*pos / iter_fac);
						obmat.m[3][i] = iter_fac * val;
					}
					if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL) {
						switch (constraint_mode) {
						case VR_UI::CONSTRAINTMODE_TRANS_X: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[0]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_Y: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[1]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_Z: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[2]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_XY: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[0]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[1]);
							*(Coord3Df*)obmat.m[3] += temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_YZ: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[1]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[2]);
							*(Coord3Df*)obmat.m[3] += temp2;
							break;
						}
						case VR_UI::CONSTRAINTMODE_TRANS_ZX: {
							temp1 = *(Coord3Df*)obmat.m[3] - *(Coord3Df*)nonsnap_m.m[3];
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[0]);
							*(Coord3Df*)obmat.m[3] = *(Coord3Df*)nonsnap_m.m[3] + temp2;
							project_v3_v3v3(&temp2.x, &temp1.x, obmat.m[2]);
							*(Coord3Df*)obmat.m[3] += temp2;
							break;
						}
						default: {
							/* TODO_XR: Local translation snappping (no constraints) */
							break;
						}
						}
					}
					break;
				}
				case VR_UI::SNAPMODE_ROTATION: {
					/* Rotation */
					if (VR_UI::shift_key_get()) {
						precision = PI / 180.0f;
					}
					else {
						precision = WIDGET_TRANSFORM_ROT_PRECISION;
					}
					/* TODO_XR: Local rotation snapping (no constraints). */
					mat4_to_eul(eul, nonsnap_m.m);
					//static float eul_orig[3];
					//memcpy(eul_orig, eul, sizeof(float) * 3);
					for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						val = roundf(eul[i] / precision);
						eul[i] = precision * val;
					}
					eul_to_mat3(rot, eul);
					for (int i = 0; i < 3; ++i) {
						memcpy(obmat.m[i], rot[i], sizeof(float) * 3);
						*(Coord3Df*)obmat.m[i] *= scale[i];
					}
					/* Update manipulator angles. */
					/* TODO_XR */
					/*for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						switch (i) {
						case 0: {
							float& m_angle = manip_angle[transform_space].x;
							m_angle += eul[i] - eul_orig[i];
							val = roundf(m_angle / precision);
							m_angle = precision * val;
							break;
						}
						case 1: {
							float& m_angle = manip_angle[transform_space].y;
							m_angle += eul[i] - eul_orig[i];
							val = roundf(m_angle / precision);
							m_angle = precision * val;
							break;
						}
						case 2: {
							float& m_angle = manip_angle[transform_space].z;
							m_angle += eul[i] - eul_orig[i];
							val = roundf(m_angle / precision);
							m_angle = precision * val;
							break;
						}
						}
					}*/
					break;
				}
				case VR_UI::SNAPMODE_SCALE: {
					/* Scale */
					if (transform_space == VR_UI::TRANSFORMSPACE_GLOBAL && constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
						/* TODO_XR */
						break;
						/*for (int i = 0; i < 3; ++i) {
							if (snap_flag[i]) {
								continue;
							}
							(*(Coord3Df*)obmat.m[i]).normalize_in_place() *= (*(Coord3Df*)nonsnap_m.m[i]).length();
						}
						static Mat44f t;
						transpose_m4_m4(t.m, nonsnap_m.m);
						for (int i = 0; i < 3; ++i) {
							scale[i] = (*(Coord3Df*)t.m[i]).length();
						}*/
					}
					for (int i = 0; i < 3; ++i) {
						if (!snap_flag[i]) {
							continue;
						}
						if (VR_UI::shift_key_get()) {
							/* Snap scale based on the power of ten magnitude of the curent scale */
							precision = 0.1f * powf(10.0f, floor(log10(scale[i])));
						}
						else {
							precision = 0.5f * powf(10.0f, floor(log10(scale[i])));
						}
						val = roundf(scale[i] / precision);
						if (val == 0.0f) {
							val = 1.0f;
						}
						(*(Coord3Df*)obmat.m[i]).normalize_in_place() *= (precision * val);
					}
					break;
				}
				default: {
					break;
				}
				}
				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obact->data, 0);
			}
		}
		else {
			if (obedit) { /* Edit mode */
				/* Transform mode */
				switch (transform_mode) {
				case TRANSFORMMODE_MOVE: {
					for (int i = 0; i < 3; ++i) {
						memcpy(delta.m[i], VR_Math::identity_f.m[i], sizeof(float) * 3);
					}
					break;
				}
				case TRANSFORMMODE_ROTATE: {
					memset(delta.m[3], 0, sizeof(float) * 3);
					break;
				}
				case TRANSFORMMODE_SCALE: {
					memset(delta.m[3], 0, sizeof(float) * 3);
					break;
				}
				case TRANSFORMMODE_OMNI: 
				default: {
					break;
				}
				}

				BMIter iter;
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BMVert *v;
					BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
						if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
							float *co = v->co;
							memcpy((float*)&temp1, co, sizeof(float) * 3);
							mul_v3_m4v3(co, delta.m, (float*)&temp1);
						}
					}
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMEdge *e;
					BM_ITER_MESH(e, &iter, bm, BM_EDGES_OF_MESH) {
						if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
							float *co1 = e->v1->co;
							float *co2 = e->v2->co;
							memcpy((float*)&temp1, co1, sizeof(float) * 3);
							memcpy((float*)&temp2, co2, sizeof(float) * 3);
							mul_v3_m4v3(co1, delta.m, (float*)&temp1);
							mul_v3_m4v3(co2, delta.m, (float*)&temp2);
						}
					}
				}
				else if (ts->selectmode & SCE_SELECT_FACE) {
					BMFace *f;
					BMLoop *l;
					BM_ITER_MESH(f, &iter, bm, BM_FACES_OF_MESH) {
						if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
							l = f->l_first;
							for (int i = 0; i < f->len; ++i, l = l->next) {
								float *co = l->v->co;
								memcpy((float*)&temp1, co, sizeof(float) * 3);
								mul_v3_m4v3(co, delta.m, (float*)&temp1);
							}
						}
					}
				}

				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obedit->data, 0);
				/* Exit object iteration loop. */
				break;
			}
			else { /* Object mode */
				m = *(Mat44f*)obact->obmat * delta;

				/* Transform mode */
				switch (transform_mode) {
				case TRANSFORMMODE_MOVE: {
					memcpy(obact->obmat[3], m.m[3], sizeof(float) * 3);
					break;
				}
				case TRANSFORMMODE_ROTATE: {
					for (int i = 0; i < 3; ++i) {
						scale[i] = (*(Coord3Df*)(obact->obmat[i])).length();
						(*(Coord3Df*)(m.m[i])).normalize_in_place();
						memcpy(obact->obmat[i], m.m[i], sizeof(float) * 3);
						*(Coord3Df*)obact->obmat[i] *= scale[i];
					}
					break;
				}
				case TRANSFORMMODE_SCALE: {
					if (transform_space == VR_UI::TRANSFORMSPACE_LOCAL && constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
						for (int i = 0; i < 3; ++i) {
							if (!constraint_flag[i]) {
								continue;
							}
							(*(Coord3Df*)obact->obmat[i]).normalize_in_place() *= (*(Coord3Df*)m.m[i]).length();
						}
					}
					else {
						for (int i = 0; i < 3; ++i) {
							(*(Coord3Df*)obact->obmat[i]).normalize_in_place() *= (*(Coord3Df*)m.m[i]).length();
						}
					}
					break;
				}
				case TRANSFORMMODE_OMNI: {
					memcpy(obact->obmat, m.m, sizeof(float) * 4 * 4);
				}
				default: {
					break;
				}
				}
				/* Set recalc flags. */
				DEG_id_tag_update((ID*)obact->data, 0);
			}
		}
	}

	if (snap) {
		snapped = true;
	}
	else {
		snapped = false;
	}

	if (manipulator || constraint_mode != VR_UI::CONSTRAINTMODE_NONE) {
		/* Update manipulator transform (also used when rendering constraints). */
		static VR_UI::TransformSpace prev_space = VR_UI::TRANSFORMSPACE_GLOBAL;
		if (prev_space != transform_space) {
			prev_space = transform_space;
			if (obedit) {
				BMEditMesh *em = BKE_editmesh_from_object(obedit);
				EDBM_mesh_normals_update(em);
			}
			update_manipulator();
			if (obedit) {
				manip_t_orig = manip_t * (*(Mat44f*)obedit->obmat).inverse();
			}
			else {
				manip_t_orig = manip_t;
			}
		}
		else {
			/* Don't update manipulator transformation for rotations. */
			if (transform_mode != TRANSFORMMODE_ROTATE) {
				update_manipulator();
			}
		}

		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = true;
		}
	}

	is_dragging = true;
}

void Widget_Transform::drag_stop(VR_UI::Cursor& c)
{
	/* Check if we're two-hand navi dragging */
	if (c.bimanual) {
		VR_UI::Cursor *other = c.other_hand;
		c.bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* the other hand is still dragging - we're leaving a two-hand drag. */
		other->bimanual = VR_UI::Cursor::BIMANUAL_OFF;
		/* ALSO: the other hand should start one-hand manipulating from here: */
		c.other_hand->interaction_position.set(((Mat44f)VR_UI::cursor_position_get(VR_SPACE_REAL, other->side)).m, VR_SPACE_REAL);
		/* Calculations are only performed by the second hand. */
		return;
	}

	/* TODO_XR: Avoid doing this twice (already done in drag_start() */
	if (manipulator) {
		constraint_mode = VR_UI::CONSTRAINTMODE_NONE;
		memset(constraint_flag, 0, sizeof(int) * 3);
		memset(snap_flag, 1, sizeof(int) * 3);
	}
	else {
		for (int i = 0; i < VR_SIDES; ++i) {
			Widget_Transform::obj.do_render[i] = false;
		}
	}
	if (omni) {
		transform_mode = TRANSFORMMODE_OMNI;
		snap_mode = VR_UI::SNAPMODE_TRANSLATION;
	}

	is_dragging = false;

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) { /* Edit mode */
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		EDBM_mesh_normals_update(em);
		update_manipulator();

		DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
		WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
		ED_undo_push(C, "Transform");
	}
	else { /* Object mode */
		Scene *scene = CTX_data_scene(C);
		ListBase ctx_data_list;
		CTX_data_selected_objects(C, &ctx_data_list);
		CollectionPointerLink *ctx_link = (CollectionPointerLink *)ctx_data_list.first;
		if (!ctx_link) {
			return;
		}
		for (; ctx_link; ctx_link = ctx_link->next) {
			Object *obact = (Object*)ctx_link->ptr.data;
			if (!obact) {
				continue;
			}

			/* Translation */
			Mat44f& t = *(Mat44f*)obact->obmat;
			memcpy(obact->loc, t.m[3], sizeof(float) * 3);
			/* Rotation */
			mat4_to_eul(obact->rot, t.m);
			/* Scale */
			obact->size[0] = (*(Coord3Df*)(t.m[0])).length();
			obact->size[1] = (*(Coord3Df*)(t.m[1])).length();
			obact->size[2] = (*(Coord3Df*)(t.m[2])).length();
		}
		update_manipulator();

		DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		ED_undo_push(C, "Transform");
	}
}

void Widget_Transform::render_axes(const float length[3], int draw_style)
{
	if (draw_style == 2 && !manipulator) {
		return;
	}

	/* Adapted from arrow_draw_geom() in arrow3d_gizmo.c */
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	bool unbind_shader = true;

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	/* Axes */
	GPU_line_width(1.0f);
	for (int i = 0; i < 3; ++i) {
		if (constraint_flag[i] || manipulator) {
			if (constraint_flag[i]) {
				immUniformColor4fv(c_manip_select[i]);
			}
			else {
				immUniformColor4fv(c_manip[i]);
			}
			immBegin(GPU_PRIM_LINES, 2);
			switch (i) {
			case 0: { /* x-axis */
				if (manipulator || transform_mode == TRANSFORMMODE_ROTATE) {
					immVertex3f(pos, 0.0f, 0.0f, 0.0f);
				}
				else {
					immVertex3f(pos, -length[i] , 0.0f, 0.0f);
				}
				immVertex3f(pos, length[i], 0.0f, 0.0f);
				break;
			}
			case 1: { /* y-axis */
				if (manipulator || transform_mode == TRANSFORMMODE_ROTATE) {
					immVertex3f(pos, 0.0f, 0.0f, 0.0f);
				}
				else {
					immVertex3f(pos, 0.0f, -length[i], 0.0f);
				}
				immVertex3f(pos, 0.0f, length[i], 0.0f);
				break;
			}
			case 2: { /* z-axis */
				if (manipulator || transform_mode == TRANSFORMMODE_ROTATE) {
					immVertex3f(pos, 0.0f, 0.0f, 0.0f);
				}
				else {
					immVertex3f(pos, 0.0f, 0.0f, -length[i]);
				}
				immVertex3f(pos, 0.0f, 0.0f, length[i]);
				break;
			}
			}
			immEnd();
		}
	}
		
	/* *** draw arrow head *** */
	GPU_matrix_push();

	switch (draw_style) {
	case 3: { /* Extrude Ball */
		unbind_shader = true;
		GPU_line_width(1.0f);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		float offset[3];
		if (Widget_Extrude::extrude_mode == Widget_Extrude::EXTRUDEMODE_REGION) {
			/* xyz extrude balls */
			for (int i = 0; i < 3; ++i) {
				if (Widget_Extrude::extrude && constraint_flag[i]) {
					GPU_batch_uniform_4fv(sphere, "color", c_manip_select[i]);
				}
				else {
					GPU_batch_uniform_4fv(sphere, "color", c_manip[i]);
				}
				float scale = length[i] * WIDGET_TRANSFORM_BALL_SCALE_FACTOR * 2.0f;
				switch (i) {
				case 0: { /* x-axis */
					offset[0] = length[0] + scale * 3.0f; offset[1] = 0.0f; offset[2] = 0.0f;
					break;
				}
				case 1: { /* y-axis */
					offset[0] = 0.0f; offset[1] = length[1] + scale * 3.0f; offset[2] = 0.0f;
					break;
				}
				case 2: { /* z-axis */
					offset[0] = 0.0f; offset[1] = 0.0f; offset[2] = length[2] + scale * 3.0f;
					break;
				}
				}

				GPU_matrix_translate_3fv(offset);
				GPU_matrix_scale_1f(scale);

				GPU_batch_draw(sphere);

				GPU_matrix_scale_1f(1.0f / (scale));
				*(Coord3Df*)offset *= -1.0f;
				GPU_matrix_translate_3fv(offset);
			}
		}
		else {
			/* extrude ball */
			if (Widget_Extrude::extrude && constraint_flag[2]) {
				GPU_batch_uniform_4fv(sphere, "color", c_manip_select[3]);
			}
			else {
				GPU_batch_uniform_4fv(sphere, "color", c_manip[3]);
			}
			float scale = length[2] * WIDGET_TRANSFORM_BALL_SCALE_FACTOR * 2.0f;
			offset[0] = 0.0f; offset[1] = 0.0f; offset[2] = length[2] + scale * 3.0f;

			GPU_matrix_translate_3fv(offset);
			GPU_matrix_scale_1f(scale);

			GPU_batch_draw(sphere);

			GPU_matrix_scale_1f(1.0f / (scale));
			*(Coord3Df*)offset *= -1.0f;
			GPU_matrix_translate_3fv(offset);
		}
		break;
	}
	case 2: { /* Ball */
		unbind_shader = true;
		GPU_line_width(1.0f);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		float offset[3];
		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[i]) {
				GPU_batch_uniform_4fv(sphere, "color", c_manip_select[i]);
			}
			else {
				GPU_batch_uniform_4fv(sphere, "color", c_manip[i]);
			}
			float scale = length[i] * WIDGET_TRANSFORM_BALL_SCALE_FACTOR;
			switch (i) {
			case 0: { /* x-axis */
				offset[0] = 0.0f; offset[1] = length[1] / 1.5f + scale / 2.0f; offset[2] = length[2] / 1.5f + scale / 2.0f;
				break;
			}
			case 1: { /* y-axis */
				offset[0] = length[0] / 1.5f + scale / 2.0f; offset[1] = 0.0f; offset[2] = length[2] / 1.5f + scale / 2.0f;
				break;
			}
			case 2: { /* z-axis */
				offset[0] = length[0] / 1.5f + scale / 2.0f; offset[1] = length[1] / 1.5f + scale / 2.0f; offset[2] = 0.0f;
				break;
			}
			}

			GPU_matrix_translate_3fv(offset);
			GPU_matrix_scale_1f(scale);

			GPU_batch_draw(sphere);

			GPU_matrix_scale_1f(1.0f / scale);
			*(Coord3Df*)offset *= -1.0f;
			GPU_matrix_translate_3fv(offset);
		}
		break;
	}
	case 1: { /* Box */
		static float size[3];
		for (int i = 0; i < 3; ++i) {
			size[i] = length[i] * WIDGET_TRANSFORM_BOX_SCALE_FACTOR;
		}

		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[i] || manipulator) {
				switch (i) {
				case 0: { /* x-axis */
					GPU_matrix_translate_3f(length[i] + size[i], 0.0f, 0.0f);
					GPU_matrix_rotate_axis(90.0f, 'Y');
					GPU_matrix_scale_3f(size[i], size[i], size[i]);

					immUnbindProgram();
					unbind_shader = false;
					if (constraint_flag[i]) {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[i]);
					}
					else {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[i]);
					}

					GPU_matrix_scale_3f(1.0f / size[i], 1.0f / size[i], 1.0f / size[i]);
					GPU_matrix_rotate_axis(-90.0f, 'Y');
					GPU_matrix_translate_3f(-(length[i] + size[i]), 0.0f, 0.0f);
					break;
				}
				case 1: { /* y-axis */
					GPU_matrix_translate_3f(0.0f, length[i] + size[i], 0.0f);
					GPU_matrix_rotate_axis(-90.0f, 'X');
					GPU_matrix_scale_3f(size[i], size[i], size[i]);

					if (constraint_flag[i]) {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[i]);
					}
					else {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[i]);
					}

					GPU_matrix_scale_3f(1.0f / size[i], 1.0f / size[i], 1.0f / size[i]);
					GPU_matrix_rotate_axis(90.0f, 'X');
					GPU_matrix_translate_3f(0.0f, -(length[i] + size[i]), 0.0f);
					break;
				}
				case 2: { /* z-axis */
					GPU_matrix_translate_3f(0.0f, 0.0f, length[i] + size[i]);
					GPU_matrix_scale_3f(size[i], size[i], size[i]);

					if (constraint_flag[i]) {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[i]);
					}
					else {
						wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[i]);
					}

					GPU_matrix_scale_3f(1.0f / size[i], 1.0f / size[i], 1.0f / size[i]);
					GPU_matrix_translate_3f(0.0f, 0.0f, -(length[i] + size[i]));
					break;
				}
				}
			}
		}
		/* Center scale box */
		if (omni && manipulator) {
			size[0] = length[0] * WIDGET_TRANSFORM_BOX_SCALE_FACTOR;
			GPU_matrix_scale_3f(size[0], size[0], size[0]);
			if (transform_mode == TRANSFORMMODE_SCALE && constraint_mode == VR_UI::CONSTRAINTMODE_NONE) {
				wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, true, c_manip_select[3]);
			}
			else {
				wm_gizmo_geometryinfo_draw(&wm_gizmo_geom_data_cube, false, c_manip[3]);
			}
			GPU_matrix_scale_3f(1.0f / size[0], 1.0f / size[0], 1.0f / size[0]);
		}
		break;
	}
	case 0: 
	default: { /* Arrow */
		for (int i = 0; i < 3; ++i) {
			if (constraint_flag[i] || manipulator) {
				float len = length[i] * WIDGET_TRANSFORM_ARROW_SCALE_FACTOR;
				float width = length[i] * 0.04f;
				switch (i) {
				case 0: { /* x-axis */
					if (constraint_flag[i]) {
						immUniformColor4fv(c_manip_select[i]);
					}
					else {
						immUniformColor4fv(c_manip[i]);
					}
					GPU_matrix_translate_3f(length[i], 0.0f, 0.0f);
					GPU_matrix_rotate_axis(90.0f, 'Y');

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_rotate_axis(-90.0f, 'Y');
					GPU_matrix_translate_3f(-length[i], 0.0f, 0.0f);
					break;
				}
				case 1: { /* y-axis */
					if (constraint_flag[i]) {
						immUniformColor4fv(c_manip_select[i]);
					}
					else {
						immUniformColor4fv(c_manip[i]);
					}
					GPU_matrix_translate_3f(0.0f, length[i], 0.0f);
					GPU_matrix_rotate_axis(-90.0f, 'X');

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_rotate_axis(90.0f, 'X');
					GPU_matrix_translate_3f(0.0f, -length[i], 0.0f);
					break;
				}
				case 2: { /* z-axis */
					if (constraint_flag[i]) {
						immUniformColor4fv(c_manip_select[i]);
					}
					else {
						immUniformColor4fv(c_manip[i]);
					}
					GPU_matrix_translate_3f(0.0f, 0.0f, length[i]);

					imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
					imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

					GPU_matrix_translate_3f(0.0f, 0.0f, -length[i]);
					break;
				}
				}
			}
		}
		break;
	}
	}

	GPU_matrix_pop();

	if (unbind_shader) {
		immUnbindProgram();
	}
}

void Widget_Transform::render_planes(const float length[3])
{
	/* Adapated from gizmo_primitive_draw_geom() in primitive3d_gizmo.c */

	if (!manipulator) {
		return;
	}

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	static float verts_plane[4][3] = { 0.0f };
	float len, len2;

	for (int i = 0; i < 3; ++i) {
		len = length[i] / 4.0f;
		len2 = len / 8.0f;
		verts_plane[0][0] = -len2; verts_plane[0][1] = -len2;
		verts_plane[1][0] = len2; verts_plane[1][1] = -len2;
		verts_plane[2][0] = len2; verts_plane[2][1] = len2;
		verts_plane[3][0] = -len2; verts_plane[3][1] = len2;

		switch (i) {
		case 0: { /* yz-plane */
			GPU_matrix_translate_3f(0.0f, len, len);
			GPU_matrix_rotate_axis(90.0f, 'Y');

			if (constraint_flag[1] && constraint_flag[2]) {
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}
			else {
				wm_gizmo_vec_draw(c_manip[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}

			GPU_matrix_rotate_axis(-90.0f, 'Y');
			GPU_matrix_translate_3f(0.0f, -len, -len);
			break;
		}
		case 1: { /* zx-plane */
			GPU_matrix_translate_3f(len, 0.0f, len);
			GPU_matrix_rotate_axis(90.0f, 'X');

			if (constraint_flag[0] && constraint_flag[2]) {
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}
			else {
				wm_gizmo_vec_draw(c_manip[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}

			GPU_matrix_rotate_axis(-90.0f, 'X');
			GPU_matrix_translate_3f(-len, 0.0f, -len);
			break;
		}
		case 2: { /* xy-axis */
			GPU_matrix_translate_3f(len, len, 0.0f);

			if (constraint_flag[0] && constraint_flag[1]) {
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}
			else {
				wm_gizmo_vec_draw(c_manip[i], verts_plane, 4, pos, GPU_PRIM_TRI_FAN);
				wm_gizmo_vec_draw(c_manip_select[i], verts_plane, 4, pos, GPU_PRIM_LINE_LOOP);
			}

			GPU_matrix_translate_3f(-len, -len, 0.0f);
			break;
		}
		}
	}

	immUnbindProgram();
}

void Widget_Transform::render_gimbal(
	const float radius[3],
	const bool filled,
	const float axis_modal_mat[4][4], const float clip_plane[4],
	const float arc_partial_angle, const float arc_inner_factor)
{
	/* Adapted from dial_geom_draw() in dial3d_gizmo.c */
	GPU_line_width(1.0f);
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	if (clip_plane) {
		immBindBuiltinProgram(GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR);
		immUniform4fv("ClipPlane", clip_plane);
		immUniformMatrix4fv("ModelMatrix", axis_modal_mat);
		glEnable(GL_CLIP_DISTANCE0);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	}

	float rad = 0.0f;
	for (int i = 0; i < 3; ++i) {
		if (constraint_flag[i] || manipulator) {
			if (constraint_flag[i]) {
				immUniformColor4fv(c_manip_select[i]);
			}
			else {
				immUniformColor4fv(c_manip[i]);
			}
			switch (i) { /* x-axis */
			case 0: {
				GPU_matrix_rotate_axis(-90.0f, 'Y');
				break;
			}
			case 1: { /* y-axis */
				GPU_matrix_rotate_axis(90.0f, 'X');
				break;
			}
			case 2: { /* z-axis */
				break;
			}
			}

			rad = radius[i] / 4.0f;

			if (filled) {
				imm_draw_circle_fill_2d(pos, 0, 0, rad, WIDGET_TRANSFORM_DIAL_RESOLUTION);
			}
			else {
				if (arc_partial_angle == 0.0f) {
					imm_draw_circle_wire_2d(pos, 0, 0, rad, WIDGET_TRANSFORM_DIAL_RESOLUTION);
					if (arc_inner_factor != 0.0f) {
						imm_draw_circle_wire_2d(pos, 0, 0, arc_inner_factor, WIDGET_TRANSFORM_DIAL_RESOLUTION);
					}
				}
				else {
					float arc_partial_deg = RAD2DEGF((M_PI * 2) - arc_partial_angle);
					imm_draw_circle_partial_wire_2d(
						pos, 0, 0, rad, WIDGET_TRANSFORM_DIAL_RESOLUTION,
						0.0f, arc_partial_deg);
				}
			}

			switch (i) { /* x-axis */
			case 0: {
				GPU_matrix_rotate_axis(90.0f, 'Y');
				break;
			}
			case 1: { /* y-axis */
				GPU_matrix_rotate_axis(-90.0f, 'X');
				break;
			}
			case 2: { /* z-axis */
				break;
			}
			}
		}
	}

	immUnbindProgram();

	if (clip_plane) {
		glDisable(GL_CLIP_DISTANCE0);
	}
}

/* From dial3d_gizmo.c. */
static void dial_ghostarc_draw(
	const float angle_ofs, const float angle_delta,
	const float arc_inner_factor, const float color[4], const float radius)
{
	const float width_inner = radius;
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	if (arc_inner_factor != 0.0) {
		float color_dark[4] = { 0 };
		color_dark[3] = color[3] / 2;
		immUniformColor4fv(color_dark);
		imm_draw_disk_partial_fill_2d(
			pos, 0, 0, arc_inner_factor, width_inner, WIDGET_TRANSFORM_DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(M_PI * 2));
	}

	immUniformColor4fv(color);
	imm_draw_disk_partial_fill_2d(
		pos, 0, 0, arc_inner_factor, width_inner, WIDGET_TRANSFORM_DIAL_RESOLUTION, RAD2DEGF(angle_ofs), RAD2DEGF(angle_delta));
	immUnbindProgram();
}

static void dial_ghostarc_draw_helpline(
	const float angle, const float co_outer[3], const float color[4])
{
	GPU_matrix_push();
	GPU_matrix_rotate_3f(RAD2DEGF(angle), 0.0f, 0.0f, -1.0f);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(GPU_PRIM_LINE_STRIP, 2);
	immVertex3f(pos, 0.0f, 0, 0.0f);
	immVertex3fv(pos, co_outer);
	immEnd();

	immUnbindProgram();

	GPU_matrix_pop();
}

void Widget_Transform::render_dial(
	const float angle_ofs, const float angle_delta,
	const float arc_inner_factor, const float radius)
{
	/* From dial_ghostarc_draw_with_helplines() in dial3d_gizmo.c */

	/* Coordinate at which the arc drawing will be started. */
	const float co_outer[4] = { 0.0f, radius, 0.0f };
	const float color[4] = { 0.8f, 0.8f, 0.8f, 0.4f };
	dial_ghostarc_draw(angle_ofs, angle_delta, arc_inner_factor, color, radius);
	GPU_line_width(1.0f);
	int index;
	switch (constraint_mode) {
	case VR_UI::CONSTRAINTMODE_ROT_X: {
		index = 0;
		break;
	}
	case VR_UI::CONSTRAINTMODE_ROT_Y: {
		index = 1;
		break;
	}
	case VR_UI::CONSTRAINTMODE_ROT_Z: {
		index = 2;
		break;
	}
	case VR_UI::CONSTRAINTMODE_NONE:
	default: {
		const float color_helpline[4] = { 0.4f, 0.4f, 0.4f, 0.6f };
		dial_ghostarc_draw_helpline(angle_ofs, co_outer, color_helpline);
		dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, color_helpline);
		return;
	}
	}
	dial_ghostarc_draw_helpline(angle_ofs, co_outer, c_manip_select[index]);
	dial_ghostarc_draw_helpline(angle_ofs + angle_delta, co_outer, c_manip_select[index]);
}

void Widget_Transform::render_incremental_angles(
	const float incremental_angle, const float offset, const float radius)
{
	/* From dial_ghostarc_draw_incremental_angle() in dial3d_gizmo.c */

	const int tot_incr = (2 * M_PI) / incremental_angle;
	GPU_line_width(2.0f);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3f(1.0f, 1.0f, 1.0f);
	immBegin(GPU_PRIM_LINES, tot_incr * 2);

	float v[3] = { 0 };
	for (int i = 0; i < tot_incr; i++) {
		v[0] = sinf(offset + incremental_angle * i);
		v[1] = cosf(offset + incremental_angle * i);

		mul_v2_fl(v, radius * 1.1f);
		immVertex3fv(pos, v);

		mul_v2_fl(v, 1.1f);
		immVertex3fv(pos, v);
	}

	immEnd();
	immUnbindProgram();
}

void Widget_Transform::render(VR_Side side)
{
	if (!manipulator) {
		Widget_Transform::obj.do_render[side] = false;
	}

	static float manip_length[3];
	for (int i = 0; i < 3; ++i) {
		manip_length[i] = manip_scale_factor * 2.0f;
	}
	static float clip_plane[4] = { 0.0f };

	if (omni && manipulator) {
		/* Dial and Gimbal */
		GPU_blend(true);
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_polygon_smooth(false);
		if (transform_mode == TRANSFORMMODE_ROTATE) {
			switch (constraint_mode) {
			case VR_UI::CONSTRAINTMODE_ROT_X: {
				GPU_matrix_rotate_axis(-90.0f, 'Y');
				render_dial(PI / 4.0f, manip_angle[transform_space].x, 0.0f, manip_length[0] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						render_incremental_angles(PI / 180.0f, 0.0f, manip_length[0] / 4.0f);
					}
					else {
						render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[0] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(90.0f, 'Y');
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Y: {
				GPU_matrix_rotate_axis(90.0f, 'X');
				render_dial(PI / 4.0f, manip_angle[transform_space].y, 0.0f, manip_length[1] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						render_incremental_angles(PI / 180.0f, 0.0f, manip_length[1] / 4.0f);
					}
					else {
						render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[1] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(-90.0f, 'X');
				break;
			}
			case VR_UI::CONSTRAINTMODE_ROT_Z: {
				GPU_matrix_rotate_axis(-90.0f, 'Z');
				render_dial(-PI / 4.0f, -manip_angle[transform_space].z, 0.0f, manip_length[2] / 4.0f);
				if (VR_UI::ctrl_key_get()) {
					if (VR_UI::shift_key_get()) {
						render_incremental_angles(PI / 180.0f, 0.0f, manip_length[2] / 4.0f);
					}
					else {
						render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[2] / 4.0f);
					}
				}
				GPU_matrix_rotate_axis(90.0f, 'Z');
				break;
			}
			default: {
				break;
			}
			}
		}
		render_gimbal(manip_length, false, manip_t.m, clip_plane, 3 * PI / 2.0f, 0.0f);
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 0);
		/* Box */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 1);
		/* Ball */
		render_axes(manip_length, 2);
		GPU_blend(false);
		GPU_matrix_pop();
		return;
	}

	switch (transform_mode) {
	case TRANSFORMMODE_OMNI: {
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_blend(true);
		render_axes(manip_length, 0);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case TRANSFORMMODE_MOVE: {
		/* Plane */
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_blend(true);
		render_planes(manip_length);
		/* Arrow */
		*((Coord3Df*)manip_length) /= 2.0f;
		render_axes(manip_length, 0);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case TRANSFORMMODE_ROTATE: {
		/* Dial and Gimbal */
		GPU_blend(true);
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_polygon_smooth(false);
		switch (constraint_mode) {
		case VR_UI::CONSTRAINTMODE_ROT_X: {
			GPU_matrix_rotate_axis(-90.0f, 'Y');
			render_dial(PI / 4.0f, manip_angle[transform_space].x, 0.0f, manip_length[0] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					render_incremental_angles(PI / 180.0f, 0.0f, manip_length[0] / 4.0f);
				}
				else {
					render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[0] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(90.0f, 'Y');
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Y: {
			GPU_matrix_rotate_axis(90.0f, 'X');
			render_dial(PI / 4.0f, manip_angle[transform_space].y, 0.0f, manip_length[1] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					render_incremental_angles(PI / 180.0f, 0.0f, manip_length[1] / 4.0f);
				}
				else {
					render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[1] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(-90.0f, 'X');
			break;
		}
		case VR_UI::CONSTRAINTMODE_ROT_Z: {
			GPU_matrix_rotate_axis(-90.0f, 'Z');
			render_dial(-PI / 4.0f, -manip_angle[transform_space].z, 0.0f, manip_length[2] / 4.0f);
			if (VR_UI::ctrl_key_get()) {
				if (VR_UI::shift_key_get()) {
					render_incremental_angles(PI / 180.0f, 0.0f, manip_length[2] / 4.0f);
				}
				else {
					render_incremental_angles(WIDGET_TRANSFORM_ROT_PRECISION, 0.0f, manip_length[2] / 4.0f);
				}
			}
			GPU_matrix_rotate_axis(90.0f, 'Z');
			break;
		}
		default: {
			break;
		}
		}
		if (!manipulator) {
			render_gimbal(manip_length, false, manip_t.m, clip_plane, 0.0f, 0.0f);
		}
		else {
			render_gimbal(manip_length, false, manip_t.m, clip_plane, 3 * PI / 2.0f, 0.0f);
		}
		/* Ball */
		*((Coord3Df*)manip_length) /= 4.0f;
		render_axes(manip_length, 2);
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	case TRANSFORMMODE_SCALE: {
		/* Plane */
		GPU_matrix_push();
		GPU_matrix_mul(manip_t.m);
		GPU_blend(true);
		render_planes(manip_length);
		/* Box */
		*((Coord3Df*)manip_length) /= 4.0f;
		render_axes(manip_length, 1);
		/* TODO_XR */
		static float zero[4][4] = { 0 };
		GPU_matrix_mul(zero);
		GPUBatch *sphere = GPU_batch_preset_sphere(0);
		GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
		GPU_batch_draw(sphere);
		/**/
		GPU_blend(false);
		GPU_matrix_pop();
		break;
	}
	default: {
		break;
	}
	}
}
