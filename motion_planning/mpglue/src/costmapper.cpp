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

#include <mpglue/costmapper.h>
#include <sfl/gplan/Mapper2d.hpp>
#include <sfl/util/numeric.hpp>
#include <costmap_2d/costmap_2d.h>

using namespace mpglue;
using namespace boost;
using namespace std;

#undef HUNT_SEGFAULT_AT_EXIT

namespace {
  
  class sflCostmapper: public mpglue::Costmapper {
  public:
    shared_ptr<sfl::Mapper2d> m2d_;
    mutable shared_ptr<sfl::RDTravmap> rdt_;
    mutable shared_ptr<mpglue::CostmapAccessor> cma_;
    mutable shared_ptr<mpglue::IndexTransform> idxt_;
    int const possibly_circumscribed_cost_;
    
    sflCostmapper(shared_ptr<sfl::Mapper2d> m2d,
		  int possibly_circumscribed_cost)
      : m2d_(m2d), possibly_circumscribed_cost_(possibly_circumscribed_cost) {}

#ifdef HUNT_SEGFAULT_AT_EXIT
    ~sflCostmapper() {
      cerr << "DBG ~mpglue::sflCostmapper() resetting index transform\n";
      idxt_.reset();
      cerr << "DBG ~mpglue::sflCostmapper() resetting costmap accessor\n";
      cma_.reset();
      cerr << "DBG ~mpglue::sflCostmapper() resetting read-travmap\n";
      rdt_.reset();
      cerr << "DBG ~mpglue::sflCostmapper() resetting mapper2d\n";
      m2d_.reset();
      cerr << "DBG ~mpglue::sflCostmapper() DONE\n";
    }
#endif // HUNT_SEGFAULT_AT_EXIT
    
    virtual boost::shared_ptr<mpglue::CostmapAccessor const> getAccessor() const
    {
      if ( ! cma_) {
	rdt_ = m2d_->CreateRDTravmap();
	cma_.reset(mpglue::createCostmapAccessor(rdt_.get(), possibly_circumscribed_cost_));
      }
      return cma_;
    }
    
    virtual boost::shared_ptr<mpglue::IndexTransform const> getIndexTransform() const
    {
      if ( ! idxt_)
	idxt_.reset(mpglue::createIndexTransform(&m2d_->GetGridFrame()));
      return idxt_;
    }
    
