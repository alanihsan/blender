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

#include "COM_SaturationOperation.h"

#include "BLI_math.h"

SaturationOperation::SaturationOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
}

void SaturationOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
}

void SaturationOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}

void SaturationOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float inputValue[4];
	this->m_inputOperation->readSampled(inputValue, x, y, sampler);

	float value = rgb_to_grayscale(inputValue);

	float gray_scale[3] = {
	    value, value, value
	};

	switch (this->m_mode) {
		case 0: // Rec 709
		{
			copy_v3_fl(gray_scale, rgb_to_grayscale(inputValue));
			break;
		}
		case 1: // Ccir 601
		{
			const float ccir = (inputValue[0] * 0.299f + inputValue[1] * 0.587f + inputValue[2] * 0.114f);
			copy_v3_fl(gray_scale, ccir);
			break;
		}
		case 2: // Average
		{
			const float average = (inputValue[0] + inputValue[1] + inputValue[2]);
			copy_v3_fl(gray_scale, average / 3.0f);
			break;
		}
		case 3: // Maximum
		{
			copy_v3_fl(gray_scale, max_fff(inputValue[0], inputValue[1], inputValue[2]));
			break;
		}
	}

	if (this->m_saturation == 0.0f) {
		copy_v3_v3(output, gray_scale);
	}
	else {
		interp_v3_v3v3(output, gray_scale, inputValue, this->m_saturation);
	}

	output[3] = inputValue[3];
}
