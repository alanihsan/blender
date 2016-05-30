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

#include "abc_exporter.h"

#include <cmath>

#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/AbcCoreOgawa/All.h>

#include "abc_camera.h"
#include "abc_mesh.h"
#include "abc_nurbs.h"
#include "abc_hair.h"
#include "abc_util.h"

extern "C" {
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"  /* for FILE_MAX */

#include "BLI_string.h"

#ifdef WIN32
/* needed for MSCV because of snprintf from BLI_string */
#include "BLI_winstuff.h"
#endif

#include "BKE_anim.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
}

using Alembic::Abc::TimeSamplingPtr;
using Alembic::Abc::OBox3dProperty;

AbcExporter::AbcExporter(Scene *scene, const char *filename, ExportSettings &settings)
    : m_settings(settings)
    , m_filename(filename)
    , m_scene(scene)
    , m_saved_frame(getCurrentFrame())
{}

AbcExporter::~AbcExporter()
{
	for (std::map<std::string, AbcTransformWriter*>::iterator it = m_xforms.begin(), e = m_xforms.end(); it != e; ++it)
		delete it->second;

	for (int i = 0, e = m_shapes.size(); i != e; ++i) {
		delete m_shapes[i];
	}

	if (getCurrentFrame() != m_saved_frame) {
		setCurrentFrame(m_saved_frame);
	}
}

void AbcExporter::getShutterSamples(double step, bool time_relative,
                                    std::vector<double> &samples)
{
	samples.clear();

	const double time_factor = time_relative ? m_scene->r.frs_sec : 1.0;
	const double shutter_open = m_settings.shutter_open;
	const double shutter_close = m_settings.shutter_close;

	/* sample all frame */
	if (shutter_open == 0.0 && shutter_close == 1.0) {
		for (double t = 0; t < 1.0; t += step) {
			samples.push_back(t / time_factor);
		}
	}
	else {
		/* sample between shutter open & close */
		const int nsamples = std::max((1.0 / step) - 1.0, 1.0);
		const double time_inc = (shutter_close - shutter_open) / nsamples;

		for (double t = shutter_open; t <= shutter_close; t += time_inc) {
			samples.push_back(t / time_factor);
		}
	}
}

Alembic::Abc::TimeSamplingPtr AbcExporter::createTimeSampling(double step)
{
	TimeSamplingPtr time_sampling;
	std::vector<double> samples;

	if (m_settings.startframe == m_settings.endframe) {
		time_sampling.reset(new Alembic::Abc::TimeSampling());
		return time_sampling;
	}

	getShutterSamples(step, true, samples);

	Alembic::Abc::TimeSamplingType ts(static_cast<uint32_t>(samples.size()), 1.0 / m_scene->r.frs_sec);
	time_sampling.reset(new Alembic::Abc::TimeSampling(ts, samples));

	return time_sampling;
}

void AbcExporter::getFrameSet(double step, std::set<double> &frames)
{
	frames.clear();

	std::vector<double> shutter_samples;

	getShutterSamples(step, false, shutter_samples);

	for (int frame = m_settings.startframe; frame <= m_settings.endframe; ++frame) {
		for (int j = 0, e = shutter_samples.size(); j < e; ++j) {
			frames.insert(frame + shutter_samples[j]);
		}
	}
}

