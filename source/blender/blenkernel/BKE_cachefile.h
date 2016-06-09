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
 * Contributor(s): Kevin Dietrich.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_CACHEFILE_H__
#define __BKE_CACHEFILE_H__

/** \file BKE_cachefile.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;

void *BKE_cachefile_add(struct Main *bmain, const char *name);

void BKE_cachefile_filepath_get(struct Scene *scene,
                                struct CacheFile *cache_file,
                                char *r_filename);

float BKE_cachefile_time_offset(struct CacheFile *cache_file, float time);

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_CACHEFILE_H__ */