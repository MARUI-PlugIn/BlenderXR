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

/** \file blender/vr/intern/vr_widget_menu.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_MENU_H__
#define __VR_WIDGET_MENU_H__

#include "vr_widget.h"

/* Interaction widget for a VR pie menu. */
class Widget_Menu : public VR_Widget
{
	friend class Widget_Alt;
	friend class Widget_SwitchTool;

	static std::vector<VR_Widget*> items[VR_SIDES];	/* The items (widgets) in the menu. */
	static uint num_items[VR_SIDES];	/* The number of items in the menu. */
	static uint depth[VR_SIDES];	/* The current menu depth (0 = base menu, 1 = first submenu, etc.). */

	static Coord2Df stick[VR_SIDES];	/* The uv coordinates of the stick/dpad (-1 ~ 1). */
	static float angle[VR_SIDES]; /* The stick/dpad angle (angle from (0,1)). */
public:
	static int highlight_index[VR_SIDES];	/* The currently highlighted menu item. */
public:
	static MenuType menu_type[VR_SIDES];	/* The current type of this menu. */
	static bool action_settings[VR_SIDES];	/* Whether the current menu is an action settings menu. */

	static void stick_center_click(VR_UI::Cursor& c);	/* Execute operation on stick/dpad center click. */

	static Widget_Menu obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "MENU"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_MENU; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */

	/* Interaction widget for a VR pie menu (left controller). */
	class Left : public VR_Widget
	{
	public:
		static Left obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "MENU_LEFT"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_MENU_LEFT; };	/* Type of Widget. */

		virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
		virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
		virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */
		virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
		virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
		virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

		virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
	};

	/* Interaction widget for a VR pie menu (right controller). */
	class Right : public VR_Widget
	{
	public:
		static Right obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "MENU_RIGHT"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_MENU_RIGHT; };	/* Type of Widget. */

		virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
		virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
		virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */
		virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
		virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
		virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

		virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
	};
};

#endif /* __VR_WIDGET_MENU_H__ */
