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

#include "planning_environment/monitors/kinematic_model_state_monitor.h"
#include "planning_environment/util/construct_object.h"
#include <angles/angles.h>
#include <sstream>

void planning_environment::KinematicModelStateMonitor::setupRSM(void)
{
    stateMonitorStarted_ = false;
    pose_.setIdentity();
    kmodel_ = NULL;
    robotState_ = NULL;
    onStateUpdate_ = NULL;
    onAfterAttachBody_ = NULL;
    attachedBodyFilter_ = NULL;
    attachedBodySubscriber_ = NULL;
    havePose_ = haveJointState_ = false;
    robotVelocity_ = 0.0;
    
    if (rm_->loadedModels())
    {
	kmodel_ = rm_->getKinematicModel().get();
	robotState_ = new planning_models::KinematicState(kmodel_);

	// joints to update based on received pose
	if (dynamic_cast<planning_models::KinematicModel::PlanarJoint*>(kmodel_->getRoot()))
	{
	    planarJoint_ = kmodel_->getRoot()->name;
	    double params[3] = { 0.0, 0.0, 0.0 };
	    robotState_->setParamsJoint(params, planarJoint_);
	}
	
	if (dynamic_cast<planning_models::KinematicModel::FloatingJoint*>(kmodel_->getRoot()))
	{
	    floatingJoint_ = kmodel_->getRoot()->name;
	    double params[7] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
	    robotState_->setParamsJoint(params, floatingJoint_);
	}
	
	robot_frame_ = kmodel_->getRoot()->after->name;
	ROS_DEBUG("Robot frame is '%s'", robot_frame_.c_str());
	
	if (includePose_)
	    ROS_DEBUG("Maintaining robot pose in frame '%s'", frame_id_.c_str());
	else
	    frame_id_ = robot_frame_;

    }
    
    startStateMonitor();
}

void planning_environment::KinematicModelStateMonitor::startStateMonitor(void)
{   
    if (stateMonitorStarted_)
	return;
    
    if (rm_->loadedModels())
    {
	jointStateSubscriber_ = nh_.subscribe("joint_states", 1, &KinematicModelStateMonitor::jointStateCallback, this);
	ROS_DEBUG("Listening to joint states");
	
	attachedBodySubscriber_ = new message_filters::Subscriber<mapping_msgs::AttachedObject>(nh_, "attach_object", 1);	
	attachedBodyFilter_ = new tf::MessageFilter<mapping_msgs::AttachedObject>(*attachedBodySubscriber_, *tf_, "", 1);
	attachedBodyFilter_->setTargetFrames(rm_->getCollisionCheckLinks());
	attachedBodyFilter_->registerCallback(boost::bind(&KinematicModelStateMonitor::attachObjectCallback, this, _1));
	
	ROS_DEBUG("Listening to attach_object using message notifier with target frame %s", attachedBodyFilter_->getTargetFramesString().c_str());
    }
    stateMonitorStarted_ = true;
}

void planning_environment::KinematicModelStateMonitor::stopStateMonitor(void)
{
    if (!stateMonitorStarted_)
	return;
    
    delete attachedBodyFilter_;
    attachedBodyFilter_ = NULL;

    delete attachedBodySubscriber_;
    attachedBodySubscriber_ = NULL;
    
    jointStateSubscriber_.shutdown();
    
    ROS_DEBUG("Kinematic state is no longer being monitored");
    
    stateMonitorStarted_ = false;
}

void planning_environment::KinematicModelStateMonitor::jointStateCallback(const sensor_msgs::JointStateConstPtr &jointState)
{
    bool change = !haveJointState_;

    static bool first_time = true;
    
    double totalv = 0.0;
    unsigned int n = jointState->get_name_size();
    if (jointState->get_name_size() != jointState->get_position_size() || jointState->get_name_size() !=jointState->get_velocity_size()){
      ROS_ERROR("Planning environment received invalid joint state");
      return;
    }
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	if (kmodel_->hasJoint(jointState->name[i]))
	{
	    planning_models::KinematicModel::Joint* joint = kmodel_->getJoint(jointState->name[i]);
	    if (joint->usedParams == 1)
	    {
		double pos = jointState->position[i];
		double vel = jointState->velocity[i];
		totalv += vel * vel;
		planning_models::KinematicModel::RevoluteJoint* rjoint = dynamic_cast<planning_models::KinematicModel::RevoluteJoint*>(joint);
		if (rjoint)
		    if (rjoint->continuous)
			pos = angles::normalize_angle(pos);
		bool this_changed = robotState_->setParamsJoint(&pos, jointState->name[i]);
		change = change || this_changed;
	    }
	    else
	    {
		if (first_time)
		    ROS_WARN("Incorrect number of parameters: %s (expected %d, had 1)", jointState->name[i].c_str(), joint->usedParams);
	    }
	}
	else
	{
	    if (first_time)
		ROS_ERROR("Unknown joint: %s", jointState->name[i].c_str());
	}
    }
    robotVelocity_ = sqrt(totalv);
    
    first_time = false;

    lastJointStateUpdate_ = jointState->header.stamp;
    if (!haveJointState_)
	haveJointState_ = robotState_->seenAll();

    if (includePose_)
    {
	std::string err;
	ros::Time tm;
	if (tf_->getLatestCommonTime(robot_frame_, frame_id_, tm, &err) == tf::NO_ERROR)
	{
	    tf::Stamped<tf::Pose> transf;
	    bool ok = true;
	    try
	    {
		tf_->lookupTransform(frame_id_, robot_frame_, tm, transf);
	    }
	    catch(...)
	    {
		ok = false;
	    }
	    
	    if (ok)
	    {		
		if (!(pose_ == transf))
		{
		    pose_ = transf;
		    kmodel_->setRootTransform(pose_);
		    change = true;
		}
		
		lastPoseUpdate_ = tm;
		havePose_ = true;
	    }
	    else
		ROS_ERROR("Transforming from link '%s' to link '%s' failed", robot_frame_.c_str(), frame_id_.c_str());
	}
	else
	    ROS_WARN("Unable to find transform from link '%s' to link '%s' (TF said: %s)", robot_frame_.c_str(), frame_id_.c_str(), err.c_str());
    }

    if (change && onStateUpdate_ != NULL)
	onStateUpdate_();
}

