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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#pragma once

#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/Statistics.h>

namespace internal {

template <typename T>
struct FloatConverter {
	static float get(T value)
	{
		return value;
	}
};

template <>
template <typename T>
struct FloatConverter< openvdb::math::Vec3<T> > {
	static float get(const openvdb::math::Vec3<T> &value)
	{
		return static_cast<float>(value.length());
	}
};

template <typename T>
struct VectorConverter {
	static openvdb::Vec3f get(T value)
	{
		return openvdb::Vec3f(0.0f, 0.0f, value);
	}
};

template <>
template <typename T>
struct VectorConverter< openvdb::math::Vec3<T> > {
	static openvdb::Vec3f get(const openvdb::math::Vec3<T> &value)
	{
		return openvdb::Vec3f(value.x(), value.y(), value.z());
	}
};

struct DenseTextureResOp {
	int *res;
	float *bbmin;
	float *bbmax;

	DenseTextureResOp(int res[3], float bbmin[3], float bbmax[3])
		: res(res)
	    , bbmin(bbmin)
	    , bbmax(bbmax)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		if (!grid->cbeginValueOn()) {
			res[0] = res[1] = res[2] = 0;
			return;
		}

		openvdb::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();
		res[0] = bbox.dim().x();
		res[1] = bbox.dim().y();
		res[2] = bbox.dim().z();

		openvdb::BBoxd vbox = grid->transform().indexToWorld(bbox);
		vbox.min().toV(bbmin);
		vbox.max().toV(bbmax);
	}
};

struct DenseTextureOp {
	float *buffer;

	DenseTextureOp(float *buffer)
		: buffer(buffer)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		typedef typename GridType::ValueType ValueType;

		typename GridType::ConstAccessor acc = grid->getConstAccessor();

		openvdb::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();

		openvdb::Coord bbmin = bbox.min(), bbmax = bbox.max();
		size_t index = 0;
		openvdb::Coord ijk;
		int &i = ijk[0], &j = ijk[1], &k = ijk[2];
		for (k = bbmin[2]; k <= bbmax[2]; ++k) {
			for (j = bbmin[1]; j <= bbmax[1]; ++j) {
				for (i = bbmin[0]; i <= bbmax[0]; ++i, ++index) {
					buffer[index] = acc.isValueOn(ijk) ? FloatConverter<ValueType>::get(acc.getValue(ijk)) : 0.0f;
				}
			}
		}
	}
};

struct CellsBufferSizeOp {
	int *numverts;
	int level_min, level_max;
	bool do_voxels;

	CellsBufferSizeOp(int *r_numverts, int min_level, int max_level, bool voxels)
	    : numverts(r_numverts)
	    , level_min(min_level)
	    , level_max(max_level)
	    , do_voxels(voxels)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		typedef typename GridType::TreeType TreeType;

		int total = 0;

		for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
			const int level = node_iter.getLevel();

			if (level < level_min || level > level_max)
				continue;

			total += 6 * 4;
		}

		if (do_voxels) {
			total += grid->tree().activeLeafVoxelCount() * 6 * 4;
		}

		*numverts = total;
	}
};

static void copy_v3_v3(float *r, const float *a)
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

