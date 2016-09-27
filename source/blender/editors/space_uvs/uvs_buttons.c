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

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"

#include "BKE_context.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvs_intern.h"

static void ED_space_uvs_get_size(SpaceUVs *suvs, int *width, int *height)
{
	UNUSED_VARS(suvs);
	*width = *height = IMG_SIZE_FALLBACK;
}

static void ED_space_uvs_get_zoom(SpaceUVs *suvs, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	ED_space_uvs_get_size(suvs, &width, &height);

	*zoomx = (float)(BLI_rcti_size_x(&ar->winrct) + 1) / (BLI_rctf_size_x(&ar->v2d.cur) * width);
	*zoomy = (float)(BLI_rcti_size_y(&ar->winrct) + 1) / (BLI_rctf_size_y(&ar->v2d.cur) * height);
}

static void ED_uvs_point_stable_pos(SpaceUVs *suvs, ARegion *ar, float x, float y, float *xr, float *yr)
{
	int sx, sy, width, height;
	float zoomx, zoomy, pos[3];

	ED_space_uvs_get_zoom(suvs, ar, &zoomx, &zoomy);
	ED_space_uvs_get_size(suvs, &width, &height);

	UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &sx, &sy);

	pos[0] = (x - sx) / zoomx;
	pos[1] = (y - sy) / zoomy;
	pos[2] = 0.0f;

	*xr = pos[0] / width;
	*yr = pos[1] / height;
}

static void ED_uvs_mouse_pos(SpaceUVs *suvs, ARegion *ar, const int mval[2], float co[2])
{
	ED_uvs_point_stable_pos(suvs, ar, mval[0], mval[1], &co[0], &co[1]);
}

static int uvs_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = uvs_has_buttons_region(sa);

	if (ar != NULL) {
		ED_region_toggle_hidden(C, ar);
	}

	return OPERATOR_FINISHED;
}

void UVS_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->idname = "UVS_OT_properties";
	ot->description = "Toggle the properties region visibility";

	ot->exec = uvs_properties_toggle_exec;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = 0;
}

/********************** view pan operator *********************/

typedef struct ViewPanData {
	float x, y;
	float xof, yof, xorig, yorig;
	int event_type;
	float *vec;
} ViewPanData;

static void view_pan_init(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ViewPanData *vpd;

	op->customdata = vpd = MEM_callocN(sizeof(ViewPanData), "ClipViewPanData");
	WM_cursor_modal_set(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x = event->x;
	vpd->y = event->y;
	vpd->vec = &suvs->xof;

	copy_v2_v2(&vpd->xof, vpd->vec);
	copy_v2_v2(&vpd->xorig, &vpd->xof);

	vpd->event_type = event->type;

	WM_event_add_modal_handler(C, op);
}

static void view_pan_exit(bContext *C, wmOperator *op, bool cancel)
{
	ViewPanData *vpd = op->customdata;

	if (cancel) {
		copy_v2_v2(vpd->vec, &vpd->xorig);

		ED_region_tag_redraw(CTX_wm_region(C));
	}

	WM_cursor_modal_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int view_pan_exec(bContext *C, wmOperator *op)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	float offset[2];

	RNA_float_get_array(op->ptr, "offset", offset);

	suvs->xof += offset[0];
	suvs->yof += offset[1];

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (event->type == MOUSEPAN) {
		SpaceUVs *suvs = CTX_wm_space_uvs(C);
		float offset[2];

		offset[0] = (event->prevx - event->x) / suvs->zoom;
		offset[1] = (event->prevy - event->y) / suvs->zoom;

		RNA_float_set_array(op->ptr, "offset", offset);

		view_pan_exec(C, op);

		return OPERATOR_FINISHED;
	}

	view_pan_init(C, op, event);

	return OPERATOR_RUNNING_MODAL;
}

static int view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ViewPanData *vpd = op->customdata;
	float offset[2];

	switch (event->type) {
		case MOUSEMOVE:
			copy_v2_v2(vpd->vec, &vpd->xorig);
			offset[0] = (vpd->x - event->x) / suvs->zoom;
			offset[1] = (vpd->y - event->y) / suvs->zoom;
			RNA_float_set_array(op->ptr, "offset", offset);
			view_pan_exec(C, op);
			break;
		case ESCKEY:
			view_pan_exit(C, op, 1);

			return OPERATOR_CANCELLED;
		case SPACEKEY:
			view_pan_exit(C, op, 0);

			return OPERATOR_FINISHED;
		default:
			if (event->type == vpd->event_type && event->val == KM_RELEASE) {
				view_pan_exit(C, op, 0);

				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void view_pan_cancel(bContext *C, wmOperator *op)
{
	view_pan_exit(C, op, true);
}

void UVS_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Pan";
	ot->idname = "UVS_OT_view_pan";
	ot->description = "Pan the view";

	/* api callbacks */
	ot->exec = view_pan_exec;
	ot->invoke = view_pan_invoke;
	ot->modal = view_pan_modal;
	ot->cancel = view_pan_cancel;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR;

	/* properties */
	RNA_def_float_vector(ot->srna, "offset", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Offset", "Offset in floating point units, 1.0 is the width and height of the image", -FLT_MAX, FLT_MAX);
}

/******************** view navigation utilities *********************/

static void suvs_zoom_set(const bContext *C, float zoom, float location[2])
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ARegion *ar = CTX_wm_region(C);

	float oldzoom = suvs->zoom;
	int width, height;

	suvs->zoom = zoom;

	if (suvs->zoom < 0.1f || suvs->zoom > 4.0f) {
		/* check zoom limits */
		ED_space_uvs_get_size(suvs, &width, &height);

		width *= suvs->zoom;
		height *= suvs->zoom;

		if ((width < 4) && (height < 4))
			suvs->zoom = oldzoom;
		else if (BLI_rcti_size_x(&ar->winrct) <= suvs->zoom)
			suvs->zoom = oldzoom;
		else if (BLI_rcti_size_y(&ar->winrct) <= suvs->zoom)
			suvs->zoom = oldzoom;
	}

	if ((U.uiflag & USER_ZOOM_TO_MOUSEPOS) && location) {
		float dx, dy;

		ED_space_uvs_get_size(suvs, &width, &height);

		dx = ((location[0] - 0.5f) * width - suvs->xof) * (suvs->zoom - oldzoom) / suvs->zoom;
		dy = ((location[1] - 0.5f) * height - suvs->yof) * (suvs->zoom - oldzoom) / suvs->zoom;

		suvs->xof += dx;
		suvs->yof += dy;
	}
}

static void suvs_zoom_set_factor(const bContext *C, float zoomfac, float location[2])
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);

	suvs_zoom_set(C, suvs->zoom * zoomfac, location);
}

