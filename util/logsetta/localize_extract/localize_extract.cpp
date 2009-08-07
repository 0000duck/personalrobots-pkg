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

#include "nav_msgs/Odometry.h"
#include <string>
#include "rosrecord/Player.h"
#include "tf/tf.h"

void localize_callback(std::string name, nav_msgs::Odometry* bL, ros::Time t, ros::Time t_no_use, void* f)
{
  FILE* file = (FILE*)f;

  fprintf(file, "%.5f ",t.toSec());

  fprintf(file, "%0.5f %0.5f %0.5f %0.5f %0.5f %0.5f %d", bL->pose_with_covariance.pose.position.x, bL->pose_with_covariance.pose.position.y, 
      tf::getYaw(bL->pose_with_covariance.pose.orientation), 
      bL->twist_with_covariance.twist.linear.x, bL->twist_with_covariance.twist.linear.y, bL->twist_with_covariance.twist.angular.z, 
      0);
      //@todo TODO: Decide if we should be getting stall information from a separate channel or not now that the Odometry message doesn't contain it
      //bL->stall);

  fprintf(file, "\n");
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("usage: localize_extract LOG\n");
    return 1;
  }

  ros::record::Player player;

  player.open(std::string(argv[1]), ros::Time());

  int count;

  FILE* file = fopen("localize_actual.txt", "w");

  player.addHandler<nav_msgs::Odometry>(std::string("localizedPose"), &localize_callback, file);

  while(player.nextMsg())  {}

  fclose(file);

  return 0;
}