static void cross_v3_v3v3(float *r, const float *a, const float *b)
{
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

static void get_normal(float nor[3], const openvdb::Vec3f &v1, const openvdb::Vec3f &v2,
                       const openvdb::Vec3f &v3, const openvdb::Vec3f &v4)
{
	openvdb::Vec3f n1, n2;

	n1 = v1 - v3;
	n2 = v2 - v4;

	cross_v3_v3v3(nor, n1.asV(), n2.asV());
}

static inline void add_quad(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                            const openvdb::Vec3f &p1, const openvdb::Vec3f &p2,
                            const openvdb::Vec3f &p3, const openvdb::Vec3f &p4,
                            const openvdb::Vec3f &color)
{
	copy_v3_v3(verts[*verts_ofs+0], p1.asV());
	copy_v3_v3(verts[*verts_ofs+1], p2.asV());
	copy_v3_v3(verts[*verts_ofs+2], p3.asV());
	copy_v3_v3(verts[*verts_ofs+3], p4.asV());

	copy_v3_v3(colors[*verts_ofs+0], color.asV());
	copy_v3_v3(colors[*verts_ofs+1], color.asV());
	copy_v3_v3(colors[*verts_ofs+2], color.asV());
	copy_v3_v3(colors[*verts_ofs+3], color.asV());

	if (normals) {
		float nor[3];

		get_normal(nor, p1, p2, p3, p4);

		copy_v3_v3(normals[*verts_ofs+0], nor);
		copy_v3_v3(normals[*verts_ofs+1], nor);
		copy_v3_v3(normals[*verts_ofs+2], nor);
		copy_v3_v3(normals[*verts_ofs+3], nor);
	}

	*verts_ofs += 4;
}

static void add_box(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                    const openvdb::Vec3f &min, const openvdb::Vec3f &max,
                    const openvdb::Vec3f &color)
{
	using namespace openvdb;

	const Vec3f corners[8] = {
	    min,
	    Vec3f(min.x(), min.y(), max.z()),
	    Vec3f(max.x(), min.y(), max.z()),
	    Vec3f(max.x(), min.y(), min.z()),
	    Vec3f(min.x(), max.y(), min.z()),
	    Vec3f(min.x(), max.y(), max.z()),
	    max,
	    Vec3f(max.x(), max.y(), min.z())
	};

	add_quad(verts, colors, normals, verts_ofs, corners[0], corners[1], corners[2], corners[3], color);
	add_quad(verts, colors, normals, verts_ofs, corners[7], corners[6], corners[5], corners[4], color);
	add_quad(verts, colors, normals, verts_ofs, corners[4], corners[5], corners[1], corners[0], color);
	add_quad(verts, colors, normals, verts_ofs, corners[3], corners[2], corners[6], corners[7], color);
	add_quad(verts, colors, normals, verts_ofs, corners[3], corners[7], corners[4], corners[0], color);
	add_quad(verts, colors, normals, verts_ofs, corners[1], corners[5], corners[6], corners[2], color);
}

template <typename TreeType>
static void OpenVDB_get_draw_buffers_cells(const openvdb::Grid<TreeType> *grid, int min_level, int max_level, bool voxels,
                                           float (*verts)[3], float (*colors)[3])
{
	using namespace openvdb;
	using namespace openvdb::math;

	typedef typename TreeType::LeafNodeType LeafNodeType;

	if (!grid) {
		return;
	}

	/* The following colors are meant to be the same as in the example images of
	 * "VDB: High-Resolution Sparse Volumes With Dynamic Topology", K. Museth, 2013
	 */
	static const Vec3f node_color[4] = {
	    Vec3f(0.045f, 0.045f, 0.045f), // root node (black)
	    Vec3f(0.043f, 0.330f, 0.041f), // first internal node level (green)
	    Vec3f(0.871f, 0.394f, 0.019f), // intermediate internal node levels (orange)
	    Vec3f(0.006f, 0.280f, 0.625f)  // leaf nodes (blue)
	};
	static const Vec3f voxel_color = Vec3f(1.000f, 0.000f, 0.000f); // active voxel (red)

	int verts_ofs = 0;
	for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		const int level = node_iter.getLevel();

		if (level < min_level || level > max_level)
			continue;

		CoordBBox bbox;
		node_iter.getBoundingBox(bbox);

		Vec3f min = Vec3f(bbox.min().x(), bbox.min().y(), bbox.min().z()) - Vec3f(0.5f, 0.5f, 0.5f);
		Vec3f max = Vec3f(bbox.max().x(), bbox.max().y(), bbox.max().z()) + Vec3f(0.5f, 0.5f, 0.5f);
		Vec3f wmin = grid->indexToWorld(min);
		Vec3f wmax = grid->indexToWorld(max);

		Vec3f color = node_color[std::max(3 - level, 0)];

		add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
	}

	if (voxels) {
		for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
			const LeafNodeType *leaf = leaf_iter.getLeaf();

			for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
				const Coord ijk = value_iter.getCoord();

				Vec3f min = Vec3f(ijk.x(), ijk.y(), ijk.z()) - Vec3f(0.5f, 0.5f, 0.5f);
				Vec3f max = Vec3f(ijk.x(), ijk.y(), ijk.z()) + Vec3f(0.5f, 0.5f, 0.5f);
				Vec3f wmin = grid->indexToWorld(min);
				Vec3f wmax = grid->indexToWorld(max);

				Vec3f color = voxel_color;

				add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
			}
		}
	}
}

struct CellsBufferOp {
	int level_min, level_max;
	bool do_voxels;
	float (*verts)[3];
	float (*colors)[3];

	CellsBufferOp(int min_level, int max_level, bool voxels, float (*verts)[3], float (*colors)[3])
	    : level_min(min_level)
	    , level_max(max_level)
	    , do_voxels(voxels)
	    , verts(verts)
	    , colors(colors)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		using namespace openvdb;
		using namespace openvdb::math;

		typedef typename GridType::TreeType     TreeType;
		typedef typename TreeType::LeafNodeType LeafNodeType;

