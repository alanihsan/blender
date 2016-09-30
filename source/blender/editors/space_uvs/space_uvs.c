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

/** \file blender/editors/space_uvs/space_uvs.c
 *  \ingroup spuvs
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_screen.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RNA_access.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvs_intern.h"

/* ******************** manage regions ********************* */

ARegion *uvs_has_buttons_region(ScrArea *sa)
{
	ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);

	if (ar != NULL) {
		return ar;
	}

	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL) {
		return NULL;
	}

	ARegion *arnew = MEM_callocN(sizeof(ARegion), "buttons for image");

	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;

	arnew->flag = RGN_FLAG_HIDDEN;

	return arnew;
}

/* ******************** default callbacks for uvs space ***************** */

static SpaceLink *uvs_new(const bContext *C)
{
	UNUSED_VARS(C);

	SpaceUVs *suvs = MEM_callocN(sizeof(SpaceUVs), "inituvs");
	suvs->spacetype = SPACE_UVS;
	suvs->uspan_min = 0;
	suvs->uspan_max = 1;
	suvs->vspan_min = 0;
	suvs->vspan_max = 1;
	suvs->zoom = 1.0f;

	ARegion *ar;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for uvs texture");

	BLI_addtail(&suvs->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* properties */
	ar = MEM_callocN(sizeof(ARegion), "properties for uvs texture");

	BLI_addtail(&suvs->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_LEFT;
	ar->flag = RGN_FLAG_HIDDEN;

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for uvs texture");

	BLI_addtail(&suvs->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	return (SpaceLink *)suvs;
}

/* not spacelink itself */
static void uvs_free(SpaceLink *sl)
{
	UNUSED_VARS(sl);
}

/* spacetype; init callback */
static void uvs_init(wmWindowManager *wm, ScrArea *sa)
{
	UNUSED_VARS(wm, sa);
}

static SpaceLink *uvs_duplicate(SpaceLink *sl)
{
	SpaceUVs *spuvs = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	return (SpaceLink *)spuvs;
}

static void uvs_operatortypes(void)
{
	WM_operatortype_append(UVS_OT_properties);
	WM_operatortype_append(UVS_OT_view_pan);
	WM_operatortype_append(UVS_OT_view_zoom_in);
	WM_operatortype_append(UVS_OT_view_zoom_out);
	WM_operatortype_append(UVS_OT_view_zoom_ratio);
	WM_operatortype_append(UVS_OT_paint);
	WM_operatortype_append(UVS_OT_add_images);
	WM_operatortype_append(UVS_OT_open_images);
}

static void uvs_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "UVs Generic", SPACE_UVS, 0);

	WM_keymap_add_item(keymap, "UVS_OT_properties", NKEY, KM_PRESS, 0, 0);

	keymap = WM_keymap_find(keyconf, "UVs Editor", SPACE_UVS, 0);

	WM_keymap_add_item(keymap, "UVS_OT_view_pan", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UVS_OT_view_pan", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "UVS_OT_view_pan", MOUSEPAN, 0, 0, 0);

	WM_keymap_add_item(keymap, "UVS_OT_view_zoom_in", WHEELINMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UVS_OT_view_zoom_out", WHEELOUTMOUSE, KM_PRESS, 0, 0);

	/* ctrl now works as well, shift + numpad works as arrow keys on Windows */
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_CTRL, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_CTRL, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_CTRL, 0)->ptr, "ratio", 2.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);

	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "UVS_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);

	WM_keymap_add_item(keymap, "UVS_OT_paint", LEFTMOUSE, KM_PRESS, 0, 0);
}

/* ************************* Main ************************* */

/* add handlers, stuff you only do once or on area/region changes */
static void uvs_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);

	wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "UVs Generic", SPACE_UVS, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "UVs Editor", SPACE_UVS, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void uvs_main_region_listener(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	UNUSED_VARS(sc, sa);

	/* context changes */
	switch (wmn->category) {
		case NC_IMAGE:
			ED_region_tag_redraw(ar);
			break;
	}
}

static void uvs_main_region_set_view2d(SpaceUVs *suvs, ARegion *ar)
{
	const float suvs_zoom = suvs->zoom;
	const float suvs_xof = suvs->xof;
	const float suvs_yof = suvs->yof;

	const float w = 256;
	const float h = 256;

	int winx = BLI_rcti_size_x(&ar->winrct) + 1;
	int winy = BLI_rcti_size_y(&ar->winrct) + 1;

	ar->v2d.tot.xmin = 0;
	ar->v2d.tot.ymin = 0;
	ar->v2d.tot.xmax = w;
	ar->v2d.tot.ymax = h;

	ar->v2d.mask.xmin = ar->v2d.mask.ymin = 0;
	ar->v2d.mask.xmax = winx;
	ar->v2d.mask.ymax = winy;

	/* which part of the image space do we see? */
	float x1 = ar->winrct.xmin + (winx - suvs_zoom * w) / 2.0f;
	float y1 = ar->winrct.ymin + (winy - suvs_zoom * h) / 2.0f;

	x1 -= suvs_zoom * suvs_xof;
	y1 -= suvs_zoom * suvs_yof;

	/* relative display right */
	ar->v2d.cur.xmin = ((ar->winrct.xmin - (float)x1) / suvs_zoom);
	ar->v2d.cur.xmax = ar->v2d.cur.xmin + ((float)winx / suvs_zoom);

	/* relative display left */
	ar->v2d.cur.ymin = ((ar->winrct.ymin - (float)y1) / suvs_zoom);
	ar->v2d.cur.ymax = ar->v2d.cur.ymin + ((float)winy / suvs_zoom);

	/* normalize 0.0..1.0 */
	ar->v2d.cur.xmin /= w;
	ar->v2d.cur.xmax /= w;
	ar->v2d.cur.ymin /= h;
	ar->v2d.cur.ymax /= h;
}

