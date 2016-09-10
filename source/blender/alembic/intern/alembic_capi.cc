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
 * Contributor(s): Esteban Tovagliari, Cedric Paille, Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../ABC_alembic.h"

#include <Alembic/AbcMaterial/IMaterial.h>

#include "abc_archive.h"
#include "abc_camera.h"
#include "abc_curves.h"
#include "abc_mesh.h"
#include "abc_points.h"
#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cachefile_types.h"
#include "DNA_curve_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cachefile.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

/* SpaceType struct has a member called 'new' which obviously conflicts with C++
 * so temporarily redefining the new keyword to make it compile. */
#define new extern_new
#include "BKE_screen.h"
#undef new

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "WM_api.h"
#include "WM_types.h"
}

using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::ObjectHeader;

using Alembic::AbcGeom::MetaData;
using Alembic::AbcGeom::P3fArraySamplePtr;
using Alembic::AbcGeom::kWrapExisting;

using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICurves;
using Alembic::AbcGeom::ICurvesSchema;
using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::ILight;
using Alembic::AbcGeom::INuPatch;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPointsSchema;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::IPolyMeshSchema;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::IV2fGeomParam;
using Alembic::AbcGeom::IXform;
using Alembic::AbcGeom::IXformSchema;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::XformSample;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IN3fArrayProperty;
using Alembic::AbcGeom::IN3fGeomParam;
using Alembic::AbcGeom::V3fArraySamplePtr;

using Alembic::AbcMaterial::IMaterial;

struct AbcArchiveHandle {
	int unused;
};

ABC_INLINE ArchiveReader *archive_from_handle(AbcArchiveHandle *handle)
{
	return reinterpret_cast<ArchiveReader *>(handle);
}

ABC_INLINE AbcArchiveHandle *handle_from_archive(ArchiveReader *archive)
{
	return reinterpret_cast<AbcArchiveHandle *>(archive);
}

static void gather_objects_paths(const IObject &object, ListBase *object_paths)
{
	if (!object.valid()) {
		return;
	}

	AlembicObjectPath *abc_path;

	for (int i = 0; i < object.getNumChildren(); ++i) {
		IObject child = object.getChild(i);

		if (!child.valid()) {
			continue;
		}

		abc_path = static_cast<AlembicObjectPath *>(
		               MEM_callocN(sizeof(AlembicObjectPath), __func__));

		BLI_strncpy(abc_path->path, child.getFullName().c_str(), PATH_MAX);
		BLI_addtail(object_paths, abc_path);

		gather_objects_paths(child, object_paths);
	}
}

AbcArchiveHandle *ABC_create_handle(const char *filename, ListBase *object_paths)
{
	ArchiveReader *archive = new ArchiveReader(filename);

	if (!archive->valid()) {
		delete archive;
		return NULL;
	}

	if (object_paths) {
		gather_objects_paths(archive->getTop(), object_paths);
	}

	return handle_from_archive(archive);
}

void ABC_free_handle(AbcArchiveHandle *handle)
{
	delete archive_from_handle(handle);
}

int ABC_get_version()
{
	return ALEMBIC_LIBRARY_VERSION;
}

static void find_iobject(const IObject &object, IObject &ret,
                         const std::string &path)
{
	if (!object.valid()) {
		return;
	}

	std::vector<std::string> tokens;
	split(path, '/', tokens);

	IObject tmp = object;

	std::vector<std::string>::iterator iter;
	for (iter = tokens.begin(); iter != tokens.end(); ++iter) {
		IObject child = tmp.getChild(*iter);
		tmp = child;
	}

	ret = tmp;
}

struct ExportJobData {
	Scene *scene;
	Main *bmain;

	char filename[1024];
	ExportSettings settings;

	short *stop;
	short *do_update;
	float *progress;

	bool was_canceled;
};

static void export_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
	ExportJobData *data = static_cast<ExportJobData *>(customdata);

	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;

	/* XXX annoying hack: needed to prevent data corruption when changing
	 * scene frame in separate threads
	 */
	G.is_rendering = true;
	BKE_spacedata_draw_locks(true);

	G.is_break = false;

	try {
		Scene *scene = data->scene;
		AbcExporter exporter(scene, data->filename, data->settings);

		const int orig_frame = CFRA;

		data->was_canceled = false;
		exporter(data->bmain, *data->progress, data->was_canceled);

		if (CFRA != orig_frame) {
			CFRA = orig_frame;

			BKE_scene_update_for_newframe(data->bmain->eval_ctx, data->bmain,
			                              scene, scene->lay);
		}
	}
	catch (const std::exception &e) {
		std::cerr << "Abc Export error: " << e.what() << '\n';
	}
	catch (...) {
		std::cerr << "Abc Export error\n";
	}
}

