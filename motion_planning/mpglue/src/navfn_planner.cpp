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

#include "navfn_planner.h"
#include "costmap.h"
#include <navfn.h>

namespace mpglue {
  
  NavFnPlanner::
  NavFnPlanner(boost::shared_ptr<CostmapAccessor const> costmap,
	       boost::shared_ptr<IndexTransform const> itransform,
	       bool _interpolate_plan)
    : CostmapPlanner(stats_, costmap, itransform),
      interpolate_plan_(_interpolate_plan)
  {
  }
  
  
  /**
     \todo find a way to handle cost changes
  */
  void NavFnPlanner::
  preCreatePlan() throw(std::exception)
  {
    bool init_costs(false);
    if ( ! planner_) {
      // NavFn cannot handle negative costmap indices, so cut off at [0][0]
      int const nx(costmap_->getXEnd());
      int const ny(costmap_->getYEnd());
      planner_.reset(new NavFn(nx, ny));
      init_costs = true;
    }
    
    if (start_changed_) {
      int foo[2] = { stats_.start_ix, stats_.start_iy };
      planner_->setStart(foo);
    }
    
    if (goal_pose_changed_) {
      int foo[2] = { stats_.goal_ix, stats_.goal_iy };
      planner_->setGoal(foo);
    }
    
    if (stats_.flush_cost_changes || init_costs) {
      int const nx(costmap_->getXEnd());
      int const ny(costmap_->getYEnd());
      COSTTYPE * cm(planner_->costarr);
      for (int iy(0); iy < ny; ++iy)
	for (int ix(0); ix < nx; ++ix, ++cm) {
	  if (costmap_->isCSpaceObstacle(ix, iy, false))
	    *cm = COST_OBS;
	  else {
	    int cost;
	    if ( ! costmap_->getCost(ix, iy, &cost))
	      *cm = COST_NEUTRAL;	// default to freespace
	    else {
	      *cm = COST_NEUTRAL + COST_FACTOR * cost;
	      if (*cm >= COST_OBS)
		*cm = COST_OBS - 1;
	    }
	  }
	}
    }
  }
  
  
  boost::shared_ptr<waypoint_plan_t> NavFnPlanner::
  doCreatePlan() throw(std::exception)
  {
    boost::shared_ptr<waypoint_plan_t> plan;
    if (planner_->calcNavFnAstar()) {
      plan.reset(new waypoint_plan_t());
      if (interpolate_plan_)
	convertPlanInterpolate(*itransform_, planner_->getPathX(), planner_->getPathY(),
			       planner_->getPathLen(), plan.get(), &stats_.plan_length,
			       &stats_.plan_angle_change, 0);
      else
	convertPlan(*itransform_, planner_->getPathX(), planner_->getPathY(),
		    planner_->getPathLen(), plan.get(), &stats_.plan_length,
		    &stats_.plan_angle_change, 0);
    }
    return plan;
  }
  
  
//   void NavFnPlanner::
//   postCreatePlan() throw(std::exception)
//   {
//   }
  
}