static void draw_buffer(const bContext *C, ARegion *ar, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	int x, y;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	glaDefine2DArea(&ar->winrct);

	/* find window pixel coordinates of origin */
	UI_view2d_view_to_region(&ar->v2d, fx, fy, &x, &y);

	/* this part is generic image display */
	unsigned char *display_buffer;
	void *cache_handle;

	/* TODO(sergey): Ideally GLSL shading should be capable of either
	 * disabling some channels or displaying buffer with custom offset.
	 */
	display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);

	if (display_buffer != NULL) {
		glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_LUMINANCE, GL_UNSIGNED_INT,
		                  display_buffer - (4 - 1));
	}

	if (cache_handle != NULL) {
		IMB_display_buffer_release(cache_handle);
	}

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
}

static void uvs_main_region_draw(const bContext *C, ARegion *ar)
{
	SpaceUVs *suvs = CTX_wm_space_uvs(C);
	uvs_main_region_set_view2d(suvs, ar);

	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	int x1, y1;

	char udim_str[8];
	for (int x = suvs->uspan_min; x < suvs->uspan_max; ++x) {
		for (int y = suvs->vspan_min; y < suvs->vspan_max; ++y) {
			ED_region_grid_draw(ar, 1.0f, 1.0f, x, y);

			if ((suvs->flags & SUV_SHOW_UDIM_NUMBERS) == 0) {
				continue;
			}

			const int udim_index = 1000 + (x + 1) + (y * 10);

			UI_ThemeColor(TH_TEXT);

			sprintf(udim_str, "%d", udim_index);

			UI_view2d_view_to_region(&ar->v2d, x, y, &x1, &y1);
			BLF_draw_default_ascii(x1 + 4.0f * U.pixelsize, y1 + 4.0f * U.pixelsize, 0.0f, udim_str, 4);
		}
	}

	Image *ima = suvs->image;

	if (ima == NULL) {
		return;
	}

	float zoomx, zoomy;
	ED_space_uvs_get_zoom(suvs, ar, &zoomx, &zoomy);

	UDIMTile *tile = ima->udim_tiles.first;

	for (; tile; tile = tile->next) {
		ImBuf *ibuf = BKE_image_acquire_udim_ibuf(tile);

		if (!ibuf) {
			continue;
		}

		const int udim_index = tile->index;
		const int u_index = ((udim_index - 1000) % 10) - 1;
		const int v_index = ((udim_index - 1000) % 100) / 10;

		draw_buffer(C, ar, ibuf, u_index, v_index, zoomx, zoomy);

		BKE_image_release_udim_ibuf(ibuf);
	}
}

/* ************************* Header ************************* */

/* add handlers, stuff you only do once or on area/region changes */
static void uvs_header_region_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_header_init(ar);

	wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "UVs Generic", SPACE_UVS, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void uvs_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void uvs_header_listener(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	UNUSED_VARS(sc, sa, ar, wmn);
	/* context changes */
}

/* ************************* Toolbar ************************* */

static void uvs_tools_region_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_panels_init(wm, ar);

	wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "UVs Generic", SPACE_UVS, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void uvs_tools_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, NULL, -1, true);
}

static void uvs_tools_region_listener(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	UNUSED_VARS(sc, sa);

	/* context changes */
	switch (wmn->category) {
		case NC_IMAGE:
			ED_region_tag_redraw(ar);
			break;
	}
}

/* ************************************************************************** */

/* only called once, from space/spacetypes.c */
void ED_spacetype_uvs(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype uvs texture");
	st->spaceid = SPACE_UVS;
	strncpy(st->name, "UVsTexture", BKE_ST_MAXNAME);

	st->new = uvs_new;
	st->free = uvs_free;
	st->init = uvs_init;
	st->duplicate = uvs_duplicate;
	st->operatortypes = uvs_operatortypes;
	st->keymap = uvs_keymap;

	/* regions: main window */
	ARegionType *art = MEM_callocN(sizeof(ARegionType), "spacetype uvs region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = uvs_main_region_init;
	art->draw = uvs_main_region_draw;
	art->listener = uvs_main_region_listener;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype uvs region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	art->listener = uvs_header_listener;
	art->init = uvs_header_region_init;
	art->draw = uvs_header_region_draw;

	BLI_addhead(&st->regiontypes, art);

	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d buttons region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 180; /* XXX */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = uvs_tools_region_listener;
	art->init = uvs_tools_region_init;
	art->draw = uvs_tools_region_draw;

	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}
