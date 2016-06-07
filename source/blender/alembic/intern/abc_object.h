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

#pragma once

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

#include "abc_export_options.h"

extern "C" {
#include "DNA_ID.h"
}

class AbcTransformWriter;

struct Main;

/* ************************************************************************** */

class AbcObjectWriter {
protected:
	Object *m_object;
    ExportSettings &m_settings;

	Scene *m_scene;
	uint32_t m_time_sampling;

    Imath::Box3d m_bounds;
    std::vector<AbcObjectWriter *> m_children;

    std::vector< std::pair<std::string, IDProperty *> > m_props;

    bool m_first_frame;
	std::string m_name;

public:
	AbcObjectWriter(Scene *scene,
	                Object *ob,
	                uint32_t sampling_time,
	                ExportSettings &settings,
	                AbcObjectWriter *parent = NULL);

    virtual ~AbcObjectWriter();

	void addChild(AbcObjectWriter *child);

    virtual Imath::Box3d bounds();

    void write();

private:
    virtual void do_write() = 0;

	void getAllProperties(IDProperty *group, std::vector<std::pair<std::string, IDProperty*> > &allProps, const std::string &parent);

	void writeArrayProperty(IDProperty *p, const Alembic::Abc::OCompoundProperty &abcProps);
	void writeProperty(IDProperty *p, const std::string &name, const Alembic::Abc::OCompoundProperty &abcProps);
	void writeGeomProperty(IDProperty *p, const std::string &name, const Alembic::Abc::OCompoundProperty &abcProps);

protected:
	bool hasProperties(ID *id);
	void writeProperties(ID *id, const Alembic::Abc::OCompoundProperty &props, bool writeAsUserData = true);

	bool getPropertyValue(ID *id, const std::string &name, double &val);
};

/* ************************************************************************** */

struct ImportSettings {
	bool do_convert_mat;
	float conversion_mat[4][4];

	int from_up;
	int from_forward;
	float scale;
	bool is_sequence;
	bool set_frame_range;
};

/* ************************************************************************** */

using Alembic::AbcCoreAbstract::chrono_t;

class AbcObjectReader {
protected:
	std::string m_name;
	std::string m_object_name;
	std::string m_data_name;
	Object *m_object;
	Alembic::Abc::IObject m_iobject;

	ImportSettings *m_settings;

	chrono_t m_min_time;
	chrono_t m_max_time;

public:
	explicit AbcObjectReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

	virtual ~AbcObjectReader();

	const Alembic::Abc::IObject &iobject() const;

	Object *object() const;

	virtual bool valid() const = 0;

	virtual void readObjectData(Main *bmain, Scene *scene, float time) = 0;

	void readObjectMatrix(const float time);

	void addDefaultModifier(Main *bmain) const;

	chrono_t minTime() const;
	chrono_t maxTime() const;
};
