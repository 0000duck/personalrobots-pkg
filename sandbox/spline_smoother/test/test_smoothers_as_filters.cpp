/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, Willow Garage, Inc.
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

/** \author Mrinal Kalakrishnan */

#include <gtest/gtest.h>
#include <manipulation_msgs/WaypointTrajWithLimits.h>
#include <filters/filter_chain.h>

using namespace filters;

TEST(TestSmoothersAsFilters, TestSmoothersAsFilters1)
{
  // make the filter chain:
  FilterChain<manipulation_msgs::WaypointTrajWithLimits> chain("manipulation_msgs::WaypointTrajWithLimits");
  ASSERT_TRUE(chain.configure("TestSmoothersAsFilters"));

  // create the input:
  int length = 5;
  manipulation_msgs::WaypointTrajWithLimits wpt;
  manipulation_msgs::WaypointTrajWithLimits wpt_out;
  wpt.points.resize(length);
  wpt.names.resize(1);
  wpt.names[0] = std::string("test");
  for (int i=0; i<length; i++)
  {
    wpt.points[i].positions.resize(1);
    wpt.points[i].accelerations.resize(1);
    wpt.points[i].velocities.resize(1);
    wpt.points[i].positions[0] = 0.0;
    wpt.points[i].velocities[0] = 0.0;
    wpt.points[i].accelerations[0] = 0.0;
    wpt.points[i].time = i;
  }

  chain.update(wpt, wpt_out);

  // verify that velocities are 0:
  for (int i=0; i<length; i++)
  {
    EXPECT_NEAR(wpt_out.points[i].velocities[0], 0.0, 1e-8);
  }

}

int main(int argc, char** argv)
{
 testing::InitGoogleTest(&argc, argv);
 ros::init(argc, argv, "test_smoothers_as_filters");
 return RUN_ALL_TESTS();
}
