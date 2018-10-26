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
		TYPE_SELECT
		,
		TYPE_SELECT_RAYCAST
		,
		TYPE_SELECT_PROXIMITY
		,
		TYPE_NAVI
		,
		TYPE_NAVI_GRABAIR
		,
		TYPE_NAVI_JOYSTICK
		,
		TYPE_NAVI_TELEPORT
		,
		TYPE_SHIFT
		,
		TYPE_ALT
		,
		TYPE_CURSOROFFSET
		,
		TYPE_ANNOTATE
		,
		TYPE_MEASURE
		,
		TYPE_QUICKGRAB
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
		TYPE_SWITCHTOOLMODE
		,
		TYPE_MENU_COLORWHEEL
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

struct bGPDspoint;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct Main;
class Widget_Measure;
class Widget_Menu_ColorWheel;
class Widget_SwitchToolMode;

/* Interaction widget for the gpencil annotation tool. */
class Widget_Annotate : public VR_Widget
{
	friend class Widget_Measure;
	friend class Widget_Menu_ColorWheel;
	friend class Widget_SwitchToolMode;

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
	static Coord3Df p0;	/* Starting point of the cursor. */
	static Coord3Df p1;	/* Current / end point of the cursor. */

	static float line_thickness;	/* Stroke thickness for lines. */
	static float color[4];	/* Stroke color. */

	static VR_Side cursor_side;	/* Side of the current interaction cursor. */
public:
	static Widget_Measure obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "MEASURE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_MEASURE; };	/* Type of Widget. */

	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render(VR_Side side) override;	/* Apply the widget's custom render function (if any). */
};

/* Interaction widget for the QuickGrab tool. */
class Widget_QuickGrab : public VR_Widget
{
	static bool bimanual; /* Whether the current interaction is bimanual. */
public:
	static Widget_QuickGrab obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "QUICKGRAB"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_QUICKGRAB; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */
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
	static std::string curr_tool_str[VR_SIDES]; /* The name of the current tool (for both controllers). */
public:
	static Widget_SwitchTool obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "SWITCHTOOL"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_SWITCHTOOL; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for switching the tool mode for the currently active tool. */
class Widget_SwitchToolMode : public VR_Widget
{
	static std::string curr_tool_mode_str[VR_SIDES]; /* The name of the current tool mode (for both controllers). */
	static void get_current_tool_mode(const VR_Widget *tool, VR_Side controller_side); /* Helper function to get the current tool mode. */
	static void set_current_tool_mode(const VR_Widget *tool, VR_Side controller_side); /* Helper function to set the current tool mode. */
public:
	static Widget_SwitchToolMode obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "SWITCHTOOLMODE"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_SWITCHTOOLMODE; };	/* Type of Widget. */

	virtual bool has_click(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "clicking". */
	virtual void click(VR_UI::Cursor& c) override;	/* Click with the index finger / trigger. */
	virtual bool has_drag(VR_UI::Cursor& c) const override;	/* Test whether this widget supports "dragging". */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

/* Interaction widget for the color wheel menu. */
class Widget_Menu_ColorWheel : public VR_Widget
{
	static float color[4];	/* The currently selected color. */
public:
	static Widget_Menu_ColorWheel obj;	/* Singleton implementation object. */
	virtual std::string name() override { return "MENU_COLORWHEEL"; };	/* Get the name of this widget. */
	virtual Type type() override { return TYPE_MENU_COLORWHEEL; };	/* Type of Widget. */

	virtual void drag_start(VR_UI::Cursor& c) override;	/* Start a drag/hold-motion with the index finger / trigger. */
	virtual void drag_contd(VR_UI::Cursor& c) override;	/* Continue drag/hold with index finger / trigger. */
	virtual void drag_stop(VR_UI::Cursor& c) override;	/* Stop drag/hold with index finger / trigger. */

	virtual void render_icon(const Mat44f& t, VR_Side controller_side, bool active = false, bool touched = false) override;	/* Render the icon/indication of the widget. */
};

#endif /* __VR_WIDGET_H__ */
