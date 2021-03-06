/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sbpl_door_planner_action/sbpl_door_planner_action.h>
#include <manipulation_msgs/IKRequest.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <pr2_ik/pr2_ik_controller.h>

#include <kdl/frames.hpp>
#include <ros/node.h>
#include <ros/rate.h>

using namespace std;
using namespace ros;
using namespace KDL;
using namespace robot_actions;
using namespace door_functions;

SBPLDoorPlanner::SBPLDoorPlanner() :  Action<door_msgs::DoorCmd, door_msgs::Door>(ros_node_.getName()), cost_map_ros_(NULL),cm_getter_(this)
{
  ROS_INFO("Starting door planner with name: %s and name space: %s",ros_node_.getName().c_str(),ros_node_.getNamespace().c_str());
  ros_node_.param("~allocated_time", allocated_time_, 1.0);
  ros_node_.param("~forward_search", forward_search_, true);
  ros_node_.param("~animate", animate_, false);

  ros_node_.param("~planner_type", planner_type_, string("ARAPlanner"));
  ros_node_.param("~plan_stats_file", plan_stats_file_, string("/tmp/move_base_sbpl.log"));

  double circumscribed_radius;
  //we'll assume the radius of the robot to be consistent with what's specified for the costmaps
  ros_node_.param("~costmap/inscribed_radius", inscribed_radius_, 0.325);
  ros_node_.param("~costmap/inflation_radius", inflation_radius_, 0.46);
  ros_node_.param("~costmap/circumscribed_radius", circumscribed_radius, 0.46);
  ros_node_.param("~global_frame", global_frame_, std::string("odom_combined"));
  ros_node_.param("~robot_base_frame", robot_base_frame_, std::string("base_link"));

//  ros_node_.param("~arm_control_topic_name",arm_control_topic_name_,std::string("r_arm_cartesian_pose_controller/command"));
  ros_node_.param("~arm_control_topic_name",arm_control_topic_name_,std::string("right_arm_ik_controller/command"));
  ros_node_.param("~base_control_topic_name",base_control_topic_name_,std::string("base/trajectory_controller/command"));

  ros_node_.param("~distance_goal",distance_goal_,0.4);
  ros_node_.param("~controller_frequency",controller_frequency_,40.0);
  ros_node_.param("~animate_frequency",animate_frequency_,10.0);

  ros_node_.param("~do_control",do_control_,true);

  cost_map_ros_ = new costmap_2d::Costmap2DROS("costmap", tf_);
  cost_map_ros_->getCostmapCopy(cost_map_);

  ros_node_.param<double>("~door_thickness", door_env_.door_thickness, 1.0);
  ros_node_.param<double>("~arm/min_workspace_angle",   door_env_.arm_min_workspace_angle, -0.2);
  ros_node_.param<double>("~arm/max_workspace_angle",   door_env_.arm_max_workspace_angle, M_PI);
  ros_node_.param<double>("~arm/min_workspace_radius",  door_env_.arm_min_workspace_radius, 0.0);
  ros_node_.param<double>("~arm/max_workspace_radius",  door_env_.arm_max_workspace_radius, 0.80);
  ros_node_.param<double>("~arm/discretization_angle",  door_env_.door_angle_discretization_interval, 0.01);

  double shoulder_x, shoulder_y;
  ros_node_.param<double>("~arm/shoulder/x",shoulder_x, 0.0);
  ros_node_.param<double>("~arm/shoulder/y",shoulder_y, -0.188);
  door_env_.shoulder.x = shoulder_x;	// changed from shoulder.y to shoulder.x
  door_env_.shoulder.y = shoulder_y;

  /** publishers */
  start_footprint_pub_ = ros_node_.advertise<geometry_msgs::PolygonStamped>("~start", 1);
  goal_footprint_pub_ = ros_node_.advertise<geometry_msgs::PolygonStamped>("~goal", 1);
  robot_footprint_pub_ = ros_node_.advertise<geometry_msgs::PolygonStamped>("~robot_footprint", 1);
  global_plan_pub_ = ros_node_.advertise<nav_msgs::Path>("~global_plan", 1);
  door_frame_pub_ = ros_node_.advertise<nav_msgs::Path>("~door/frame", 1);
  door_pub_ = ros_node_.advertise<nav_msgs::Path>("~door/door", 1);
  viz_markers_ = ros_node_.advertise<visualization_msgs::Marker>( "visualization_marker",1);
  display_path_pub_ = ros_node_.advertise<motion_planning_msgs::KinematicPath>("display_kinematic_path", 10);
  pr2_ik_pub_ = ros_node_.advertise<manipulation_msgs::IKRequest>(arm_control_topic_name_,1);
  base_control_pub_ = ros_node_.advertise<manipulation_msgs::JointTraj>(base_control_topic_name_,1);
	viz_marker_array_pub_ = ros_node_.advertise<visualization_msgs::MarkerArray>("visualization_marker_array", 3);
	
  /** subscribers */
  joints_subscriber_ = ros_node_.subscribe("/joint_states", 1, &SBPLDoorPlanner::jointsCallback, this);

  /** for the arm planner */
  ros_node_.param ("~num_joints", num_joints_, 7);
  joint_names_.resize(num_joints_);
  ros_node_.param<std::string>("~joint_name_0", joint_names_[0], "r_shoulder_pan_joint");
  ros_node_.param<std::string>("~joint_name_1", joint_names_[1], "r_shoulder_lift_joint");
  ros_node_.param<std::string>("~joint_name_2", joint_names_[2], "r_upper_arm_roll_joint");
  ros_node_.param<std::string>("~joint_name_3", joint_names_[3], "r_elbow_flex_joint");
  ros_node_.param<std::string>("~joint_name_4", joint_names_[4], "r_forearm_roll_joint");
  ros_node_.param<std::string>("~joint_name_5", joint_names_[5], "r_wrist_flex_joint");
  ros_node_.param<std::string>("~joint_name_6", joint_names_[6], "r_wrist_roll_joint");
  ros_node_.param("~gripper_palm_wrist_distance",gripper_palm_wrist_distance_,0.10);

  double tmp;
  ros_node_.param<double>("~vel/x_limit",tmp,0.4);
  velocity_limits_.push_back(tmp);
  ros_node_.param<double>("~vel/y_limit",tmp,0.4);
  velocity_limits_.push_back(tmp);
  ros_node_.param<double>("~vel/theta_limit",tmp,1.0);
  velocity_limits_.push_back(tmp);
  ros_node_.param<double>("~vel/door_limit",tmp,0.1);
  velocity_limits_.push_back(tmp);
  ros_node_.param<int>("~door_index",door_index_,3);

  ros_node_.param<std::string>("~trajectory_type", trajectory_type_, "linear");
  

  //create a square footprint
  geometry_msgs::Point pt;
  pt.x = inscribed_radius_;
  pt.y = -1 * (inscribed_radius_);
  footprint_.push_back(pt);
  pt.x = -1 * (inscribed_radius_);
  pt.y = -1 * (inscribed_radius_);
  footprint_.push_back(pt);
  pt.x = -1 * (inscribed_radius_);
  pt.y = inscribed_radius_;
  footprint_.push_back(pt);
  pt.x = inscribed_radius_;
  pt.y = inscribed_radius_;
  footprint_.push_back(pt);
};