static void export_endjob(void *customdata)
{
	ExportJobData *data = static_cast<ExportJobData *>(customdata);

	if (data->was_canceled && BLI_exists(data->filename)) {
		BLI_delete(data->filename, false, false);
	}

	G.is_rendering = false;
	BKE_spacedata_draw_locks(false);
}

void ABC_export(
        Scene *scene,
        bContext *C,
        const char *filepath,
        const struct AlembicExportParams *params)
{
	ExportJobData *job = static_cast<ExportJobData *>(MEM_mallocN(sizeof(ExportJobData), "ExportJobData"));
	job->scene = scene;
	job->bmain = CTX_data_main(C);
	BLI_strncpy(job->filename, filepath, 1024);

	job->settings.scene = job->scene;
	job->settings.frame_start = params->frame_start;
	job->settings.frame_end = params->frame_end;
	job->settings.frame_step_xform = params->frame_step_xform;
	job->settings.frame_step_shape = params->frame_step_shape;
	job->settings.shutter_open = params->shutter_open;
	job->settings.shutter_close = params->shutter_close;
	job->settings.selected_only = params->selected_only;
	job->settings.export_face_sets = params->face_sets;
	job->settings.export_normals = params->normals;
	job->settings.export_uvs = params->uvs;
	job->settings.export_vcols = params->vcolors;
	job->settings.apply_subdiv = params->apply_subdiv;
	job->settings.flatten_hierarchy = params->flatten_hierarchy;
	job->settings.visible_layers_only = params->visible_layers_only;
	job->settings.renderable_only = params->renderable_only;
	job->settings.use_subdiv_schema = params->use_subdiv_schema;
	job->settings.export_ogawa = (params->compression_type == ABC_ARCHIVE_OGAWA);
	job->settings.pack_uv = params->packuv;
	job->settings.global_scale = params->global_scale;
	job->settings.triangulate = params->triangulate;
	job->settings.quad_method = params->quad_method;
	job->settings.ngon_method = params->ngon_method;

	if (job->settings.frame_start > job->settings.frame_end) {
		std::swap(job->settings.frame_start, job->settings.frame_end);
	}

	wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
	                            CTX_wm_window(C),
	                            job->scene,
	                            "Alembic Export",
	                            WM_JOB_PROGRESS,
	                            WM_JOB_TYPE_ALEMBIC);

	/* setup job */
	WM_jobs_customdata_set(wm_job, job, MEM_freeN);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
	WM_jobs_callbacks(wm_job, export_startjob, NULL, NULL, export_endjob);

	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/* ********************** Import file ********************** */

enum {
	ABC_NO_ERROR = 0,
	ABC_ARCHIVE_FAIL,
};

struct ImportJobData {
	Main *bmain;
	Scene *scene;

	char filename[1024];
	ImportSettings settings;

	AbcObjectReader *root;

	short *stop;
	short *do_update;
	float *progress;

	char error_code;
	bool was_cancelled;
};

static void import_startjob(void *user_data, short *stop, short *do_update, float *progress)
{
	ImportJobData *data = static_cast<ImportJobData *>(user_data);

	data->stop = stop;
	data->do_update = do_update;
	data->progress = progress;

	ArchiveReader *archive = new ArchiveReader(data->filename);

	if (!archive->valid()) {
		delete archive;
		data->error_code = ABC_ARCHIVE_FAIL;
		return;
	}

	CacheFile *cache_file = static_cast<CacheFile *>(BKE_cachefile_add(data->bmain, BLI_path_basename(data->filename)));

	/* Decrement the ID ref-count because it is going to be incremented for each
	 * modifier and constraint that it will be attached to, so since currently
	 * it is not used by anyone, its use count will off by one. */
	id_us_min(&cache_file->id);

	cache_file->is_sequence = data->settings.is_sequence;
	cache_file->scale = data->settings.scale;
	cache_file->handle = handle_from_archive(archive);
	BLI_strncpy(cache_file->filepath, data->filename, 1024);

	data->settings.cache_file = cache_file;

	*data->do_update = true;
	*data->progress = 0.05f;

	/* Parse Alembic Archive. */
	data->root = new AbcEmptyReader(archive->getTop(), data->settings);

	if (G.is_break) {
		data->was_cancelled = true;
		return;
	}

	*data->do_update = true;
	*data->progress = 0.5f;

	/* Create objects and set scene frame range. */

	data->root->do_read(data->bmain, 0.0f);

	const chrono_t min_time = data->root->minTime();
	const chrono_t max_time = data->root->maxTime();

	if (data->settings.set_frame_range) {
		Scene *scene = data->scene;

		if (data->settings.is_sequence) {
			SFRA = data->settings.offset;
			EFRA = SFRA + (data->settings.sequence_len - 1);
			CFRA = SFRA;
		}
		else if (min_time < max_time) {
			SFRA = min_time * FPS;
			EFRA = max_time * FPS;
			CFRA = SFRA;
		}
	}
}

