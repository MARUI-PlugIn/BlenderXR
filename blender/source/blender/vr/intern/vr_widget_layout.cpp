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
* The Original Code is Copyright (C) 2016 by Mike Erwin.
* All rights reserved.
*
* Contributor(s): Blender Foundation
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/vr/intern/vr_widget_layout.h
*   \ingroup vr
*
* Layouts and button mappings for VR widget UI.
*/

#include "vr_types.h"
#include "vr_main.h"

#include "vr_widget.h"

#include "vr_widget_layout.h"


 /***********************************************************************************************//**
  * \class                               Widget_Layout
  ***************************************************************************************************
  *  Widget UI layouts and button mappings for VR input devices.
  *
  **************************************************************************************************/
Coord3Df VR_Widget_Layout::button_positions[VR_UI_TYPES][VR_SIDES][BUTTONIDS] =
{
	/*******************************************
	*				VR_UI_Type_Null			   *
	********************************************/
	{
		/* VR_SIDE_LEFT */
		{
			Coord3Df(0, 0, 0)// BUTTONID_TRIGGER
			,
			Coord3Df(0, 0, 0)// BUTTONID_GRIP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKLEFT
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKRIGHT
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKUP
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)// BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, 0, 0)// BUTTONID_MENU
			,
			Coord3Df(0, 0, 0)// BUTTONID_SYSTEM
		}
		,
		/* VR_SIDE_RIGHT */
		{
			Coord3Df(0, 0, 0)// BUTTONID_TRIGGER
			,
			Coord3Df(0, 0, 0)// BUTTONID_GRIP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKLEFT
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKRIGHT
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKUP
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)// BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, 0, 0)// BUTTONID_MENU
			,
			Coord3Df(0, 0, 0)// BUTTONID_SYSTEM
		}
	} /* Type_Null */
	,
		/*******************************************
		*			   VR_UI_Type_Oculus		   *
		********************************************/
	{
		/* VR_SIDE_LEFT */
		{
			Coord3Df(0.020f, -0.007f, 0.001f)// BUTTONID_TRIGGER
			,
			Coord3Df(0.019f, -0.068f, -0.022f)// BUTTONID_GRIP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(-0.012f, -0.038f, 0.011f)// BUTTONID_STICKLEFT
			,
			Coord3Df(0.008f, -0.038f, 0.011f)// BUTTONID_STICKRIGHT
			,
			Coord3Df(-0.001f, -0.028f, 0.013f)// BUTTONID_STICKUP
			,
			Coord3Df(-0.001f, -0.048f, 0.009f)// BUTTONID_STICKDOWN
			,
			Coord3Df(-0.002f, -0.038f, 0.011f)//0.015f)// BUTTONID_STICK
			,

			Coord3Df(0.027f, -0.045f, 0.001f)// BUTTONID_THUMBREST
			,
			Coord3Df(0.011f, -0.050f, 0.002f)// BUTTONID_XA
			,
			Coord3Df(0.017f, -0.035f, 0.004f)// BUTTONID_YB
			,
			Coord3Df(0, 0, 0)// BUTTONID_MENU
			,
			Coord3Df(0, 0, 0)// BUTTONID_SYSTEM
		}
		,
		/* VR_SIDE_RIGHT */
		{
			Coord3Df(-0.020f, -0.007f, 0.001f)// BUTTONID_TRIGGER
			,
			Coord3Df(-0.019f, -0.068f, -0.022f)// BUTTONID_GRIP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(-0.009f, -0.038f, 0.011f)// BUTTONID_STICKLEFT
			,
			Coord3Df(0.012f, -0.038f, 0.011f)// BUTTONID_STICKRIGHT
			,
			Coord3Df(0.002f, -0.028f, 0.013f)// BUTTONID_STICKUP
			,
			Coord3Df(0.002f, -0.048f, 0.009f)// BUTTONID_STICKDOWN
			,
			Coord3Df(0.0015f, -0.038f, 0.011f)//0.015f)// BUTTONID_STICK
			,
			Coord3Df(-0.027f, -0.045f, 0.001f)// BUTTONID_THUMBREST
			,
			Coord3Df(-0.011f, -0.050f, 0.002f)// BUTTONID_XA
			,
			Coord3Df(-0.017f, -0.035f, 0.004f)// BUTTONID_YB
			,
			Coord3Df(0, 0, 0)// BUTTONID_MENU
			,
			Coord3Df(0, 0, 0)// BUTTONID_SYSTEM
		}
	} /* Type_Oculus */
	,
	/*******************************************
	*			   VR_UI_Type_Vive			   *
	********************************************/
	{
		/* VR_SIDE_LEFT */
		{
			Coord3Df(0.040f, -0.085f, -0.030f)// BUTTONID_TRIGGER
			,
			Coord3Df(0.0275f, -0.149f, -0.012f)// BUTTONID_GRIP
			,
			Coord3Df(-0.0125f, -0.109f, 0.008f)// BUTTONID_DPADLEFT
			,
			Coord3Df(0.0125f, -0.109f, 0.008f)// BUTTONID_DPADRIGHT
			,
			Coord3Df(0, -0.097f, 0.010f)// BUTTONID_DPADUP
			,
			Coord3Df(0, -0.122f, 0.006f)// BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)//BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, -0.077f, 0.008f)// BUTTONID_MENU
			,
			Coord3Df(0, -0.150f, 0.010f)// BUTTONID_SYSTEM
		}
		,
		/* VR_SIDE_RIGHT */
		{
			Coord3Df(-0.040f, -0.085f, -0.030f)// BUTTONID_TRIGGER
			,
			Coord3Df(-0.0275f, -0.149f, -0.012f)// BUTTONID_GRIP
			,
			Coord3Df(-0.0125f, -0.109f, 0.008f)// BUTTONID_DPADLEFT
			,
			Coord3Df(0.0125f, -0.109f, 0.008f)// BUTTONID_DPADRIGHT
			,
			Coord3Df(0, -0.097f, 0.010f)// BUTTONID_DPADUP
			,
			Coord3Df(0, -0.122f, 0.006f)// BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)//BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, -0.077f, 0.008f)// BUTTONID_MENU
			,
			Coord3Df(0, -0.150f, 0.010f)// BUTTONID_SYSTEM
		}
	} /* Type_Vive */
	,
	/********************************************
	*			   VR_UI_Type_Microsoft			*
	********************************************/
	{
		/* VR_SIDE_LEFT */
		{
			Coord3Df(-0.030f, -0.085f, -0.030f)// BUTTONID_TRIGGER
			,
			Coord3Df(0.0275f, -0.149f, -0.012f)// BUTTONID_GRIP
			,
			Coord3Df(-0.011f, -0.0775f, -0.0065f)// BUTTONID_DPADLEFT
			,
			Coord3Df(0.011f, -0.0775f, -0.0065f)// BUTTONID_DPADRIGHT
			,
			Coord3Df(0, -0.0674f, -0.01264f)// BUTTONID_DPADUP
			,
			Coord3Df(0, -0.085f, 0.0015f)// BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_DPAD
			,
			Coord3Df(0.024f, -0.007f, 0.0065f)// BUTTONID_STICKLEFT
			,
			Coord3Df(0.038f, -0.077f, 0.0065f)// BUTTONID_STICKRIGHT
			,
			Coord3Df(0.029f, -0.071f, 0.0001f)// BUTTONID_STICKUP
			,
			Coord3Df(0.029f, -0.0855f, 0.0105f)// BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)// BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, -0.077f, 0.008f)// BUTTONID_MENU
			,
			Coord3Df(0.003f, -0.108f, 0.010f)// BUTTONID_SYSTEM
		}
		,
		/* VR_SIDE_RIGHT */
		{
			Coord3Df(0.030f, -0.085f, -0.030f)// BUTTONID_TRIGGER
			,
			Coord3Df(-0.0275f, -0.149f, -0.012f)// BUTTONID_GRIP
			,
			Coord3Df(-0.011f, -0.0775f, -0.0065f)// BUTTONID_DPADLEFT
			,
			Coord3Df(0.011f, -0.0775f, -0.0065f)// BUTTONID_DPADRIGHT
			,
			Coord3Df(0, -0.0674f, -0.01264f)// BUTTONID_DPADUP
			,
			Coord3Df(0, -0.085f, 0.0015f)// BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_DPAD
			,
			Coord3Df(-0.038f, -0.077f, 0.0065f)// BUTTONID_STICKLEFT
			,
			Coord3Df(-0.024f, -0.077f, 0.0065f)// BUTTONID_STICKRIGHT
			,
			Coord3Df(-0.029f, -0.071f, 0.0001f)// BUTTONID_STICKUP
			,
			Coord3Df(-0.029f, -0.0855f, 0.0105f)// BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)// BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, -0.077f, 0.008f)// BUTTONID_MENU
			,
			Coord3Df(-0.003f, -0.108f, 0.010f)// BUTTONID_SYSTEM
		}
	} /* Type_Microsoft */
	,
	/********************************************
	*			   VR_UI_Type_Fove				*
	********************************************/
	{
		/* VR_SIDE_LEFT */
		{
			Coord3Df(0, 0.115f, -0.300f)// BUTTONID_TRIGGER
			,
			Coord3Df(0, 0.115f, -0.300f)// BUTTONID_GRIP
			,
			Coord3Df(-0.140f, 0.115f, -0.300f)// BUTTONID_DPADLEFT
			,
			Coord3Df(-0.105f, 0.115f, -0.300f)// BUTTONID_DPADRIGHT
			,
			Coord3Df(-0.070f, 0.115f, -0.300f)// BUTTONID_DPADUP
			,
			Coord3Df(-0.035f, 0.115f, -0.300f)// BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_DPAD
			,
			Coord3Df(0.035f, 0.115f, -0.300f)// BUTTONID_STICKLEFT
			,
			Coord3Df(0.070f, 0.115f, -0.300f)// BUTTONID_STICKRIGHT
			,
			Coord3Df(0.105f, 0.115f, -0.300f)// BUTTONID_STICKUP
			,
			Coord3Df(0.140f, 0.115f, -0.300f)// BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)//BUTTONID_THUMBREST
			,
			Coord3Df(-0.018f, 0.135f, -0.300f)// BUTTONID_XA
			,
			Coord3Df(0.018f, 0.135f, -0.300f)// BUTTONID_YB
			,
			Coord3Df(0, 0, 0)//BUTTONID_MENU
			,
			Coord3Df(0, 0, 0)//BUTTONID_SYSTEM
		}
		,
		/* VR_SIDE_RIGHT */
		{
			Coord3Df(0, 0, 0)// BUTTONID_TRIGGER
			,
			Coord3Df(0, 0, 0)// BUTTONID_GRIP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADLEFT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADRIGHT
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADUP
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPADDOWN
			,
			Coord3Df(0, 0, 0)//BUTTONID_DPAD
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKLEFT
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKRIGHT
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKUP
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICKDOWN
			,
			Coord3Df(0, 0, 0)// BUTTONID_STICK
			,
			Coord3Df(0, 0, 0)// BUTTONID_THUMBREST
			,
			Coord3Df(0, 0, 0)// BUTTONID_XA
			,
			Coord3Df(0, 0, 0)// BUTTONID_YB
			,
			Coord3Df(0, 0, 0)// BUTTONID_MENU
			,
			Coord3Df(0, 0, 0)// BUTTONID_SYSTEM
		}
	} /* Type_Fove */
};

