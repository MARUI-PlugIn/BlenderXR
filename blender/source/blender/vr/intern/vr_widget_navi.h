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

/** \file blender/vr/intern/vr_widget_navi.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_NAVI_H__
#define __VR_WIDGET_NAVI_H__

#include "vr_widget.h"

/* Interaction widget for navigation: selects the respective sub-widget based on the setting VR_UI::navigation_mode. */
class Widget_Navi : public VR_Widget
{
	friend class Widget_Menu;

	static VR_UI::NavLock nav_lock[3];	/* The current navigation locks (if any). */
public:
	static Widget_Navi obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "NAVI"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_NAVI; };	/* Type of Widget. */

	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the navigation button. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with the navigation button. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with the  navigation button. */
	
	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */

	/* Interaction widget for grabbing-the-air navigation. */
	class GrabAir : public VR_Widget
	{
	public:
		static GrabAir obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "NAVI_GRABAIR"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_NAVI_GRABAIR; };	/* Type of Widget. */

		virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the navigation button. */
		virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with the navigation button. */
		virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with the  navigation button. */
		
		virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
	};

	/* Interaction widget for joystick-style navigation. */
	class Joystick : public VR_Widget
	{
		static float move_speed;	/* Speed multiplier for moving / translation. */
		static float turn_speed;	/* Speed multiplier for turning around (rotating around up-axis). */
		static float zoom_speed;	/* Speed multiplier for scaling / zooming. */
	public:
		static Joystick obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "NAVI_JOYSTICK"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_NAVI_JOYSTICK; };	/* Type of Widget. */

		virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the navigation button. */
		virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with the navigation button. */
		virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with the  navigation button. */

		virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
	};

	/* Interaction widget for teleport navigation. */
	class Teleport : public VR_Widget
	{
		static Mat44f arrow;	/* Transformation matrix of the arrow (rotation set to identity). */

		static bool cancel;	/* Whether to cancel the teleportation result. */
	public:
		static Teleport obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "NAVI_TELEPORT"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_NAVI_TELEPORT; };	/* Type of Widget. */

		virtual void drag_start(VR_UI::Cursor& c) override; //!< Start a drag/hold-motion with the navigation button.
		virtual void drag_contd(VR_UI::Cursor& c) override; //!< Continue drag/hold with the navigation button.
		virtual void drag_stop(VR_UI::Cursor& c) override; //!< Stop drag/hold with the  navigation button.
		
		virtual void render(VR_Side side) override; //!< Apply the widget's custom render function (if any).
		virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override; //!< Render the icon/indication of the widget.
	};
};

#endif /* __VR_WIDGET_NAVI_H__ */
