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

/** \file blender/editors/space_uvs/uvs_buttons.c
 *  \ingroup spuvs
 */

#include <stdio.h>

#include "BKE_context.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvs_intern.h"

static int uvs_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	fprintf(stderr, "%s\n", __func__);

	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = uvs_has_buttons_region(sa);

	if (ar != NULL) {
		ED_region_toggle_hidden(C, ar);
	}

	return OPERATOR_FINISHED;
}

void UVS_OT_properties(wmOperatorType *ot)
{
	fprintf(stderr, "%s\n", __func__);
	ot->name = "Properties";
	ot->idname = "UVS_OT_properties";
	ot->description = "Toggle the properties region visibility";

	ot->exec = uvs_properties_toggle_exec;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = 0;
}