void AbcExporter::operator()(float &progress)
{
	/* Create archive here */
	std::string scene_name;

	if (G.main->name[0] != '\0') {
		char sceneFileName[FILE_MAX];
		BLI_strncpy(sceneFileName, G.main->name, FILE_MAX);
		scene_name = sceneFileName;
	}
	else {
		scene_name = "untitled";
	}

	Scene *scene = m_scene;
	const int fps = FPS;
	char buf[16];
	snprintf(buf, 15, "%d", fps);
	const std::string str_fps = buf;

	Alembic::AbcCoreAbstract::MetaData md;
	md.set("FramesPerTimeUnit", str_fps);

	Alembic::Abc::Argument arg(md);

	if (!m_settings.export_ogawa) {
		m_archive = Alembic::Abc::CreateArchiveWithInfo(Alembic::AbcCoreHDF5::WriteArchive(), m_filename, "Blender",
		                                               scene_name, Alembic::Abc::ErrorHandler::kThrowPolicy, arg);
	}
	else {
		m_archive = Alembic::Abc::CreateArchiveWithInfo(Alembic::AbcCoreOgawa::WriteArchive(), m_filename, "Blender",
		                                               scene_name, Alembic::Abc::ErrorHandler::kThrowPolicy, arg);
	}

	/* Create time samplings for transforms and shapes */
	TimeSamplingPtr trans_time = createTimeSampling(m_settings.xform_frame_step);

	m_trans_sampling_index = m_archive.addTimeSampling(*trans_time);

	TimeSamplingPtr shape_time;

	if ((m_settings.shape_frame_step == m_settings.xform_frame_step) ||
	    (m_settings.startframe == m_settings.endframe))
	{
		shape_time = trans_time;
		m_shape_sampling_index = m_trans_sampling_index;
	}
	else {
		shape_time = createTimeSampling(m_settings.shape_frame_step);
		m_shape_sampling_index = m_archive.addTimeSampling(*shape_time);
	}

	OBox3dProperty archive_bounds_prop = Alembic::AbcGeom::CreateOArchiveBounds(m_archive, m_trans_sampling_index);

	if (m_settings.flatten_hierarchy) {
		createTransformWritersFlat();
	}
	else {
		createTransformWritersHierarchy();
	}

	createShapeWriters();

	/* make a list of frames to export */
	std::set<double> xform_frames;
	getFrameSet(m_settings.xform_frame_step, xform_frames);

	std::set<double> shape_frames;
	getFrameSet(m_settings.shape_frame_step, shape_frames);

	/* merge all frames needed */
	std::set<double> frames(xform_frames);
	frames.insert(shape_frames.begin(), shape_frames.end());

	/* export all frames */

	std::set<double>::const_iterator begin = frames.begin();
	std::set<double>::const_iterator end = frames.end();

	const float size = static_cast<float>(frames.size());
	size_t i = 0;

	for (; begin != end; ++begin) {
		progress = (++i / size);

		double f = *begin;
		setCurrentFrame(f);

		if (shape_frames.count(f) != 0) {
			for (int i = 0, e = m_shapes.size(); i != e; ++i) {
				m_shapes[i]->write();
			}
		}

		if (xform_frames.count(f) == 0) {
			continue;
		}

		std::map<std::string, AbcTransformWriter *>::iterator xit, xe;
		for (xit = m_xforms.begin(), xe = m_xforms.end(); xit != xe; ++xit) {
			xit->second->write();
		}

		/* Save the archive 's bounding box. */
		Imath::Box3d bounds;

		for (xit = m_xforms.begin(), xe = m_xforms.end(); xit != xe; ++xit) {
			Imath::Box3d box = xit->second->bounds();
			bounds.extendBy(box);
		}

		archive_bounds_prop.set(bounds);
	}
}

void AbcExporter::createTransformWritersHierarchy()
{
	Base *base = static_cast<Base *>(m_scene->base.first);

	while (base) {
		Object *ob = base->object;

		if (m_settings.exportObject(ob)) {
			switch(ob->type) {
				case OB_LAMP:
				case OB_LATTICE:
				case OB_MBALL:
				case OB_SPEAKER:
					/* we do not export transforms for objects of these classes */
					break;

				default:
					exploreTransform(ob, ob->parent, NULL);
			}
		}

		base = base->next;
	}
}

void AbcExporter::createTransformWritersFlat()
{
	Base *base = static_cast<Base *>(m_scene->base.first);

	while (base) {
		Object *ob = base->object;

		if (m_settings.exportObject(ob) && objectIsShape(ob)) {
			std::string name = get_id_name(ob);
			m_xforms[name] = new AbcTransformWriter(ob, m_archive.getTop(), 0, m_trans_sampling_index, m_settings);
		}

		base = base->next;
	}
}

void AbcExporter::exploreTransform(Object *ob, Object *parent, Object *dupliObParent)
{
	createTransformWriter(ob, parent, dupliObParent);

	ListBase *lb = object_duplilist(G.main->eval_ctx, m_scene, ob);

	if (lb) {
		DupliObject *link = static_cast<DupliObject *>(lb->first);
		Object *dupli_ob = NULL;
		Object *dupli_parent = NULL;
		
		while (link) {
			dupli_ob = link->ob;

			if (dupli_ob->parent)
				dupli_parent = dupli_ob->parent;
			else
				dupli_parent = ob;

			if (link->type == OB_DUPLIGROUP)
				exploreTransform(dupli_ob, dupli_parent, ob);

			link = link->next;
		}
	}

	free_object_duplilist(lb);
}

void AbcExporter::createTransformWriter(Object *ob, Object *parent, Object *dupliObParent)
{
	const std::string name = get_object_dag_path_name(ob, dupliObParent);

	/* check if we have already created a transform writer for this object */
	if (m_xforms.find(name) != m_xforms.end()){
		std::cerr << "xform " << name << " already exists\n";
		return;
	}

	AbcTransformWriter *parent_xform = NULL;

	if (parent) {
		const std::string parentname = get_object_dag_path_name(parent, dupliObParent);
		parent_xform = getXForm(parentname);

		if (!parent_xform) {
			if (parent->parent) {
				createTransformWriter(parent, parent->parent, dupliObParent);
			}
			else {
				createTransformWriter(parent, dupliObParent, dupliObParent);
			}

			parent_xform = getXForm(parentname);
		}
	}

	if (parent_xform) {
		m_xforms[name] = new AbcTransformWriter(ob, parent_xform->alembicXform(), parent_xform, m_trans_sampling_index, m_settings);
		m_xforms[name]->setParent(parent);
	}
	else {
		m_xforms[name] = new AbcTransformWriter(ob, m_archive.getTop(), NULL, m_trans_sampling_index, m_settings);
	}
}

