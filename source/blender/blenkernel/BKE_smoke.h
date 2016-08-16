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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_SMOKE_H__
#define __BKE_SMOKE_H__

/** \file BKE_smoke.h
 *  \ingroup bke
 *  \author Daniel Genrich
 */

typedef float (*bresenham_callback)(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

struct DerivedMesh *BKE_smoke_step(struct SmokeModifierData *smd, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm);

void BKE_smoke_reallocate_fluid(struct SmokeDomainSettings *sds, int res[3], bool free_old);
void BKE_smoke_reallocate_highres_fluid(struct SmokeDomainSettings *sds, float dx, int res[3]);
void BKE_smoke_free(struct SmokeModifierData *smd);
void BKE_smoke_reset(struct SmokeModifierData *smd);
void BKE_smoke_reset_turbulence(struct SmokeModifierData *smd);
void BKE_smoke_create_type(struct SmokeModifierData *smd);
void BKE_smoke_copy(struct SmokeModifierData *smd, struct SmokeModifierData *tsmd);

float BKE_smoke_get_velocity_at(struct Object *ob, float position[3], float velocity[3]);
int BKE_smoke_get_data_flags(struct SmokeDomainSettings *sds);

#endif /* __BKE_SMOKE_H__ */
