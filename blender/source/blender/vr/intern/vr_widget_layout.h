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

/** \file blender/vr/intern/vr_widget_layout.h
*   \ingroup vr
*/

#ifndef __VR_WIDGET_LAYOUT_H__
#define __VR_WIDGET_LAYOUT_H__

#include "vr_ui.h"

/* Widget UI layouts implementation. */
class VR_Widget_Layout
{
public:
	/* Bit in the controller button bitflag word corresponding to certain buttons. */
	typedef enum ButtonBit : uint64_t {
		BUTTONBIT_NONE			= 0x00000000			/* No bit set (null). */
		,
		BUTTONBIT_LEFTTRIGGER	= (uint64_t(1) << 0)	/* Bit of the left controller trigger as a button */
		,
		BUTTONBIT_RIGHTTRIGGER	= (uint64_t(1) << 1)	/* Bit of the right controller trigger as a button */
		,
		BUTTONBITS_TRIGGERS		= (BUTTONBIT_LEFTTRIGGER | BUTTONBIT_RIGHTTRIGGER)	/* Bits of pressing any of the controller triggers */
		,
		BUTTONBIT_LEFTGRIP		= (uint64_t(1) << 2)	/* Bit of the left "grip" (side/shoulder) button */
		,
		BUTTONBIT_RIGHTGRIP		= (uint64_t(1) << 3)	/* Bit of the right "grip" (side/shoulder) button  */
		,
		BUTTONBITS_GRIPS		 = (BUTTONBIT_LEFTGRIP | BUTTONBIT_RIGHTGRIP)	/* Bit of pressing any of the "grip" (side/shoulder) buttons  */
		,
		BUTTONBIT_DPADLEFT		= (uint64_t(1) << 4)	/* Bit of the trackpad left-side (as button) */
		,
		BUTTONBIT_DPADRIGHT		= (uint64_t(1) << 5)	/* Bit of the trackpad right-side (as button)  */
		,
		BUTTONBIT_DPADUP		= (uint64_t(1) << 6)	/* Bit of the trackpad up-side (as button) */
		,
		BUTTONBIT_DPADDOWN		= (uint64_t(1) << 7)	/* Bit of the trackpad down-side (as button) */
		,
		BUTTONBITS_DPADANY		= (BUTTONBIT_DPADLEFT | BUTTONBIT_DPADRIGHT | BUTTONBIT_DPADUP | BUTTONBIT_DPADDOWN)	/* Bits of pushing the trackpad in any direction */
		,
		BUTTONBIT_LEFTDPAD		= (uint64_t(1) << 8)	/* Bit of pressing the left trackpad (center) as a button */
		,
		BUTTONBIT_RIGHTDPAD		= (uint64_t(1) << 9)	/* Bit of pressing the right trackpad (center) as a button */
		,
		BUTTONBITS_DPADS		= (BUTTONBIT_LEFTDPAD | BUTTONBIT_RIGHTDPAD)	/* Bits of pressing any of the trackpads */
		,
		BUTTONBIT_STICKLEFT		= (uint64_t(1) << 10)	/* Bit of pushing the thumbstick to the left */
		,
		BUTTONBIT_STICKRIGHT	= (uint64_t(1) << 11)	/* Bit of pushing the thumbstick to the right */
		,
		BUTTONBIT_STICKUP		= (uint64_t(1) << 12)	/* Bit of pushing the thumbstick up */
		,
		BUTTONBIT_STICKDOWN		= (uint64_t(1) << 13)	/* Bit of pushing the thumbstick down */
		,
		BUTTONBITS_STICKANY		= (BUTTONBIT_STICKLEFT | BUTTONBIT_STICKRIGHT | BUTTONBIT_STICKUP | BUTTONBIT_STICKDOWN)	/* Bits of pushing the thumbstick in any direction */
		,
		BUTTONBIT_LEFTSTICK		= (uint64_t(1) << 14)	/* Bit of pressing the left thumbstick */
		,
		BUTTONBIT_RIGHTSTICK	= (uint64_t(1) << 15)	/* Bit of pressing the right thumbstick */
		,
		BUTTONBITS_STICKS		= (BUTTONBIT_LEFTSTICK | BUTTONBIT_RIGHTSTICK)	/* Bits of pressing any of the thumbsticks */
		,
		BUTTONBIT_LEFTTHUMBREST = (uint64_t(1) << 16)	/* Bit of pressing (touching) the left thumbrest area */
		,
		BUTTONBIT_RIGHTTHUMBREST= (uint64_t(1) << 17)	/* Bit of pressing (touching) the right thumbrest area */
		,
		BUTTONBITS_THUMBRESTS	= (BUTTONBIT_LEFTTHUMBREST | BUTTONBIT_RIGHTTHUMBREST)	/* Bit of pressing (touching) any of the thumbrest areas */
		,
		BUTTONBIT_X				= (uint64_t(1) << 18)	/* Bit of the "X" button */
		,
		BUTTONBIT_Y				= (uint64_t(1) << 19)	/*  Bit of the "Y" button */
		,
		BUTTONBIT_A				= (uint64_t(1) << 20)	/* Bit of the "A" button */
		,
		BUTTONBIT_B				= (uint64_t(1) << 21)	/*  Bit of the "B" button */
		,
		BUTTONBITS_XA			= (BUTTONBIT_X | BUTTONBIT_A)	/* Bits of the "X" or "A" button */
		,
		BUTTONBITS_YB			= (BUTTONBIT_Y | BUTTONBIT_B)	/* Bits of the "Y" or "B" button */
		,
		BUTTONBIT_MENU			= (uint64_t(1) << 22)	/* Bit of the "menu" button */
		,
		BUTTONBIT_SYSTEM		= (uint64_t(1) << 23)	/* Bit of the "system" button */
	} ButtonBit;