/********************** view zoom in/out operator *********************/

static int view_zoom_in_exec(bContext *C, wmOperator *op)
{
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);

	suvs_zoom_set_factor(C, powf(2.0f, 1.0f / 3.0f), location);

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_zoom_in_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ARegion *ar = CTX_wm_region(C);

	float location[2];

	ED_uvs_mouse_pos(suvs, ar, event->mval, location);
	RNA_float_set_array(op->ptr, "location", location);

	return view_zoom_in_exec(C, op);
}

void UVS_OT_view_zoom_in(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Zoom In";
	ot->idname = "UVS_OT_view_zoom_in";
	ot->description = "Zoom in the view";

	/* api callbacks */
	ot->exec = view_zoom_in_exec;
	ot->invoke = view_zoom_in_invoke;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	prop = RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "Cursor location in screen coordinates", -10.0f, 10.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int view_zoom_out_exec(bContext *C, wmOperator *op)
{
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);

	suvs_zoom_set_factor(C, powf(0.5f, 1.0f / 3.0f), location);

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_zoom_out_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ARegion *ar = CTX_wm_region(C);

	float location[2];

	ED_uvs_mouse_pos(suvs, ar, event->mval, location);
	RNA_float_set_array(op->ptr, "location", location);

	return view_zoom_out_exec(C, op);
}

void UVS_OT_view_zoom_out(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Zoom Out";
	ot->idname = "UVS_OT_view_zoom_out";
	ot->description = "Zoom out the view";

	/* api callbacks */
	ot->exec = view_zoom_out_exec;
	ot->invoke = view_zoom_out_invoke;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	prop = RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "Cursor location in normalized (0.0-1.0) coordinates", -10.0f, 10.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/********************** view zoom ratio operator *********************/

static int uvs_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ARegion *ar = CTX_wm_region(C);

	suvs_zoom_set_factor(C, RNA_float_get(op->ptr, "ratio"), NULL);

	/* ensure pixel exact locations for draw */
	suvs->xof = (int)suvs->xof;
	suvs->yof = (int)suvs->yof;

	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void UVS_OT_view_zoom_ratio(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Ratio";
	ot->idname = "UVS_OT_view_zoom_ratio";
	ot->description = "Set zoom ratio of the view";

	/* api callbacks */
	ot->exec = uvs_view_zoom_ratio_exec;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	RNA_def_float(ot->srna, "ratio", 0.0f, -FLT_MAX, FLT_MAX,
	              "Ratio", "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out", -FLT_MAX, FLT_MAX);
}