SBPLDoorPlanner::~SBPLDoorPlanner()
{
  if(cost_map_ros_ != NULL)
    delete cost_map_ros_;
}

void PrintUsage(char *argv[])
{
  printf("USAGE: %s <cfg file>\n", argv[0]);
}

bool SBPLDoorPlanner::initializePlannerAndEnvironment(const door_msgs::Door &door)
{
  door_env_.door = door;
  try 
  {
    cost_map_ros_->getCostmapCopy(cost_map_);
    cm_access_.reset(mpglue::createCostmapAccessor(&cm_getter_));
    cm_index_.reset(mpglue::createIndexTransform(&cm_getter_));

    FILE *action_fp;
    std::string filename = "sbpl_action_tmp.txt";
    std::string sbpl_action_string;
    if(!ros_node_.getParam("~sbpl_action_file", sbpl_action_string))
      return false;
    action_fp = fopen(filename.c_str(),"wt");
    fprintf(action_fp,"%s",sbpl_action_string.c_str());
    fclose(action_fp);
    double nominalvel_mpersecs, timetoturn45degsinplace_secs;
    ros_node_.param("~nominalvel_mpersecs", nominalvel_mpersecs, 0.4);
    ros_node_.param("~timetoturn45degsinplace_secs", timetoturn45degsinplace_secs, 0.6);
    env_.reset(mpglue::SBPLEnvironment::createXYThetaDoor(cm_access_, cm_index_, footprint_, nominalvel_mpersecs,timetoturn45degsinplace_secs, filename, 0, door));
	
    boost::shared_ptr<SBPLPlanner> sbplPlanner;
    if ("ARAPlanner" == planner_type_)
    {
      sbplPlanner.reset(new ARAPlanner(env_->getDSI(), forward_search_));
    }
    else if ("ADPlanner" == planner_type_)
    {
      sbplPlanner.reset(new ADPlanner(env_->getDSI(), forward_search_));
    }
    else 
    {
      ROS_ERROR("in MoveBaseSBPL: invalid planner_type_ \"%s\",use ARAPlanner or ADPlanner",planner_type_.c_str());
      throw int(5);
    }
    pWrap_.reset(new mpglue::SBPLPlannerWrap(env_, sbplPlanner));
  }
  catch (int ii) 
  {
    return false;
  }
  return true;
}

void SBPLDoorPlanner::jointsCallback(const sensor_msgs::JointStateConstPtr &joint_states)
{
  //this is a stupid way of doing things ?
  if (joint_states->get_name_size() != joint_states->get_position_size())
    ROS_ERROR("SBPL door planner received invalid joint state");
  else
    joint_states_ = *joint_states;
}

bool SBPLDoorPlanner::removeDoor()
{
  const std::vector<geometry_msgs::Point> door_polygon = door_functions::getPolygon(door_env_.door,door_env_.door_thickness); 
  for(int i=0; i < (int) door_polygon.size(); i++)
  {
    ROS_INFO("DOOR POLYGON %d:: %f, %f",i,door_polygon[i].x,door_polygon[i].y);
  }
  if(cost_map_.setConvexPolygonCost(door_polygon,costmap_2d::FREE_SPACE))
  {
    ROS_INFO("Reinflating obstacles everywhere around %f, %f",(door_env_.door.frame_p1.x+door_env_.door.frame_p2.x)/2.0,(door_env_.door.frame_p1.y+door_env_.door.frame_p2.y)/2.0 );
    //make sure to re-inflate obstacles in the affected region
    cost_map_.reinflateWindow((door_env_.door.frame_p1.x+door_env_.door.frame_p2.x)/2.0, (door_env_.door.frame_p1.y+door_env_.door.frame_p2.y)/2.0, 1.0,1.0);
    return true;
  }
  ROS_INFO("Could not remove door");
  return false;
}