		/* The following colors are meant to be the same as in the example images of
		 * "VDB: High-Resolution Sparse Volumes With Dynamic Topology", K. Museth, 2013
		 */
		static const Vec3f node_color[4] = {
		    Vec3f(0.045f, 0.045f, 0.045f), // root node (black)
		    Vec3f(0.043f, 0.330f, 0.041f), // first internal node level (green)
		    Vec3f(0.871f, 0.394f, 0.019f), // intermediate internal node levels (orange)
		    Vec3f(0.006f, 0.280f, 0.625f)  // leaf nodes (blue)
		};
		static const Vec3f voxel_color = Vec3f(1.000f, 0.000f, 0.000f); // active voxel (red)

		int verts_ofs = 0;
		for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
			const int level = node_iter.getLevel();

			if (level < level_min || level > level_max)
				continue;

			CoordBBox bbox;
			node_iter.getBoundingBox(bbox);

			Vec3f min = Vec3f(bbox.min().x(), bbox.min().y(), bbox.min().z()) - Vec3f(0.5f, 0.5f, 0.5f);
			Vec3f max = Vec3f(bbox.max().x(), bbox.max().y(), bbox.max().z()) + Vec3f(0.5f, 0.5f, 0.5f);
			Vec3f wmin = grid->indexToWorld(min);
			Vec3f wmax = grid->indexToWorld(max);

			Vec3f color = node_color[std::max(3 - level, 0)];

			add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
		}

		if (do_voxels) {
			for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
				const LeafNodeType *leaf = leaf_iter.getLeaf();

				for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
					const Coord ijk = value_iter.getCoord();

					Vec3f min = Vec3f(ijk.x(), ijk.y(), ijk.z()) - Vec3f(0.5f, 0.5f, 0.5f);
					Vec3f max = Vec3f(ijk.x(), ijk.y(), ijk.z()) + Vec3f(0.5f, 0.5f, 0.5f);
					Vec3f wmin = grid->indexToWorld(min);
					Vec3f wmax = grid->indexToWorld(max);

					Vec3f color = voxel_color;

					add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
				}
			}
		}
	}
};

static void get_normal(float nor[3], const openvdb::Vec3f &v1, const openvdb::Vec3f &v2, const openvdb::Vec3f &v3)
{
	openvdb::Vec3f n1, n2;

	n1 = v2 - v1;
	n2 = v3 - v1;

	cross_v3_v3v3(nor, n1.asV(), n2.asV());
}

static inline void add_tri(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                           const openvdb::Vec3f &p1, const openvdb::Vec3f &p2, const openvdb::Vec3f &p3, const openvdb::Vec3f &color)
{
	copy_v3_v3(verts[*verts_ofs+0], p1.asV());
	copy_v3_v3(verts[*verts_ofs+1], p2.asV());
	copy_v3_v3(verts[*verts_ofs+2], p3.asV());

	copy_v3_v3(colors[*verts_ofs+0], color.asV());
	copy_v3_v3(colors[*verts_ofs+1], color.asV());
	copy_v3_v3(colors[*verts_ofs+2], color.asV());

	if (normals) {
		float nor[3];

		get_normal(nor, p1, p2, p3);

		copy_v3_v3(normals[*verts_ofs+0], nor);
		copy_v3_v3(normals[*verts_ofs+1], nor);
		copy_v3_v3(normals[*verts_ofs+2], nor);
	}

	*verts_ofs += 3;
}

static void add_needle(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                       const openvdb::Vec3f &center, const openvdb::Vec3f &dir, float len,
                       const openvdb::Vec3f &color)
{
	using namespace openvdb;

	Vec3f corners[4] = {
	    Vec3f(0.0f, 0.2f, -0.5f),
	    Vec3f(-0.2f * 0.866f, -0.2f * 0.5f, -0.5f),
	    Vec3f(0.2f * 0.866f, -0.2f * 0.5f, -0.5f),
	    Vec3f(0.0f, 0.0f, 0.5f)
	};
	Vec3f up(0.0f, 0.0f, 1.0f);
	Mat3R rot = math::rotation<Mat3R>(up, dir).transpose();
	for (int i = 0; i < 4; ++i) {
		corners[i] = (rot * corners[i]) * len + center;
	}

	add_tri(verts, colors, normals, verts_ofs, corners[0], corners[1], corners[2], color);
	add_tri(verts, colors, normals, verts_ofs, corners[0], corners[1], corners[3], color);
	add_tri(verts, colors, normals, verts_ofs, corners[1], corners[2], corners[3], color);
	add_tri(verts, colors, normals, verts_ofs, corners[2], corners[0], corners[3], color);
}

