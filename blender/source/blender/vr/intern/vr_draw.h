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

/** \file blender/vr/intern/vr_draw.h
*   \ingroup vr
*/

#ifndef __VR_DRAW_H__
#define __VR_DRAW_H__

class VR_Draw
{
	VR_Draw();	/* Constructor. */
	virtual	~VR_Draw();	/* Destructor. */
public:
	static bool	initialized;	/* Whether the draw module has been initialized. */
#ifdef WIN32
	static int init(void* device, void* context);	/* Initialize OpenGL objects. */
#else
	static int init(void* display, void* drawable, void* context);	/* Initialize OpenGL objects. */
#endif
	static void	uninit(); /* Un-initialize OpenGL objects. */
protected:
#ifdef WIN32
	static void *device;	/* OpenGL graphics device for rendering (HDC). */
	static void *context;	/* OpenGL rendering context (HGLRC). */
	//static HDC   device;  /* OpenGL graphics device for rendering. */
	//static HGLRC context; /* OpenGL rendering context. */
#else
	static void *display;	/* The connection to the X server (Display*). */
	static void *drawable;	/* Pointer to the GLX drawable (GLXDrawable*). Either an X window ID or a GLX pixmap ID. */
	static void *context;	/* Pointer to the GLX rendering context (GLXContext*) attached to drawable. */
	//static Display* display;		/* The connection to the X server. */
	//static GLXDrawable drawable;	/* The GLX drawable. Either an X window ID or a GLX pixmap ID. */
	//static GLXContext context;	/* The GLX rendering context attached to the GLX drawable. */
#endif
	/* OpenGL shader set. */
	typedef struct Shader {
		uint	program;			/* Shader program handle. */
		uint	vertex_shader;		/* Vertex shader handle. */
		uint	fragment_shader;	/* Fragment shader handle. */

		int		position_location;	/* Location of the shader position vector. */
		int		normal_location;	/* Location of the shader normal vector. */
		int		uv_location;		/* Location of the shader uv vector. */
		int		modelview_location; /* Location of the shader modelview matrix. */
		int		projection_location;/* Location of the shader projection matrix. */
		int		normal_matrix_location;	/* Location of the shader normal matrix. normal_matrix = transpose(inverse(modelview)). */
		int		color_location;		/* Location of the shader color vector. */
		int		sampler_location;	/* Location of the shader sampler. */
		
		Shader();	/* Default constructor. */
		~Shader();  /* Default destructor. */
		int create(const char* vs, const char* fs, bool has_tex);	/* Create shader program from source. */
		int release();	/* Release allocated objects. */

		static uint		color_shader();	/* The default (non-textured) shader. */
		static uint		texture_shader();/* The default textured shader. */
		static Shader	shader_col;	/* The default (non-textured) shader. */
		static Shader   shader_tex; /* The default textured shader. */
	private:
		static const char* const shader_col_vsource;	/* Vertex shader source code for the default (non-textured) shader. */
		static const char* const shader_col_fsource;	/* Fragment shader source code for the default (non-textured) shader. */
		static const char* const shader_tex_vsource;	/* Vertex shader source code for the textured shader. */
		static const char* const shader_tex_fsource;	/* Fragment shader source code for the textured shader. */
	} Shader;

	/* Texture objects and global texture library (texture ID and metainfo). */
	class Texture
	{
	public:
		/* OpenGL implementation of a 2D texture. */
		typedef struct TextureImplementation {
		private:
			uint	texture_id;	/* OpenGL texture ID. */
		public:
			uint	width;	/* Width of the texture in pixels. */
			uint	height; /* Height of the texture in pixels. */
			uint	depth;  /* Depth of the texture in bytes per pixel. */
			TextureImplementation(const uchar* png_blob);	/* Create texture from PNG blob in memory. */
			virtual ~TextureImplementation();	/* Destructor. */
			virtual void bind();	/* Bind the texture instance for rendering. */
			virtual void unbind();	/* Unbind last bound texture from the context. */
		} TextureImplementation; /* OpenGL implementation of a 2D texture. */

		const uchar*	png_blob;	/* Binary image data buffer (in PNG format), if created from PNG. */
		
		TextureImplementation* implementation;	/* API dependent instance of this texture. */
		bool create_implementation();	/* Internal helper function to generate the implementation object. */
	public:
		Texture(const uchar* png_blob);	/* Create/get texture from PNG format block in memory. */
		Texture(const Texture& cpy);	/* Copy constructor. */
		~Texture();	/* Destructor. */

		Texture& operator= (const Texture& o);	/* Assignment operator. */

		uint width();	/* Get image width (in pixels). */
		uint height();	/* Get image height (in pixels). */

		void bind();	/* Bind the texture for rendering. */
		void unbind();	/* Unbind currently bound texture. */
	};

