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

/** \file blender/vr/intern/vr_widget_sculpt.cpp
 *   \ingroup vr
 *
 */

#include "vr_types.h"
#include <list>
#include <assert.h>

#include "vr_main.h"
#include "vr_ui.h"

#include "vr_widget_sculpt.h"
#include "vr_widget_transform.h"
#include "vr_widget_switchcomponent.h"

#include "vr_draw.h"
#include "vr_math.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_gsqueue.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_subsurf.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
//#include "BKE_screen.h"
struct ScrArea *BKE_screen_find_area_xy(struct bScreen *sc, const int spacetype, int x, int y);
struct ARegion *BKE_area_find_region_xy(struct ScrArea *sa, const int regiontype, int x, int y);
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

//#include "ED_object.h"
#include "BKE_scene.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "MEM_guardedalloc.h"

#include "paint_intern.h"
// #include "sculpt_intern.h"
///
#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_key_types.h"
#include "BLI_bitmap.h"
#include "BLI_threads.h"

typedef enum {
  SCULPT_UNDO_COORDS,
  SCULPT_UNDO_HIDDEN,
  SCULPT_UNDO_MASK,
  SCULPT_UNDO_DYNTOPO_BEGIN,
  SCULPT_UNDO_DYNTOPO_END,
  SCULPT_UNDO_DYNTOPO_SYMMETRIZE,
  SCULPT_UNDO_GEOMETRY,
} SculptUndoType;

typedef struct SculptUndoNode {
  struct SculptUndoNode *next, *prev;

  SculptUndoType type;

  char idname[MAX_ID_NAME]; /* name instead of pointer*/
  void *node;               /* only during push, not valid afterwards! */

  float (*co)[3];
  float (*orig_co)[3];
  short (*no)[3];
  float *mask;
  int totvert;

  /* non-multires */
  int maxvert; /* to verify if totvert it still the same */
  int *index;  /* to restore into right location */
  BLI_bitmap *vert_hidden;

  /* multires */
  int maxgrid;  /* same for grid */
  int gridsize; /* same for grid */
  int totgrid;  /* to restore into right location */
  int *grids;   /* to restore into right location */
  BLI_bitmap **grid_hidden;

  /* bmesh */
  struct BMLogEntry *bm_entry;
  bool applied;

  /* shape keys */
  char shapeName[sizeof(((KeyBlock *)0))->name];

  /* geometry modification operations and bmesh enter data */
  CustomData geom_vdata;
  CustomData geom_edata;
  CustomData geom_ldata;
  CustomData geom_pdata;
  int geom_totvert;
  int geom_totedge;
  int geom_totloop;
  int geom_totpoly;

  /* pivot */
  float pivot_pos[3];
  float pivot_rot[4];

  size_t undo_size;
} SculptUndoNode;

typedef struct {
  struct BMLog *bm_log;

  SculptUndoNode *unode;
  float (*coords)[3];
  short (*normals)[3];
  const float *vmasks;

  /* Original coordinate, normal, and mask */
  const float *co;
  const short *no;
  float mask;
} SculptOrigVertData;

/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f

struct SculptRakeData {
  float follow_dist;
  float follow_co[3];
};

/* Single struct used by all BLI_task threaded callbacks, let's avoid adding 10's of those... */
typedef struct SculptThreadedTaskData {
  struct bContext *C;
  struct Sculpt *sd;
  struct Object *ob;
  const struct Brush *brush;
  struct PBVHNode **nodes;
  int totnode;

  struct VPaint *vp;
  struct VPaintData *vpd;
  struct WPaintData *wpd;
  struct WeightPaintInfo *wpi;
  unsigned int *lcol;
  struct Mesh *me;
  /* For passing generic params. */
  void *custom_data;

  /* Data specific to some callbacks. */

  /* Note: even if only one or two of those are used at a time,
   *       keeping them separated, names help figuring out
   *       what it is, and memory overhead is ridiculous anyway. */
  float flippedbstrength;
  float angle;
  float strength;
  bool smooth_mask;
  bool has_bm_orco;

  struct SculptProjectVector *spvc;
  float *offset;
  float *grab_delta;
  float *cono;
  float *area_no;
  float *area_no_sp;
  float *area_co;
  float (*mat)[4];
  float (*vertCos)[3];

  int filter_type;
  float filter_strength;

  bool use_area_cos;
  bool use_area_nos;
  bool any_vertex_sampled;

  float *prev_mask;

  float *pose_origin;
  float *pose_initial_co;
  float *pose_factor;
  float (*transform_rot)[4], (*transform_trans)[4], (*transform_trans_inv)[4];

  float max_distance_squared;
  float nearest_vertex_search_co[3];

  int mask_expand_update_it;
  bool mask_expand_invert_mask;
  bool mask_expand_use_normals;
  bool mask_expand_keep_prev_mask;

  float transform_mats[8][4][4];

  float dirty_mask_min;
  float dirty_mask_max;
  bool dirty_mask_dirty_only;

  ThreadMutex mutex;

} SculptThreadedTaskData;

typedef enum SculptUpdateType {
  SCULPT_UPDATE_COORDS = 1 << 0,
  SCULPT_UPDATE_MASK = 1 << 1,
} SculptUpdateType;


typedef struct SculptCursorGeometryInfo {
  float location[3];
  float normal[3];
  float active_vertex_co[3];
} SculptCursorGeometryInfo;

static bool sculpt_stroke_get_location(struct bContext *C, float out[3], const float mouse[2]);
static bool sculpt_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal);
static void sculpt_geometry_preview_lines_update(bContext *C, struct SculptSession *ss, float radius);
static void sculpt_pose_calc_pose_data(struct Sculpt *sd,
                                struct Object *ob,
                                struct SculptSession *ss,
                                float initial_location[3],
                                float radius,
                                float pose_offset,
                                float *r_pose_origin,
                                float *r_pose_factor);


/* Dynamic topology */
static void sculpt_pbvh_clear(Object *ob);
static void sculpt_dyntopo_node_layers_add(struct SculptSession *ss);
static void sculpt_dynamic_topology_disable(bContext *C, struct SculptUndoNode *unode);


/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f




/*************** Brush testing declarations ****************/
typedef struct SculptBrushTest {
  float radius_squared;
  float location[3];
  float dist;
  int mirror_symmetry_pass;

  /* For circle (not sphere) projection. */
  float plane_view[4];

  /* Some tool code uses a plane for it's calculateions. */
  float plane_tool[4];

  /* View3d clipping - only set rv3d for clipping */
  struct RegionView3D *clip_rv3d;
} SculptBrushTest;

typedef bool (*SculptBrushTestFn)(SculptBrushTest *test, const float co[3]);

typedef struct {
  struct Sculpt *sd;
  struct SculptSession *ss;
  float radius_squared;
  float *center;
  bool original;
  bool ignore_fully_masked;
} SculptSearchSphereData;

typedef struct {
  struct Sculpt *sd;
  struct SculptSession *ss;
  float radius_squared;
  bool original;
  bool ignore_fully_masked;
  struct DistRayAABB_Precalc *dist_ray_to_aabb_precalc;
} SculptSearchCircleData;

static void sculpt_brush_test_init(struct SculptSession *ss, SculptBrushTest *test);
static bool sculpt_brush_test_sphere(SculptBrushTest *test, const float co[3]);
static bool sculpt_brush_test_sphere_sq(SculptBrushTest *test, const float co[3]);
static bool sculpt_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3]);
static bool sculpt_brush_test_cube(SculptBrushTest *test, const float co[3], float local[4][4]);
static bool sculpt_brush_test_circle_sq(SculptBrushTest *test, const float co[3]);
static bool sculpt_search_sphere_cb(PBVHNode *node, void *data_v);
static bool sculpt_search_circle_cb(PBVHNode *node, void *data_v);

static SculptBrushTestFn sculpt_brush_test_init_with_falloff_shape(SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape);
static const float *sculpt_brush_frontface_normal_from_falloff_shape(SculptSession *ss,
                                                              char falloff_shape);

static float tex_strength(struct SculptSession *ss,
                   const struct Brush *br,
                   const float point[3],
                   const float len,
                   const short vno[3],
                   const float fno[3],
                   const float mask,
                   const int vertex_index,
                   const int thread_id);

/* just for vertex paint. */
static bool sculpt_pbvh_calc_area_normal(const struct Brush *brush,
                                  Object *ob,
                                  PBVHNode **nodes,
                                  int totnode,
                                  bool use_threading,
                                  float r_area_no[3]);


typedef struct StrokeCache {
  /* Invariants */
  float initial_radius;
  float scale[3];
  int flag;
  float clip_tolerance[3];
  float initial_mouse[2];

  /* Variants */
  float radius;
  float radius_squared;
  float true_location[3];
  float true_last_location[3];
  float location[3];
  float last_location[3];
  bool is_last_valid;

  bool pen_flip;
  bool invert;
  float pressure;
  float mouse[2];
  float bstrength;
  float normal_weight; /* from brush (with optional override) */

  /* The rest is temporary storage that isn't saved as a property */

  bool first_time; /* Beginning of stroke may do some things special */

  /* from ED_view3d_ob_project_mat_get() */
  float projection_mat[4][4];

  /* Clean this up! */
  struct ViewContext *vc;
  const struct Brush *brush;

  float special_rotation;
  float grab_delta[3], grab_delta_symmetry[3];
  float old_grab_location[3], orig_grab_location[3];

  /* screen-space rotation defined by mouse motion */
  float rake_rotation[4], rake_rotation_symmetry[4];
  bool is_rake_rotation_valid;
  struct SculptRakeData rake_data;

  /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
   * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  int symmetry;
  int mirror_symmetry_pass; /* the symmetry pass we are currently on between 0 and 7*/
  float true_view_normal[3];
  float view_normal[3];

  /* sculpt_normal gets calculated by calc_sculpt_normal(), then the
   * sculpt_normal_symm gets updated quickly with the usual symmetry
   * transforms */
  float sculpt_normal[3];
  float sculpt_normal_symm[3];

  /* Used for area texture mode, local_mat gets calculated by
   * calc_brush_local_mat() and used in tex_strength(). */
  float brush_local_mat[4][4];

  float plane_offset[3]; /* used to shift the plane around when doing tiled strokes */
  int tile_pass;

  float last_center[3];
  int radial_symmetry_pass;
  float symm_rot_mat[4][4];
  float symm_rot_mat_inv[4][4];
  bool original;
  float anchored_location[3];

  /* Pose brush */
  float *pose_factor;
  float pose_initial_co[3];
  float pose_origin[3];

  float vertex_rotation; /* amount to rotate the vertices when using rotate brush */
  struct Dial *dial;

  char saved_active_brush_name[MAX_ID_NAME];
  char saved_mask_brush_tool;
  int saved_smooth_size; /* smooth tool copies the size of the current tool */
  bool alt_smooth;

  float plane_trim_squared;

  bool supports_gravity;
  float true_gravity_direction[3];
  float gravity_direction[3];

  float *automask;

  rcti previous_r; /* previous redraw rectangle */
  rcti current_r;  /* current redraw rectangle */

} StrokeCache;

typedef struct FilterCache {
  bool enabled_axis[3];
  int random_seed;

  /* unmasked nodes */
  PBVHNode **nodes;
  int totnode;

  /* mask expand iteration caches */
  int mask_update_current_it;
  int mask_update_last_it;
  int *mask_update_it;
  float *normal_factor;
  float *edge_factor;
  float *prev_mask;
  float mask_expand_initial_co[3];
} FilterCache;

static void sculpt_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const char symm,
                                      const char axis,
                                      const float angle);
static void sculpt_cache_free(StrokeCache *cache);

extern "C" SculptUndoNode *sculpt_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type);
extern "C" SculptUndoNode *sculpt_undo_get_node(PBVHNode *node);
extern "C" void sculpt_undo_push_begin(const char *name);
extern "C" void sculpt_undo_push_end(void);

static void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3]);

static void sculpt_update_object_bounding_box(struct Object *ob);



///

#include "WM_api.h"
#include "wm_message_bus.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

/***************************************************************************************************
 * \class                               Widget_Sculpt
 ***************************************************************************************************
 * Interaction widget for the Sculpt tool.
 *
 **************************************************************************************************/
#define WIDGET_SCULPT_MAX_RADIUS 0.2f /* Max sculpt radius (in Blender meters) */

Widget_Sculpt Widget_Sculpt::obj;

float Widget_Sculpt::sculpt_radius(0.02f);
float Widget_Sculpt::sculpt_strength(1.0f);

Coord3Df Widget_Sculpt::p_hmd;
Coord3Df Widget_Sculpt::p_cursor;
float Widget_Sculpt::dist;
float Widget_Sculpt::sculpt_radius_prev;
float Widget_Sculpt::sculpt_strength_prev;
bool Widget_Sculpt::param_mode(false);
bool Widget_Sculpt::stroke_started(false);
bool Widget_Sculpt::is_dragging(false);

VR_Side Widget_Sculpt::cursor_side;

int Widget_Sculpt::mode(BRUSH_STROKE_NORMAL);
int Widget_Sculpt::mode_orig(BRUSH_STROKE_NORMAL);
int Widget_Sculpt::brush(SCULPT_TOOL_DRAW);
float Widget_Sculpt::location[3];
float Widget_Sculpt::mouse[2];
float Widget_Sculpt::pressure(1.0f);
bool Widget_Sculpt::use_trigger_pressure(true);
bool Widget_Sculpt::raycast(false);
bool Widget_Sculpt::dyntopo(false);
char Widget_Sculpt::symmetry(0x00);
bool Widget_Sculpt::pen_flip(false);
bool Widget_Sculpt::ignore_background_click(true);

/* Dummy op for sculpt functions. */
static wmOperator sculpt_dummy_op;
/* Dummy event for sculpt functions. */
static wmEvent sculpt_dummy_event;

/* Sculpt PBVH abstraction API
 *
 * This is read-only, for writing use PBVH vertex iterators. There vd.index matches
 * the indices used here.
 *
 * For multires, the same vertex in multiple grids is counted multiple times, with
 * different index for each grid. */

static void sculpt_vertex_random_access_init(SculptSession *ss)
{
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);
  }
}

static int sculpt_vertex_count_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->totvert;
    case PBVH_BMESH:
      return BM_mesh_elem_count(BKE_pbvh_get_bmesh(ss->pbvh), BM_VERT);
    case PBVH_GRIDS:
      return BKE_pbvh_get_grid_num_vertices(ss->pbvh);
  }

  return 0;
}

static const float *sculpt_vertex_co_get(SculptSession *ss, int index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->mvert[index].co;
    case PBVH_BMESH:
      return BM_vert_at_index(BKE_pbvh_get_bmesh(ss->pbvh), index)->co;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }
  return NULL;
}

static void sculpt_vertex_normal_get(SculptSession *ss, int index, float no[3])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      normal_short_to_float_v3(no, ss->mvert[index].no);
      return;
    case PBVH_BMESH:
      copy_v3_v3(no, BM_vert_at_index(BKE_pbvh_get_bmesh(ss->pbvh), index)->no);
      break;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      copy_v3_v3(no, CCG_elem_no(key, CCG_elem_offset(key, elem, vertex_index)));
      break;
    }
  }
}

static float sculpt_vertex_mask_get(SculptSession *ss, int index)
{
  BMVert *v;
  float *mask;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->vmask[index];
    case PBVH_BMESH:
      v = BM_vert_at_index(BKE_pbvh_get_bmesh(ss->pbvh), index);
      mask = (float *)BM_ELEM_CD_GET_VOID_P(v,
                                            CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK));
      return *mask;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return *CCG_elem_mask(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }

  return 0.0f;
}

static int sculpt_active_vertex_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->active_vertex_index;
    case PBVH_BMESH:
      return ss->active_vertex_index;
    case PBVH_GRIDS:
      return ss->active_vertex_index;
  }

  return 0;
}

static const float *sculpt_active_vertex_co_get(SculptSession *ss)
{
  return sculpt_vertex_co_get(ss, sculpt_active_vertex_get(ss));
}

static void sculpt_active_vertex_normal_get(SculptSession *ss, float normal[3])
{
  sculpt_vertex_normal_get(ss, sculpt_active_vertex_get(ss), normal);
}

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 256

typedef struct SculptVertexNeighborIter {
  /* Storage */
  int *neighbors;
  int size;
  int capacity;
  int neighbors_fixed[SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY];

  /* Internal iterator. */
  int num_duplicates;
  int i;

  /* Public */
  int index;
  bool is_duplicate;
} SculptVertexNeighborIter;

static void sculpt_vertex_neighbor_add(SculptVertexNeighborIter *iter, int neighbor_index)
{
  for (int i = 0; i < iter->size; i++) {
    if (iter->neighbors[i] == neighbor_index) {
      return;
    }
  }

  if (iter->size >= iter->capacity) {
    iter->capacity += SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    if (iter->neighbors == iter->neighbors_fixed) {
      iter->neighbors = (int *)MEM_mallocN(iter->capacity * sizeof(int), "neighbor array");
      memcpy(iter->neighbors, iter->neighbors_fixed, sizeof(int) * iter->size);
    }
    else {
      iter->neighbors = (int *)MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(int), "neighbor array");
    }
  }

  iter->neighbors[iter->size] = neighbor_index;
  iter->size++;
}

static void sculpt_vertex_neighbors_get_bmesh(SculptSession *ss,
                                              int index,
                                              SculptVertexNeighborIter *iter)
{
  BMVert *v = BM_vert_at_index(ss->bm, index);
  BMIter liter;
  BMLoop *l;
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;

  int i = 0;
  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    const BMVert *adj_v[2] = {l->prev->v, l->next->v};
    for (i = 0; i < ARRAY_SIZE(adj_v); i++) {
      const BMVert *v_other = adj_v[i];
      if (BM_elem_index_get(v_other) != (int)index) {
        sculpt_vertex_neighbor_add(iter, BM_elem_index_get(v_other));
      }
    }
  }
}

static void sculpt_vertex_neighbors_get_faces(SculptSession *ss,
                                              int index,
                                              SculptVertexNeighborIter *iter)
{
  int i;
  MeshElemMap *vert_map = &ss->pmap[(int)index];
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;

  for (i = 0; i < ss->pmap[(int)index].count; i++) {
    const MPoly *p = &ss->mpoly[vert_map->indices[i]];
    unsigned f_adj_v[2];
    if (poly_get_adj_loops_from_vert(p, ss->mloop, (int)index, f_adj_v) != -1) {
      int j;
      for (j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
        if (f_adj_v[j] != (int)index) {
          sculpt_vertex_neighbor_add(iter, f_adj_v[j]);
        }
      }
    }
  }
}

static void sculpt_vertex_neighbors_get_grids(SculptSession *ss,
                                              const int index,
                                              const bool include_duplicates,
                                              SculptVertexNeighborIter *iter)
{
  /* TODO: optimize this. We could fill SculptVertexNeighborIter directly,
   * maybe provide coordinate and mask pointers directly rather than converting
   * back and forth between CCGElem and global index. */
  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;

  SubdivCCGCoord coord;
  coord.grid_index = grid_index;
  coord.x = vertex_index % key->grid_size;
  coord.y = vertex_index / key->grid_size;

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(ss->subdiv_ccg, &coord, include_duplicates, &neighbors);

  iter->size = 0;
  iter->num_duplicates = neighbors.num_duplicates;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;

  for (int i = 0; i < neighbors.size; i++) {
    sculpt_vertex_neighbor_add(iter,
                               neighbors.coords[i].grid_index * key->grid_area +
                                   neighbors.coords[i].y * key->grid_size + neighbors.coords[i].x);
  }

  if (neighbors.coords != neighbors.coords_fixed) {
    MEM_freeN(neighbors.coords);
  }
}

static void sculpt_vertex_neighbors_get(SculptSession *ss,
                                        const int index,
                                        const bool include_duplicates,
                                        SculptVertexNeighborIter *iter)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      sculpt_vertex_neighbors_get_faces(ss, index, iter);
      return;
    case PBVH_BMESH:
      sculpt_vertex_neighbors_get_bmesh(ss, index, iter);
      return;
    case PBVH_GRIDS:
      sculpt_vertex_neighbors_get_grids(ss, index, include_duplicates, iter);
      return;
  }
}

/* Iterator over neighboring vertices. */
#define sculpt_vertex_neighbors_iter_begin(ss, v_index, neighbor_iterator) \
  sculpt_vertex_neighbors_get(ss, v_index, false, &neighbor_iterator); \
  for (neighbor_iterator.i = 0; neighbor_iterator.i < neighbor_iterator.size; \
       neighbor_iterator.i++) { \
    neighbor_iterator.index = ni.neighbors[ni.i];

/* Iterate over neighboring and duplicate vertices (for PBVH_GRIDS). Duplicates come
 * first since they are nearest for floodfill. */
#define sculpt_vertex_duplicates_and_neighbors_iter_begin(ss, v_index, neighbor_iterator) \
  sculpt_vertex_neighbors_get(ss, v_index, true, &neighbor_iterator); \
  for (neighbor_iterator.i = neighbor_iterator.size - 1; neighbor_iterator.i >= 0; \
       neighbor_iterator.i--) { \
    neighbor_iterator.index = ni.neighbors[ni.i]; \
    neighbor_iterator.is_duplicate = (ni.i >= \
                                      neighbor_iterator.size - neighbor_iterator.num_duplicates);

#define sculpt_vertex_neighbors_iter_end(neighbor_iterator) \
  } \
  if (neighbor_iterator.neighbors != neighbor_iterator.neighbors_fixed) { \
    MEM_freeN(neighbor_iterator.neighbors); \
  } \
  ((void)0)

/* Utils */
static bool check_vertex_pivot_symmetry(const float vco[3], const float pco[3], const char symm)
{
  bool is_in_symmetry_area = true;
  for (int i = 0; i < 3; i++) {
    char symm_it = 1 << i;
    if (symm & symm_it) {
      if (pco[i] == 0.0f) {
        if (vco[i] > 0.0f) {
          is_in_symmetry_area = false;
        }
      }
      if (vco[i] * pco[i] < 0.0f) {
        is_in_symmetry_area = false;
      }
    }
  }
  return is_in_symmetry_area;
}

typedef struct NearestVertexTLSData {
  int nearest_vertex_index;
  float nearest_vertex_distance_squared;
} NearestVertexTLSData;

static void do_nearest_vertex_get_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  NearestVertexTLSData *nvtd = (NearestVertexTLSData *)tls->userdata_chunk;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    float distance_squared = len_squared_v3v3(vd.co, data->nearest_vertex_search_co);
    if (distance_squared < nvtd->nearest_vertex_distance_squared &&
        distance_squared < data->max_distance_squared) {
      nvtd->nearest_vertex_index = vd.index;
      nvtd->nearest_vertex_distance_squared = distance_squared;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void nearest_vertex_get_reduce(const void *__restrict UNUSED(userdata),
                                      void *__restrict chunk_join,
                                      void *__restrict chunk)
{
  NearestVertexTLSData *join = (NearestVertexTLSData *)chunk_join;
  NearestVertexTLSData *nvtd = (NearestVertexTLSData *)chunk;
  if (join->nearest_vertex_index == -1) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
  else if (nvtd->nearest_vertex_distance_squared < join->nearest_vertex_distance_squared) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
}

static int sculpt_nearest_vertex_get(
    Sculpt *sd, Object *ob, float co[3], float max_distance, bool use_original)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  int totnode;
  SculptSearchSphereData data;
  data.ss = ss;
  data.sd = sd;
  data.radius_squared = max_distance * max_distance;
  data.original = use_original;
  data.center = co;
  BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);
  if (totnode == 0) {
    return -1;
  }

  SculptThreadedTaskData task_data;
  task_data.sd = sd;
  task_data.ob = ob;
  task_data.nodes = nodes;
  task_data.max_distance_squared = max_distance * max_distance;

  copy_v3_v3(task_data.nearest_vertex_search_co, co);
  NearestVertexTLSData nvtd;
  nvtd.nearest_vertex_index = -1;
  nvtd.nearest_vertex_distance_squared = FLT_MAX;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = nearest_vertex_get_reduce;
  settings.userdata_chunk = &nvtd;
  settings.userdata_chunk_size = sizeof(NearestVertexTLSData);
  BKE_pbvh_parallel_range(0, totnode, &task_data, do_nearest_vertex_get_task_cb, &settings);

  MEM_SAFE_FREE(nodes);

  return nvtd.nearest_vertex_index;
}

static bool is_symmetry_iteration_valid(char i, char symm)
{
  return i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)));
}

/* Checks if a vertex is inside the brush radius from any of its mirrored axis */
static bool sculpt_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                                      const float br_co[3],
                                                      float radius,
                                                      char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (is_symmetry_iteration_valid(i, symm)) {
      float location[3];
      flip_v3_v3(location, br_co, (char)i);
      if (len_squared_v3v3(location, vertex) < radius * radius) {
        return true;
      }
    }
  }
  return false;
}

/* Sculpt Flood Fill API
 *
 * Iterate over connected vertices, starting from one or more initial vertices. */

typedef struct SculptFloodFill {
  GSQueue *queue;
  char *visited_vertices;
} SculptFloodFill;

static void sculpt_floodfill_init(SculptSession *ss, SculptFloodFill *flood)
{
  int vertex_count = sculpt_vertex_count_get(ss);
  sculpt_vertex_random_access_init(ss);

  flood->queue = BLI_gsqueue_new(sizeof(int));
  flood->visited_vertices = (char*)MEM_callocN(vertex_count * sizeof(char), "visited vertices");
}

static void sculpt_floodfill_add_initial(SculptFloodFill *flood, int index)
{
  BLI_gsqueue_push(flood->queue, &index);
}

static void sculpt_floodfill_add_active(
    Sculpt *sd, Object *ob, SculptSession *ss, SculptFloodFill *flood, float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  for (char i = 0; i <= symm; ++i) {
    if (is_symmetry_iteration_valid(i, symm)) {
      int v = -1;
      if (i == 0) {
        v = sculpt_active_vertex_get(ss);
      }
      else if (radius > 0.0f) {
        float radius_squared = (radius == FLT_MAX) ? FLT_MAX : radius * radius;
        float location[3];
        flip_v3_v3(location, sculpt_active_vertex_co_get(ss), i);
        v = sculpt_nearest_vertex_get(sd, ob, location, radius_squared, false);
      }
      if (v != -1) {
        sculpt_floodfill_add_initial(flood, v);
      }
    }
  }
}

static void sculpt_floodfill_execute(
    SculptSession *ss,
    SculptFloodFill *flood,
    bool (*func)(SculptSession *ss, int from_v, int to_v, bool is_duplicate, void *userdata),
    void *userdata)
{
  while (!BLI_gsqueue_is_empty(flood->queue)) {
    int from_v;
    BLI_gsqueue_pop(flood->queue, &from_v);
    SculptVertexNeighborIter ni;
    sculpt_vertex_duplicates_and_neighbors_iter_begin(ss, from_v, ni)
    {
      const int to_v = ni.index;
      if (flood->visited_vertices[to_v] == 0) {
        flood->visited_vertices[to_v] = 1;

        if (func(ss, from_v, to_v, ni.is_duplicate, userdata)) {
          BLI_gsqueue_push(flood->queue, &to_v);
        }
      }
    }
    sculpt_vertex_neighbors_iter_end(ni);
  }
}

static void sculpt_floodfill_free(SculptFloodFill *flood)
{
  MEM_SAFE_FREE(flood->visited_vertices);
  BLI_gsqueue_free(flood->queue);
  flood->queue = NULL;
}

/** \name Tool Capabilities
 *
 * Avoid duplicate checks, internal logic only,
 * share logic with #rna_def_sculpt_capabilities where possible.
 *
 * \{ */

/* Check if there are any active modifiers in stack
 * (used for flushing updates at enter/exit sculpt mode) */
static bool sculpt_has_active_modifiers(Scene *scene, Object *ob)
{
  ModifierData *md;
  VirtualModifierData virtualModifierData;

  md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

  /* exception for shape keys because we can edit those */
  for (; md; md = md->next) {
    if (modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
      return 1;
    }
  }

  return 0;
}

static bool sculpt_tool_needs_original(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_GRAB,
              SCULPT_TOOL_ROTATE,
              SCULPT_TOOL_THUMB,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_DRAW_SHARP,
              SCULPT_TOOL_ELASTIC_DEFORM,
              SCULPT_TOOL_POSE);
}

static bool sculpt_tool_is_proxy_used(const char sculpt_tool)
{
  return ELEM(sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_LAYER, SCULPT_TOOL_POSE);
}

static bool sculpt_brush_use_topology_rake(const SculptSession *ss, const Brush *brush)
{
  return SCULPT_TOOL_HAS_TOPOLOGY_RAKE(brush->sculpt_tool) &&
         (brush->topology_rake_factor > 0.0f) && (ss->bm != NULL);
}

/**
 * Test whether the #StrokeCache.sculpt_normal needs update in #do_brush_action
 */
static int sculpt_brush_needs_normal(const SculptSession *ss, const Brush *brush)
{
  return ((SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool) &&
           (ss->cache->normal_weight > 0.0f)) ||

          ELEM(brush->sculpt_tool,
               SCULPT_TOOL_BLOB,
               SCULPT_TOOL_CREASE,
               SCULPT_TOOL_DRAW,
               SCULPT_TOOL_DRAW_SHARP,
               SCULPT_TOOL_LAYER,
               SCULPT_TOOL_NUDGE,
               SCULPT_TOOL_ROTATE,
               SCULPT_TOOL_ELASTIC_DEFORM,
               SCULPT_TOOL_THUMB) ||

          (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA)) ||
         sculpt_brush_use_topology_rake(ss, brush);
}
/** \} */

