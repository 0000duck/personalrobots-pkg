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

/* \author Ioan Sucan */

#include "ompl/base/Goal.h"
#include "ompl/base/SpaceInformation.h"

ompl::base::GoalRegion::GoalRegion(SpaceInformation *si) : Goal(si)
{
    threshold = 0.0;
}

bool ompl::base::GoalRegion::isSatisfied(const State *s, double *distance) const
{
    double d2g = distanceGoal(s);
    if (distance)
	*distance = d2g;
    return d2g < threshold;
}

void ompl::base::GoalRegion::print(std::ostream &out) const
{
    out << "Goal region, threshold = " << threshold << ", memory address = " << reinterpret_cast<const void*>(this) << std::endl;
}

ompl::base::GoalState::GoalState(SpaceInformation *si) : GoalRegion(si)
{
    state = NULL;
}
	    
double ompl::base::GoalState::distanceGoal(const State *s) const
{
    return m_si->distance(s, state);
}

void ompl::base::GoalState::print(std::ostream &out) const
{
    out << "Goal state, threshold = " << threshold << ", memory address = " << reinterpret_cast<const void*>(this) << ", state = ";
    m_si->printState(state, out);
}
