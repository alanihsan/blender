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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jeroen Bakker
 *                 Monique Dewanchand
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_saturation.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** Pixelate ******************** */

static bNodeSocketTemplate cmp_node_saturation_in[] = {
	{   SOCK_RGBA, 1, N_("Color"),      0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{   -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_saturation_out[] = {
	{   SOCK_RGBA, 0, N_("Color")},
	{   -1, 0, ""   }
};

static void node_composit_init_saturation(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = 0;
	node->custom3 = 1.0f;
}

void register_node_type_cmp_saturation(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_SATURATION, "Saturation", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, cmp_node_saturation_in, cmp_node_saturation_out);
	node_type_init(&ntype, node_composit_init_saturation);

	nodeRegisterType(&ntype);
}
