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

/** \file blender/vr/intern/vr_widget_duplicate.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_duplicate.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

#include "BLI_listbase.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_gpencil.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lamp.h"
#include "BKE_library.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_speaker.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_undo.h"

#include "mesh_intern.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/***********************************************************************************************//**
 * \class                               Widget_Duplicate
 ***************************************************************************************************
* Interaction widget for performing a 'duplicate' operation.
*
**************************************************************************************************/
Widget_Duplicate Widget_Duplicate::obj;

/* Dummy op to pass to edbm_duplicate_exec() */
static wmOperator duplicate_dummy_op;

/* From object_add.c */
static void copy_object_set_idnew(bContext *C)
{
	Main *bmain = CTX_data_main(C);

	ListBase ctx_data_list;
	CollectionPointerLink *ctx_link;
	CTX_data_selected_editable_objects(C, &ctx_data_list);
	for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next) {
		Object *ob = (Object*)ctx_link->ptr.data;
		BKE_libblock_relink_to_newid(&ob->id);
	}
	BLI_freelistN(&ctx_data_list);

	BKE_main_id_clear_newpoins(bmain);
}

/* From object_add.c */
/* used below, assumes id.new is correct */
/* leaves selection of base/object unaltered */
/* Does set ID->newid pointers. */
static Base *object_add_duplicate_internal(Main *bmain, Scene *scene, ViewLayer *view_layer, Object *ob, int dupflag)
{
#define ID_NEW_REMAP_US(a, type) if (      (a)->id.newid) { (a) = (type *)(a)->id.newid;       (a)->id.us++; }
#define ID_NEW_REMAP_US2(a)	if (((ID *)a)->newid)    { (a) = ((ID  *)a)->newid;     ((ID *)a)->us++;    }

	Base *base, *basen = NULL;
	Material ***matarar;
	Object *obn;
	ID *id;
	int a, didit;

	if (ob->mode & OB_MODE_POSE) {
		; /* nothing? */
	}
	else {
		obn = (Object*)ID_NEW_SET(ob, BKE_object_copy(bmain, ob));
		DEG_id_tag_update(&obn->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

		base = BKE_view_layer_base_find(view_layer, ob);
		if ((base != NULL) && (base->flag & BASE_VISIBLE)) {
			BKE_collection_object_add_from(bmain, scene, ob, obn);
		}
		else {
			LayerCollection *layer_collection = BKE_layer_collection_get_active(view_layer);
			BKE_collection_object_add(bmain, layer_collection->collection, obn);
		}
		basen = BKE_view_layer_base_find(view_layer, obn);

		/* 1) duplis should end up in same collection as the original
		 * 2) Rigid Body sim participants MUST always be part of a collection...
		 */
		 // XXX: is 2) really a good measure here?
		if (ob->rigidbody_object || ob->rigidbody_constraint) {
			Collection *collection;
			for (collection = (Collection*)bmain->collection.first; collection; collection = (Collection*)collection->id.next) {
				if (BKE_collection_has_object(collection, ob))
					BKE_collection_object_add(bmain, collection, obn);
			}
		}

		/* duplicates using userflags */
		if (dupflag & USER_DUP_ACT) {
			BKE_animdata_copy_id_action(bmain, &obn->id, true);
		}

		if (dupflag & USER_DUP_MAT) {
			for (a = 0; a < obn->totcol; a++) {
				id = (ID *)obn->mat[a];
				if (id) {
					ID_NEW_REMAP_US(obn->mat[a], Material)
				else {
					obn->mat[a] = (Material*)ID_NEW_SET(obn->mat[a], BKE_material_copy(bmain, obn->mat[a]));
					/* duplicate grease pencil settings */
					if (ob->mat[a]->gp_style) {
						obn->mat[a]->gp_style = (MaterialGPencilStyle*)MEM_dupallocN(ob->mat[a]->gp_style);
					}
				}
				id_us_min(id);

				if (dupflag & USER_DUP_ACT) {
					BKE_animdata_copy_id_action(bmain, &obn->mat[a]->id, true);
				}
				}
			}
		}
		if (dupflag & USER_DUP_PSYS) {
			ParticleSystem *psys;
			for (psys = (ParticleSystem*)obn->particlesystem.first; psys; psys = psys->next) {
				id = (ID *)psys->part;
				if (id) {
					ID_NEW_REMAP_US(psys->part, ParticleSettings)
				else {
					psys->part = (ParticleSettings*)ID_NEW_SET(psys->part, BKE_particlesettings_copy(bmain, psys->part));
				}

				if (dupflag & USER_DUP_ACT) {
					BKE_animdata_copy_id_action(bmain, &psys->part->id, true);
				}

				id_us_min(id);
				}
			}
		}

		id = (ID*)obn->data;
		didit = 0;

		switch (obn->type) {
		case OB_MESH:
			if (dupflag & USER_DUP_MESH) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_mesh_copy(bmain, (const Mesh*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_CURVE:
			if (dupflag & USER_DUP_CURVE) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, (const Curve*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_SURF:
			if (dupflag & USER_DUP_SURF) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, (const Curve*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_FONT:
			if (dupflag & USER_DUP_FONT) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, (const Curve*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_MBALL:
			if (dupflag & USER_DUP_MBALL) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_mball_copy(bmain, (const MetaBall*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_LAMP:
			if (dupflag & USER_DUP_LAMP) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_lamp_copy(bmain, (const Lamp*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_ARMATURE:
			DEG_id_tag_update(&obn->id, ID_RECALC_GEOMETRY);
			if (obn->pose)
				BKE_pose_tag_recalc(bmain, obn->pose);
			if (dupflag & USER_DUP_ARM) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_armature_copy(bmain, (const bArmature*)obn->data));
				BKE_pose_rebuild(bmain, obn, (bArmature*)obn->data, true);
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_LATTICE:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_lattice_copy(bmain, (const Lattice*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_CAMERA:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_camera_copy(bmain, (const Camera*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_SPEAKER:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_speaker_copy(bmain, (const Speaker*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		case OB_GPENCIL:
			if (dupflag != 0) {
				ID_NEW_REMAP_US2(obn->data)
			else {
				obn->data = ID_NEW_SET(obn->data, BKE_gpencil_copy(bmain, (const bGPdata*)obn->data));
				didit = 1;
			}
			id_us_min(id);
			}
			break;
		}

		/* check if obdata is copied */
		if (didit) {
			Key *key = BKE_key_from_object(obn);

			Key *oldkey = BKE_key_from_object(ob);
			if (oldkey != NULL) {
				ID_NEW_SET(oldkey, key);
			}

			if (dupflag & USER_DUP_ACT) {
				BKE_animdata_copy_id_action(bmain, (ID *)obn->data, true);
				if (key) {
					BKE_animdata_copy_id_action(bmain, (ID *)key, true);
				}
			}

			if (dupflag & USER_DUP_MAT) {
				matarar = give_matarar(obn);
				if (matarar) {
					for (a = 0; a < obn->totcol; a++) {
						id = (ID *)(*matarar)[a];
						if (id) {
							ID_NEW_REMAP_US((*matarar)[a], Material)
						else {
							(*matarar)[a] = (Material*)ID_NEW_SET((*matarar)[a], BKE_material_copy(bmain, (*matarar)[a]));
						}
						id_us_min(id);
						}
					}
				}
			}
		}
	}
	return basen;

#undef ID_NEW_REMAP_US
#undef ID_NEW_REMAP_US2
}

/* From duplicate_exec() in object_add.c */
static int duplicate_selected_objects(bool linked = true)
{
	bContext *C = vr_get_obj()->ctx;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	int dupflag = (linked) ? 0 : U.dupflag;

	ListBase ctx_data_list;
	CollectionPointerLink *ctx_link;
	CTX_data_selected_bases(C, &ctx_data_list);
	for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next) {
		Base *base = (Base*)ctx_link->ptr.data;

		Base *basen = object_add_duplicate_internal(bmain, scene, view_layer, base->object, dupflag);

		/* note that this is safe to do with this context iterator,
		 * the list is made in advance */
		ED_object_base_select(base, BA_DESELECT);
		ED_object_base_select(basen, BA_SELECT);

		if (basen == NULL) {
			continue;
		}

		/* new object becomes active */
		if (BASACT(view_layer) == base)
			ED_object_base_activate(C, basen);

		if (basen->object->data) {
			DEG_id_tag_update((ID*)basen->object->data, 0);
		}
	}
	BLI_freelistN(&ctx_data_list);

	copy_object_set_idnew(C);

	BKE_main_id_clear_newpoins(bmain);

	DEG_relations_tag_update(bmain);
	DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_undo_push(C, "Duplicate");

	return 0;
}

/* From editmesh_tools.c */
static int edbm_duplicate_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (em->bm->totvertsel == 0) {
			continue;
		}

		BMOperator bmop;
		BMesh *bm = em->bm;

		EDBM_op_init(
			em, &bmop, op,
			"duplicate geom=%hvef use_select_history=%b",
			BM_ELEM_SELECT, true);

		BMO_op_exec(bm, &bmop);

		/* de-select all would clear otherwise */
		BM_SELECT_HISTORY_BACKUP(bm);

		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

		BMO_slot_buffer_hflag_enable(bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);

		/* rebuild editselection */
		BM_SELECT_HISTORY_RESTORE(bm);

		if (!EDBM_op_finish(em, &bmop, op, true)) {
			continue;
		}
		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);
	ED_undo_push(C, "Duplicate");

	return OPERATOR_FINISHED;
}

bool Widget_Duplicate::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Duplicate::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) {
		edbm_duplicate_exec(C, &duplicate_dummy_op);
	}
	else {
		duplicate_selected_objects();
	}

	/* Update manipulators */
	Widget_Transform::update_manipulator();
}

bool Widget_Duplicate::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Duplicate::render_icon(const Mat44f& t, VR_Side controller_side, bool active, bool touched)
{
	if (touched) {
		const Mat44f& t_touched = m_widget_touched * t;
		VR_Draw::update_modelview_matrix(&t_touched, 0);
	}
	else {
		VR_Draw::update_modelview_matrix(&t, 0);
	}
	if (active) {
		VR_Draw::set_color(1.0f, 0.0f, 0.0f, 1.0f);
	}
	else {
		VR_Draw::set_color(1.0f, 1.0f, 1.0f, 1.0f);
	}
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::duplicate_tex);
}