const VR_Widget_Layout::Layout VR_Widget_Layout::default_layouts[VR_UI_TYPES][DEFAULTLAYOUTS]
{
	/********************************************
	*			   VR_UI_Type_Null				*
	********************************************/
	{
	/* DefaultLayout_Modeling ======================================================================= */
	{
		std::string("Modeling")
		,
		VR_UI_TYPE_NULL
		,
		/* VR_Widget* m[VR_SIDES][BUTTONIDS][ALTSTATES] */
		{
		/* VR_SIDE_LEFT */
		{
			//BUTTONID_TRIGGER
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_GRIP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_MENU
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_SYSTEM
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
		}
		,
		/* VR_SIDE_RIGHT */
		{
			//BUTTONID_TRIGGER
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_GRIP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_MENU
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_SYSTEM
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
		}
		}
		,
		/* ButtonBit shift_button_bits[VR_SIDES][ALTSTATES] */
		{
			/* VR_SIDE_LEFT */
			{
				// Alt_Off
				BUTTONBIT_NONE
				, // Alt_On
				BUTTONBIT_NONE
			}
			,
			/* VR_SIDE_RIGHT */
			{
				// Alt_Off
				BUTTONBITS_XA
				, // Alt_On
				BUTTONBITS_XA
			}
		}
		,
		/* ButtonBit alt_button_bits[VR_SIDES] */
		{
			/* VR_SIDE_LEFT */
			BUTTONBITS_XA
			,
			/* VR_SIDE_RIGHT */
			BUTTONBIT_NONE
		}
	} /* DefaultLayout_Modeling */
	} /* Type_Null */
	,
	/********************************************
	*			   VR_UI_Type_Oculus			*
	********************************************/
	{
	/* DefaultLayout_Modeling ======================================================================= */
	{
		std::string("Modeling")
		,
		VR_UI_TYPE_OCULUS
		,
		/* VR_Widget* m[VR_SIDES][BUTTONIDS][ALTSTATES] */
		{
		/* VR_SIDE_LEFT */
		{
			//BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SELECT_PROXIMITY)
			}
			,
			//BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
			}
			,
			//BUTTONID_DPADLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
				, // Alt_On
				(VR_Widget*)0
				}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_XA
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
			}
			,
			//BUTTONID_YB
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			//BUTTONID_MENU
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_SYSTEM
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
		}
		,
		/* VR_SIDE_RIGHT */
		{
			//BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ANNOTATE)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
			}
			,
			//BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
			}
			,
			//BUTTONID_DPADLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_XA
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
			}
			,
			//BUTTONID_YB
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			//BUTTONID_MENU
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_SYSTEM
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
		}
		}
		,
		/* ButtonBit shift_button_bits[VR_SIDES][ALTSTATES] */
		{
			/* VR_SIDE_LEFT */
			{
				// Alt_Off
				BUTTONBIT_NONE
				, // Alt_On
				BUTTONBIT_NONE
			}
			,
			/* VR_SIDE_RIGHT */
			{
				// Alt_Off
				BUTTONBITS_XA
				, // Alt_On
				BUTTONBITS_XA
			}
		}
		,
		/* ButtonBit alt_button_bits[VR_SIDES] */
		{
			// VR_SIDE_LEFT
			BUTTONBITS_XA
			,
			// VR_SIDE_RIGHT
			BUTTONBIT_NONE
		}
	} /* DefaultLayout_Modeling */
	} /* Type_Oculus */
	,
	/********************************************
	*			   VR_UI_Type_Vive				*
	********************************************/
	{
	/* DefaultLayout_Modeling ===================================================================== */
	{
		std::string("Modeling")
		,
		VR_UI_TYPE_VIVE
		,
		/* VR_Widget* m[VR_SIDES][BUTTONIDS][ALTSTATES] */
		{
		/* VR_SIDE_LEFT */
		{
			// BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SELECT_PROXIMITY)
			}
			,
			// BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
			}
			,
			// BUTTONID_DPADLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADUP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADDOWN
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_MENU
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			// BUTTONID_SYSTEM
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
		}
		,
		/* VR_SIDE_RIGHT */
		{
			// BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ANNOTATE)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
			}
			,
			// BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
			}
			,
			// BUTTONID_DPADLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADUP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADDOWN
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_MENU
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			// BUTTONID_SYSTEM
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
		}
		}
		,
		/* ButtonBit shift_button_bits[VR_SIDES][ALTSTATES] */
		{
			/* VR_SIDE_LEFT */
			{
				// Alt_Off
				BUTTONBIT_NONE
				, // Alt_On
				BUTTONBIT_NONE
			}
			,
			/* VR_SIDE_RIGHT */
			{
				// Alt_Off
				BUTTONBIT_DPADDOWN
				, // Alt_On
				BUTTONBIT_DPADDOWN
			}
		}
		,
		/* ButtonBit alt_button_bits[VR_SIDES] */
		{
			// VR_SIDE_LEFT
			BUTTONBIT_DPADDOWN
			,
			// VR_SIDE_RIGHT
			BUTTONBIT_NONE
		}
	} /* DefaultLayout_Modeling */
	} /* Type_Vive */
	,
	/********************************************
	*			 VR_UI_Type_Microsoft			*
	********************************************/
	{
	/* DefaultLayout_Modeling ===================================================================== */
	{
		std::string("Modeling")
		,
		VR_UI_TYPE_MICROSOFT
		,
		/* VR_Widget* m[VR_SIDES][BUTTONIDS][ALTSTATES] */
		{
		/* VR_SIDE_LEFT */
		{
			// BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SELECT_PROXIMITY)
			}
			,
			// BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
			}
			,
			// BUTTONID_DPADLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADUP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADDOWN
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
			}
			,
			// BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_STICKLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_STICKRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_STICKUP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			// BUTTONID_STICKDOWN
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_MENU
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_SYSTEM
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
		}
		,
		/* VR_SIDE_RIGHT */
		{ 
			// BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ANNOTATE)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
			}
			,
			// BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI)
			}
			,
			// BUTTONID_DPADLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADUP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_DPADDOWN
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
			}
			,
			// BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_STICKLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_STICKRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_STICKUP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			// BUTTONID_STICKDOWN
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			// BUTTONID_MENU
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			// BUTTONID_SYSTEM
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
		}
		}
		,
		/* ButtonBit shift_button_bits[VR_SIDES][ALTSTATES] */
		{
			/* VR_SIDE_LEFT */
			{
				// Alt_Off
				BUTTONBIT_NONE
				, // Alt_On
				BUTTONBIT_NONE
			}
			,
			/* VR_SIDE_RIGHT */
			{
				// Alt_Off
				BUTTONBIT_DPADDOWN
				, // Alt_On
				BUTTONBIT_DPADDOWN
			}
		}
		,
		/* ButtonBit alt_button_bits[VR_SIDES]; */
		{
			/* VR_SIDE_LEFT */
			BUTTONBIT_DPADDOWN
			,
			/* VR_SIDE_RIGHT */
			BUTTONBIT_NONE
		}
	} /* DefaultLayout_Modeling */
	} /* Type_Microsoft */
	,
	/********************************************
	*			 VR_UI_Type_Fove				*
	********************************************/
	{
	/* DefaultLayout_Modeling ======================================================================= */
	{
		std::string("Modeling")
		,
		VR_UI_TYPE_FOVE
		,
		/* VR_Widget* m[VR_SIDES][BUTTONIDS][ALTSTATES] */
		{
		/* VR_SIDE_LEFT */
		{
			//BUTTONID_TRIGGER
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ANNOTATE)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER)
			}
			,
			//BUTTONID_GRIP
			{
				// Alt_Off
				(VR_Widget*)0 //VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				(VR_Widget*)0 //VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
			}
			,
			//BUTTONID_DPADLEFT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_DPADRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_DPADUP
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_DPADDOWN
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_SELECT_PROXIMITY)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_TELEPORT)
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_GRABAIR)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_CURSOR_OFFSET)
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				(VR_Widget*)0
				, // Alt_On
				(VR_Widget*)0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_XA
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_ALT)
			}
			,
			//BUTTONID_YB
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)
			}
			,
			//BUTTONID_MENU
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_SYSTEM
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
		}
		,
		/* VR_SIDE_RIGHT */
		{
			//BUTTONID_TRIGGER
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_GRIP
			{
				// Alt_Off
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
				, // Alt_On
				VR_Widget::get_widget(VR_Widget::TYPE_NAVI_JOYSTICK)
			}
			,
			//BUTTONID_DPADLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPADDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_DPAD
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKLEFT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKRIGHT
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKUP
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICKDOWN
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_STICK
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_THUMBREST
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_XA
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_YB
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_MENU
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
			,
			//BUTTONID_SYSTEM
			{
				// Alt_Off
				0
				, // Alt_On
				0
			}
		}
		}
		,
		/* ButtonBit shift_button_bits[VR_SIDES][ALTSTATES] */
		{
			/* VR_SIDE_LEFT */
			{
				// Alt_Off
				BUTTONBITS_YB
				, // Alt_On
				BUTTONBITS_YB
			}
			,
			/* VR_SIDE_RIGHT */
			{
				// Alt_Off
				BUTTONBIT_NONE
				, // Alt_On
				BUTTONBIT_NONE
			}
		}
		,
		/* ButtonBit alt_button_bits[VR_SIDES] */
		{
			/* VR_SIDE_LEFT */
			BUTTONBITS_XA
			,
			/* VR_SIDE_RIGHT */
			BUTTONBIT_NONE
		}
	} /* DefaultLayout_Modeling */
	} /* Type_Fove */
};

