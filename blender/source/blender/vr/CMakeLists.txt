# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	./intern
	./img
	./obj
	./extern/inc/libpng_1.6.35
	./extern/inc/zlib_1.2.11
	../../../extern/Eigen3
	../../../extern/glew/include/GL
	../blenkernel
	../blenlib
	../blentranslation
	../bmesh
	../depsgraph
	../draw
	../draw/intern
	../editors/include
	../editors/gizmo_library
	../editors/gpencil
	../editors/mesh
	../editors/transform
	../gpu
	../makesdna
	../makesrna
	../render/extern/include
	../windowmanager
	../windowmanager/gizmo
	../../../intern/glew-mx
	../../../intern/guardedalloc
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/vr_main.c
	intern/vr_draw.cpp
	intern/vr_math.cpp
	intern/vr_ui.cpp
	intern/vr_util.cpp
	intern/vr_widget.cpp
	intern/vr_widget_addprimitive.cpp
	intern/vr_widget_alt.cpp
	intern/vr_widget_annotate.cpp
	intern/vr_widget_bevel.cpp
	intern/vr_widget_ctrl.cpp
	intern/vr_widget_cursor.cpp
	intern/vr_widget_cursoroffset.cpp
	intern/vr_widget_delete.cpp
	intern/vr_widget_duplicate.cpp
	intern/vr_widget_extrude.cpp
	intern/vr_widget_insetfaces.cpp
	intern/vr_widget_join.cpp
	intern/vr_widget_knife.cpp
	intern/vr_widget_layout.cpp
	intern/vr_widget_loopcut.cpp
	intern/vr_widget_measure.cpp
	intern/vr_widget_menu.cpp
	intern/vr_widget_navi.cpp
	intern/vr_widget_redo.cpp
	intern/vr_widget_select.cpp
	intern/vr_widget_shift.cpp
	intern/vr_widget_separate.cpp
	intern/vr_widget_switchcomponent.cpp
	intern/vr_widget_switchlayout.cpp
	intern/vr_widget_switchspace.cpp
	intern/vr_widget_switchtool.cpp
	intern/vr_widget_transform.cpp
	intern/vr_widget_undo.cpp
	
	vr_build.h
	vr_api.h
	vr_main.h
	intern/vr_types.h
	intern/vr_draw.h
	intern/vr_math.h
	intern/vr_ui.h
	intern/vr_util.h
	intern/vr_widget.h
	intern/vr_widget_addprimitive.h
	intern/vr_widget_alt.h
	intern/vr_widget_annotate.h
	intern/vr_widget_bevel.h
	intern/vr_widget_ctrl.h
	intern/vr_widget_cursor.h
	intern/vr_widget_cursoroffset.h
	intern/vr_widget_delete.h
	intern/vr_widget_duplicate.h
	intern/vr_widget_extrude.h
	intern/vr_widget_insetfaces.h
	intern/vr_widget_join.h
	intern/vr_widget_knife.h
	intern/vr_widget_layout.h
	intern/vr_widget_loopcut.h
	intern/vr_widget_measure.h
	intern/vr_widget_menu.h
	intern/vr_widget_navi.h
	intern/vr_widget_redo.h
	intern/vr_widget_select.h
	intern/vr_widget_shift.h
	intern/vr_widget_separate.h
	intern/vr_widget_switchcomponent.h
	intern/vr_widget_switchlayout.h
	intern/vr_widget_switchspace.h
	intern/vr_widget_switchtool.h
	intern/vr_widget_transform.h
	intern/vr_widget_undo.h
)

add_definitions(${GL_DEFINITIONS})

if(WITH_X11)
	add_definitions(-DWITH_X11)
endif()

blender_add_lib(bf_vr "${SRC}" "${INC}" "${INC_SYS}")
