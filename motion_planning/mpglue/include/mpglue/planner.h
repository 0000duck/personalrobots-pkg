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

/** \file planner.h Uniform handling of various costmap planners. */

#ifndef MPGLUE_PLANNER_HPP
#define MPGLUE_PLANNER_HPP

#include <mpglue/plan.h>
#include <mpglue/costmap.h>
#include <boost/shared_ptr.hpp>
#include <stdexcept>

namespace mpglue {
  
  
  std::string canonicalPlannerName(std::string const & name_or_alias);
  
  
  class CostmapAccessor;
  class IndexTransform;
  
  
  struct CostmapPlannerStats {
    CostmapPlannerStats();
    virtual ~CostmapPlannerStats();
    virtual CostmapPlannerStats * copy() const;
    
    double start_x;	     /**< global X-coordinate of start pose */
    double start_y;	     /**< global Y-coordinate of start pose */
    double start_th;	     /**< global heading of start pose */
    ssize_t start_ix;        /**< X-index of the start in the costmap frame */
    ssize_t start_iy;        /**< Y-index of the start in the costmap frame */
    double goal_x;	     /**< global X-coordinate of goal pose */
    double goal_y;	     /**< global X-coordinate of goal pose */
    double goal_th;	     /**< global heading of goal pose */
    ssize_t goal_ix;	     /**< X-index of the goal in the costmap frame */
    ssize_t goal_iy;	     /**< Y-index of the goal in the costmap frame */
    double goal_tol_distance;/**< maximum distance at which to stop planning */
    double goal_tol_angle;   /**< maximum heading error at which to stop planning */
    
    bool plan_from_scratch;  /**< whether to discard any previous solutions */
    size_t cost_delta_count; /**< number of cells that changed costs (see
				  CostmapPlanner::flushCostChanges()). */
    
    bool success;             /**< whether planning was successfull */
    double actual_time_wall;  /**< duration actually used for planning (wallclock) */
    double actual_time_user;  /**< duration actually used for planning (user time) */
    
    // XXXX to do: avoid forcing subclass implementers to update these manually...
    double plan_length;       /**< cumulated Euclidean distance between planned waypoints */
    double plan_angle_change; /**< cumulated abs(delta(angle)) along planned waypoints */
    
    /** Append this entry to a logfile (which is opened and closed
	each time). Ends up calling the polymorphic logStream(), so
	subclasses need not bother with logFile(). */
    void logFile(char const * filename,
		 std::string const & title,
		 std::string const & prefix) const;
    
    /** Append this entry to a stream. Subclasses should override this
	method, and start by calling their base class' implementation,
	then append their specific entries. The title parameter is
	only used in the topmost implementation, subclasses just have
	to pass it upwards. */
    virtual void logStream(std::ostream & os,
			   std::string const & title,
			   std::string const & prefix) const;
    
    /** Dump in XML to a stream, like this:
	\code
	<mpglue_costmap_planner_stats>
	  <type> name of (sub)class </type>
	  <goal> x y th ix iy tol_dist tol_ang </goal>
	  <start> x y th ix iy </start>
	  <plan_from_scratch> true or false </plan_from_scratch>
	  <!--- etc etc --->
	</mpglue_costmap_planner_stats>
	\endcode
	
	The subclass name is retrieved using getClassName(), and
	dumpSubclassXML() is called before the closing
	<code>&lt;/mpglue_costmap_planner_stats&gt;</code>
    */
    virtual void dumpXML(std::ostream & os,
			 std::string const & prefix) const;
    
    /** base implementation returns "mpglue::CostmapPlannerStats" */
    virtual char const * getClassName() const;
    
    /** empty base implementation */
    virtual void dumpSubclassXML(std::ostream & os,
				 std::string const & prefix) const;
  };
  
  
  class CostmapPlanner
  {
  public:
    virtual ~CostmapPlanner();
    
    /** Default implementation just stores the start and its
	grid-coordinates in the stats__ field. */
    virtual void setStart(double px, double py, double pth) throw(std::out_of_range);
    
    /** Default implementation just stores the goal and its
	grid-coordinates in the stats__ field. */
    virtual void setGoal(double px, double py, double pth) throw(std::out_of_range);
    
    /** Default implementation just stores the goal tolerance in the
	stats__ field. */
    virtual void setGoalTolerance(double dist_tol, double angle_tol);
    
    /** Default implementation just sets a flag in the stats__
	field. */
    virtual void forcePlanningFromScratch(bool flag);
    
