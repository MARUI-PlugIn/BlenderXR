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

/** \file blender/vr/intern/vr_widget_knife.cpp
*   \ingroup vr
*
*/

#include "vr_types.h"
#include <list>
#include "vr_main.h"
#include "vr_ui.h"
#include "vr_widget_knife.h"
#include "vr_widget_transform.h"
#include "vr_math.h"

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include "MEM_guardedalloc.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"
#include "ED_screen.h"
#include "ED_undo.h"
#include "WM_api.h"
#include "WM_types.h"
#include "DNA_mesh_types.h"
#include "DEG_depsgraph.h"
#include "wm_event_system.h"

#include "vr_util.h"

/* From editors/mesh/editmesh_knife.c */
extern void EDBM_mesh_knife(bContext *C, LinkNode *polys, bool use_tag, bool cut_through);
extern void MESH_OT_knife_tool(wmOperatorType *ot);
extern wmOperatorType *WM_operatortype_find(const char *idname, bool quiet);
enum {
	KNF_MODAL_CANCEL = 1,
	KNF_MODAL_CONFIRM,
	KNF_MODAL_MIDPOINT_ON,
	KNF_MODAL_MIDPOINT_OFF,
	KNF_MODAL_NEW_CUT,
	KNF_MODEL_IGNORE_SNAP_ON,
	KNF_MODEL_IGNORE_SNAP_OFF,
	KNF_MODAL_ADD_CUT,
	KNF_MODAL_ANGLE_SNAP_TOGGLE,
	KNF_MODAL_CUT_THROUGH_TOGGLE,
	KNF_MODAL_PANNING,
	KNF_MODAL_ADD_CUT_CLOSED,
};

/***********************************************************************************************//**
 * \class									Widget_Knife
 ***************************************************************************************************
 * Interaction widget for the Knife tool.
 * This widget works by sending simulated mouse events to the knife tool implemented in
 * editors/mesh/editmesh_knife.c
 **************************************************************************************************/

Widget_Knife Widget_Knife::obj;
Coord3Df Widget_Knife::p0;
Coord3Df Widget_Knife::p1;

static wmOperator knife_dummy_op; /* Dummy operator, used to send events to the knife tool. */

bool Widget_Knife::has_click(VR_UI::Cursor& c) const
{
	return true;
}

void Widget_Knife::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	const Mat44f& m = c.position.get();
	if (CTX_data_edit_object(vr_get_obj()->ctx)) {
		VR_Util::raycast_select_single_edit(*(Coord3Df*)m.m[3], VR_UI::shift_key_get(), VR_UI::ctrl_key_get());
	}

	/* Update manipulators */
	Widget_Transform::update_manipulator();
}

void Widget_Knife::drag_start(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	if (c.bimanual) {
		return;
	}

	p1 = p0 = *(Coord3Df*)c.interaction_position.get(VR_SPACE_BLENDER).m[3];

	/* Start knife tool operation */
    if (knife_dummy_op.type == 0) {
        knife_dummy_op.type = WM_operatortype_find("MESH_OT_knife_tool", true);
        if (knife_dummy_op.type == 0) {
		    return;
        }
    }
    if (knife_dummy_op.ptr == 0) {
        knife_dummy_op.ptr = (PointerRNA*)MEM_callocN(sizeof(PointerRNA), __func__);
        if (knife_dummy_op.ptr == 0) {
		    return;
        }
        WM_operator_properties_create_ptr(knife_dummy_op.ptr, knife_dummy_op.type);
	    WM_operator_properties_sanitize(knife_dummy_op.ptr, 0);
    }
    if (knife_dummy_op.reports == 0) {
        knife_dummy_op.reports = (ReportList*)MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
        if (knife_dummy_op.reports == 0) {
		    return;
        }
	    BKE_reports_init(knife_dummy_op.reports, RPT_STORE | RPT_FREE);
    }

    ARegion* ar = CTX_wm_region(C);
    RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	
    float projmat[4][4];
    mul_m4_m4m4(projmat, (float (*)[4])rv3d->winmat, (float (*)[4])rv3d->viewmat);
    float in[4] = {p0.x, p0.y, p0.z, 1};
    // mul_project_m4_v3(projmat, v0);
    float v0[4];
	mul_v4_m4v3(v0, projmat, in);
    if (v0[3] == 0) return;
    v0[0] = (float(ar->winx) / 2.0f) + (float(ar->winx) / 2.0f) * (v0[0]/v0[3]);
	v0[1] = (float(ar->winy) / 2.0f) + (float(ar->winy) / 2.0f) * (v0[1]/v0[3]);

    wmEvent event;
    memset(&event, 0, sizeof(wmEvent));
	event.x = v0[0];
    event.y = v0[1];
	event.mval[0] = v0[0];
    event.mval[1] = v0[1];

    event.type = LEFTMOUSE;
	event.val = KM_PRESS;
    knife_dummy_op.type->invoke(C, &knife_dummy_op, &event);

    event.type = EVT_MODAL_MAP;
    event.val = KNF_MODAL_ADD_CUT;
    knife_dummy_op.type->modal(C, &knife_dummy_op, &event);

    /* The knifetool installs an eventhandler for modal that we don't need or want.
     *  Find it and remove it... */
    wmWindow *win = CTX_wm_window(C);
    wmEventHandler* handler = (wmEventHandler*)win->modalhandlers.first;
    if (handler->op == &knife_dummy_op) {
        BLI_remlink(&win->modalhandlers, handler);
    }

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Knife::obj.do_render[i] = true;
	}
}