bool SBPLDoorPlanner::makePlan(const pr2_robot_actions::Pose2D &start, const pr2_robot_actions::Pose2D &goal, manipulation_msgs::JointTraj &path)
{
  ROS_INFO("[replan] getting fresh copy of costmap");
  lock_.lock();
  cost_map_ros_->getCostmapCopy(cost_map_);
  lock_.unlock();
  clearRobotFootprint(cost_map_);
  if(!removeDoor())
  {
    ROS_ERROR("Could not remove the door from the costmap");
    return false;
  }

  publishFootprint(start,start_footprint_pub_,0,1.0,0);
  publishFootprint(goal,goal_footprint_pub_,0,0,1.0);
  ROS_INFO("[replan] replanning...");


  try {
    int max_cost = costmap_2d::FREE_SPACE;
    int min_cost = costmap_2d::NO_INFORMATION;
    // Update costs
    //cm_access_.reset(mpglue::createCostmapAccessor(&cm_getter_));
    //cm_index_.reset(mpglue::createIndexTransform(&cm_getter_));
    for (mpglue::index_t ix(cm_access_->getXBegin()); ix < cm_access_->getXEnd(); ++ix) {
      for (mpglue::index_t iy(cm_access_->getYBegin()); iy < cm_access_->getYEnd(); ++iy) {
        mpglue::cost_t cost;
        if (cm_access_->getCost(ix, iy, &cost)) { // always succeeds though
          // Note that ompl::EnvironmentWrapper::UpdateCost() will
          // check if the cost has actually changed, and do nothing
          // if it hasn't.  It internally maintains a list of the
          // cells that have actually changed, and this list is what
          // gets "flushed" to the planner (a couple of lines
          // further down).
          if(cost == costmap_2d::NO_INFORMATION)
            cost = costmap_2d::FREE_SPACE;
          env_->UpdateCost(ix, iy, cost);
          max_cost = std::max<int>(cost,max_cost);
          min_cost = std::min<int>(cost,min_cost);
          ROS_DEBUG("Updated cost");
        }
      }
    }
    ROS_DEBUG("Max cost: %d, Min cost: %d",max_cost,min_cost);
    
    // Tell the planner about the changed costs. Again, the called
    // code checks whether anything has really changed before
    // embarking on expensive computations.
		
    /** commented out for now because it doesn't exist, replaced with the following line */
    // pWrap_->flushCostChanges(true);
    pWrap_->forcePlanningFromScratch(true);
		
    // Assume the robot is constantly moving, so always set start.
    // Maybe a bit inefficient, but not as bad as "changing" the
    // goal when it hasn't actually changed.
    pWrap_->setStart(start.x, start.y, start.th);
    pWrap_->setGoal(goal.x, goal.y, goal.th);
	
    // BTW if desired, we could call pWrap_->forcePlanningFromScratch(true)...
	
    // Invoke the planner, updating the statistics in the process.
    // The returned plan might be empty, but it will not contain a
    // null pointer.  On planner errors, the createPlan() method
    // throws a std::exception.
    try
    {
      boost::shared_ptr<mpglue::waypoint_plan_t> plan(pWrap_->createPlan());
      if (plan->empty()) 
      {
        ROS_ERROR("No plan found\n");
        return false;
      }
      else
      {
        path.points.clear();
        ROS_DEBUG("Found a path with %d points",(int) plan->size());
        for(int i=0; i < (int) plan->size(); i++)
        {
          manipulation_msgs::JointTrajPoint path_point;
          mpglue::waypoint_s const * wpt((*plan)[i].get());
          mpglue::door_waypoint_s const * doorwpt(dynamic_cast<mpglue::door_waypoint_s const *>(wpt));	
          path_point.positions.push_back(doorwpt->x);
          path_point.positions.push_back(doorwpt->y);
          path_point.positions.push_back(doorwpt->theta);
          path_point.positions.push_back(doorwpt->min_door_angle);
//          path_point.positions.push_back(doorwpt->plan_interval);
          path.points.push_back(path_point);
          ROS_DEBUG("Path point: %f %f %f %f %d",doorwpt->x,doorwpt->y,doorwpt->theta,doorwpt->min_door_angle,(int)doorwpt->plan_interval);
        }
      }
    }
    catch (std::exception &e)
    {
      ROS_ERROR("Planning failed");
      return false;
    }
    //path.points.clear();// just paranoid
    //mpglue::PlanConverter::convertToJointTraj(plan.get(), &path);
    return true;
  }
  catch (std::runtime_error const & ee) {
    ROS_ERROR("runtime_error in makePlan(): %s\n", ee.what());
  }
  return false;
}

robot_actions::ResultStatus SBPLDoorPlanner::execute(const door_msgs::DoorCmd& door_cmd, door_msgs::Door& feedback)
{
  door_msgs::Door door_msg_in = door_cmd.door;
  door_open_direction_ = door_cmd.side;
  manipulation_msgs::JointTraj path;
  door_msgs::Door door;
  if(!updateGlobalPose())
  {
    return robot_actions::ABORTED;
  }

  ROS_DEBUG("Current position: %f %f %f",global_pose_2D_.x,global_pose_2D_.y,global_pose_2D_.th);

  if (!door_functions::transformTo(tf_,global_frame_,door_msg_in,door))
  {
    return robot_actions::ABORTED;
  }
  cout << "SBPLDoorPlanner::Door input" <<  door;

  ROS_DEBUG("Initializing planner and environment");
   
  if(!initializePlannerAndEnvironment(door))
  {
    ROS_ERROR("Door planner and environment not initialized");
    return robot_actions::ABORTED;
  }

  ROS_DEBUG("Door planner and environment initialized");
  KDL::Vector door_normal = getFrameNormal(door);
  goal_.th = atan2(door_normal(1),door_normal(0));
  goal_.x = (door.frame_p1.x+door.frame_p2.x)/2.0 + distance_goal_ * cos(goal_.th);
  goal_.y = (door.frame_p1.y+door.frame_p2.y)/2.0 + distance_goal_ * sin(goal_.th);

  if(door_open_direction_ == door_msgs::DoorCmd::PULL)
  {
    goal_.th = atan2(-door_normal(1),-door_normal(0));
    goal_.x = (door.frame_p1.x+door.frame_p2.x)/2.0 - distance_goal_ * cos(goal_.th);
    goal_.y = (door.frame_p1.y+door.frame_p2.y)/2.0 - distance_goal_ * sin(goal_.th);
  }

  publishDoor(door_env_.door,getFrameAngle(door_env_.door));
  ROS_DEBUG("Goal: %f %f %f",goal_.x,goal_.y,goal_.th);
  if(!isPreemptRequested())
  {
    if(!makePlan(global_pose_2D_, goal_, path))
    {
      return robot_actions::ABORTED;      
    }
    else
    {
      ROS_INFO("Found solution");
    }
  }
  if(!isPreemptRequested())
  {
    ROS_INFO("Publishing path");
    publishPath(path,global_plan_pub_,1.0,0.0,0.0,0.0);
  }

  handle_hinge_distance_ = getHandleHingeDistance(door_env_.door);

  manipulation_msgs::JointTraj new_path;
  processPlan(path,new_path);

  feedback = door;
  if(new_path.points.size() != path.points.size())
  {
    feedback = rotateDoor(door,new_path.points.back().positions[3]-getFrameAngle(door_env_.door));
  }

  if(animate_ && !isPreemptRequested())
  {
    ROS_INFO("Animating path");
    animate(new_path);
  }

  if(do_control_)
  {
    dispatchControl(new_path,door);
  }

  return robot_actions::SUCCESS;
}

