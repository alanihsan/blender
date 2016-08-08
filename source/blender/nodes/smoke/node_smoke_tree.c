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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/node_texture_tree.c
 *  \ingroup nodes
 */


#include <string.h>

#include "node_smoke_util.h"

bNodeTreeType *ntreeType_Smoke;

void register_node_tree_type_smoke(void)
{
	bNodeTreeType *tt = ntreeType_Smoke = MEM_callocN(sizeof(bNodeTreeType), "smoke node tree type");
	
	tt->type = NTREE_SMOKE;
	strcpy(tt->idname, "SmokeNodeTree");
	strcpy(tt->ui_name, "Smoke");
	tt->ui_icon = 0;    /* defined in drawnode.c */
	strcpy(tt->ui_description, "Smoke nodes");
	
#if 0
	tt->foreach_nodeclass = foreach_nodeclass;
	tt->update = update;
	tt->localize = localize;
	tt->local_sync = local_sync;
	tt->local_merge = local_merge;
	tt->get_from_context = texture_get_from_context;
#endif
	
	tt->ext.srna = &RNA_SmokeNodeTree;
	
	ntreeTypeAdd(tt);
}