VR_Widget_Layout::Layout* VR_Widget_Layout::current_layout(0);
std::vector<VR_Widget_Layout::Layout*> VR_Widget_Layout::layouts[VR_UI_TYPES];

VR_Widget_Layout::ButtonID VR_Widget_Layout::buttonBitToID(ButtonBit bit)
{
	if (bit & BUTTONBITS_TRIGGERS) {
		return BUTTONID_TRIGGER;
	}
	if (bit & BUTTONBITS_GRIPS) {
		return BUTTONID_GRIP;
	}
	if (bit & BUTTONBIT_DPADLEFT) {
		return BUTTONID_DPADLEFT;
	}
	if (bit & BUTTONBIT_DPADRIGHT) {
		return BUTTONID_DPADRIGHT;
	}
	if (bit & BUTTONBIT_DPADUP) {
		return BUTTONID_DPADUP;
	}
	if (bit & BUTTONBIT_DPADDOWN) {
		return BUTTONID_DPADDOWN;
	}
	if (bit & (BUTTONBITS_DPADS)) {
		return BUTTONID_DPAD;
	}
	if (bit & BUTTONBIT_STICKLEFT) {
		return BUTTONID_STICKLEFT;
	}
	if (bit & BUTTONBIT_STICKRIGHT) {
		return BUTTONID_STICKRIGHT;
	}
	if (bit & BUTTONBIT_STICKUP) {
		return BUTTONID_STICKUP;
	}
	if (bit & BUTTONBIT_STICKDOWN) {
		return BUTTONID_STICKDOWN;
	}
	if (bit & BUTTONBITS_STICKS) {
		return BUTTONID_STICK;
	}
	if (bit & BUTTONBITS_THUMBRESTS) {
		return BUTTONID_THUMBREST;
	}
	if (bit & BUTTONBITS_XA) {
		return BUTTONID_XA;
	}
	if (bit & BUTTONBITS_YB) {
		return BUTTONID_YB;
	}
	if (bit & BUTTONBIT_MENU) {
		return BUTTONID_MENU;
	}
	if (bit & BUTTONBIT_SYSTEM) {
		return BUTTONID_SYSTEM;
	}

	return BUTTONID_UNKNOWN;
}

