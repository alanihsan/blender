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

#ifndef __ABC_CAMERA_WRITER_H__
#define __ABC_CAMERA_WRITER_H__

#include "abc_shape.h"

class AbcCameraWriter : public AbcShapeWriter {
	Alembic::AbcGeom::OCameraSchema m_camera_schema;
    Alembic::AbcGeom::CameraSample m_camera_sample;
    Alembic::AbcGeom::OCompoundProperty m_custom_data_container;
    Alembic::AbcGeom::OFloatProperty m_stereo_distance;
    Alembic::AbcGeom::OFloatProperty m_eye_separation;

public:
	AbcCameraWriter(Scene *scene,
	                Object *ob,
	                AbcTransformWriter *parent,
	                uint32_t timeSampling,
	                ExportSettings &settings);

private:
	virtual void do_write();
};

class AbcCameraReader : public AbcObjectReader {
	Alembic::AbcGeom::ICameraSchema m_schema;

public:
	AbcCameraReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

	bool valid() const;

	void readObjectData(Main *bmain, Scene *scene, float time);
};

#endif  /* __ABC_CAMERA_WRITER_H__ */