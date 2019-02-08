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

/** \file blender/vr/intern/vr_widget_delete.cpp
*   \ingroup vr
* 
* Main module for the VR widget UI.
*/

#include "vr_types.h"
#include <list>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_delete.h"
#include "vr_widget_transform.h"

#include "vr_draw.h"

#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "DNA_gpencil_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_undo.h"

#include "mesh_intern.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/***********************************************************************************************//**
 * \class                               Widget_Delete
 ***************************************************************************************************
 * Interaction widget for performing a 'delete' operation.
 *
 **************************************************************************************************/
Widget_Delete Widget_Delete::obj;

/* Dummy op to pass to EDBM_op_callf() */
static wmOperator delete_dummy_op;

/* From object_delete_exec() in object_add.c */
static int delete_selected_objects(bool use_global = true)
{
	bContext *C = vr_get_obj()->ctx;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;
	bool changed = false;

	if (CTX_data_edit_object(C)) {
		return -1;
	}

	ListBase ctx_data_list;
	CollectionPointerLink *ctx_link;
	CTX_data_selected_objects(C, &ctx_data_list);
	for (ctx_link = (CollectionPointerLink*)ctx_data_list.first; ctx_link; ctx_link = ctx_link->next) {
		Object *ob = (Object*)ctx_link->ptr.data;
		const bool is_indirectly_used = BKE_library_ID_is_indirectly_used(bmain, ob);
		if (ob->id.tag & LIB_TAG_INDIRECT) {
			/* Can this case ever happen? */
			continue;
		}
		else if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
			continue;
		}

		/* if grease pencil object, set cache as dirty */
		if (ob->type == OB_GPENCIL) {
			bGPdata *gpd = (bGPdata *)ob->data;
			DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
		}

		/* This is sort of a quick hack to address T51243 - Proper thing to do here would be to nuke most of all this
		 * custom scene/object/base handling, and use generic lib remap/query for that.
		 * But this is for later (aka 2.8, once layers & co are settled and working).
		 */
		if (use_global && ob->id.lib == NULL) {
			/* We want to nuke the object, let's nuke it the easy way (not for linked data though)... */
			BKE_id_delete(bmain, &ob->id);
			changed = true;
			continue;
		}

		/* remove from Grease Pencil parent */
		/* XXX This is likely not correct? Will also remove parent from grease pencil from other scenes,
		 *     even when use_global is false... */
		for (bGPdata *gpd = (bGPdata*)bmain->gpencil.first; gpd; gpd = (bGPdata*)gpd->id.next) {
			for (bGPDlayer *gpl = (bGPDlayer*)gpd->layers.first; gpl; gpl = gpl->next) {
				if (gpl->parent != NULL) {
					if (gpl->parent == ob) {
						gpl->parent = NULL;
					}
				}
			}
		}

		/* remove from current scene only */
		ED_object_base_free_and_unlink(bmain, scene, ob);
		changed = true;

		if (use_global) {
			Scene *scene_iter;
			for (scene_iter = (Scene*)bmain->scene.first; scene_iter; scene_iter = (Scene*)scene_iter->id.next) {
				if (scene_iter != scene && !ID_IS_LINKED(scene_iter)) {
					if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
						break;
					}
					ED_object_base_free_and_unlink(bmain, scene_iter, ob);
				}
			}
		}
		/* end global */
	}
	BLI_freelistN(&ctx_data_list);

	if (!changed) {
		return -1;
	}

	/* delete has to handle all open scenes */
	BKE_main_id_tag_listbase(&bmain->scene, LIB_TAG_DOIT, true);
	for (win = (wmWindow*)wm->windows.first; win; win = win->next) {
		scene = WM_window_get_active_scene(win);

		if (scene->id.tag & LIB_TAG_DOIT) {
			scene->id.tag &= ~LIB_TAG_DOIT;

			DEG_relations_tag_update(bmain);

			DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
			WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
		}
	}
	ED_undo_push(C, "Delete");

	return 0;
}

/* Note, these values must match delete_mesh() event values */
enum {
	MESH_DELETE_VERT = 0,
	MESH_DELETE_EDGE = 1,
	MESH_DELETE_FACE = 2,
	MESH_DELETE_EDGE_FACE = 3,
	MESH_DELETE_ONLY_FACE = 4,
};

/* From editmesh_tools.c */
static int edbm_delete_exec(bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	static ObjectsInModeParams params = { OB_MODE_EDIT, true };
	Object **objects = BKE_view_layer_array_from_objects_in_mode_params(view_layer, CTX_wm_view3d(C), &objects_len, &params);
	bool changed_multi = false;

	ToolSettings *ts = CTX_data_scene(C)->toolsettings;
	int type;
	/* TODO_XR: Multi-select mode. */
	switch (ts->selectmode) {
	case SCE_SELECT_VERTEX: {
		type = MESH_DELETE_VERT;
		break;
	}
	case SCE_SELECT_EDGE: {
		type = MESH_DELETE_EDGE;
		break;
	}
	case SCE_SELECT_FACE: {
		type = MESH_DELETE_FACE;
		break;
	}
	default: {
		break;
	}
	}

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		switch (type) {
		case MESH_DELETE_VERT: /* Erase Vertices */
			if (!(em->bm->totvertsel &&
				EDBM_op_callf(em, &delete_dummy_op, "delete geom=%hv context=%i", BM_ELEM_SELECT, DEL_VERTS)))
			{
				continue;
			}
			break;
		case MESH_DELETE_EDGE: /* Erase Edges */
			if (!(em->bm->totedgesel &&
				EDBM_op_callf(em, &delete_dummy_op, "delete geom=%he context=%i", BM_ELEM_SELECT, DEL_EDGES)))
			{
				continue;
			}
			break;
		case MESH_DELETE_FACE: /* Erase Faces */
			if (!(em->bm->totfacesel &&
				EDBM_op_callf(em, &delete_dummy_op, "delete geom=%hf context=%i", BM_ELEM_SELECT, DEL_FACES)))
			{
				continue;
			}
			break;
		case MESH_DELETE_EDGE_FACE:
			/* Edges and Faces */
			if (!((em->bm->totedgesel || em->bm->totfacesel) &&
				EDBM_op_callf(em, &delete_dummy_op, "delete geom=%hef context=%i", BM_ELEM_SELECT, DEL_EDGESFACES)))
			{
				continue;
			}
			break;
		case MESH_DELETE_ONLY_FACE:
			/* Only faces. */
			if (!(em->bm->totfacesel &&
				EDBM_op_callf(em, &delete_dummy_op, "delete geom=%hf context=%i", BM_ELEM_SELECT, DEL_ONLYFACES)))
			{
				continue;
			}
			break;
		default:
			BLI_assert(0);
			break;
		}

		changed_multi = true;

		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

		EDBM_update_generic(em, true, true);
	}

	MEM_freeN(objects);
	if (changed_multi) {
		ED_undo_push(C, "Delete");
	}
	return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

bool Widget_Delete::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Delete::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (obedit) {
		edbm_delete_exec(C);
	}
	else {
		delete_selected_objects();
	}

	/* Update manipulators */
	Widget_Transform::update_manipulator();
}

bool Widget_Delete::has_drag(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Delete::render_icon(const Mat44f& t, VR_Side controller_side,  bool active, bool touched)
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
	VR_Draw::render_rect(-0.009f, 0.009f, 0.009f, -0.009f, 0.001f, 1.0f, 1.0f, VR_Draw::delete_tex);
}
