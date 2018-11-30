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
	../bmesh/intern
	../depsgraph
	../draw
	../draw/intern
	../editors/include
	../editors/gizmo_library
	../editors/gpencil
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
	intern/vr_widget.cpp
	intern/vr_widget_layout.cpp
	
	vr_build.h
	vr_api.h
	vr_main.h
	intern/vr_types.h
	intern/vr_draw.h
	intern/vr_math.h
	intern/vr_ui.h
	intern/vr_widget.h
	intern/vr_widget_layout.h
)

add_definitions(${GL_DEFINITIONS})

if(WITH_X11)
	add_definitions(-DWITH_X11)
endif()

blender_add_lib(bf_vr "${SRC}" "${INC}" "${INC_SYS}")