static bool sculpt_brush_needs_rake_rotation(const Brush *brush)
{
  return SCULPT_TOOL_HAS_RAKE(brush->sculpt_tool) && (brush->rake_factor != 0.0f);
}

typedef enum StrokeFlags {
  CLIP_X = 1,
  CLIP_Y = 2,
  CLIP_Z = 4,
} StrokeFlags;

/************** Access to original unmodified vertex data *************/

#if !WITH_VR
typedef struct {
  BMLog *bm_log;

  SculptUndoNode *unode;
  float (*coords)[3];
  short (*normals)[3];
  const float *vmasks;

  /* Original coordinate, normal, and mask */
  const float *co;
  const short *no;
  float mask;
} SculptOrigVertData;
#endif

/* Initialize a SculptOrigVertData for accessing original vertex data;
 * handles BMesh, mesh, and multires */
static void sculpt_orig_vert_data_unode_init(SculptOrigVertData *data,
                                             Object *ob,
                                             SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  BMesh *bm = ss->bm;

  memset(data, 0, sizeof(*data));
  data->unode = unode;

  if (bm) {
    data->bm_log = ss->bm_log;
  }
  else {
    data->coords = data->unode->co;
    data->normals = data->unode->no;
    data->vmasks = data->unode->mask;
  }
}

/* Initialize a SculptOrigVertData for accessing original vertex data;
 * handles BMesh, mesh, and multires */
static void sculpt_orig_vert_data_init(SculptOrigVertData *data, Object *ob, PBVHNode *node)
{
  SculptUndoNode *unode;
  unode = sculpt_undo_push_node(ob, node, SCULPT_UNDO_COORDS);
  sculpt_orig_vert_data_unode_init(data, ob, unode);
}

/* Update a SculptOrigVertData for a particular vertex from the PBVH
 * iterator */
static void sculpt_orig_vert_data_update(SculptOrigVertData *orig_data, PBVHVertexIter *iter)
{
  if (orig_data->unode->type == SCULPT_UNDO_COORDS) {
    if (orig_data->bm_log) {
      BM_log_original_vert_data(orig_data->bm_log, iter->bm_vert, &orig_data->co, &orig_data->no);
    }
    else {
      orig_data->co = orig_data->coords[iter->i];
      orig_data->no = orig_data->normals[iter->i];
    }
  }
  else if (orig_data->unode->type == SCULPT_UNDO_MASK) {
    if (orig_data->bm_log) {
      orig_data->mask = BM_log_original_mask(orig_data->bm_log, iter->bm_vert);
    }
    else {
      orig_data->mask = orig_data->vmasks[iter->i];
    }
  }
}

static void sculpt_rake_data_update(struct SculptRakeData *srd, const float co[3])
{
  float rake_dist = len_v3v3(srd->follow_co, co);
  if (rake_dist > srd->follow_dist) {
    interp_v3_v3v3(srd->follow_co, srd->follow_co, co, rake_dist - srd->follow_dist);
  }
}

static void sculpt_rake_rotate(const SculptSession *ss,
                               const float sculpt_co[3],
                               const float v_co[3],
                               float factor,
                               float r_delta[3])
{
  float vec_rot[3];

#if 0
  /* lerp */
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);
  mul_qt_v3(ss->cache->rake_rotation_symmetry, vec_rot);
  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
  mul_v3_fl(r_delta, factor);
#else
  /* slerp */
  float q_interp[4];
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);

  copy_qt_qt(q_interp, ss->cache->rake_rotation_symmetry);
  pow_qt_fl_normalized(q_interp, factor);
  mul_qt_v3(q_interp, vec_rot);

  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
#endif
}

/**
 * Align the grab delta to the brush normal.
 *
 * \param grab_delta: Typically from `ss->cache->grab_delta_symmetry`.
 */
static void sculpt_project_v3_normal_align(SculptSession *ss,
                                           const float normal_weight,
                                           float grab_delta[3])
{
  /* signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss->cache->sculpt_normal_symm, grab_delta);

  /* this scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss->cache->sculpt_normal_symm, ss->cache->view_normal);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss->cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss->cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

/** \name SculptProjectVector
 *
 * Fast-path for #project_plane_v3_v3v3
 *
 * \{ */

typedef struct SculptProjectVector {
  float plane[3];
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;

} SculptProjectVector;

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float plane[3])
{
  copy_v3_v3(spvc->plane, plane);
  spvc->len_sq = len_squared_v3(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float vec[3], float r_vec[3])
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  madd_v3_v3fl(r_vec, spvc->plane, dot_v3v3(vec, spvc->plane) * spvc->len_sq_inv_neg);
#endif
}

/** \} */

/**********************************************************************/

/* Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without. Same goes for alt-
 * key smoothing. */
static bool sculpt_stroke_is_dynamic_topology(const SculptSession *ss, const Brush *brush)
{
  return ((BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) &&

          (!ss->cache || (!ss->cache->alt_smooth)) &&

          /* Requires mesh restore, which doesn't work with
           * dynamic-topology */
          !(brush->flag & BRUSH_ANCHORED) && !(brush->flag & BRUSH_DRAG_DOT) &&

          SCULPT_TOOL_HAS_DYNTOPO(brush->sculpt_tool));
}

/*** paint mesh ***/

static void paint_mesh_restore_co_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = (SculptSession *)data->ob->sculpt;

  SculptUndoNode *unode;
  SculptUndoType type = (data->brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                        SCULPT_UNDO_COORDS);

  if (ss->bm) {
    unode = sculpt_undo_push_node(data->ob, data->nodes[n], type);
  }
  else {
    unode = sculpt_undo_get_node(data->nodes[n]);
  }

  if (unode) {
    PBVHVertexIter vd;
    SculptOrigVertData orig_data;

    sculpt_orig_vert_data_unode_init(&orig_data, data->ob, unode);

    BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
    {
      sculpt_orig_vert_data_update(&orig_data, &vd);

      if (orig_data.unode->type == SCULPT_UNDO_COORDS) {
        copy_v3_v3(vd.co, orig_data.co);
        if (vd.no) {
          copy_v3_v3_short(vd.no, orig_data.no);
        }
        else {
          normal_short_to_float_v3(vd.fno, orig_data.no);
        }
      }
      else if (orig_data.unode->type == SCULPT_UNDO_MASK) {
        *vd.mask = orig_data.mask;
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
    BKE_pbvh_vertex_iter_end;

    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void paint_mesh_restore_co(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  /**
   * Disable OpenMP when dynamic-topology is enabled. Otherwise, new entries might be inserted by
   * #sculpt_undo_push_node() into the GHash used internally by #BM_log_original_vert_co()
   * by a different thread. See T33787. */
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP) && !ss->bm, totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, paint_mesh_restore_co_task_cb, &settings);

  MEM_SAFE_FREE(nodes);
}

/*** BVH Tree ***/

static void sculpt_extend_redraw_rect_previous(Object *ob, rcti *rect)
{
  /* expand redraw rect with redraw rect from previous step to
   * prevent partial-redraw issues caused by fast strokes. This is
   * needed here (not in sculpt_flush_update) as it was before
   * because redraw rectangle should be the same in both of
   * optimized PBVH draw function and 3d view redraw (if not -- some
   * mesh parts could disappear from screen (sergey) */
  SculptSession *ss = ob->sculpt;

  if (ss->cache) {
    if (!BLI_rcti_is_empty(&ss->cache->previous_r)) {
      BLI_rcti_union(rect, &ss->cache->previous_r);
    }
  }
}

/* Get a screen-space rectangle of the modified area */
static bool sculpt_get_redraw_rect(ARegion *ar, RegionView3D *rv3d, Object *ob, rcti *rect)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  float bb_min[3], bb_max[3];

  if (!pbvh) {
    return 0;
  }

  BKE_pbvh_redraw_BB(pbvh, bb_min, bb_max);

  /* convert 3D bounding box to screen space */
  if (!paint_convert_bb_to_rect(rect, bb_min, bb_max, ar, rv3d, ob)) {
    return 0;
  }

  return 1;
}

static void ED_sculpt_redraw_planes_get(float planes[4][4], ARegion *ar, Object *ob)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  /* copy here, original will be used below */
  rcti rect = ob->sculpt->cache->current_r;

  sculpt_extend_redraw_rect_previous(ob, &rect);

  paint_calc_redraw_planes(planes, ar, ob, &rect);

  /* we will draw this rect, so now we can set it as the previous partial rect.
   * Note that we don't update with the union of previous/current (rect), only with
   * the current. Thus we avoid the rectangle needlessly growing to include
   * all the stroke area */
  ob->sculpt->cache->previous_r = ob->sculpt->cache->current_r;

  /* clear redraw flag from nodes */
  if (pbvh) {
    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateRedraw);
  }
}

/************************ Brush Testing *******************/

static void sculpt_brush_test_init(SculptSession *ss, SculptBrushTest *test)
{
  RegionView3D *rv3d = ss->cache ? ss->cache->vc->rv3d : ss->rv3d;

  test->radius_squared = ss->cache ? ss->cache->radius_squared :
                                     ss->cursor_radius * ss->cursor_radius;
  if (ss->cache) {
    copy_v3_v3(test->location, ss->cache->location);
    test->mirror_symmetry_pass = ss->cache->mirror_symmetry_pass;
  }
  else {
    copy_v3_v3(test->location, ss->cursor_location);
    test->mirror_symmetry_pass = 0;
  }

  test->dist = 0.0f; /* just for initialize */

  /* Only for 2D projection. */
  zero_v4(test->plane_view);
  zero_v4(test->plane_tool);

  test->mirror_symmetry_pass = ss->cache ? ss->cache->mirror_symmetry_pass : 0;

  if (rv3d->rflag & RV3D_CLIPPING) {
    test->clip_rv3d = rv3d;
  }
  else {
    test->clip_rv3d = NULL;
  }
}

BLI_INLINE bool sculpt_brush_test_clipping(const SculptBrushTest *test, const float co[3])
{
  RegionView3D *rv3d = test->clip_rv3d;
  if (!rv3d) {
    return false;
  }
  float symm_co[3];
  flip_v3_v3(symm_co, co, test->mirror_symmetry_pass);
  return ED_view3d_clipping_test(rv3d, symm_co, true);
}

static bool sculpt_brush_test_sphere(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq <= test->radius_squared) {
    if (sculpt_brush_test_clipping(test, co)) {
      return 0;
    }
    test->dist = sqrtf(distsq);
    return 1;
  }
  else {
    return 0;
  }
}

static bool sculpt_brush_test_sphere_sq(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq <= test->radius_squared) {
    if (sculpt_brush_test_clipping(test, co)) {
      return 0;
    }
    test->dist = distsq;
    return 1;
  }
  else {
    return 0;
  }
}

static bool sculpt_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3])
{
  if (sculpt_brush_test_clipping(test, co)) {
    return 0;
  }
  return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

static bool sculpt_brush_test_circle_sq(SculptBrushTest *test, const float co[3])
{
  float co_proj[3];
  closest_to_plane_normalized_v3(co_proj, test->plane_view, co);
  float distsq = len_squared_v3v3(co_proj, test->location);

  if (distsq <= test->radius_squared) {
    if (sculpt_brush_test_clipping(test, co)) {
      return 0;
    }
    test->dist = distsq;
    return 1;
  }
  else {
    return 0;
  }
}

static bool sculpt_brush_test_cube(SculptBrushTest *test, const float co[3], float local[4][4])
{
  float side = M_SQRT1_2;
  float local_co[3];

  if (sculpt_brush_test_clipping(test, co)) {
    return 0;
  }

  mul_v3_m4v3(local_co, local, co);

  local_co[0] = fabsf(local_co[0]);
  local_co[1] = fabsf(local_co[1]);
  local_co[2] = fabsf(local_co[2]);

  const float p = 8.0f;
  if (local_co[0] <= side && local_co[1] <= side && local_co[2] <= side) {
    test->dist = ((powf(local_co[0], p) + powf(local_co[1], p) + powf(local_co[2], p)) /
                  powf(side, p));

    return 1;
  }
  else {
    return 0;
  }
}

static SculptBrushTestFn sculpt_brush_test_init_with_falloff_shape(SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape)
{
  sculpt_brush_test_init(ss, test);
  SculptBrushTestFn sculpt_brush_test_sq_fn;
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    sculpt_brush_test_sq_fn = sculpt_brush_test_sphere_sq;
  }
  else {
    /* PAINT_FALLOFF_SHAPE_TUBE */
    plane_from_point_normal_v3(test->plane_view, test->location, ss->cache->view_normal);
    sculpt_brush_test_sq_fn = sculpt_brush_test_circle_sq;
  }
  return sculpt_brush_test_sq_fn;
}

static const float *sculpt_brush_frontface_normal_from_falloff_shape(SculptSession *ss,
                                                              char falloff_shape)
{
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    return ss->cache->sculpt_normal_symm;
  }
  else {
    /* PAINT_FALLOFF_SHAPE_TUBE */
    return ss->cache->view_normal;
  }
}

static float frontface(const Brush *br,
                       const float sculpt_normal[3],
                       const short no[3],
                       const float fno[3])
{
  if (br->flag & BRUSH_FRONTFACE) {
    float dot;

    if (no) {
      float tmp[3];

      normal_short_to_float_v3(tmp, no);
      dot = dot_v3v3(tmp, sculpt_normal);
    }
    else {
      dot = dot_v3v3(fno, sculpt_normal);
    }
    return dot > 0 ? dot : 0;
  }
  else {
    return 1;
  }
}


/* Automasking */

static bool sculpt_automasking_enabled(SculptSession *ss, const Brush *br)
{
  if (sculpt_stroke_is_dynamic_topology(ss, br)) {
    return false;
  }
  if (br->automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY) {
    return true;
  }
  return false;
}

static float sculpt_automasking_factor_get(SculptSession *ss, int vert)
{
  if (ss->cache->automask) {
    return ss->cache->automask[vert];
  }
  else {
    return 1.0f;
  }
}

static void sculpt_automasking_end(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->cache && ss->cache->automask) {
    MEM_freeN(ss->cache->automask);
  }
}

static bool sculpt_automasking_is_constrained_by_radius(Brush *br)
{
  /* 2D falloff is not constrained by radius */
  if (br->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    return false;
  }

  if (ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB)) {
    return true;
  }
  return false;
}

typedef struct AutomaskFloodFillData {
  float *automask_factor;
  float radius;
  bool use_radius;
  float location[3];
  char symm;
} AutomaskFloodFillData;

static bool automask_floodfill_cb(
    SculptSession *ss, int UNUSED(from_v), int to_v, bool UNUSED(is_duplicate), void *userdata)
{
  AutomaskFloodFillData *data = (AutomaskFloodFillData *)userdata;

  data->automask_factor[to_v] = 1.0f;
  return (!data->use_radius ||
          sculpt_is_vertex_inside_brush_radius_symm(
              sculpt_vertex_co_get(ss, to_v), data->location, data->radius, data->symm));
}

static float *sculpt_topology_automasking_init(Sculpt *sd, Object *ob, float *automask_factor)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!sculpt_automasking_enabled(ss, brush)) {
    return NULL;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"Topology masking: pmap missing");
    return NULL;
  }

  /* Flood fill automask to connected vertices. Limited to vertices inside
   * the brush radius if the tool requires it */
  SculptFloodFill flood;
  sculpt_floodfill_init(ss, &flood);
  sculpt_floodfill_add_active(sd, ob, ss, &flood, ss->cache->radius);

  AutomaskFloodFillData fdata;
  fdata.automask_factor = automask_factor;
  fdata.radius = ss->cache->radius;
  fdata.use_radius = sculpt_automasking_is_constrained_by_radius(brush);
  fdata.symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  copy_v3_v3(fdata.location, sculpt_active_vertex_co_get(ss));
  sculpt_floodfill_execute(ss, &flood, automask_floodfill_cb, &fdata);
  sculpt_floodfill_free(&flood);

  return automask_factor;
}

static void sculpt_automasking_init(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  ss->cache->automask = (float*)MEM_callocN(sizeof(float) * sculpt_vertex_count_get(ss),
                                    "automask_factor");

  if (brush->automasking_flags & BRUSH_AUTOMASKING_TOPOLOGY) {
    sculpt_vertex_random_access_init(ss);
    sculpt_topology_automasking_init(sd, ob, ss->cache->automask);
  }
}

/* ===== Sculpting =====
 */
static void flip_v3(float v[3], const char symm)
{
  flip_v3_v3(v, v, symm);
}

static float calc_overlap(StrokeCache *cache, const char symm, const char axis, const float angle)
{
  float mirror[3];
  float distsq;

  /* flip_v3_v3(mirror, cache->traced_location, symm); */
  flip_v3_v3(mirror, cache->true_location, symm);

  if (axis != 0) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, axis, angle);
    mul_m3_v3(mat, mirror);
  }

  /* distsq = len_squared_v3v3(mirror, cache->traced_location); */
  distsq = len_squared_v3v3(mirror, cache->true_location);

  if (distsq <= 4.0f * (cache->radius_squared)) {
    return (2.0f * (cache->radius) - sqrtf(distsq)) / (2.0f * (cache->radius));
  }
  else {
    return 0;
  }
}

static float calc_radial_symmetry_feather(Sculpt *sd,
                                          StrokeCache *cache,
                                          const char symm,
                                          const char axis)
{
  int i;
  float overlap;

  overlap = 0;
  for (i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2 * M_PI * i / sd->radial_symm[axis - 'X'];
    overlap += calc_overlap(cache, symm, axis, angle);
  }

  return overlap;
}

static float calc_symmetry_feather(Sculpt *sd, StrokeCache *cache)
{
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float overlap;
    int symm = cache->symmetry;
    int i;

    overlap = 0;
    for (i = 0; i <= symm; i++) {
      if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {

        overlap += calc_overlap(cache, i, 0, 0);

        overlap += calc_radial_symmetry_feather(sd, cache, i, 'X');
        overlap += calc_radial_symmetry_feather(sd, cache, i, 'Y');
        overlap += calc_radial_symmetry_feather(sd, cache, i, 'Z');
      }
    }

    return 1 / overlap;
  }
  else {
    return 1;
  }
}

/** \name Calculate Normal and Center
 *
 * Calculate geometry surrounding the brush center.
 * (optionally using original coordinates).
 *
 * Functions are:
 * - #calc_area_center
 * - #calc_area_normal
 * - #calc_area_normal_and_center
 *
 * \note These are all _very_ similar, when changing one, check others.
 * \{ */

typedef struct AreaNormalCenterTLSData {
  /* 0=towards view, 1=flipped */
  float area_cos[2][3];
  float area_nos[2][3];
  int area_count[2];
} AreaNormalCenterTLSData;

static void calc_area_normal_and_center_task_cb(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  AreaNormalCenterTLSData *anctd = (AreaNormalCenterTLSData *)tls->userdata_chunk;
  const bool use_area_nos = data->use_area_nos;
  const bool use_area_cos = data->use_area_cos;

  PBVHVertexIter vd;
  SculptUndoNode *unode = NULL;

  bool use_original = false;

  if (ss->cache && ss->cache->original) {
    unode = sculpt_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
    use_original = (unode->co || unode->bm_entry);
  }

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  /* Update the test radius to sample the normal using the normal radius of the brush */
  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(test.radius_squared);
    /* Layer brush produces artifacts with normal radius */
    if (!(ss->cache && data->brush->sculpt_tool == SCULPT_TOOL_LAYER)) {
      test_radius *= data->brush->normal_radius_factor;
    }
    test.radius_squared = test_radius * test_radius;
  }

  /* when the mesh is edited we can't rely on original coords
   * (original mesh may not even have verts in brush radius) */
  if (use_original && data->has_bm_orco) {
    float(*orco_coords)[3];
    int(*orco_tris)[3];
    int orco_tris_num;
    int i;

    BKE_pbvh_node_get_bm_orco_data(data->nodes[n], &orco_tris, &orco_tris_num, &orco_coords);

    for (i = 0; i < orco_tris_num; i++) {
      const float *co_tri[3] = {
          orco_coords[orco_tris[i][0]],
          orco_coords[orco_tris[i][1]],
          orco_coords[orco_tris[i][2]],
      };
      float co[3];

      closest_on_tri_to_point_v3(co, test.location, UNPACK3(co_tri));

      if (sculpt_brush_test_sq_fn(&test, co)) {
        float no[3];
        int flip_index;

        normal_tri_v3(no, UNPACK3(co_tri));

        flip_index = (dot_v3v3(ss->cache->view_normal, no) <= 0.0f);
        if (use_area_cos) {
          add_v3_v3(anctd->area_cos[flip_index], co);
        }
        if (use_area_nos) {
          add_v3_v3(anctd->area_nos[flip_index], no);
        }
        anctd->area_count[flip_index] += 1;
      }
    }
  }
  else {
    BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
    {
      const float *co;
      const short *no_s; /* bm_vert only */

      if (use_original) {
        if (unode->bm_entry) {
          BM_log_original_vert_data(ss->bm_log, vd.bm_vert, &co, &no_s);
        }
        else {
          co = unode->co[vd.i];
          no_s = unode->no[vd.i];
        }
      }
      else {
        co = vd.co;
      }

      if (sculpt_brush_test_sq_fn(&test, co)) {
        float no_buf[3];
        const float *no;
        int flip_index;

        data->any_vertex_sampled = true;

        if (use_original) {
          normal_short_to_float_v3(no_buf, no_s);
          no = no_buf;
        }
        else {
          if (vd.no) {
            normal_short_to_float_v3(no_buf, vd.no);
            no = no_buf;
          }
          else {
            no = vd.fno;
          }
        }

        flip_index = (dot_v3v3(ss->cache ? ss->cache->view_normal : ss->cursor_view_normal, no) <=
                      0.0f);
        if (use_area_cos) {
          add_v3_v3(anctd->area_cos[flip_index], co);
        }
        if (use_area_nos) {
          add_v3_v3(anctd->area_nos[flip_index], no);
        }
        anctd->area_count[flip_index] += 1;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_area_normal_and_center_reduce(const void *__restrict UNUSED(userdata),
                                               void *__restrict chunk_join,
                                               void *__restrict chunk)
{
  AreaNormalCenterTLSData *join = (AreaNormalCenterTLSData *)chunk_join;
  AreaNormalCenterTLSData *anctd = (AreaNormalCenterTLSData *)chunk;

  /* for flatten center */
  add_v3_v3(join->area_cos[0], anctd->area_cos[0]);
  add_v3_v3(join->area_cos[1], anctd->area_cos[1]);

  /* for area normal */
  add_v3_v3(join->area_nos[0], anctd->area_nos[0]);
  add_v3_v3(join->area_nos[1], anctd->area_nos[1]);

  /* weights */
  join->area_count[0] += anctd->area_count[0];
  join->area_count[1] += anctd->area_count[1];
}

static void calc_area_center(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_co[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && sculpt_stroke_is_dynamic_topology(ss, brush);
  int n;

  /* Intentionally set 'sd' to NULL since we share logic with vertex paint. */
  SculptThreadedTaskData data;
  data.sd = NULL;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.totnode = totnode;
  data.has_bm_orco = has_bm_orco;
  data.use_area_cos = true;

  AreaNormalCenterTLSData anctd = {{{0}}};

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BKE_pbvh_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* for flatten center */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.area_count[n] != 0) {
      mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.area_count[n]);
      break;
    }
  }
  if (n == 2) {
    zero_v3(r_area_co);
  }
}

static void calc_area_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  bool use_threading = (sd->flags & SCULPT_USE_OPENMP);
  sculpt_pbvh_calc_area_normal(brush, ob, nodes, totnode, use_threading, r_area_no);
}

/* expose 'calc_area_normal' externally. */
static bool sculpt_pbvh_calc_area_normal(const Brush *brush,
                                  Object *ob,
                                  PBVHNode **nodes,
                                  int totnode,
                                  bool use_threading,
                                  float r_area_no[3])
{
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && sculpt_stroke_is_dynamic_topology(ss, brush);

  /* Intentionally set 'sd' to NULL since this is used for vertex paint too. */
  SculptThreadedTaskData data;
  data.sd = NULL;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.totnode = totnode;
  data.has_bm_orco = has_bm_orco;
  data.use_area_nos = true;
  data.any_vertex_sampled = false;

  AreaNormalCenterTLSData anctd = {{{0}}};

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, use_threading, totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BKE_pbvh_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* for area normal */
  for (int i = 0; i < ARRAY_SIZE(anctd.area_nos); i++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[i]) != 0.0f) {
      break;
    }
  }

  return data.any_vertex_sampled;
}

/* this calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time */
static void calc_area_normal_and_center(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && sculpt_stroke_is_dynamic_topology(ss, brush);
  int n;

  /* Intentionally set 'sd' to NULL since this is used for vertex paint too. */
  SculptThreadedTaskData data;
  data.sd = NULL;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.totnode = totnode;
  data.has_bm_orco = has_bm_orco;
  data.use_area_cos = true;
  data.use_area_nos = true;

  AreaNormalCenterTLSData anctd = {{{0}}};

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BKE_pbvh_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* for flatten center */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.area_count[n] != 0) {
      mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.area_count[n]);
      break;
    }
  }
  if (n == 2) {
    zero_v3(r_area_co);
  }

  /* for area normal */
  for (n = 0; n < ARRAY_SIZE(anctd.area_nos); n++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[n]) != 0.0f) {
      break;
    }
  }
}

/** \} */

/* Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor. */
static float brush_strength(const Sculpt *sd,
                            const StrokeCache *cache,
                            const float feather,
                            const UnifiedPaintSettings *ups)
{
  const Scene *scene = cache->vc->scene;
  const Brush *brush = BKE_paint_brush((Paint *)&sd->paint);

  /* Primary strength input; square it to make lower values more sensitive */
  const float root_alpha = BKE_brush_alpha_get(scene, brush);
  float alpha = root_alpha * root_alpha;
  float dir = (brush->flag & BRUSH_DIR_IN) ? -1 : 1;
  float pressure = BKE_brush_use_alpha_pressure(scene, brush) ? cache->pressure : 1;
  float pen_flip = cache->pen_flip ? -1 : 1;
  float invert = cache->invert ? -1 : 1;
  float overlap = ups->overlap_factor;
  /* spacing is integer percentage of radius, divide by 50 to get
   * normalized diameter */

  float flip = dir * invert * pen_flip;

  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_DRAW_SHARP:
    case SCULPT_TOOL_LAYER:
      return alpha * flip * pressure * overlap * feather;
    case SCULPT_TOOL_CLAY_STRIPS:
      /* Clay Strips needs extra strength to compensate for its default normal radius */
      return alpha * flip * pressure * overlap * feather * 1.3f;

    case SCULPT_TOOL_MASK:
      overlap = (1 + overlap) / 2;
      switch ((BrushMaskTool)brush->mask_tool) {
        case BRUSH_MASK_DRAW:
          return alpha * flip * pressure * overlap * feather;
        case BRUSH_MASK_SMOOTH:
          return alpha * pressure * feather;
      }
      BLI_assert(!"Not supposed to happen");
      return 0.0f;

    case SCULPT_TOOL_CREASE:
    case SCULPT_TOOL_BLOB:
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_INFLATE:
      if (flip > 0) {
        return 0.250f * alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.125f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FLATTEN:
      if (flip > 0) {
        overlap = (1 + overlap) / 2;
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        /* reduce strength for DEEPEN, PEAKS, and CONTRAST */
        return 0.5f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_SMOOTH:
      return alpha * pressure * feather;

    case SCULPT_TOOL_PINCH:
      if (flip > 0) {
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.25f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_NUDGE:
      overlap = (1 + overlap) / 2;
      return alpha * pressure * overlap * feather;

    case SCULPT_TOOL_THUMB:
      return alpha * pressure * feather;

    case SCULPT_TOOL_SNAKE_HOOK:
      return root_alpha * feather;

    case SCULPT_TOOL_GRAB:
      return root_alpha * feather;

    case SCULPT_TOOL_ROTATE:
      return alpha * pressure * feather;

    case SCULPT_TOOL_ELASTIC_DEFORM:
    case SCULPT_TOOL_POSE:
      return root_alpha * feather;

    default:
      return 0;
  }
}

/* Return a multiplier for brush strength on a particular vertex. */
static float tex_strength(SculptSession *ss,
                   const Brush *br,
                   const float brush_point[3],
                   const float len,
                   const short vno[3],
                   const float fno[3],
                   const float mask,
                   const int vertex_index,
                   const int thread_id)
{
  StrokeCache *cache = ss->cache;
  const Scene *scene = cache->vc->scene;
  const MTex *mtex = &br->mtex;
  float avg = 1;
  float rgba[4];
  float point[3];

  sub_v3_v3v3(point, brush_point, cache->plane_offset);

  if (!mtex->tex) {
    avg = 1;
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex
     * location directly into a texture */
    avg = BKE_brush_sample_tex_3d(scene, br, point, rgba, 0, ss->tex_pool);
  }
  else if (ss->texcache) {
    float symm_point[3], point_2d[2];
    float x = 0.0f, y = 0.0f; /* Quite warnings */

    /* if the active area is being applied for symmetry, flip it
     * across the symmetry axis and rotate it back to the original
     * position in order to project it. This insures that the
     * brush texture will be oriented correctly. */

    flip_v3_v3(symm_point, point, cache->mirror_symmetry_pass);

    if (cache->radial_symmetry_pass) {
      mul_m4_v3(cache->symm_rot_mat_inv, symm_point);
    }

    ED_view3d_project_float_v2_m4(cache->vc->ar, symm_point, point_2d, cache->projection_mat);

    /* still no symmetry supported for other paint modes.
     * Sculpt does it DIY */
    if (mtex->brush_map_mode == MTEX_MAP_MODE_AREA) {
      /* Similar to fixed mode, but projects from brush angle
       * rather than view direction */

      mul_m4_v3(cache->brush_local_mat, symm_point);

      x = symm_point[0];
      y = symm_point[1];

      x *= br->mtex.size[0];
      y *= br->mtex.size[1];

      x += br->mtex.ofs[0];
      y += br->mtex.ofs[1];

      avg = paint_get_tex_pixel(&br->mtex, x, y, ss->tex_pool, thread_id);

      avg += br->texture_sample_bias;
    }
    else {
      const float point_3d[3] = {point_2d[0], point_2d[1], 0.0f};
      avg = BKE_brush_sample_tex_3d(scene, br, point_3d, rgba, 0, ss->tex_pool);
    }
  }

  /* Falloff curve */
  avg *= BKE_brush_curve_strength(br, len, cache->radius);
  avg *= frontface(br, cache->view_normal, vno, fno);

  /* Paint mask */
  avg *= 1.0f - mask;

  /* Automasking */
  avg *= sculpt_automasking_factor_get(ss, vertex_index);

  return avg;
}

/* Test AABB against sphere */
static bool sculpt_search_sphere_cb(PBVHNode *node, void *data_v)
{
  SculptSearchSphereData *data = (SculptSearchSphereData *)data_v;
  float *center, nearest[3];
  if (data->center) {
    center = data->center;
  }
  else {
    center = data->ss->cache ? data->ss->cache->location : data->ss->cursor_location;
  }
  float t[3], bb_min[3], bb_max[3];
  int i;

  if (data->ignore_fully_masked) {
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_max);
  }

  for (i = 0; i < 3; i++) {
    if (bb_min[i] > center[i]) {
      nearest[i] = bb_min[i];
    }
    else if (bb_max[i] < center[i]) {
      nearest[i] = bb_max[i];
    }
    else {
      nearest[i] = center[i];
    }
  }

  sub_v3_v3v3(t, center, nearest);

  return len_squared_v3(t) < data->radius_squared;
}

/* 2D projection (distance to line). */
static bool sculpt_search_circle_cb(PBVHNode *node, void *data_v)
{
  SculptSearchCircleData *data = (SculptSearchCircleData *)data_v;
  float bb_min[3], bb_max[3];

  if (data->ignore_fully_masked) {
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_min);
  }

  float dummy_co[3], dummy_depth;
  const float dist_sq = dist_squared_ray_to_aabb_v3(
      data->dist_ray_to_aabb_precalc, bb_min, bb_max, dummy_co, &dummy_depth);

  return dist_sq < data->radius_squared || 1;
}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags */
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3])
{
  int i;

  for (i = 0; i < 3; i++) {
    if (sd->flags & (SCULPT_LOCK_X << i)) {
      continue;
    }

    if ((ss->cache->flag & (CLIP_X << i)) && (fabsf(co[i]) <= ss->cache->clip_tolerance[i])) {
      co[i] = 0.0f;
    }
    else {
      co[i] = val[i];
    }
  }
}