	/* 3D Model (vertex buffer) object. */
	typedef struct Model {
	private:
		uint	verts;	/* Vertex buffer for the model. */
		uint	nrmls;  /* Normal buffer for the model. */
		uint	uvs;    /* Texture coordinate buffer for the model. */
		uint	vertex_array;	/* Vertex array. */
		uint	num_verts;		/* Number of vertices in the buffer. */
	public:
		Texture* texture;	/* Texture for the model (if any). */
		Model();	/* Constructor. */
		virtual ~Model();	/* Destructor. */
		static Model* create(float* verts, float* nrmls, float* uvs, uint num_verts);	/* Create model from buffers. */

		virtual int render();	/* Render the model. */
		virtual int render(const Mat44f& pos);	/* Render the model at the given position. */
	} Model;
public:
	/* Controller models / textures. */
	static Model *controller_model[VR_SIDES]; /* Controller models (left and right). */
	static Texture *controller_tex;	/* Textures for the controller. */
	static Model *vr_cursor_model;	/* Model for the VR cursor. */
	static Texture *vr_cursor_tex; /* Texture for the VR cursor. */
	static Texture *cursor_tex;	/* Texture for the Blender cursor. */
	static Texture *mouse_cursor_tex; /* Texture for the mouse cursor. */

	static int create_controller_models(VR_UI_Type type);	/* Create the controller models for a specified UI type. */
	
	/* Image textures. */
	static Texture *nav_grabair_tex;	/* Texture for the "nav grabair" icon. */
	static Texture *nav_joystick_tex;	/* Texture for the "nav joystick" icon. */
	static Texture *nav_teleport_tex;	/* Texture for the "nav teleport" icon. */
	static Texture *nav_locktrans_tex;	/* Texture for the "nav locktrans* icon. */
	static Texture *nav_locktransup_tex;	/* Texture for the "nav locktransup" icon */
	static Texture *nav_lockrot_tex;	/* Texture for the "nav lockrot" icon. */
	static Texture *nav_lockrotup_tex;	/* Texture for the "nav lockrotup" icon. */
	static Texture *nav_lockscale_tex;	/* Texture for the "nav lockscale" icon. */
	static Texture *nav_lockscalereal_tex;	/* Texture for the "nav lockscalereal" icon. */
	static Texture *ctrl_tex;	/* Texture for the "ctrl" icon. */
	static Texture *shift_tex;	/* Texture for the "shift" icon. */
	static Texture *alt_tex;	/* Texture for the "alt" icon. */
	static Texture *select_tex;	/* Texture for the "select" icon. */
	static Texture *select_raycast_tex;	/* Texture for the "select raycast" icon. */
	static Texture *select_proximity_tex;	/* Texture for the "select proximity" icon. */
	static Texture *cursor_teleport_tex;	/* Texture for the "teleport to cursor" icon. */
	static Texture *cursor_worldorigin_tex;	/* Texture for the "set cursor to world origin" icon. */
	static Texture *cursor_objorigin_tex;	/* Texture for the "set cursor to object origin" icon. */
	static Texture *transform_tex;	/* Texture for the "transform" icon. */
	static Texture *move_tex;	/* Texture for the "move" icon. */
	static Texture *rotate_tex;	/* Texture for the "rotate" icon. */
	static Texture *scale_tex;	/* Texture for the "scale" icon. */
	static Texture *annotate_tex;	/* Texture for the "annotate" icon. */
	static Texture *measure_tex;	/* Texture for the "measure" icon. */
	static Texture *mesh_tex;	/* Texture for the "mesh" icon. */
	static Texture *mesh_plane_tex;	/* Texture for the "mesh plane" icon. */
	static Texture *mesh_cube_tex;	/* Texture for the "mesh cube" icon. */
	static Texture *mesh_circle_tex;	/* Texture for the "mesh circle" icon. */
	static Texture *mesh_cylinder_tex;	/* Texture for the "mesh cylinder" icon. */
	static Texture *mesh_cone_tex;	/* Texture for the "mesh cone" icon. */
	static Texture *mesh_grid_tex;	/* Texture for the "mesh grid" icon. */
	static Texture *mesh_monkey_tex;	/* Texture for the "mesh monkey" icon. */
	static Texture *mesh_uvsphere_tex;	/* Texture for the "mesh uvsphere" icon. */
	static Texture *mesh_icosphere_tex;	/* Texture for the "mesh icosphere" icon. */
	static Texture *extrude_tex;	/* Texture for the "extrude* icon. */
	static Texture *extrude_individual_tex;	/* Texture for the "extrude individual* icon. */
	static Texture *extrude_normals_tex;	/* Texture for the "extrude normals* icon. */
	static Texture *insetfaces_tex;	/* Texture for the "inset faces" icon. */
	static Texture *bevel_tex;	/* Texture for the "bevel" icon. */
	static Texture *loopcut_tex;	/* Texture for the "loopcut" icon. */
	static Texture *knife_tex;	/* Texture for the "knife" icon. */
	static Texture *delete_tex;	/* Texture for the "delete" icon. */
	static Texture *delete_alt_tex; /* Texture for the "updated delete" icon. */
	static Texture *duplicate_tex;	/* Texture for the "duplicate" icon. */
	static Texture *join_tex;	/* Texture for the "join" icon. */
	static Texture *separate_tex;	/* Texture for the "separate" icon. */
	static Texture *undo_tex;	/* Texture for the "undo" icon. */
	static Texture *redo_tex;	/* Texture for the "redo" icon. */
	static Texture *manip_global_tex;	/* Texture for the "global manipulator" icon. */
	static Texture *manip_local_tex;	/* Texture for the "local manipulator" icon. */
	static Texture *manip_normal_tex;	/* Texture for the "normal manipulator" icon. */
	static Texture *manip_plus_tex;	/* Texture for the "grow manipulator" icon. */
	static Texture *manip_minus_tex;	/* Texture for the "shrink manipulator" icon. */
	static Texture *objectmode_tex;	/* Texture for the "object mode" icon. */
	static Texture *editmode_tex;	/* Texture for the "edit mode" icon. */
	static Texture *object_tex;	/* Texture for the "object" icon. */
	static Texture *vertex_tex;	/* Texture for the "vertex" icon. */
	static Texture *edge_tex;	/* Texture for the "edge" icon. */
	static Texture *face_tex;	/* Texture for the "face" icon. */
	static Texture *toolsettings_tex;	/* Texture for the "toolsettings" icon. */
	static Texture *box_empty_tex;	/* Texture for the "box empty" icon. */
	static Texture *box_filled_tex;	/* Texture for the "box filled" icon. */
	static Texture *plus_tex;	/* Texture for the "plus" icon. */
	static Texture *minus_tex;	/* Texture for the "minus" icon. */
	static Texture *reset_tex;	/* Texture for the "reset" icon. */

