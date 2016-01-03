




// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).




#line 1 "/Users/user/Developer/Xcode Projects/blenderFireIntegration/mantaflowgit/source/fluidsolver.h"
/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey 
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL) 
 * http://www.gnu.org/licenses
 *
 * Main class for the fluid solver
 *
 ******************************************************************************/

#ifndef _FLUIDSOLVER_H
#define _FLUIDSOLVER_H

#include "manta.h"
#include "vectorbase.h"
#include <vector>
#include <map>

namespace Manta { 
	
//! Encodes grid size, timstep etc.

class FluidSolver : public PbClass {public:
	FluidSolver(Vec3i gridSize, int dim=3); static int _W_0 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { PbClass* obj = Pb::objFromPy(_self); if (obj) delete obj; try { PbArgs _args(_linargs, _kwds); pbPreparePlugin(0, "FluidSolver::FluidSolver" ); { ArgLocker _lock; Vec3i gridSize = _args.get<Vec3i >("gridSize",0,&_lock); int dim = _args.getOpt<int >("dim",1,3,&_lock);  obj = new FluidSolver(gridSize,dim); obj->registerObject(_self, &_args); _args.check(); } pbFinalizePlugin(obj->getParent(),"FluidSolver::FluidSolver" ); return 0; } catch(std::exception& e) { pbSetError("FluidSolver::FluidSolver",e.what()); return -1; } }
	virtual ~FluidSolver();
	
	// accessors
	Vec3i getGridSize() { return mGridSize; } static PyObject* _W_1 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(_self)); pbPreparePlugin(pbo->getParent(), "FluidSolver::getGridSize"); PyObject *_retval = 0; { ArgLocker _lock;  pbo->_args.copy(_args);  _retval = toPy(pbo->getGridSize());  pbo->_args.check(); } pbFinalizePlugin(pbo->getParent(),"FluidSolver::getGridSize"); return _retval; } catch(std::exception& e) { pbSetError("FluidSolver::getGridSize",e.what()); return 0; } }
	inline Real  getDt()       { return mDt; }
	inline Real  getDx()       { return 1.0 / mGridSize.max(); }
	inline Real  getTime()     { return mTimeTotal; }

	//! Check dimensionality
	inline bool is2D() const { return mDim==2; }
	//! Check dimensionality
	inline bool is3D() const { return mDim==3; }
	
	void printMemInfo(); static PyObject* _W_2 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(_self)); pbPreparePlugin(pbo->getParent(), "FluidSolver::printMemInfo"); PyObject *_retval = 0; { ArgLocker _lock;  pbo->_args.copy(_args);  _retval = getPyNone(); pbo->printMemInfo();  pbo->_args.check(); } pbFinalizePlugin(pbo->getParent(),"FluidSolver::printMemInfo"); return _retval; } catch(std::exception& e) { pbSetError("FluidSolver::printMemInfo",e.what()); return 0; } }
	
	//! Advance the solver one timestep, update GUI if present
	void step(); static PyObject* _W_3 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(_self)); pbPreparePlugin(pbo->getParent(), "FluidSolver::step"); PyObject *_retval = 0; { ArgLocker _lock;  pbo->_args.copy(_args);  _retval = getPyNone(); pbo->step();  pbo->_args.check(); } pbFinalizePlugin(pbo->getParent(),"FluidSolver::step"); return _retval; } catch(std::exception& e) { pbSetError("FluidSolver::step",e.what()); return 0; } }
	
	//! Update the timestep size based on given maximal velocity magnitude 
	void adaptTimestep(Real maxVel); static PyObject* _W_4 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(_self)); pbPreparePlugin(pbo->getParent(), "FluidSolver::adaptTimestep"); PyObject *_retval = 0; { ArgLocker _lock; Real maxVel = _args.get<Real >("maxVel",0,&_lock);  pbo->_args.copy(_args);  _retval = getPyNone(); pbo->adaptTimestep(maxVel);  pbo->_args.check(); } pbFinalizePlugin(pbo->getParent(),"FluidSolver::adaptTimestep"); return _retval; } catch(std::exception& e) { pbSetError("FluidSolver::adaptTimestep",e.what()); return 0; } }
	
	//! create a object with the solver as its parent
	PbClass* create(PbType type, PbTypeVec T=PbTypeVec(),const std::string& name = ""); static PyObject* _W_5 (PyObject* _self, PyObject* _linargs, PyObject* _kwds) { try { PbArgs _args(_linargs, _kwds); FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(_self)); pbPreparePlugin(pbo->getParent(), "FluidSolver::create"); PyObject *_retval = 0; { ArgLocker _lock; PbType type = _args.get<PbType >("type",0,&_lock); PbTypeVec T = _args.getOpt<PbTypeVec >("T",1,PbTypeVec(),&_lock); const std::string& name = _args.getOpt<std::string >("name",2,"",&_lock);  pbo->_args.copy(_args);  _retval = toPy(pbo->create(type,T,name));  pbo->_args.check(); } pbFinalizePlugin(pbo->getParent(),"FluidSolver::create"); return _retval; } catch(std::exception& e) { pbSetError("FluidSolver::create",e.what()); return 0; } }
	
	// temp grid and plugin stuff: you shouldn't call this manually
	template<class T> T* getGridPointer();
	template<class T> void freeGridPointer(T* ptr);    

	//! expose animation time to python
	Real mDt;static PyObject* _GET_mDt(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mDt); } static int _SET_mDt(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mDt = fromPy<Real  >(val); return 0; }  
	Real mTimeTotal;static PyObject* _GET_mTimeTotal(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mTimeTotal); } static int _SET_mTimeTotal(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mTimeTotal = fromPy<Real  >(val); return 0; }
	int mFrame;static PyObject* _GET_mFrame(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mFrame); } static int _SET_mFrame(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mFrame = fromPy<int  >(val); return 0; }
	//! parameters for adaptive time stepping
	Real mCflCond;static PyObject* _GET_mCflCond(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mCflCond); } static int _SET_mCflCond(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mCflCond = fromPy<Real  >(val); return 0; }  
	Real mDtMin;static PyObject* _GET_mDtMin(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mDtMin); } static int _SET_mDtMin(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mDtMin = fromPy<Real  >(val); return 0; }  
	Real mDtMax;static PyObject* _GET_mDtMax(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mDtMax); } static int _SET_mDtMax(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mDtMax = fromPy<Real  >(val); return 0; }  
	Real mFrameLength;static PyObject* _GET_mFrameLength(PyObject* self, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); return toPy(pbo->mFrameLength); } static int _SET_mFrameLength(PyObject* self, PyObject* val, void* cl) { FluidSolver* pbo = dynamic_cast<FluidSolver*>(Pb::objFromPy(self)); pbo->mFrameLength = fromPy<Real  >(val); return 0; }

protected:
	Vec3i     mGridSize;
	const int mDim;
	Real      mTimePerFrame;
	bool      mLockDt;
	bool      mAdaptDt;
		
	//! subclass for managing grid memory
	//! stored as a stack to allow fast allocation
	template<class T> struct GridStorage {
		GridStorage() : used(0) {}
		T* get(Vec3i size);
		void free();
		void release(T* ptr);
		
		std::vector<T*> grids;
		int used;
	};
	
	GridStorage<int> mGridsInt;
	GridStorage<Real> mGridsReal; 	GridStorage<Vec3> mGridsVec; public: PbArgs _args;}
#define _C_FluidSolver
;

}

#endif