VR_Widget_Layout::ButtonBit VR_Widget_Layout::buttonIDToBit(ButtonID id)
{
	switch (id) {
	case BUTTONID_TRIGGER:
		return BUTTONBITS_TRIGGERS;
	case BUTTONID_GRIP:
		return BUTTONBITS_GRIPS;
	case BUTTONID_DPADLEFT:
		return BUTTONBIT_DPADLEFT;
	case BUTTONID_DPADRIGHT:
		return BUTTONBIT_DPADRIGHT;
	case BUTTONID_DPADUP:
		return BUTTONBIT_DPADUP;
	case BUTTONID_DPADDOWN:
		return BUTTONBIT_DPADDOWN;
	case BUTTONID_DPAD:
		return BUTTONBITS_DPADS;
	case BUTTONID_STICKLEFT:
		return BUTTONBIT_STICKLEFT;
	case BUTTONID_STICKRIGHT:
		return BUTTONBIT_STICKRIGHT;
	case BUTTONID_STICKUP:
		return BUTTONBIT_STICKUP;
	case BUTTONID_STICKDOWN:
		return BUTTONBIT_STICKDOWN;
	case BUTTONID_STICK:
		return BUTTONBITS_STICKS;
	case BUTTONID_THUMBREST:
		return BUTTONBITS_THUMBRESTS;
	case BUTTONID_XA:
		return BUTTONBITS_XA;
	case BUTTONID_YB:
		return BUTTONBITS_YB;
	case BUTTONID_MENU:
		return BUTTONBIT_MENU;
	case BUTTONID_SYSTEM:
		return BUTTONBIT_SYSTEM;
	default:
		return (ButtonBit)0; /* unknown */
	}
}

