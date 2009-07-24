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

#ifndef OMPL_PLANNING_EXTENSIONS_PROJECTION_EVALUATORS_
#define OMPL_PLANNING_EXTENSIONS_PROJECTION_EVALUATORS_

#include <ompl/base/ProjectionEvaluator.h>
#include "ompl_planning/ModelBase.h"

namespace ompl_planning
{

    class LinkPositionProjectionEvaluator : public ompl::base::ProjectionEvaluator
    {
    public:

        LinkPositionProjectionEvaluator(ModelBase *model, const std::string &linkName) : ompl::base::ProjectionEvaluator()
	{
	    model_ = model;
	    link_  = model_->kmodel->getLink(linkName);
	    if (link_ == NULL)
		ROS_ERROR("Unknown link: '%s'", linkName.c_str());
	}
	
	/** Return the dimension of the projection defined by this evaluator */
	virtual unsigned int getDimension(void) const
	{
	    return 3;
	}
		
	/** Compute the projection as an array of double values */
	virtual void operator()(const ompl::base::State *state, double *projection) const
	{  
	    model_->kmodel->computeTransformsGroup(state->values, model_->groupID);
	    const btVector3 &origin = link_->globalTrans.getOrigin();
	    projection[0] = origin.x();
	    projection[1] = origin.y();
	    projection[2] = origin.z();
	}
	
    protected:
	
	ModelBase                             *model_;
	planning_models::KinematicModel::Link *link_;
	
    };
    
}

#endif
