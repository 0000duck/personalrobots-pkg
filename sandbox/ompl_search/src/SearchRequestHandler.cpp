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

#include "SearchRequestHandler.h"
#include <ompl_ros/base/GoalDefinitions.h>
#include <ros/console.h>
#include <sstream>
#include <cstdlib>

bool ompl_search::SearchRequestHandler::isRequestValid(ModelMap &models, motion_planning_msgs::ConvertToJointConstraint::Request &req)
{   
    ModelMap::const_iterator pos = models.find(req.params.model_id);
    
    if (pos == models.end())
    {
	ROS_ERROR("Cannot search for '%s'. Model does not exist", req.params.model_id.c_str());
	return false;
    }
    
    /* find the model */
    SearchModel *m = pos->second;
    
    /* check if the desired distance metric is defined */
    if (m->mk->sde.find(req.params.distance_metric) == m->mk->sde.end())
    {
	ROS_ERROR("Distance evaluator not found: '%s'", req.params.distance_metric.c_str());
	return false;
    }
    
    // check headers
    for (unsigned int i = 0 ; i < req.constraints.pose_constraint.size() ; ++i)
	if (!m->mk->planningMonitor->getTransformListener()->frameExists(req.constraints.pose_constraint[i].pose.header.frame_id))
	{
	    ROS_ERROR("Frame '%s' is not defined for pose constraint message %u", req.constraints.pose_constraint[i].pose.header.frame_id.c_str(), i);
	    return false;
	}
    
    for (unsigned int i = 0 ; i < req.constraints.joint_constraint.size() ; ++i)
	if (!m->mk->planningMonitor->getTransformListener()->frameExists(req.constraints.joint_constraint[i].header.frame_id))
	{
	    ROS_ERROR("Frame '%s' is not defined for joint constraint message %u", req.constraints.joint_constraint[i].header.frame_id.c_str(), i);
	    return false;
	}
    
    if (!req.states.empty())
    {
	unsigned int jdim = 0;
	
	// make sure all joints are in the group
	for (unsigned int i = 0 ; i < req.names.size() ; ++i)
	{
	    if (!m->mk->group->hasJoint(req.names[i]))
	    {
		ROS_ERROR("Joint '%s' is not in group '%s'", req.names[i].c_str(), m->mk->groupName.c_str());
		return false;
	    }
	    jdim += m->mk->planningMonitor->getKinematicModel()->getJoint(req.names[i])->usedParams;
	}
	
	if (m->mk->si->getStateDimension() != jdim)
	{
	    ROS_ERROR("The state dimension for model '%s' does not match the dimension of the joints defining the hint states", req.params.model_id.c_str());
	    return false;
	}
    }
    
    return true;
}

void ompl_search::SearchRequestHandler::configure(const planning_models::KinematicState *startState, motion_planning_msgs::ConvertToJointConstraint::Request &req, SearchModel *model)
{
    /* clear memory */
    model->mk->si->clearGoal();
    
    // clear clones of environments 
    model->mk->clearEnvironmentDescriptions();

    setWorkspaceBounds(req.params, model->mk);
    model->mk->si->setStateDistanceEvaluator(model->mk->sde[req.params.distance_metric]);

    /* set the pose of the whole robot */
    ompl_ros::EnvironmentDescription *ed = model->mk->getEnvironmentDescription();
    ed->kmodel->computeTransforms(startState->getParams());
    ed->collisionSpace->updateRobotModel();
    
    /* add goal state */
    model->mk->planningMonitor->transformConstraintsToFrame(req.constraints, model->mk->planningMonitor->getFrameId());
    model->mk->si->setGoal(ompl_ros::computeGoalFromConstraints(model->mk, req.constraints));

    /* print some information */
    printSettings(model->mk->si);
}

