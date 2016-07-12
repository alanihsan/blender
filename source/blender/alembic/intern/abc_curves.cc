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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "abc_curves.h"

#include <cstdio>

#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"

#include "BKE_curve.h"
#include "BKE_object.h"

#include "ED_curve.h"
}

using Alembic::Abc::IInt32ArrayProperty;
using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::FloatArraySamplePtr;
using Alembic::Abc::P3fArraySamplePtr;
using Alembic::Abc::UcharArraySamplePtr;

using Alembic::AbcGeom::ICurves;
using Alembic::AbcGeom::ICurvesSchema;
using Alembic::AbcGeom::IFloatGeomParam;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::CurvePeriodicity;

using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

/* ************************************************************************** */

AbcCurveWriter::AbcCurveWriter(Scene *scene,
                               Object *ob,
                               AbcTransformWriter *parent,
                               uint32_t time_sampling,
                               ExportSettings &settings)
    : AbcObjectWriter(scene, ob, time_sampling, settings, parent)
{
	OCurves curves(parent->alembicXform(), m_name, m_time_sampling);
	m_schema = curves.getSchema();
}

void AbcCurveWriter::do_write()
{
	Curve *curve = static_cast<Curve *>(m_object->data);

	std::vector<Imath::V3f> verts;
	std::vector<int32_t> vert_counts;
	std::vector<float> widths;
	std::vector<float> weights;
	std::vector<float> knots;
	std::vector<uint8_t> orders;
	Imath::V3f temp_vert;

	Alembic::AbcGeom::BasisType curve_basis;
	Alembic::AbcGeom::CurveType curve_type;
	Alembic::AbcGeom::CurvePeriodicity periodicity;

	Nurb *nurbs = static_cast<Nurb *>(curve->nurb.first);
	for (; nurbs; nurbs = nurbs->next) {
		if (nurbs->bp) {
			curve_basis = Alembic::AbcGeom::kNoBasis;
			curve_type = Alembic::AbcGeom::kLinear;

			const int totpoint = nurbs->pntsu * nurbs->pntsv;

			const BPoint *point = nurbs->bp;

			for (int i = 0; i < totpoint; ++i, ++point) {
				copy_zup_yup(temp_vert.getValue(), point->vec);
				verts.push_back(temp_vert);
				weights.push_back(point->vec[3]);
				widths.push_back(point->radius);
			}
		}
		else if (nurbs->bezt) {
			curve_basis = Alembic::AbcGeom::kBezierBasis;
			curve_type = Alembic::AbcGeom::kCubic;

			const int totpoint = nurbs->pntsu;

			const BezTriple *bezier = nurbs->bezt;

			/* TODO(kevin): store info about handles, Alembic doesn't have this. */
			for (int i = 0; i < totpoint; ++i, ++bezier) {
				copy_zup_yup(temp_vert.getValue(), bezier->vec[1]);
				verts.push_back(temp_vert);
				widths.push_back(bezier->radius);
			}
		}

		if ((nurbs->flagu & CU_NURB_ENDPOINT) != 0) {
			periodicity = Alembic::AbcGeom::kNonPeriodic;
		}
		else if ((nurbs->flagu & CU_NURB_CYCLIC) != 0) {
			periodicity = Alembic::AbcGeom::kPeriodic;

			/* Duplicate the start points to indicate that the curve is actually
			 * cyclic since other software need those.
			 */

			for (int i = 0; i < nurbs->orderu; ++i) {
				verts.push_back(verts[i]);
			}
		}

		const size_t num_knots = KNOTSU(nurbs);

		/* Add an extra knot at the beggining and end of the array since most apps
		 * require/expect them. */
		knots.resize(num_knots + 2);

		for (int i = 0; i < num_knots; ++i) {
			knots[i + 1] = nurbs->knotsu[i];
		}

		if ((nurbs->flagu & CU_NURB_CYCLIC) != 0) {
			knots[0] = nurbs->knotsu[0];
			knots[num_knots - 1] = nurbs->knotsu[num_knots - 1];
		}
		else {
			knots[0] = (2.0f * nurbs->knotsu[0] - nurbs->knotsu[1]);
			knots[num_knots - 1] = (2.0f * nurbs->knotsu[num_knots - 1] - nurbs->knotsu[num_knots - 2]);
		}

		orders.push_back(nurbs->orderu + 1);
		vert_counts.push_back(verts.size());
	}

	Alembic::AbcGeom::OFloatGeomParam::Sample width_sample;
	width_sample.setVals(widths);

	m_sample = OCurvesSchema::Sample(verts,
	                                 vert_counts,
	                                 curve_type,
	                                 periodicity,
	                                 width_sample,
	                                 OV2fGeomParam::Sample(),  /* UVs */
	                                 ON3fGeomParam::Sample(),  /* normals */
	                                 curve_basis,
	                                 weights,
	                                 orders,
	                                 knots);

	m_sample.setSelfBounds(bounds());
	m_schema.set(m_sample);
}

/* ************************************************************************** */

AbcCurveReader::AbcCurveReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
	ICurves abc_curves(object, kWrapExisting);
	m_curves_schema = abc_curves.getSchema();

	get_min_max_time(m_curves_schema, m_min_time, m_max_time);
}

bool AbcCurveReader::valid() const
{
	return m_curves_schema.valid();
}