void SBPLDoorPlanner::dispatchControl(const manipulation_msgs::JointTraj &path, const door_msgs::Door &door)
{
  int plan_count = 0;
  ros::Rate control_rate(controller_frequency_);
  while(!isPreemptRequested() && plan_count < (int) path.get_points_size())
  {
    manipulation_msgs::JointTraj base_plan;
    manipulation_msgs::JointTrajPoint path_point;
    path_point.positions.push_back(path.points[plan_count].positions[0]);
    path_point.positions.push_back(path.points[plan_count].positions[1]);
    path_point.positions.push_back(angles::normalize_angle(path.points[plan_count].positions[2]));
    base_plan.points.push_back(path_point);    
    tf::Stamped<tf::Pose> gripper_pose = getGlobalHandlePosition(door,angles::normalize_angle(path.points[plan_count].positions[3]-getFrameAngle(door)));
    tf::Pose gripper_rotate(tf::Quaternion(0.0,0.0,M_PI/2.0),tf::Vector3(0.0,0.0,0.0));
    tf::Pose gripper_pullback(tf::Quaternion(0,0,0),tf::Vector3(-gripper_palm_wrist_distance_,0.0,0.0));
    gripper_pose.mult(gripper_pose,gripper_rotate);
    gripper_pose.mult(gripper_pose,gripper_pullback);

    geometry_msgs::PoseStamped gripper_msg;
    gripper_pose.stamp_ = ros::Time::now();
    tf::poseStampedTFToMsg(gripper_pose, gripper_msg);

    manipulation_msgs::IKRequest cmd;
    cmd.pose_stamped = gripper_msg;
		
    base_control_pub_.publish(base_plan);
    pr2_ik_pub_.publish(cmd);

    plan_count++;
    ROS_DEBUG("Cmd: %d of %d",plan_count,path.get_points_size());
    if (!control_rate.sleep())
    {
      ROS_WARN("Control loop missed its desired cycle rate of %.4f Hz", controller_frequency_);
    }
  }
}

void SBPLDoorPlanner::processPlan(const manipulation_msgs::JointTraj &path, manipulation_msgs::JointTraj &return_path)
{

  int rot_dir = 1.0;
  if(door_env_.door.rot_dir == door_env_.door.ROT_DIR_CLOCKWISE) 
    rot_dir = -1.0;
  double global_yaw = getFrameAngle(door_env_.door);  //Take the plan and add the last few steps to open the door
  double global_open_angle = global_yaw+rot_dir*M_PI/2.0;

  return_path = path;

  manipulation_msgs::JointTrajPoint additional_point = path.points.back();
  additional_point.positions[3] = global_open_angle;
  return_path.points.push_back(additional_point);    
  printPlan("Original",return_path);
  manipulation_msgs::JointTraj pruned_path = prunePlan(return_path,path.points.front().positions[door_index_],global_open_angle,0.05,true);
  printPlan("Pruned",pruned_path);
  manipulation_msgs::JointTraj scaled_path;

  double control_dt;
  if(controller_frequency_ > 1)
    control_dt = 1.0/controller_frequency_;
  else
    control_dt = 0.1;

  if(!scalePlan(pruned_path,control_dt,scaled_path))
  {
    ROS_ERROR("Could not scale the plan");
  }
  //Break the plan up into two parts
  int switch_arm_index = -1;
  for(int i=0; i < (int)scaled_path.points.size(); i++)
  {
    if(checkArmDoorCollide(scaled_path.points[i]))
    {
      switch_arm_index = i;
      break;
    }
  }
  switch_arm_index = std::max(switch_arm_index-10,0);
  if(switch_arm_index > 0)
  {
    ROS_INFO("Breaking plan into two parts: %d,%d",switch_arm_index,scaled_path.points.size()-switch_arm_index);
    scaled_path.points.resize(switch_arm_index);
  }
  return_path = scaled_path;
	
	
	//display the expanded states in rviz
	displayExpandedStates();
}

manipulation_msgs::JointTraj SBPLDoorPlanner::prunePlan(const manipulation_msgs::JointTraj &path, 
                                                        const double &start_door_angle, 
                                                        const double &end_door_angle, 
                                                        const double &tolerance,
                                                        const bool &impose_monotonic)
{
  if(path.points.size() < 4)
  {
    ROS_ERROR("Trajectory has only %d elements, must have 4 elements",(int) path.points.size());
  }
  else
  {
    ROS_INFO("Trajectory has dimension: %d, points: %d",(int) path.points[0].positions.size(), (int) path.points.size());
  }
  manipulation_msgs::JointTraj result = path;  
  std::vector<manipulation_msgs::JointTrajPoint>::iterator it=result.points.begin();
  while(it != result.points.end())
//  for(std::vector<manipulation_msgs::JointTrajPoint>::iterator it=result.points.begin(); it != result.points.end(); it++)
  {
    if( fabs(angles::shortest_angular_distance(it->positions[door_index_],start_door_angle)) <= tolerance)
    {   
      break;
    }
    else
    {
      it = result.points.erase(it);
    }
  }  
  if(result.points.empty())
  {
    ROS_ERROR("No door path found");
    return result;
  }
  else
  {
    printPlan("beginPrune",result);
  }
  for(std::vector<manipulation_msgs::JointTrajPoint>::iterator it=result.points.begin(); it != result.points.end(); it++)
  {
    if(fabs(angles::shortest_angular_distance(it->positions[door_index_],end_door_angle)) <= tolerance)
    {   
      if((it+1) == result.points.end())
        break;
      else
      {
        result.points.erase(it+1,result.points.end());
        break;
      }
    }
  }  
  if(result.points.empty()){
    ROS_ERROR("No door path found");
    return result;
  }
  else
  {
    printPlan("endPrune",result);
  }
  if(impose_monotonic)
  {
    bool move_positive = true;
    if(angles::shortest_angular_distance(start_door_angle,end_door_angle) < 0)
    {
      move_positive = false;
    }
    std::vector<manipulation_msgs::JointTrajPoint>::iterator it=result.points.begin();
    while(it != result.points.end())
    {
      if(it == result.points.begin())
      {   
        ++it;
        continue;
      }
      if(angles::shortest_angular_distance((it-1)->positions[door_index_],it->positions[door_index_]) < 0 && move_positive)
      {
        it = result.points.erase(it);
        continue;
      }
      if(angles::shortest_angular_distance((it-1)->positions[door_index_],it->positions[door_index_])  > 0 && !move_positive)
      {
        it = result.points.erase(it);
        continue;
      }
      ++it;
    } 
    printPlan("imposeMonotonic",result); 
  }
  return result;
}

