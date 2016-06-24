﻿/*
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

#include "abc_util.h"

extern "C" {
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

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
}

using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IXform;
using Alembic::AbcGeom::IXformSchema;

using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::ODoubleArrayProperty;
using Alembic::AbcGeom::ODoubleProperty;
using Alembic::AbcGeom::OFloatArrayProperty;
using Alembic::AbcGeom::OFloatProperty;
using Alembic::AbcGeom::OInt32ArrayProperty;
using Alembic::AbcGeom::OInt32Property;
using Alembic::AbcGeom::OStringArrayProperty;
using Alembic::AbcGeom::OStringProperty;

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

AbcObjectReader::AbcObjectReader(const IObject &object, ImportSettings &settings)
    : m_name("")
    , m_object_name("")
    , m_data_name("")
    , m_object(NULL)
    , m_iobject(object)
    , m_settings(&settings)
    , m_min_time(std::numeric_limits<chrono_t>::max())
    , m_max_time(std::numeric_limits<chrono_t>::min())
{
	m_name = object.getFullName();
	std::vector<std::string> parts;
	split(m_name, '/', parts);

	m_object_name = m_data_name = parts[parts.size() - 1];
}

AbcObjectReader::~AbcObjectReader()
{}

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

	if (IXform::matches(m_iobject.getMetaData())) {
		ixform = IXform(m_iobject, Alembic::AbcGeom::kWrapExisting);
	}
	else if (IXform::matches(m_iobject.getParent().getMetaData())) {
		ixform = IXform(m_iobject.getParent(), Alembic::AbcGeom::kWrapExisting);
	}
	else {
		return;
	}

	const IXformSchema &schema(ixform.getSchema());

	if (!schema.valid()) {
		return;
	}

	Alembic::AbcGeom::ISampleSelector sample_sel(time);
	Alembic::AbcGeom::XformSample xs;
	schema.get(xs, sample_sel);

	create_input_transform(sample_sel, ixform, m_object, m_object->obmat, m_settings->scale);

	invert_m4_m4(m_object->imat, m_object->obmat);

	BKE_object_apply_mat4(m_object, m_object->obmat, false,  false);

	if (!schema.isConstant()) {
		bConstraint *con = BKE_constraint_add_for_object(m_object, NULL, CONSTRAINT_TYPE_TRANSFORMCACHE);
		bTransformCacheConstraint *data = static_cast<bTransformCacheConstraint *>(con->data);
		BLI_strncpy(data->abc_object_path, m_iobject.getFullName().c_str(), FILE_MAX);

		data->cache_file = m_settings->cache_file;
		id_us_plus(&data->cache_file->id);
	}
}

void AbcObjectReader::addDefaultModifier(Main *bmain) const
{
	ModifierData *md = modifier_new(eModifierType_MeshSequenceCache);
	BLI_addtail(&m_object->modifiers, md);

	MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

	mcmd->cache_file = m_settings->cache_file;
	id_us_plus(&mcmd->cache_file->id);

	BLI_strncpy(mcmd->abc_object_path, m_iobject.getFullName().c_str(), FILE_MAX);

	DAG_id_tag_update(&m_object->id, OB_RECALC_DATA);
	DAG_relations_tag_update(bmain);
}

chrono_t AbcObjectReader::minTime() const
{
	return m_min_time;
}

chrono_t AbcObjectReader::maxTime() const
{
	return m_max_time;
}
