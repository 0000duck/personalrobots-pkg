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

#ifndef OMPL_ROS_BASE_GOAL_DEFINITIONS_
#define OMPL_ROS_BASE_GOAL_DEFINITIONS_

#include "ompl_ros/ModelBase.h"
#include <planning_environment/util/kinematic_state_constraint_evaluator.h>
#include <ompl/extension/kinematic/SpaceInformationKinematic.h>

namespace ompl_ros
{
    
    class GoalToState : public ompl::base::GoalState
    {
    public:
	GoalToState(ModelBase *model, const std::vector<motion_planning_msgs::JointConstraint> &jc) : ompl::base::GoalState(model->si)
	{
	    setup(model, jc);
	}
	
	virtual ~GoalToState(void)
	{
	    delete compState_;
	}
	
	virtual double distanceGoal(const ompl::base::State *s) const;
	const std::vector< std::pair<double, double> >& getBounds(void) const;
	virtual void print(std::ostream &out = std::cout) const;
	
    protected:
	
	void setup(ModelBase *model, const std::vector<motion_planning_msgs::JointConstraint> &jc);

	ompl::base::State                        *compState_;
	double                                   *cVals_;
	double                                   *stateVals_;
	int                                       dim_;	
	std::vector< std::pair<double, double> >  bounds_;
	
    };
    
    class GoalToPosition : public ompl::base::GoalRegion
    {
    public:
	
        GoalToPosition(ModelBase *model, const std::vector<motion_planning_msgs::PoseConstraint> &pc) : ompl::base::GoalRegion(model->si)
	{
	    model_ = model;
	    for (unsigned int i = 0 ; i < pc.size() ; ++i)
	    {
		planning_environment::PoseConstraintEvaluator *pce = new planning_environment::PoseConstraintEvaluator();
		pce->use(model_->planningMonitor->getKinematicModel(), pc[i]);
		pce_.push_back(pce);
		threshold = ompl::STATE_EPSILON;
	    }
	}
	
	virtual ~GoalToPosition(void)
	{
	    for (unsigned int i = 0 ; i < pce_.size() ; ++i)
		delete pce_[i];
	}
	
	virtual double distanceGoal(const ompl::base::State *state) const;
	virtual bool isSatisfied(const ompl::base::State *state, double *dist = NULL) const;
	virtual void print(std::ostream &out = std::cout) const;
	
    protected:
	
	double evaluateGoalAux(const ompl::base::State *state, std::vector<bool> *decision) const;
	
	mutable ModelBase                                           *model_;
	std::vector<planning_environment::PoseConstraintEvaluator*>  pce_;
	
    };
    
    class GoalToMultipleConstraints : public ompl::kinematic::GoalRegionKinematic
    {
    public:
	GoalToMultipleConstraints(ModelBase *model, const motion_planning_msgs::KinematicConstraints &kc) : GoalRegionKinematic(dynamic_cast<ompl::kinematic::SpaceInformationKinematic*>(model->si)), sCore_(model->si),
													    gp_(model, kc.pose_constraint), gs_(model, kc.joint_constraint)
	{
	    threshold = gp_.threshold + gs_.threshold;
	    std::vector< std::pair<double, double> > b = gs_.getBounds();
	    rho_.resize(b.size());
	    for (unsigned int i = 0 ; i < b.size() ; ++i)
		rho_[i] = (b[i].second - b[i].first) / 2.0;	    
	}

	virtual ~GoalToMultipleConstraints(void)
	{
	}
	
	virtual double distanceGoal(const ompl::base::State *s) const;
	virtual bool isSatisfied(const ompl::base::State *state, double *dist = NULL) const;
	virtual void sampleNearGoal(ompl::base::State *s) const;
	virtual void print(std::ostream &out = std::cout) const;
	
    protected:
	
	mutable ompl::base::SpaceInformation::StateSamplingCore sCore_;
	mutable boost::mutex                                    lock_;
	
	GoalToPosition      gp_;
	GoalToState         gs_;
	std::vector<double> rho_;
    };
    
    ompl::base::Goal* computeGoalFromConstraints(ModelBase *model, const motion_planning_msgs::KinematicConstraints &kc);
}

#endif