	/* ID of contoller buttons and respective index in the widget map. */
	typedef enum ButtonID : uint64_t {
		BUTTONID_TRIGGER	= 0		/* ID of the controller trigger as a button */
		,
		BUTTONID_GRIP		= 1		/* ID of the "grip" (side/shoulder) button */
		,
		BUTTONID_DPADLEFT	= 2		/* ID of the trackpad left-side (as button) */
		,
		BUTTONID_DPADRIGHT	= 3		/* ID of the trackpad right-side (as button) */
		,
		BUTTONID_DPADUP		= 4		/* ID of the trackpad up-side (as button) */
		,
		BUTTONID_DPADDOWN	= 5		/* ID of the trackpad down-side (as button) */
		, 
		BUTTONID_DPAD		= 6		/* ID of pressing the trackpad (center) */
		,
		BUTTONID_STICKLEFT	= 7		/* ID of pushing the thumbstick to the left */
		,
		BUTTONID_STICKRIGHT = 8		/* ID of pushing the thumbstick to the right */
		,
		BUTTONID_STICKUP	= 9		/* ID of pushing the thumbstick up */
		,
		BUTTONID_STICKDOWN	= 10	/* ID of pushing the thumbstick down */
		,
		BUTTONID_STICK		= 11	/* ID of pressing the thumbstick */
		,
		BUTTONID_THUMBREST	= 12	/* ID of pressing (touching) the thumbrest */
		,
		BUTTONID_XA			= 13	/* ID of the "X" (left) or "A" (right) button */
		,
		BUTTONID_YB			= 14	/* ID of the "Y" (left) or "B" (right) button */
		,
		BUTTONID_MENU		= 15	/* ID of the "menu" button */
		,
		BUTTONID_SYSTEM		= 16	/* ID of the "system" button */
		,
		BUTTONIDS			= 17	/* Number of button IDs */
		,
		BUTTONID_UNKNOWN = (uint64_t)(-1)	/* ID for unknown buttons (should not be mapped). */
	} ButtonID;

	static ButtonID  buttonBitToID(ButtonBit bit);	/* Helper function to convert button bits to button IDs. */
	static ButtonBit buttonIDToBit(ButtonID  id);	/* Helper function to convert button IDs to button bit. */
	static std::string buttonIDToString(ButtonID  id);	/* Helper function to convert button IDs to ident string. */
	static ButtonID  buttonStringToID(const std::string str);	/* Helper function to convert button ident strings to IDs. */

	static Coord3Df button_positions[VR_UI_TYPES][VR_SIDES][BUTTONIDS];	/* 3D positions of the buttons with respect to the controllers. */