std::string VR_Widget_Layout::buttonIDToString(ButtonID  id)
{
	switch (id) {
	case BUTTONID_TRIGGER:
		return "TRIGGER";
	case BUTTONID_GRIP:
		return "GRIP";
	case BUTTONID_DPADLEFT:
		return "DPADLEFT";
	case BUTTONID_DPADRIGHT:
		return "DPADRIGHT";
	case BUTTONID_DPADUP:
		return "DPADUP";
	case BUTTONID_DPADDOWN:
		return "DPADDOWN";
	case BUTTONID_DPAD:
		return "DPAD";
	case BUTTONID_STICKLEFT:
		return "STICKLEFT";
	case BUTTONID_STICKRIGHT:
		return "STICKRIGHT";
	case BUTTONID_STICKUP:
		return "STICKUP";
	case BUTTONID_STICKDOWN:
		return "STICKDOWN";
	case BUTTONID_STICK:
		return "STICK";
	case BUTTONID_THUMBREST:
		return "THUMBREST";
	case BUTTONID_XA:
		return "XA";
	case BUTTONID_YB:
		return "YB";
	case BUTTONID_MENU:
		return "MENU";
	case BUTTONID_SYSTEM:
		return "SYSTEM";
	case BUTTONID_UNKNOWN:
	default:
		return "UNKNOWN";
	}
}

