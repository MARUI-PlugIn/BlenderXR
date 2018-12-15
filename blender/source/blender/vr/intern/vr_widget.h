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

#define VR_WIDGET_TOOL_MOVE_AXISTHRES   0.020f	/* Threshold for locking/applying translation in an axis direction in meters. */
#define VR_WIDGET_TOOL_ROTATE_AXISTHRES	0.020f	/* Threshold for locking/applying rotation in an axis direction in meters. */
#define VR_WIDGET_TOOL_SCALE_AXISTHRES  0.030f	/* Threshold for locking/applying scaling in an axis direction in meters. */

/* Widget base class. */
class VR_Widget {
public:
	/* Type of Widget. */
	typedef enum Type {
		TYPE_INVALID	/* Invalid or unrecognized type of widget. */
		,
		TYPE_TRIGGER
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
		TYPE_CURSOROFFSET
		,
		TYPE_SELECT
		,
		TYPE_SELECT_RAYCAST
		,
		TYPE_SELECT_PROXIMITY
		,
		TYPE_TRANSFORM
		,
		TYPE_ANNOTATE
		,
		TYPE_MEASURE
		,
		TYPE_DELETE
		,
		TYPE_DUPLICATE
		,
		TYPE_UNDO
		,
		TYPE_REDO
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
		MENUTYPE_TS_SELECT	/* Tool settings for the select widget. */
		,
		MENUTYPE_TS_TRANSFORM	/* Tool settings for the transform widget. */
		,
		MENUTYPE_TS_ANNOTATE	/* Tool settings for the annotate widget. */
		,
		MENUTYPE_TS_MEASURE	/* Tool settings for the measure widget. */
		,
		MENUTYPE_AS_SELECT	/* Action settings for the select widget. */
		,
		MENUTYPE_AS_TRANSFORM	/* Action settings for the transform widget. */
	} MenuType;
};

/* Interaction widget for the controller trigger (generalized). */
class Widget_Trigger : public VR_Widget
{
public:
	static Widget_Trigger obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "TRIGGER"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_TRIGGER; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool allows_focus_steal(Type by) const override;	/* Whether this widget allows other widgets to steal its focus. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

/* Interaction widget for object selection. (Default ray-casting mode). */
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

/* Interaction widget for navigation: selects the respective sub-widget based on the setting VR_UI::navigation_mode. */
class Widget_Navi : public VR_Widget
{
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
		static float	move_speed;	/* Speed multiplier for moving / translation. */
		static float	turn_speed;	/* Speed multiplier for turning around (rotating around up-axis). */
		static float	zoom_speed;	/* Speed multiplier for scaling / zooming. */
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

/* Interaction widget for emulating a 'ctrl' key on a keyboard. */
class Widget_Ctrl : public VR_Widget
{
public:
	static Widget_Ctrl obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "CTRL"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_CTRL; };	/* Type of Widget. */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for emulating a 'shift' key on a keyboard. */
class Widget_Shift : public VR_Widget
{
public:
	static Widget_Shift obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "SHIFT"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_SHIFT; };	/* Type of Widget. */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for emulating an 'alt' key on a keyboard. */
class Widget_Alt : public VR_Widget
{
public:
	static Widget_Alt obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "ALT"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_ALT; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for manipulating the VR UI cursor offset. */
class Widget_CursorOffset : public VR_Widget
{
public:
	static Widget_CursorOffset obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "CURSOROFFSET"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_CURSOROFFSET; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

class Widget_SwitchTool;
class Widget_Menu;

/* Interaction widget for the Transform tool. */
class Widget_Transform : public VR_Widget
{
	friend class Widget_SwitchTool;
	friend class Widget_Menu;