static PBVHNode **sculpt_pbvh_gather_cursor_update(Object *ob,
                                                   Sculpt *sd,
                                                   bool use_original,
                                                   int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  SculptSearchSphereData data;
  data.ss = ss;
  data.sd = sd;
  data.radius_squared = ss->cursor_radius;
  data.original = use_original;
  data.ignore_fully_masked = false;
  data.center = NULL;
  BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, r_totnode);
  return nodes;
}

static PBVHNode **sculpt_pbvh_gather_generic(Object *ob,
                                             Sculpt *sd,
                                             const Brush *brush,
                                             bool use_original,
                                             float radius_scale,
                                             int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;

  /* Build a list of all nodes that are potentially within the cursor or brush's area of influence
   */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    SculptSearchSphereData data;
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = SQUARE(ss->cache->radius * radius_scale);
    data.original = use_original;
    data.ignore_fully_masked = brush->sculpt_tool != SCULPT_TOOL_MASK;
    data.center = NULL;
    BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, r_totnode);
  }
  else {
    struct DistRayAABB_Precalc dist_ray_to_aabb_precalc;
    dist_squared_ray_to_aabb_v3_precalc(
        &dist_ray_to_aabb_precalc, ss->cache->location, ss->cache->view_normal);
    SculptSearchCircleData data;
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = ss->cache ? SQUARE(ss->cache->radius * radius_scale) : ss->cursor_radius;
    data.original = use_original;
    data.dist_ray_to_aabb_precalc = &dist_ray_to_aabb_precalc;
    data.ignore_fully_masked = brush->sculpt_tool != SCULPT_TOOL_MASK;
    BKE_pbvh_search_gather(ss->pbvh, sculpt_search_circle_cb, &data, &nodes, r_totnode);
  }
  return nodes;
}

/* Calculate primary direction of movement for many brushes */
static void calc_sculpt_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3])
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  const SculptSession *ss = ob->sculpt;

  switch (brush->sculpt_plane) {
    case SCULPT_DISP_DIR_VIEW:
      copy_v3_v3(r_area_no, ss->cache->true_view_normal);
      break;

    case SCULPT_DISP_DIR_X:
      ARRAY_SET_ITEMS(r_area_no, 1, 0, 0);
      break;

    case SCULPT_DISP_DIR_Y:
      ARRAY_SET_ITEMS(r_area_no, 0, 1, 0);
      break;

    case SCULPT_DISP_DIR_Z:
      ARRAY_SET_ITEMS(r_area_no, 0, 0, 1);
      break;

    case SCULPT_DISP_DIR_AREA:
      calc_area_normal(sd, ob, nodes, totnode, r_area_no);
      break;

    default:
      break;
  }
}

static void update_sculpt_normal(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  const Brush *brush = BKE_paint_brush(&sd->paint);
  StrokeCache *cache = ob->sculpt->cache;
  /* Grab brush does not update the sculpt normal during a stroke */
  const bool update_normal = !(brush->flag & BRUSH_ORIGINAL_NORMAL) &&
                             !(brush->sculpt_tool == SCULPT_TOOL_GRAB) &&
                             !(brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
                             !(brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK &&
                               cache->normal_weight > 0.0f);

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0 &&
      (cache->first_time || update_normal)) {
    calc_sculpt_normal(sd, ob, nodes, totnode, cache->sculpt_normal);
    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache->sculpt_normal, cache->sculpt_normal, cache->view_normal);
      normalize_v3(cache->sculpt_normal);
    }
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
  }
  else {
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
    flip_v3(cache->sculpt_normal_symm, cache->mirror_symmetry_pass);
    mul_m4_v3(cache->symm_rot_mat, cache->sculpt_normal_symm);
  }
}

static void calc_local_y(ViewContext *vc, const float center[3], float y[3])
{
  Object *ob = vc->obact;
  float loc[3], mval_f[2] = {0.0f, 1.0f};
  float zfac;

  mul_v3_m4v3(loc, ob->imat, center);
  zfac = ED_view3d_calc_zfac(vc->rv3d, loc, NULL);

  ED_view3d_win_to_delta(vc->ar, mval_f, y, zfac);
  normalize_v3(y);

  add_v3_v3(y, ob->loc);
  mul_m4_v3(ob->imat, y);
}

static void calc_brush_local_mat(const Brush *brush, Object *ob, float local_mat[4][4])
{
  const StrokeCache *cache = ob->sculpt->cache;
  float tmat[4][4];
  float mat[4][4];
  float scale[4][4];
  float angle, v[3];
  float up[3];

  /* Ensure ob->imat is up to date */
  invert_m4_m4(ob->imat, ob->obmat);

  /* Initialize last column of matrix */
  mat[0][3] = 0;
  mat[1][3] = 0;
  mat[2][3] = 0;
  mat[3][3] = 1;

  /* Get view's up vector in object-space */
  calc_local_y(cache->vc, cache->location, up);

  /* Calculate the X axis of the local matrix */
  cross_v3_v3v3(v, up, cache->sculpt_normal);
  /* Apply rotation (user angle, rake, etc.) to X axis */
  angle = brush->mtex.rot - cache->special_rotation;
  rotate_v3_v3v3fl(mat[0], v, cache->sculpt_normal, angle);

  /* Get other axes */
  cross_v3_v3v3(mat[1], cache->sculpt_normal, mat[0]);
  copy_v3_v3(mat[2], cache->sculpt_normal);

  /* Set location */
  copy_v3_v3(mat[3], cache->location);

  /* Scale by brush radius */
  normalize_m4(mat);
  scale_m4_fl(scale, cache->radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Return inverse (for converting from modelspace coords to local
   * area coords) */
  invert_m4_m4(local_mat, tmat);
}

static void update_brush_local_mat(Sculpt *sd, Object *ob)
{
  StrokeCache *cache = ob->sculpt->cache;

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0) {
    calc_brush_local_mat(BKE_paint_brush(&sd->paint), ob, cache->brush_local_mat);
  }
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
 * a smoothed location for vert. Skips corner vertices (used by only one
 * polygon.) */
static void neighbor_average(SculptSession *ss, float avg[3], unsigned vert)
{
  const MeshElemMap *vert_map = &ss->pmap[vert];
  const MVert *mvert = ss->mvert;
  float(*deform_co)[3] = ss->deform_cos;

  /* Don't modify corner vertices */
  if (vert_map->count > 1) {
    int i, total = 0;

    zero_v3(avg);

    for (i = 0; i < vert_map->count; i++) {
      const MPoly *p = &ss->mpoly[vert_map->indices[i]];
      unsigned f_adj_v[2];

      if (poly_get_adj_loops_from_vert(p, ss->mloop, vert, f_adj_v) != -1) {
        int j;
        for (j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
          if (vert_map->count != 2 || ss->pmap[f_adj_v[j]].count <= 2) {
            add_v3_v3(avg, deform_co ? deform_co[f_adj_v[j]] : mvert[f_adj_v[j]].co);

            total++;
          }
        }
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      return;
    }
  }

  copy_v3_v3(avg, deform_co ? deform_co[vert] : mvert[vert].co);
}

/* Similar to neighbor_average(), but returns an averaged mask value
 * instead of coordinate. Also does not restrict based on border or
 * corner vertices. */
static float neighbor_average_mask(SculptSession *ss, unsigned vert)
{
  const float *vmask = ss->vmask;
  float avg = 0;
  int i, total = 0;

  for (i = 0; i < ss->pmap[vert].count; i++) {
    const MPoly *p = &ss->mpoly[ss->pmap[vert].indices[i]];
    unsigned f_adj_v[2];

    if (poly_get_adj_loops_from_vert(p, ss->mloop, vert, f_adj_v) != -1) {
      int j;
      for (j = 0; j < ARRAY_SIZE(f_adj_v); j += 1) {
        avg += vmask[f_adj_v[j]];
        total++;
      }
    }
  }

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    return vmask[vert];
  }
}

/* Same logic as neighbor_average(), but for bmesh rather than mesh */
static void bmesh_neighbor_average(float avg[3], BMVert *v)
{
  /* logic for 3 or more is identical */
  const int vfcount = BM_vert_face_count_at_most(v, 3);

  /* Don't modify corner vertices */
  if (vfcount > 1) {
    BMIter liter;
    BMLoop *l;
    int i, total = 0;

    zero_v3(avg);

    BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
      const BMVert *adj_v[2] = {l->prev->v, l->next->v};

      for (i = 0; i < ARRAY_SIZE(adj_v); i++) {
        const BMVert *v_other = adj_v[i];
        if (vfcount != 2 || BM_vert_face_count_at_most(v_other, 2) <= 2) {
          add_v3_v3(avg, v_other->co);
          total++;
        }
      }
    }

    if (total > 0) {
      mul_v3_fl(avg, 1.0f / total);
      return;
    }
  }

  copy_v3_v3(avg, v->co);
}

/* For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure. */
static void bmesh_four_neighbor_average(float avg[3], float direction[3], BMVert *v)
{

  float avg_co[3] = {0, 0, 0};
  float tot_co = 0;

  BMIter eiter;
  BMEdge *e;

  BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      copy_v3_v3(avg, v->co);
      return;
    }
    BMVert *v_other = (e->v1 == v) ? e->v2 : e->v1;
    float vec[3];
    sub_v3_v3v3(vec, v_other->co, v->co);
    madd_v3_v3fl(vec, v->no, -dot_v3v3(vec, v->no));
    normalize_v3(vec);

    /* fac is a measure of how orthogonal or parallel the edge is
     * relative to the direction */
    float fac = dot_v3v3(vec, direction);
    fac = fac * fac - 0.5f;
    fac *= fac;
    madd_v3_v3fl(avg_co, v_other->co, fac);
    tot_co += fac;
  }

  /* In case vert has no Edge s */
  if (tot_co > 0) {
    mul_v3_v3fl(avg, avg_co, 1.0f / tot_co);

    /* Preserve volume. */
    float vec[3];
    sub_v3_v3(avg, v->co);
    mul_v3_v3fl(vec, v->no, dot_v3v3(avg, v->no));
    sub_v3_v3(avg, vec);
    add_v3_v3(avg, v->co);
  }
  else {
    zero_v3(avg);
  }
}

/* Same logic as neighbor_average_mask(), but for bmesh rather than mesh */
static float bmesh_neighbor_average_mask(BMVert *v, const int cd_vert_mask_offset)
{
  BMIter liter;
  BMLoop *l;
  float avg = 0;
  int i, total = 0;

  BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
    /* skip this vertex */
    const BMVert *adj_v[2] = {l->prev->v, l->next->v};

    for (i = 0; i < ARRAY_SIZE(adj_v); i++) {
      const BMVert *v_other = adj_v[i];
      const float *vmask = (float *)BM_ELEM_CD_GET_VOID_P(v_other, cd_vert_mask_offset);
      avg += (*vmask);
      total++;
    }
  }

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    const float *vmask = (float *)BM_ELEM_CD_GET_VOID_P(v, cd_vert_mask_offset);
    return (*vmask);
  }
}

static void grids_neighbor_average(SculptSession *ss, float result[3], int index)
{
  float avg[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, index, ni)
  {
    add_v3_v3(avg, sculpt_vertex_co_get(ss, ni.index));
    total++;
  }
  sculpt_vertex_neighbors_iter_end(ni);

  if (total > 0) {
    mul_v3_v3fl(result, avg, 1.0f / (float)total);
  }
  else {
    copy_v3_v3(result, sculpt_vertex_co_get(ss, index));
  }
}

static float grids_neighbor_average_mask(SculptSession *ss, int index)
{
  float avg = 0.0f;
  int total = 0;

  SculptVertexNeighborIter ni;
  sculpt_vertex_neighbors_iter_begin(ss, index, ni)
  {
    avg += sculpt_vertex_mask_get(ss, ni.index);
    total++;
  }
  sculpt_vertex_neighbors_iter_end(ni);

  if (total > 0) {
    return avg / (float)total;
  }
  else {
    return sculpt_vertex_mask_get(ss, index);
  }
}

/* Note: uses after-struct allocated mem to store actual cache... */
typedef struct SculptDoBrushSmoothGridDataChunk {
  size_t tmpgrid_size;
} SculptDoBrushSmoothGridDataChunk;

typedef struct {
  SculptSession *ss;
  const float *ray_start;
  const float *ray_normal;
  bool hit;
  float depth;
  bool original;

  int active_vertex_index;
  float *face_normal;

  struct IsectRayPrecalc isect_precalc;
} SculptRaycastData;

typedef struct {
  const float *ray_start;
  bool hit;
  float depth;
  float edge_length;

  struct IsectRayPrecalc isect_precalc;
} SculptDetailRaycastData;

typedef struct {
  SculptSession *ss;
  const float *ray_start, *ray_normal;
  bool hit;
  float depth;
  float dist_sq_to_ray;
  bool original;
} SculptFindNearestToRayData;

static void do_smooth_brush_mesh_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  smooth_mask ? 0.0f : (vd.mask ? *vd.mask : 0.0f),
                                                  vd.index,
                                                  tls->thread_id);
      if (smooth_mask) {
        float val = neighbor_average_mask(ss, vd.vert_indices[vd.i]) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];

        neighbor_average(ss, avg, vd.vert_indices[vd.i]);
        sub_v3_v3v3(val, avg, vd.co);

        madd_v3_v3v3fl(val, vd.co, val, fade);

        sculpt_clip(sd, ss, vd.co, val);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smooth_brush_bmesh_task_cb_ex(void *__restrict userdata,
                                             const int n,
                                             const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  PBVHVertexIter vd;

  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  smooth_mask ? 0.0f : *vd.mask,
                                                  vd.index,
                                                  tls->thread_id);
      if (smooth_mask) {
        float val = bmesh_neighbor_average_mask(vd.bm_vert, vd.cd_vert_mask_offset) - *vd.mask;
        val *= fade * bstrength;
        *vd.mask += val;
        CLAMP(*vd.mask, 0.0f, 1.0f);
      }
      else {
        float avg[3], val[3];

        bmesh_neighbor_average(avg, vd.bm_vert);
        sub_v3_v3v3(val, avg, vd.co);

        madd_v3_v3v3fl(val, vd.co, val, fade);

        sculpt_clip(sd, ss, vd.co, val);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_topology_rake_bmesh_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;

  float direction[3];
  copy_v3_v3(direction, ss->cache->grab_delta_symmetry);

  float tmp[3];
  mul_v3_v3fl(
      tmp, ss->cache->sculpt_normal_symm, dot_v3v3(ss->cache->sculpt_normal_symm, direction));
  sub_v3_v3(direction, tmp);
  normalize_v3(direction);

  /* Cancel if there's no grab data. */
  if (is_zero_v3(direction)) {
    return;
  }

  float bstrength = data->strength;
  CLAMP(bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength *
                         tex_strength(ss,
                                      brush,
                                      vd.co,
                                      sqrtf(test.dist),
                                      vd.no,
                                      vd.fno,
                                      *vd.mask,
                                      vd.index,
                                      tls->thread_id) *
                         ss->cache->pressure;

      float avg[3], val[3];

      bmesh_four_neighbor_average(avg, direction, vd.bm_vert);

      sub_v3_v3v3(val, avg, vd.co);

      madd_v3_v3v3fl(val, vd.co, val, fade);

      sculpt_clip(sd, ss, vd.co, val);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smooth_brush_multires_task_cb_ex(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptDoBrushSmoothGridDataChunk *data_chunk = (SculptDoBrushSmoothGridDataChunk *)tls->userdata_chunk;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const bool smooth_mask = data->smooth_mask;
  float bstrength = data->strength;

  CCGElem **griddata, *gddata;

  float(*tmpgrid_co)[3] = NULL;
  float tmprow_co[2][3];
  float *tmpgrid_mask = NULL;
  float tmprow_mask[2];

  BLI_bitmap *const *grid_hidden;
  int *grid_indices, totgrid, gridsize;
  int i, x, y;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  CLAMP(bstrength, 0.0f, 1.0f);

  BKE_pbvh_node_get_grids(
      ss->pbvh, data->nodes[n], &grid_indices, &totgrid, NULL, &gridsize, &griddata);
  CCGKey key = *BKE_pbvh_get_grid_key(ss->pbvh);

  grid_hidden = BKE_pbvh_grid_hidden(ss->pbvh);

  if (smooth_mask) {
    tmpgrid_mask = (float *)(data_chunk + 1);
  }
  else {
    tmpgrid_co = (float (*)[3])(data_chunk + 1);
  }

  for (i = 0; i < totgrid; i++) {
    int gi = grid_indices[i];
    const BLI_bitmap *gh = grid_hidden[gi];
    gddata = griddata[gi];

    if (smooth_mask) {
      memset(tmpgrid_mask, 0, data_chunk->tmpgrid_size);
    }
    else {
      memset(tmpgrid_co, 0, data_chunk->tmpgrid_size);
    }

    for (y = 0; y < gridsize - 1; y++) {
      const int v = y * gridsize;
      if (smooth_mask) {
        tmprow_mask[0] = (*CCG_elem_offset_mask(&key, gddata, v) +
                          *CCG_elem_offset_mask(&key, gddata, v + gridsize));
      }
      else {
        add_v3_v3v3(tmprow_co[0],
                    CCG_elem_offset_co(&key, gddata, v),
                    CCG_elem_offset_co(&key, gddata, v + gridsize));
      }

      for (x = 0; x < gridsize - 1; x++) {
        const int v1 = x + y * gridsize;
        const int v2 = v1 + 1;
        const int v3 = v1 + gridsize;
        const int v4 = v3 + 1;

        if (smooth_mask) {
          float tmp;

          tmprow_mask[(x + 1) % 2] = (*CCG_elem_offset_mask(&key, gddata, v2) +
                                      *CCG_elem_offset_mask(&key, gddata, v4));
          tmp = tmprow_mask[(x + 1) % 2] + tmprow_mask[x % 2];

          tmpgrid_mask[v1] += tmp;
          tmpgrid_mask[v2] += tmp;
          tmpgrid_mask[v3] += tmp;
          tmpgrid_mask[v4] += tmp;
        }
        else {
          float tmp[3];

          add_v3_v3v3(tmprow_co[(x + 1) % 2],
                      CCG_elem_offset_co(&key, gddata, v2),
                      CCG_elem_offset_co(&key, gddata, v4));
          add_v3_v3v3(tmp, tmprow_co[(x + 1) % 2], tmprow_co[x % 2]);

          add_v3_v3(tmpgrid_co[v1], tmp);
          add_v3_v3(tmpgrid_co[v2], tmp);
          add_v3_v3(tmpgrid_co[v3], tmp);
          add_v3_v3(tmpgrid_co[v4], tmp);
        }
      }
    }

    /* blend with existing coordinates */
    for (y = 0; y < gridsize; y++) {
      for (x = 0; x < gridsize; x++) {
        float *co;
        const float *fno;
        float *mask;
        const int index = y * gridsize + x;

        if (gh) {
          if (BLI_BITMAP_TEST(gh, index)) {
            continue;
          }
        }

        co = CCG_elem_offset_co(&key, gddata, index);
        fno = CCG_elem_offset_no(&key, gddata, index);
        mask = CCG_elem_offset_mask(&key, gddata, index);

        if (sculpt_brush_test_sq_fn(&test, co)) {
          const float strength_mask = (smooth_mask ? 0.0f : *mask);
          const float fade =
              bstrength *
              tex_strength(
                  ss, brush, co, sqrtf(test.dist), NULL, fno, strength_mask, 0, tls->thread_id);
          float f = 1.0f / 16.0f;

          if (x == 0 || x == gridsize - 1) {
            f *= 2.0f;
          }

          if (y == 0 || y == gridsize - 1) {
            f *= 2.0f;
          }

          if (smooth_mask) {
            *mask += ((tmpgrid_mask[index] * f) - *mask) * fade;
          }
          else {
            float *avg = tmpgrid_co[index];
            float val[3];

            mul_v3_fl(avg, f);
            sub_v3_v3v3(val, avg, co);
            madd_v3_v3v3fl(val, co, val, fade);

            sculpt_clip(sd, ss, co, val);
          }
        }
      }
    }
  }
}

static void smooth(Sculpt *sd,
                   Object *ob,
                   PBVHNode **nodes,
                   const int totnode,
                   float bstrength,
                   const bool smooth_mask)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const int max_iterations = 4;
  const float fract = 1.0f / max_iterations;
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  int iteration, count;
  float last;

  CLAMP(bstrength, 0.0f, 1.0f);

  count = (int)(bstrength * max_iterations);
  last = max_iterations * (bstrength - count * fract);

  if (type == PBVH_FACES && !ss->pmap) {
    BLI_assert(!"sculpt smooth: pmap missing");
    return;
  }

  for (iteration = 0; iteration <= count; iteration++) {
    const float strength = (iteration != count) ? 1.0f : last;

    SculptThreadedTaskData data;
    data.sd = sd;
    data.ob = ob;
    data.brush = brush;
    data.nodes = nodes;
    data.smooth_mask = smooth_mask;
    data.strength = strength;

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);

    switch (type) {
      case PBVH_GRIDS: {
        int gridsize;
        size_t size;
        SculptDoBrushSmoothGridDataChunk *data_chunk;

        BKE_pbvh_node_get_grids(ss->pbvh, NULL, NULL, NULL, NULL, &gridsize, NULL);
        size = (size_t)gridsize;
        size = sizeof(float) * size * size * (smooth_mask ? 1 : 3);
        data_chunk = (SculptDoBrushSmoothGridDataChunk *)MEM_mallocN(sizeof(*data_chunk) + size, __func__);
        data_chunk->tmpgrid_size = size;
        size += sizeof(*data_chunk);

        settings.userdata_chunk = data_chunk;
        settings.userdata_chunk_size = size;
        BKE_pbvh_parallel_range(0, totnode, &data, do_smooth_brush_multires_task_cb_ex, &settings);

        MEM_freeN(data_chunk);
        break;
      }
      case PBVH_FACES:
        BKE_pbvh_parallel_range(0, totnode, &data, do_smooth_brush_mesh_task_cb_ex, &settings);
        break;
      case PBVH_BMESH:
        BKE_pbvh_parallel_range(0, totnode, &data, do_smooth_brush_bmesh_task_cb_ex, &settings);
        break;
    }

    if (ss->multires) {
      multires_stitch_grids(ob);
    }
  }
}

static void bmesh_topology_rake(
    Sculpt *sd, Object *ob, PBVHNode **nodes, const int totnode, float bstrength)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  CLAMP(bstrength, 0.0f, 1.0f);

  /* Interactions increase both strength and quality. */
  const int iterations = 3;

  int iteration;
  const int count = iterations * bstrength + 1;
  const float factor = iterations * bstrength / count;

  for (iteration = 0; iteration <= count; iteration++) {

    SculptThreadedTaskData data;
    data.sd = sd;
    data.ob = ob;
    data.brush = brush;
    data.nodes = nodes;
    data.strength = factor;
    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);

    BKE_pbvh_parallel_range(0, totnode, &data, do_topology_rake_bmesh_task_cb_ex, &settings);
  }
}

static void do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  smooth(sd, ob, nodes, totnode, ss->cache->bstrength, false);
}

static void do_mask_brush_draw_task_cb_ex(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = tex_strength(
          ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, 0.0f, vd.index, tls->thread_id);

      (*vd.mask) += fade * bstrength;
      CLAMP(*vd.mask, 0, 1);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_mask_brush_draw(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* threaded loop over nodes */
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_mask_brush_draw_task_cb_ex, &settings);
}

static void do_mask_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((BrushMaskTool)brush->mask_tool) {
    case BRUSH_MASK_DRAW:
      do_mask_brush_draw(sd, ob, nodes, totnode);
      break;
    case BRUSH_MASK_SMOOTH:
      smooth(sd, ob, nodes, totnode, ss->cache->bstrength, true);
      break;
  }
}

static void do_draw_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      /* offset vertex */
      const float fade = tex_strength(ss,
                                      brush,
                                      vd.co,
                                      sqrtf(test.dist),
                                      vd.no,
                                      vd.fno,
                                      vd.mask ? *vd.mask : 0.0f,
                                      vd.index,
                                      tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], offset, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* offset with as much as possible factored in already */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX - this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping */
  BKE_curvemapping_initialize(brush->curve);

  /* threaded loop over nodes */
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.offset = offset;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_draw_brush_task_cb_ex, &settings);
}

static void do_draw_sharp_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);
    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      /* offset vertex */
      const float fade = tex_strength(ss,
                                      brush,
                                      orig_data.co,
                                      sqrtf(test.dist),
                                      orig_data.no,
                                      NULL,
                                      vd.mask ? *vd.mask : 0.0f,
                                      vd.index,
                                      tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], offset, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_sharp_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* offset with as much as possible factored in already */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX - this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping */
  BKE_curvemapping_initialize(brush->curve);

  /* threaded loop over nodes */
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.offset = offset;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_draw_sharp_brush_task_cb_ex, &settings);
}

/**
 * Used for 'SCULPT_TOOL_CREASE' and 'SCULPT_TOOL_BLOB'
 */