    virtual size_t updateObstacles(index_collection_t const * added_obstacle_indices,
				   index_collection_t const * removed_obstacle_indices,
				   ostream * dbgos)
    {
      sfl::Mapper2d::index_buffer_t add_b;
      sfl::Mapper2d::index_buffer_t * add_p(0);
      if (added_obstacle_indices) {
	add_p = &add_b;
	for (index_collection_t::const_iterator ia(added_obstacle_indices->begin());
	     ia != added_obstacle_indices->end(); ++ia)
	  add_b.insert(sfl::Mapper2d::index_t(ia->ix, ia->iy));
	if (dbgos)
	  *dbgos << "sflCostmapper::updateObstacles(): adding " << add_b.size() << " cells\n";
      }
      else if (dbgos)
	*dbgos << "sflCostmapper::updateObstacles(): no cells to add\n";
      
      sfl::Mapper2d::index_buffer_t remove_b;
      sfl::Mapper2d::index_buffer_t * remove_p(0);
      if (removed_obstacle_indices) {
	remove_p = &remove_b;
	for (index_collection_t::const_iterator ir(removed_obstacle_indices->begin());
	     ir != removed_obstacle_indices->end(); ++ir)
	  remove_b.insert(sfl::Mapper2d::index_t(ir->ix, ir->iy));
	if (dbgos)
	  *dbgos << "sflCostmapper::updateObstacles(): removing " << remove_b.size() << " cells\n";
      }
      else if (dbgos)
	*dbgos << "sflCostmapper::updateObstacles(): no cells to remove\n";
      
      return m2d_->UpdateObstacles(add_p, false, remove_p, 0);
    }
    
  };
  
  
  class cm2dCostmapper
    : public mpglue::Costmapper
  {
  public:
    struct cm2dgetter: public mpglue::costmap_2d_getter {
      cm2dgetter(costmap_2d::Costmap2D * cm): cm_(cm) {}
      virtual costmap_2d::Costmap2D * operator () () { return cm_; }
      costmap_2d::Costmap2D * cm_;
    };
    
    boost::shared_ptr<costmap_2d::Costmap2D> cm_;
    boost::shared_ptr<cm2dgetter> getter_;
    boost::shared_ptr<CostmapAccessor> cma_;
    boost::shared_ptr<IndexTransform> idxt_;
    
    cm2dCostmapper(boost::shared_ptr<costmap_2d::Costmap2D> cm)
      : cm_(cm),
	getter_(new cm2dgetter(cm.get())),
	cma_(mpglue::createCostmapAccessor(getter_.get())),
	idxt_(mpglue::createIndexTransform(getter_.get())) {}

#ifdef HUNT_SEGFAULT_AT_EXIT
    ~cm2dCostmapper() {
      cerr << "DBG ~mpglue::cm2dCostmapper() resetting IndexTransform\n";
      idxt_.reset();
      cerr << "DBG ~mpglue::cm2dCostmapper() resetting CostmapAccessor\n";
      cma_.reset();
      cerr << "DBG ~mpglue::cm2dCostmapper() resetting getter\n";
      getter_.reset();
      cerr << "DBG ~mpglue::cm2dCostmapper() resetting Costmap2D\n";
      cm_.reset();
      cerr << "DBG ~mpglue::cm2dCostmapper() DONE\n";
    }
#endif // HUNT_SEGFAULT_AT_EXIT
    
    virtual boost::shared_ptr<CostmapAccessor const> getAccessor() const
    { return cma_; }
    
    virtual boost::shared_ptr<IndexTransform const> getIndexTransform() const
    { return idxt_; }
    
    
    virtual size_t updateObstacles(index_collection_t const * added_obstacle_indices,
				   index_collection_t const * removed_obstacle_indices,
				   std::ostream * dbgos)
    {
      size_t count(0);
      unsigned int bbx0(std::numeric_limits<unsigned int>::max());
      unsigned int bbx1(std::numeric_limits<unsigned int>::min());
      unsigned int bby0(std::numeric_limits<unsigned int>::max());
      unsigned int bby1(std::numeric_limits<unsigned int>::min());
      unsigned int const sizex(cm_->getSizeInCellsX());
      unsigned int const sizey(cm_->getSizeInCellsY());
      
      if (removed_obstacle_indices) {
	for (index_collection_t::const_iterator irem(removed_obstacle_indices->begin());
	     irem != removed_obstacle_indices->end(); ++irem) {
	  if ((irem->ix < 0) || (irem->iy < 0)) {
	    if (dbgos)
	      *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): not removing negative index ["
		     << irem->ix << "][" << irem->iy << "]\n";
	    continue;
	  }
	  unsigned int const ix(irem->ix);
	  unsigned int const iy(irem->iy);
	  if ((ix >= sizex) || (iy >= sizey)) {
	    if (dbgos)
	      *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): not removing out of bounds ["
		     << ix << "][" << iy << "]\n";
	    continue;
	  }
	  if (costmap_2d::LETHAL_OBSTACLE != cm_->getCost(ix, iy)) {
	    if (dbgos)
	      *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): not removing non-lethal ["
		     << ix << "][" << iy << "]\n";
	    continue;
	  }
	  cm_->setCost(ix, iy, costmap_2d::FREE_SPACE);
	  ++count;
	  bbx0 = std::min(bbx0, ix);
	  bbx1 = std::max(bbx1, ix);
	  bby0 = std::min(bby0, iy);
	  bby1 = std::max(bby1, iy);
	}
	if (dbgos)
	  *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): removed " << count << " lethal obstacles\n"
		 << "  bbox: x in " << bbx0 << "-" << bbx1 << "  y in " << bby0 << "-" << bby1 << "\n";
      }
      
      if (added_obstacle_indices) {
	for (index_collection_t::const_iterator iadd(added_obstacle_indices->begin());
	     iadd != added_obstacle_indices->end(); ++iadd) {
	  if ((iadd->ix < 0) || (iadd->iy < 0)) {
	    if (dbgos)
	      *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): not adding negative index ["
		     << iadd->ix << "][" << iadd->iy << "]\n";
	    continue;
	  }
	  unsigned int const ix(iadd->ix);
	  unsigned int const iy(iadd->iy);
	  if ((ix >= sizex) || (iy >= sizey)) {
	    if (dbgos)
	      *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): not adding out of bounds ["
		     << ix << "][" << iy << "]\n";
	    continue;
	  }
	  if (costmap_2d::LETHAL_OBSTACLE == cm_->getCost(ix, iy)) {
	    if (dbgos)
	      *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): not adding already lethal ["
		     << ix << "][" << iy << "]\n";
	    continue;
	  }
	  cm_->setCost(ix, iy, costmap_2d::LETHAL_OBSTACLE);
	  ++count;
	  bbx0 = std::min(bbx0, ix);
	  bbx1 = std::max(bbx1, ix);
	  bby0 = std::min(bby0, iy);
	  bby1 = std::max(bby1, iy);
	}
	if (dbgos)
	  *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): removed and/or added " << count << " lethal obstacles\n"
		 << "  bbox: x in " << bbx0 << "-" << bbx1 << "  y in " << bby0 << "-" << bby1 << "\n";
      }
      
