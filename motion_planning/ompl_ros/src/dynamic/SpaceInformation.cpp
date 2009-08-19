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

#include "ompl_ros/dynamic/SpaceInformation.h"
#include "ompl_ros/dynamic/StateValidator.h"
#include "ompl_ros/dynamic/extensions/PR2PropagationModels.h"
#include <ros/console.h>

void ompl_ros::ROSSpaceInformationDynamic::configureOMPLSpace(ModelBase *model)
{	   
    kmodel_ = model->planningMonitor->getKinematicModel();
    groupID_ = model->groupID;
    propagationModel_ = NULL;
    
    /* compute the state space for this group */
    m_stateDimension = kmodel_->getModelInfo().groupStateIndexList[groupID_].size();
    m_stateComponent.resize(m_stateDimension);
    
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
    {	
	int p = kmodel_->getModelInfo().groupStateIndexList[groupID_][i] * 2;
	
	if (m_stateComponent[i].type == ompl::base::StateComponent::UNKNOWN)
	{
	    planning_models::KinematicModel::RevoluteJoint *rj = 
		dynamic_cast<planning_models::KinematicModel::RevoluteJoint*>(kmodel_->getJoint(kmodel_->getModelInfo().parameterName[p]));
	    if (rj && rj->continuous)
		m_stateComponent[i].type = ompl::base::StateComponent::WRAPPING_ANGLE;
	    else
		m_stateComponent[i].type = ompl::base::StateComponent::LINEAR;
	}
	
	m_stateComponent[i].minValue   = kmodel_->getModelInfo().stateBounds[p    ];
	m_stateComponent[i].maxValue   = kmodel_->getModelInfo().stateBounds[p + 1];
	
	for (unsigned int j = 0 ; j < kmodel_->getModelInfo().floatingJoints.size() ; ++j)
	    if (kmodel_->getModelInfo().floatingJoints[j] == p)
	    {
		floatingJoints_.push_back(i);
		m_stateComponent[i + 3].type = ompl::base::StateComponent::QUATERNION;
		m_stateComponent[i + 4].type = ompl::base::StateComponent::QUATERNION;
		m_stateComponent[i + 5].type = ompl::base::StateComponent::QUATERNION;
		m_stateComponent[i + 6].type = ompl::base::StateComponent::QUATERNION;
		break;
	    }
	
	for (unsigned int j = 0 ; j < kmodel_->getModelInfo().planarJoints.size() ; ++j)
	    if (kmodel_->getModelInfo().planarJoints[j] == p)
	    {
		planarJoints_.push_back(i);
		m_stateComponent[i + 2].type = ompl::base::StateComponent::WRAPPING_ANGLE;
		break;		    
	    }
    }
    
    // create a backup of this, in case it gets bound by joint constraints
    basicStateComponent_ = m_stateComponent;
    checkBounds();
    
    if (model->groupName == "base")
	propagationModel_ = new PR2BaseModel(this);
    
    assert(propagationModel_);
    propagationModel_->controlDefinition(m_controlComponent, &m_controlDimension, 
					 &m_minControlDuration, &m_maxControlDuration, &m_resolution);
    
    m_stateForwardPropagator = propagationModel_;
}

void ompl_ros::ROSSpaceInformationDynamic::setPlanningVolume(double x0, double y0, double z0, double x1, double y1, double z1)
{
    for (unsigned int i = 0 ; i < floatingJoints_.size() ; ++i)
    {
	int id = floatingJoints_[i];		
	m_stateComponent[id    ].minValue = x0;
	m_stateComponent[id    ].maxValue = x1;
	m_stateComponent[id + 1].minValue = y0;
	m_stateComponent[id + 1].maxValue = y1;
	m_stateComponent[id + 2].minValue = z0;
	m_stateComponent[id + 2].maxValue = z1;
    }
    checkBounds();    
}

void ompl_ros::ROSSpaceInformationDynamic::setPlanningArea(double x0, double y0, double x1, double y1)
{
    for (unsigned int i = 0 ; i < planarJoints_.size() ; ++i)
    {
	int id = planarJoints_[i];		
	m_stateComponent[id    ].minValue = x0;
	m_stateComponent[id    ].maxValue = x1;
	m_stateComponent[id + 1].minValue = y0;
	m_stateComponent[id + 1].maxValue = y1;
    }
    checkBounds();    
}

void ompl_ros::ROSSpaceInformationDynamic::clearPathConstraints(void)
{
    m_stateComponent = basicStateComponent_;
    ROSStateValidityPredicateDynamic *svp = dynamic_cast<ROSStateValidityPredicateDynamic*>(getStateValidityChecker());
    svp->clearConstraints();
}

void ompl_ros::ROSSpaceInformationDynamic::setPathConstraints(const motion_planning_msgs::KinematicConstraints &kc)
{    
    const std::vector<motion_planning_msgs::JointConstraint> &jc = kc.joint_constraint;
    
    // tighten the bounds based on the constraints
    for (unsigned int i = 0 ; i < jc.size() ; ++i)
    {
	// get the index at which the joint parameters start
	int idx = kmodel_->getJointIndexInGroup(jc[i].joint_name, groupID_);
	if (idx >= 0)
	{
	    unsigned int usedParams = kmodel_->getJoint(jc[i].joint_name)->usedParams;
	    
	    if (jc[i].tolerance_above.size() != jc[i].tolerance_below.size() || jc[i].value.size() != jc[i].tolerance_below.size() || jc[i].value.size() != usedParams)
		ROS_ERROR("Constraint on joint %s has incorrect number of parameters. Expected %u", jc[i].joint_name.c_str(), usedParams);
	    else
	    {
		for (unsigned int j = 0 ; j < usedParams ; ++j)
		{

		    if (m_stateComponent[idx + j].minValue < jc[i].value[j] - jc[i].tolerance_below[j])
			m_stateComponent[idx + j].minValue = jc[i].value[j] - jc[i].tolerance_below[j];
		    if (m_stateComponent[idx + j].maxValue > jc[i].value[j] + jc[i].tolerance_above[j])
			m_stateComponent[idx + j].maxValue = jc[i].value[j] + jc[i].tolerance_above[j];
		}
	    }
	}
    }
    
    checkBounds();
    
    motion_planning_msgs::KinematicConstraints temp_kc = kc;
    temp_kc.joint_constraint.clear();
    ROSStateValidityPredicateDynamic *svp = dynamic_cast<ROSStateValidityPredicateDynamic*>(getStateValidityChecker());
    svp->setConstraints(temp_kc);
}

bool ompl_ros::ROSSpaceInformationDynamic::checkBounds(void)
{
    // check if joint bounds are feasible
    bool valid = true;
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
	if (m_stateComponent[i].minValue > m_stateComponent[i].maxValue)
	{
	    valid = false;
	    ROS_ERROR("Inconsistent set of joint constraints imposed on path at index %d. Sampling will not find any valid states between %f and %f", i,
		      m_stateComponent[i].minValue, m_stateComponent[i].maxValue);
	    break;
	}
    return valid;
}
