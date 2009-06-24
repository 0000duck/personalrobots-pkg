/*********************************************************************
*
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
*
* Author: Eitan Marder-Eppstein
*********************************************************************/
#include <costmap_2d/costmap_2d_publisher.h>

namespace costmap_2d {
  Costmap2DPublisher::Costmap2DPublisher(ros::NodeHandle& ros_node, double publish_frequency, std::string global_frame) 
    : ros_node_(ros_node), global_frame_(global_frame), 
    visualizer_thread_(NULL), active_(false), new_data_(false){
    raw_obs_pub_ = ros_node_.advertise<visualization_msgs::Polyline>("~raw_obstacles", 1);
    inf_obs_pub_ = ros_node_.advertise<visualization_msgs::Polyline>("~inflated_obstacles", 1);
    visualizer_thread_ = new boost::thread(boost::bind(&Costmap2DPublisher::mapPublishLoop, this, publish_frequency));
  }

  Costmap2DPublisher::~Costmap2DPublisher(){
    if(visualizer_thread_ != NULL){
      visualizer_thread_->join();
      delete visualizer_thread_;
    }
  }

  void Costmap2DPublisher::mapPublishLoop(double frequency){
    //the user might not want to run the loop every cycle
    if(frequency == 0.0)
      return;

    active_ = true;
    costmap_2d::Rate r(frequency);
    while(ros_node_.ok()){
      //check if we have new data to publish
      if(new_data_){
        //we are about to publish the latest data
        new_data_ = false;
        publishCostmap();
      }
      if(!r.sleep())
        ROS_WARN("Map publishing loop missed its desired rate of %.4f the actual time the loop took was %.4f sec", frequency, r.cycleTime().toSec());
    }
  }

  void Costmap2DPublisher::updateCostmapData(const Costmap2D& costmap){
    std::vector< std::pair<double, double> > raw_obstacles, inflated_obstacles;
    for(unsigned int i = 0; i < costmap.cellSizeX(); i++){
      for(unsigned int j = 0; j < costmap.cellSizeY(); j++){
        double wx, wy;
        costmap.mapToWorld(i, j, wx, wy);
        std::pair<double, double> p(wx, wy);

        //if(costmap.getCost(i, j) == costmap_2d::LETHAL_OBSTACLE || costmap.getCost(i, j) == costmap_2d::NO_INFORMATION)
        if(costmap.getCost(i, j) == costmap_2d::LETHAL_OBSTACLE)
          raw_obstacles.push_back(p);
        else if(costmap.getCost(i, j) == costmap_2d::INSCRIBED_INFLATED_OBSTACLE)
          inflated_obstacles.push_back(p);
      }
    }
    lock_.lock();
    raw_obstacles_ = raw_obstacles;
    inflated_obstacles_ = inflated_obstacles;
    lock_.unlock();
    
    //let the publisher know we have new data to publish
    new_data_ = true;
  }

  void Costmap2DPublisher::publishCostmap(){
    std::vector< std::pair<double, double> > raw_obstacles, inflated_obstacles;

    lock_.lock();
    raw_obstacles = raw_obstacles_;
    inflated_obstacles = inflated_obstacles_;
    lock_.unlock();

    // First publish raw obstacles in red
    visualization_msgs::Polyline obstacle_msg;
    obstacle_msg.header.frame_id = global_frame_;
    unsigned int pointCount = raw_obstacles.size();
    obstacle_msg.set_points_size(pointCount);
    obstacle_msg.color.a = 0.0;
    obstacle_msg.color.r = 1.0;
    obstacle_msg.color.b = 0.0;
    obstacle_msg.color.g = 0.0;

    for(unsigned int i=0;i<pointCount;i++){
      obstacle_msg.points[i].x = raw_obstacles[i].first;
      obstacle_msg.points[i].y = raw_obstacles[i].second;
      obstacle_msg.points[i].z = 0;
    }

    raw_obs_pub_.publish(obstacle_msg);

    // Now do inflated obstacles in blue
    pointCount = inflated_obstacles.size();
    obstacle_msg.set_points_size(pointCount);
    obstacle_msg.color.a = 0.0;
    obstacle_msg.color.r = 0.0;
    obstacle_msg.color.b = 1.0;
    obstacle_msg.color.g = 0.0;

    for(unsigned int i=0;i<pointCount;i++){
      obstacle_msg.points[i].x = inflated_obstacles[i].first;
      obstacle_msg.points[i].y = inflated_obstacles[i].second;
      obstacle_msg.points[i].z = 0;
    }

    inf_obs_pub_.publish(obstacle_msg);
  }

};
