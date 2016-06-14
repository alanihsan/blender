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

#include "abc_nurbs.h"

#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_curve.h"
#include "BKE_object.h"
}

using Alembic::AbcGeom::bool_t;
using Alembic::AbcGeom::FloatArraySample;
using Alembic::AbcGeom::FloatArraySamplePtr;
using Alembic::AbcGeom::MetaData;
using Alembic::AbcGeom::P3fArraySamplePtr;
using Alembic::AbcGeom::kWrapExisting;

using Alembic::AbcGeom::IBoolProperty;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::INuPatch;
using Alembic::AbcGeom::INuPatchSchema;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::ISampleSelector;

using Alembic::AbcGeom::OBoolProperty;
using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::ONuPatch;
using Alembic::AbcGeom::ONuPatchSchema;

/* ************************************************************************** */

AbcNurbsWriter::AbcNurbsWriter(Scene *scene,
                               Object *ob,
                               AbcTransformWriter *parent,
                               uint32_t time_sampling,
                               ExportSettings &settings)
    : AbcObjectWriter(scene, ob, time_sampling, settings, parent)
{
	m_is_animated = isAnimated();

	/* if the object is static, use the default static time sampling */
	if (!m_is_animated) {
		time_sampling = 0;
	}

	Curve *curve = static_cast<Curve *>(m_object->data);
	size_t numNurbs = BLI_listbase_count(&curve->nurb);

	for (size_t i = 0; i < numNurbs; ++i) {
		std::stringstream str;
		str << m_name << '_' << i;

		while (parent->alembicXform().getChildHeader(str.str())) {
			str << "_";
		}

		ONuPatch nurbs(parent->alembicXform(), str.str().c_str(), m_time_sampling);
		m_nurbs_schema.push_back(nurbs.getSchema());
	}
}

bool AbcNurbsWriter::isAnimated() const
{
	/* check if object has shape keys */
	Curve *cu = static_cast<Curve *>(m_object->data);
	return (cu->key != NULL);
}

static void recompute_pnts_cyclic(const BPoint *bps,
                                  const int num_u, const int num_v,
                                  const int add_u, const int add_v,
                                  std::vector<Imath::V3f> &pos,
                                  std::vector<float> &posWeight)
{
	const int new_u = num_u;/* + add_u; */
	const int new_v = num_v;/* + add_v; */
	const int new_size = new_u * new_v;

	pos.reserve(new_size);
	posWeight.reserve(new_size);

	std::vector< std::vector<Imath::Vec4<float> > > pnts;
	pnts.resize(new_u);

	for (int u = 0; u < new_u; ++u) {
		pnts[u].resize(new_v);

		for (int v = 0; v < new_v; ++v) {
			const BPoint& bp = bps[u + (v * new_u)];
			pnts[u][v] = Imath::Vec4<float>(bp.vec[0], bp.vec[1], bp.vec[2], bp.vec[3]);
		}
	}

	for (int u = 0; u < new_u; ++u) {
		for (int v = 0; v < new_v; ++v) {
			const Imath::Vec4<float> &pnt = pnts[u][v];

			/* Convert Z-up to Y-up. */
			pos.push_back(Imath::V3f(pnt.x, pnt.z, -pnt.y));

			posWeight.push_back(pnt.z);
		}
	}
}

void AbcNurbsWriter::do_write()
{
	/* we have already stored a sample for this object. */
	if (!m_first_frame && !m_is_animated)
		return;

	if (!ELEM(m_object->type, OB_SURF, OB_CURVE)) {
		return;
	}

	Curve *curve = static_cast<Curve *>(m_object->data);
	ListBase *nulb;

	if (m_object->curve_cache->deformed_nurbs.first != NULL) {
		nulb = &m_object->curve_cache->deformed_nurbs;
	}
	else {
		nulb = BKE_curve_nurbs_get(curve);
	}

	size_t count = 0;
	for (Nurb *nu = static_cast<Nurb *>(nulb->first); nu; nu = nu->next, count++) {
		const int numKnotsU = KNOTSU(nu);
		std::vector<float> knotsU;
		knotsU.reserve(numKnotsU);

		for (int i = 0; i < numKnotsU; ++i) {
			knotsU.push_back(nu->knotsu[i]);
		}

		const int numKnotsV = KNOTSV(nu);
		std::vector<float> knotsV;
		knotsV.reserve(numKnotsV);

		for (int i = 0; i < numKnotsV; ++i) {
			knotsV.push_back(nu->knotsv[i]);
		}

		ONuPatchSchema::Sample nuSamp;
		nuSamp.setUOrder(nu->orderu);
		nuSamp.setVOrder(nu->orderv);

		const int add_u = (nu->flagu & CU_NURB_CYCLIC) ? nu->orderu - 1 : 0;
		const int add_v = (nu->flagv & CU_NURB_CYCLIC) ? nu->orderv - 1 : 0;

		std::vector<Imath::V3f> sampPos;
		std::vector<float> sampPosWeights;
		recompute_pnts_cyclic(nu->bp, nu->pntsu, nu->pntsv, add_u, add_v,
		                      sampPos, sampPosWeights);

		nuSamp.setPositions(sampPos);
		nuSamp.setPositionWeights(sampPosWeights);
		nuSamp.setUKnot(FloatArraySample(knotsU));
		nuSamp.setVKnot(FloatArraySample(knotsV));
		nuSamp.setNu(nu->pntsu);
		nuSamp.setNv(nu->pntsv);

		const bool endu = nu->flagu & CU_NURB_ENDPOINT;
		const bool endv = nu->flagv & CU_NURB_ENDPOINT;

		OCompoundProperty typeContainer = m_nurbs_schema[count].getUserProperties();
		OBoolProperty enduprop(typeContainer, "endU");
		enduprop.set(endu);
		OBoolProperty endvprop(typeContainer, "endV");
		endvprop.set(endv);

		m_nurbs_schema[count].set(nuSamp);
	}
}