static void add_staggered_needle(float (*verts)[3], float (*colors)[3], int *verts_ofs,
                                 const openvdb::Vec3f &center, float size, const openvdb::Vec3f &vec)
{
	using namespace openvdb;

#define SHIFT(v, n) Vec3f((v)[(n)%3], (v)[((n)+2)%3], (v)[((n)+1)%3])

	Vec3f corners[6] = {
	    Vec3f(1.0f, 0.0f, 0.0f),
	    Vec3f(0.0f, 0.0f, 0.15f),
	    Vec3f(0.0f, 0.0f, -0.15f),
	    Vec3f(0.0f, 0.15f, 0.0f),
	    Vec3f(0.0f, -0.15f, 0.0f),
	};

	for (int n = 0; n < 3; ++n) {
		float len = vec[n] * size;

		Vec3f tip = SHIFT(corners[0], n) * len;
		Vec3f a = SHIFT(corners[1], n) * len;
		Vec3f b = SHIFT(corners[2], n) * len;
		Vec3f c = SHIFT(corners[3], n) * len;
		Vec3f d = SHIFT(corners[4], n) * len;

		Vec3f base = center - SHIFT(Vec3f(size * 0.5f, 0.0f, 0.0f), n);

		Vec3f color = SHIFT(Vec3f(1,0,0), n);

		add_tri(verts, colors, NULL, verts_ofs, base + tip, base + a, base + b, color);
		add_tri(verts, colors, NULL, verts_ofs, base + tip, base + c, base + d, color);
	}

#undef SHIFT
}

static inline void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b)
{
	float nr, ng, nb;

	nr =        fabs(h * 6.0f - 3.0f) - 1.0f;
	ng = 2.0f - fabs(h * 6.0f - 2.0f);
	nb = 2.0f - fabs(h * 6.0f - 4.0f);

	nr = std::max(0.0f, std::min(nr, 1.0f));
	ng = std::max(0.0f, std::min(ng, 1.0f));
	nb = std::max(0.0f, std::min(nb, 1.0f));

	*r = ((nr - 1.0f) * s + 1.0f) * v;
	*g = ((ng - 1.0f) * s + 1.0f) * v;
	*b = ((nb - 1.0f) * s + 1.0f) * v;
}

struct BoxesBufferSizeOp {
	int *numverts;

	BoxesBufferSizeOp(int *r_numverts)
	    : numverts(r_numverts)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		*numverts = grid->tree().activeLeafVoxelCount() * 6 * 4;
	}
};

struct BoxesBufferOp {
	float value_scale;
	float (*verts)[3];
	float (*colors)[3];
	float (*normals)[3];

	BoxesBufferOp(float value_scale, float (*verts)[3], float (*colors)[3], float (*normals)[3])
	    : value_scale(value_scale)
	    , verts(verts)
	    , colors(colors)
	    , normals(normals)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		using namespace openvdb;
		using namespace openvdb::math;

		typedef typename GridType::TreeType     TreeType;
		typedef typename TreeType::ValueType    ValueType;
		typedef typename TreeType::LeafNodeType LeafNodeType;

		if (!grid) {
			return;
		}

		const float bg = FloatConverter<ValueType>::get(grid->background());
		int verts_ofs = 0;

		for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
			const LeafNodeType *leaf = leaf_iter.getLeaf();

			for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
				const Coord ijk = value_iter.getCoord();

				float fac = FloatConverter<ValueType>::get(value_iter.getValue());
				fac = (fac - bg) * value_scale;
				fac = std::max(-1.0f, std::min(fac, 1.0f));

				Vec3f center(ijk.x(), ijk.y(), ijk.z());
				Vec3f min = center - Vec3f(0.5f, 0.5f, 0.5f) * fac;
				Vec3f max = center + Vec3f(0.5f, 0.5f, 0.5f) * fac;
				Vec3f wmin = grid->indexToWorld(min);
				Vec3f wmax = grid->indexToWorld(max);

				float r, g, b;
				// -1..0..1 = red..yellow..green
				hsv_to_rgb((fac + 1.0f) / 6.0f, 1.0f, 1.0f, &r, &g, &b);
				Vec3f color = Vec3f(r, g, b);

				add_box(verts, colors, normals, &verts_ofs, wmin, wmax, color);
			}
		}
	}
};

struct NeedlesBufferSizeOp {
	int *numverts;

	NeedlesBufferSizeOp(int *r_numverts)
	    : numverts(r_numverts)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		*numverts = grid->tree().activeLeafVoxelCount() * 4 * 3;
	}
};

struct NeedlesBufferOp {
	float value_scale;
	float (*verts)[3];
	float (*colors)[3];
	float (*normals)[3];

