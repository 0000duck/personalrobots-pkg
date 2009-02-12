/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* \author Ioan Sucan */

#ifndef OMPL_DATASTRUCTURES_GRID_X_
#define OMPL_DATASTRUCTURES_GRID_X_

#include "ompl/datastructures/GridN.h"
#include "ompl/datastructures/BinaryHeap.h"

namespace ompl
{
    
    /** This class defines a grid that keeps track of its boundary:
     * it distinguishes between interior and exterior cells */
    template < typename _T,
	       class LessThanExternal = std::less<_T>,
	       class LessThanInternal = LessThanExternal >
    
    class GridX : public GridN<_T>
    {
    protected:
	
        struct CellX : public GridN<_T>::Cell
	{
	    
	    CellX(void) : GridN<_T>::Cell()
	    {
	    }
	    
	    virtual ~CellX(void)
	    {
	    }

	    void           *heapElement;
	};

    public:
	
	typedef void (*EventCellUpdate)(typename GridN<_T>::Cell*, void*);

        explicit
	GridX(unsigned int dimension) : GridN<_T>(dimension)
	{
	    setupHeaps();
	}
	
        virtual ~GridX(void)
	{
	    clearHeaps();
	}
	
	void onCellUpdate(EventCellUpdate event, void *arg)
	{
	    m_eventCellUpdate = event;
	    m_eventCellUpdateData = arg;
	}

	typename GridN<_T>::Cell* topInternal(void) const
	{
	    typename GridN<_T>::Cell* top = static_cast<typename GridN<_T>::Cell*>(m_internal.top()->data);
	    return top ? top : topExternal();
	}
	
	typename GridN<_T>::Cell* topExternal(void) const
	{
	    typename GridN<_T>::Cell* top = static_cast<typename GridN<_T>::Cell*>(m_external.top()->data);
	    return top ? top : topInternal();
	}
	
	unsigned int countInternal(void) const
	{
	    return m_internal.size();
	}
	
	unsigned int countExternal(void) const
	{
	    return m_external.size();
	}
	
	double fracExternal(void) const
	{
	    return m_external.empty() ? 0.0 : (double)(m_external.size()) / (double)(m_external.size() + m_internal.size());
	}
	
	double fracInternal(void) const
	{
	    return 1.0 - fracExternal();
	}
	
	void update(typename GridN<_T>::Cell* cell)
	{
	    m_eventCellUpdate(cell, m_eventCellUpdateData);
	    if (cell->border)
		m_external.update(reinterpret_cast<typename externalBHeap::Element*>
				  (static_cast<CellX*>(cell)->heapElement));
	    else
		m_internal.update(reinterpret_cast<typename internalBHeap::Element*>
				  (static_cast<CellX*>(cell)->heapElement));
	}
    
	void updateAll(void)
	{
	    std::vector< typename GridN<_T>::Cell* > cells;
	    getCells(cells);
	    for (int i = cells.size() - 1 ; i >= 0 ; --i)
		m_eventCellUpdate(cells[i], m_eventCellUpdateData);
	    m_external.rebuild();
	    m_internal.rebuild();
	}
	
	virtual typename GridN<_T>::Cell* createCell(const typename GridN<_T>::Coord& coord,
						     typename GridN<_T>::CellArray *nbh = NULL)
	{
	    CellX* cell = new CellX();
	    cell->coord = coord;

	    typename GridN<_T>::CellArray *list = nbh ? nbh : new typename GridN<_T>::CellArray;
	    neighbors(cell->coord, *list);

	    for (typename GridN<_T>::CellArray::iterator cl = list->begin() ; cl != list->end() ; ++cl)
	    {
		CellX* c = static_cast<CellX*>(*cl);
		bool wasBorder = c->border;
		c->neighbors++;
		if (c->border && c->neighbors >= GridN<_T>::m_interiorCellNeighborsLimit)
		    c->border = false;
		
		m_eventCellUpdate(c, m_eventCellUpdateData);
		
		if (c->border)
		    m_external.update(reinterpret_cast<typename externalBHeap::Element*>(c->heapElement));
		else
		{
		    if (wasBorder)
		    {
			m_external.remove(reinterpret_cast<typename externalBHeap::Element*>(c->heapElement));
			m_internal.insert(c);
		    }
		    else
			m_internal.update(reinterpret_cast<typename internalBHeap::Element*>(c->heapElement));
		}
	    }
	    
	    cell->neighbors = GridN<_T>::numberOfBoundaryDimensions(cell->coord) + list->size();
	    if (cell->border && cell->neighbors >= GridN<_T>::m_interiorCellNeighborsLimit)
		cell->border = false;
	    
	    if (!nbh)
		delete list;

	    return static_cast<typename GridN<_T>::Cell*>(cell);
	}
	