void ompl_search::SearchRequestHandler::setWorkspaceBounds(motion_planning_msgs::KinematicSpaceParameters &params, ompl_ros::ModelKinematic *model)
{
    /* set the workspace volume for planning */
    if (model->planningMonitor->getTransformListener()->frameExists(params.volumeMin.header.frame_id) &&
	model->planningMonitor->getTransformListener()->frameExists(params.volumeMax.header.frame_id))
    {
	bool err = false;
	try
	{
	    model->planningMonitor->getTransformListener()->transformPoint(model->planningMonitor->getFrameId(), params.volumeMin, params.volumeMin);
	    model->planningMonitor->getTransformListener()->transformPoint(model->planningMonitor->getFrameId(), params.volumeMax, params.volumeMax);
	}
	catch(...)
	{
	    err = true;
	}
	if (err)
	    ROS_ERROR("Unable to transform workspace bounds to planning frame");
	else
	{	    
	    // only area or volume should go through
	    ompl_ros::ROSSpaceInformationKinematic *s = dynamic_cast<ompl_ros::ROSSpaceInformationKinematic*>(model->si);
	    s->setPlanningArea(params.volumeMin.point.x, params.volumeMin.point.y,
			       params.volumeMax.point.x, params.volumeMax.point.y);
	    s->setPlanningVolume(params.volumeMin.point.x, params.volumeMin.point.y, params.volumeMin.point.z,
				 params.volumeMax.point.x, params.volumeMax.point.y, params.volumeMax.point.z);
	}
    }
    else
	ROS_DEBUG("No workspace bounding volume was set");
}


void ompl_search::SearchRequestHandler::printSettings(ompl::base::SpaceInformation *si)
{
    std::stringstream ss;
    si->printSettings(ss);
    ROS_DEBUG("%s", ss.str().c_str());
}

bool ompl_search::SearchRequestHandler::findState(ModelMap &models, const planning_models::KinematicState *start, motion_planning_msgs::ConvertToJointConstraint::Request &req,
						  motion_planning_msgs::ConvertToJointConstraint::Response &res)
{    
    if (!isRequestValid(models, req))
	return false;
        
    // find the data we need 
    SearchModel *m = models[req.params.model_id];
    
    // copy the hint states to OMPL datastructures
    const unsigned int dim = m->mk->si->getStateDimension();
    ompl::base::State *goal = new ompl::base::State(dim);
    std::vector<ompl::base::State*> hints;
    planning_models::KinematicState hs(*start);
    unsigned int sdim = m->mk->si->getStateDimension();
    for (unsigned int i = 0 ; i < req.states.size() ; ++i)
    {
	if (req.states[i].vals.size() != sdim)
	{
	    ROS_ERROR("Incorrect number of parameters for hint state at index %u. Expected %u, got %d.", i, sdim, (int)req.states[i].vals.size());
	    continue;
	}
	ompl::base::State *st = new ompl::base::State(dim);
	hs.setParamsJoints(req.states[i].vals, req.names);
	hs.copyParamsGroup(st->values, m->mk->group);
	std::stringstream ss;
	ss << "Hint state: ";
	m->mk->si->printState(st, ss);	
	ROS_DEBUG("%s", ss.str().c_str());
	hints.push_back(st);
    }
    
    m->mk->planningMonitor->getEnvironmentModel()->lock();
    m->mk->planningMonitor->getKinematicModel()->lock();
    
    configure(start, req, m);
    ros::WallTime startTime = ros::WallTime::now();
    bool found = m->gaik->solve(req.allowed_time, goal, hints);
    double stime = (ros::WallTime::now() - startTime).toSec();
    m->mk->planningMonitor->getEnvironmentModel()->unlock();
    m->mk->planningMonitor->getKinematicModel()->unlock();
    
    ROS_DEBUG("Spent %f seconds searching for state", stime);
    
    if (found)
    {
	int u = 0;
	const std::vector<std::string> &jnames = m->mk->group->jointNames;
	std::stringstream ss;
	res.joint_constraint.resize(jnames.size());
	for (unsigned int i = 0; i < jnames.size() ; ++i)
	{
	    res.joint_constraint[i].header.frame_id = m->mk->planningMonitor->getFrameId();
	    res.joint_constraint[i].header.stamp = m->mk->planningMonitor->lastMapUpdate();
	    res.joint_constraint[i].joint_name = jnames[i];
	    planning_models::KinematicModel::Joint *joint = m->mk->group->joints[i];
	    res.joint_constraint[i].value.resize(joint->usedParams);
	    res.joint_constraint[i].tolerance_above.resize(joint->usedParams, 0.0);
	    res.joint_constraint[i].tolerance_below.resize(joint->usedParams, 0.0);
	    for (unsigned int j = 0 ; j < joint->usedParams ; ++j)
	    {
		res.joint_constraint[i].value[j] = goal->values[j + u];
		ss << goal->values[j + u] << " ";
	    }
	    u += joint->usedParams;
	}
	ROS_DEBUG("Solution was found: %s", ss.str().c_str());
    }
    else
	ROS_DEBUG("No solution was found");
    
    m->mk->si->clearGoal();
    delete goal;
    for (unsigned int i = 0 ; i < hints.size() ; ++i)
	delete hints[i];
    
    return true;
}