static void import_endjob(void *user_data)
{
	ImportJobData *data = static_cast<ImportJobData *>(user_data);

	/* Delete objects on cancelation. */
	if (data->was_cancelled) {
		data->root->free_object(data->bmain);
	}
	else {
		/* Add object to scene. */
		BKE_scene_base_deselect_all(data->scene);
		data->root->add_to_scene(data->bmain, data->scene);
		DAG_relations_tag_update(data->bmain);
	}

	data->root->free_all();
	delete data->root;

	switch (data->error_code) {
		default:
		case ABC_NO_ERROR:
			break;
		case ABC_ARCHIVE_FAIL:
			WM_report(RPT_ERROR, "Could not open Alembic archive for reading! See console for detail.");
			break;
	}

	WM_main_add_notifier(NC_SCENE | ND_FRAME, data->scene);
}

static void import_freejob(void *user_data)
{
	ImportJobData *data = static_cast<ImportJobData *>(user_data);
	delete data;
}

void ABC_import(bContext *C, const char *filepath, float scale, bool is_sequence, bool set_frame_range, int sequence_len, int offset, bool validate_meshes)
{
	/* Using new here since MEM_* funcs do not call ctor to properly initialize
	 * data. */
	ImportJobData *job = new ImportJobData();
	job->bmain = CTX_data_main(C);
	job->scene = CTX_data_scene(C);
	BLI_strncpy(job->filename, filepath, 1024);

	job->settings.scale = scale;
	job->settings.is_sequence = is_sequence;
	job->settings.set_frame_range = set_frame_range;
	job->settings.sequence_len = sequence_len;
	job->settings.offset = offset;
	job->settings.validate_meshes = validate_meshes;
	job->error_code = ABC_NO_ERROR;
	job->was_cancelled = false;

	G.is_break = false;

	wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
	                            CTX_wm_window(C),
	                            job->scene,
	                            "Alembic Import",
	                            WM_JOB_PROGRESS,
	                            WM_JOB_TYPE_ALEMBIC);

	/* setup job */
	WM_jobs_customdata_set(wm_job, job, import_freejob);
	WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_FRAME, NC_SCENE | ND_FRAME);
	WM_jobs_callbacks(wm_job, import_startjob, NULL, NULL, import_endjob);

	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

/* ******************************* */

void ABC_get_transform(CacheReader *reader, Object *ob, float r_mat[4][4], float time, float scale, bool is_camera)
{
	if (!reader) {
		return;
	}

	AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);

	IObject tmp = abc_reader->iobject();

	IXform ixform;

	if (IXform::matches(tmp.getHeader())) {
		ixform = IXform(tmp, kWrapExisting);
	}
	else {
		ixform = IXform(tmp.getParent(), kWrapExisting);
	}

	IXformSchema schema = ixform.getSchema();

	if (!schema.valid() || schema.isConstantIdentity()) {
		return;
	}

	ISampleSelector sample_sel(time);

	create_input_transform(sample_sel, ixform, ob, r_mat, scale,
	                       (is_camera) || (ob->type == OB_CAMERA));
}

/* ***************************************** */

static bool check_smooth_poly_flag(DerivedMesh *dm)
{
	MPoly *mpolys = dm->getPolyArray(dm);

	for (int i = 0, e = dm->getNumPolys(dm); i < e; ++i) {
		MPoly &poly = mpolys[i];

		if ((poly.flag & ME_SMOOTH) != 0) {
			return true;
		}
	}

	return false;
}

static void set_smooth_poly_flag(DerivedMesh *dm)
{
	MPoly *mpolys = dm->getPolyArray(dm);

	for (int i = 0, e = dm->getNumPolys(dm); i < e; ++i) {
		MPoly &poly = mpolys[i];
		poly.flag |= ME_SMOOTH;
	}
}

