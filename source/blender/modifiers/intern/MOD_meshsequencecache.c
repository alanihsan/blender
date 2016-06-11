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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_meshsequencecache.c
 *  \ingroup modifiers
 */

#include "DNA_cachefile_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cachefile.h"
#include "BKE_DerivedMesh.h"
#include "BKE_library_query.h"
#include "BKE_scene.h"

#include "MOD_modifiertypes.h"

#ifdef WITH_ALEMBIC
#	include "ABC_alembic.h"
#endif

static void initData(ModifierData *md)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;

	mcmd->cache_file = NULL;
	mcmd->abc_object_path[0] = '\0';
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;
	MeshSeqCacheModifierData *tmcmd = (MeshSeqCacheModifierData *)target;
#endif
	modifier_copyData_generic(md, target);
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	/* leave it up to the modifier to check the file is valid on calculation */
	return (mcmd->cache_file == NULL) || (mcmd->abc_object_path[0] == '\0');
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
#ifdef WITH_ALEMBIC
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	Scene *scene = md->scene;

	char filepath[1024];
	if (!BKE_cachefile_filepath_get(scene, mcmd->cache_file, filepath)) {
		return dm;
	}

	const float frame = BKE_scene_frame_get(scene);
	const float time = BKE_cachefile_time_offset(mcmd->cache_file, frame / FPS);

	DerivedMesh *result = ABC_read_mesh(dm,
	                                    filepath,
	                                    mcmd->abc_object_path,
	                                    time);

	return result ? result : dm;
	UNUSED_VARS(ob, flag);
#else
	return dm;
	UNUSED_VARS(md, ob, flag);
#endif
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag flag)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	Scene *scene = md->scene;

	char filepath[1024];
	BKE_cachefile_filepath_get(scene, mcmd->cache_file, filepath);

	const float frame = BKE_scene_frame_get(scene);
	const float time = BKE_cachefile_time_offset(mcmd->cache_file, frame / FPS);

	ABC_read_vertex_cache(filepath,
	                      mcmd->abc_object_path,
                          time,
	                      vertexCos,
	                      numVerts);

	UNUSED_VARS(ob, derivedData, flag);
}

static bool dependsOnTime(ModifierData *md)
{
	UNUSED_VARS(md);
	return true;
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *) md;

	walk(userData, ob, (ID **)&mcmd->cache_file, IDWALK_USER);
}

ModifierTypeInfo modifierType_MeshSequenceCache = {
    /* name */              "Mesh Sequence Cache",
    /* structName */        "MeshSeqCacheModifierData",
    /* structSize */        sizeof(MeshSeqCacheModifierData),
    /* type */              eModifierTypeType_DeformOrConstruct,
    /* flags */             eModifierTypeFlag_AcceptsMesh |
                            eModifierTypeFlag_AcceptsCVs,
    /* copyData */          copyData,
    /* deformVerts */       deformVerts,
    /* deformMatrices */    NULL,
    /* deformVertsEM */     NULL,
    /* deformMatricesEM */  NULL,
    /* applyModifier */     applyModifier,
    /* applyModifierEM */   NULL,
    /* initData */          initData,
    /* requiredDataMask */  NULL,
    /* freeData */          NULL,
    /* isDisabled */        isDisabled,
    /* updateDepgraph */    NULL,
    /* updateDepsgraph */   NULL,
    /* dependsOnTime */     dependsOnTime,
    /* dependsOnNormals */  NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */     foreachIDLink,
    /* foreachTexLink */    NULL,
};
