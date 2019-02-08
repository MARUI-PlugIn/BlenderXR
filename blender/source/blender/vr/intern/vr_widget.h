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

/** \file blender/vr/intern/vr_widget.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_H__
#define __VR_WIDGET_H__

#include "vr_ui.h"

/* Widget base class. */
class VR_Widget {
public:
	static const Mat44f m_widget_touched;
	/* Type of Widget. */
	typedef enum Type {
		TYPE_INVALID	/* Invalid or unrecognized type of widget. */
		,
		TYPE_NAVI
		,
		TYPE_NAVI_GRABAIR
		,
		TYPE_NAVI_JOYSTICK
		,
		TYPE_NAVI_TELEPORT
		,
		TYPE_CTRL
		,
		TYPE_SHIFT
		,
		TYPE_ALT
		,
		TYPE_SELECT
		,
		TYPE_SELECT_RAYCAST
		,
		TYPE_SELECT_PROXIMITY
		,
		TYPE_CURSOR
		,
		TYPE_TRANSFORM
		,
		TYPE_ANNOTATE
		,
		TYPE_MEASURE
		,
		TYPE_ADDPRIMITIVE
		,
		TYPE_EXTRUDE
		,
		TYPE_INSETFACES
		,
		TYPE_BEVEL
		,
		TYPE_LOOPCUT
		,
		TYPE_KNIFE
		,
		TYPE_CURSOROFFSET
		,
		TYPE_DELETE
		,
		TYPE_DUPLICATE
		,
		TYPE_JOIN
		,
		TYPE_SEPARATE
		,
		TYPE_UNDO
		,
		TYPE_REDO
		,
		TYPE_SWITCHLAYOUT
		,
		TYPE_SWITCHCOMPONENT
		,
		TYPE_SWITCHSPACE
		,
		TYPE_SWITCHTOOL
		,
		TYPE_MENU
		,
		TYPE_MENU_LEFT
		,
		TYPE_MENU_RIGHT
	} Type;

	// ============================================================================================== //
	// ====================+=======    STATIC GLOBAL WIDGET MONITOR    ==================+=========== //
	// ---------------------------------------------------------------------------------------------- //
	static VR_Widget* get_widget(Type type, const char* ident = 0);	/* Get widget pointer by type and (optionally) identifier. */
	static VR_Widget* get_widget(const std::string& str);	/* Get widget pointer by name. */
	static Type get_widget_type(const std::string& str);	/* Get widget type from string. */
	std::vector<std::string> list_widgets();	/* List all widget names. */
	std::string type_to_string(Type type);	/* Convert typeID to string. */
	bool delete_widget(const std::string& str);	/* Delete custom widget. */

	// ============================================================================================== //
	// ===========================    DYNAMIC UI OBJECT IMPLEMENTATION    =========================== //
	// ---------------------------------------------------------------------------------------------- //
	virtual std::string name() = 0; /* Get the name of this widget. */
	virtual Type type() = 0; /* Type of Widget. */
public:
	VR_Widget();	/* Constructor. */
	virtual ~VR_Widget();	/* Destructor. */

	virtual bool has_click(VR_UI::Cursor& c) const;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c);	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const;	/* Test whether this widget supports "dragging". */
	virtual void drag_start(VR_UI::Cursor& c);	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c);	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c);	/* Stop drag/hold with index finger / trigger. */
	virtual bool allows_focus_steal(Type by) const;	/* Whether this widget allows other widgets to steal its focus. */
	virtual bool steals_focus(Type from) const;	/* Whether this widget steals focus from other widgets. */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false);	/* Render the icon/indication of the widget. */
	virtual void render(VR_Side side);	/* Apply the widget's custom render function (if any). */
	bool do_render[VR_SIDES];	/* Flag to enable/disable the widget's render function. */

	/* Type of custom pie menu. */
	typedef enum MenuType {
		MENUTYPE_INVALID	/* Invalid or unrecognized type of menu. */
		,
		MENUTYPE_MAIN_8	/* Main menu (8 items). */
		,
		MENUTYPE_MAIN_12	/* Main menu (12 items). */
		,
		MENUTYPE_SWITCHTOOL	/* Switch tool menu. */
		,
		MENUTYPE_TS_SELECT	/* Tool settings for the select widget. */
		,
		MENUTYPE_TS_CURSOR	/* Tool settings for the cursor widget. */
		,
		MENUTYPE_TS_TRANSFORM	/* Tool settings for the transform widget. */
		,
		MENUTYPE_TS_ANNOTATE	/* Tool settings for the annotate widget. */
		,
		MENUTYPE_TS_MEASURE	/* Tool settings for the measure widget. */
		,
		MENUTYPE_TS_ADDPRIMITIVE	/* Tool settings for the "add primitive" widget. */
		,
		MENUTYPE_TS_EXTRUDE	/* Tool settings for the extrude widget. */
		,
		MENUTYPE_TS_INSETFACES	/* Tool settings for the inset faces widget. */
		,
		MENUTYPE_TS_BEVEL	/* Tool settings for the bevel widget. */
		,
		MENUTYPE_TS_LOOPCUT	/* Tool settings for the loop cut widget. */
		,
		MENUTYPE_TS_KNIFE	/* Tool settings for the knife widget. */
		,
		MENUTYPE_AS_NAVI	/* Action settings for the navi widget. */
		,
		MENUTYPE_AS_TRANSFORM	/* Action settings for the transform widget. */
		,
		MENUTYPE_AS_EXTRUDE	/* Action settings for the extrude widget. */
	} MenuType;
};

#endif /* __VR_WIDGET_H__ */
