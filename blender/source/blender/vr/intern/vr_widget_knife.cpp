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
#include "vr_draw.h"
#include "vr_util.h"

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"
#include "BLI_memarena.h"

#include "BLT_translation.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_report.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h"  /* own include */

#include "wm_event_system.h"

extern void EDBM_mesh_knife(bContext *C, LinkNode *polys, bool use_tag, bool cut_through);
extern void MESH_OT_knife_tool(wmOperatorType *ot);
extern void WM_operatortype_append(void (*opfunc)(wmOperatorType *));
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
 *
 **************************************************************************************************/
Widget_Knife Widget_Knife::obj;

Coord3Df Widget_Knife::p0;
Coord3Df Widget_Knife::p1;
VR_Side Widget_Knife::cursor_side;

/* Dummy op */
static wmOperator knife_dummy_op;

bool Widget_Knife::has_click(VR_UI::Cursor& c) const
{
	return false;
}

void Widget_Knife::click(VR_UI::Cursor& c)
{
	bContext *C = vr_get_obj()->ctx;
	Object *obedit = CTX_data_edit_object(C);
	if (!obedit) {
		return;
	}

	p0 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
	

    

    // knifetool_invoke(C, &op, &event);

	DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
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

	cursor_side = c.side;
	p1 = p0 = *(Coord3Df*)c.interaction_position.get(VR_SPACE_BLENDER).m[3];

	// Start knife tool operation
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
    event.next = event.prev = 0;
    event.type = LEFTMOUSE;
	event.val = KM_PRESS; // KM_CLICK;
	event.x = v0[0];
    event.y = v0[1];
	event.mval[0] = v0[0];
    event.mval[1] = v0[1];
	for (int i=7; i>=0; i--) {
        event.utf8_buf[i] = 0;
    }
    event.ascii = 0;
	event.pad = 0;

    event.prevtype = 0;
	event.prevval = 0;
	event.prevx = event.prevy = 0;
	event.prevclicktime = 0;
	event.prevclickx = event.prevclicky = 0;
    event.shift = event.ctrl = event.alt = event.oskey = 0;
	event.keymodifier = 0;

	/* set in case a KM_PRESS went by unhandled */
	event.check_click = 0;
	event.check_drag = 0;
	event.is_motion_absolute = 0;

	/* keymap item, set by handler (weak?) */
	event.keymap_idname = 0;

	/* tablet info, only use when the tablet is active */
	event.tablet_data = 0;

	/* custom data */
	event.custom = 0;		/* custom data type, stylus, 6dof, see wm_event_types.h */
	event.customdatafree = 0;
	event.pad2 = 0;
	event.customdata = 0;	/* ascii, unicode, mouse coords, angles, vectors, dragdrop info */
    /*
    const EnumPropertyItem rna_enum_operator_property_tags[] = {
	    {OP_PROP_TAG_ADVANCED, "ADVANCED", 0, "Advanced", "The property is advanced so UI is suggested to hide it"},
	    {0, NULL, 0, NULL, NULL}
    };
    knife_dummy_op.type = (wmOperatorType*)MEM_callocN(sizeof(wmOperatorType), "operatortype");
    knife_dummy_op.type->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_OperatorProperties);
	RNA_def_struct_property_tags(knife_dummy_op.type->srna, rna_enum_operator_property_tags);
	/ * Set the default i18n context now, so that opfunc can redefine it if needed! * /
	RNA_def_struct_translation_context(knife_dummy_op.type->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
	knife_dummy_op.type->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;

    MESH_OT_knife_tool(knife_dummy_op.type);

    WM_operatortype_props_advanced_end(knife_dummy_op.type);
	RNA_def_struct_ui_text(knife_dummy_op.type->srna, knife_dummy_op.type->name, knife_dummy_op.type->description ? knife_dummy_op.type->description : N_("(undocumented operator)"));
	RNA_def_struct_identifier(&BLENDER_RNA, knife_dummy_op.type->srna, knife_dummy_op.type->idname);

	//BLI_ghash_insert(global_ops_hash, (void *)knife_dummy_op.type->idname, knife_dummy_op.type);

    knife_dummy_op.type->invoke(C, &knife_dummy_op, &event); */
    if (knife_dummy_op.type == 0) {
        knife_dummy_op.type = WM_operatortype_find("MESH_OT_knife_tool", true);
    }
    if (knife_dummy_op.ptr == 0) {
        knife_dummy_op.ptr = (PointerRNA*)MEM_callocN(sizeof(PointerRNA), __func__);
        WM_operator_properties_create_ptr(knife_dummy_op.ptr, knife_dummy_op.type);
	    WM_operator_properties_sanitize(knife_dummy_op.ptr, 0);
    }
    if (knife_dummy_op.reports == 0) {
        knife_dummy_op.reports = (ReportList*)MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
	    BKE_reports_init(knife_dummy_op.reports, RPT_STORE | RPT_FREE);
    }
    knife_dummy_op.type->invoke(C, &knife_dummy_op, &event);


    event.type = EVT_MODAL_MAP;
    event.val = KNF_MODAL_ADD_CUT;
    knife_dummy_op.type->modal(C, &knife_dummy_op, &event);

    // knifetool installs an eventhandler for modal that we don't need or want
    // find it and remove it
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


	// Continue knife operation
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
    event.next = event.prev = 0;
    event.type = LEFTMOUSE;
	event.val = KM_RELEASE;
	event.x = v0[0];
    event.y = v0[1];
	event.mval[0] = v0[0];
    event.mval[1] = v0[1];
	for (int i=7; i>=0; i--) {
        event.utf8_buf[i] = 0;
    }
    event.ascii = 0;
	event.pad = 0;

    event.prevtype = 0;
	event.prevval = 0;
	event.prevx = event.prevy = 0;
	event.prevclicktime = 0;
	event.prevclickx = event.prevclicky = 0;
    event.shift = event.ctrl = event.alt = event.oskey = 0;
	event.keymodifier = 0;

	/* set in case a KM_PRESS went by unhandled */
	event.check_click = 0;
	event.check_drag = 0;
	event.is_motion_absolute = 0;

	/* keymap item, set by handler (weak?) */
	event.keymap_idname = 0;

	/* tablet info, only use when the tablet is active */
	event.tablet_data = 0;

	/* custom data */
	event.custom = 0;		/* custom data type, stylus, 6dof, see wm_event_types.h */
	event.customdatafree = 0;
	event.pad2 = 0;
	event.customdata = 0;	/* ascii, unicode, mouse coords, angles, vectors, dragdrop info */
    
    event.type = MOUSEMOVE;
    knife_dummy_op.type->modal(C, &knife_dummy_op, &event);

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

	// Finish knife operation
	p1 = *(Coord3Df*)c.position.get(VR_SPACE_BLENDER).m[3];
	
    // Continue knife operation
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
    event.next = event.prev = 0;
    event.type = EVT_MODAL_MAP;
    event.val = KNF_MODAL_ADD_CUT;
	event.x = v0[0];
    event.y = v0[1];
	event.mval[0] = v0[0];
    event.mval[1] = v0[1];
	for (int i=7; i>=0; i--) {
        event.utf8_buf[i] = 0;
    }
    event.ascii = 0;
	event.pad = 0;

    event.prevtype = 0;
	event.prevval = 0;
	event.prevx = event.prevy = 0;
	event.prevclicktime = 0;
	event.prevclickx = event.prevclicky = 0;
    event.shift = event.ctrl = event.alt = event.oskey = 0;
	event.keymodifier = 0;

	/* set in case a KM_PRESS went by unhandled */
	event.check_click = 0;
	event.check_drag = 0;
	event.is_motion_absolute = 0;

	/* keymap item, set by handler (weak?) */
	event.keymap_idname = 0;

	/* tablet info, only use when the tablet is active */
	event.tablet_data = 0;

	/* custom data */
	event.custom = 0;		/* custom data type, stylus, 6dof, see wm_event_types.h */
	event.customdatafree = 0;
	event.pad2 = 0;
	event.customdata = 0;	/* ascii, unicode, mouse coords, angles, vectors, dragdrop info */
    
    knife_dummy_op.type->modal(C, &knife_dummy_op, &event);

    event.type = EVT_MODAL_MAP;
    event.val = KNF_MODAL_CONFIRM;
    knife_dummy_op.type->modal(C, &knife_dummy_op, &event);

    WM_operator_properties_free(knife_dummy_op.ptr);
    knife_dummy_op.ptr = 0;
    
    BKE_reports_clear(knife_dummy_op.reports);
    MEM_freeN(knife_dummy_op.reports);
    knife_dummy_op.reports = 0;
	

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

	DEG_id_tag_update((ID*)obedit->data, ID_RECALC_GEOMETRY);
	WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	ED_undo_push(C, "Knife");

	for (int i = 0; i < VR_SIDES; ++i) {
		Widget_Knife::obj.do_render[i] = false;
	}
    */
}

void Widget_Knife::render(VR_Side side)
{
    if (!Widget_Knife::obj.do_render[side]) {
        return;
    }
    /*
	VR_Draw::set_depth_test(false, false);
	VR_Draw::set_color(0.8f, 0.8f, 0.8f, 1.0f);

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_line_width(10.0f);

	static const float color[3] = { 0.5f, 0.0f, 0.0f };
	immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
	immBeginAtMost(GPU_PRIM_LINES, 2);
	immUniformColor3fvAlpha(color, 1.0f);
	immUniform1f("dash_width", 6.0f);

	immVertex3fv(pos, (float*)&p0);
	immVertex3fv(pos, (float*)&p1);

	if (p0 == p1) {
		immVertex3fv(pos, (float*)&p0);
	}
	immEnd();
    */
	Widget_Knife::obj.do_render[side] = false;
}
