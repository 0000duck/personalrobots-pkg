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

/** \author Ioan Sucan */

#ifndef KINEMATIC_PLANNING_RKP_BASIC_REQUEST_STATE_
#define KINEMATIC_PLANNING_RKP_BASIC_REQUEST_STATE_

#include "kinematic_planning/RKPBasicRequest.h"
#include <robot_srvs/KinematicPlanState.h>
#include <robot_srvs/KinematicReplanState.h>

namespace kinematic_planning
{

    static const int R_STATE = 1; // state request

    template<>
    RKPBasicRequest<robot_msgs::KinematicPlanStateRequest>::RKPBasicRequest(void)
    {
	m_type = R_STATE;
    }

    /** Validate request for planning towards a state */
    template<>
    bool RKPBasicRequest<robot_msgs::KinematicPlanStateRequest>::isRequestValid(ModelMap &models, robot_msgs::KinematicPlanStateRequest &req)
    {
	if (!areSpaceParamsValid(models, req.params))
	    return false;
	
	RKPModel *m = models[req.params.model_id];
	RKPPlannerSetup *psetup = m->planners[req.params.planner_id];
	
	if (m->kmodel->getModelInfo().stateDimension != req.start_state.get_vals_size())
	{
	    ROS_ERROR("Dimension of start state expected to be %d but was received as %d", m->kmodel->getModelInfo().stateDimension, req.start_state.get_vals_size());
	    return false;
	}
	
	if (psetup->si->getStateDimension() != req.goal_state.get_vals_size())
	{
	    ROS_ERROR("Dimension of start goal expected to be %d but was received as %d", psetup->si->getStateDimension(), req.goal_state.get_vals_size());
	    return false;
	}
	
	return true;
    }
    
    /** Set the goal using a destination state */
    template<>
    void RKPBasicRequest<robot_msgs::KinematicPlanStateRequest>::setupGoalState(RKPPlannerSetup *psetup, robot_msgs::KinematicPlanStateRequest &req)
    {
	/* set the goal */
	ompl::sb::GoalState *goal = new ompl::sb::GoalState(psetup->si);
	const unsigned int dim = psetup->si->getStateDimension();
	goal->state = new ompl::sb::State(dim);
	for (unsigned int i = 0 ; i < dim ; ++i)
	    static_cast<ompl::sb::State*>(goal->state)->values[i] = req.goal_state.vals[i];
	goal->threshold = req.threshold;
	psetup->si->setGoal(goal);
    }    

} // kinematic_planning

#endif
    
