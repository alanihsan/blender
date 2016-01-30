/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "levelset.h"
#include "scene.h"
#include "util_foreach.h"
#include "util_progress.h"
#include <stdio.h>

openvdb::FloatGrid::Ptr last_level_set;

CCL_NAMESPACE_BEGIN

// TODO(kevin): de-duplicate this with the ones in util_volume.cpp, keeping them
// here for now.
template <typename T>
void release_map_memory(unordered_map<pthread_t, T> &map)
{
	typename unordered_map<pthread_t, T>::iterator iter;

	for(iter = map.begin(); iter != map.end(); ++iter) {
		delete iter->second;
	}
}

template <typename IsectorType>
void create_isectors_threads(unordered_map<pthread_t, IsectorType *> &isect_map,
                             const vector<pthread_t> &thread_ids,
                             const IsectorType &main_isect)
{
	release_map_memory(isect_map);

	pthread_t my_thread = pthread_self();

	for (size_t i = 0; i < thread_ids.size(); ++i) {
		if (isect_map.find(thread_ids[i]) == isect_map.end()) {
			isect_map[thread_ids[i]] = new IsectorType(main_isect);
		}
	}

	if (isect_map.find(my_thread) == isect_map.end()) {
		isect_map[my_thread] = new IsectorType(main_isect);
	}
}

void OpenVDB_initialize()
{
	openvdb::initialize();
}

void OpenVDB_file_read_to_levelset(const char* filename, Scene* /*scene*/, LevelSet* levelset, int shader )
{
	using namespace openvdb;
	OpenVDB_initialize();
	openvdb::FloatGrid::Ptr level_set_ptr;
	try {
		io::File file(filename);
		file.open();

		size_t size = file.getSize();
		printf("Opening %s, is %lu bytes!\n", filename, size);

		for (io::File::NameIterator iter = file.beginName(); iter != file.endName(); ++iter) {
			printf("Reading grid %s!\n", iter.gridName().c_str());

			GridBase::Ptr grid = file.readGrid(iter.gridName());
			grid->print();

			if(grid->getGridClass() != GRID_LEVEL_SET)
				continue;

			if (grid->isType<FloatGrid>())
				level_set_ptr = gridPtrCast<openvdb::FloatGrid>(grid);
			else
				printf("No FloatGrid, ignoring!\n");
		}

		file.close();
	}
	catch (const IoError &e) {
		std::cerr << e.what() << "\n";
	}

	levelset->initialize( level_set_ptr, shader );
}

LevelSet::LevelSet()
{
}

LevelSet::LevelSet(openvdb::FloatGrid::Ptr gridPtr, int shader_)
    : grid(gridPtr), shader(shader_)
{
	main_isect = new isect_t(*grid);
	create_isectors_threads(isect_map, TaskScheduler::thread_ids(), *main_isect);
}

LevelSet::LevelSet( const LevelSet& levelset )
    : grid( levelset.grid ), shader( levelset.shader )
{
	main_isect = new isect_t(*grid);
	//printf( "LevelSet Copy Constructor\n" );

	// printf( "Initializing thread accessor mapping from thread %u.\n", pthread_self() );
	create_isectors_threads(isect_map, TaskScheduler::thread_ids(), *main_isect);
}

LevelSet::~LevelSet()
{
	release_map_memory(isect_map);
	delete main_isect;
	isect_map.clear();
}

void LevelSet::initialize(openvdb::FloatGrid::Ptr& gridPtr, int shader_)
{
	// printf( "LevelSet Post-Construction Initializer\n" );
	grid.swap(gridPtr);
	shader = shader_;
	main_isect = new isect_t(*grid);

	//printf( "Initializing thread accessor mapping from thread %u.\n", pthread_self() );
	create_isectors_threads(isect_map, TaskScheduler::thread_ids(), *main_isect);
}

void LevelSet::tag_update(Scene *scene)
{
	scene->level_set_manager->need_update = true;
}