bool SBPLDoorPlanner::scalePlan(const manipulation_msgs::JointTraj &traj, const double dT, manipulation_msgs::JointTraj &traj_out)
{
  std::vector<trajectory::Trajectory::TPoint> result_c;
  trajectory::Trajectory traj_c(traj.points[0].positions.size());
  manipulation_msgs::JointTraj result;
  traj_c.setJointWraps(2);
  traj_c.setJointWraps(3);
  traj_c.autocalc_timing_ = true;
  traj_c.setMaxRates(velocity_limits_);
  traj_c.setInterpolationMethod(trajectory_type_);
  if(!createTrajectoryFromMsg(traj,traj_c))
    return false;
  traj_c.getTrajectory(result_c,dT);
//  traj_c.write(std::string("door_planner_traj.txt"),dT);
  if(!createMsgFromTrajectory(result_c,result))
     return false;
  traj_out = result;
  printPlan("Scaled",traj_out);
  return true;
}

void SBPLDoorPlanner::printPlan(const std::string name, manipulation_msgs::JointTraj &traj_out)
{
  ROS_INFO(" ");
  ROS_INFO("%s plan has %d points",name.c_str(),traj_out.points.size());
  for(int i=0; i < (int) traj_out.points.size(); i++)
  {
    ROS_INFO("%d: %f %f %f %f",i,traj_out.points[i].positions[0],traj_out.points[i].positions[1],traj_out.points[i].positions[2],traj_out.points[i].positions[3]);
  }
}

bool SBPLDoorPlanner::createTrajectoryPointsVectorFromMsg(const manipulation_msgs::JointTraj &new_traj, std::vector<trajectory::Trajectory::TPoint> &tp)
{
  if(new_traj.points.empty())
  {
    ROS_WARN("Trajectory message has no way points");
    return false;
  }

  tp.resize((int)new_traj.get_points_size());

  for(int i=0; i < (int) new_traj.get_points_size(); i++)
  {
    tp[i].setDimension((int) new_traj.points[0].positions.size());
    for(int j=0; j < (int) new_traj.points[0].positions.size(); j++)
    {
      tp[i].q_[j] = new_traj.points[i].positions[j];
      tp[i].time_ = new_traj.points[i].time;
    }
  }
  return true;
}

bool SBPLDoorPlanner::createMsgFromTrajectory(const std::vector<trajectory::Trajectory::TPoint> &tp, manipulation_msgs::JointTraj &new_traj)
{

  if(tp.empty())
    return false;

  new_traj.points.resize((int) tp.size());
  for(int i=0; i < (int) tp.size(); i++)
  {
    new_traj.points[i].positions.resize((int)tp[i].q_.size());
    for(int j=0; j < (int) tp[i].q_.size(); j++)
    {
      new_traj.points[i].positions[j] = tp[i].q_[j];
    }
  }
  return true;
}

bool SBPLDoorPlanner::createTrajectoryFromMsg(const manipulation_msgs::JointTraj &new_traj,trajectory::Trajectory &return_trajectory)
{
  std::vector<trajectory::Trajectory::TPoint> tp;
  if(!createTrajectoryPointsVectorFromMsg(new_traj, tp)){
    return false;
  }
  if(!return_trajectory.setTrajectory(tp)){
    ROS_WARN("Trajectory not set correctly");
    return false;
  }
  return true;
}

bool SBPLDoorPlanner::checkArmDoorCollide(const manipulation_msgs::JointTrajPoint &waypoint)
{
  geometry_msgs::Point32 global_shoulder_position;

  // rotate the door
  door_msgs::Door rotated_door = door_functions::rotateDoor(door_env_.door, waypoint.positions[3]-getFrameAngle(door_env_.door));

  //global handle position
  tf::Stamped<tf::Pose>  global_handle_position_tf = getGlobalHandlePosition(door_env_.door, waypoint.positions[3]);

  geometry_msgs::PoseStamped handle_msg;
  geometry_msgs::Point32 handle_position;
  global_handle_position_tf.stamp_ = ros::Time::now();
  tf::poseStampedTFToMsg(global_handle_position_tf, handle_msg);

  //get shoulder in global frame
  global_shoulder_position.x = waypoint.positions[0] + (door_env_.shoulder.x*cos(waypoint.positions[2])-door_env_.shoulder.y*sin(waypoint.positions[2]));
  global_shoulder_position.y = waypoint.positions[1] + (door_env_.shoulder.x*sin(waypoint.positions[2])+door_env_.shoulder.y*cos(waypoint.positions[2]));

  handle_position.x = handle_msg.pose.position.x;
  handle_position.y = handle_msg.pose.position.y;
  handle_position.z = handle_msg.pose.position.z;

  //debug
//  printPoint("rotated_door.door_p1",rotated_door.door_p1);
//  printPoint("rotated_door.door_p2",rotated_door.door_p2);
//  printPoint("global_handle_position",global_handle_position);
//  printPoint("global_shoulder_position",global_shoulder_position);

  return doLineSegsIntersect(rotated_door.door_p1, rotated_door.door_p2, global_shoulder_position, handle_position);
}

bool SBPLDoorPlanner::doLineSegsIntersect(geometry_msgs::Point32 a, geometry_msgs::Point32 b, geometry_msgs::Point32 c, geometry_msgs::Point32 d)
{
  double b_a[2], c_d[2], c_a[2];
  b_a[0] = b.x-a.x;
  b_a[1] = b.y-a.y;

  c_d[0] = c.x-d.x;
  c_d[1] = c.y-d.y;

  c_a[0] = c.x-a.x;
  c_a[1] = c.y-a.y;

  double det = (b_a[0]*c_d[1]) - (b_a[1]*c_d[0]);
  double t = ((c_a[0]*c_d[1]) - (c_a[1]*c_d[0])) / det;
  double u = ((b_a[0]*c_a[1]) - (b_a[1]*c_a[0])) / det;

  if ((t<0)||(u<0)||(t>1)||(u>1))
    return false;
  else
    return true;
}

void SBPLDoorPlanner::animate(const manipulation_msgs::JointTraj &path)
{
  ros::Rate animate_rate(animate_frequency_);
  for(int i=0; i < (int) path.get_points_size(); i++)
  {
    pr2_robot_actions::Pose2D draw;
    draw.x = path.points[i].positions[0];
    draw.y = path.points[i].positions[1];
    draw.th = path.points[i].positions[2];
    publishFootprint(draw,robot_footprint_pub_,0.5,0.5,0);
    publishDoor(door_env_.door,path.points[i].positions[3]);
    publishGripper(angles::normalize_angle(path.points[i].positions[3]-getFrameAngle(door_env_.door)));
    if (!animate_rate.sleep())
    {
      ROS_WARN("Animate loop missed its desired cycle rate of %.4f Hz", animate_frequency_);
    }
  }
}