static void *add_customdata_cb(void *user_data, const char *name, int data_type)
{
	DerivedMesh *dm = static_cast<DerivedMesh *>(user_data);
	CustomDataType cd_data_type = static_cast<CustomDataType>(data_type);
	void *cd_ptr = NULL;

	if (ELEM(cd_data_type, CD_MLOOPUV, CD_MLOOPCOL)) {
		cd_ptr = CustomData_get_layer_named(dm->getLoopDataLayout(dm), cd_data_type, name);

		if (cd_ptr == NULL) {
			cd_ptr = CustomData_add_layer_named(dm->getLoopDataLayout(dm),
			                                    cd_data_type,
			                                    CD_DEFAULT,
			                                    NULL,
			                                    dm->getNumLoops(dm),
			                                    name);
		}
	}

	return cd_ptr;
}

ABC_INLINE CDStreamConfig get_config(DerivedMesh *dm)
{
	CDStreamConfig config;

	config.user_data = dm;
	config.mvert = dm->getVertArray(dm);
	config.mloop = dm->getLoopArray(dm);
	config.mpoly = dm->getPolyArray(dm);
	config.totloop = dm->getNumLoops(dm);
	config.totpoly = dm->getNumPolys(dm);
	config.loopdata = dm->getLoopDataLayout(dm);
	config.add_customdata_cb = add_customdata_cb;

	return config;
}

static DerivedMesh *read_mesh_sample(DerivedMesh *dm, const IObject &iobject, const float time, int read_flag)
{
	IPolyMesh mesh(iobject, kWrapExisting);
	IPolyMeshSchema schema = mesh.getSchema();
	ISampleSelector sample_sel(time);
	const IPolyMeshSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();
	const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
	const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

	DerivedMesh *new_dm = NULL;

	/* Only read point data when streaming meshes, unless we need to create new ones. */
	ImportSettings settings;
	settings.read_flag |= read_flag;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_from_template(dm,
		                            positions->size(),
		                            0,
		                            0,
		                            face_indices->size(),
		                            face_counts->size());

		settings.read_flag |= MOD_MESHSEQ_READ_ALL;
	}

	CDStreamConfig config = get_config(new_dm ? new_dm : dm);
	config.time = time;

	bool do_normals = false;
	read_mesh_sample(&settings, schema, sample_sel, config, do_normals);

	if (new_dm) {
		/* Check if we had ME_SMOOTH flag set to restore it. */
		if (!do_normals && check_smooth_poly_flag(dm)) {
			set_smooth_poly_flag(new_dm);
		}

		CDDM_calc_normals(new_dm);
		CDDM_calc_edges(new_dm);

		return new_dm;
	}

	if (do_normals) {
		CDDM_calc_normals(dm);
	}

	return dm;
}

using Alembic::AbcGeom::ISubDSchema;

static DerivedMesh *read_subd_sample(DerivedMesh *dm, const IObject &iobject, const float time, int read_flag)
{
	ISubD mesh(iobject, kWrapExisting);
	ISubDSchema schema = mesh.getSchema();
	ISampleSelector sample_sel(time);
	const ISubDSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();
	const Alembic::Abc::Int32ArraySamplePtr &face_indices = sample.getFaceIndices();
	const Alembic::Abc::Int32ArraySamplePtr &face_counts = sample.getFaceCounts();

	DerivedMesh *new_dm = NULL;

	ImportSettings settings;
	settings.read_flag |= read_flag;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_from_template(dm,
		                            positions->size(),
		                            0,
		                            0,
		                            face_indices->size(),
		                            face_counts->size());

		settings.read_flag |= MOD_MESHSEQ_READ_ALL;
	}

	/* Only read point data when streaming meshes, unless we need to create new ones. */
	CDStreamConfig config = get_config(new_dm ? new_dm : dm);
	config.time = time;
	read_subd_sample(&settings, schema, sample_sel, config);

	if (new_dm) {
		/* Check if we had ME_SMOOTH flag set to restore it. */
		if (check_smooth_poly_flag(dm)) {
			set_smooth_poly_flag(new_dm);
		}

		CDDM_calc_normals(new_dm);
		CDDM_calc_edges(new_dm);

		return new_dm;
	}

	return dm;
}

static DerivedMesh *read_points_sample(DerivedMesh *dm, const IObject &iobject, const float time)
{
	IPoints points(iobject, kWrapExisting);
	IPointsSchema schema = points.getSchema();
	ISampleSelector sample_sel(time);
	const IPointsSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();

	DerivedMesh *new_dm = NULL;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_new(positions->size(), 0, 0, 0, 0);
	}

	CDStreamConfig config = get_config(new_dm ? new_dm : dm);
	read_points_sample(schema, sample_sel, config, time);

	return new_dm ? new_dm : dm;
}