VR_Widget_Layout::ButtonID VR_Widget_Layout::buttonStringToID(const std::string  str)
{
	ButtonID btn;
	if (str == "TRIGGER") {
		btn = BUTTONID_TRIGGER;
	}
	else if (str == "GRIP") {
		btn = BUTTONID_GRIP;
	}
	else if (str == "DPADLEFT") {
		btn = BUTTONID_DPADLEFT;
	}
	else if (str == "DPADRIGHT") {
		btn = BUTTONID_DPADRIGHT;
	}
	else if (str == "DPADUP") {
		btn = BUTTONID_DPADUP;
	}
	else if (str == "DPADDOWN") {
		btn = BUTTONID_DPADDOWN;
	}
	else if (str == "DPAD") {
		btn = BUTTONID_DPAD;
	}
	else if (str == "STICKLEFT") {
		btn = BUTTONID_STICKLEFT;
	}
	else if (str == "STICKRIGHT") {
		btn = BUTTONID_STICKRIGHT;
	}
	else if (str == "STICKUP") {
		btn = BUTTONID_STICKUP;
	}
	else if (str == "STICKDOWN") {
		btn = BUTTONID_STICKDOWN;
	}
	else if (str == "STICK") {
		btn = BUTTONID_STICK;
	}
	else if (str == "THUMBREST") {
		btn = BUTTONID_THUMBREST;
	}
	else if (str == "XA") {
		btn = BUTTONID_XA;
	}
	else if (str == "YB") {
		btn = BUTTONID_YB;
	}
	else if (str == "MENU") {
		btn = BUTTONID_MENU;
	}
	else if (str == "SYSTEM") {
		btn = BUTTONID_SYSTEM;
	}
	else {
		btn = BUTTONID_UNKNOWN;
	}
	return btn;
}

VR_Widget_Layout::VR_Widget_Layout()
{
	//
}

VR_Widget_Layout::~VR_Widget_Layout()
{
	if (current_layout) {
		delete current_layout;
		current_layout = 0;
	}

	for (int i = 0; i < VR_UI_TYPES; ++i) {
		for (auto it = layouts[i].begin(); it != layouts[i].end(); ++it) {
			if (*it) {
				delete *it;
			}
		}
		layouts[i].clear();
	}
}