void AbcCurveReader::readObjectData(Main *bmain, Scene *scene, float time)
{
	Curve *cu = BKE_curve_add(bmain, m_data_name.c_str(), OB_CURVE);

	cu->flag |= CU_DEFORM_FILL | CU_3D;
	cu->actvert = CU_ACT_NONE;

	m_object = BKE_object_add(bmain, scene, OB_CURVE, m_object_name.c_str());
	m_object->data = cu;

	read_curve_sample(cu, m_curves_schema, time);

	if (m_settings->is_sequence || !m_curves_schema.isConstant()) {
		addDefaultModifier(bmain);
	}
}

/* ************************************************************************** */

void read_curve_sample(Curve *cu, const ICurvesSchema &schema, const float time)
{
	const ISampleSelector sample_sel(time);
	ICurvesSchema::Sample smp = schema.getValue(sample_sel);
	const Int32ArraySamplePtr num_vertices = smp.getCurvesNumVertices();
	const P3fArraySamplePtr positions = smp.getPositions();
	const FloatArraySamplePtr weights = smp.getPositionWeights();
	const FloatArraySamplePtr knots = smp.getKnots();
	const CurvePeriodicity periodicity = smp.getWrap();
	const UcharArraySamplePtr orders = smp.getOrders();

	const IFloatGeomParam widths_param = schema.getWidthsParam();
	FloatArraySamplePtr radiuses;

	if (widths_param.valid()) {
		IFloatGeomParam::Sample wsample = widths_param.getExpandedValue(sample_sel);
		radiuses = wsample.getVals();
	}

	int knot_offset = 0;

	size_t idx = 0;
	for (size_t i = 0; i < num_vertices->size(); ++i) {
		const int num_verts = (*num_vertices)[i];

		Nurb *nu = static_cast<Nurb *>(MEM_callocN(sizeof(Nurb), "abc_getnurb"));
		nu->resolu = cu->resolu;
		nu->resolv = cu->resolv;
		nu->pntsu = num_verts;
		nu->pntsv = 1;
		nu->flag |= CU_SMOOTH;

		nu->orderu = num_verts;

		if (smp.getType() == Alembic::AbcGeom::kCubic) {
			nu->orderu = 3;
		}
		else if (orders && orders->size() > i) {
			nu->orderu = static_cast<short>((*orders)[i] - 1);
		}

		if (periodicity == Alembic::AbcGeom::kNonPeriodic) {
			nu->flagu |= CU_NURB_ENDPOINT;
		}
		else if (periodicity == Alembic::AbcGeom::kPeriodic) {
			nu->flagu |= CU_NURB_CYCLIC;

			/* Check the number of points which overlap, we don't have
			 * overlapping points in Blender, but other software do use them to
			 * indicate that a curve is actually cyclic. Usually the number of
			 * overlapping points is equal to the order/degree of the curve.
			 */

			const int start = idx;
			const int end = idx + num_verts;
			int overlap = 0;

			for (int j = start, k = end - nu->orderu; j < nu->orderu; ++j, ++k) {
				const Imath::V3f &p1 = (*positions)[j];
				const Imath::V3f &p2 = (*positions)[k];

				if (p1 != p2) {
					break;
				}

				++overlap;
			}

			/* TODO: Special case, need to figure out how it coincides with knots. */
			if (overlap == 0 && num_verts > 2 && (*positions)[start] == (*positions)[end - 1]) {
				overlap = 1;
			}

			/* There is no real cycles. */
			if (overlap == 0) {
				nu->flagu &= ~CU_NURB_CYCLIC;
				nu->flagu |= CU_NURB_ENDPOINT;
			}

			nu->pntsu -= overlap;
		}

		float weight = 1.0f;

		const bool do_radius = (radiuses != NULL) && (radiuses->size() > 1);
		float radius = (radiuses && radiuses->size() == 1) ? (*radiuses)[0] : 1.0f;

		nu->type = CU_NURBS;

		nu->bp = static_cast<BPoint *>(MEM_callocN(sizeof(BPoint) * nu->pntsu, "abc_getnurb"));
		BPoint *bp = nu->bp;

		for (int j = 0; j < nu->pntsu; ++j, ++bp, ++idx) {
			const Imath::V3f &pos = (*positions)[idx];

			if (do_radius) {
				radius = (*radiuses)[idx];
			}

			if (weights) {
				weight = (*weights)[idx];
			}

			copy_yup_zup(bp->vec, pos.getValue());
			bp->vec[3] = weight;
			bp->f1 = SELECT;
			bp->radius = radius;
			bp->weight = 1.0f;
		}

		if (knots && knots->size() != 0) {
			nu->knotsu = static_cast<float *>(MEM_callocN(KNOTSU(nu) * sizeof(float), "abc_setsplineknotsu"));

			/* TODO: second check is temporary, for until the check for cycles is rock solid. */
			if (periodicity == Alembic::AbcGeom::kPeriodic && (KNOTSU(nu) == knots->size() - 2)) {
				/* Skip first and last knots. */
				for (size_t i = 1; i < knots->size() - 1; ++i) {
					nu->knotsu[i - 1] = (*knots)[knot_offset + i];
				}
			}
			else {
				/* TODO: figure out how to use the knots array from other
				 * software in this case. */
				BKE_nurb_knot_calc_u(nu);
			}

			knot_offset += knots->size();
		}
		else {
			BKE_nurb_knot_calc_u(nu);
		}

		BLI_addtail(BKE_curve_nurbs_get(cu), nu);
	}
}