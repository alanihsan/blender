/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor:
 *		Kevin Dietrich
 */

#include "COM_SaturationNode.h"

#include "COM_SaturationOperation.h"
#include "COM_ExecutionSystem.h"

#include "BKE_node.h"

SaturationNode::SaturationNode(bNode *editorNode)
    : Node(editorNode)
{}

void SaturationNode::convertToOperations(NodeConverter &converter, const CompositorContext &/*context*/) const
{
	bNode *bnode = this->getbNode();

	NodeInput *inputSocket = this->getInputSocket(0);
	NodeOutput *outputSocket = this->getOutputSocket(0);

	SaturationOperation *operation = new SaturationOperation();
	operation->setMode(bnode->custom1);
	operation->setSaturation(bnode->custom3);

	converter.addOperation(operation);

	converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
	converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
}
