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

#include "abc_object.h"

#include "abc_camera.h"
#include "abc_curves.h"
#include "abc_hair.h"
#include "abc_mesh.h"
#include "abc_nurbs.h"
#include "abc_points.h"
#include "abc_transform.h"
#include "abc_util.h"

#include <Alembic/AbcMaterial/IMaterial.h>

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"  /* for FILE_MAX */

#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
}

using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IXform;
using Alembic::AbcGeom::IXformSchema;
using Alembic::AbcGeom::MetaData;

using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICurves;
using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::ILight;
using Alembic::AbcGeom::INuPatch;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::IXform;

using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::ODoubleArrayProperty;
using Alembic::AbcGeom::ODoubleProperty;
using Alembic::AbcGeom::OFloatArrayProperty;
using Alembic::AbcGeom::OFloatProperty;
using Alembic::AbcGeom::OInt32ArrayProperty;
using Alembic::AbcGeom::OInt32Property;
using Alembic::AbcGeom::OStringArrayProperty;
using Alembic::AbcGeom::OStringProperty;

using Alembic::AbcMaterial::IMaterial;

/* ************************************************************************** */

AbcObjectWriter::AbcObjectWriter(Scene *scene,
                                 Object *ob,
                                 uint32_t time_sampling,
                                 ExportSettings &settings,
                                 AbcObjectWriter *parent)
    : m_object(ob)
    , m_settings(settings)
    , m_scene(scene)
    , m_time_sampling(time_sampling)
    , m_first_frame(true)
{
	m_name = get_id_name(m_object) + "Shape";

	if (parent) {
		parent->addChild(this);
	}
}

AbcObjectWriter::~AbcObjectWriter()
{}

void AbcObjectWriter::addChild(AbcObjectWriter *child)
{
	m_children.push_back(child);
}

Imath::Box3d AbcObjectWriter::bounds()
{
	BoundBox *bb = BKE_object_boundbox_get(this->m_object);

	if (!bb) {
		if (this->m_object->type != OB_CAMERA) {
			std::cerr << "Boundbox is null!\n";
		}

		return Imath::Box3d();
	}

	/* Convert Z-up to Y-up. */
	this->m_bounds.min.x = bb->vec[0][0];
	this->m_bounds.min.y = bb->vec[0][2];
	this->m_bounds.min.z = -bb->vec[0][1];

	this->m_bounds.max.x = bb->vec[6][0];
	this->m_bounds.max.y = bb->vec[6][2];
	this->m_bounds.max.z = -bb->vec[6][1];

	return this->m_bounds;
}

void AbcObjectWriter::write()
{
	do_write();
	m_first_frame = false;
}

/* ************************************************************************** */

//#define USE_NURBS

AbcObjectReader::AbcObjectReader(const IObject &object, ImportSettings &settings)
    : m_name("")
    , m_object_name("")
    , m_data_name("")
    , m_object(NULL)
    , m_iobject(object)
    , m_parent(NULL)
    , m_settings(&settings)
    , m_min_time(std::numeric_limits<chrono_t>::max())
    , m_max_time(std::numeric_limits<chrono_t>::min())
{
	if (!m_iobject) {
		return;
	}

	m_name = object.getFullName();

	if (m_name != "/") {
		std::vector<std::string> parts;
		split(m_name, '/', parts);

		if (parts.size() >= 2) {
			m_object_name = parts[parts.size() - 2];
			m_data_name = parts[parts.size() - 1];
		}
		else {
			m_object_name = m_data_name = parts[parts.size() - 1];
		}
	}

	size_t num_children = m_iobject.getNumChildren();
	for (size_t i = 0; i < num_children; ++i) {
		IObject child = object.getChild(i);

		if (!child.valid()) {
			continue;
		}

		AbcObjectReader *reader = NULL;
		const MetaData &md = child.getMetaData();

		if (IXform::matches(md)) {
			reader = new AbcEmptyReader(child, settings);
		}
		else if (IPolyMesh::matches(md)) {
			reader = new AbcMeshReader(child, settings);
		}
		else if (ISubD::matches(md)) {
			reader = new AbcSubDReader(child, settings);
		}
		else if (INuPatch::matches(md)) {
#ifdef USE_NURBS
			/* TODO(kevin): importing cyclic NURBS from other software crashes
			 * at the moment. This is due to the fact that NURBS in other
			 * software have duplicated points which causes buffer overflows in
			 * Blender. Need to figure out exactly how these points are
			 * duplicated, in all cases (cyclic U, cyclic V, and cyclic UV).
			 * Until this is fixed, disabling NURBS reading. */
			reader = new AbcNurbsReader(child, settings);
#endif
		}
		else if (ICamera::matches(md)) {
			reader = new AbcCameraReader(child, settings);
		}
		else if (IPoints::matches(md)) {
			reader = new AbcPointsReader(child, settings);
		}
		else if (IMaterial::matches(md)) {
			/* Pass for now. */
		}
		else if (ILight::matches(md)) {
			/* Pass for now. */
		}
		else if (IFaceSet::matches(md)) {
			/* Pass, those are handled in the mesh reader. */
		}
		else if (ICurves::matches(md)) {
			reader = new AbcCurveReader(child, settings);
		}
		else {
			assert(false);
		}

		if (reader) {
			m_children.push_back(reader);
			reader->parent(this);

			m_min_time = std::min(m_min_time, reader->minTime());
			m_max_time = std::max(m_max_time, reader->maxTime());

			AlembicObjectPath *abc_path = static_cast<AlembicObjectPath *>(
			                                  MEM_callocN(sizeof(AlembicObjectPath), "AlembicObjectPath"));

			BLI_strncpy(abc_path->path, child.getFullName().c_str(), PATH_MAX);

			BLI_addtail(&m_settings->cache_file->object_paths, abc_path);
		}
	}
}