static void do_crease_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float flippedbstrength = data->flippedbstrength;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      /* offset vertex */
      const float fade = tex_strength(ss,
                                      brush,
                                      vd.co,
                                      sqrtf(test.dist),
                                      vd.no,
                                      vd.fno,
                                      vd.mask ? *vd.mask : 0.0f,
                                      vd.index,
                                      tls->thread_id);
      float val1[3];
      float val2[3];

      /* first we pinch */
      sub_v3_v3v3(val1, test.location, vd.co);
      if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(val1, val1, ss->cache->view_normal);
      }

      mul_v3_fl(val1, fade * flippedbstrength);

      sculpt_project_v3(spvc, val1, val1);

      /* then we draw */
      mul_v3_v3fl(val2, offset, fade);

      add_v3_v3v3(proxy[vd.i], val1, val2);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  const Scene *scene = ss->cache->vc->scene;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  float bstrength = ss->cache->bstrength;
  float flippedbstrength, crease_correction;
  float brush_alpha;

  SculptProjectVector spvc;

  /* offset with as much as possible factored in already */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* We divide out the squared alpha and multiply by the squared crease
   * to give us the pinch strength. */
  crease_correction = brush->crease_pinch_factor * brush->crease_pinch_factor;
  brush_alpha = BKE_brush_alpha_get(scene, brush);
  if (brush_alpha > 0.0f) {
    crease_correction /= brush_alpha * brush_alpha;
  }

  /* we always want crease to pinch or blob to relax even when draw is negative */
  flippedbstrength = (bstrength < 0) ? -crease_correction * bstrength :
                                       crease_correction * bstrength;

  if (brush->sculpt_tool == SCULPT_TOOL_BLOB) {
    flippedbstrength *= -1.0f;
  }

  /* Use surface normal for 'spvc',
   * so the vertices are pinched towards a line instead of a single point.
   * Without this we get a 'flat' surface surrounding the pinch */
  sculpt_project_v3_cache_init(&spvc, ss->cache->sculpt_normal_symm);

  /* threaded loop over nodes */
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.spvc = &spvc;
  data.offset = offset;
  data.flippedbstrength = flippedbstrength;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_crease_brush_task_cb_ex, &settings);
}

static void do_pinch_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);
      float val[3];

      sub_v3_v3v3(val, test.location, vd.co);
      if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(val, val, ss->cache->view_normal);
      }
      mul_v3_v3fl(proxy[vd.i], val, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_pinch_brush_task_cb_ex, &settings);
}

static void do_grab_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  orig_data.co,
                                                  sqrtf(test.dist),
                                                  orig_data.no,
                                                  NULL,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.grab_delta = grab_delta;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_grab_brush_task_cb_ex, &settings);
}

/* Regularized Kelvinlets: Sculpting Brushes based on Fundamental Solutions of Elasticity
 * Pixar Technical Memo #17-03 */

typedef struct KelvinletParams {
  float f;
  float a;
  float b;
  float c;
  float radius_scaled;
} KelvinletParams;

static int sculpt_kelvinlet_get_scale_iteration_count(eBrushElasticDeformType type)
{
  if (type == BRUSH_ELASTIC_DEFORM_GRAB) {
    return 1;
  }
  if (type == BRUSH_ELASTIC_DEFORM_GRAB_BISCALE) {
    return 2;
  }
  if (type == BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE) {
    return 3;
  }
  return 0;
}

static void sculpt_kelvinet_integrate(void (*kelvinlet)(float disp[3],
                                                        const float vertex_co[3],
                                                        const float location[3],
                                                        float normal[3],
                                                        KelvinletParams *p),
                                      float r_disp[3],
                                      const float vertex_co[3],
                                      const float location[3],
                                      float normal[3],
                                      KelvinletParams *p)
{
  float k[4][3], k_it[4][3];
  kelvinlet(k[0], vertex_co, location, normal, p);
  copy_v3_v3(k_it[0], k[0]);
  mul_v3_fl(k_it[0], 0.5f);
  add_v3_v3v3(k_it[0], vertex_co, k_it[0]);
  kelvinlet(k[1], k_it[0], location, normal, p);
  copy_v3_v3(k_it[1], k[1]);
  mul_v3_fl(k_it[1], 0.5f);
  add_v3_v3v3(k_it[1], vertex_co, k_it[1]);
  kelvinlet(k[2], k_it[1], location, normal, p);
  copy_v3_v3(k_it[2], k[2]);
  add_v3_v3v3(k_it[2], vertex_co, k_it[2]);
  sub_v3_v3v3(k_it[2], k_it[2], location);
  kelvinlet(k[3], k_it[2], location, normal, p);
  copy_v3_v3(r_disp, k[0]);
  madd_v3_v3fl(r_disp, k[1], 2);
  madd_v3_v3fl(r_disp, k[2], 2);
  add_v3_v3(r_disp, k[3]);
  mul_v3_fl(r_disp, 1.0f / 6.0f);
}

/* Regularized Kelvinlets: Formula (16) */
static void sculpt_kelvinlet_scale(float disp[3],
                                   const float vertex_co[3],
                                   const float location[3],
                                   float UNUSED(normal[3]),
                                   KelvinletParams *p)
{
  float r_v[3];
  sub_v3_v3v3(r_v, vertex_co, location);
  float r = len_v3(r_v);
  float r_e = sqrtf(r * r + p->radius_scaled * p->radius_scaled);
  float u = (2.0f * p->b - p->a) * ((1.0f / (r_e * r_e * r_e))) +
            ((3.0f * p->radius_scaled * p->radius_scaled) / (2.0f * r_e * r_e * r_e * r_e * r_e));
  float fade = u * p->c;
  mul_v3_v3fl(disp, r_v, fade * p->f);
}

/* Regularized Kelvinlets: Formula (15) */
static void sculpt_kelvinlet_twist(float disp[3],
                                   const float vertex_co[3],
                                   const float location[3],
                                   float normal[3],
                                   KelvinletParams *p)
{
  float r_v[3], q_r[3];
  sub_v3_v3v3(r_v, vertex_co, location);
  float r = len_v3(r_v);
  float r_e = sqrtf(r * r + p->radius_scaled * p->radius_scaled);
  float u = -p->a * ((1.0f / (r_e * r_e * r_e))) +
            ((3.0f * p->radius_scaled * p->radius_scaled) / (2.0f * r_e * r_e * r_e * r_e * r_e));
  float fade = u * p->c;
  cross_v3_v3v3(q_r, normal, r_v);
  mul_v3_v3fl(disp, q_r, fade * p->f);
}

static void do_elastic_deform_brush_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;
  const float *location = ss->cache->location;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  const float bstrength = ss->cache->bstrength;

  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  /* Maybe this can be exposed to the user */
  float radius_e[3] = {1.0f, 2.0f, 2.0f};
  float r_e[3];
  float kvl[3];
  float radius_scaled[3];

  radius_scaled[0] = ss->cache->radius * radius_e[0];
  radius_scaled[1] = radius_scaled[0] * radius_e[1];
  radius_scaled[2] = radius_scaled[1] * radius_e[2];

  float shear_modulus = 1.0f;
  float poisson_ratio = brush->elastic_deform_volume_preservation;

  float a = 1.0f / (4.0f * (float)M_PI * shear_modulus);
  float b = a / (4.0f * (1.0f - poisson_ratio));
  float c = 2 * (3.0f * a - 2.0f * b);

  float dir;
  if (ss->cache->mouse[0] > ss->cache->initial_mouse[0]) {
    dir = 1.0f;
  }
  else {
    dir = -1.0f;
  }

  if (brush->elastic_deform_type == BRUSH_ELASTIC_DEFORM_TWIST) {
    int symm = ss->cache->mirror_symmetry_pass;
    if (symm == 1 || symm == 2 || symm == 4 || symm == 7) {
      dir = -dir;
    }
  }
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);
    float fade, final_disp[3], weights[3];
    float r = len_v3v3(location, orig_data.co);
    KelvinletParams params;
    params.a = a;
    params.b = b;
    params.c = c;
    params.radius_scaled = radius_scaled[0];

    int multi_scale_it = sculpt_kelvinlet_get_scale_iteration_count((eBrushElasticDeformType)brush->elastic_deform_type);
    for (int it = 0; it < max_ii(1, multi_scale_it); it++) {
      r_e[it] = sqrtf(r * r + radius_scaled[it] * radius_scaled[it]);
    }

    /* Regularized Kelvinlets: Formula (6) */
    for (int s_it = 0; s_it < multi_scale_it; s_it++) {
      kvl[s_it] = ((a - b) / r_e[s_it]) + ((b * r * r) / (r_e[s_it] * r_e[s_it] * r_e[s_it])) +
                  ((a * radius_scaled[s_it] * radius_scaled[s_it]) /
                   (2.0f * r_e[s_it] * r_e[s_it] * r_e[s_it]));
    }

    switch (brush->elastic_deform_type) {
      /* Regularized Kelvinlets: Multi-scale extrapolation. Formula (11) */
      case BRUSH_ELASTIC_DEFORM_GRAB:
        fade = kvl[0] * c;
        mul_v3_v3fl(final_disp, grab_delta, fade * bstrength * 20.f);
        break;
      case BRUSH_ELASTIC_DEFORM_GRAB_BISCALE: {
        const float u = kvl[0] - kvl[1];
        fade = u * c / ((1.0f / radius_scaled[0]) - (1.0f / radius_scaled[1]));
        mul_v3_v3fl(final_disp, grab_delta, fade * bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE: {
        weights[0] = 1.0f;
        weights[1] = -(
            (radius_scaled[2] * radius_scaled[2] - radius_scaled[0] * radius_scaled[0]) /
            (radius_scaled[2] * radius_scaled[2] - radius_scaled[1] * radius_scaled[1]));
        weights[2] = ((radius_scaled[1] * radius_scaled[1] - radius_scaled[0] * radius_scaled[0]) /
                      (radius_scaled[2] * radius_scaled[2] - radius_scaled[1] * radius_scaled[1]));

        const float u = weights[0] * kvl[0] + weights[1] * kvl[1] + weights[2] * kvl[2];
        fade = u * c /
               (weights[0] / radius_scaled[0] + weights[1] / radius_scaled[1] +
                weights[2] / radius_scaled[2]);
        mul_v3_v3fl(final_disp, grab_delta, fade * bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_SCALE:
        params.f = len_v3(grab_delta) * dir * bstrength;
        sculpt_kelvinet_integrate(sculpt_kelvinlet_scale,
                                  final_disp,
                                  orig_data.co,
                                  location,
                                  ss->cache->sculpt_normal_symm,
                                  &params);
        break;
      case BRUSH_ELASTIC_DEFORM_TWIST:
        params.f = len_v3(grab_delta) * dir * bstrength;
        sculpt_kelvinet_integrate(sculpt_kelvinlet_twist,
                                  final_disp,
                                  orig_data.co,
                                  location,
                                  ss->cache->sculpt_normal_symm,
                                  &params);
        break;
    }

    if (vd.mask) {
      mul_v3_fl(final_disp, 1.0f - *vd.mask);
    }

    mul_v3_fl(final_disp, sculpt_automasking_factor_get(ss, vd.index));

    copy_v3_v3(proxy[vd.i], final_disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_elastic_deform_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.grab_delta = grab_delta;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_elastic_deform_brush_task_cb_ex, &settings);
}

static void do_pose_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;

  PBVHVertexIter vd;
  float disp[3], val[3];
  float final_pos[3];

  SculptOrigVertData orig_data;
  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {

    sculpt_orig_vert_data_update(&orig_data, &vd);
    if (check_vertex_pivot_symmetry(
            orig_data.co, data->pose_initial_co, ss->cache->mirror_symmetry_pass)) {
      copy_v3_v3(val, orig_data.co);
      mul_m4_v3(data->transform_trans_inv, val);
      mul_m4_v3(data->transform_rot, val);
      mul_m4_v3(data->transform_trans, val);
      sub_v3_v3v3(disp, val, orig_data.co);

      mul_v3_fl(disp, ss->cache->pose_factor[vd.index]);
      float mask = vd.mask ? *vd.mask : 0.0f;
      mul_v3_fl(disp, 1.0f - mask);
      add_v3_v3v3(final_pos, orig_data.co, disp);
      copy_v3_v3(vd.co, final_pos);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_pose_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3], rot_quat[4], initial_v[3], current_v[3], temp[3];
  float pose_origin[3];
  float pose_initial_co[3];
  float transform_rot[4][4], transform_trans[4][4], transform_trans_inv[4][4];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  copy_v3_v3(pose_origin, ss->cache->pose_origin);
  flip_v3(pose_origin, (char)ss->cache->mirror_symmetry_pass);

  copy_v3_v3(pose_initial_co, ss->cache->pose_initial_co);
  flip_v3(pose_initial_co, (char)ss->cache->mirror_symmetry_pass);

  sub_v3_v3v3(initial_v, pose_initial_co, pose_origin);
  normalize_v3(initial_v);

  add_v3_v3v3(temp, pose_initial_co, grab_delta);
  sub_v3_v3v3(current_v, temp, pose_origin);
  normalize_v3(current_v);

  rotation_between_vecs_to_quat(rot_quat, initial_v, current_v);
  unit_m4(transform_rot);
  unit_m4(transform_trans);
  quat_to_mat4(transform_rot, rot_quat);
  translate_m4(transform_trans, pose_origin[0], pose_origin[1], pose_origin[2]);
  invert_m4_m4(transform_trans_inv, transform_trans);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.grab_delta = grab_delta;
  data.pose_origin = pose_origin;
  data.pose_initial_co = pose_initial_co;
  data.transform_rot = transform_rot;
  data.transform_trans = transform_trans;
  data.transform_trans_inv = transform_trans_inv;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_pose_brush_task_cb_ex, &settings);
}

typedef struct PoseGrowFactorTLSData {
  float pos_avg[3];
  int pos_count;
} PoseGrowFactorTLSData;

static void pose_brush_grow_factor_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  PoseGrowFactorTLSData *gftd = (PoseGrowFactorTLSData *)tls->userdata_chunk;
  SculptSession *ss = data->ob->sculpt;
  const char symm = data->sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  const float *active_co = sculpt_active_vertex_co_get(ss);
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SculptVertexNeighborIter ni;
    float max = 0.0f;
    sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
    {
      float vmask_f = data->prev_mask[ni.index];
      if (vmask_f > max) {
        max = vmask_f;
      }
    }
    sculpt_vertex_neighbors_iter_end(ni);
    if (max != data->prev_mask[vd.index]) {
      data->pose_factor[vd.index] = max;
      if (check_vertex_pivot_symmetry(vd.co, active_co, symm)) {
        add_v3_v3(gftd->pos_avg, vd.co);
        gftd->pos_count++;
      }
    }
  }

  BKE_pbvh_vertex_iter_end;
}

static void pose_brush_grow_factor_reduce(const void *__restrict UNUSED(userdata),
                                          void *__restrict chunk_join,
                                          void *__restrict chunk)
{
  PoseGrowFactorTLSData *join = (PoseGrowFactorTLSData *)chunk_join;
  PoseGrowFactorTLSData *gftd = (PoseGrowFactorTLSData *)chunk;
  add_v3_v3(join->pos_avg, gftd->pos_avg);
  join->pos_count += gftd->pos_count;
}

/* Grow the factor until its boundary is near to the offset pose origin */
static void sculpt_pose_grow_pose_factor(
    Sculpt *sd, Object *ob, SculptSession *ss, float pose_origin[3], float *pose_factor)
{
  PBVHNode **nodes;
  PBVH *pbvh = ob->sculpt->pbvh;
  int totnode;

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.nodes = nodes;
  data.totnode = totnode;
  data.pose_factor = pose_factor;
  PBVHParallelSettings settings;
  PoseGrowFactorTLSData gftd;
  gftd.pos_count = 0;
  zero_v3(gftd.pos_avg);
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = pose_brush_grow_factor_reduce;
  settings.userdata_chunk = &gftd;
  settings.userdata_chunk_size = sizeof(PoseGrowFactorTLSData);

  bool grow_next_iteration = true;
  float prev_len = FLT_MAX;
  data.prev_mask = (float*)MEM_mallocN(sculpt_vertex_count_get(ss) * sizeof(float), "prev mask");
  while (grow_next_iteration) {
    zero_v3(gftd.pos_avg);
    gftd.pos_count = 0;
    memcpy(data.prev_mask, pose_factor, sculpt_vertex_count_get(ss) * sizeof(float));
    BKE_pbvh_parallel_range(0, totnode, &data, pose_brush_grow_factor_task_cb_ex, &settings);
    if (gftd.pos_count != 0) {
      mul_v3_fl(gftd.pos_avg, 1.0f / (float)gftd.pos_count);
      float len = len_v3v3(gftd.pos_avg, pose_origin);
      if (len < prev_len) {
        prev_len = len;
        grow_next_iteration = true;
      }
      else {
        grow_next_iteration = false;
        memcpy(pose_factor, data.prev_mask, sculpt_vertex_count_get(ss) * sizeof(float));
      }
    }
    else {
      grow_next_iteration = false;
    }
  }
  MEM_freeN(data.prev_mask);

  MEM_SAFE_FREE(nodes);
}

static bool sculpt_pose_brush_is_vertex_inside_brush_radius(const float vertex[3],
                                                            const float br_co[3],
                                                            float radius,
                                                            char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (is_symmetry_iteration_valid(i, symm)) {
      float location[3];
      flip_v3_v3(location, br_co, (char)i);
      if (len_v3v3(location, vertex) < radius) {
        return true;
      }
    }
  }
  return false;
}

/* Calculate the pose origin and (Optionaly the pose factor) that is used when using the pose brush
 *
 * r_pose_origin must be a valid pointer. the r_pose_factor is optional. When set to NULL it won't
 * be calculated. */
typedef struct PoseFloodFillData {
  float pose_initial_co[3];
  float radius;
  int symm;

  float *pose_factor;
  float pose_origin[3];
  int tot_co;
} PoseFloodFillData;

static bool pose_floodfill_cb(
    SculptSession *ss, int UNUSED(from_v), int to_v, bool is_duplicate, void *userdata)
{
  PoseFloodFillData *data = (PoseFloodFillData *)userdata;

  if (data->pose_factor) {
    data->pose_factor[to_v] = 1.0f;
  }

  const float *co = sculpt_vertex_co_get(ss, to_v);
  if (sculpt_pose_brush_is_vertex_inside_brush_radius(
          co, data->pose_initial_co, data->radius, data->symm)) {
    return true;
  }
  else if (check_vertex_pivot_symmetry(co, data->pose_initial_co, data->symm)) {
    if (!is_duplicate) {
      add_v3_v3(data->pose_origin, co);
      data->tot_co++;
    }
  }

  return false;
}

static void sculpt_pose_calc_pose_data(Sculpt *sd,
                                Object *ob,
                                SculptSession *ss,
                                float initial_location[3],
                                float radius,
                                float pose_offset,
                                float *r_pose_origin,
                                float *r_pose_factor)
{
  sculpt_vertex_random_access_init(ss);

  /* Calculate the pose rotation point based on the boundaries of the brush factor. */
  SculptFloodFill flood;
  sculpt_floodfill_init(ss, &flood);
  sculpt_floodfill_add_active(sd, ob, ss, &flood, (r_pose_factor) ? radius : 0.0f);

  PoseFloodFillData fdata;
  fdata.radius = radius;
  fdata.symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  fdata.pose_factor = r_pose_factor;
  fdata.tot_co = 0;
  zero_v3(fdata.pose_origin);
  copy_v3_v3(fdata.pose_initial_co, initial_location);
  sculpt_floodfill_execute(ss, &flood, pose_floodfill_cb, &fdata);
  sculpt_floodfill_free(&flood);

  if (fdata.tot_co > 0) {
    mul_v3_fl(fdata.pose_origin, 1.0f / (float)fdata.tot_co);
  }

  /* Offset the pose origin */
  float pose_d[3];
  sub_v3_v3v3(pose_d, fdata.pose_origin, fdata.pose_initial_co);
  normalize_v3(pose_d);
  madd_v3_v3fl(fdata.pose_origin, pose_d, radius * pose_offset);
  copy_v3_v3(r_pose_origin, fdata.pose_origin);

  if (pose_offset != 0.0f && r_pose_factor) {
    sculpt_pose_grow_pose_factor(sd, ob, ss, fdata.pose_origin, r_pose_factor);
  }
}

static void pose_brush_init_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SculptVertexNeighborIter ni;
    float avg = 0;
    int total = 0;
    sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
    {
      avg += ss->cache->pose_factor[ni.index];
      total++;
    }
    sculpt_vertex_neighbors_iter_end(ni);

    if (total > 0) {
      ss->cache->pose_factor[vd.index] = avg / (float)total;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_pose_brush_init(
    Sculpt *sd, Object *ob, SculptSession *ss, Brush *br, float initial_location[3], float radius)
{
  float *pose_factor = (float*)MEM_callocN(sculpt_vertex_count_get(ss) * sizeof(float), "Pose factor");

  sculpt_pose_calc_pose_data(
      sd, ob, ss, initial_location, radius, br->pose_offset, ss->cache->pose_origin, pose_factor);

  copy_v3_v3(ss->cache->pose_initial_co, initial_location);
  ss->cache->pose_factor = pose_factor;

  PBVHNode **nodes;
  PBVH *pbvh = ob->sculpt->pbvh;
  int totnode;

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = br;
  data.nodes = nodes;

  /* Smooth the pose brush factor for cleaner deformation */
  for (int i = 0; i < 4; i++) {
    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, pose_brush_init_task_cb_ex, &settings);
  }

  MEM_SAFE_FREE(nodes);
}

static void do_nudge_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], cono, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.cono = cono;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_nudge_brush_task_cb_ex, &settings);
}

static void do_snake_hook_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;
  const bool do_rake_rotation = ss->cache->is_rake_rotation_valid;
  const bool do_pinch = (brush->crease_pinch_factor != 0.5f);
  const float pinch = do_pinch ? (2.0f * (0.5f - brush->crease_pinch_factor) *
                                  (len_v3(grab_delta) / ss->cache->radius)) :
                                 0.0f;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

      /* negative pinch will inflate, helps maintain volume */
      if (do_pinch) {
        float delta_pinch_init[3], delta_pinch[3];

        sub_v3_v3v3(delta_pinch, vd.co, test.location);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(delta_pinch, delta_pinch, ss->cache->true_view_normal);
        }

        /* important to calculate based on the grabbed location
         * (intentionally ignore fade here). */
        add_v3_v3(delta_pinch, grab_delta);

        sculpt_project_v3(spvc, delta_pinch, delta_pinch);

        copy_v3_v3(delta_pinch_init, delta_pinch);

        float pinch_fade = pinch * fade;
        /* when reducing, scale reduction back by how close to the center we are,
         * so we don't pinch into nothingness */
        if (pinch > 0.0f) {
          /* square to have even less impact for close vertices */
          pinch_fade *= pow2f(min_ff(1.0f, len_v3(delta_pinch) / ss->cache->radius));
        }
        mul_v3_fl(delta_pinch, 1.0f + pinch_fade);
        sub_v3_v3v3(delta_pinch, delta_pinch_init, delta_pinch);
        add_v3_v3(proxy[vd.i], delta_pinch);
      }

      if (do_rake_rotation) {
        float delta_rotate[3];
        sculpt_rake_rotate(ss, test.location, vd.co, fade, delta_rotate);
        add_v3_v3(proxy[vd.i], delta_rotate);
      }

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const float bstrength = ss->cache->bstrength;
  float grab_delta[3];

  SculptProjectVector spvc;

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (bstrength < 0) {
    negate_v3(grab_delta);
  }

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  /* optionally pinch while painting */
  if (brush->crease_pinch_factor != 0.5f) {
    sculpt_project_v3_cache_init(&spvc, grab_delta);
  }

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.spvc = &spvc;
  data.grab_delta = grab_delta;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_snake_hook_brush_task_cb_ex, &settings);
}

static void do_thumb_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  orig_data.co,
                                                  sqrtf(test.dist),
                                                  orig_data.no,
                                                  NULL,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], cono, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.cono = cono;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_thumb_brush_task_cb_ex, &settings);
}

static void do_rotate_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float angle = data->angle;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      float vec[3], rot[3][3];
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  orig_data.co,
                                                  sqrtf(test.dist),
                                                  orig_data.no,
                                                  NULL,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);

      sub_v3_v3v3(vec, orig_data.co, ss->cache->location);
      axis_angle_normalized_to_mat3(rot, ss->cache->sculpt_normal_symm, angle * fade);
      mul_v3_m3v3(proxy[vd.i], rot, vec);
      add_v3_v3(proxy[vd.i], ss->cache->location);
      sub_v3_v3(proxy[vd.i], orig_data.co);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  static const int flip[8] = {1, -1, -1, 1, -1, 1, 1, -1};
  const float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.angle = angle;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_rotate_brush_task_cb_ex, &settings);
}

static void do_layer_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float *layer_disp;
  const float bstrength = ss->cache->bstrength;
  const float lim = (bstrength < 0) ? -data->brush->height : data->brush->height;
  /* XXX: layer brush needs conversion to proxy but its more complicated */
  /* proxy = BKE_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co; */

  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  /* Why does this have to be thread-protected? */
  BLI_mutex_lock(&data->mutex);
  layer_disp = BKE_pbvh_node_layer_disp_get(ss->pbvh, data->nodes[n]);
  BLI_mutex_unlock(&data->mutex);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);

    if (sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);
      float *disp = &layer_disp[vd.i];
      float val[3];

      *disp += fade;

      /* Don't let the displacement go past the limit */
      if ((lim < 0.0f && *disp < lim) || (lim >= 0.0f && *disp > lim)) {
        *disp = lim;
      }

      mul_v3_v3fl(val, offset, *disp);

      if (!ss->multires && !ss->bm && ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
        int index = vd.vert_indices[vd.i];

        /* persistent base */
        add_v3_v3(val, ss->layer_co[index]);
      }
      else {
        add_v3_v3(val, orig_data.co);
      }

      sculpt_clip(sd, ss, vd.co, val);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];

  mul_v3_v3v3(offset, ss->cache->scale, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.offset = offset;
  BLI_mutex_init(&data.mutex);

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_layer_brush_task_cb_ex, &settings);

  BLI_mutex_end(&data.mutex);
}

static void do_inflate_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = bstrength * tex_strength(ss,
                                                  brush,
                                                  vd.co,
                                                  sqrtf(test.dist),
                                                  vd.no,
                                                  vd.fno,
                                                  vd.mask ? *vd.mask : 0.0f,
                                                  vd.index,
                                                  tls->thread_id);
      float val[3];

      if (vd.fno) {
        copy_v3_v3(val, vd.fno);
      }
      else {
        normal_short_to_float_v3(val, vd.no);
      }

      mul_v3_fl(val, fade * ss->cache->radius);
      mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_inflate_brush_task_cb_ex, &settings);
}