void SBPLDoorPlanner::publishGripper(const double &angle)
{
  door_msgs::Door result = rotateDoor(door_env_.door,angle);
  geometry_msgs::PoseStamped gripper_msg;

  tf::Stamped<tf::Pose> gripper_pose = getGlobalHandlePosition(door_env_.door,angle);
  gripper_pose.stamp_ = ros::Time::now();
  tf::poseStampedTFToMsg(gripper_pose, gripper_msg);
  visualization_msgs::Marker marker;
  marker.header.frame_id = gripper_msg.header.frame_id;
  marker.header.stamp = ros::Time();
  marker.ns = "~";
  marker.id = 1;
  marker.type = visualization_msgs::Marker::ARROW;
  marker.action = visualization_msgs::Marker::ADD;
  marker.pose = gripper_msg.pose;
  marker.scale.x = 0.10;
  marker.scale.y = 0.05;
  marker.scale.z = 0.05;
  marker.color.a = 1.0;
  marker.color.r = 0.0;
  marker.color.g = 1.0;
  marker.color.b = 0.0;
  viz_markers_.publish(marker);
}

tf::Stamped<tf::Pose> SBPLDoorPlanner::getGlobalHandlePosition(const door_msgs::Door &door_in, const double &angle)
{
  door_msgs::Door door = door_in;
  door.header.stamp = ros::Time::now();
  tf::Stamped<tf::Pose> handle_pose = getGripperPose(door,angle,handle_hinge_distance_,door_open_direction_);
  return handle_pose;
}

double SBPLDoorPlanner::getHandleHingeDistance(const door_msgs::Door &door)
{
  geometry_msgs::Point32 hinge;
  if(door.hinge == door.HINGE_P1)
    hinge = door.frame_p1;
  else
    hinge = door.frame_p2;
  double result = sqrt((hinge.x-door.handle.x)*(hinge.x-door.handle.x)+(hinge.y-door.handle.y)*(hinge.y-door.handle.y));
  ROS_INFO("Handle hinge distance is %f",result);
  return result;
}

void SBPLDoorPlanner::publishPath(const manipulation_msgs::JointTraj &path, const ros::Publisher &pub, double r, double g, double b, double a)
{
  nav_msgs::Path gui_path_msg;
  gui_path_msg.header.frame_id = global_frame_;
  gui_path_msg.header.stamp = ros::Time::now();

  //given an empty path we won't do anything
  if(path.get_points_size() > 0)
  {
    // Extract the plan in world co-ordinates, we assume the path is all in the same frame
    //    gui_path_msg.header.stamp = door_env_.door.header.stamp;
    gui_path_msg.set_poses_size(path.get_points_size());
    for(unsigned int i=0; i < path.get_points_size(); i++){
      gui_path_msg.poses[i].pose.position.x = path.points[i].positions[0];
      gui_path_msg.poses[i].pose.position.y = path.points[i].positions[1];
      gui_path_msg.poses[i].pose.position.z = 0.0;
    }

    pub.publish(gui_path_msg);
  }
}

bool SBPLDoorPlanner::updateGlobalPose()
{
  tf::Stamped<tf::Pose> robot_pose;
  robot_pose.setIdentity();
  robot_pose.frame_id_ = robot_base_frame_;
  robot_pose.stamp_ = ros::Time();

  try
  {
    tf_.transformPose(global_frame_, robot_pose, global_pose_);
  }
  catch(tf::LookupException& ex) {
    ROS_ERROR("No Transform available Error: %s\n", ex.what());
    return false;
  }
  catch(tf::ConnectivityException& ex) {
    ROS_ERROR("Connectivity Error: %s\n", ex.what());
    return false;
  }
  catch(tf::ExtrapolationException& ex) {
    ROS_ERROR("Extrapolation Error: %s\n", ex.what());
    return false;
  }
  global_pose_2D_ = getPose2D(global_pose_);
  return true;
}

pr2_robot_actions::Pose2D SBPLDoorPlanner::getPose2D(const tf::Stamped<tf::Pose> &pose)
{
  pr2_robot_actions::Pose2D tmp_pose;
  double useless_pitch, useless_roll, yaw;
  pose.getBasis().getEulerZYX(yaw, useless_pitch, useless_roll);
  tmp_pose.x = pose.getOrigin().x();
  tmp_pose.y = pose.getOrigin().y();
  tmp_pose.th = yaw;
  return tmp_pose;
}

bool SBPLDoorPlanner::computeOrientedFootprint(const pr2_robot_actions::Pose2D &position, const std::vector<geometry_msgs::Point>& footprint_spec, std::vector<geometry_msgs::Point>& oriented_footprint)
{
  if(footprint_spec.size() < 3)//if we have no footprint... do nothing
  {
    ROS_ERROR("No footprint available");
    return false;
  }
  oriented_footprint.clear();
  double cos_th = cos(position.th);
  double sin_th = sin(position.th);
  for(unsigned int i = 0; i < footprint_spec.size(); ++i) //build the oriented footprint
  {
    geometry_msgs::Point new_pt;
    new_pt.x = position.x + (footprint_spec[i].x * cos_th - footprint_spec[i].y * sin_th);
    new_pt.y = position.y + (footprint_spec[i].x * sin_th + footprint_spec[i].y * cos_th);
    oriented_footprint.push_back(new_pt);
    ROS_DEBUG("Oriented footprint:: %f, %f",new_pt.x,new_pt.y);
  }
  ROS_DEBUG("Oriented footprint:: %d, %f",footprint_spec.size(),position.th);
  return true;
}

bool SBPLDoorPlanner::clearRobotFootprint(costmap_2d::Costmap2D& cost_map)
{
  std::vector<geometry_msgs::Point> oriented_footprint;
  computeOrientedFootprint(global_pose_2D_,footprint_,oriented_footprint);

  //set the associated costs in the cost map to be free
  if(!cost_map_.setConvexPolygonCost(oriented_footprint, costmap_2d::FREE_SPACE))
  {
    ROS_DEBUG("Could not clear footprint");
    return false;
  }
  double max_inflation_dist = inflation_radius_ + inscribed_radius_;
  //make sure to re-inflate obstacles in the affected region
  cost_map.reinflateWindow(global_pose_2D_.x, global_pose_2D_.y, max_inflation_dist, max_inflation_dist);
  return true;
}

