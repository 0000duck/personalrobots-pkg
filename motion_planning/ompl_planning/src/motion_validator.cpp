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

/**


@htmlinclude ../manifest.html

@b MotionValidator is a node capable of verifying if a path is valid
or not (collides or does not collide). The path is consider to be a
straight line between two states.

<hr>

@section usage Usage
@verbatim
$ motion_validator [standard ROS args]
@endverbatim

@par Example

@verbatim
$ motion_validator robot_description:=robotdesc/pr2
@endverbatim

<hr>

@section topic ROS topics

Subscribes to (name/type):
- None

Publishes to (name/type):
- None

<hr>

@section services ROS services

Uses (name/type):
- None

Provides (name/type):
- @b "validate_direct_path"/ValidateKinematicPath : given a robot model, starting and goal states, this service computes whether the straight path is valid
- @b "validate_state"/ValidateKinematicState : given a robot model and a state, this service computes whether the state is valid

<hr>

@section parameters ROS parameters
- None

**/

#include "kinematic_planning/CollisionSpaceMonitor.h"
#include <ompl/extension/samplingbased/kinematic/SpaceInformationKinematic.h>

#include <motion_planning_srvs/ValidateKinematicPath.h>
#include <motion_planning_srvs/ValidateKinematicState.h>

#include "kinematic_planning/ompl_extensions/RKPStateValidator.h"
#include "kinematic_planning/ompl_extensions/RKPSpaceInformation.h"

#include <iostream>
#include <sstream>
#include <string>
#include <map>
using namespace kinematic_planning;

class MotionValidator : public CollisionSpaceMonitor
{
public:
    
    class myModel : public RKPModelBase
    {
    public:
	myModel(void) : RKPModelBase()
	{
	    si = NULL;
	    svc = NULL;
	}
	
	virtual ~myModel(void)
	{
	    if (svc)
		delete svc;
	    if (si)
		delete si;
	}
	
	ompl::sb::SpaceInformationKinematic *si;
	ompl::base::StateValidityChecker    *svc;
    };    
    
    MotionValidator(ros::Node *node) : CollisionSpaceMonitor(node)
    {
	m_node->advertiseService("validate_direct_path", &MotionValidator::validatePath, this);
	m_node->advertiseService("validate_state",       &MotionValidator::validateState, this);
    }
    
    /** Free the memory */
    virtual ~MotionValidator(void)
    {
	for (std::map<std::string, myModel*>::iterator i = m_models.begin() ; i != m_models.end() ; i++)
	    delete i->second;
    }
    
    bool validateState(motion_planning_srvs::ValidateKinematicState::Request &req, motion_planning_srvs::ValidateKinematicState::Response &res)
    {
	myModel *model = m_models[req.model_id];
	if (model)
	{
	    if (model->kmodel->getModelInfo().stateDimension != req.state.get_vals_size() && 
		model->si->getStateDimension() != req.state.get_vals_size())
	    {
		ROS_ERROR("Dimension of state expected to be %d or %d but was received as %d", 
			  model->kmodel->getModelInfo().stateDimension, model->si->getStateDimension(),
			  req.state.get_vals_size());
		return false;
	    }
	    
	    const unsigned int dim = model->si->getStateDimension();
	    ompl::sb::State *state = new ompl::sb::State(dim);
	    
	    model->collisionSpace->lock();
	    model->kmodel->lock();

	    if (model->groupID >= 0)
	    {
		/* set the pose of the whole robot to the current state */
		model->kmodel->computeTransforms(m_robotState->getParams());
		model->collisionSpace->updateRobotModel(model->collisionSpaceID);

		/* extract the components needed for the start state of the desired group */
		for (unsigned int i = 0 ; i < dim ; ++i)
		    state->values[i] = req.state.vals[model->kmodel->getModelInfo().groupStateIndexList[model->groupID][i]];
	    }
	    else
	    {
		/* the start state is the complete state */
		for (unsigned int i = 0 ; i < dim ; ++i)
		    state->values[i] = req.state.vals[i];
	    }
	    
	    std::vector<motion_planning_msgs::PoseConstraint> cstrs;
	    req.constraints.get_pose_vec(cstrs);
	    static_cast<StateValidityPredicate*>(model->svc)->setPoseConstraints(cstrs);

	    ROS_INFO("Validating state for '%s'...", req.model_id.c_str());
	    
	    res.valid = model->si->isValid(state) ? 1 : 0;

	    model->kmodel->unlock();
	    model->collisionSpace->unlock();

	    ROS_INFO("Result: %d", (int)res.valid);
	
	    delete state;
	    
	    return true;
	}
	else
	{
	    ROS_ERROR("Model '%s' not known", req.model_id.c_str());
	    return false;
	}
    }
    