bool planning_environment::KinematicModelStateMonitor::attachObject(const mapping_msgs::AttachedObjectConstPtr &attachedObject)
{
    bool result = false;
    kmodel_->lock();
    
    planning_models::KinematicModel::Link *link = kmodel_->getLink(attachedObject->link_name);
    
    if (attachedObject->objects.size() != attachedObject->poses.size())
	ROS_ERROR("Number of objects to attach does not match number of poses");
    else
	if (link)
	{	
	    // clear the previously attached bodies 
	    for (unsigned int i = 0 ; i < link->attachedBodies.size() ; ++i)
		delete link->attachedBodies[i];
	    link->attachedBodies.resize(0);
	    unsigned int n = attachedObject->objects.size();
	    
	    // create the new ones
	    for (unsigned int i = 0 ; i < n ; ++i)
	    {
		geometry_msgs::PoseStamped pose;
		geometry_msgs::PoseStamped poseP;
		pose.pose = attachedObject->poses[i];
		pose.header = attachedObject->header;
		bool err = false;
		try
		{
		    tf_->transformPose(attachedObject->link_name, pose, poseP);
		}
		catch(...)
		{
		    err = true;
		    ROS_ERROR("Unable to transform object to be attached from frame '%s' to frame '%s'", attachedObject->header.frame_id.c_str(), attachedObject->link_name.c_str());
		}
		if (err)
		    continue;
		
		shapes::Shape *shape = constructObject(attachedObject->objects[i]);
		if (!shape)
		    continue;
		
		unsigned int j = link->attachedBodies.size();
		link->attachedBodies.push_back(new planning_models::KinematicModel::AttachedBody(link));
		tf::poseMsgToTF(poseP.pose, link->attachedBodies[j]->attachTrans);
		link->attachedBodies[j]->shape = shape;
		link->attachedBodies[j]->touchLinks = attachedObject->touch_links;
	    }
	    
	    result = true;	    
	    ROS_DEBUG("Link '%s' has %d objects attached", attachedObject->link_name.c_str(), n);
	}
	else
	    ROS_WARN("Unable to attach object to link '%s'", attachedObject->link_name.c_str());

    kmodel_->unlock();

    if (result && (onAfterAttachBody_ != NULL))
	onAfterAttachBody_(link, attachedObject); 
    
    return result;
}

void planning_environment::KinematicModelStateMonitor::attachObjectCallback(const mapping_msgs::AttachedObjectConstPtr &attachedObject)
{
    attachObject(attachedObject);
}

void planning_environment::KinematicModelStateMonitor::waitForState(void) const
{
    int s = 0;
    while (nh_.ok() && !haveState())
    {
	if (s == 0)
	    ROS_INFO("Waiting for robot state ...");
	s = (s + 1) % 40;
	ros::spinOnce();
	ros::Duration().fromSec(0.05).sleep();
    }
    if (haveState())
	ROS_INFO("Robot state received!");
}

bool planning_environment::KinematicModelStateMonitor::isJointStateUpdated(double sec) const
{  
    // less than 10us is considered 0 
    if (sec > 1e-5 && lastJointStateUpdate_ < ros::Time::now() - ros::Duration(sec))
	return false;
    else
	return true;
}

bool planning_environment::KinematicModelStateMonitor::isPoseUpdated(double sec) const
{  
    // less than 10us is considered 0 
    if (sec > 1e-5 && lastPoseUpdate_ < ros::Time::now() - ros::Duration(sec))
	return false;
    else
	return true;
}

void planning_environment::KinematicModelStateMonitor::printRobotState(void) const
{
    std::stringstream ss;
    robotState_->print(ss);
    ROS_INFO("%s", ss.str().c_str());
}