      if (0 < count) {
	if (dbgos)
	  *dbgos << "mpglue::cm2dCostmapper::updateObstacles(): changed " << count << " cells\n";
	// Re-inflate in a region that's 2*inflation bigger than the
	// bounds of the changed cells, after clearing it.  This
	// wastes some operations, but if you want efficiency you
	// probably want to use costmap_2d::Costmap2D directly anyway,
	// without passing through mpglue.
	double const resolution(cm_->getResolution());
	double const inflation2(2 * cm_->getInflationRadius());
	double const wx(resolution * 0.5 * (bbx0 + bbx1));
	double const wy(resolution * 0.5 * (bby0 + bby1));
	double const w_size_x(resolution * (bbx1 - bbx0) + inflation2);
	double const w_size_y(resolution * (bby1 - bby0) + inflation2);
	cm_->reinflateWindow(wx, wy, w_size_x, w_size_y, true);
      }
      
      return count;
    }
    
  };
  
}

namespace mpglue {
  
  
  void ObstacleDelta::
  addIndex(index_pair const & idx)
  {
    added_.insert(idx);
    removed_.erase(idx);
  }
  
  
  void ObstacleDelta::
  removeIndex(index_pair const & idx)
  {
    removed_.insert(idx);
    added_.erase(idx);
  }
  
  
  costmapper_params::
  costmapper_params(costmapper_params const & orig)
    : origin_x(orig.origin_x),
      origin_y(orig.origin_y),
      resolution(orig.resolution),
      inscribed_radius(orig.inscribed_radius),
      circumscribed_radius(orig.circumscribed_radius),
      inflation_radius(orig.inflation_radius),
      ix_end(orig.ix_end),
      iy_end(orig.iy_end)
  {
  }
  
  
  costmapper_params::
  costmapper_params(double _origin_x,
		    double _origin_y,
		    double _resolution,
		    double _inscribed_radius,
		    double _circumscribed_radius,
		    double _inflation_radius,
		    index_t _ix_end,
		    index_t _iy_end)
    : origin_x(_origin_x),
      origin_y(_origin_y),
      resolution(_resolution),
      inscribed_radius(_inscribed_radius),
      circumscribed_radius(_circumscribed_radius),
      inflation_radius(_inflation_radius),
      ix_end(_ix_end),
      iy_end(_iy_end)
  {
  }
  
  
  sfl_costmapper_params::
  sfl_costmapper_params(costmapper_params const & base)
    : costmapper_params(base),
      origin_th(0),
      ix_begin(0),
      iy_begin(0),
      obstacle_cost(0)
  {
  }
  
  
  sfl_costmapper_params::
  sfl_costmapper_params(costmapper_params const & base,
			double _origin_th,
			index_t _ix_begin,
			index_t _iy_begin,
			int _obstacle_cost)
    : costmapper_params(base),
      origin_th(_origin_th),
      ix_begin(_ix_begin),
      iy_begin(_iy_begin),
      obstacle_cost(_obstacle_cost)
  {
  }
  
  
  sfl_costmapper_params::
  sfl_costmapper_params(double origin_x,
			double origin_y,
			double _origin_th,
			double resolution,
			double inscribed_radius,
			double circumscribed_radius,
			double inflation_radius,
			index_t _ix_begin,
			index_t _iy_begin,
			index_t ix_end,
			index_t iy_end,
			int _obstacle_cost)
    : costmapper_params(origin_x,
			origin_y,
			resolution,
			inscribed_radius,
			circumscribed_radius,
			inflation_radius,
			ix_end,
			iy_end),
      origin_th(_origin_th),
      ix_begin(_ix_begin),
      iy_begin(_iy_begin),
      obstacle_cost(_obstacle_cost)
  {
  }
  
  
  cm2d_costmapper_params::
  cm2d_costmapper_params(costmapper_params const & base)
    : costmapper_params(base),
      obstacle_range(8),
      max_obstacle_height(1),
      raytrace_range(8),
      weight(1)
  {
  }
  
  
  cm2d_costmapper_params::
  cm2d_costmapper_params(costmapper_params const & base,
			 double _obstacle_range,
			 double _max_obstacle_height,
			 double _raytrace_range,
			 double _weight)
    : costmapper_params(base),
      obstacle_range(_obstacle_range),
      max_obstacle_height(_max_obstacle_height),
      raytrace_range(_raytrace_range),
      weight(_weight)
  {
  }
  
  
  cm2d_costmapper_params::
  cm2d_costmapper_params(double origin_x,
			 double origin_y,
			 double resolution,
			 double inscribed_radius,
			 double circumscribed_radius,
			 double inflation_radius,
			 index_t ix_end,
			 index_t iy_end,
			 double _obstacle_range,
			 double _max_obstacle_height,
			 double _raytrace_range,
			 double _weight)
    : costmapper_params(origin_x,
			origin_y,
			resolution,
			inscribed_radius,
			circumscribed_radius,
			inflation_radius,
			ix_end,
			iy_end),
      obstacle_range(_obstacle_range),
      max_obstacle_height(_max_obstacle_height),
      raytrace_range(_raytrace_range),
      weight(_weight)
  {
  }
  
  
  shared_ptr<Costmapper> createCostmapper(shared_ptr<sfl::Mapper2d> m2d,
					  int possibly_circumscribed_cost)
  {
    shared_ptr<Costmapper> foo(new sflCostmapper(m2d, possibly_circumscribed_cost));
    return foo;
  }
  
  
  boost::shared_ptr<Costmapper> createCostmapper(boost::shared_ptr<costmap_2d::Costmap2D> cm)
  {
    shared_ptr<Costmapper> foo(new cm2dCostmapper(cm));
    return foo;
  }
  
  
  boost::shared_ptr<Costmapper> createCostmapper(sfl_costmapper_params const & sfl_params)
  {
    shared_ptr<sfl::Mapper2d::travmap_grow_strategy> growstrategy(new sfl::Mapper2d::always_grow());
    double const buffer_zone(sfl_params.inflation_radius - sfl_params.inscribed_radius);
    double const padding_factor(0);
    // costmap_2d seems to use a quadratic decay in r7215
    shared_ptr<sfl::travmap_cost_decay> decay(new sfl::exponential_travmap_cost_decay(2));
    sfl::GridFrame const gframe(sfl_params.origin_x,
				sfl_params.origin_y,
				sfl_params.origin_th,
				sfl_params.resolution);
    shared_ptr<sfl::Mapper2d> m2d(new sfl::Mapper2d(gframe,
						    sfl_params.ix_begin,
						    sfl_params.iy_begin,
						    sfl_params.ix_end,
						    sfl_params.iy_end,
						    sfl_params.inscribed_radius,
						    sfl::maxval(0.0, buffer_zone),
						    padding_factor,
						    0, // freespace cost
						    sfl_params.obstacle_cost,
						    decay,
						    "m2d",
						    sfl::RWlock::Create("m2d"),
						    growstrategy));
    return createCostmapper(m2d, m2d->ComputeCost(sfl_params.circumscribed_radius));
  }
  
  
  boost::shared_ptr<Costmapper> createCostmapper(cm2d_costmapper_params const & cm2d_params)
  {
    vector<unsigned char> static_data(cm2d_params.ix_end * cm2d_params.iy_end);
    unsigned char const lethal_threshold(1);
    
    // If we do not give an initial "static" map to Costmap2D, then
    // it fills everything with NO_INFORMATION, which is actually
    // LETHAL + 1, and we end up thinking the map is filled with
    // obstacles... not very useful. So: create an empty initial
    // map.
    shared_ptr<costmap_2d::Costmap2D>
      cm2d(new costmap_2d::Costmap2D(cm2d_params.ix_end,
				     cm2d_params.iy_end,
				     cm2d_params.resolution,
				     cm2d_params.origin_x,
				     cm2d_params.origin_y,
				     cm2d_params.inscribed_radius,
				     cm2d_params.circumscribed_radius,
				     cm2d_params.inflation_radius,
				     cm2d_params.obstacle_range,
				     cm2d_params.max_obstacle_height,
				     cm2d_params.raytrace_range,
				     cm2d_params.weight,
				     static_data,
				     lethal_threshold));
    return createCostmapper(cm2d);
  }
  
  
  boost::shared_ptr<Costmapper> createCostmapper(costmapper_params const * params)
  {
    sfl_costmapper_params const *
      sfl_params(dynamic_cast<sfl_costmapper_params const *>(params));
    if (sfl_params)
      return createCostmapper(*sfl_params);
    cm2d_costmapper_params const *
      cm2d_params(dynamic_cast<cm2d_costmapper_params const *>(params));
    if (cm2d_params)
      return createCostmapper(*cm2d_params);
    // fall back on cm2d
    return createCostmapper(cm2d_costmapper_params(*params));
  }
  
}