VR_UI::Error VR_Widget_Layout::reset_to_default_layouts()
{
	VR_UI* ui = VR_UI::i();
	if (!ui) {
		return VR_UI::ERROR_INTERNALFAILURE;
	}
	uint type = ui->type();

	/* Delete all prior layouts. */
	for (auto it = layouts[type].begin(); it != layouts[type].end(); ++it) {
		if (*it) {
			delete *it;
		}
	}
	layouts[type].clear();

	/* Set up default UI layouts. */
	for (int i = 0; i < DEFAULTLAYOUTS; ++i) {
		Layout* map = new Layout();
		map->type = (VR_UI_Type)type;
		*map = default_layouts[type][i];
		layouts[type].push_back(map);
	}
	current_layout = layouts[type][0];

	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::get_current_layout(std::string& layout)
{
	if (!current_layout) {
		return VR_UI::ERROR_INVALIDPARAMETER;
	}

	layout = current_layout->name;
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::set_current_layout(const std::string& layout_name)
{
	VR_UI* ui = VR_UI::i();
	if (!ui) {
		return VR_UI::ERROR_INTERNALFAILURE;
	}
	uint type = ui->type();

	Layout* layout = 0;
	for (int i = 0; i < layouts[type].size(); ++i) {
		if (layouts[type][i]->name == layout_name) {
			layout = layouts[type][i];
			break;
		}
	}
	if (!layout) {
		layout = new Layout();
		layout->name = layout_name;
		layout->type = (VR_UI_Type)type;
		memset(layout->m, 0, sizeof(layout->m));
		/* At a minimim, define the TRIGGER and GRIP button */
		layout->m[VR_SIDE_LEFT][BUTTONID_TRIGGER][0] = VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER);
		layout->m[VR_SIDE_RIGHT][BUTTONID_TRIGGER][0] = VR_Widget::get_widget(VR_Widget::TYPE_TRIGGER);
		layout->m[VR_SIDE_LEFT][BUTTONID_GRIP][0] = VR_Widget::get_widget(VR_Widget::TYPE_NAVI);
		layout->m[VR_SIDE_RIGHT][BUTTONID_GRIP][0] = VR_Widget::get_widget(VR_Widget::TYPE_NAVI);
		/* Add to the list */
		layouts[type].push_back(layout);
	}
	current_layout = layout;
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::rename_current_layout(const std::string& new_name)
{
	if (!current_layout) {
		return VR_UI::ERROR_INVALIDPARAMETER;
	}
	uint type = current_layout->type;

	/* Search if the other name is already taken */
	for (int i = int(layouts[type].size() - 1); i >= 0; i--) {
		if (layouts[type][i] != current_layout && layouts[type][i]->name == new_name) {
			return VR_UI::ERROR_INVALIDPARAMETER;
		}
	}
	/* else: name not taken yet */
	current_layout->name = new_name;
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::delete_current_layout()
{
	if (!current_layout) {
		return VR_UI::ERROR_INVALIDPARAMETER;
	}
	uint type = current_layout->type;

	/* Delete the layout from the list */
	for (auto it = layouts[type].begin(); it != layouts[type].end(); ++it) {
		if (*it && *it == current_layout) {
			delete *it;
			layouts[type].erase(it);
			current_layout = (layouts[type].size() > 0) ? layouts[type][0] : 0;
			return VR_UI::ERROR_NONE;
		}
	}
	/* else: not found? Should not happen */
	return VR_UI::ERROR_INVALIDPARAMETER;
}

VR_UI::Error VR_Widget_Layout::get_layouts_list(std::vector<std::string>& list)
{
	VR_UI* ui = VR_UI::i();
	if (!ui) {
		return VR_UI::ERROR_INTERNALFAILURE;
	}
	uint type = ui->type();

	for (int i = 0; i < layouts[type].size(); ++i) {
		list.push_back(layouts[type][i]->name);
	}
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::map_widget(VR_Side side, std::string event, VR_UI::AltState alt, std::string widget)
{
	if (!current_layout) {
		return VR_UI::ERROR_INVALIDPARAMETER;
	}

	VR_Widget* m = 0;
	if (widget.length() > 0 && widget != "UNKNOWN") {
		m = VR_Widget::get_widget(widget);
		if (!m) {
			return VR_UI::ERROR_INVALIDPARAMETER;
		}
	}

	ButtonID btn;
	if (event == "TRIGGER") {
		btn = BUTTONID_TRIGGER;
	}
	else if (event == "GRIP") {
		btn = BUTTONID_GRIP;
	}
	else if (event == "DPADLEFT") {
		btn = BUTTONID_DPADLEFT;
	}
	else if (event == "DPADRIGHT") {
		btn = BUTTONID_DPADRIGHT;
	}
	else if (event == "DPADUP") {
		btn = BUTTONID_DPADUP;
	}
	else if (event == "DPADDOWN") {
		btn = BUTTONID_DPADDOWN;
	}
	else if (event == "DPAD") {
		btn = BUTTONID_DPAD;
	}
	else if (event == "STICKLEFT") {
		btn = BUTTONID_STICKLEFT;
	}
	else if (event == "STICKRIGHT") {
		btn = BUTTONID_STICKRIGHT;
	}
	else if (event == "STICKUP") {
		btn = BUTTONID_STICKUP;
	}
	else if (event == "STICKDOWN") {
		btn = BUTTONID_STICKDOWN;
	}
	else if (event == "STICK") {
		btn = BUTTONID_STICK;
	}
	else if (event == "THUMBREST") {
		btn = BUTTONID_THUMBREST;
	}
	else if (event == "XA") {
		btn = BUTTONID_XA;
	}
	else if (event == "YB") {
		btn = BUTTONID_YB;
	}
	else if (event == "MENU") {
		btn = BUTTONID_MENU;
	}
	else if (event == "SYSTEM") {
		btn = BUTTONID_SYSTEM;
	}
	else {
		return VR_UI::ERROR_INVALIDPARAMETER;
	}

	/* If prior mapping was SHIFT or ALT, remove it from the shift/alt-bits */
	if (current_layout->m[side][btn][alt] == VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)) {
		ButtonBit bit = buttonIDToBit(btn);
		current_layout->shift_button_bits[side][alt] = ButtonBit(current_layout->shift_button_bits[side][alt] & (~bit));
	}
	if (current_layout->m[side][btn][alt] == VR_Widget::get_widget(VR_Widget::TYPE_ALT)) {
		ButtonBit bit = buttonIDToBit(btn);
		current_layout->alt_button_bits[side] = ButtonBit(current_layout->alt_button_bits[side] & (~bit));
	}
	/* If the new mapping is SHIFT or ALT, add it to the shift/alt-bits */
	if (m == VR_Widget::get_widget(VR_Widget::TYPE_SHIFT)) {
		ButtonBit bit = buttonIDToBit(btn);
		current_layout->shift_button_bits[side][alt] = ButtonBit(current_layout->shift_button_bits[side][alt] | bit);
	}
	if (m == VR_Widget::get_widget(VR_Widget::TYPE_ALT)) {
		ButtonBit bit = buttonIDToBit(btn);
		current_layout->alt_button_bits[side] = ButtonBit(current_layout->alt_button_bits[side] | bit);
	}

	current_layout->m[side][btn][alt] = m;
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::unmap_widget(const std::string& widget)
{
	VR_UI* ui = VR_UI::i();
	if (!ui) {
		return VR_UI::ERROR_INTERNALFAILURE;
	}
	uint type = ui->type();

	VR_Widget* w = VR_Widget::get_widget(widget);
	for (auto it = layouts[type].begin(); it != layouts[type].end(); ++it) {
		for (int side = 0; side < 2; ++side) {
			for (int button = 0; button < BUTTONIDS; ++button) {
				for (int alt = 0; alt < 2; ++alt) {
					if ((*it) && (*it)->m[side][button][alt] == w) {
						(*it)->m[side][button][alt] = 0; /* unmap */
					}
				}
			}
		}
	}
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::get_mapped_widget(VR_Side side, const std::string& event, VR_UI::AltState alt, std::string& widget)
{
	if (!current_layout) {
		widget = "";
		return VR_UI::ERROR_NOTAVAILABLE;
	}
	ButtonID btn = buttonStringToID(event);
	if (btn == BUTTONID_UNKNOWN) {
		widget = "";
		return VR_UI::ERROR_NOTAVAILABLE;
	}

	VR_Widget* w = current_layout->m[side][btn][alt];

	/* If none is mapped, return empty string, else the widget name */
	widget = (w) ? w->name() : "";
	return VR_UI::ERROR_NONE;
}

VR_UI::Error VR_Widget_Layout::get_mapped_event(const std::string& widget, VR_Side& side, std::string& event, VR_UI::AltState& alt)
{
	return get_mapped_event(current_layout, widget, side, event, alt);
}

VR_UI::Error VR_Widget_Layout::get_mapped_event(const VR_Widget* widget, VR_Side& side, std::string& event, VR_UI::AltState& alt)
{
	return get_mapped_event(current_layout, widget, side, event, alt);
}

VR_UI::Error VR_Widget_Layout::get_mapped_event(const Layout* layout, const std::string& widget, VR_Side& side, std::string& event, VR_UI::AltState& alt)
{
	return get_mapped_event(layout, VR_Widget::get_widget(widget), side, event, alt);
}

VR_UI::Error VR_Widget_Layout::get_mapped_event(const Layout* layout, const VR_Widget* widget, VR_Side& side, std::string& event, VR_UI::AltState& alt)
{
	ButtonID button;

	VR_UI::Error e = get_mapped_event(layout, widget, side, button, alt);
	if (e == VR_UI::ERROR_NONE) {
		event = buttonIDToString(button);
	}

	return e;
}

VR_UI::Error VR_Widget_Layout::get_mapped_event(const Layout* layout, const VR_Widget* widget, VR_Side& side, ButtonID& event, VR_UI::AltState& alt)
{
	if (!layout || !widget) {
		VR_UI::ERROR_INVALIDPARAMETER;
	}

	for (int s = 0; s < 2; ++s) {
		for (int b = 0; b < BUTTONIDS; ++b) {
			for (int a = 0; a < VR_UI::ALTSTATES; ++a) {
				if (layout->m[s][b][a] == widget) {
					side = VR_Side(s);
					event = ButtonID(b);
					alt = VR_UI::AltState(a);
					return VR_UI::ERROR_NONE;
				}
			}
		}
	}

	return VR_UI::ERROR_NOTAVAILABLE;
}

VR_UI::Error VR_Widget_Layout::get_events_list(std::vector<std::string>& events)
{
	events.push_back("TRIGGER");
	events.push_back("GRIP");
	events.push_back("DPADLEFT");
	events.push_back("DPADRIGHT");
	events.push_back("DPADUP");
	events.push_back("DPADDOWN");
	events.push_back("DPAD");
	events.push_back("STICKLEFT");
	events.push_back("STICKRIGHT");
	events.push_back("STICKUP");
	events.push_back("STICKDOWN");
	events.push_back("STICK");
	events.push_back("THUMBREST");
	events.push_back("XA");
	events.push_back("YB");
	events.push_back("MENU");
	events.push_back("SYSTEM");

	return VR_UI::ERROR_NONE;
}