static void calc_sculpt_plane(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (ss->cache->mirror_symmetry_pass == 0 && ss->cache->radial_symmetry_pass == 0 &&
      ss->cache->tile_pass == 0 &&
      (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_PLANE) ||
       !(brush->flag & BRUSH_ORIGINAL_NORMAL))) {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1, 0, 0);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0, 1, 0);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0, 0, 1);
        break;

      case SCULPT_DISP_DIR_AREA:
        calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* for flatten center */
    /* flatten center has not been calculated yet if we are not using the area normal */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      calc_area_center(sd, ob, nodes, totnode, r_area_co);
    }

    /* for area normal */
    if ((!ss->cache->first_time) && (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* for flatten center */
    if ((!ss->cache->first_time) && (brush->flag & BRUSH_ORIGINAL_PLANE)) {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* for area normal */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* for flatten center */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* for area normal */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* for flatten center */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* for area normal */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* for flatten center */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* shift the plane for the current tile */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

static int plane_trim(const StrokeCache *cache, const Brush *brush, const float val[3])
{
  return (!(brush->flag & BRUSH_PLANE_TRIM) ||
          ((dot_v3v3(val, val) <= cache->radius_squared * cache->plane_trim_squared)));
}

static bool plane_point_side_flip(const float co[3], const float plane[4], const bool flip)
{
  float d = plane_point_side_v3(plane, co);
  if (flip) {
    d = -d;
  }
  return d <= 0.0f;
}

static int plane_point_side(const float co[3], const float plane[4])
{
  float d = plane_point_side_v3(plane, co);
  return d <= 0.0f;
}

static float get_offset(Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  float rv = brush->plane_offset;

  if (brush->flag & BRUSH_OFFSET_PRESSURE) {
    rv *= ss->cache->pressure;
  }

  return rv;
}

static void do_flatten_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      float intr[3];
      float val[3];

      closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

      sub_v3_v3v3(val, intr, vd.co);

      if (plane_trim(ss->cache, brush, val)) {
        const float fade = bstrength * tex_strength(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.index,
                                                    tls->thread_id);

        mul_v3_v3fl(proxy[vd.i], val, fade);

        if (vd.mvert) {
          vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];

  float offset = get_offset(sd, ss);
  float displace;
  float temp[3];

  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.area_no = area_no;
  data.area_co = area_co;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_flatten_brush_task_cb_ex, &settings);
}

static void do_clay_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const bool flip = (ss->cache->bstrength < 0);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (plane_point_side_flip(vd.co, test.plane_tool, flip)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (plane_trim(ss->cache, brush, val)) {
          /* note, the normal from the vertices is ignored,
           * causes glitch with planes, see: T44390 */
          const float fade = bstrength * tex_strength(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const bool flip = (ss->cache->bstrength < 0);
  const float radius = flip ? -ss->cache->radius : ss->cache->radius;

  float offset = get_offset(sd, ss);
  float displace;

  float area_no[3];
  float area_co[3];
  float temp[3];

  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = radius * (0.25f + offset);

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* add_v3_v3v3(p, ss->cache->location, area_no); */

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.area_no = area_no;
  data.area_co = area_co;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_clay_brush_task_cb_ex, &settings);
}

static void do_clay_strips_brush_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  SculptBrushTest test;
  float(*proxy)[3];
  const bool flip = (ss->cache->bstrength < 0);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  sculpt_brush_test_init(ss, &test);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_cube(&test, vd.co, mat)) {
      if (plane_point_side_flip(vd.co, test.plane_tool, flip)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (plane_trim(ss->cache, brush, val)) {
          /* note, the normal from the vertices is ignored,
           * causes glitch with planes, see: T44390 */
          const float fade = bstrength * tex_strength(ss,
                                                      brush,
                                                      vd.co,
                                                      ss->cache->radius * test.dist,
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_clay_strips_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const bool flip = (ss->cache->bstrength < 0);
  const float radius = flip ? -ss->cache->radius : ss->cache->radius;
  const float offset = get_offset(sd, ss);
  const float displace = radius * (0.25f + offset);

  float area_no_sp[3]; /* the sculpt-plane normal (whatever its set to) */
  float area_no[3];    /* geometry normal */
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  calc_sculpt_plane(sd, ob, nodes, totnode, area_no_sp, area_co);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* delay the first daub because grab delta is not setup */
  if (ss->cache->first_time) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* init mat */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1;
  normalize_m4(mat);

  /* scale mat */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  invert_m4_m4(mat, tmat);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.area_no_sp = area_no_sp;
  data.area_co = area_co;
  data.mat = mat;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_clay_strips_brush_task_cb_ex, &settings);
}

static void do_fill_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (plane_point_side(vd.co, test.plane_tool)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (plane_trim(ss->cache, brush, val)) {
          const float fade = bstrength * tex_strength(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = get_offset(sd, ss);

  float displace;

  float temp[3];

  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.area_no = area_no;
  data.area_co = area_co;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_fill_brush_task_cb_ex, &settings);
}

static void do_scrape_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      if (!plane_point_side(vd.co, test.plane_tool)) {
        float intr[3];
        float val[3];

        closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

        sub_v3_v3v3(val, intr, vd.co);

        if (plane_trim(ss->cache, brush, val)) {
          const float fade = bstrength * tex_strength(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.index,
                                                      tls->thread_id);

          mul_v3_v3fl(proxy[vd.i], val, fade);

          if (vd.mvert) {
            vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = get_offset(sd, ss);

  float displace;

  float temp[3];

  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  displace = -radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.area_no = area_no;
  data.area_co = area_co;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_scrape_brush_task_cb_ex, &settings);
}

static void do_gravity_task_cb_ex(void *__restrict userdata,
                                  const int n,
                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = sculpt_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    if (sculpt_brush_test_sq_fn(&test, vd.co)) {
      const float fade = tex_strength(ss,
                                      brush,
                                      vd.co,
                                      sqrtf(test.dist),
                                      vd.no,
                                      vd.fno,
                                      vd.mask ? *vd.mask : 0.0f,
                                      vd.index,
                                      tls->thread_id);

      mul_v3_v3fl(proxy[vd.i], offset, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_gravity(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float bstrength)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float offset[3] /*, area_no[3]*/;
  float gravity_vector[3];

  mul_v3_v3fl(gravity_vector, ss->cache->gravity_direction, -ss->cache->radius_squared);

  /* offset with as much as possible factored in already */
  mul_v3_v3v3(offset, gravity_vector, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* threaded loop over nodes */
  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.brush = brush;
  data.nodes = nodes;
  data.offset = offset;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_gravity_task_cb_ex, &settings);
}

static void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3])
{
  Mesh *me = (Mesh *)ob->data;
  float(*ofs)[3] = NULL;
  int a;
  const int kb_act_idx = ob->shapenr - 1;
  KeyBlock *currkey;

  /* for relative keys editing of base should update other keys */
  if (BKE_keyblock_is_basis(me->key, kb_act_idx)) {
    ofs = BKE_keyblock_convert_to_vertcos(ob, kb);

    /* calculate key coord offsets (from previous location) */
    for (a = 0; a < me->totvert; a++) {
      sub_v3_v3v3(ofs[a], vertCos[a], ofs[a]);
    }

    /* apply offsets on other keys */
    for (currkey = (KeyBlock*)me->key->block.first; currkey; currkey = currkey->next) {
      if ((currkey != kb) && (currkey->relative == kb_act_idx)) {
        BKE_keyblock_update_from_offset(ob, currkey, ofs);
      }
    }

    MEM_freeN(ofs);
  }

  /* modifying of basis key should update mesh */
  if (kb == me->key->refkey) {
    MVert *mvert = me->mvert;

    for (a = 0; a < me->totvert; a++, mvert++) {
      copy_v3_v3(mvert->co, vertCos[a]);
    }

    BKE_mesh_calc_normals(me);
  }

  /* apply new coords on active key block, no need to re-allocate kb->data here! */
  BKE_keyblock_update_from_vertcos(ob, kb, vertCos);
}

/* Note: we do the topology update before any brush actions to avoid
 * issues with the proxies. The size of the proxy can't change, so
 * topology must be updated first. */
static void sculpt_topology_update(Sculpt *sd,
                                   Object *ob,
                                   Brush *brush,
                                   UnifiedPaintSettings *UNUSED(ups))
{
  SculptSession *ss = ob->sculpt;

  int n, totnode;
  /* Build a list of all nodes that are potentially within the brush's area of influence */
  const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                             ss->cache->original;
  const float radius_scale = 1.25f;
  PBVHNode **nodes = sculpt_pbvh_gather_generic(
      ob, sd, brush, use_original, radius_scale, &totnode);

  /* Only act if some verts are inside the brush area */
  if (totnode) {
    PBVHTopologyUpdateMode mode = (PBVHTopologyUpdateMode)0;
    float location[3];

    if (!(sd->flags & SCULPT_DYNTOPO_DETAIL_MANUAL)) {
      if (sd->flags & SCULPT_DYNTOPO_SUBDIVIDE) {
        mode = (PBVHTopologyUpdateMode) (mode | PBVH_Subdivide);
      }

      if ((sd->flags & SCULPT_DYNTOPO_COLLAPSE) || (brush->sculpt_tool == SCULPT_TOOL_SIMPLIFY)) {
        mode = (PBVHTopologyUpdateMode) (mode | PBVH_Collapse);
      }
    }

    for (n = 0; n < totnode; n++) {
      sculpt_undo_push_node(ob,
                            nodes[n],
                            brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                     SCULPT_UNDO_COORDS);
      BKE_pbvh_node_mark_update(nodes[n]);

      if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
        BKE_pbvh_node_mark_topology_update(nodes[n]);
        BKE_pbvh_bmesh_node_save_orig(ss->bm, nodes[n]);
      }
    }

    if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
      BKE_pbvh_bmesh_update_topology(ss->pbvh,
                                     mode,
                                     ss->cache->location,
                                     ss->cache->view_normal,
                                     ss->cache->radius,
                                     (brush->flag & BRUSH_FRONTFACE) != 0,
                                     (brush->falloff_shape != PAINT_FALLOFF_SHAPE_SPHERE));
    }

    MEM_SAFE_FREE(nodes);

    /* update average stroke position */
    copy_v3_v3(location, ss->cache->true_location);
    mul_m4_v3(ob->obmat, location);
  }
}

static void do_brush_action_task_cb(void *__restrict userdata,
                                    const int n,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;

  sculpt_undo_push_node(data->ob,
                        data->nodes[n],
                        data->brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                       SCULPT_UNDO_COORDS);
  if (data->brush->sculpt_tool == SCULPT_TOOL_MASK) {
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
  else {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void do_brush_action(Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups)
{
  SculptSession *ss = ob->sculpt;
  int totnode;
  PBVHNode **nodes;

  /* Build a list of all nodes that are potentially within the brush's area of influence */

  /* These brushes need to update all nodes as they are not constrained by the brush radius */
  if (brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) {
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  }
  else if (brush->sculpt_tool == SCULPT_TOOL_POSE) {
    float final_radius = ss->cache->radius * (1 + brush->pose_offset);
    SculptSearchSphereData data;
    data.ss = ss;
    data.sd = sd;
    data.radius_squared = final_radius * final_radius;
    data.original = true;
    BKE_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);
  }
  else {
    const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                               ss->cache->original;
    float radius_scale = 1.0f;
    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations */
    if (brush->sculpt_tool == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = 2.0f;
    }
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale, &totnode);
  }

  /* Only act if some verts are inside the brush area */
  if (totnode) {
    float location[3];

    SculptThreadedTaskData task_data;
    task_data.sd = sd;
    task_data.ob = ob;
    task_data.brush = brush;
    task_data.nodes = nodes;

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &task_data, do_brush_action_task_cb, &settings);

    if (sculpt_brush_needs_normal(ss, brush)) {
      update_sculpt_normal(sd, ob, nodes, totnode);
    }

    if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA) {
      update_brush_local_mat(sd, ob);
    }

    if (ss->cache->first_time && ss->cache->mirror_symmetry_pass == 0) {
      if (sculpt_automasking_enabled(ss, brush)) {
        sculpt_automasking_init(sd, ob);
      }
    }

    if (brush->sculpt_tool == SCULPT_TOOL_POSE && ss->cache->first_time &&
        ss->cache->mirror_symmetry_pass == 0) {
      sculpt_pose_brush_init(sd, ob, ss, brush, ss->cache->location, ss->cache->radius);
    }

    /* Apply one type of brush action */
    switch (brush->sculpt_tool) {
      case SCULPT_TOOL_DRAW:
        do_draw_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SMOOTH:
        do_smooth_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CREASE:
        do_crease_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_BLOB:
        do_crease_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_PINCH:
        do_pinch_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_INFLATE:
        do_inflate_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_GRAB:
        do_grab_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ROTATE:
        do_rotate_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SNAKE_HOOK:
        do_snake_hook_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_NUDGE:
        do_nudge_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_THUMB:
        do_thumb_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_LAYER:
        do_layer_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FLATTEN:
        do_flatten_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY:
        do_clay_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY_STRIPS:
        do_clay_strips_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FILL:
        do_fill_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SCRAPE:
        do_scrape_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_MASK:
        do_mask_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_POSE:
        do_pose_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DRAW_SHARP:
        do_draw_sharp_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ELASTIC_DEFORM:
        do_elastic_deform_brush(sd, ob, nodes, totnode);
        break;
    }

    if (!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
        brush->autosmooth_factor > 0) {
      if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
        smooth(
            sd, ob, nodes, totnode, brush->autosmooth_factor * (1 - ss->cache->pressure), false);
      }
      else {
        smooth(sd, ob, nodes, totnode, brush->autosmooth_factor, false);
      }
    }

    if (sculpt_brush_use_topology_rake(ss, brush)) {
      bmesh_topology_rake(sd, ob, nodes, totnode, brush->topology_rake_factor);
    }

    if (ss->cache->supports_gravity) {
      do_gravity(sd, ob, nodes, totnode, sd->gravity_factor);
    }

    MEM_SAFE_FREE(nodes);

    /* update average stroke position */
    copy_v3_v3(location, ss->cache->true_location);
    mul_m4_v3(ob->obmat, location);

    add_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter++;
    /* update last stroke position */
    ups->last_stroke_valid = true;
  }
}

/* flush displacement from deformed PBVH vertex to original mesh */
static void sculpt_flush_pbvhvert_deform(Object *ob, PBVHVertexIter *vd)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = (Mesh*)ob->data;
  float disp[3], newco[3];
  int index = vd->vert_indices[vd->i];

  sub_v3_v3v3(disp, vd->co, ss->deform_cos[index]);
  mul_m3_v3(ss->deform_imats[index], disp);
  add_v3_v3v3(newco, disp, ss->orig_cos[index]);

  copy_v3_v3(ss->deform_cos[index], vd->co);
  copy_v3_v3(ss->orig_cos[index], newco);

  if (!ss->shapekey_active) {
    copy_v3_v3(me->mvert[index].co, newco);
  }
}

static void sculpt_combine_proxies_task_cb(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  Object *ob = data->ob;

  /* these brushes start from original coordinates */
  const bool use_orco = ELEM(data->brush->sculpt_tool,
                             SCULPT_TOOL_GRAB,
                             SCULPT_TOOL_ROTATE,
                             SCULPT_TOOL_THUMB,
                             SCULPT_TOOL_ELASTIC_DEFORM,
                             SCULPT_TOOL_POSE);

  PBVHVertexIter vd;
  PBVHProxyNode *proxies;
  int proxy_count;
  float(*orco)[3] = NULL;

  if (use_orco && !ss->bm) {
    orco = sculpt_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS)->co;
  }

  BKE_pbvh_node_get_proxies(data->nodes[n], &proxies, &proxy_count);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    float val[3];
    int p;

    if (use_orco) {
      if (ss->bm) {
        copy_v3_v3(val, BM_log_original_vert_co(ss->bm_log, vd.bm_vert));
      }
      else {
        copy_v3_v3(val, orco[vd.i]);
      }
    }
    else {
      copy_v3_v3(val, vd.co);
    }

    for (p = 0; p < proxy_count; p++) {
      add_v3_v3(val, proxies[p].co[vd.i]);
    }

    sculpt_clip(sd, ss, vd.co, val);

    if (ss->deform_modifiers_active) {
      sculpt_flush_pbvhvert_deform(ob, &vd);
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_free_proxies(data->nodes[n]);
}

static void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);

  /* first line is tools that don't support proxies */
  if (ss->cache->supports_gravity || (sculpt_tool_is_proxy_used(brush->sculpt_tool) == false)) {
    SculptThreadedTaskData data;
    data.sd = sd;
    data.ob = ob;
    data.brush = brush;
    data.nodes = nodes;

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, sculpt_combine_proxies_task_cb, &settings);
  }

  MEM_SAFE_FREE(nodes);
}

/* copy the modified vertices from bvh to the active key */
static void sculpt_update_keyblock(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  float(*vertCos)[3];

  /* Keyblock update happens after handling deformation caused by modifiers,
   * so ss->orig_cos would be updated with new stroke */
  if (ss->orig_cos) {
    vertCos = ss->orig_cos;
  }
  else {
    vertCos = BKE_pbvh_vert_coords_alloc(ss->pbvh);
  }

  if (vertCos) {
    sculpt_vertcos_to_key(ob, ss->shapekey_active, vertCos);

    if (vertCos != ss->orig_cos) {
      MEM_freeN(vertCos);
    }
  }
}

static void sculpt_flush_stroke_deform_task_cb(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  Object *ob = data->ob;
  float(*vertCos)[3] = data->vertCos;

  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    sculpt_flush_pbvhvert_deform(ob, &vd);

    if (vertCos) {
      int index = vd.vert_indices[vd.i];
      copy_v3_v3(vertCos[index], ss->orig_cos[index]);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

/* flush displacement from deformed PBVH to original layer */
static void sculpt_flush_stroke_deform(Sculpt *sd, Object *ob, bool is_proxy_used)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (is_proxy_used) {
    /* this brushes aren't using proxies, so sculpt_combine_proxies() wouldn't
     * propagate needed deformation to original base */

    int totnode;
    Mesh *me = (Mesh *)ob->data;
    PBVHNode **nodes;
    float(*vertCos)[3] = NULL;

    if (ss->shapekey_active) {
      vertCos = (float (*)[3])MEM_mallocN(sizeof(*vertCos) * me->totvert, "flushStrokeDeofrm keyVerts");

      /* mesh could have isolated verts which wouldn't be in BVH,
       * to deal with this we copy old coordinates over new ones
       * and then update coordinates for all vertices from BVH
       */
      memcpy(vertCos, ss->orig_cos, sizeof(*vertCos) * me->totvert);
    }

    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    SculptThreadedTaskData data;
    data.sd = sd;
    data.ob = ob;
    data.brush = brush;
    data.nodes = nodes;
    data.vertCos = vertCos;

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, sculpt_flush_stroke_deform_task_cb, &settings);

    if (vertCos) {
      sculpt_vertcos_to_key(ob, ss->shapekey_active, vertCos);
      MEM_freeN(vertCos);
    }

    MEM_SAFE_FREE(nodes);

    /* Modifiers could depend on mesh normals, so we should update them/
     * Note, then if sculpting happens on locked key, normals should be re-calculated
     * after applying coords from keyblock on base mesh */
    BKE_mesh_calc_normals(me);
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
 * calculate multiple modifications to the mesh when symmetry is enabled. */
static void sculpt_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const char symm,
                                      const char axis,
                                      const float angle)
{
  flip_v3_v3(cache->location, cache->true_location, symm);
  flip_v3_v3(cache->last_location, cache->true_last_location, symm);
  flip_v3_v3(cache->grab_delta_symmetry, cache->grab_delta, symm);
  flip_v3_v3(cache->view_normal, cache->true_view_normal, symm);

  /* XXX This reduces the length of the grab delta if it approaches the line of symmetry
   * XXX However, a different approach appears to be needed */
#if 0
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float frac = 1.0f / max_overlap_count(sd);
    float reduce = (feather - frac) / (1 - frac);

    printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

    if (frac < 1) {
      mul_v3_fl(cache->grab_delta_symmetry, reduce);
    }
  }
#endif

  unit_m4(cache->symm_rot_mat);
  unit_m4(cache->symm_rot_mat_inv);
  zero_v3(cache->plane_offset);

  if (axis) { /* expects XYZ */
    rotate_m4(cache->symm_rot_mat, axis, angle);
    rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
  }

  mul_m4_v3(cache->symm_rot_mat, cache->location);
  mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);

  if (cache->supports_gravity) {
    flip_v3_v3(cache->gravity_direction, cache->true_gravity_direction, symm);
    mul_m4_v3(cache->symm_rot_mat, cache->gravity_direction);
  }

  if (cache->is_rake_rotation_valid) {
    flip_qt_qt(cache->rake_rotation_symmetry, cache->rake_rotation, symm);
  }
}

typedef void (*BrushActionFunc)(Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups);

static void do_tiled(
    Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups, BrushActionFunc action)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float radius = cache->radius;
  BoundBox *bb = BKE_object_boundbox_get(ob);
  const float *bbMin = bb->vec[0];
  const float *bbMax = bb->vec[6];
  const float *step = sd->paint.tile_offset;
  int dim;

  /* These are integer locations, for real location: multiply with step and add orgLoc.
   * So 0,0,0 is at orgLoc. */
  int start[3];
  int end[3];
  int cur[3];

  float orgLoc[3]; /* position of the "prototype" stroke for tiling */
  copy_v3_v3(orgLoc, cache->location);

  for (dim = 0; dim < 3; dim++) {
    if ((sd->paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }

  /* first do the "untiled" position to initialize the stroke for this location */
  cache->tile_pass = 0;
  action(sd, ob, brush, ups);

  /* now do it for all the tiles */
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          continue; /* skip tile at orgLoc, this was already handled before all others */
        }

        ++cache->tile_pass;

        for (dim = 0; dim < 3; dim++) {
          cache->location[dim] = cur[dim] * step[dim] + orgLoc[dim];
          cache->plane_offset[dim] = cur[dim] * step[dim];
        }
        action(sd, ob, brush, ups);
      }
    }
  }
}

static void do_radial_symmetry(Sculpt *sd,
                               Object *ob,
                               Brush *brush,
                               UnifiedPaintSettings *ups,
                               BrushActionFunc action,
                               const char symm,
                               const int axis,
                               const float UNUSED(feather))
{
  SculptSession *ss = ob->sculpt;
  int i;

  for (i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2 * M_PI * i / sd->radial_symm[axis - 'X'];
    ss->cache->radial_symmetry_pass = i;
    sculpt_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);
    do_tiled(sd, ob, brush, ups, action);
  }
}

/* noise texture gives different values for the same input coord; this
 * can tear a multires mesh during sculpting so do a stitch in this
 * case */
static void sculpt_fix_noise_tear(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  if (ss->multires && mtex->tex && mtex->tex->type == TEX_NOISE) {
    multires_stitch_grids(ob);
  }
}

static void do_symmetrical_brush_actions(Sculpt *sd,
                                         Object *ob,
                                         BrushActionFunc action,
                                         UnifiedPaintSettings *ups)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  int i;

  float feather = calc_symmetry_feather(sd, ss->cache);

  cache->bstrength = brush_strength(sd, cache, feather, ups);
  cache->symmetry = symm;

  /* symm is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (i = 0; i <= symm; i++) {
    if (i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)))) {
      cache->mirror_symmetry_pass = i;
      cache->radial_symmetry_pass = 0;

      sculpt_cache_calc_brushdata_symm(cache, i, 0, 0);
      do_tiled(sd, ob, brush, ups, action);

      do_radial_symmetry(sd, ob, brush, ups, action, i, 'X', feather);
      do_radial_symmetry(sd, ob, brush, ups, action, i, 'Y', feather);
      do_radial_symmetry(sd, ob, brush, ups, action, i, 'Z', feather);
    }
  }
}

static void sculpt_update_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int radius = BKE_brush_size_get(scene, brush);

  if (ss->texcache) {
    MEM_freeN(ss->texcache);
    ss->texcache = NULL;
  }

  if (ss->tex_pool) {
    BKE_image_pool_free(ss->tex_pool);
    ss->tex_pool = NULL;
  }

  /* Need to allocate a bigger buffer for bigger brush size */
  ss->texcache_side = 2 * radius;
  if (!ss->texcache || ss->texcache_side > ss->texcache_actual) {
    ss->texcache = BKE_brush_gen_texture_cache(brush, radius, false);
    ss->texcache_actual = ss->texcache_side;
    ss->tex_pool = BKE_image_pool_new();
  }
}

static bool sculpt_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT;
}

static bool sculpt_mode_poll_view3d(bContext *C)
{
  return (sculpt_mode_poll(C) && CTX_wm_region_view3d(C));
}

static bool sculpt_poll(bContext *C)
{
  return sculpt_mode_poll(C) && paint_poll(C);
}

static bool sculpt_poll_view3d(bContext *C)
{
  return (sculpt_poll(C) && CTX_wm_region_view3d(C));
}


static const char *sculpt_tool_name(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((eBrushSculptTool)brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
      return "Draw Brush";
    case SCULPT_TOOL_SMOOTH:
      return "Smooth Brush";
    case SCULPT_TOOL_CREASE:
      return "Crease Brush";
    case SCULPT_TOOL_BLOB:
      return "Blob Brush";
    case SCULPT_TOOL_PINCH:
      return "Pinch Brush";
    case SCULPT_TOOL_INFLATE:
      return "Inflate Brush";
    case SCULPT_TOOL_GRAB:
      return "Grab Brush";
    case SCULPT_TOOL_NUDGE:
      return "Nudge Brush";
    case SCULPT_TOOL_THUMB:
      return "Thumb Brush";
    case SCULPT_TOOL_LAYER:
      return "Layer Brush";
    case SCULPT_TOOL_FLATTEN:
      return "Flatten Brush";
    case SCULPT_TOOL_CLAY:
      return "Clay Brush";
    case SCULPT_TOOL_CLAY_STRIPS:
      return "Clay Strips Brush";
    case SCULPT_TOOL_FILL:
      return "Fill Brush";
    case SCULPT_TOOL_SCRAPE:
      return "Scrape Brush";
    case SCULPT_TOOL_SNAKE_HOOK:
      return "Snake Hook Brush";
    case SCULPT_TOOL_ROTATE:
      return "Rotate Brush";
    case SCULPT_TOOL_MASK:
      return "Mask Brush";
    case SCULPT_TOOL_SIMPLIFY:
      return "Simplify Brush";
    case SCULPT_TOOL_DRAW_SHARP:
      return "Draw Sharp Brush";
    case SCULPT_TOOL_ELASTIC_DEFORM:
      return "Elastic Deform Brush";
    case SCULPT_TOOL_POSE:
      return "Pose Brush";
  }

  return "Sculpting";
}

/**
 * Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

static void sculpt_cache_free(StrokeCache *cache)
{
  if (cache->dial) {
    MEM_freeN(cache->dial);
  }
  if (cache->pose_factor) {
    MEM_freeN(cache->pose_factor);
  }
  MEM_freeN(cache);
}

/* Initialize mirror modifier clipping */
static void sculpt_init_mirror_clipping(Object *ob, SculptSession *ss)
{
  ModifierData *md;
  int i;

  for (md = (ModifierData *)ob->modifiers.first; md; md = md->next) {
    if (md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
      MirrorModifierData *mmd = (MirrorModifierData *)md;

      if (mmd->flag & MOD_MIR_CLIPPING) {
        /* check each axis for mirroring */
        for (i = 0; i < 3; i++) {
          if (mmd->flag & (MOD_MIR_AXIS_X << i)) {
            /* enable sculpt clipping */
            ss->cache->flag |= CLIP_X << i;

            /* update the clip tolerance */
            if (mmd->tolerance > ss->cache->clip_tolerance[i]) {
              ss->cache->clip_tolerance[i] = mmd->tolerance;
            }
          }
        }
      }
    }
  }
}

