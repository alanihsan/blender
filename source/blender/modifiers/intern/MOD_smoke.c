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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_smoke.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_object_force.h"

#include "BLI_utildefines.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_smoke.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md) 
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	
	smd->domain = NULL;
	smd->flow = NULL;
	smd->coll = NULL;
	smd->type = 0;
	smd->time = -1;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SmokeModifierData *smd  = (SmokeModifierData *)md;
	SmokeModifierData *tsmd = (SmokeModifierData *)target;
	
	smokeModifier_copy(smd, tsmd);
}

static void freeData(ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	
	smokeModifier_free(smd);
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
                                  DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (flag & MOD_APPLY_ORCO)
		return dm;

	return smokeModifier_do(smd, md->scene, ob, dm);
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static bool is_coll_cb(Object *UNUSED(ob), ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	return (smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll;
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *scene, struct Object *ob,
                           DagNode *obNode)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		for (SmokeFlowSettings *sfs = smd->domain->sources.first; sfs; sfs = sfs->next) {
			if (!sfs->object) {
				continue;
			}

			DagNode *node2 = dag_get_node(forest, sfs->object);
			dag_add_relation(forest, node2, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Smoke Flow");
		}

		dag_add_collision_relations(forest, scene, ob, obNode, smd->domain->coll_group, ob->lay|scene->lay, eModifierType_Smoke, is_coll_cb, true, "Smoke Coll");

		dag_add_forcefield_relations(forest, scene, ob, obNode, smd->domain->effector_weights, true, PFIELD_SMOKEFLOW, "Smoke Force Field");
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *scene,
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	SmokeModifierData *smd = (SmokeModifierData *)md;

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		for (SmokeFlowSettings *sfs = smd->domain->sources.first; sfs; sfs = sfs->next) {
			if (!sfs->object) {
				continue;
			}

			DEG_add_object_relation(node, sfs->object, DEG_OB_COMP_TRANSFORM, "Smoke Flow");
			DEG_add_object_relation(node, sfs->object, DEG_OB_COMP_GEOMETRY, "Smoke Flow");
		}

		DEG_add_collision_relations(node, scene, ob, smd->domain->coll_group, ob->lay|scene->lay, eModifierType_Smoke, is_coll_cb, true, "Smoke Coll");

		DEG_add_forcefield_relations(node, scene, ob, smd->domain->effector_weights, true, PFIELD_SMOKEFLOW, "Smoke Force Field");
	}
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (smd->type == MOD_SMOKE_TYPE_DOMAIN && smd->domain) {
		for (SmokeFlowSettings *sfs = smd->domain->sources.first; sfs; sfs = sfs->next) {
			if (!sfs->object) {
				continue;
			}

			walk(userData, ob, (ID **)&sfs->object, IDWALK_NOP);
			walk(userData, ob, (ID **)&sfs->noise_texture, IDWALK_USER);
		}

		walk(userData, ob, (ID **)&smd->domain->coll_group, IDWALK_NOP);
		walk(userData, ob, (ID **)&smd->domain->eff_group, IDWALK_NOP);

		if (smd->domain->effector_weights) {
			walk(userData, ob, (ID **)&smd->domain->effector_weights->group, IDWALK_NOP);
		}
	}
}

ModifierTypeInfo modifierType_Smoke = {
	/* name */              "Smoke",
	/* structName */        "SmokeModifierData",
	/* structSize */        sizeof(SmokeModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_UsesPointCache |
	                        eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL
};
