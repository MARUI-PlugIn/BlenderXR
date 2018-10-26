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
	static int	init(void* device, void* context);	/* Initialize OpenGL objects. */
#else
	static int	init(void* display, void* drawable, void* context);	/* Initialize OpenGL objects. */
#endif
	static void	uninit(); /* Un-initialize OpenGL objects. */
protected:
#ifdef WIN32
	static void* device;	/* OpenGL graphics device for rendering (HDC). */
	static void* context;	/* OpenGL rendering context (HGLRC). */
	//static HDC   device;  /* OpenGL graphics device for rendering. */
	//static HGLRC context; /* OpenGL rendering context. */
#else
	static void* display;	/* The connection to the X server (Display*). */
	static void* drawable;	/* Pointer to the GLX drawable (GLXDrawable*). Either an X window ID or a GLX pixmap ID. */
	static void* context;	/* Pointer to the GLX rendering context (GLXContext*) attached to drawable. */
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
	static Model*	controller_model[VR_SIDES]; /* Controller models (left and right). */
	static Texture* controller_tex;	/* Textures for the controller. */
	static Model*	cursor_model;	/* Model for the cursor. */
	static Texture* cursor_tex; /* Texture for the cursor. */
	static Texture* crosshair_cursor_tex;	/* Texture for the "crosshair cursor" icon. */

	static int create_controller_models(VR_UI_Type type);	/* Create the controller models for a specified UI type. */
	
	/* Image textures. */
	static Texture *ascii_tex;	/* Texture for the ascii characters. */
	static Texture *zoom_tex;	/* Texture for the "zoom" icon. */
	static Texture *close_tex;	/* Texture for the "close" icon. */
	static Texture *nav_tex;	/* Texture for the "nav" icon. */
	static Texture *nav_joystick_tex;	/* Texture for the "nav joystick" icon. */
	static Texture *nav_teleport_tex;	/* Texture for the "nav teleport" icon. */
	static Texture *ctrl_tex;	/* Texture for the "ctrl" icon. */
	static Texture *shift_tex;	/* Texture for the "shift" icon. */
	static Texture *alt_tex;	/* Texture for the "alt" icon. */
	static Texture *cursoroffset_tex;	/* Texture for the "cursor offset" icon. */
	static Texture *delete_tex;	/* Texture for the "delete" icon. */
	static Texture *duplicate_tex;	/* Texture for the "duplicate" icon. */
	static Texture *undo_tex;	/* Texture for the "undo" icon. */
	static Texture *redo_tex;	/* Texture for the "redo" icon. */
	static Texture *colorwheel_menu_tex;	/* Texture for the "color wheel" menu. */
	static Texture *colorwheel_menu_triangle_tex;	/* Texture for the "triangle" icon for the "color wheel" menu . */
	static Texture *colorwheel_menu_trianglecancel_tex;	/* Texture for the "triangle cancel" icon for the "color wheel" menu. */

	/* String textures. */
	static Texture *select_str_tex;	/* Texture for the "SELECT" string. */
	static Texture *quickgrab_str_tex;	/* Texture for the "QUICKGRAB" string. */
	static Texture *annotate_str_tex;	/* Texture for the "ANNOTATE" string. */
	static Texture *measure_str_tex;	/* Texture for the "MEASURE" string. */
	static Texture *raycast_str_tex;	/* Texture for the "RAYCAST" string. */
	static Texture *proximity_str_tex;	/* Texture for the "PROXIMITY" string. */
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
	static void render_box(const Coord3Df& p0, const Coord3Df& p1);	/* Render an axis-aligned box. */
	static void render_ball(float r, bool golf=false);	/* Render a ball with currently set transformation. */
	static void render_arrow(const Coord3Df& from, const Coord3Df& to, float width, float u, float v, Texture* tex);	/* Render an arrow. */
	static void render_string(const char* str, float character_width, float character_height, VR_HAlign h_align, VR_VAlign v_align, float x_offset = 0.0f, float y_offset = 0.0f, float z_offset = 0.0f);	/* Render a string. */
};

#endif /* __VR_DRAW_H__ */