/* Initialize the stroke cache invariants from operator properties */
static void sculpt_update_cache_invariants(
    bContext *C, Sculpt *sd, SculptSession *ss, wmOperator *op, const float mouse[2])
{
  StrokeCache *cache = (StrokeCache *)MEM_callocN(sizeof(StrokeCache), "stroke cache");
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd->paint);
  //ViewContext *vc = (ViewContext *)paint_stroke_view_context((PaintStroke *)op->customdata);
  ViewContext *vc = paint_stroke_view_context((PaintStroke*)sculpt_dummy_op.customdata);
  Object *ob = CTX_data_active_object(C);
  float mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  float max_scale;
  int i;
  int mode;

  ss->cache = cache;

  /* Set scaling adjustment */
  if (brush->sculpt_tool == SCULPT_TOOL_LAYER) {
    max_scale = 1.0f;
  }
  else {
    max_scale = 0.0f;
    for (i = 0; i < 3; i++) {
      max_scale = max_ff(max_scale, fabsf(ob->scale[i]));
    }
  }
  cache->scale[0] = max_scale / ob->scale[0];
  cache->scale[1] = max_scale / ob->scale[1];
  cache->scale[2] = max_scale / ob->scale[2];

  cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

  cache->flag = 0;

  sculpt_init_mirror_clipping(ob, ss);

  /* Initial mouse location */
  if (mouse) {
    copy_v2_v2(cache->initial_mouse, mouse);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = Widget_Sculpt::mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = Widget_Sculpt::mode == BRUSH_STROKE_SMOOTH;
  cache->normal_weight = brush->normal_weight;

  /* interpret invert as following normal, for grab brushes */
  if (SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool)) {
    if (cache->invert) {
      cache->invert = false;
      cache->normal_weight = (cache->normal_weight == 0.0f);
    }
  }

  /* not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey) */
  if (cache->invert) {
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  /* Alt-Smooth */
  if (cache->alt_smooth) {
    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      cache->saved_mask_brush_tool = brush->mask_tool;
      brush->mask_tool = BRUSH_MASK_SMOOTH;
    }
    else {
      Paint *p = &sd->paint;
      Brush *br;
      int size = BKE_brush_size_get(scene, brush);

      BLI_strncpy(cache->saved_active_brush_name,
                  brush->id.name + 2,
                  sizeof(cache->saved_active_brush_name));

      br = (Brush *)BKE_libblock_find_name(bmain, ID_BR, "Smooth");
      if (br) {
        BKE_paint_brush_set(p, br);
        brush = br;
        /* TODO_XR */
        // BKE_brush_size_set(scene, brush, Widget_Sculpt::sculpt_radius);
        cache->saved_smooth_size = BKE_brush_size_get(scene, brush);
        BKE_brush_size_set(scene, brush, size);
        BKE_curvemapping_initialize(brush->curve);
      }
    }
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  copy_v2_v2(ups->tex_mouse, cache->initial_mouse);

  /* Truly temporary data that isn't stored in properties */

  cache->vc = vc;

  cache->brush = brush;

  /* cache projection matrix */
  ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

  invert_m4_m4(ob->imat, ob->obmat);
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(cache->true_view_normal, viewDir);

  cache->supports_gravity =
      (!ELEM(brush->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_SIMPLIFY) &&
       (sd->gravity_factor > 0.0f));
  /* get gravity vector in world space */
  if (cache->supports_gravity) {
    if (sd->gravity_object) {
      Object *gravity_object = sd->gravity_object;

      copy_v3_v3(cache->true_gravity_direction, gravity_object->obmat[2]);
    }
    else {
      cache->true_gravity_direction[0] = cache->true_gravity_direction[1] = 0.0;
      cache->true_gravity_direction[2] = 1.0;
    }

    /* transform to sculpted object space */
    mul_m3_v3(mat, cache->true_gravity_direction);
    normalize_v3(cache->true_gravity_direction);
  }

  /* Initialize layer brush displacements and persistent coords */
  if (brush->sculpt_tool == SCULPT_TOOL_LAYER) {
    /* not supported yet for multires or dynamic topology */
    if (!ss->multires && !ss->bm && !ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
      if (!ss->layer_co) {
        ss->layer_co = (float (*)[3])MEM_mallocN(sizeof(float) * 3 * ss->totvert, "sculpt mesh vertices copy");
      }

      if (ss->deform_cos) {
        memcpy(ss->layer_co, ss->deform_cos, ss->totvert);
      }
      else {
        for (i = 0; i < ss->totvert; i++) {
          copy_v3_v3(ss->layer_co[i], ss->mvert[i].co);
        }
      }
    }

    if (ss->bm) {
      /* Free any remaining layer displacements from nodes. If not and topology changes
       * from using another tool, then next layer toolstroke
       * can access past disp array bounds */
      BKE_pbvh_free_layer_disp(ss->pbvh);
    }
  }

  /* Make copies of the mesh vertex locations and normals for some tools */
  if (brush->flag & BRUSH_ANCHORED) {
    cache->original = true;
  }

  /* Draw sharp does not need the original coordinates to produce the accumulate effect, so it
   * should work the opposite way. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
    cache->original = true;
  }

  if (SCULPT_TOOL_HAS_ACCUMULATE(brush->sculpt_tool)) {
    if (!(brush->flag & BRUSH_ACCUMULATE)) {
      cache->original = true;
      if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
        cache->original = false;
      }
    }
  }

  cache->first_time = 1;

#define PIXEL_INPUT_THRESHHOLD 5
  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->dial = BLI_dial_initialize(cache->initial_mouse, PIXEL_INPUT_THRESHHOLD);
  }

#undef PIXEL_INPUT_THRESHHOLD
}

static void sculpt_update_brush_delta(UnifiedPaintSettings *ups, Object *ob, Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float mouse[2] = {
      cache->mouse[0],
      cache->mouse[1],
  };
  int tool = brush->sculpt_tool;

  if (ELEM(tool,
           SCULPT_TOOL_GRAB,
           SCULPT_TOOL_ELASTIC_DEFORM,
           SCULPT_TOOL_NUDGE,
           SCULPT_TOOL_CLAY_STRIPS,
           SCULPT_TOOL_SNAKE_HOOK,
           SCULPT_TOOL_POSE,
           SCULPT_TOOL_THUMB) ||
      sculpt_brush_use_topology_rake(ss, brush)) {
    float grab_location[3], imat[4][4], delta[3], loc[3];

    if (cache->first_time) {
      if (tool == SCULPT_TOOL_GRAB && brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
        copy_v3_v3(cache->orig_grab_location, sculpt_active_vertex_co_get(ss));
      }
      else {
        copy_v3_v3(cache->orig_grab_location, cache->true_location);
      }
    }
    else if (tool == SCULPT_TOOL_SNAKE_HOOK) {
      add_v3_v3(cache->true_location, cache->grab_delta);
    }

    if (Widget_Sculpt::raycast) {
        /* compute 3d coordinate at same z from original location + mouse */
        mul_v3_m4v3(loc, ob->obmat, cache->orig_grab_location);
        ED_view3d_win_to_3d(cache->vc->v3d, cache->vc->ar, loc, mouse, grab_location);
    } else {
      float obimat[4][4];
      invert_m4_m4(obimat, ob->obmat);
      mul_m4_v3(obimat, Widget_Sculpt::location);
      copy_v3_v3(grab_location, Widget_Sculpt::location);
    }

    /* compute delta to move verts by */
    if (!cache->first_time) {
      switch (tool) {
        case SCULPT_TOOL_GRAB:
        case SCULPT_TOOL_POSE:
        case SCULPT_TOOL_THUMB:
        case SCULPT_TOOL_ELASTIC_DEFORM:
          sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
          invert_m4_m4(imat, ob->obmat);
          mul_mat3_m4_v3(imat, delta);
          add_v3_v3(cache->grab_delta, delta);
          break;
        case SCULPT_TOOL_CLAY_STRIPS:
        case SCULPT_TOOL_NUDGE:
        case SCULPT_TOOL_SNAKE_HOOK:
          if (brush->flag & BRUSH_ANCHORED) {
            float orig[3];
            mul_v3_m4v3(orig, ob->obmat, cache->orig_grab_location);
            sub_v3_v3v3(cache->grab_delta, grab_location, orig);
          }
          else {
            sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
          }
          invert_m4_m4(imat, ob->obmat);
          mul_mat3_m4_v3(imat, cache->grab_delta);
          break;
        default:
          /* Use for 'Brush.topology_rake_factor'. */
          sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
          break;
      }
    }
    else {
      zero_v3(cache->grab_delta);
    }

    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache->grab_delta, cache->grab_delta, ss->cache->true_view_normal);
    }

    copy_v3_v3(cache->old_grab_location, grab_location);

    if (tool == SCULPT_TOOL_GRAB) {
      if (brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
        copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
      }
      else {
        copy_v3_v3(cache->anchored_location, cache->true_location);
      }
    }
    else if (tool == SCULPT_TOOL_ELASTIC_DEFORM) {
      copy_v3_v3(cache->anchored_location, cache->true_location);
    }
    else if (tool == SCULPT_TOOL_THUMB) {
      copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
    }

    if (ELEM(tool,
             SCULPT_TOOL_GRAB,
             SCULPT_TOOL_THUMB,
             SCULPT_TOOL_ELASTIC_DEFORM,
             SCULPT_TOOL_POSE)) {
      /* location stays the same for finding vertices in brush radius */
      copy_v3_v3(cache->true_location, cache->orig_grab_location);

      ups->draw_anchored = true;
      copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
      ups->anchored_size = ups->pixel_radius;
    }

    /* handle 'rake' */
    cache->is_rake_rotation_valid = false;

    if (cache->first_time) {
      copy_v3_v3(cache->rake_data.follow_co, grab_location);
    }

    if (sculpt_brush_needs_rake_rotation(brush)) {
      cache->rake_data.follow_dist = cache->radius * SCULPT_RAKE_BRUSH_FACTOR;

      if (!is_zero_v3(cache->grab_delta)) {
        const float eps = 0.00001f;

        float v1[3], v2[3];

        copy_v3_v3(v1, cache->rake_data.follow_co);
        copy_v3_v3(v2, cache->rake_data.follow_co);
        sub_v3_v3(v2, cache->grab_delta);

        sub_v3_v3(v1, grab_location);
        sub_v3_v3(v2, grab_location);

        if ((normalize_v3(v2) > eps) && (normalize_v3(v1) > eps) &&
            (len_squared_v3v3(v1, v2) > eps)) {
          const float rake_dist_sq = len_squared_v3v3(cache->rake_data.follow_co, grab_location);
          const float rake_fade = (rake_dist_sq > SQUARE(cache->rake_data.follow_dist)) ?
                                      1.0f :
                                      sqrtf(rake_dist_sq) / cache->rake_data.follow_dist;

          float axis[3], angle;
          float tquat[4];

          rotation_between_vecs_to_quat(tquat, v1, v2);

          /* use axis-angle to scale rotation since the factor may be above 1 */
          quat_to_axis_angle(axis, &angle, tquat);
          normalize_v3(axis);

          angle *= brush->rake_factor * rake_fade;
          axis_angle_normalized_to_quat(cache->rake_rotation, axis, angle);
          cache->is_rake_rotation_valid = true;
        }
      }
      sculpt_rake_data_update(&cache->rake_data, grab_location);
    }
  }
}


/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, Object *ob, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Get the 3d position and 2d-projected position of the VR cursor. */
  memcpy(Widget_Sculpt::location,
         VR_UI::cursor_position_get(VR_SPACE_BLENDER, Widget_Sculpt::cursor_side).m[3],
         sizeof(float) * 3);
  if (Widget_Sculpt::raycast) {
    ARegion *ar = CTX_wm_region(C);
    RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
    float projmat[4][4];
    mul_m4_m4m4(projmat, (float(*)[4])rv3d->winmat, (float(*)[4])rv3d->viewmat);
    mul_project_m4_v3(projmat, Widget_Sculpt::location);
    Widget_Sculpt::mouse[0] = (int)((ar->winx / 2.0f) +
                                    (ar->winx / 2.0f) * Widget_Sculpt::location[0]);
    Widget_Sculpt::mouse[1] = (int)((ar->winy / 2.0f) +
                                    (ar->winy / 2.0f) * Widget_Sculpt::location[1]);
  }

  Widget_Sculpt::pressure = vr_get_obj()->controller[Widget_Sculpt::cursor_side]->trigger_pressure;

  /* RNA_float_get_array(ptr, "location", cache->traced_location); */

  if (cache->first_time ||
      !((brush->flag & BRUSH_ANCHORED) || (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) ||
        (brush->sculpt_tool == SCULPT_TOOL_ROTATE))) {
    if (Widget_Sculpt::raycast) {
    	memcpy(cache->true_location, Widget_Sculpt::location, sizeof(float) * 3); //RNA_float_get_array(ptr, "location", cache->true_location);
    }
    else {
		float obimat[4][4];
		invert_m4_m4(obimat, ob->obmat);
		mul_m4_v3(obimat, Widget_Sculpt::location);
		copy_v3_v3(cache->true_location, Widget_Sculpt::location);
    }
  }

  cache->pen_flip = Widget_Sculpt::pen_flip;  // RNA_boolean_get(ptr, "pen_flip");
  memcpy(cache->mouse, Widget_Sculpt::mouse, sizeof(float) * 2);
  // RNA_float_get_array(ptr, "mouse", cache->mouse);

  /* XXX: Use pressure value from first brush step for brushes which don't
   *      support strokes (grab, thumb). They depends on initial state and
   *      brush coord/pressure/etc.
   *      It's more an events design issue, which doesn't split coordinate/pressure/angle
   *      changing events. We should avoid this after events system re-design */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    if (Widget_Sculpt::use_trigger_pressure) {
      cache->pressure = Widget_Sculpt::pressure;  // RNA_float_get(ptr, "pressure");
    } else {
      cache->pressure = Widget_Sculpt::sculpt_strength;
    }
  }

  /* Truly temporary data that isn't stored in properties */
  // if (cache->first_time) {
  //	if (!BKE_brush_use_locked_size(scene, brush)) {
  //		cache->initial_radius = paint_calc_object_space_radius(cache->vc,
  //			cache->true_location,
  //			BKE_brush_size_get(scene, brush));
  //		BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius);
  //	}
  //	else {
  //		cache->initial_radius = BKE_brush_unprojected_radius_get(scene, brush);
  //	}
  //}

  //if (BKE_brush_use_size_pressure(scene, brush) &&
  //    paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT)) {
  //  cache->radius = cache->initial_radius * cache->pressure;
  //}
  //else {
  //  cache->radius = cache->initial_radius;
  //}

  /* TODO_XR: Test with different display scaling. (see
   * Widget_Transform::raycast_select_manipulator()) */
  cache->radius = Widget_Sculpt::sculpt_radius * VR_UI::navigation_scale_get();

  cache->radius_squared = cache->radius * cache->radius;

  if (brush->flag & BRUSH_ANCHORED) {
    /* true location has been calculated as part of the stroke system already here */
    if (brush->flag & BRUSH_EDGE_TO_EDGE) {
		if (Widget_Sculpt::raycast) {
			memcpy(cache->true_location, Widget_Sculpt::location, sizeof(float) * 3); //RNA_float_get_array(ptr, "location", cache->true_location);
		}
		else {
			float obimat[4][4];
			invert_m4_m4(obimat, ob->obmat);
			mul_m4_v3(obimat, Widget_Sculpt::location);
			copy_v3_v3(cache->true_location, Widget_Sculpt::location);
		}
    }

    cache->radius = paint_calc_object_space_radius(
        cache->vc, cache->true_location, ups->pixel_radius);
    cache->radius_squared = cache->radius * cache->radius;

    copy_v3_v3(cache->anchored_location, cache->true_location);
  }

  sculpt_update_brush_delta(ups, ob, brush);

  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->vertex_rotation = -BLI_dial_angle(cache->dial, cache->mouse) * cache->bstrength;

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    copy_v3_v3(cache->anchored_location, cache->true_location);
    ups->anchored_size = ups->pixel_radius;
  }

  cache->special_rotation = ups->brush_rotation;
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth) */
static bool sculpt_needs_connectivity_info(const Brush *brush, SculptSession *ss, int stroke_mode)
{
  if (ss && ss->pbvh && sculpt_automasking_enabled(ss, brush)) {
    return true;
  }
  return ((stroke_mode == BRUSH_STROKE_SMOOTH) || (ss && ss->cache && ss->cache->alt_smooth) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMOOTH) || (brush->autosmooth_factor > 0) ||
          ((brush->sculpt_tool == SCULPT_TOOL_MASK) && (brush->mask_tool == BRUSH_MASK_SMOOTH)) ||
          (brush->sculpt_tool == SCULPT_TOOL_POSE));
}

static void sculpt_stroke_modifiers_check(const bContext *C, Object *ob, const Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  View3D *v3d = CTX_wm_view3d(C);

  bool need_pmap = sculpt_needs_connectivity_info(brush, ss, 0);
  if (ss->shapekey_active || ss->deform_modifiers_active ||
      (!BKE_sculptsession_use_pbvh_draw(ob, v3d) && need_pmap)) {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_sculpt_update_object_for_edit(depsgraph, ob, need_pmap, false);
  }
}

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptRaycastData *srd = (SculptRaycastData *)data_v;
    float(*origco)[3] = NULL;
    bool use_origco = false;

    if (srd->original && srd->ss->cache) {
      if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
        use_origco = true;
      }
      else {
        /* intersect with coordinates from before we started stroke */
        SculptUndoNode *unode = sculpt_undo_get_node(node);
        origco = (unode) ? unode->co : NULL;
        use_origco = origco ? true : false;
      }
    }

    if (BKE_pbvh_node_raycast(srd->ss->pbvh,
                              node,
                              origco,
                              use_origco,
                              srd->ray_start,
                              srd->ray_normal,
                              &srd->isect_precalc,
                              &srd->depth,
                              &srd->active_vertex_index,
                              srd->face_normal)) {
      srd->hit = 1;
      *tmin = srd->depth;
    }
  }
}

static void sculpt_find_nearest_to_ray_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptFindNearestToRayData *srd = (SculptFindNearestToRayData *)data_v;
    float(*origco)[3] = NULL;
    bool use_origco = false;

    if (srd->original && srd->ss->cache) {
      if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
        use_origco = true;
      }
      else {
        /* intersect with coordinates from before we started stroke */
        SculptUndoNode *unode = sculpt_undo_get_node(node);
        origco = (unode) ? unode->co : NULL;
        use_origco = origco ? true : false;
      }
    }

    if (BKE_pbvh_node_find_nearest_to_ray(srd->ss->pbvh,
                                          node,
                                          origco,
                                          use_origco,
                                          srd->ray_start,
                                          srd->ray_normal,
                                          &srd->depth,
                                          &srd->dist_sq_to_ray)) {
      srd->hit = 1;
      *tmin = srd->dist_sq_to_ray;
    }
  }
}

static void sculpt_raycast_detail_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) < *tmin) {
    SculptDetailRaycastData *srd = (SculptDetailRaycastData *)data_v;
    if (BKE_pbvh_bmesh_node_raycast_detail(
            node, srd->ray_start, &srd->isect_precalc, &srd->depth, &srd->edge_length)) {
      srd->hit = 1;
      *tmin = srd->depth;
    }
  }
}

static float sculpt_raycast_init(ViewContext *vc,
                                 const float mouse[2],
                                 float ray_start[3],
                                 float ray_end[3],
                                 float ray_normal[3],
                                 bool original)
{
  float obimat[4][4];
  float dist;
  Object *ob = vc->obact;
  RegionView3D *rv3d = (RegionView3D *)vc->ar->regiondata;

  /* TODO: what if the segment is totally clipped? (return == 0) */
  ED_view3d_win_to_segment_clipped(
      vc->depsgraph, vc->ar, vc->v3d, mouse, ray_start, ray_end, true);

  invert_m4_m4(obimat, ob->obmat);
  mul_m4_v3(obimat, ray_start);
  mul_m4_v3(obimat, ray_end);

  sub_v3_v3v3(ray_normal, ray_end, ray_start);
  dist = normalize_v3(ray_normal);

  if ((rv3d->is_persp == false) &&
      /* if the ray is clipped, don't adjust its start/end */
      ((rv3d->rflag & RV3D_CLIPPING) == 0)) {
    BKE_pbvh_raycast_project_ray_root(ob->sculpt->pbvh, original, ray_start, ray_end, ray_normal);

    /* recalculate the normal */
    sub_v3_v3v3(ray_normal, ray_end, ray_start);
    dist = normalize_v3(ray_normal);
  }

  return dist;
}

/* Gets the normal, location and active vertex location of the geometry under the cursor. This also
 * updates
 * the active vertex and cursor related data of the SculptSession using the mouse position */
static bool sculpt_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Sculpt *sd = scene->toolsettings->sculpt;
  Object *ob;
  SculptSession *ss;
  ViewContext vc;
  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3], sampled_normal[3],
      mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  int totnode;
  bool original = false, hit = false;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;
  ss = ob->sculpt;

  if (!ss->pbvh) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* PBVH raycast to get active vertex and face normal */
  depth = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);
  sculpt_stroke_modifiers_check(C, ob, brush);

  SculptRaycastData srd;
  srd.original = original;
  srd.ss = ob->sculpt;
  srd.hit = 0;
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = depth;
  srd.face_normal = face_normal;

  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  BKE_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original);

  /* Cursor is not over the mesh, return default values */
  if (!srd.hit) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* Update the active vertex of the SculptSession */
  ss->active_vertex_index = srd.active_vertex_index;
  copy_v3_v3(out->active_vertex_co, sculpt_active_vertex_co_get(ss));

  copy_v3_v3(out->location, ray_normal);
  mul_v3_fl(out->location, srd.depth);
  add_v3_v3(out->location, ray_start);

  /* Option to return the face normal directly for performance o accuracy reasons */
  if (!use_sampled_normal) {
    copy_v3_v3(out->normal, srd.face_normal);
    return hit;
  }

  /* Sampled normal calculation */
  float radius;

  /* Update cursor data in SculptSession */
  invert_m4_m4(ob->imat, ob->obmat);
  copy_m3_m4(mat, vc.rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(ss->cursor_view_normal, viewDir);
  copy_v3_v3(ss->cursor_normal, srd.face_normal);
  copy_v3_v3(ss->cursor_location, out->location);
  ss->rv3d = vc.rv3d;

  if (!BKE_brush_use_locked_size(scene, brush)) {
    radius = paint_calc_object_space_radius(&vc, out->location, BKE_brush_size_get(scene, brush));
  }
  else {
    radius = BKE_brush_unprojected_radius_get(scene, brush);
  }
  ss->cursor_radius = radius;

  PBVHNode **nodes = sculpt_pbvh_gather_cursor_update(ob, sd, original, &totnode);

  /* In case there are no nodes under the cursor, return the face normal */
  if (!totnode) {
    MEM_SAFE_FREE(nodes);
    copy_v3_v3(out->normal, srd.face_normal);
    return true;
  }

  /* Calculate the sampled normal */
  if (sculpt_pbvh_calc_area_normal(brush, ob, nodes, totnode, true, sampled_normal)) {
    copy_v3_v3(out->normal, sampled_normal);
  }
  else {
    /* Use face normal when there are no vertices to sample inside the cursor radius */
    copy_v3_v3(out->normal, srd.face_normal);
  }
  MEM_SAFE_FREE(nodes);
  return true;
}

/* Do a raycast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 * Returns 0 if the ray doesn't hit the mesh, non-zero otherwise
 */
static bool sculpt_stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob;
  SculptSession *ss;
  StrokeCache *cache;
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3];
  bool original;
  ViewContext vc;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;

  ss = ob->sculpt;
  cache = ss->cache;
  original = (cache) ? cache->original : false;

  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  sculpt_stroke_modifiers_check(C, ob, brush);

  depth = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT);
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);
  }

  bool hit = false;
  {
    SculptRaycastData srd;
    srd.ss = ob->sculpt;
    srd.ray_start = ray_start;
    srd.ray_normal = ray_normal;
    srd.hit = 0;
    srd.depth = depth;
    srd.original = original;
    srd.face_normal = face_normal;
    isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

    BKE_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original);
    if (srd.hit) {
      hit = true;
      copy_v3_v3(out, ray_normal);
      mul_v3_fl(out, srd.depth);
      add_v3_v3(out, ray_start);
    }
  }

  if (hit == false) {
    if (ELEM(brush->falloff_shape, PAINT_FALLOFF_SHAPE_TUBE)) {
      SculptFindNearestToRayData srd;
      srd.original = original;
      srd.ss = ob->sculpt;
      srd.hit = 0;
      srd.ray_start = ray_start;
      srd.ray_normal = ray_normal;
      srd.depth = FLT_MAX;
      srd.dist_sq_to_ray = FLT_MAX,
      BKE_pbvh_find_nearest_to_ray(
          ss->pbvh, sculpt_find_nearest_to_ray_cb, &srd, ray_start, ray_normal, srd.original);
      if (srd.hit) {
        hit = true;
        copy_v3_v3(out, ray_normal);
        mul_v3_fl(out, srd.depth);
        add_v3_v3(out, ray_start);
      }
    }
  }

  return hit;
}

static void sculpt_brush_init_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  /* init mtex nodes */
  if (mtex->tex && mtex->tex->nodetree) {
    /* has internal flag to detect it only does it once */
    ntreeTexBeginExecTree(mtex->tex->nodetree);
  }

  /* TODO: Shouldn't really have to do this at the start of every
   * stroke, but sculpt would need some sort of notification when
   * changes are made to the texture. */
  sculpt_update_tex(scene, sd, ss);
}

static void sculpt_brush_stroke_init(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = CTX_data_active_object(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  int mode = RNA_enum_get(op->ptr, "mode");
  bool is_smooth;
  bool need_mask = false;

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    need_mask = true;
  }

  view3d_operator_needs_opengl(C);
  sculpt_brush_init_tex(scene, sd, ss);

  is_smooth = sculpt_needs_connectivity_info(brush, ss, Widget_Sculpt::mode);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, is_smooth, need_mask);
}

static void sculpt_restore_mesh(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Restore the mesh before continuing with anchored stroke */
  if ((brush->flag & BRUSH_ANCHORED) ||
      ((brush->sculpt_tool == SCULPT_TOOL_GRAB ||
        brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
       BKE_brush_use_size_pressure(ss->cache->vc->scene, brush)) ||
      (brush->flag & BRUSH_DRAG_DOT)) {
    paint_mesh_restore_co(sd, ob);
  }
}

/* Copy the PBVH bounding box into the object's bounding box */
static void sculpt_update_object_bounding_box(Object *ob)
{
  if (ob->runtime.bb) {
    float bb_min[3], bb_max[3];

    BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
    BKE_boundbox_init_from_minmax(ob->runtime.bb, bb_min, bb_max);
  }
}

static void sculpt_flush_update_step(bContext *C, SculptUpdateType update_flags)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *ar = CTX_wm_region(C);
  MultiresModifierData *mmd = ss->multires;
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (rv3d) {
    /* Mark for faster 3D viewport redraws. */
    rv3d->rflag |= RV3D_PAINTING;
  }

  if (mmd != NULL) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  /* Only current viewport matters, slower update for all viewports will
   * be done in sculpt_flush_update_done. */
  if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
    /* Slow update with full dependency graph update and all that comes with it.
     * Needed when there are modifiers or full shading in the 3D viewport. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(ar);
  }
  else {
    /* Fast path where we just update the BVH nodes that changed, and redraw
     * only the part of the 3D viewport where changes happened. */
    rcti r;

    if (update_flags & SCULPT_UPDATE_COORDS) {
      BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB);
      /* Update the object's bounding box too so that the object
       * doesn't get incorrectly clipped during drawing in
       * draw_mesh_object(). [#33790] */
      sculpt_update_object_bounding_box(ob);
    }

    if (sculpt_get_redraw_rect(ar, CTX_wm_region_view3d(C), ob, &r)) {
      if (ss->cache) {
        ss->cache->current_r = r;
      }

      /* previous is not set in the current cache else
       * the partial rect will always grow */
      sculpt_extend_redraw_rect_previous(ob, &r);

      r.xmin += ar->winrct.xmin - 2;
      r.xmax += ar->winrct.xmin + 2;
      r.ymin += ar->winrct.ymin - 2;
      r.ymax += ar->winrct.ymin + 2;
      ED_region_tag_redraw_partial(ar, &r, true);
    }
  }
}

static void sculpt_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags)
{
  /* After we are done drawing the stroke, check if we need to do a more
   * expensive depsgraph tag to update geometry. */
  wmWindowManager *wm = CTX_wm_manager(C);
  View3D *current_v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = (Mesh *)ob->data;
  bool need_tag = (mesh->id.us > 1); /* Always needed for linked duplicates. */

  if (rv3d) {
    rv3d->rflag &= ~RV3D_PAINTING;
  }

  for (wmWindow *win = (wmWindow *)wm->windows.first; win; win = win->next) {
    bScreen *screen = WM_window_get_active_screen(win);
    for (ScrArea *sa = (ScrArea *)screen->areabase.first; sa; sa = sa->next) {
      SpaceLink *sl = (SpaceLink *)sa->spacedata.first;
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        if (v3d != current_v3d) {
          need_tag |= !BKE_sculptsession_use_pbvh_draw(ob, v3d);
        }

        /* Tag all 3D viewports for redraw now that we are done. Others
         * viewports did not get a full redraw, and anti-aliasing for the
         * current viewport was deactivated. */
        for (ARegion *ar = (ARegion *)sa->regionbase.first; ar; ar = ar->next) {
          if (ar->regiontype == RGN_TYPE_WINDOW) {
            ED_region_tag_redraw(ar);
          }
        }
      }
    }
  }

  if (update_flags & SCULPT_UPDATE_COORDS) {
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateOriginalBB);
  }

  if (update_flags & SCULPT_UPDATE_MASK) {
    BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BKE_pbvh_bmesh_after_stroke(ss->pbvh);
  }

  /* optimization: if there is locked key and active modifiers present in */
  /* the stack, keyblock is updating at each step. otherwise we could update */
  /* keyblock only when stroke is finished */
  if (ss->shapekey_active && !ss->deform_modifiers_active) {
    sculpt_update_keyblock(ob);
  }

  if (need_tag) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0) */
static bool over_mesh(bContext *C, struct wmOperator *UNUSED(op), float x, float y)
{
  float mouse[2], co[3];

  mouse[0] = x;
  mouse[1] = y;

  return sculpt_stroke_get_location(C, co, mouse);
}

static bool sculpt_stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  /* Don't start the stroke until mouse goes over the mesh.
   * note: mouse will only be null when re-executing the saved stroke.
   * We have exception for 'exec' strokes since they may not set 'mouse',
   * only 'location', see: T52195. */
  if (((op->flag & OP_IS_INVOKE) == 0) || (mouse == NULL) ||
      over_mesh(C, op, mouse[0], mouse[1])) {
    Object *ob = CTX_data_active_object(C);
    SculptSession *ss = ob->sculpt;
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

    ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

    sculpt_update_cache_invariants(C, sd, ss, op, mouse);

    sculpt_undo_push_begin(sculpt_tool_name(sd));

    return 1;
  }
  else {
    return 0;
  }
}

static void sculpt_stroke_update_step(bContext *C,
                                      struct PaintStroke *UNUSED(stroke),
                                      PointerRNA *itemptr)
{
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  sculpt_stroke_modifiers_check(C, ob, brush);
  sculpt_update_cache_variants(C, sd, ob, itemptr);
  sculpt_restore_mesh(sd, ob);

  if (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    float object_space_constant_detail = 1.0f / (sd->constant_detail * mat4_to_scale(ob->obmat));
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh, object_space_constant_detail);
  }
  else if (sd->flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh, ss->cache->radius * sd->detail_percent / 100.0f);
  }
  else {
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh,
                                   (ss->cache->radius / (float)ups->pixel_radius) *
                                       (float)(sd->detail_size * U.pixelsize) / 0.4f);
  }

  if (sculpt_stroke_is_dynamic_topology(ss, brush)) {
    do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);
  }

  do_symmetrical_brush_actions(sd, ob, do_brush_action, ups);
  sculpt_combine_proxies(sd, ob);

  /* hack to fix noise texture tearing mesh */
  sculpt_fix_noise_tear(sd, ob);

  /* TODO(sergey): This is not really needed for the solid shading,
   * which does use pBVH drawing anyway, but texture and wireframe
   * requires this.
   *
   * Could be optimized later, but currently don't think it's so
   * much common scenario.
   *
   * Same applies to the DEG_id_tag_update() invoked from
   * sculpt_flush_update_step().
   */
  if (ss->deform_modifiers_active) {
    sculpt_flush_stroke_deform(sd, ob, sculpt_tool_is_proxy_used(brush->sculpt_tool));
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }

  ss->cache->first_time = false;

  /* Cleanup */
  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    sculpt_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  else {
    sculpt_flush_update_step(C, SCULPT_UPDATE_COORDS);
  }
}

static void sculpt_brush_exit_tex(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  if (mtex->tex && mtex->tex->nodetree) {
    ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
  }
}