/* ************************************************************************** */

AbcNurbsReader::AbcNurbsReader(const IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
	getNurbsPatches(m_iobject);
	get_min_max_time(m_schemas[0].first, m_min_time, m_max_time);
}

bool AbcNurbsReader::valid() const
{
	if (m_schemas.empty()) {
		return false;
	}

	/* TODO */
	return m_schemas[0].first.valid();
}

void AbcNurbsReader::readObjectData(Main *bmain, Scene *scene, float time)
{
	Curve *cu = static_cast<Curve *>(BKE_curve_add(bmain, "abc_curve", OB_SURF));

	std::vector< std::pair<INuPatchSchema, IObject> >::iterator it;

	for (it = m_schemas.begin(); it != m_schemas.end(); ++it) {
		Nurb *nu = (Nurb *)MEM_callocN(sizeof(Nurb), "abc_getnurb");
		nu->flag  = CU_SMOOTH;
		nu->type = CU_NURBS;
		nu->resolu = 4;
		nu->resolv = 4;

		const ISampleSelector sample_sel(time);
		const INuPatchSchema::Sample smp = it->first.getValue(sample_sel);
		const P3fArraySamplePtr positions = smp.getPositions();
		const FloatArraySamplePtr positionsW = smp.getPositionWeights();
		const int32_t num_U = smp.getNumU();
		const int32_t num_V = smp.getNumV();
		const int32_t u_order = smp.getUOrder();
		const int32_t v_order = smp.getVOrder();
		const FloatArraySamplePtr u_knot = smp.getUKnot();
		const FloatArraySamplePtr v_knot = smp.getVKnot();

		const size_t numPt = positions->size();
		const size_t numKnotsU = u_knot->size();
		const size_t numKnotsV = v_knot->size();

		nu->orderu = u_order;
		nu->orderv = v_order;
		nu->pntsu = num_U;
		nu->pntsv = num_V;
		nu->bezt = NULL;

		nu->bp = (BPoint *)MEM_callocN(numPt * sizeof(BPoint), "abc_setsplinetype");
		nu->knotsu = (float *)MEM_callocN(numKnotsU * sizeof(float), "abc_setsplineknotsu");
		nu->knotsv = (float *)MEM_callocN(numKnotsV * sizeof(float), "abc_setsplineknotsv");
		nu->bp->radius = 1.0f;

		for (int i = 0; i < numPt; ++i) {
			Imath::V3f pos_in = (*positions)[i];
			float posw_in = 1.0f;

			if (positionsW && i < positionsW->size()) {
				posw_in = (*positionsW)[i];
			}

			/* Convert Y-up to Z-up. */
			copy_yup_zup(nu->bp[i].vec, pos_in.getValue());
			nu->bp[i].vec[3] = posw_in;
		}

		for (size_t i = 0; i < numKnotsU; i++) {
			nu->knotsu[i] = (*u_knot)[i];
		}

		for (size_t i = 0; i < numKnotsV; i++) {
			nu->knotsv[i] = (*v_knot)[i];
		}

		ICompoundProperty userProps = it->first.getUserProperties();
		if (userProps.valid() && userProps.getPropertyHeader("endU") != 0) {
			IBoolProperty enduProp(userProps, "endU");
			bool_t endu;
			enduProp.get(endu, sample_sel);

			if (endu) {
				nu->flagu = CU_NURB_ENDPOINT;
			}
		}

		if (userProps.valid() && userProps.getPropertyHeader("endV") != 0) {
			IBoolProperty endvProp(userProps, "endV");
			bool_t endv;
			endvProp.get(endv, sample_sel);

			if (endv) {
				nu->flagv = CU_NURB_ENDPOINT;
			}
		}

		BLI_addtail(BKE_curve_nurbs_get(cu), nu);
	}

	BLI_strncpy(cu->id.name + 2, m_data_name.c_str(), m_data_name.size() + 1);

	m_object = BKE_object_add(bmain, scene, OB_CURVE, m_object_name.c_str());
	m_object->data = cu;
}

void AbcNurbsReader::getNurbsPatches(const IObject &obj)
{
	if (!obj.valid()) {
		return;
	}

	const int num_children = obj.getNumChildren();

	if (num_children == 0) {
		INuPatch abc_nurb(obj, kWrapExisting);
		INuPatchSchema schem = abc_nurb.getSchema();
		m_schemas.push_back(std::pair<INuPatchSchema, IObject>(schem, obj));
		return;
	}

	for (int i = 0; i < num_children; ++i) {
		bool ok = true;
		IObject child(obj, obj.getChildHeader(i).getName());

		if (!m_name.empty() && child.valid() && !begins_with(child.getFullName(), m_name)) {
			ok = false;
		}

		if (!child.valid())
			continue;

		const MetaData &md = child.getMetaData();

		if (INuPatch::matches(md) && ok) {
			INuPatch abc_nurb(child, kWrapExisting);
			INuPatchSchema schem = abc_nurb.getSchema();
			m_schemas.push_back(std::pair<INuPatchSchema, IObject>(schem, child));
		}

		getNurbsPatches(child);
	}
}