void SBPLDoorPlanner::publishFootprint(const pr2_robot_actions::Pose2D &position, const ros::Publisher &pub, double r, double g, double b)
{
  std::vector<geometry_msgs::Point> oriented_footprint;
  computeOrientedFootprint(position,footprint_,oriented_footprint);

  geometry_msgs::PolygonStamped robot_polygon;
  robot_polygon.header.frame_id = global_frame_;
  robot_polygon.header.stamp = ros::Time();
  robot_polygon.polygon.set_points_size(2*footprint_.size());

  for(int i=0; i < (int) oriented_footprint.size(); ++i)
  {
    int index_2 = (i+1)%((int)oriented_footprint.size());
    robot_polygon.polygon.points[2*i].x = oriented_footprint[i].x;
    robot_polygon.polygon.points[2*i].y = oriented_footprint[i].y;
    robot_polygon.polygon.points[2*i].z = oriented_footprint[i].z;

    robot_polygon.polygon.points[2*i+1].x = oriented_footprint[index_2].x;
    robot_polygon.polygon.points[2*i+1].y = oriented_footprint[index_2].y;
    robot_polygon.polygon.points[2*i+1].z = oriented_footprint[index_2].z;
  }

  pub.publish(robot_polygon);
}

void SBPLDoorPlanner::publishDoor(const door_msgs::Door &door_in, const double &angle)
{
  door_msgs::Door door = rotateDoor(door_in,angles::normalize_angle(angle-getFrameAngle(door_in)));

  nav_msgs::Path marker_door;
  marker_door.header.frame_id = global_frame_;
  marker_door.header.stamp = ros::Time();
  marker_door.set_poses_size(2);

  marker_door.poses[0].pose.position.x = door.door_p1.x;
  marker_door.poses[0].pose.position.y = door.door_p1.y;
  marker_door.poses[0].pose.position.z = door.door_p1.z;

  marker_door.poses[1].pose.position.x = door.door_p2.x;
  marker_door.poses[1].pose.position.y = door.door_p2.y;
  marker_door.poses[1].pose.position.z = door.door_p2.z;

  nav_msgs::Path marker_frame;
  marker_frame.header.frame_id = global_frame_;
  marker_frame.header.stamp =  ros::Time();
  marker_frame.set_poses_size(2);

  marker_frame.poses[0].pose.position.x = door.frame_p1.x;
  marker_frame.poses[0].pose.position.y = door.frame_p1.y;
  marker_frame.poses[0].pose.position.z = door.frame_p1.z;

  marker_frame.poses[1].pose.position.x = door.frame_p2.x;
  marker_frame.poses[1].pose.position.y = door.frame_p2.y;
  marker_frame.poses[1].pose.position.z = door.frame_p2.z;

  visualization_msgs::Marker marker;
  marker.header.frame_id = global_frame_;
  marker.header.stamp = ros::Time();
  marker.ns = "~";
  marker.id = 0;
  marker.type = visualization_msgs::Marker::SPHERE;
  marker.action = visualization_msgs::Marker::ADD;
  marker.pose.position.x = door.handle.x;
  marker.pose.position.y = door.handle.y;
  marker.pose.position.z = door.handle.z;
  marker.pose.orientation.x = 0.0;
  marker.pose.orientation.y = 0.0;
  marker.pose.orientation.z = 0.0;
  marker.pose.orientation.w = 1.0;
  marker.scale.x = 0.03;
  marker.scale.y = 0.03;
  marker.scale.z = 0.03;
  marker.color.a = 1.0;
  marker.color.r = 1.0;
  marker.color.g = 1.0;
  marker.color.b = 1.0;

  viz_markers_.publish(marker);
  door_frame_pub_.publish(marker_frame);
  door_pub_.publish(marker_door);
}

// remove after testing
void SBPLDoorPlanner::printPoint(std::string name, geometry_msgs::Point32 point)
{
  printf("%s: y: %.3f y: %.3f z: %.3f\n", name.c_str(), point.x, point.y, point.z);
}

void SBPLDoorPlanner::updatePathVizualization(std::string frame_id, std::vector<std::string> &joints, std::vector<std::vector<double> > &joint_vals,
                                              std::vector<std::string> &planar_links, std::vector<std::vector<double> > &planar_vals)
{
  unsigned int i, j, path_length;	
 
  robot_path_.header.frame_id = frame_id;
  robot_path_.header.stamp = ros::Time::now();
	
  if(joint_vals.size() != planar_vals.size())
    ROS_INFO("The joint path and planar link path are not the same length.");

  // set length of path
  if(joint_vals.size() > planar_vals.size())
    path_length = joint_vals.size();
  else
    path_length = planar_vals.size();
	
  robot_path_.set_names_size(joints.size()+planar_links.size());
  robot_path_.set_states_size(path_length);

  // get start_state of the robot from joint_states topic
  for(i = 0; i < joint_states_.get_name_size(); ++i)
  {
    robot_path_.start_state[i].header = joint_states_.header;
    robot_path_.start_state[i].joint_name = joint_states_.name[i];
    robot_path_.start_state[i].value[0] = joint_states_.position[i];
  }

  // set names of arm joints in path
  for(i = 0; i < joints.size(); ++i)
    robot_path_.names[i] = joints[i];
	
  // set names of planar links in path
  for(i = 0; i < planar_links.size(); ++i)
    robot_path_.names[joints.size()+i] = planar_links[i];
	
  // add joint path values {theta}
  for(i = 0; i < path_length; ++i)
  {
    robot_path_.states[i].set_vals_size(joint_vals[i].size());
		
    if(i <= joint_vals.size())
    {
      for(j = 0; j < joint_vals[i].size(); ++j)
        robot_path_.states[i].vals[j] = joint_vals[i][j];
    }
    else
    {
      for(j = 0; j < joint_vals[joint_vals.size()-1].size(); ++j)
        robot_path_.states[i].vals[j] = joint_vals[joint_vals.size()-1][j];
    }			
  }
	
  // add planar links path {x,y,theta} 
  for(i = 0; i < path_length; ++i)
  {
    if(i <= planar_vals.size())
    {
      for(j = 0; j < planar_vals[i].size(); ++j)
        robot_path_.states[i].vals.push_back(planar_vals[i][j]);
    }
    else
    {
      for(j = 0; j < planar_vals[planar_vals.size()-1].size(); ++j)
        robot_path_.states[i].vals.push_back(planar_vals[planar_vals.size()-1][j]);
    }
  }
	
  display_path_pub_.publish(robot_path_);
}