static void sculpt_stroke_done(const bContext *C, struct PaintStroke *UNUSED(stroke))
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Finished */
  if (ss->cache) {
    UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
    Brush *brush = BKE_paint_brush(&sd->paint);
    BLI_assert(brush == ss->cache->brush); /* const, so we shouldn't change. */
    ups->draw_inverted = false;

    sculpt_stroke_modifiers_check(C, ob, brush);

    /* Alt-Smooth */
    if (ss->cache->alt_smooth) {
      if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
        brush->mask_tool = ss->cache->saved_mask_brush_tool;
      }
      else {
        BKE_brush_size_set(scene, brush, ss->cache->saved_smooth_size);
        brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, ss->cache->saved_active_brush_name);
        if (brush) {
          BKE_paint_brush_set(&sd->paint, brush);
        }
      }
    }

    if (sculpt_automasking_enabled(ss, brush)) {
      sculpt_automasking_end(ob);
    }

    sculpt_cache_free(ss->cache);
    ss->cache = NULL;

    sculpt_undo_push_end();

    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      sculpt_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
    }
    else {
      sculpt_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    }

    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }

  sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  struct PaintStroke *stroke;
  int ignore_background_click;
  //int retval;

  sculpt_brush_stroke_init(C, op);

  stroke = paint_stroke_new(C,
                            op,
                            sculpt_stroke_get_location,
                            sculpt_stroke_test_start,
                            sculpt_stroke_update_step,
                            NULL,
                            sculpt_stroke_done,
                            event->type);

  op->customdata = stroke;

  /* For tablet rotation */
  ignore_background_click = Widget_Sculpt::ignore_background_click;  // RNA_boolean_get(op->ptr, "ignore_background_click");

  /* Get the 3d position and 2d-projected position of the VR cursor. */
  memcpy(Widget_Sculpt::location,
         VR_UI::cursor_position_get(VR_SPACE_BLENDER, Widget_Sculpt::cursor_side).m[3],
         sizeof(float) * 3);
  if (Widget_Sculpt::raycast) {
    ARegion *ar = CTX_wm_region(C);
    RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
    float projmat[4][4];
    mul_m4_m4m4(projmat, (float(*)[4])rv3d->winmat, (float(*)[4])rv3d->viewmat);
    mul_project_m4_v3(projmat, Widget_Sculpt::location);
    Widget_Sculpt::mouse[0] = (int)((ar->winx / 2.0f) +
                                    (ar->winx / 2.0f) * Widget_Sculpt::location[0]);
    Widget_Sculpt::mouse[1] = (int)((ar->winy / 2.0f) +
                                    (ar->winy / 2.0f) * Widget_Sculpt::location[1]);
  }

  Widget_Sculpt::pressure = vr_get_obj()->controller[Widget_Sculpt::cursor_side]->trigger_pressure;

  //if (ignore_background_click && !over_mesh(C, op, event->x, event->y)) {
  //  paint_stroke_free(C, op);
  //  return OPERATOR_PASS_THROUGH;
  //}

  //if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
  //  paint_stroke_free(C, op);
  //  return OPERATOR_FINISHED;
  //}
  /* add modal handler */
  //WM_event_add_modal_handler(C, op);

  //OPERATOR_RETVAL_CHECK(retval);
  //BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  sculpt_stroke_test_start(C, op, Widget_Sculpt::mouse);

  if (Widget_Sculpt::raycast) {
    sculpt_stroke_get_location(C, Widget_Sculpt::location, Widget_Sculpt::mouse);
  }
  else {
    ViewContext vc;
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    ED_view3d_viewcontext_init(C, &vc, depsgraph);
    Object *ob = vc.obact;
    SculptSession *ss = ob->sculpt;
    StrokeCache *cache = ss->cache;
    if (cache) {
      const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
      sculpt_stroke_modifiers_check(C, ob, brush);

      /* Test if object mesh is within sculpt sphere radius. */
      Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
      int totnode;
      const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ?
                                    true :
                                    ss->cache->original;
      const float radius_scale = 1.25f;
      cache->radius = Widget_Sculpt::sculpt_radius;
      sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale, &totnode);
      if (totnode) {
        float obimat[4][4];
        invert_m4_m4(obimat, ob->obmat);
        mul_m4_v3(obimat, Widget_Sculpt::location);
        copy_v3_v3(cache->true_location, Widget_Sculpt::location);
      }
    }
  }

  // SculptSession *ss = CTX_data_active_object(C)->sculpt;
  // memcpy(ss->cache->true_location, Widget_Sculpt::location, sizeof(float) * 3);

  sculpt_brush_stroke_init(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
  /*
  sculpt_brush_stroke_init(C, op);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    sculpt_stroke_get_location,
                                    sculpt_stroke_test_start,
                                    sculpt_stroke_update_step,
                                    NULL,
                                    sculpt_stroke_done,
                                    0);
  */
  
  // sculpt_stroke_update_step, NULL, sculpt_stroke_done, 0);
  sculpt_stroke_update_step(C, NULL, NULL);
  
  /* frees op->customdata */
  //paint_stroke_exec(C, op);

  return OPERATOR_FINISHED;
}

static void sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  /* XXX Canceling strokes that way does not work with dynamic topology,
   *     user will have to do real undo for now. See T46456. */
  if (ss->cache && !sculpt_stroke_is_dynamic_topology(ss, brush)) {
    paint_mesh_restore_co(sd, ob);
  }

  paint_stroke_cancel(C, op);

  if (ss->cache) {
    sculpt_cache_free(ss->cache);
    ss->cache = NULL;
  }

  sculpt_brush_exit_tex(sd);
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sculpt";
  ot->idname = "SCULPT_OT_brush_stroke";
  ot->description = "Sculpt a stroke into the geometry";

  /* api callbacks */
  ot->invoke = sculpt_brush_stroke_invoke;
  ot->modal = paint_stroke_modal;
  ot->exec = sculpt_brush_stroke_exec;
  ot->poll = sculpt_poll;
  ot->cancel = sculpt_brush_stroke_cancel;

  /* flags (sculpt does own undo? (ton) */
  ot->flag = OPTYPE_BLOCKING;

  /* properties */

  paint_stroke_operator_properties(ot);

  RNA_def_boolean(ot->srna,
                  "ignore_background_click",
                  0,
                  "Ignore Background Click",
                  "Clicks on the background do not start the stroke");
}

/* Reset the copy of the mesh that is being sculpted on (currently just for the layer brush) */

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator *UNUSED(op))
{
  SculptSession *ss = CTX_data_active_object(C)->sculpt;

  if (ss) {
    if (ss->layer_co) {
      MEM_freeN(ss->layer_co);
    }
    ss->layer_co = NULL;
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Persistent Base";
  ot->idname = "SCULPT_OT_set_persistent_base";
  ot->description = "Reset the copy of the mesh that is being sculpted on";

  /* api callbacks */
  ot->exec = sculpt_set_persistent_base_exec;
  ot->poll = sculpt_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Dynamic Topology **************************/

static void sculpt_dynamic_topology_triangulate(BMesh *bm)
{
  if (bm->totloop != bm->totface * 3) {
    BM_mesh_triangulate(
        bm, MOD_TRIANGULATE_QUAD_BEAUTY, MOD_TRIANGULATE_NGON_EARCLIP, 4, false, NULL, NULL, NULL);
  }
}

static void sculpt_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = NULL;
  }

  if (ss->pmap) {
    MEM_freeN(ss->pmap);
    ss->pmap = NULL;
  }

  if (ss->pmap_mem) {
    MEM_freeN(ss->pmap_mem);
    ss->pmap_mem = NULL;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

static void sculpt_dyntopo_node_layers_add(SculptSession *ss)
{
  int cd_node_layer_index;

  char layer_id[] = "_dyntopo_node_id";

  cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT, layer_id);
  if (cd_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_INT, layer_id);
    cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_INT, layer_id);
  }

  ss->cd_vert_node_offset = CustomData_get_n_offset(
      &ss->bm->vdata,
      CD_PROP_INT,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->vdata, CD_PROP_INT));

  ss->bm->vdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;

  cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->pdata, CD_PROP_INT, layer_id);
  if (cd_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->pdata, CD_PROP_INT, layer_id);
    cd_node_layer_index = CustomData_get_named_layer_index(&ss->bm->pdata, CD_PROP_INT, layer_id);
  }

  ss->cd_face_node_offset = CustomData_get_n_offset(
      &ss->bm->pdata,
      CD_PROP_INT,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->pdata, CD_PROP_INT));

  ss->bm->pdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;
}

static void sculpt_dynamic_topology_enable_ex(Main *bmain,
                                              Depsgraph *depsgraph,
                                              Scene *scene,
                                              Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = (Mesh *)ob->data;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  sculpt_pbvh_clear(ob);

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove [#36280] */
  BKE_mesh_mselect_clear(me);

  /* Create triangles-only BMesh */
  struct BMeshCreateParams _bmeshcreateparams;
  _bmeshcreateparams.use_toolflags = false;
  ss->bm = BM_mesh_create(&allocsize,
                          &_bmeshcreateparams);

  struct BMeshFromMeshParams _bmeshfrommeshparams;
  _bmeshfrommeshparams.calc_face_normal = true;
  _bmeshfrommeshparams.use_shapekey = true;
  _bmeshfrommeshparams.active_shapekey = ob->shapenr;
  BM_mesh_bm_from_me(ss->bm,
                     me,
                     &_bmeshfrommeshparams);
  sculpt_dynamic_topology_triangulate(ss->bm);
  BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
  sculpt_dyntopo_node_layers_add(ss);
  /* make sure the data for existing faces are initialized */
  if (me->totpoly != ss->bm->totface) {
    BM_mesh_normals_update(ss->bm);
  }

  /* Enable dynamic topology */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Enable logging for undo/redo */
  ss->bm_log = BM_log_create(ss->bm);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from */
static void sculpt_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = (Mesh *)ob->data;

  sculpt_pbvh_clear(ob);

  if (unode) {
    /* Free all existing custom data */
    CustomData_free(&me->vdata, me->totvert);
    CustomData_free(&me->edata, me->totedge);
    CustomData_free(&me->fdata, me->totface);
    CustomData_free(&me->ldata, me->totloop);
    CustomData_free(&me->pdata, me->totpoly);

    /* Copy over stored custom data */
    me->totvert = unode->geom_totvert;
    me->totloop = unode->geom_totloop;
    me->totpoly = unode->geom_totpoly;
    me->totedge = unode->geom_totedge;
    me->totface = 0;
    CustomData_copy(
        &unode->geom_vdata, &me->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, unode->geom_totvert);
    CustomData_copy(
        &unode->geom_edata, &me->edata, CD_MASK_MESH.emask, CD_DUPLICATE, unode->geom_totedge);
    CustomData_copy(
        &unode->geom_ldata, &me->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, unode->geom_totloop);
    CustomData_copy(
        &unode->geom_pdata, &me->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, unode->geom_totpoly);

    BKE_mesh_update_customdata_pointers(me, false);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);
  }

  /* Clear data */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* typically valid but with global-undo they can be NULL, [#36234] */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = NULL;
  }
  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = NULL;
  }

  BKE_particlesystem_reset_all(ob);
  BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

static void sculpt_dynamic_topology_disable(bContext *C, SculptUndoNode *unode)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  sculpt_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, unode);
}

static void sculpt_dynamic_topology_disable_with_undo(Main *bmain,
                                                      Depsgraph *depsgraph,
                                                      Scene *scene,
                                                      Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm) {
    sculpt_undo_push_begin("Dynamic topology disable");
    sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_END);
    sculpt_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, NULL);
    sculpt_undo_push_end();
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm == NULL) {
    sculpt_undo_push_begin("Dynamic topology enable");
    sculpt_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
    sculpt_undo_push_end();
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  WM_cursor_wait(1);

  if (ss->bm) {
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(0);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

  return OPERATOR_FINISHED;
}

enum eDynTopoWarnFlag {
  DYNTOPO_WARN_VDATA = (1 << 0),
  DYNTOPO_WARN_EDATA = (1 << 1),
  DYNTOPO_WARN_LDATA = (1 << 2),
  DYNTOPO_WARN_MODIFIER = (1 << 3),
};

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_VDATA | DYNTOPO_WARN_EDATA | DYNTOPO_WARN_LDATA)) {
    const char *msg_error = TIP_("Vertex Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve vertex colors, UVs, or other customdata");
    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  if (flag & DYNTOPO_WARN_MODIFIER) {
    const char *msg_error = TIP_("Generative Modifiers Detected!");
    const char *msg = TIP_(
        "Keeping the modifiers will increase polycount when returning to object mode");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0, NULL);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static enum eDynTopoWarnFlag sculpt_dynamic_topology_check(Scene *scene, Object *ob)
{
  Mesh *me = (Mesh *)ob->data;
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = (eDynTopoWarnFlag)0;

  BLI_assert(ss->bm == NULL);
  UNUSED_VARS_NDEBUG(ss);

  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (!ELEM(i, CD_MVERT, CD_MEDGE, CD_MFACE, CD_MLOOP, CD_MPOLY, CD_PAINT_MASK, CD_ORIGINDEX)) {
      if (CustomData_has_layer(&me->vdata, i)) {
        flag = (eDynTopoWarnFlag) (flag | DYNTOPO_WARN_VDATA);
      }
      if (CustomData_has_layer(&me->edata, i)) {
        flag = (eDynTopoWarnFlag) (flag | DYNTOPO_WARN_EDATA);
      }
      if (CustomData_has_layer(&me->ldata, i)) {
        flag = (eDynTopoWarnFlag) (flag | DYNTOPO_WARN_LDATA);
      }
    }
  }

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

    /* exception for shape keys because we can edit those */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = modifierType_getInfo((ModifierType)md->type);
      if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mti->type == eModifierTypeType_Constructive) {
        flag = (eDynTopoWarnFlag) (flag | DYNTOPO_WARN_MODIFIER);
        break;
      }
    }
  }

  return flag;
}

static int sculpt_dynamic_topology_toggle_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    Scene *scene = CTX_data_scene(C);
    enum eDynTopoWarnFlag flag = sculpt_dynamic_topology_check(scene, ob);

    if (flag) {
      /* The mesh has customdata that will be lost, let the user confirm this is OK */
      return dyntopo_warning_popup(C, op->type, flag);
    }
  }

  return sculpt_dynamic_topology_toggle_exec(C, op);
}

static void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dynamic Topology Toggle";
  ot->idname = "SCULPT_OT_dynamic_topology_toggle";
  ot->description = "Dynamic topology alters the mesh topology while sculpting";

  /* api callbacks */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = sculpt_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************* SCULPT_OT_optimize *************************/

static int sculpt_optimize_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  sculpt_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static bool sculpt_and_dynamic_topology_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  return sculpt_mode_poll(C) && ob->sculpt->bm;
}

/* The BVH gets less optimal more quickly with dynamic topology than
 * regular sculpting. There is no doubt more clever stuff we can do to
 * optimize it on the fly, but for now this gives the user a nicer way
 * to recalculate it than toggling modes. */
static void SCULPT_OT_optimize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Optimize";
  ot->idname = "SCULPT_OT_optimize";
  ot->description = "Recalculate the sculpt BVH to improve performance";

  /* api callbacks */
  ot->exec = sculpt_optimize_exec;
  ot->poll = sculpt_and_dynamic_topology_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* Dynamic topology symmetrize ********************/

static int sculpt_symmetrize_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);
  const Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;

  /* To simplify undo for symmetrize, all BMesh elements are logged
   * as deleted, then after symmetrize operation all BMesh elements
   * are logged as added (as opposed to attempting to store just the
   * parts that symmetrize modifies) */
  sculpt_undo_push_begin("Dynamic topology symmetrize");
  sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_SYMMETRIZE);
  BM_log_before_all_removed(ss->bm, ss->bm_log);

  BM_mesh_toolflags_set(ss->bm, true);

  /* Symmetrize and re-triangulate */
  BMO_op_callf(ss->bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "symmetrize input=%avef direction=%i  dist=%f",
               sd->symmetrize_direction,
               0.00001f);
  sculpt_dynamic_topology_triangulate(ss->bm);

  /* bisect operator flags edges (keep tags clean for edge queue) */
  BM_mesh_elem_hflag_disable_all(ss->bm, BM_EDGE, BM_ELEM_TAG, false);

  BM_mesh_toolflags_set(ss->bm, false);

  /* Finish undo */
  BM_log_all_added(ss->bm, ss->bm_log);
  sculpt_undo_push_end();

  /* Redraw */
  sculpt_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_symmetrize(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Symmetrize";
  ot->idname = "SCULPT_OT_symmetrize";
  ot->description = "Symmetrize the topology modifications";

  /* api callbacks */
  ot->exec = sculpt_symmetrize_exec;
  ot->poll = sculpt_and_dynamic_topology_poll;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  /* Create persistent sculpt mode data */
  BKE_sculpt_toolsettings_data_ensure(scene);

  ob->sculpt = (SculptSession *)MEM_callocN(sizeof(SculptSession), "sculpt session");
  ob->sculpt->mode_type = OB_MODE_SCULPT;
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false);
}

static int ed_object_sculptmode_flush_recalc_flag(Scene *scene,
                                                  Object *ob,
                                                  MultiresModifierData *mmd)
{
  int flush_recalc = 0;
  /* multires in sculpt mode could have different from object mode subdivision level */
  flush_recalc |= mmd && BKE_multires_sculpt_level_get(mmd) != mmd->lvl;
  /* if object has got active modifiers, it's dm could be different in sculpt mode  */
  flush_recalc |= (int)sculpt_has_active_modifiers(scene, ob);
  return flush_recalc;
}

static void ED_object_sculptmode_enter_ex(Main *bmain,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   const bool force_dyntopo,
                                   ReportList *reports)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  /* Enter sculptmode */
  ob->mode |= mode_flag;

  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);

  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);

  if (flush_recalc) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  /* Create sculpt mode session data */
  if (ob->sculpt) {
    BKE_sculptsession_free(ob);
  }

  /* Make sure derived final from original object does not reference possibly
   * freed memory.
   */
  BKE_object_free_derived_caches(ob);

  sculpt_init_session(depsgraph, scene, ob);

  /* Mask layer is required */
  if (mmd) {
    /* XXX, we could attempt to support adding mask data mid-sculpt mode (with multi-res)
     * but this ends up being quite tricky (and slow) */
    BKE_sculpt_mask_layers_ensure(ob, mmd);
  }

  if (!(fabsf(ob->scale[0] - ob->scale[1]) < 1e-4f &&
        fabsf(ob->scale[1] - ob->scale[2]) < 1e-4f)) {
    BKE_report(
        reports, RPT_WARNING, "Object has non-uniform scale, sculpting may be unpredictable");
  }
  else if (is_negative_m4(ob->obmat)) {
    BKE_report(reports, RPT_WARNING, "Object has negative scale, sculpting may be unpredictable");
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(scene, PAINT_MODE_SCULPT);
  BKE_paint_init(bmain, scene, PAINT_MODE_SCULPT, PAINT_CURSOR_SCULPT);

  paint_cursor_start_explicit(paint, (wmWindowManager *)bmain->wm.first, sculpt_poll_view3d);

  /* Check dynamic-topology flag; re-enter dynamic-topology mode when changing modes,
   * As long as no data was added that is not supported. */
  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    const char *message_unsupported = NULL;
    if (me->totloop != me->totpoly * 3) {
      message_unsupported = TIP_("non-triangle face");
    }
    else if (mmd != NULL) {
      message_unsupported = TIP_("multi-res modifier");
    }
    else {
      enum eDynTopoWarnFlag flag = sculpt_dynamic_topology_check(scene, ob);
      if (flag == 0) {
        /* pass */
      }
      else if (flag & DYNTOPO_WARN_VDATA) {
        message_unsupported = TIP_("vertex data");
      }
      else if (flag & DYNTOPO_WARN_EDATA) {
        message_unsupported = TIP_("edge data");
      }
      else if (flag & DYNTOPO_WARN_LDATA) {
        message_unsupported = TIP_("face data");
      }
      else if (flag & DYNTOPO_WARN_MODIFIER) {
        message_unsupported = TIP_("constructive modifier");
      }
      else {
        BLI_assert(0);
      }
    }

    if ((message_unsupported == NULL) || force_dyntopo) {
      /* Needed because we may be entering this mode before the undo system loads. */
      wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
      bool has_undo = wm->undo_stack != NULL;
      /* undo push is needed to prevent memory leak */
      if (has_undo) {
        sculpt_undo_push_begin("Dynamic topology enable");
      }
      sculpt_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
      if (has_undo) {
        sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
        sculpt_undo_push_end();
      }
    }
    else {
      BKE_reportf(
          reports, RPT_WARNING, "Dynamic Topology found: %s, disabled", message_unsupported);
      me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
    }
  }

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

static void ED_object_sculptmode_enter(struct bContext *C, Depsgraph *depsgraph, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, reports);
}

static void ED_object_sculptmode_exit_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  multires_flush_sculpt_updates(ob);

  /* Not needed for now. */
#if 0
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);
#endif

  /* Always for now, so leaving sculpt mode always ensures scene is in
   * a consistent state.
   */
  if (true || /* flush_recalc || */ (ob->sculpt && ob->sculpt->bm)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    /* Dynamic topology must be disabled before exiting sculpt
     * mode to ensure the undo stack stays in a consistent
     * state */
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);

    /* store so we know to re-enable when entering sculpt mode */
    me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
  }

  /* Leave sculptmode */
  ob->mode &= ~mode_flag;

  BKE_sculptsession_free(ob);

  paint_cursor_delete_textures();

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

static void ED_object_sculptmode_exit(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
}

static const char *object_mode_op_string(eObjectMode mode)
{
  if (mode & OB_MODE_EDIT) {
    return "OBJECT_OT_editmode_toggle";
  }
  if (mode == OB_MODE_SCULPT) {
    return "SCULPT_OT_sculptmode_toggle";
  }
  if (mode == OB_MODE_VERTEX_PAINT) {
    return "PAINT_OT_vertex_paint_toggle";
  }
  if (mode == OB_MODE_WEIGHT_PAINT) {
    return "PAINT_OT_weight_paint_toggle";
  }
  if (mode == OB_MODE_TEXTURE_PAINT) {
    return "PAINT_OT_texture_paint_toggle";
  }
  if (mode == OB_MODE_PARTICLE_EDIT) {
    return "PARTICLE_OT_particle_edit_toggle";
  }
  if (mode == OB_MODE_POSE) {
    return "OBJECT_OT_posemode_toggle";
  }
  if (mode == OB_MODE_EDIT_GPENCIL) {
    return "GPENCIL_OT_editmode_toggle";
  }
  if (mode == OB_MODE_PAINT_GPENCIL) {
    return "GPENCIL_OT_paintmode_toggle";
  }
  if (mode == OB_MODE_SCULPT_GPENCIL) {
    return "GPENCIL_OT_sculptmode_toggle";
  }
  if (mode == OB_MODE_WEIGHT_GPENCIL) {
    return "GPENCIL_OT_weightmode_toggle";
  }
  return NULL;
}

static bool ED_object_mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports)
{
  bool ok;
  if (!ELEM(ob->mode, mode, OB_MODE_OBJECT)) {
    const char *opstring = object_mode_op_string((eObjectMode)ob->mode);

    WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
    ok = ELEM(ob->mode, mode, OB_MODE_OBJECT);
    if (!ok) {
      wmOperatorType *ot = WM_operatortype_find(opstring, false);
      BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
    }
  }
  else {
    ok = true;
  }

  return ok;
}

static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  const int mode_flag = OB_MODE_SCULPT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, (eObjectMode)mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
  }
  else {
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, op->reports);
    BKE_paint_toolslots_brush_validate(bmain, &ts->sculpt->paint);

    if (ob->mode & mode_flag) {
      Mesh *me = (Mesh *)ob->data;
      /* Dyntopo add's it's own undo step. */
      if ((me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) == 0) {
        /* Without this the memfile undo step is used,
         * while it works it causes lag when undoing the first undo step, see T71564. */
        // wmWindowManager *wm = CTX_wm_manager(C);
        // if (wm->op_undo_depth <= 1) {
        //   sculpt_undo_push_begin(op->type->name);
        //}
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

  // TODO max-k WM_msg_publish_rna_prop causes unresolved external symbol "struct PropertyRNA rna_Object_mode"
  //WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sculpt Mode";
  ot->idname = "SCULPT_OT_sculptmode_toggle";
  ot->description = "Toggle sculpt mode in 3D view";

  /* api callbacks */
  ot->exec = sculpt_mode_toggle_exec;
  ot->poll = ED_operator_object_active_editable_mesh;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool sculpt_and_constant_or_manual_detail_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  return sculpt_mode_poll(C) && ob->sculpt->bm &&
         (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL));
}

static int sculpt_detail_flood_fill_exec(bContext *C, wmOperator *UNUSED(op))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  float size;
  float bb_min[3], bb_max[3], center[3], dim[3];
  int i, totnodes;
  PBVHNode **nodes;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnodes);

  if (!totnodes) {
    return OPERATOR_CANCELLED;
  }

  for (i = 0; i < totnodes; i++) {
    BKE_pbvh_node_mark_topology_update(nodes[i]);
  }
  /* get the bounding box, it's center and size */
  BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
  add_v3_v3v3(center, bb_min, bb_max);
  mul_v3_fl(center, 0.5f);
  sub_v3_v3v3(dim, bb_max, bb_min);
  size = max_fff(dim[0], dim[1], dim[2]);

  /* update topology size */
  float object_space_constant_detail = 1.0f / (sd->constant_detail * mat4_to_scale(ob->obmat));
  BKE_pbvh_bmesh_detail_size_set(ss->pbvh, object_space_constant_detail);

  sculpt_undo_push_begin("Dynamic topology flood fill");
  sculpt_undo_push_node(ob, NULL, SCULPT_UNDO_COORDS);

  while (BKE_pbvh_bmesh_update_topology(
      ss->pbvh, (PBVHTopologyUpdateMode)(PBVH_Collapse | PBVH_Subdivide), center, NULL, size, false, false)) {
    for (i = 0; i < totnodes; i++) {
      BKE_pbvh_node_mark_topology_update(nodes[i]);
    }
  }

  MEM_SAFE_FREE(nodes);
  sculpt_undo_push_end();

  /* force rebuild of pbvh for better BB placement */
  sculpt_pbvh_clear(ob);
  /* Redraw */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_detail_flood_fill(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Detail Flood Fill";
  ot->idname = "SCULPT_OT_detail_flood_fill";
  ot->description = "Flood fill the mesh with the selected detail setting";

  /* api callbacks */
  ot->exec = sculpt_detail_flood_fill_exec;
  ot->poll = sculpt_and_constant_or_manual_detail_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void sample_detail(bContext *C, int mx, int my)
{
  /* Find 3D view to pick from. */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *sa = BKE_screen_find_area_xy(screen, SPACE_VIEW3D, mx, my);
  ARegion *ar = (sa) ? BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my) : NULL;
  if (ar == NULL) {
    return;
  }

  /* Set context to 3D view. */
  ScrArea *prev_sa = CTX_wm_area(C);
  ARegion *prev_ar = CTX_wm_region(C);
  CTX_wm_area_set(C, sa);
  CTX_wm_region_set(C, ar);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  /* Pick sample detail. */
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = vc.obact;
  Brush *brush = BKE_paint_brush(&sd->paint);

  sculpt_stroke_modifiers_check(C, ob, brush);

  float mouse[2] = {(float)(mx - ar->winrct.xmin), (float)(my - ar->winrct.ymin)};
  float ray_start[3], ray_end[3], ray_normal[3];
  float depth = sculpt_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, false);

  SculptDetailRaycastData srd;
  srd.hit = 0;
  srd.ray_start = ray_start;
  srd.depth = depth;
  srd.edge_length = 0.0f;
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

  BKE_pbvh_raycast(ob->sculpt->pbvh, sculpt_raycast_detail_cb, &srd, ray_start, ray_normal, false);

  if (srd.hit && srd.edge_length > 0.0f) {
    /* Convert edge length to world space detail resolution. */
    sd->constant_detail = 1 / (srd.edge_length * mat4_to_scale(ob->obmat));
  }

  /* Restore context. */
  CTX_wm_area_set(C, prev_sa);
  CTX_wm_region_set(C, prev_ar);
}

static int sculpt_sample_detail_size_exec(bContext *C, wmOperator *op)
{
  int ss_co[2];
  RNA_int_get_array(op->ptr, "location", ss_co);
  sample_detail(C, ss_co[0], ss_co[1]);
  return OPERATOR_FINISHED;
}

