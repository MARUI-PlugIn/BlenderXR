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

/** \file blender/vr/intern/vr_widget_select.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_SELECT_H__
#define __VR_WIDGET_SELECT_H__

#include "vr_widget.h"

/* Interaction widget for the Select tool. */
class Widget_Select : public VR_Widget
{
public:
	static Widget_Select obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "SELECT"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_SELECT; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */

	/* Interaction widget for object selection: Cursor ray-casting mode (default). */
	class Raycast : public VR_Widget
	{
		static struct SelectionRect {
			float x0;
			float y0;
			float x1;
			float y1;
		} selection_rect[VR_SIDES];
	public:
		static Raycast obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "SELECT_RAYCAST"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_SELECT_RAYCAST; };	/* Type of Widget. */

		virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
		virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
		virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
		virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
		virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

		virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
	};

	/* Interaction widget for object selection in proximity selection mode. */
	class Proximity : public VR_Widget
	{
		static Coord3Df  p0;	/* Starting point of the selection volume. */
		static Coord3Df  p1;	/* Current / end point of the selection volume. */
	public:
		static Proximity obj;	/* Singleton implementation object. */
		virtual std::string name() override { return "SELECT_PROXIMITY"; };	/* Get the name of this widget. */
		virtual Type type() override { return TYPE_SELECT_PROXIMITY; };	/* Type of Widget. */

		virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
		virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
		virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
		virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
		virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

		virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
	};
};

#endif /* __VR_WIDGET_SELECT_H__ */