bool LevelSet::intersect(const Ray* ray, Intersection *isect)
{
	pthread_t thread = pthread_self();
	isect_map_t::iterator iter = isect_map.find(thread);
	assert(iter != isect_map.end());
	isect_t *isector = iter->second;

	vdb_ray_t::Vec3Type P(ray->P.x, ray->P.y, ray->P.z);
	vdb_ray_t::Vec3Type D(ray->D.x, ray->D.y, ray->D.z);
	D.normalize();

	float max_ray_t;
	if( ray->t  > 100000.0f ){
		//printf( "Ray time exceeded max cap: %f, capping to 100000.\n", ray->t );
		max_ray_t = 100000.0f;}
	else
		max_ray_t = ray->t;

	vdb_ray_t vdbray(P, D, 1e-5f, max_ray_t);
	vdb_ray_t::Vec3Type pos(0.0), normal;
	double t;

	bool intersects = false;

	try {
		intersects = isector->intersectsWS(vdbray, pos, normal, t);
	}
	catch(...) {
		printf( "OpenVDB intersection test threw an exception. Something is wrong, but trying to ignore.\n" );

		//printf( "Ray (eye): %f %f %f\n", vdbray.eye()[0], vdbray.eye()[1], vdbray.eye()[2] );
		//printf( "Ray (dir): %f %f %f\n", vdbray.dir()[0], vdbray.dir()[1], vdbray.dir()[2] );
		//printf( "Ray (timespan): %f %f\n", vdbray.t0(), vdbray.t1());
		return false;
	}

	if(intersects) {
		isect->t = static_cast<float>(t);
		isect->u = isect->v = 1.0f / 3.0f;
		isect->type = PRIMITIVE_LEVEL_SET;
		isect->shad = shader;
		float x = static_cast<float>(normal.x());
		float y = static_cast<float>(normal.y());
		float z = static_cast<float>(normal.z());
		isect->norm = (make_float3(x, y, z));
		isect->prim = 0;

		/* Do this to avoid instancing code. Since levelsets are not part of the
		 * BVH, object instancing does not apply. */
		isect->object = OBJECT_NONE;

		return true;
	}

	return false;
}

LevelSetManager::LevelSetManager()
{
	need_update = true;
}

LevelSetManager::~LevelSetManager()
{
}

void LevelSetManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	device_free(device, dscene);

	progress.set_status("Updating Level Sets", "Copying Level Sets to device");

	dscene->data.tables.num_level_sets = scene->level_sets.size();

	if (scene->level_sets.size() > 0) {
		/* Allocate a memory pool big enough for all LevelSets */
		LevelSet **levelset_pool = new LevelSet*[dscene->data.tables.num_level_sets];

		for(int ls = 0; ls < dscene->data.tables.num_level_sets; ls++) {

			/* We need to protect against potential leaks due to failed construction */
			try {
				levelset_pool[ls] = new LevelSet(*(scene->level_sets[ls]));
			}
			catch (...) {
				/* if something goes wrong, rewind all constructed entries... */
				for(int rls = ls-1; rls >= 0; rls--) {
					delete levelset_pool[rls];
				}

				delete [] levelset_pool;

				/* finally, toss the exception upward */
				throw;
			}
		}

		dscene->data.tables.level_sets = static_cast<void *>(levelset_pool);
	}

	if(progress.get_cancel()) return;

	need_update = false;
}

void LevelSetManager::device_free(Device */*device*/, DeviceScene *dscene)
{
	if(dscene->data.tables.num_level_sets > 0) {
		LevelSet **levelset_pool = static_cast<LevelSet **>(dscene->data.tables.level_sets);

		for(int ls = 0; ls < dscene->data.tables.num_level_sets; ls++) {
			delete levelset_pool[ls];
		}

		delete [] levelset_pool;

		dscene->data.tables.level_sets = NULL;
		dscene->data.tables.num_level_sets = 0;
	}
}

void LevelSetManager::tag_update(Scene */*scene*/)
{
	need_update = true;
}

CCL_NAMESPACE_END