/* NOTE: Alembic only stores data about control points, but the DerivedMesh
 * passed from the cache modifier contains the displist, which has more data
 * than the control points, so to avoid corrupting the displist we modify the
 * object directly and create a new DerivedMesh from that. Also we might need to
 * create new or delete existing NURBS in the curve.
 */
static DerivedMesh *read_curves_sample(Object *ob, const IObject &iobject, const float time)
{
	ICurves points(iobject, kWrapExisting);
	ICurvesSchema schema = points.getSchema();
	ISampleSelector sample_sel(time);
	const ICurvesSchema::Sample sample = schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();
	const Int32ArraySamplePtr num_vertices = sample.getCurvesNumVertices();

	int vertex_idx = 0;
	int curve_idx = 0;
	Curve *curve = static_cast<Curve *>(ob->data);

	const int curve_count = BLI_listbase_count(&curve->nurb);

	if (curve_count != num_vertices->size()) {
		BKE_nurbList_free(&curve->nurb);
		read_curve_sample(curve, schema, time);
	}
	else {
		Nurb *nurbs = static_cast<Nurb *>(curve->nurb.first);
		for (; nurbs; nurbs = nurbs->next, ++curve_idx) {
			const int totpoint = (*num_vertices)[curve_idx];

			if (nurbs->bp) {
				BPoint *point = nurbs->bp;

				for (int i = 0; i < totpoint; ++i, ++point, ++vertex_idx) {
					const Imath::V3f &pos = (*positions)[vertex_idx];
					copy_yup_zup(point->vec, pos.getValue());
				}
			}
			else if (nurbs->bezt) {
				BezTriple *bezier = nurbs->bezt;

				for (int i = 0; i < totpoint; ++i, ++bezier, ++vertex_idx) {
					const Imath::V3f &pos = (*positions)[vertex_idx];
					copy_yup_zup(bezier->vec[1], pos.getValue());
				}
			}
		}
	}

	return CDDM_from_curve(ob);
}

DerivedMesh *ABC_read_mesh(CacheReader *reader,
                           Object *ob,
                           DerivedMesh *dm,
                           const float time,
                           const char **err_str,
                           int read_flag)
{
	AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);

	IObject iobject = abc_reader->iobject();

	if (!iobject.valid()) {
		*err_str = "Invalid object: verify object path";
		return NULL;
	}

	const ObjectHeader &header = iobject.getHeader();

	if (IPolyMesh::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a mesh!";
			return NULL;
		}

		return read_mesh_sample(dm, iobject, time, read_flag);
	}
	else if (ISubD::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a subdivision mesh!";
			return NULL;
		}

		return read_subd_sample(dm, iobject, time, read_flag);
	}
	else if (IPoints::matches(header)) {
		if (ob->type != OB_MESH) {
			*err_str = "Object type mismatch: object path points to a point cloud (requires a mesh object)!";
			return NULL;
		}

		return read_points_sample(dm, iobject, time);
	}
	else if (ICurves::matches(header)) {
		if (ob->type != OB_CURVE) {
			*err_str = "Object type mismatch: object path points to a curve!";
			return NULL;
		}

		return read_curves_sample(ob, iobject, time);
	}

	*err_str = "Unsupported object type: verify object path"; // or poke developer
	return NULL;
}

/* ***************************************** */

void CacheReader_free(CacheReader *reader)
{
	AbcObjectReader *abc_reader = reinterpret_cast<AbcObjectReader *>(reader);
	abc_reader->decref();

	if (abc_reader->refcount() == 0) {
		delete abc_reader;
	}
}

CacheReader *CacheReader_open_alembic_object(AbcArchiveHandle *handle, CacheReader *reader, const char *object_path)
{
	if (object_path[0] == '\0') {
		return reader;
	}

	ArchiveReader *archive = archive_from_handle(handle);

	if (!archive || !archive->valid()) {
		return reader;
	}

	IObject iobject;
	find_iobject(archive->getTop(), iobject, object_path);

	if (reader) {
		CacheReader_free(reader);
	}

	ImportSettings settings;
	AbcObjectReader *abc_reader = create_reader(iobject, settings);
	abc_reader->incref();

	return reinterpret_cast<CacheReader *>(abc_reader);
}
