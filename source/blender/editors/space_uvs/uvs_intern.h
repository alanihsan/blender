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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/** \file blender/editors/space_uvs/uvs_intern.h
 *  \ingroup spuvs
 */

#ifndef __UVS_INTERN_H__
#define __UVS_INTERN_H__

struct ARegion;
struct ScrArea;
struct wmOperatorType;

struct ARegion *uvs_has_buttons_region(struct ScrArea *sa);

void UVS_OT_properties(struct wmOperatorType *ot);
void UVS_OT_view_pan(struct wmOperatorType *ot);
void UVS_OT_view_zoom_in(struct wmOperatorType *ot);
void UVS_OT_view_zoom_out(struct wmOperatorType *ot);
void UVS_OT_view_zoom_ratio(struct wmOperatorType *ot);
void UVS_OT_paint(wmOperatorType *ot);

#endif  /* __UVS_INTERN_H__ */
