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
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BIF_gl.h"

#include "ED_screen.h"
#include "ED_space_api.h"

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
}

static void uvs_keymap(wmKeyConfig *keyconf)
{
	fprintf(stderr, "%s\n", __func__);
	wmKeyMap *keymap = WM_keymap_find(keyconf, "UVs Generic", SPACE_UVS, 0);

	WM_keymap_add_item(keymap, "UVS_OT_properties", NKEY, KM_PRESS, 0, 0);
}

/* add handlers, stuff you only do once or on area/region changes */
static void uvs_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	/* do not use here, the properties changed in uvss do a system-wide refresh, then scroller jumps back */
	/*	ar->v2d.flag &= ~V2D_IS_INITIALISED; */

	ar->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

	ED_region_panels_init(wm, ar);
}

static void uvs_main_region_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d = &ar->v2d;

	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	/* only set once */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_MAP1_VERTEX_3);

	/* default grid */
	UI_view2d_multi_grid_draw(v2d, TH_BACK, U.widget_unit, 5, 2);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	/* reset view matrix */
	UI_view2d_view_restore(C);
}

/* add handlers, stuff you only do once or on area/region changes */
static void uvs_header_region_init(wmWindowManager *wm, ARegion *ar)
{
	UNUSED_VARS(wm);
	ED_region_header_init(ar);
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

static void uvs_main_region_listener(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	UNUSED_VARS(sc, sa, ar, wmn);
	/* context changes */
}

/* ************************* Toolbar ************************* */

static void uvs_tools_region_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_panels_init(wm, ar);
}

static void uvs_tools_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, NULL, -1, true);
}

static void uvs_tools_region_listener(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	UNUSED_VARS(sc, sa, ar, wmn);
	/* context changes */
}

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
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

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
