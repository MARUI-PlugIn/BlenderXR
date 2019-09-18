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
* The Original Code is Copyright (C) 2019 by Blender Foundation.
* All rights reserved.
*
* Contributor(s): Multiplexed Reality
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_widget_animation.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_ANIMATION_H__
#define __VR_WIDGET_ANIMATION_H__

#include "vr_widget.h"

struct Object;

/* Interaction widget for the Animation tool. */
class Widget_Animation : public VR_Widget
{
  friend class Widget_Menu;

  /* Possible object-to-equipment bind types. */
  typedef enum BindType {
    BINDTYPE_NONE = 0	/* No binding. */
    ,
    BINDTYPE_HMD = 1 /* Bind to HMD. */
    ,
    BINDTYPE_CONTROLLER_LEFT = 2  /* Bind to left controller. */
    ,
    BINDTYPE_CONTROLLER_RIGHT = 3	/* Bind to right controller. */
    ,
    BINDTYPE_TRACKER = 4 /* Bind to tracker. */
    ,
    BINDTYPES  /* Number of distinct bind types. */
  } BindType;

  static BindType bind_type; /* The current bind type for the Animation tool. */
  static std::vector<Object*> bindings;  /* The currently bound objects. */
  static bool binding_paused; /* Whether the current bindings are paused. */

  static int constraint_flag[3][3]; /* TRS-XYZ flags describing the current constraint mode. */
  static VR_UI::TransformSpace transform_space;	/* The current transform space for the Animation tool. */
public:
  static void update_bindings();  /* Update any object bindings. */
  static void clear_bindings(); /* Clear any object bindings. */
protected:
  static bool manipulator;	/* Whether the manipulator is active and visible. */
  static Mat44f manip_t;	/* The transformation of the manipulator. */
  static Coord3Df manip_angle[2]; /* The current manipulator angle (euler xyz) when constraining rotations. */
  static float manip_scale_factor;	/* Scale factor for the manipulator (relative to longest selected object axis). */

  static void update_manipulator();	/* Update the manipulator transform. */

  static void render_axes(const float length[3], int draw_style = 0); /* Render manipulator axes. */
  static void render_gimbal(const float radius[3], const bool filled,
    const float axis_modal_mat[4][4], const float clip_plane[4],
    const float arc_partial_angle, const float arc_inner_factor); /* Render manipulator gimbal. */
  static void render_dial(const int index, const float angle_ofs, const float angle_delta,
    const float arc_inner_factor, const float radius);	/* Render manipulator dial. */
public:
  static Widget_Animation obj;	/* Singleton implementation object. */
  virtual std::string name() override { return "ANIMATION"; };	/* Get the name of this widget. */
  virtual Type type() override { return TYPE_ANIMATION; };	/* Type of Widget. */

  virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
  virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
  virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
  virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
  virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

  virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

#endif /* __VR_WIDGET_ANIMATION_H__ */