	virtual void add(typename GridN<_T>::Cell* cell)
	{
	    CellX* ccell = static_cast<CellX*>(cell);
	    m_eventCellUpdate(ccell, m_eventCellUpdateData);
	    
	    GridN<_T>::add(cell);
	    
	    if (cell->border)
		m_external.insert(ccell);
	    else
		m_internal.insert(ccell);
	}

	virtual bool remove(typename GridN<_T>::Cell* cell)
	{
	    if (cell)
	    {
		typename GridN<_T>::CellArray *list = new typename GridN<_T>::CellArray();
		neighbors(cell->coord, *list);

		for (typename GridN<_T>::CellArray::iterator cl = list->begin() ; cl != list->end() ; ++cl)
		{
		    CellX* c = static_cast<CellX*>(*cl);
		    bool wasBorder = c->border;
		    c->neighbors--;
		    if (!c->border && c->neighbors < GridN<_T>::m_interiorCellNeighborsLimit)
			c->border = true;
		    
		    m_eventCellUpdate(c, m_eventCellUpdateData);
		    
		    if (c->border)
		    {
			if (wasBorder)
			    m_external.update(reinterpret_cast<typename externalBHeap::Element*>(c->heapElement));
			else
			{
			    m_internal.remove(reinterpret_cast<typename internalBHeap::Element*>(c->heapElement));
			    m_external.insert(c);
			}
		    }
		    else
			m_internal.update(reinterpret_cast<typename internalBHeap::Element*>(c->heapElement));
		}
		
		delete list;

		typename GridN<_T>::iterator pos = GridN<_T>::m_hash.find(&cell->coord);
		if (pos != GridN<_T>::m_hash.end())
		{
		    GridN<_T>::m_hash.erase(pos);
		    CellX* cx = static_cast<CellX*>(cell);
		    if (cx->border)
			m_external.remove(reinterpret_cast<typename externalBHeap::Element*>(cx->heapElement));
		    else
			m_internal.remove(reinterpret_cast<typename internalBHeap::Element*>(cx->heapElement));
		    return true;
		}
	    }
	    return false;
	}
	
	virtual void clear(void)
	{
	    GridN<_T>::clear();
	    clearHeaps();	    
	}
	
    protected:
	
	EventCellUpdate          m_eventCellUpdate;
	void                    *m_eventCellUpdateData;

        static void noCellUpdate(typename GridN<_T>::Cell*, void *)
	{
	}
    
	void setupHeaps(void)
	{
	    m_eventCellUpdate     = noCellUpdate;
	    m_eventCellUpdateData = NULL;
	    m_internal.onAfterInsert(setHeapElementI, NULL);
	    m_external.onAfterInsert(setHeapElementE, NULL);
	}
	
	void clearHeaps(void)
	{
	    m_internal.clear();
	    m_external.clear();
	}
	
	void updateFrac(void) const
	{
	}
	
	struct LessThanInternalCell
	{
	    bool operator()(const CellX* const a, const CellX* const b) const
	    {
		return LessThanInternal()(a->data, b->data);
	    }	    
	};
	
	struct LessThanExternalCell
	{
	    bool operator()(const CellX* const a, const CellX* const b) const
	    {
		return LessThanExternal()(a->data, b->data);
	    }
	};
	
	typedef BinaryHeap< CellX*, LessThanInternalCell > internalBHeap;
	typedef BinaryHeap< CellX*, LessThanExternalCell > externalBHeap;
	
	static void setHeapElementI(typename internalBHeap::Element* element, void *)
	{
	    element->data->heapElement = reinterpret_cast<void*>(element);
	}

        static void setHeapElementE(typename externalBHeap::Element* element, void *)
	{
	    element->data->heapElement = reinterpret_cast<void*>(element);
	}	

	internalBHeap m_internal;
	externalBHeap m_external;
    };
    
}

#endif