static int sculpt_sample_detail_size_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
{
  ED_workspace_status_text(C, TIP_("Click on the mesh to set the detail"));
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EYEDROPPER);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_detail_size_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        int ss_co[2] = {event->x, event->y};

        sample_detail(C, ss_co[0], ss_co[1]);

        RNA_int_set_array(op->ptr, "location", ss_co);
        WM_cursor_modal_restore(CTX_wm_window(C));
        ED_workspace_status_text(C, NULL);
        WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

        return OPERATOR_FINISHED;
      }
      break;

    case RIGHTMOUSE: {
      WM_cursor_modal_restore(CTX_wm_window(C));
      ED_workspace_status_text(C, NULL);

      return OPERATOR_CANCELLED;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void SCULPT_OT_sample_detail_size(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Detail Size";
  ot->idname = "SCULPT_OT_sample_detail_size";
  ot->description = "Sample the mesh detail on clicked point";

  /* api callbacks */
  ot->invoke = sculpt_sample_detail_size_invoke;
  ot->exec = sculpt_sample_detail_size_exec;
  ot->modal = sculpt_sample_detail_size_modal;
  ot->poll = sculpt_and_constant_or_manual_detail_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int_array(ot->srna,
                    "location",
                    2,
                    NULL,
                    0,
                    SHRT_MAX,
                    "Location",
                    "Screen Coordinates of sampling",
                    0,
                    SHRT_MAX);
}

/* Dynamic-topology detail size
 *
 * This should be improved further, perhaps by showing a triangle
 * grid rather than brush alpha */
static void set_brush_rc_props(PointerRNA *ptr, const char *prop)
{
  char *path = BLI_sprintfN("tool_settings.sculpt.brush.%s", prop);
  RNA_string_set(ptr, "data_path_primary", path);
  MEM_freeN(path);
}

static int sculpt_set_detail_size_exec(bContext *C, wmOperator *UNUSED(op))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("WM_OT_radial_control", true);

  WM_operator_properties_create_ptr(&props_ptr, ot);

  if (sd->flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    set_brush_rc_props(&props_ptr, "constant_detail_resolution");
    RNA_string_set(
        &props_ptr, "data_path_primary", "tool_settings.sculpt.constant_detail_resolution");
  }
  else if (sd->flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    set_brush_rc_props(&props_ptr, "constant_detail_resolution");
    RNA_string_set(&props_ptr, "data_path_primary", "tool_settings.sculpt.detail_percent");
  }
  else {
    set_brush_rc_props(&props_ptr, "detail_size");
    RNA_string_set(&props_ptr, "data_path_primary", "tool_settings.sculpt.detail_size");
  }

  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);

  WM_operator_properties_free(&props_ptr);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_detail_size(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Detail Size";
  ot->idname = "SCULPT_OT_set_detail_size";
  ot->description =
      "Set the mesh detail (either relative or constant one, depending on current dyntopo mode)";

  /* api callbacks */
  ot->exec = sculpt_set_detail_size_exec;
  ot->poll = sculpt_and_dynamic_topology_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
static void filter_cache_init_task_cb(void *__restrict userdata,
                                      const int i,
                                      const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  PBVHNode *node = data->nodes[i];

  sculpt_undo_push_node(data->ob, node, SCULPT_UNDO_COORDS);
}

static void sculpt_filter_cache_init(Object *ob, Sculpt *sd)
{
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  ss->filter_cache = (FilterCache *)MEM_callocN(sizeof(FilterCache), "filter cache");

  ss->filter_cache->random_seed = rand();

  float center[3] = {0.0f};
  SculptSearchSphereData search_data;
  search_data.original = true;
  search_data.center = center;
  search_data.radius_squared = FLT_MAX;
  search_data.ignore_fully_masked = true;

  BKE_pbvh_search_gather(pbvh,
                         sculpt_search_sphere_cb,
                         &search_data,
                         &ss->filter_cache->nodes,
                         &ss->filter_cache->totnode);

  for (int i = 0; i < ss->filter_cache->totnode; i++) {
    BKE_pbvh_node_mark_normals_update(ss->filter_cache->nodes[i]);
  }

  /* mesh->runtime.subdiv_ccg is not available. Updating of the normals is done during drawing.
   * Filters can't use normals in multires. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
    BKE_pbvh_update_normals(ss->pbvh, NULL);
  }

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.nodes = ss->filter_cache->nodes;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BKE_pbvh_parallel_range(
      0, ss->filter_cache->totnode, &data, filter_cache_init_task_cb, &settings);
}

static void sculpt_filter_cache_free(SculptSession *ss)
{
  if (ss->filter_cache->nodes) {
    MEM_freeN(ss->filter_cache->nodes);
  }
  if (ss->filter_cache->mask_update_it) {
    MEM_freeN(ss->filter_cache->mask_update_it);
  }
  if (ss->filter_cache->prev_mask) {
    MEM_freeN(ss->filter_cache->prev_mask);
  }
  if (ss->filter_cache->normal_factor) {
    MEM_freeN(ss->filter_cache->normal_factor);
  }
  MEM_freeN(ss->filter_cache);
  ss->filter_cache = NULL;
}

typedef enum eSculptMeshFilterTypes {
  MESH_FILTER_SMOOTH = 0,
  MESH_FILTER_SCALE = 1,
  MESH_FILTER_INFLATE = 2,
  MESH_FILTER_SPHERE = 3,
  MESH_FILTER_RANDOM = 4,
} eSculptMeshFilterTypes;

static EnumPropertyItem prop_mesh_filter_types[] = {
    {MESH_FILTER_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth mesh"},
    {MESH_FILTER_SCALE, "SCALE", 0, "Scale", "Scale mesh"},
    {MESH_FILTER_INFLATE, "INFLATE", 0, "Inflate", "Inflate mesh"},
    {MESH_FILTER_SPHERE, "SPHERE", 0, "Sphere", "Morph into sphere"},
    {MESH_FILTER_RANDOM, "RANDOM", 0, "Random", "Randomize vertex positions"},
    {0, NULL, 0, NULL, NULL},
};

typedef enum eMeshFilterDeformAxis {
  MESH_FILTER_DEFORM_X = 1 << 0,
  MESH_FILTER_DEFORM_Y = 1 << 1,
  MESH_FILTER_DEFORM_Z = 1 << 2,
} eMeshFilterDeformAxis;

static EnumPropertyItem prop_mesh_filter_deform_axis_items[] = {
    {MESH_FILTER_DEFORM_X, "X", 0, "X", "Deform in the X axis"},
    {MESH_FILTER_DEFORM_Y, "Y", 0, "Y", "Deform in the Y axis"},
    {MESH_FILTER_DEFORM_Z, "Z", 0, "Z", "Deform in the Z axis"},
    {0, NULL, 0, NULL, NULL},
};

static void mesh_filter_task_cb(void *__restrict userdata,
                                const int i,
                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];

  const int filter_type = data->filter_type;

  SculptOrigVertData orig_data;
  sculpt_orig_vert_data_init(&orig_data, data->ob, data->nodes[i]);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    sculpt_orig_vert_data_update(&orig_data, &vd);
    float orig_co[3], val[3], avg[3], normal[3], disp[3], disp2[3], transform[3][3], final_pos[3];
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1 - fade;
    fade *= data->filter_strength;

    if (fade == 0.0f) {
      continue;
    }

    copy_v3_v3(orig_co, orig_data.co);
    switch (filter_type) {
      case MESH_FILTER_SMOOTH:
        CLAMP(fade, -1.0f, 1.0f);
        switch (BKE_pbvh_type(ss->pbvh)) {
          case PBVH_FACES:
            neighbor_average(ss, avg, vd.index);
            break;
          case PBVH_BMESH:
            bmesh_neighbor_average(avg, vd.bm_vert);
            break;
          case PBVH_GRIDS:
            grids_neighbor_average(ss, avg, vd.index);
            break;
        }
        sub_v3_v3v3(val, avg, orig_co);
        madd_v3_v3v3fl(val, orig_co, val, fade);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_INFLATE:
        normal_short_to_float_v3(normal, orig_data.no);
        mul_v3_v3fl(disp, normal, fade);
        break;
      case MESH_FILTER_SCALE:
        unit_m3(transform);
        scale_m3_fl(transform, 1 + fade);
        copy_v3_v3(val, orig_co);
        mul_m3_v3(transform, val);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_SPHERE:
        normalize_v3_v3(disp, orig_co);
        if (fade > 0) {
          mul_v3_v3fl(disp, disp, fade);
        }
        else {
          mul_v3_v3fl(disp, disp, -fade);
        }

        unit_m3(transform);
        if (fade > 0) {
          scale_m3_fl(transform, 1 - fade);
        }
        else {
          scale_m3_fl(transform, 1 + fade);
        }
        copy_v3_v3(val, orig_co);
        mul_m3_v3(transform, val);
        sub_v3_v3v3(disp2, val, orig_co);

        mid_v3_v3v3(disp, disp, disp2);
        break;
      case MESH_FILTER_RANDOM: {
        normal_short_to_float_v3(normal, orig_data.no);
        /* Index is not unique for multires, so hash by vertex coordinates. */
        const uint *hash_co = (const uint *)orig_co;
        const uint hash = BLI_hash_int_2d(hash_co[0], hash_co[1]) ^
                          BLI_hash_int_2d(hash_co[2], ss->filter_cache->random_seed);
        mul_v3_fl(normal, hash * (1.0f / (float)0xFFFFFFFF) - 0.5f);
        mul_v3_v3fl(disp, normal, fade);
        break;
      }
    }

    for (int it = 0; it < 3; it++) {
      if (!ss->filter_cache->enabled_axis[it]) {
        disp[it] = 0.0f;
      }
    }

    add_v3_v3v3(final_pos, orig_co, disp);
    copy_v3_v3(vd.co, final_pos);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_redraw(node);
  BKE_pbvh_node_mark_normals_update(node);
}

static int sculpt_mesh_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int filter_type = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    sculpt_filter_cache_free(ss);
    sculpt_undo_push_end();
    sculpt_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  float len = event->prevclickx - event->mval[0];
  filter_strength = filter_strength * -len * 0.001f * UI_DPI_FAC;

  sculpt_vertex_random_access_init(ss);

  bool needs_pmap = (filter_type == MESH_FILTER_SMOOTH);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false);

  SculptThreadedTaskData data;
  data.sd = sd;
  data.ob = ob;
  data.nodes = ss->filter_cache->nodes;
  data.filter_type = filter_type;
  data.filter_strength = filter_strength;

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(
      &settings, (sd->flags & SCULPT_USE_OPENMP), ss->filter_cache->totnode);
  BKE_pbvh_parallel_range(0, ss->filter_cache->totnode, &data, mesh_filter_task_cb, &settings);

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    sculpt_flush_stroke_deform(sd, ob, true);
  }

  sculpt_flush_update_step(C, SCULPT_UPDATE_COORDS);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_mesh_filter_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int filter_type = RNA_enum_get(op->ptr, "type");
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  int deform_axis = RNA_enum_get(op->ptr, "deform_axis");
  if (deform_axis == 0) {
    return OPERATOR_CANCELLED;
  }

  sculpt_vertex_random_access_init(ss);

  bool needs_pmap = (filter_type == MESH_FILTER_SMOOTH);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES && needs_pmap && !ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  sculpt_undo_push_begin("Mesh filter");

  sculpt_filter_cache_init(ob, sd);

  ss->filter_cache->enabled_axis[0] = deform_axis & MESH_FILTER_DEFORM_X;
  ss->filter_cache->enabled_axis[1] = deform_axis & MESH_FILTER_DEFORM_Y;
  ss->filter_cache->enabled_axis[2] = deform_axis & MESH_FILTER_DEFORM_Z;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void SCULPT_OT_mesh_filter(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Filter mesh";
  ot->idname = "SCULPT_OT_mesh_filter";
  ot->description = "Applies a filter to modify the current mesh";

  /* api callbacks */
  ot->invoke = sculpt_mesh_filter_invoke;
  ot->modal = sculpt_mesh_filter_modal;
  ot->poll = sculpt_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  RNA_def_enum(ot->srna,
               "type",
               prop_mesh_filter_types,
               MESH_FILTER_INFLATE,
               "Filter type",
               "Operation that is going to be applied to the mesh");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter Strength", -10.0f, 10.0f);
  RNA_def_enum_flag(ot->srna,
                    "deform_axis",
                    prop_mesh_filter_deform_axis_items,
                    MESH_FILTER_DEFORM_X | MESH_FILTER_DEFORM_Y | MESH_FILTER_DEFORM_Z,
                    "Deform axis",
                    "Apply the deformation in the selected axis");
}

typedef enum eSculptMaskFilterTypes {
  MASK_FILTER_SMOOTH = 0,
  MASK_FILTER_SHARPEN = 1,
  MASK_FILTER_GROW = 2,
  MASK_FILTER_SHRINK = 3,
  MASK_FILTER_CONTRAST_INCREASE = 5,
  MASK_FILTER_CONTRAST_DECREASE = 6,
} eSculptMaskFilterTypes;

static EnumPropertyItem prop_mask_filter_types[] = {
    {MASK_FILTER_SMOOTH, "SMOOTH", 0, "Smooth Mask", "Smooth mask"},
    {MASK_FILTER_SHARPEN, "SHARPEN", 0, "Sharpen Mask", "Sharpen mask"},
    {MASK_FILTER_GROW, "GROW", 0, "Grow Mask", "Grow mask"},
    {MASK_FILTER_SHRINK, "SHRINK", 0, "Shrink Mask", "Shrink mask"},
    {MASK_FILTER_CONTRAST_INCREASE,
     "CONTRAST_INCREASE",
     0,
     "Increase contrast",
     "Increase the contrast of the paint mask"},
    {MASK_FILTER_CONTRAST_DECREASE,
     "CONTRAST_DECREASE",
     0,
     "Decrease contrast",
     "Decrease the contrast of the paint mask"},
    {0, NULL, 0, NULL, NULL},
};

static void mask_filter_task_cb(void *__restrict userdata,
                                const int i,
                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = (SculptThreadedTaskData *)userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  bool update = false;

  const int mode = data->filter_type;
  float contrast = 0.0f;

  PBVHVertexIter vd;

  if (mode == MASK_FILTER_CONTRAST_INCREASE) {
    contrast = 0.1f;
  }

  if (mode == MASK_FILTER_CONTRAST_DECREASE) {
    contrast = -0.1f;
  }

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    float delta, gain, offset, max, min;
    float prev_val = *vd.mask;
    SculptVertexNeighborIter ni;
    switch (mode) {
      case MASK_FILTER_SMOOTH:
      case MASK_FILTER_SHARPEN: {
        float val = 0.0f;

        switch (BKE_pbvh_type(ss->pbvh)) {
          case PBVH_FACES:
            val = neighbor_average_mask(ss, vd.index);
            break;
          case PBVH_BMESH:
            val = bmesh_neighbor_average_mask(vd.bm_vert, vd.cd_vert_mask_offset);
            break;
          case PBVH_GRIDS:
            val = grids_neighbor_average_mask(ss, vd.index);
            break;
        }

        val -= *vd.mask;

        if (mode == MASK_FILTER_SMOOTH) {
          *vd.mask += val;
        }
        else if (mode == MASK_FILTER_SHARPEN) {
          if (*vd.mask > 0.5f) {
            *vd.mask += 0.05f;
          }
          else {
            *vd.mask -= 0.05f;
          }
          *vd.mask += val / 2;
        }
        break;
      }
      case MASK_FILTER_GROW:
        max = 0.0f;
        sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
        {
          float vmask_f = data->prev_mask[ni.index];
          if (vmask_f > max) {
            max = vmask_f;
          }
        }
        sculpt_vertex_neighbors_iter_end(ni);
        *vd.mask = max;
        break;
      case MASK_FILTER_SHRINK:
        min = 1.0f;
        sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
        {
          float vmask_f = data->prev_mask[ni.index];
          if (vmask_f < min) {
            min = vmask_f;
          }
        }
        sculpt_vertex_neighbors_iter_end(ni);
        *vd.mask = min;
        break;
      case MASK_FILTER_CONTRAST_INCREASE:
      case MASK_FILTER_CONTRAST_DECREASE:
        delta = contrast / 2.0f;
        gain = 1.0f - delta * 2.0f;
        if (contrast > 0) {
          gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
          offset = gain * (-delta);
        }
        else {
          delta *= -1;
          offset = gain * (delta);
        }
        *vd.mask = gain * (*vd.mask) + offset;
        break;
    }
    CLAMP(*vd.mask, 0.0f, 1.0f);
    if (*vd.mask != prev_val) {
      update = true;
    }
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (update) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static int sculpt_mask_filter_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode **nodes;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int totnode;
  int filter_type = RNA_enum_get(op->ptr, "filter_type");

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true);

  sculpt_vertex_random_access_init(ss);

  if (!ob->sculpt->pmap) {
    return OPERATOR_CANCELLED;
  }

  int num_verts = sculpt_vertex_count_get(ss);

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  sculpt_undo_push_begin("Mask filter");

  for (int i = 0; i < totnode; i++) {
    sculpt_undo_push_node(ob, nodes[i], SCULPT_UNDO_MASK);
  }

  float *prev_mask = NULL;
  int iterations = RNA_int_get(op->ptr, "iterations");

  /* Auto iteration count calculates the number of iteration based on the vertices of the mesh to
   * avoid adding an unnecessary amount of undo steps when using the operator from a shortcut.
   * One iteration per 50000 vertices in the mesh should be fine in most cases.
   * Maybe we want this to be configurable. */
  if (RNA_boolean_get(op->ptr, "auto_iteration_count")) {
    iterations = (int)(num_verts / 50000.0f) + 1;
  }

  for (int i = 0; i < iterations; i++) {
    if (ELEM(filter_type, MASK_FILTER_GROW, MASK_FILTER_SHRINK)) {
      prev_mask = (float *)MEM_mallocN(num_verts * sizeof(float), "prevmask");
      for (int j = 0; j < num_verts; j++) {
        prev_mask[j] = sculpt_vertex_mask_get(ss, j);
      }
    }

    SculptThreadedTaskData data;
    data.sd = sd;
    data.ob = ob;
    data.nodes = nodes;
    data.filter_type = filter_type;
    data.prev_mask = prev_mask;

    PBVHParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
    BKE_pbvh_parallel_range(0, totnode, &data, mask_filter_task_cb, &settings);

    if (ELEM(filter_type, MASK_FILTER_GROW, MASK_FILTER_SHRINK)) {
      MEM_freeN(prev_mask);
    }
  }

  MEM_SAFE_FREE(nodes);

  sculpt_undo_push_end();

  ED_region_tag_redraw(ar);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  return OPERATOR_FINISHED;
}



void Widget_Sculpt::toggle_dyntopo()
{
  sculpt_dynamic_topology_toggle_exec(vr_get_obj()->ctx, NULL);
}

void Widget_Sculpt::update_brush(int new_brush)
{
  bContext *C = vr_get_obj()->ctx;
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    /* Exit edit mode */
    VR_UI::editmode_exit = true;
    Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
    return;
  }

  // sculpt_mode_toggle_exec(C, &sculpt_dummy_op);
  // TODO: when to toggle sculpt mode?
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  if (!ob->sculpt) {
    sculpt_mode_toggle_exec(C, &sculpt_dummy_op);
  }

  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *br = BKE_paint_brush(&sd->paint);
  br->sculpt_tool = new_brush;
  brush = new_brush;
}

void Widget_Sculpt::drag_start(VR_UI::Cursor &c)
{
  if (c.bimanual) {
    return;
  }

  bContext *C = vr_get_obj()->ctx;
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    return;
  }

  /* Start sculpt tool operation */
  if (sculpt_dummy_op.type == 0) {
    sculpt_dummy_op.type = WM_operatortype_find("SCULPT_OT_brush_stroke", true);
    if (sculpt_dummy_op.type == 0) {
	  return;
    }
  }
  if (sculpt_dummy_op.ptr == 0) {
    sculpt_dummy_op.ptr = (PointerRNA*)MEM_callocN(sizeof(PointerRNA), __func__);
    if (sculpt_dummy_op.ptr == 0) {
	  return;
    }
    WM_operator_properties_create_ptr(sculpt_dummy_op.ptr, sculpt_dummy_op.type);
    WM_operator_properties_sanitize(sculpt_dummy_op.ptr, 0);
  }
  if (sculpt_dummy_op.reports == 0) {
    sculpt_dummy_op.reports = (ReportList*)MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
    if (sculpt_dummy_op.reports == 0) {
	  return;
    }
    BKE_reports_init(sculpt_dummy_op.reports, RPT_STORE | RPT_FREE);
  }

  cursor_side = c.side;

  // TODO: when to toggle sculpt mode?
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  if (!ob->sculpt) {
    sculpt_mode_toggle_exec(C, &sculpt_dummy_op);
  }

  /* Scale parameters based on distance from hmd. */
  const Mat44f &hmd = VR_UI::hmd_position_get(VR_SPACE_REAL);
  p_hmd = *(Coord3Df *)hmd.m[3];
  p_cursor = *(Coord3Df *)c.position.get().m[3];
  dist = (p_cursor - p_hmd).length();

  sculpt_radius_prev = sculpt_radius;
  sculpt_strength_prev = sculpt_strength;

  /* Save original sculpt mode. */
  mode_orig = mode;

  if (VR_UI::shift_key_get()) {
    param_mode = true;
  }
  else {
    if (brush == SCULPT_TOOL_SMOOTH) {
      mode = BRUSH_STROKE_SMOOTH;
    }
    else {
      if (VR_UI::ctrl_key_get()) {
        if (mode_orig == BRUSH_STROKE_NORMAL) {
          mode = BRUSH_STROKE_INVERT;
        }
        else {
          mode = BRUSH_STROKE_NORMAL;
        }
      }
    }
    if (CTX_data_active_object(C)) {
      stroke_started = true;
      /* Perform stroke */
      sculpt_brush_stroke_invoke(C, &sculpt_dummy_op, &sculpt_dummy_event);
    }
  }

  // for (int i = 0; i < VR_SIDES; ++i) {
  //	Widget_Sculpt::obj.do_render[i] = true;
  //}

  is_dragging = true;
}

void Widget_Sculpt::drag_contd(VR_UI::Cursor &c)
{
  if (c.bimanual) {
    return;
  }

  bContext *C = vr_get_obj()->ctx;
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    return;
  }

  if (VR_UI::shift_key_get()) {
    param_mode = true;
    const Coord3Df &p = *(Coord3Df *)c.position.get().m[3];
    float current_dist = (p - p_hmd).length();
    float delta = (p - p_cursor).length();

    /* Adjust radius */
    if ((current_dist < dist)) {
      sculpt_radius = sculpt_radius_prev + delta;
      if (sculpt_radius > WIDGET_SCULPT_MAX_RADIUS) {
        sculpt_radius = WIDGET_SCULPT_MAX_RADIUS;
      }
    }
    else {
      sculpt_radius = sculpt_radius_prev - delta;
      if (sculpt_radius < 0.0f) {
        sculpt_radius = 0.0f;
      }
    }
  }
  else if (!param_mode) {
    bContext *C = vr_get_obj()->ctx;
    if (CTX_data_active_object(C)) {
      sculpt_brush_stroke_exec(C, &sculpt_dummy_op);
    }
  }

  // for (int i = 0; i < VR_SIDES; ++i) {
  //	Widget_Sculpt::obj.do_render[i] = true;
  //}

  is_dragging = true;
}

void Widget_Sculpt::drag_stop(VR_UI::Cursor &c)
{
  if (c.bimanual) {
    return;
  }

  is_dragging = false;

  bContext *C = vr_get_obj()->ctx;
  Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    /* Exit edit mode */
    VR_UI::editmode_exit = true;
    Widget_Transform::transform_space = VR_UI::TRANSFORMSPACE_LOCAL;
    return;
  }

  if (VR_UI::shift_key_get()) {
    param_mode = true;
    const Coord3Df &p = *(Coord3Df *)c.position.get().m[3];
    float current_dist = (p - p_hmd).length();
    float delta = (p - p_cursor).length();

    /* Adjust radius */
    if ((current_dist < dist)) {
      sculpt_radius = sculpt_radius_prev + delta;
      if (sculpt_radius > WIDGET_SCULPT_MAX_RADIUS) {
        sculpt_radius = WIDGET_SCULPT_MAX_RADIUS;
      }
    }
    else {
      sculpt_radius = sculpt_radius_prev - delta;
      if (sculpt_radius < 0.0f) {
        sculpt_radius = 0.0f;
      }
    }
  }

  if (stroke_started) {
    bContext *C = vr_get_obj()->ctx;
    if (CTX_data_active_object(C)) {
      sculpt_stroke_done(C, NULL);
    }
  }

  /* Restore original sculpt mode. */
  mode = mode_orig;

  stroke_started = false;
  param_mode = false;

  // for (int i = 0; i < VR_SIDES; ++i) {
  //	Widget_Sculpt::obj.do_render[i] = false;
  //}
}

static void render_gimbal(const float radius,
                          const float color[4],
                          const bool filled,
                          const float arc_partial_angle,
                          const float arc_inner_factor)
{
  /* Adapted from dial_geom_draw() in dial3d_gizmo.c */

  GPU_line_width(1.0f);
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor4fv(color);

  if (filled) {
    imm_draw_circle_fill_2d(pos, 0, 0, radius, 100.0f);
  }
  else {
    if (arc_partial_angle == 0.0f) {
      imm_draw_circle_wire_2d(pos, 0, 0, radius, 100.0f);
      if (arc_inner_factor != 0.0f) {
        imm_draw_circle_wire_2d(pos, 0, 0, arc_inner_factor, 100.0f);
      }
    }
    else {
      float arc_partial_deg = RAD2DEGF((M_PI * 2) - arc_partial_angle);
      imm_draw_circle_partial_wire_2d(pos, 0, 0, radius, 100.0f, 0.0f, arc_partial_deg);
    }
  }

  immUnbindProgram();
}

void Widget_Sculpt::render(VR_Side side)
{
  // if (param_mode) {
  //	/* Render measurement text. */
  //	const Mat44f& prior_model_matrix = VR_Draw::get_model_matrix();
  //	static Mat44f m;
  //	m = VR_UI::hmd_position_get(VR_SPACE_REAL);
  //	const Mat44f& c = VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side);
  //	memcpy(m.m[3], c.m[3], sizeof(float) * 3);
  //	VR_Draw::update_modelview_matrix(&m, 0);

  //	VR_Draw::set_depth_test(false, false);
  //	VR_Draw::set_color(0.8f, 0.8f, 0.8f, 1.0f);
  //	static std::string param_str;
  //	sprintf((char*)param_str.data(), "%.3f", sculpt_radius);
  //	VR_Draw::render_string(param_str.c_str(), 0.02f, 0.02f, VR_HALIGN_CENTER, VR_VALIGN_TOP,
  //0.0f, 0.08f, 0.001f);

  //	VR_Draw::set_depth_test(true, true);
  //	VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);
  //}

  static float color[4] = {1.0f, 1.0f, 1.0f, 0.8f};
  switch ((eBrushSculptTool)brush) {
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_CLAY:
    case SCULPT_TOOL_CLAY_STRIPS:
    case SCULPT_TOOL_LAYER:
    case SCULPT_TOOL_INFLATE:
    case SCULPT_TOOL_BLOB:
    case SCULPT_TOOL_CREASE:
    case SCULPT_TOOL_MASK: {
      if (Widget_Sculpt::is_dragging) {
        if (Widget_Sculpt::mode == BRUSH_STROKE_INVERT) {
          color[0] = 0.0f;
          color[1] = 0.0f;
          color[2] = 1.0f;
        }
        else {
          color[0] = 1.0f;
          color[1] = 0.0f;
          color[2] = 0.0f;
        }
      }
      else {
        if (VR_UI::ctrl_key_get()) {
          if (Widget_Sculpt::mode_orig == BRUSH_STROKE_INVERT) {
            color[0] = 1.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
          }
          else {
            color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 1.0f;
          }
        }
        else {
          if (Widget_Sculpt::mode_orig == BRUSH_STROKE_INVERT) {
            color[0] = 0.0f;
            color[1] = 0.0f;
            color[2] = 1.0f;
          }
          else {
            color[0] = 1.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
          }
        }
      }
      break;
    }
    case SCULPT_TOOL_FLATTEN:
    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_PINCH: {
      if (Widget_Sculpt::is_dragging) {
        if (Widget_Sculpt::mode == BRUSH_STROKE_INVERT) {
          color[0] = 1.0f;
          color[1] = 1.0f;
          color[2] = 0.0f;
        }
        else {
          color[0] = 0.0f;
          color[1] = 1.0f;
          color[2] = 1.0f;
        }
      }
      else {
        if (VR_UI::ctrl_key_get()) {
          if (Widget_Sculpt::mode_orig == BRUSH_STROKE_INVERT) {
            color[0] = 0.0f;
            color[1] = 1.0f;
            color[2] = 1.0f;
          }
          else {
            color[0] = 1.0f;
            color[1] = 1.0f;
            color[2] = 0.0f;
          }
        }
        else {
          if (Widget_Sculpt::mode_orig == BRUSH_STROKE_INVERT) {
            color[0] = 1.0f;
            color[1] = 1.0f;
            color[2] = 0.0f;
          }
          else {
            color[0] = 0.0f;
            color[1] = 1.0f;
            color[2] = 1.0f;
          }
        }
      }
      break;
    }
    case SCULPT_TOOL_GRAB:
    case SCULPT_TOOL_SNAKE_HOOK:
    case SCULPT_TOOL_NUDGE:
    case SCULPT_TOOL_THUMB:
    case SCULPT_TOOL_ROTATE: {
      color[0] = 0.0f;
      color[1] = 1.0f;
      color[2] = 0.0f;
      break;
    }
    case SCULPT_TOOL_SMOOTH:
    case SCULPT_TOOL_SIMPLIFY:
    default: {
      color[0] = 1.0f;
      color[1] = 1.0f;
      color[2] = 1.0f;
      break;
    }
  }

  if (raycast) {
    /* Render sculpt circle. */
    GPU_blend(true);
    GPU_matrix_push();
    static Mat44f m = VR_Math::identity_f;
    m = vr_get_obj()->t_eye[VR_SPACE_BLENDER][side];
    memcpy(
        m.m[3], VR_UI::cursor_position_get(VR_SPACE_BLENDER, cursor_side).m[3], sizeof(float) * 3);
    GPU_matrix_mul(m.m);
    GPU_polygon_smooth(false);

    color[3] = 0.8f;
    render_gimbal(sculpt_radius, color, false, 0.0f, 0.0f);

    GPU_blend(false);
    GPU_matrix_pop();
  }
  else {
    /* Render sculpt ball. */
    const Mat44f &prior_model_matrix = VR_Draw::get_model_matrix();

    VR_Draw::update_modelview_matrix(&VR_UI::cursor_position_get(VR_SPACE_REAL, cursor_side), 0);
    // VR_Draw::set_depth_test(false, false);
    // color[3] = 0.1f;
    // VR_Draw::set_color(color);
    // VR_Draw::render_ball(sculpt_radius);
    // VR_Draw::set_depth_test(true, false);
    color[3] = 0.1f;
    VR_Draw::set_color(color);
    VR_Draw::render_ball(sculpt_radius);
    // VR_Draw::set_depth_test(true, true);

    VR_Draw::update_modelview_matrix(&prior_model_matrix, 0);
  }

  // Widget_Sculpt::obj.do_render[side] = false;
}