    bool validatePath(motion_planning_srvs::ValidateKinematicPath::Request &req, motion_planning_srvs::ValidateKinematicPath::Response &res)
    {
	myModel *model = m_models[req.model_id];
	if (model)
	{
	    if (model->kmodel->getModelInfo().stateDimension != req.start_state.get_vals_size())
	    {
		ROS_ERROR("Dimension of start state expected to be %d but was received as %d", model->kmodel->getModelInfo().stateDimension, req.start_state.get_vals_size());
		return false;
	    }
	    
	    if (model->si->getStateDimension() != req.goal_state.get_vals_size())
	    {
		ROS_ERROR("Dimension of start goal expected to be %d but was received as %d", model->si->getStateDimension(), req.goal_state.get_vals_size());
		return false;
	    }
	    
	    ROS_INFO("Validating direct path for '%s'...", req.model_id.c_str());
	    
	    const unsigned int dim = model->si->getStateDimension();
	    ompl::sb::State *start = new ompl::sb::State(dim);

	    model->collisionSpace->lock();
	    model->kmodel->lock();
	    
	    if (model->groupID >= 0)
	    {
		/* set the pose of the whole robot */
		model->kmodel->computeTransforms(&req.start_state.vals[0]);
		model->collisionSpace->updateRobotModel(model->collisionSpaceID);
		
		/* extract the components needed for the start state of the desired group */
		for (unsigned int i = 0 ; i < dim ; ++i)
		    start->values[i] = req.start_state.vals[model->kmodel->getModelInfo().groupStateIndexList[model->groupID][i]];
	    }
	    else
	    {
		/* the start state is the complete state */
		for (unsigned int i = 0 ; i < dim ; ++i)
		    start->values[i] = req.start_state.vals[i];
	    }
	    
	    ompl::sb::State *goal = new ompl::sb::State(dim);
	    for (unsigned int i = 0 ; i < dim ; ++i)
		goal->values[i] = req.goal_state.vals[i];
	    
	    std::vector<motion_planning_msgs::PoseConstraint> cstrs;
	    req.constraints.get_pose_vec(cstrs);
	    static_cast<StateValidityPredicate*>(model->svc)->setPoseConstraints(cstrs);

	    res.valid = model->si->checkMotionIncremental(start, goal) ? 1 : 0;

	    model->kmodel->unlock();
	    model->collisionSpace->unlock();

	    ROS_INFO("Result: %d", (int)res.valid);
	    
	    delete start;
	    delete goal;
	    
	    return true;	    
	}
	else
	{
	    ROS_ERROR("Model '%s' not known", req.model_id.c_str());
	    return false;
	}
    }

    virtual void setRobotDescription(robot_desc::URDF *file)
    {
	CollisionSpaceMonitor::setRobotDescription(file);	
	
	ROS_INFO("=======================================");	
	std::stringstream ss;
	m_kmodel->printModelInfo(ss);
	ROS_INFO("%s", ss.str().c_str());
	ROS_INFO("=======================================");

	/* set the data for the model */
	myModel *model = new myModel();
	model->collisionSpaceID = 0;
	model->collisionSpace = m_collisionSpace;
        model->kmodel = m_kmodel;
	model->groupName = m_kmodel->getModelName();
	setupModel(model);

	/* remember the model by the robot's name */
	m_models[model->groupName] = model;
	
	/* create a model for each group */
	std::vector<std::string> groups;
	m_kmodel->getGroups(groups);

	for (unsigned int i = 0 ; i < groups.size() ; ++i)
	{
	    myModel *model = new myModel();
	    model->collisionSpaceID = 0;
	    model->collisionSpace = m_collisionSpace;
	    model->kmodel = m_kmodel;
	    model->groupID = m_kmodel->getGroupID(groups[i]);
	    model->groupName = groups[i];
	    setupModel(model);
	    m_models[model->groupName] = model;
	}
    }
    
    void knownModels(std::vector<std::string> &model_ids)
    {
	for (std::map<std::string, myModel*>::const_iterator i = m_models.begin() ; i != m_models.end() ; ++i)
	    model_ids.push_back(i->first);
    }
    
    void run(void)
    {
	loadRobotDescription();
	
	std::vector<std::string> mlist;    
	knownModels(mlist);
	ROS_INFO("Known models:");    
	for (unsigned int i = 0 ; i < mlist.size() ; ++i)
	    ROS_INFO("  * %s", mlist[i].c_str());
	if (mlist.size() > 0)
	    m_node->spin();
	else
	    ROS_ERROR("No robot models defined. Path validation node cannot start.");
    }

private:
    
    void setupModel(myModel *model)
    {
	model->si  = new SpaceInformationRKPModel(model);
	model->svc = new StateValidityPredicate(model);
	model->si->setStateValidityChecker(model->svc);
    }
    
    std::map<std::string, myModel*> m_models;
    
};


int main(int argc, char **argv)
{ 
    ros::init(argc, argv);
    
    ros::Node node("motion_validator");
    MotionValidator validator(&node);
    validator.run();
    
    return 0;    
}