AbcObjectReader::~AbcObjectReader()
{
	std::vector<AbcObjectReader *>::iterator child_iter;
	for (child_iter = m_children.begin(); child_iter != m_children.end(); ++child_iter) {
		delete (*child_iter);
	}
}

const IObject &AbcObjectReader::iobject() const
{
	return m_iobject;
}

Object *AbcObjectReader::object() const
{
	return m_object;
}

void AbcObjectReader::readObjectMatrix(const float time)
{
	IXform ixform;
	bool is_camera = false;

	if (IXform::matches(m_iobject.getMetaData())) {
		ixform = IXform(m_iobject, Alembic::AbcGeom::kWrapExisting);
	}
	else {
		unit_m4(m_object->obmat);
		invert_m4_m4(m_object->imat, m_object->obmat);

		/* Cameras need to be rotated so redo the transform for its parent. */
		if (m_object->type != OB_CAMERA) {
			return;
		}

		is_camera = true;
		ixform = IXform(m_parent->iobject(), Alembic::AbcGeom::kWrapExisting);
	}

	const IXformSchema &schema(ixform.getSchema());

	if (!schema.valid() || schema.isConstantIdentity()) {
		return;
	}

	Alembic::AbcGeom::ISampleSelector sample_sel(time);
	Alembic::AbcGeom::XformSample xs;
	schema.get(xs, sample_sel);

	Object *ob = (is_camera) ? m_parent->object() : m_object;

	create_input_transform(sample_sel, ixform, ob, ob->obmat, m_settings->scale, is_camera);

	invert_m4_m4(ob->imat, ob->obmat);

	BKE_object_apply_mat4(ob, ob->obmat, false,  false);

	/* Make sure the constraint is only added once, to the parent object. */
	if (!schema.isConstant()) {
		if (is_camera) {
			bConstraint *con = BKE_constraints_find_name(&m_parent->object()->constraints, "Transform Cache");
			bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
			data->is_camera = true;
		}
		else {
			bConstraint *con = BKE_constraint_add_for_object(m_object, NULL, CONSTRAINT_TYPE_TRANSFORM_CACHE);
			bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
			BLI_strncpy(data->object_path, m_iobject.getFullName().c_str(), FILE_MAX);

			data->cache_file = m_settings->cache_file;
			id_us_plus(&data->cache_file->id);
		}
	}
}

void AbcObjectReader::addCacheModifier() const
{
	ModifierData *md = modifier_new(eModifierType_MeshSequenceCache);
	BLI_addtail(&m_object->modifiers, md);

	MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

	mcmd->cache_file = m_settings->cache_file;
	id_us_plus(&mcmd->cache_file->id);

	BLI_strncpy(mcmd->object_path, m_iobject.getFullName().c_str(), FILE_MAX);
}

void AbcObjectReader::parent(AbcObjectReader *reader)
{
	m_parent = reader;
}

void AbcObjectReader::do_read(Main *bmain, float time)
{
	if (this->valid()) {
		this->readObjectData(bmain, time);

		if (m_parent && m_parent->object()) {
			m_object->parent = m_parent->object();
		}

		this->readObjectMatrix(time);
	}

	std::vector<AbcObjectReader *>::iterator child_iter;
	for (child_iter = m_children.begin(); child_iter != m_children.end(); ++child_iter) {
		(*child_iter)->do_read(bmain, time);
	}
}

void AbcObjectReader::free_object(Main *bmain)
{
	Object *ob = m_object;

	if (ob) {
		if (ob->data) {
			BKE_libblock_free_us(bmain, ob->data);
			ob->data = NULL;
		}

		BKE_libblock_free_us(bmain, ob);
	}

	std::vector<AbcObjectReader *>::iterator child_iter;
	for (child_iter = m_children.begin(); child_iter != m_children.end(); ++child_iter) {
		(*child_iter)->free_object(bmain);
	}
}

void AbcObjectReader::add_to_scene(Main *bmain, Scene *scene)
{
	Object *ob = m_object;

	if (ob) {
		ob->lay = scene->lay;

		BKE_scene_base_add(scene, ob);

		DAG_id_tag_update_ex(bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	}

	std::vector<AbcObjectReader *>::iterator child_iter;
	for (child_iter = m_children.begin(); child_iter != m_children.end(); ++child_iter) {
		(*child_iter)->add_to_scene(bmain, scene);
	}
}

chrono_t AbcObjectReader::minTime() const
{
	return m_min_time;
}

chrono_t AbcObjectReader::maxTime() const
{
	return m_max_time;
}