	VR_Widget_Layout();				/* Constructor. */
	virtual ~VR_Widget_Layout();	/* Destructor. */
public:
	/* UI Layout: mapping of controller functions to widgets. */
	typedef struct Layout {
		std::string name;	/* Name of the mapping. */
		VR_UI_Type type;	/* The UI type of the layout. */
		VR_Widget* m[VR_SIDES][BUTTONIDS][VR_UI::ALTSTATES];	/* Mapping of buttons to widgets (one for each controller side, each button, ALT on/off). */
		ButtonBit shift_button_bits[VR_SIDES][VR_UI::ALTSTATES];	/* Controller button bits defined to be the 'shift buttons' (per side, and whether ALT is pressed). */
		ButtonBit alt_button_bits[VR_SIDES];	/* Controller button bits defined to be the 'alt buttons' (per side). */
	} Layout;

	static Layout*              current_layout;	/* The layouts (widget mapping) currently in use. */
	static std::vector<Layout*>	layouts[VR_UI_TYPES];	/* List of all layouts (mappings). */

	enum DefaultLayout {
		DEFAULTLAYOUT_MODELING	= 0		/* Default layout for modeling. */
		,
		DEFAULTLAYOUTS			= 1		/* Number of default layouts. */
	};
	static const Layout default_layouts[VR_UI_TYPES][DEFAULTLAYOUTS];	/* Default layout mapping. */

	static VR_UI::Error   reset_to_default_layouts();	/* Load the default layout. */
	static VR_UI::Error   get_current_layout(std::string& layout);	/* Get the current UI layout (task mode). */
	static VR_UI::Error   set_current_layout(const std::string& layout_name);	/* Set the current UI layout (task mode). */
	static const VR_Widget* get_current_tool(VR_Side side);	/* Get the currently active tool for the controller. */
	static VR_UI::Error   set_current_tool(const VR_Widget* tool, VR_Side side);	/* Set the currently active tool for the controller. */
	static VR_UI::Error   rename_current_layout(const std::string& new_name);	/* Update the name of the layout (task mode). */
	static VR_UI::Error   delete_current_layout();	/* Delete the current layout (task mode). */
	static VR_UI::Error   get_layouts_list(std::vector<std::string>& list);	/* Get list of all known UI layouts (task modes). */
	static VR_UI::Error   map_widget(VR_Side side, std::string event, VR_UI::AltState alt, std::string widget);	/* Set a button-to-widget mapping. */
	static VR_UI::Error   unmap_widget(const std::string& widget);	/* Unmap widget everywhere. */
	static VR_UI::Error   get_mapped_widget(VR_Side side, const std::string& event, VR_UI::AltState alt, std::string& widget);	/* Get a single button-to-widget mapping (widget name). */
	static VR_UI::Error   get_mapped_event(const std::string& widget, VR_Side& side, std::string& event, VR_UI::AltState& alt);	/* Get a single button-to-widget mapping (event name). */
	static VR_UI::Error   get_mapped_event(const VR_Widget* widget, VR_Side& side, std::string& event, VR_UI::AltState& alt);	/* Get a single button-to-widget mapping (event name). */
	static VR_UI::Error   get_mapped_event(const Layout* layout, const std::string& widget, VR_Side& side, std::string& event, VR_UI::AltState& alt);	/* Get a single button-to-widget mapping (event name). */
	static VR_UI::Error   get_mapped_event(const Layout* layout, const VR_Widget* widget, VR_Side& side, std::string& event, VR_UI::AltState& alt);	/* Get a single button-to-widget mapping (event name). */
	static VR_UI::Error   get_mapped_event(const Layout* layout, const VR_Widget* widget, VR_Side& side, ButtonID& event, VR_UI::AltState& alt);	/* Get a single button-to-widget mapping (event name). */
	static VR_UI::Error   get_events_list(std::vector<std::string>& events);	/* Get a list of all the events (buttons) available for this UI. */
};

inline VR_Widget_Layout::ButtonBit& operator|=(VR_Widget_Layout::ButtonBit& b0, const VR_Widget_Layout::ButtonBit& b1)
{
	return b0 = VR_Widget_Layout::ButtonBit(b0 | b1);
};
inline VR_Widget_Layout::ButtonBit operator|(const VR_Widget_Layout::ButtonBit& b0, const VR_Widget_Layout::ButtonBit& b1)
{
	return VR_Widget_Layout::ButtonBit((uint64_t)(b0) | (uint64_t)(b1));
};

#endif /* __VR_WIDGET_LAYOUT_H__ */