    /** Sets the CostmapPlannerStats::cost_delta_count field to the
	size of the given cost_delta_map_t, and then calls
	doFlushCostChanges() where subclasses can implement the actual
	handling of the cost changes.
	
	\note If opt_cost_delta_map is NULL, then the cost_delta_count
	is set to zero and doFlushCostChanges() does NOT get
	called. On the other hand, if the pointer opt_cost_delta_map
	is non-NULL, but the opt_cost_delta_map has size zero,
	doFlushCostChanges() DOES get called.
    */
    void flushCostChanges(cost_delta_map_t const * opt_cost_delta_map);
    
    /** Calls preCreatePlan(), doCreatePlan(), and postCreatePlan() in
	that order. If any of them throw an exception, the following
	are not called and the CostmapPlannerStats::success flag will
	be false. If doCreatePlan() returns a null plan, this is
	likewise considered a failure. However, if it returns an empty
	but non-null plan, it is considered a success.
	
	\note The base class measures the execution times, but if an
	exception occurrs in doCreatePlan() then the fields
	CostmapPlannerStats::actual_time_wall and
	CostmapPlannerStats::actual_time_user are not consistent.
    */
    boost::shared_ptr<waypoint_plan_t> createPlan() throw(std::exception);
    
    /** Default implementation just calls CostmapPlannerStats::copy().
	Subclasses can rely on the polymorphic implementation thereof
	to do the right thing. */
    virtual boost::shared_ptr<CostmapPlannerStats> copyStats() const;
    
    /** Read-only access to the stats entity owned by the planner.  If
	you want to accumulate statistics, you should use copyStats()
	instead, as this stats instance keeps being updated. */
    CostmapPlannerStats const & getStats() const { return stats__; }
    
  protected:
    /** Subclasses have to provide a stats field, which can be (and
	typically is) a subclass of CostmapPlannerStats. This allows
	polymorphic handling of planning statistics throughout the
	planner hierarchy. */
    CostmapPlanner(CostmapPlannerStats & stats,
		   boost::shared_ptr<CostmapAccessor const> costmap,
		   boost::shared_ptr<IndexTransform const> itransform);
    
    virtual void doFlushCostChanges(cost_delta_map_t const & delta) = 0;
    
    /** Hook for subclasses. Called by createPlan() before calling
	doCreatePlan(). Subclasses must call their superclass'
	implementation at the end of their override. */
    virtual void preCreatePlan() throw(std::exception);
    
    /** Called by createPlan(). Subclasses implement their "core
	algorithm" here. */
    virtual boost::shared_ptr<waypoint_plan_t> doCreatePlan() throw(std::exception) = 0;
    
    /** Hook for subclasses. Called by createPlan() after calling
	doCreatePlan(). Subclasses must call their superclass'
	implementation at the beginning of their override. The base
	implementation here simply resets the foo_changed_ flags. */
    virtual void postCreatePlan() throw(std::exception);
    
    /** \note Two underscores because implementing subclasses might
	want to use stats_ for the specific storage they require. */    
    CostmapPlannerStats & stats__;
    boost::shared_ptr<CostmapAccessor const> costmap_;
    boost::shared_ptr<IndexTransform const> itransform_;
    
    bool start_changed_;
    bool goal_pose_changed_;
    bool goal_tol_changed_;
    bool plan_from_scratch_changed_;
    bool flush_cost_changes_changed_;
  };
  
  
  struct AnytimeCostmapPlannerStats
    : public CostmapPlannerStats {
    AnytimeCostmapPlannerStats();
    virtual AnytimeCostmapPlannerStats * copy() const;
    
    bool stop_at_first_solution; /**< whether to just plan until any plan is found */
    double allocated_time;       /**< duration available for planning */
    
    virtual void logStream(std::ostream & os,
			   std::string const & title,
			   std::string const & prefix) const;
    
    virtual char const * getClassName() const;
    
    virtual void dumpSubclassXML(std::ostream & os,
				 std::string const & prefix) const;
  };
  
  
  class AnytimeCostmapPlanner
    : public CostmapPlanner
  {
  public:
    /** Default implementation just sets a flag in the stats__
	field. */
    virtual void stopAtFirstSolution(bool flag);
    
    /** Default implementation just sets allocated_time in the
	stats__ field. */
    virtual void setAllocatedTime(double seconds);
    
  protected:
    AnytimeCostmapPlanner(AnytimeCostmapPlannerStats & stats,
			  boost::shared_ptr<CostmapAccessor const> costmap,
			  boost::shared_ptr<IndexTransform const> itransform);
    
    /** Calls CostmapPlanner::postCreatePlan() and then likewise
	resets the foo_changed_ flags. */
    virtual void postCreatePlan() throw(std::exception);
    
    /** \note This shadows the identically named stats in
	CostmapPlanner, which is OK because it ends up being the same
	object anyway. */    
    AnytimeCostmapPlannerStats & stats__;

    bool stop_at_first_solution_changed_;
    double allocated_time_changed_;
  };
  
}

#endif // MPGLUE_PLANNER_HPP
