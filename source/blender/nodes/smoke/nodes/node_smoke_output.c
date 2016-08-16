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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_smoke_util.h"

#include "NOD_smoke.h"

static bNodeSocketTemplate inputs[] = {
	{ SOCK_STRING, 1, N_("Input") },
	{ -1, 0, "" }
};

void register_node_type_smoke_output(void)
{
	static bNodeType ntype;

	smoke_node_type_base(&ntype, SMK_NODE_OUTPUT, "Output", NODE_CLASS_OUTPUT, 0);
	node_type_socket_templates(&ntype, inputs, NULL);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_storage(&ntype, "SmokeNodeOutput", node_free_standard_storage, NULL);
	node_type_exec(&ntype, NULL, NULL, NULL);

	/* Do not allow muting output. */
	node_type_internal_links(&ntype, NULL);

	nodeRegisterType(&ntype);
}
