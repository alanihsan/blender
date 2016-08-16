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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/node_smoke_util.h
 *  \ingroup nodes
 */

#ifndef __NODE_SMOKE_UTIL_H__
#define __NODE_SMOKE_UTIL_H__

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_smoke_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_modifier.h"

#include "node_util.h"
#include "NOD_smoke.h"

#include "RNA_access.h"

#include "BLT_translation.h"

int smoke_node_poll_default(struct bNodeType *ntype, struct bNodeTree *ntree);
void smoke_node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass, short flag);

#endif  /* __NODE_SMOKE_UTIL_H__ */
