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

/** \file blender/vr/intern/vr_widget_transform.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_TRANSFORM_H__
#define __VR_WIDGET_TRANSFORM_H__

#include "vr_widget.h"

/* Interaction widget for the Transform tool. */
class Widget_Transform : public VR_Widget
{
	friend class Widget_LoopCut;
	friend class Widget_Extrude;
	friend class Widget_SwitchLayout;
	friend class Widget_SwitchComponent;
	friend class Widget_SwitchSpace;
	friend class Widget_SwitchTool;
	friend class Widget_Menu;

	typedef enum TransformMode {
		TRANSFORMMODE_OMNI = 0	/* 9DoF transformation mode. */
		,
		TRANSFORMMODE_MOVE = 1	/* Translation mode. */
		,
		TRANSFORMMODE_ROTATE = 2	/* Rotation mode. */
		,
		TRANSFORMMODE_SCALE = 3	/* Scale mode. */
		,
		TRANSFORMMODES /* Number of transform modes. */
	} TransformMode;

	static TransformMode transform_mode; /* The current transform mode for the Transform tool. */
	static bool omni;	/* Whether the Transform tool is in omni mode (used for maintaining correct transform_mode between interactions). */
	static VR_UI::ConstraintMode constraint_mode; /* The current constraint mode for the Transform tool. */
	static int constraint_flag[3];	/* Flags (x, y, z) describing the current constraint mode. */
	static VR_UI::SnapMode snap_mode;	/* The current snap mode for the Transform tool. */
	static int snap_flag[3];	/* Flags (x, y, z) describing the current snap mode. */
	static std::vector<Mat44f*> nonsnap_t; /* The actual (non-snapped) transformations of the interaction objects. */
	static bool snapped; /* Whether a snap was applied in the previous transformation. */

	//static bool edit; /* Whether the Transform tool is in edit mode. */
	static VR_UI::TransformSpace transform_space;	/* The current transform space for the Transform tool. */
	static bool is_dragging; /* Whether the Transform tool is currently dragging. */

	static bool manipulator;	/* Whether the manipulator is active and visible. */
	static Mat44f manip_t;	/* The transformation of the manipulator. */
	static Mat44f manip_t_orig;	/* The original transformation of the manipulator on drag_start(). */
	static Mat44f manip_t_snap;	/* The snapped trnasformation of the manipulator. */
	static Coord3Df manip_angle[VR_UI::TRANSFORMSPACES];	/* The current manipulator angle (euler xyz) when constraining rotations. */
	static float manip_scale_factor;	/* Scale factor for the manipulator (relative to longest selected object axis). */

	static Mat44f obmat_inv;	/* The inverse of the selected object's transformation (edit mode). */

	static void raycast_select_manipulator(const Coord3Df& p, bool *extrude=0);	/* Select a manipulator component with raycast selection. */
public:
	static void update_manipulator();	/* Update the manipulator transform. */
protected:
	static void render_axes(const float length[3], int draw_style = 0); /* Render manipulator axes. */
	static void render_planes(const float length[3]);	/* Render manipulator planes. */
	static void render_gimbal(const float radius[3], const bool filled,
							  const float axis_modal_mat[4][4], const float clip_plane[4],
							  const float arc_partial_angle, const float arc_inner_factor); /* Render manipulator gimbal. */
	static void render_dial(const float angle_ofs, const float angle_delta,
							const float arc_inner_factor, const float radius);	/* Render manipulator dial. */
	static void render_incremental_angles(const float incremental_angle, const float offset, const float radius);	/* Render manipulator incremental angles. */
public:
	static Widget_Transform obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "TRANSFORM"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_TRANSFORM; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_TRANSFORM_H__ */