	NeedlesBufferOp(float value_scale, float (*verts)[3], float (*colors)[3], float (*normals)[3])
	    : value_scale(value_scale)
	    , verts(verts)
	    , colors(colors)
	    , normals(normals)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		using namespace openvdb;
		using namespace openvdb::math;

		typedef typename GridType::TreeType     TreeType;
		typedef typename TreeType::ValueType    ValueType;
		typedef typename TreeType::LeafNodeType LeafNodeType;

		typedef typename GridType::ConstAccessor AccessorType;
		typedef openvdb::tools::GridSampler<AccessorType, tools::BoxSampler> SamplerType;

		if (!grid) {
			return;
		}

		const float bg = FloatConverter<ValueType>::get(grid->background());
		int verts_ofs = 0;

		AccessorType acc(grid->tree());
		SamplerType sampler(acc, grid->transform());

		for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
			const LeafNodeType *leaf = leaf_iter.getLeaf();

			for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
				const Coord ijk = value_iter.getCoord();
				Vec3f center = grid->indexToWorld(ijk);

				Vec3f vec = VectorConverter<ValueType>::get(sampler.wsSample(center));
				float len = vec.length();
				if (len != 0.0f) {
					vec /= len;

					len = (len - bg) * value_scale;
					len = std::max(-1.0f, std::min(len, 1.0f));
				}

				float r, g, b;
				// -1..0..1 = red..yellow..green
				hsv_to_rgb((len + 1.0f) / 6.0f, 1.0f, 1.0f, &r, &g, &b);
				Vec3f color = Vec3f(r, g, b);

				add_needle(verts, colors, normals, &verts_ofs, center, vec, len * grid->voxelSize().x(), color);
			}
		}
	}
};

struct StaggeredBufferSizeOp {
	int *numverts;

	StaggeredBufferSizeOp(int *r_numverts)
	    : numverts(r_numverts)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		*numverts = grid->tree().activeLeafVoxelCount() * 6 * 3;
	}
};

struct StaggeredBufferOp {
	float value_scale;
	float (*verts)[3];
	float (*colors)[3];

	StaggeredBufferOp(float value_scale, float (*verts)[3], float (*colors)[3])
	    : value_scale(value_scale)
	    , verts(verts)
	    , colors(colors)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		using namespace openvdb;
		using namespace openvdb::math;

		typedef typename GridType::TreeType     TreeType;
		typedef typename TreeType::ValueType    ValueType;
		typedef typename TreeType::LeafNodeType LeafNodeType;

		typedef typename GridType::ConstAccessor AccessorType;
		typedef openvdb::tools::GridSampler<AccessorType, tools::BoxSampler> SamplerType;

		if (!grid) {
			return;
		}

		const float bg = FloatConverter<ValueType>::get(grid->background());
		int verts_ofs = 0;

		AccessorType acc(grid->tree());
		SamplerType sampler(acc, grid->transform());

		for (typename TreeType::LeafCIter leaf_iter = grid->tree().cbeginLeaf(); leaf_iter; ++leaf_iter) {
			const LeafNodeType *leaf = leaf_iter.getLeaf();

			for (typename LeafNodeType::ValueOnCIter value_iter = leaf->cbeginValueOn(); value_iter; ++value_iter) {
				const Coord ijk = value_iter.getCoord();
				Vec3f center = grid->indexToWorld(ijk);

				Vec3f vec = VectorConverter<ValueType>::get(sampler.wsSample(center));
				float len = vec.length();
				if (len != 0.0f) {
					vec /= len;

					len = (len - bg) * value_scale;
					len = std::max(-1.0f, std::min(len, 1.0f));
					vec *= len;
				}

				add_staggered_needle(verts, colors, &verts_ofs, center, grid->voxelSize().x(), vec);
			}
		}
	}
};

struct GridValueRangeOp {
	float *min;
	float *max;
	float *bg;

	GridValueRangeOp(float *mn, float *mx, float *bgd)
	    : min(mn)
	    , max(mx)
	    , bg(bgd)
	{}

	template <typename GridType>
	void operator()(typename GridType::ConstPtr grid)
	{
		typedef typename GridType::ValueType ValueType;

		*bg = FloatConverter<ValueType>::get(grid->background());

		if (grid->empty()) {
			*min = *bg;
			*max = *bg;
			return;
		}

		openvdb::math::Extrema ex = openvdb::tools::extrema(grid->cbeginValueOn());
		*min = std::min((float)ex.min(), *bg);
		*max = std::max((float)ex.max(), *bg);
	}
};

} /* namespace internal */