	/* Menu textures. */
	static Texture *background_menu_tex;	/* Texture for the menu background. */
	static Texture *colorwheel_menu_tex;	/* Texture for the "colorwheel" menu. */

	/* String textures. */
	static Texture *ascii_tex;	/* Texture for the ascii characters. */
	static Texture *on_str_tex;	/* Texture for the "ON" string. */
	static Texture *off_str_tex;	/* Texture for the "OFF" string. */
	static Texture *x_str_tex;	/* Texture for the "X" string. */
	static Texture *y_str_tex;	/* Texture for the "Y" string. */
	static Texture *z_str_tex;	/* Texture for the "Z" string. */
	static Texture *xy_str_tex;	/* Texture for the "XY" string. */
	static Texture *yz_str_tex;	/* Texture for the "YZ" string. */
	static Texture *zx_str_tex;	/* Texture for the "ZX" string. */
protected:
	static Mat44f	model_matrix;	/* OpenGL model matrix. */ 
	static Mat44f	view_matrix;	/* OpenGL view matrix. */
	static Mat44f	projection_matrix;	/* OpenGL projection matrix. */
	static Mat44f	modelview_matrix;	/* OpenGL modelview matrix. */
	static Mat44f	modelview_matrix_inv;	/* OpenGL modelview matrix inverse. */
	static float	color_vector[4];	/* OpenGL color vector. */
public:
	static const Mat44f& get_model_matrix();	/* Get the current model matrix. */
	static const Mat44f& get_view_matrix();	/* Get the current view matrix. */
	static const Mat44f& get_projection_matrix();	/* Get the current projection matrix. */
	static const float*  get_color();	/* Get the current object / render color. */

	static void update_model_matrix(const float _model[4][4]);	/* Set the current model matrix. */
	static void update_view_matrix(const float _view[4][4]);	/* Set the current view matrix. */
	static void update_projection_matrix(const float _projection[4][4]);	/* Set the current projection matrix. */
	static void update_modelview_matrix(const Mat44f* _model, const Mat44f* _view);	/* Set the current modelview matrix. */
	static void set_color(const float color[4]);	/* Set the current object / render color. */
	static void set_color(const float& r, const float& g, const float& b, const float& a);	/* Set the current object / render color. */
	static void set_blend(bool on_off);	/* Enable/disable alpha blending. */
	static void set_depth_test(bool on_off, bool write_depth);	/* Enable / disable depth testing. */
	
	static void render_rect(float left, float right, float top, float bottom, float z, float u=1.0f, float v=1.0f, Texture* tex=0);	/* Render a rectangle with currently set transformation. */
	static void render_frame(float left, float right, float top, float bottom, float b, float z = 0);	/* Render a flat frame. */
	static void render_box(const Coord3Df& p0, const Coord3Df& p1, bool outline=false);	/* Render an axis-aligned box. */
	static void render_ball(float r, bool golf=false);	/* Render a ball with currently set transformation. */
	static void render_arrow(const Coord3Df& from, const Coord3Df& to, float width);	/* Render an arrow. */
	static void render_string(const char* str, float character_width, float character_height, VR_HAlign h_align, VR_VAlign v_align, float x_offset = 0.0f, float y_offset = 0.0f, float z_offset = 0.0f);	/* Render a string. */
};

#endif /* __VR_DRAW_H__ */