/** \brief Check /joint_state for current joint angles */
void SBPLDoorPlanner::getCurrentJointAngles(const std::vector <std::string> &joint_names, std::vector <double> *joint_angles)
{
  int nind = 0;
  joint_angles->resize(num_joints_);
  for(unsigned int i = 0; i < joint_states_.get_name_size(); i++)
  {
    if(joint_names_[nind].compare(joint_states_.name[i]) == 0)
    {
      ROS_DEBUG("%s: %.3f",joint_states_.name[i].c_str(), joint_states_.position[i]);
      joint_angles->at(nind) = joint_states_.position[i];
      nind++;
    }
    if(nind == num_joints_)
      break;
  }
}

/*  double last_plan_angle = path.points.back().positions[3];
  double additional_open_angle = angles::shortest_angular_distance(last_plan_angle,global_open_angle);
  double delta_open_angle = 0.05;
  int num_intervals = fabs(additional_open_angle)/delta_open_angle;

  manipulation_msgs::JointTrajPoint additional_point = path.points.back();
  for(int i=0; i<num_intervals; i++)
  {
    additional_point.positions[3] = angles::normalize_angle(last_plan_angle + i*additional_open_angle/(double)num_intervals);
    return_path.points.push_back(additional_point);
    ROS_INFO("Additional point: %f %f %f %f",additional_point.positions[0],additional_point.positions[1],additional_point.positions[2],additional_point.positions[3]);
  }
  for(int i = 0; i < 20; i++)
  {
    additional_point.positions[3] = global_open_angle;
    return_path.points.push_back(additional_point);    
    ROS_INFO("Additional point: %f %f %f %f",additional_point.positions[0],additional_point.positions[1],additional_point.positions[2],additional_point.positions[3]);
  }
*/

void SBPLDoorPlanner::displayExpandedStates()
{
	unsigned int inc;
	std::vector<int> states;
	double x,y,theta;
	visualization_msgs::MarkerArray marker_array;
	mpglue::rfct_pose mp_pose(0,0,0);

	//get expanded state IDs
	states = env_->getDSI()->GetExpandedStates();

	//check if the list is empty
	if(states.empty())
	{
		ROS_INFO("There are no states in the expanded states list");
		return;
	}
	else
		ROS_INFO("There are %i states in the expanded states list",states.size());

	//if there are too many states, rviz will crash and burn when drawing
	if(states.size() > 50000)
		inc = 200;
	else if(states.size() > 5000)
		inc = 20;
	else if(states.size() > 500)
		inc = 10;
	else
		inc = 1;
		
	ROS_INFO("Expanded Nodes:");
	unsigned int mind = 0;
	for(unsigned int i = 0; i < states.size(); i=i+inc)
	{
		//cubes
		marker_array.set_markers_size(marker_array.get_markers_size()+2);
		marker_array.markers[mind].header.frame_id = "odom_combined";
		marker_array.markers[mind].header.stamp = ros::Time();
		marker_array.markers[mind].ns = "sbpl_door_planner";
		marker_array.markers[mind].id = mind;
		marker_array.markers[mind].type = visualization_msgs::Marker::CUBE;
		marker_array.markers[mind].action =  visualization_msgs::Marker::ADD;
		marker_array.markers[mind].scale.x = .05;
		marker_array.markers[mind].scale.y = .05;
		marker_array.markers[mind].scale.z = .05;
		marker_array.markers[mind].color.r = 1.0 - double(i)/double(states.size());
		marker_array.markers[mind].color.g = 0.0 + double(i)/double(states.size());
		marker_array.markers[mind].color.b = 0.0;
		marker_array.markers[mind].color.a = 0.5;
		marker_array.markers[mind].lifetime = ros::Duration(180.0);

		
		mp_pose = env_->GetPoseFromState(states[i]);
		cm_index_->localToGlobal(mp_pose.x, mp_pose.y, mp_pose.th, &x, &y, &theta);
		marker_array.markers[mind].pose.position.x = x;
		marker_array.markers[mind].pose.position.y = y;
		marker_array.markers[mind].pose.position.z = 0.01;
		
		++mind;
		
		//arrows
		marker_array.markers[mind].header.frame_id = "odom_combined";
		marker_array.markers[mind].header.stamp = ros::Time();
		marker_array.markers[mind].ns = "sbpl_door_planner";
		marker_array.markers[mind].id = mind;
		marker_array.markers[mind].type = visualization_msgs::Marker::ARROW;
		marker_array.markers[mind].action =  visualization_msgs::Marker::ADD;
		marker_array.markers[mind].scale.x = 0.05;
		marker_array.markers[mind].scale.y = 0.075;
		marker_array.markers[mind].scale.z = 0.05;
		marker_array.markers[mind].color.r = 0.0;
		marker_array.markers[mind].color.g = 0.5;
		marker_array.markers[mind].color.b = 0.5;
		marker_array.markers[mind].color.a = 0.5;
		marker_array.markers[mind].lifetime = ros::Duration(180.0);

		marker_array.markers[mind].pose.position.x = x;
		marker_array.markers[mind].pose.position.y = y;
		marker_array.markers[mind].pose.position.z = 0.01;

		btQuaternion marker_quat(theta,0,0);
		ROS_INFO("%i: x:%.3f  y:%.3f  theta:%.3f",i,x,y,theta);	
		ROS_INFO("      theta: %.3f  -->  x: %.3f  y:%.3f   z:%.3f   w:%.3f",theta, marker_quat.getX(),marker_quat.getY(),marker_quat.getZ(),marker_quat.getW());
		marker_array.markers[mind].pose.orientation.x = marker_quat.getX();
		marker_array.markers[mind].pose.orientation.y = marker_quat.getY();
		marker_array.markers[mind].pose.orientation.z = marker_quat.getZ();
		marker_array.markers[mind].pose.orientation.w = marker_quat.getW();

		++mind;
	}
	ROS_DEBUG("published %d markers in the marker array", marker_array.get_markers_size());
	viz_marker_array_pub_.publish(marker_array);
}