void AbcExporter::createShapeWriters()
{
	Base *base = static_cast<Base *>(m_scene->base.first);

	while (base) {
		Object *ob = base->object;
		exploreObject(ob, NULL);

		base = base->next;
	}
}

void AbcExporter::exploreObject(Object *ob, Object *dupliObParent)
{
	ListBase *lb = object_duplilist(G.main->eval_ctx, m_scene, ob);
	
	createShapeWriter(ob, dupliObParent);
	
	if (lb) {
		DupliObject *link = static_cast<DupliObject *>(lb->first);
		Object *dupliob = NULL;

		while (link) {
			dupliob = link->ob;

			if (link->type == OB_DUPLIGROUP) {
				exploreObject(dupliob, ob);
			}

			link = link->next;
		}
	}

	free_object_duplilist(lb);
}

void AbcExporter::createShapeWriter(Object *ob, Object *dupliObParent)
{
	if (!objectIsShape(ob)) {
		return;
	}

	if (!m_settings.exportObject(ob)) {
		return;
	}

	std::string name = get_object_dag_path_name(ob, dupliObParent);
	
	AbcTransformWriter *xform = getXForm(name);

	if (!xform) {
		std::cerr << __func__ << ": xform " << name << "is NULL\n";
		return;
	}

	int enable_hair = true;
	int enable_hair_child = true;
	int enable_geo = true;

	ID *id = reinterpret_cast<ID *>(ob);
	IDProperty *xport_props = IDP_GetProperties(id, 0);

	/* Check for special export object flags */
	if (xport_props) {
		IDProperty *enable_prop = IDP_GetPropertyFromGroup(xport_props, "abc_hair");
		if (enable_prop) {
			enable_hair = IDP_Int(enable_prop);
		}

		enable_prop = IDP_GetPropertyFromGroup(xport_props, "abc_geo");
		if (enable_prop) {
			if (IDP_Int(enable_prop) == 2) {
				enable_geo = false;
			}
			else {
				enable_geo = IDP_Int(enable_prop);
			}
		}

		enable_prop = IDP_GetPropertyFromGroup(xport_props, "abc_hair_child");
		if (enable_prop) {
			enable_hair_child = IDP_Int(enable_prop);
		}
	}

	ParticleSystem *psys = static_cast<ParticleSystem *>(ob->particlesystem.first);

	for (; psys; psys = psys->next) {
		if (!psys_check_enabled(ob, psys))
			continue;

		if (enable_hair && psys->part && (psys->part->type == PART_HAIR)) {
			m_settings.export_child_hairs = enable_hair_child;
			m_shapes.push_back(new AbcHairWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings, psys));
		}
	}

	switch(ob->type) {
		case OB_MESH:
		{
			if (enable_geo) {
				Mesh *me = static_cast<Mesh *>(ob->data);

				if (!me || me->totvert == 0) {
					return;
				}

				m_shapes.push_back(new AbcMeshWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			}

			break;
		}
		case OB_SURF:
		{
			if (enable_geo) {
				Curve *cu = static_cast<Curve *>(ob->data);

				if (!cu) {
					return;
				}

				m_shapes.push_back(new AbcNurbsWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			}

			break;
		}
		case OB_CAMERA:
		{
			Camera *cam = static_cast<Camera *>(ob->data);

			if (cam->type == CAM_PERSP) {
				m_shapes.push_back(new AbcCameraWriter(m_scene, ob, xform, m_shape_sampling_index, m_settings));
			}

			break;
		}
	}
}

AbcTransformWriter *AbcExporter::getXForm(const std::string &name)
{
	std::map<std::string, AbcTransformWriter *>::iterator it = m_xforms.find(name);

	if (it == m_xforms.end()) {
		return NULL;
	}

	return it->second;
}

double AbcExporter::getCurrentFrame() const
{
	return m_scene->r.cfra + m_scene->r.subframe;
}

void AbcExporter::setCurrentFrame(double t)
{
	m_scene->r.cfra = std::floor(t);
	m_scene->r.subframe = t - m_scene->r.cfra;
	BKE_scene_update_for_newframe(G.main->eval_ctx, G.main, m_scene, (1<<20)-1);
}

bool AbcExporter::objectIsShape(Object *ob)
{
	switch(ob->type) {
		case OB_MESH:
			if (objectIsSmokeSim(ob)) {
				return false;
			}

			return true;
			break;
		case OB_SURF:
		case OB_CAMERA:
			return true;
		default:
			return false;
	}
}

bool AbcExporter::objectIsSmokeSim(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Smoke);

	if (md) {
		SmokeModifierData *smd = reinterpret_cast<SmokeModifierData *>(md);
		return (smd->type == MOD_SMOKE_TYPE_DOMAIN);
	}

	return false;
}