	typedef enum TransformMode {
		TRANSFORMMODE_OMNI = 0	/* 9DoF transformation mode */
		,
		TRANSFORMMODE_MOVE = 1	/* Translation mode */
		,
		TRANSFORMMODE_ROTATE = 2	/* Rotation mode */
		,
		TRANSFORMMODE_SCALE = 3	/* Scale mode */
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

	static bool local;	/* Whether the Transform tool is in world or local coordinates. */
	static bool manipulator;	/* Whether the manipulator is active and visible. */
	static Mat44f manip_t;	/* The transformation of the shared (world) manipulator. */
	static std::vector<Mat44f*> manip_t_local;	/* The transformations of the local manipulators. */
	static Coord3Df manip_angle;	/* The current manipulator angle (euler xyz) when constraining world rotation. */
	static std::vector<Coord3Df*> manip_angle_local;	/* The current manipulator angle(s) when constraining local rotations. */
	static float manip_scale_factor;	/* Scale factor for manipulator (relative to longest selected object axis). */
	static int manip_interact_index;	/* The index of the manipulator currently being interacted with (for local coordinates). */

	static void raycast_select_manipulator(const Coord3Df& p);	/* Select a manipulator component with raycast selection. */
public:
	static void update_manipulator(bool selection_changed=true);	/* Update the manipulator transform. */
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

struct bGPDspoint;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct Main;
class Widget_Measure;

/* Interaction widget for the gpencil annotation tool. */
class Widget_Annotate : public VR_Widget
{
	friend class Widget_Measure;
	friend class Widget_Menu;

	static struct bGPdata *gpd;	/* The VR gpencil data .*/
	static std::vector<struct bGPDlayer *>gpl;	/* The VR gpencil layer. */
	static std::vector<struct bGPDframe *>gpf;	/* The VR gpencil frame. */
	static struct Main *main;	/* The current scene data. */

	static uint num_layers;	/* The number of VR gpencil layers (one layer for each color + measure tool layer). */
	static uint active_layer;	/* The currently active VR gpencil layer. */
	static int init(bool new_scene); /* Initialize the VR gpencil structs. */

	static std::vector<bGPDspoint> points;	/* The 3D points associated with the current stroke. */

	//static float point_thickness;	/* Stroke thickness for points. */
	static float line_thickness;	/* Stroke thickness for lines. */
	static float color[4];	/* Stroke color. */

	static bool eraser;	/* Whether the annotate widget is in eraser mode. */
	static VR_Side cursor_side;	/* Side of the current interaction cursor. */
	static float eraser_radius;	/* Radius of the eraser ball. */
	static void erase_stroke(bGPDstroke *gps, bGPDframe *gp_frame);	/*	Helper function to erase a stroke. */
public:
	static Widget_Annotate obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "ANNOTATE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_ANNOTATE; };	/* Type of Widget. */

	//virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	//virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

class Widget_Measure : public VR_Widget
{
public:
	/* Measure states. */
	typedef enum Measure_State {
		INIT,
		DRAW,
		MEASURE,
		DONE
	} Measure_State;
protected:
	static Coord3Df measure_points[3];	/* The current measure points. */
	static bGPDstroke *current_stroke;	/* The current gpencil stroke. */
	static bGPDspoint current_stroke_points[3];	/* The current gpencil points. */

	static Measure_State measure_state;	/* The current measure state. */
	static VR_UI::CtrlState measure_ctrl_state;	/* The current ctrl state. */
	static int measure_ctrl_count;	/* The current measure ctrl count. */

	static float line_thickness;	/* Stroke thickness for lines. */
	static float color[4];	/* Stroke color. */

	static float angle;	/* The current measured angle. */

	static VR_Side cursor_side;	/* Side of the current interaction cursor. */

	static void render_GPFont(const uint num, const uint numPoint, const Coord3Df& o);
	static void draw_line(VR_UI::Cursor& c, Coord3Df& p1, Coord3Df& p2);
public:
	static Widget_Measure obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "MEASURE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_MEASURE; };	/* Type of Widget. */

	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

/* Interaction widget for performing a 'delete' operation. */
class Widget_Delete : public VR_Widget
{
public:
	static Widget_Delete obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "DELETE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_DELETE; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for performing a 'duplicate' operation. */
class Widget_Duplicate : public VR_Widget
{
public:
	static Widget_Duplicate obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "DUPLICATE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_DUPLICATE; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for performing an 'undo' operation. */
class Widget_Undo : public VR_Widget
{
public:
	static Widget_Undo obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "UNDO"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_UNDO; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for performing a 'redo' operation. */
class Widget_Redo : public VR_Widget
{
public:
	static Widget_Redo obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "REDO"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_REDO; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for switching the currently active tool. */
class Widget_SwitchTool : public VR_Widget
{
	friend class Widget_Alt;

	static VR_Widget *curr_tool[VR_SIDES]; /* The current tool for each controller. */
public:
	static Widget_SwitchTool obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "SWITCHTOOL"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_SWITCHTOOL; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

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

#endif /* __VR_WIDGET_H__ */
