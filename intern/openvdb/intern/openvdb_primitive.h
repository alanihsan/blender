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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_PRIMITIVE_H__
#define __OPENVDB_PRIMITIVE_H__

#include <openvdb/openvdb.h>

class OpenVDBPrimitive {
    openvdb::GridBase::Ptr m_grid;
	bool m_is_empty;

public:
    OpenVDBPrimitive();
    ~OpenVDBPrimitive();

    openvdb::GridBase &getGrid();
    const openvdb::GridBase &getConstGrid() const;
    openvdb::GridBase::Ptr getGridPtr();
    openvdb::GridBase::ConstPtr getConstGridPtr() const;

    void setGridPtr(openvdb::GridBase::Ptr grid);
	void setGrid(openvdb::GridBase::ConstPtr grid);
    void setTransform(const float mat[4][4]);

	bool isEmpty() const;
};

#endif /* __OPENVDB_PRIMITIVE_H__ */