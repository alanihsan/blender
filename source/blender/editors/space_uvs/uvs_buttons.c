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

#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BLT_translation.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvs_intern.h"

static void ED_space_uvs_get_size(SpaceUVs *suvs, int *width, int *height)
{
	UNUSED_VARS(suvs);
	*width = *height = IMG_SIZE_FALLBACK;
}

void ED_space_uvs_get_zoom(SpaceUVs *suvs, ARegion *ar, float *zoomx, float *zoomy)
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

/********************** view zoom ratio operator *********************/

static int paint_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	UNUSED_VARS(op);

	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ARegion *ar = CTX_wm_region(C);

	if (event->val == KM_RELEASE) {
		return OPERATOR_FINISHED;
	}

	float mouse[2];
	ED_uvs_mouse_pos(suvs, ar, event->mval, mouse);

	if (mouse[0] < 0.0f || mouse[1] < 0.0f) {
		return OPERATOR_RUNNING_MODAL;
	}

	const int u_index = (int)mouse[0];
	const int v_index = (int)mouse[1];

	if (u_index >= suvs->uspan_max || v_index >= suvs->uspan_max) {
		return OPERATOR_RUNNING_MODAL;
	}

	const int udim_index = 1000 + (u_index + 1) + (v_index * 10);

	fprintf(stderr, "Mouse pos: %f, %f\n", mouse[0], mouse[1]);
	fprintf(stderr, "UDIM tile: %d\n", udim_index);

	return OPERATOR_RUNNING_MODAL;
}

static int paint_exec(bContext *C, wmOperator *op)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	float mouse[2];
	RNA_float_get_array(op->ptr, "location", mouse);

	if (mouse[0] < 0.0f || mouse[1] < 0.0f) {
		return OPERATOR_CANCELLED;
	}

	const int u_index = (int)mouse[0];
	const int v_index = (int)mouse[1];

	if (u_index >= suvs->uspan_max || v_index >= suvs->uspan_max) {
		return OPERATOR_CANCELLED;
	}

	const int udim_index = 1000 + (u_index + 1) + (v_index * 10);

	fprintf(stderr, "Mouse pos: %f, %f\n", mouse[0], mouse[1]);
	fprintf(stderr, "UDIM tile: %d\n", udim_index);

	/* frees op->customdata */
	return OPERATOR_FINISHED;
}

static int paint_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	ARegion *ar = CTX_wm_region(C);

	float location[2];
	ED_uvs_mouse_pos(suvs, ar, event->mval, location);
	RNA_float_set_array(op->ptr, "location", location);

	int retval;

	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		return OPERATOR_FINISHED;
	}

	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);

	return OPERATOR_RUNNING_MODAL;
}

void UVS_OT_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Ratio";
	ot->idname = "UVS_OT_paint";
	ot->description = "Set zoom ratio of the view";

	/* api callbacks */
	ot->modal = paint_stroke_modal;
	ot->invoke = paint_invoke;
	ot->exec = paint_exec;
	ot->poll = ED_operator_uvs_active;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	PropertyRNA *prop;
	prop = RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location",
	                            "Cursor location in normalized (0.0-1.0) coordinates", -10.0f, 10.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* ************************************************************************** */

static void ED_space_uvs_set(SpaceUVs *sima, Object *obedit, Image *ima)
{
	/* change the space ima after because uvedit_face_visible_test uses the space ima
	 * to check if the face is displayed in UV-localview */
	sima->image = ima;

	if (sima->image) {
		BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
	}

	id_us_ensure_real((ID *)sima->image);

	if (obedit) {
		WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
	}

	WM_main_add_notifier(NC_SPACE | ND_SPACE_IMAGE, NULL);
}

enum {
	IMG_RES_256  = 0,
	IMG_RES_512  = 1,
	IMG_RES_1024 = 2,
	IMG_RES_2048 = 3,
	IMG_RES_4096 = 4,
};