void Widget_Knife::drag_contd(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
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
	} else {
		return;
	}

	if (c.bimanual) {
		return;
	}

	p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];


	/* Continue knife operation */
    ARegion* ar = CTX_wm_region(C);
    RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	
    float projmat[4][4];
    mul_m4_m4m4(projmat, (float (*)[4])rv3d->winmat, (float (*)[4])rv3d->viewmat);
    float in[4] = {p1.x, p1.y, p1.z, 1};
    // mul_project_m4_v3(projmat, v0);
    float v0[4];
	mul_v4_m4v3(v0, projmat, in);
    if (v0[3] == 0) return;
    v0[0] = (float(ar->winx) / 2.0f) + (float(ar->winx) / 2.0f) * (v0[0]/v0[3]);
	v0[1] = (float(ar->winy) / 2.0f) + (float(ar->winy) / 2.0f) * (v0[1]/v0[3]);

    wmEvent event;
    memset(&event, 0, sizeof(wmEvent));
	event.x = v0[0];
    event.y = v0[1];
	event.mval[0] = v0[0];
    event.mval[1] = v0[1];

    event.type = MOUSEMOVE;
	event.val = KM_RELEASE;
	if (knife_dummy_op.type && knife_dummy_op.customdata) {
		knife_dummy_op.type->modal(C, &knife_dummy_op, &event);
	}

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Knife::obj.do_render[i] = true;
	}
}

void Widget_Knife::drag_stop(VR_UI::Cursor& c)
{
	if (c.bimanual) {
		return;
	}

	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	if (!knife_dummy_op.type || !knife_dummy_op.customdata) {
		return;
	}

	/* Finish knife operation */
	p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];

    ARegion* ar = CTX_wm_region(C);
    RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	
    float projmat[4][4];
    mul_m4_m4m4(projmat, (float (*)[4])rv3d->winmat, (float (*)[4])rv3d->viewmat);
    float in[4] = {p1.x, p1.y, p1.z, 1};
    // mul_project_m4_v3(projmat, v0);
    float v0[4];
	mul_v4_m4v3(v0, projmat, in);
    if (v0[3] == 0) return;
    v0[0] = (float(ar->winx) / 2.0f) + (float(ar->winx) / 2.0f) * (v0[0]/v0[3]);
	v0[1] = (float(ar->winy) / 2.0f) + (float(ar->winy) / 2.0f) * (v0[1]/v0[3]);

    wmEvent event;
    memset(&event, 0, sizeof(wmEvent));
	event.x = v0[0];
    event.y = v0[1];
	event.mval[0] = v0[0];
    event.mval[1] = v0[1];

    event.type = EVT_MODAL_MAP;
    event.val = KNF_MODAL_ADD_CUT;
	//if (knife_dummy_op.type && knife_dummy_op.customdata) {
		knife_dummy_op.type->modal(C, &knife_dummy_op, &event);
	//}

    event.type = EVT_MODAL_MAP;
    event.val = KNF_MODAL_CONFIRM;
	//if (knife_dummy_op.type && knife_dummy_op.customdata) {
		knife_dummy_op.type->modal(C, &knife_dummy_op, &event);
	//}
	//if (knife_dummy_op.ptr) {
		WM_operator_properties_free(knife_dummy_op.ptr);
		knife_dummy_op.ptr = 0;
	//}
	//if (knife_dummy_op.reports) {
		BKE_reports_clear(knife_dummy_op.reports);
		MEM_freeN(knife_dummy_op.reports);
		knife_dummy_op.reports = 0;
	//}

    /*
    //Scene *scene = CTX_data_scene(C);
	//Object *obedit = CTX_data_edit_object(C);
    ARegion* ar = CTX_wm_region(C);
    RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	
    float (*mval_fl)[2] = (float(*)[2])MEM_mallocN(sizeof(float)*2*2, "BlenderXR Knife Tool tmp screencoords");

    float projmat[4][4];
    mul_m4_m4m4(projmat, (float (*)[4])rv3d->winmat, (float (*)[4])rv3d->viewmat);
    float v0[3] = {p0.x, p0.y, p0.z};
    mul_project_m4_v3(projmat, v0);
    float v1[3] = {p1.x, p1.y, p1.z};
    mul_project_m4_v3(projmat, v1);
	mval_fl[0][0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * v0[0];
	mval_fl[0][1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * v0[1];
	mval_fl[1][0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * v1[0];
	mval_fl[1][1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * v1[1];

    LinkNode cut_screencoords;
    cut_screencoords.link = mval_fl;
    cut_screencoords.next = 0;
    EDBM_mesh_knife(C, &cut_screencoords, false, false);

	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	EDBM_mesh_normals_update(em);
	Widget_Transform::update_manipulator();

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Knife::obj.do_render[i] = false;
	}
    */

    DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	ED_undo_push(C, "Knife");
}