static int add_images_exec(bContext *C, wmOperator *op)
{
	/* retrieve state */
	SpaceUVs *sima = CTX_wm_space_uvs(C);
	Object *obedit = CTX_data_edit_object(C);
	Main *bmain = CTX_data_main(C);

	/* TODO(kevin): expose those. */
	const bool floatbuf = true;
	const bool alpha = false;

	char _name[MAX_ID_NAME - 2];
	char *name = _name;

	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "name");
	RNA_property_string_get(op->ptr, prop, name);

	if (!RNA_property_is_set(op->ptr, prop)) {
		/* Default value, we can translate! */
		name = (char *)DATA_(name);
	}

	const int gen_type = 0;
	const int img_res = RNA_enum_get(op->ptr, "img_res");
	int width, height;

	switch (img_res) {
		default:
		case IMG_RES_256:
			width = height = 256;
			break;
		case IMG_RES_512:
			width = height = 512;
			break;
		case IMG_RES_1024:
			width = height = 1024;
			break;
		case IMG_RES_2048:
			width = height = 2048;
			break;
		case IMG_RES_4096:
			width = height = 4096;
			break;
	}

	float color[4];
	RNA_float_get_array(op->ptr, "color", color);

	if (!alpha) {
		color[3] = 1.0f;
	}

	Image *ima = BKE_add_image_no_buffer(bmain, width, height, name, alpha ? 32 : 24, floatbuf, gen_type, color);

	if (!ima) {
		BKE_report(op->reports, RPT_ERROR, "Cannot add images");
		return OPERATOR_CANCELLED;
	}

	for (int u = sima->uspan_min; u < sima->uspan_max; ++u) {
		for (int v = sima->vspan_min; v < sima->vspan_max; ++v) {
			const int udim_index = 1000 + (u + 1) + (v * 10);

			BKE_image_add_udim_tile(ima, width, floatbuf, alpha ? 32 : 24, color, udim_index);
		}
	}

	ED_space_uvs_set(sima, obedit, ima);

	BKE_image_signal(ima, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);

	WM_event_add_notifier(C, NC_IMAGE | NA_ADDED, ima);

	return OPERATOR_FINISHED;
}

static int add_images_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	UNUSED_VARS(event);

	RNA_string_set(op->ptr, "name", DATA_("Untitled"));
	return WM_operator_props_dialog_popup(C, op, 15 * UI_UNIT_X, 5 * UI_UNIT_Y);
}

static void image_new_draw(bContext *C, wmOperator *op)
{
	UNUSED_VARS(C);

	uiLayout *split, *col[2];
	uiLayout *layout = op->layout;
	PointerRNA ptr;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

	/* copy of WM_operator_props_dialog_popup() layout */

	split = uiLayoutSplit(layout, 0.5f, false);
	col[0] = uiLayoutColumn(split, false);
	col[1] = uiLayoutColumn(split, false);

	uiItemL(col[0], IFACE_("Name"), ICON_NONE);
	uiItemR(col[1], &ptr, "name", 0, "", ICON_NONE);

	uiItemL(col[0], IFACE_("Resolution"), ICON_NONE);
	uiItemR(col[1], &ptr, "img_res", 0, "", ICON_NONE);

	uiItemL(col[0], IFACE_("Color"), ICON_NONE);
	uiItemR(col[1], &ptr, "color", 0, "", ICON_NONE);
}

void UVS_OT_add_images(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Image";
	ot->idname = "UVS_OT_add_images";
	ot->description = "Set zoom ratio of the view";

	/* api callbacks */
	ot->invoke = add_images_invoke;
	ot->exec = add_images_exec;
	ot->ui = image_new_draw;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */

	static float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	static EnumPropertyItem img_res_items[] = {
		{IMG_RES_256,  "IMG_RES_256",  0, "256x256",   ""},
	    {IMG_RES_512,  "IMG_RES_512",  0, "512x512",   ""},
	    {IMG_RES_1024, "IMG_RES_1024", 0, "1024x1024", ""},
	    {IMG_RES_2048, "IMG_RES_2048", 0, "2048x2048", ""},
	    {IMG_RES_4096, "IMG_RES_4096", 0, "4096x4096", ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	RNA_def_string(ot->srna, "name", "Untitled", MAX_ID_NAME - 2, "Name", "Image data-block name");
	prop = RNA_def_float_color(ot->srna, "color", 4, NULL, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
	RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
	RNA_def_property_float_array_default(prop, default_color);
	RNA_def_enum(ot->srna, "img_res", img_res_items, IMG_RES_1024, "Resolution", "Resolution of the generated images");
}
