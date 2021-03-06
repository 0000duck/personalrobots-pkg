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
 *   * Neither the name of Willow Garage nor the names of its
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
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING INeco
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/


#include <plugs_core/action_plug_in.h>


namespace plugs_core {

const double READY_TO_INSERT = -0.026;
const double READY_TO_PUSH = -0.0150;  // -0.0135;  // -0.009;
const double FORCING_SUCCESSFUL = -0.009;
const double MIN_STANDOFF = 0.022;

const double SUCCESS_THRESHOLD = 0.025;
enum {MEASURING, MOVING, INSERTING, FORCING, HOLDING};
const static char* TRACKER_ACTIVATE = "/plug_detector/activate_tracker";


double getOffset(int id)
{
  switch (id)
  {
  case 0: return 1.0;
  case 1: return 0.98948537037195827;
  case 6: return 0.97692542711765129;
  case 39: return 0.98097524550697912;
  case 4: return 0.97561703536950839;
  case 3: return 0.99092973248973848;
  case 38: return 0.95012308281214775;
  case 40: return 0.98361298809350939;
  case 27: return 0.9864272331729832;
  case 26: return 0.98778051326139238;
  case 25: return 0.98898110374305592;
  case 20: return 0.98890562635876034;
  case 21: return 0.9780777627427707;
  default:
    ROS_ERROR("Invalid outlet id: %d", id);
    return 0;
  }
}

PlugInAction::PlugInAction(ros::Node& node) :
  robot_actions::Action<std_msgs::Int32, std_msgs::Empty>("plug_in"),
  action_name_("plug_in"),
  node_(node),
  arm_controller_("r_arm_hybrid_controller"),
  battery_level_(95.0)
{
  node_.setParam("~roi_policy", "LastImageLocation");
  node_.setParam("~display", 0);
  node_.setParam("~square_size", 0.0042);
  node_.setParam("~board_width", 3);
  node_.setParam("~board_height",4);

  node_.param(action_name_ + "/arm_controller", arm_controller_, arm_controller_);

  if(arm_controller_ == "" )
  {
    ROS_ERROR("%s: Aborted, arm controller param was not set.", action_name_.c_str());
    terminate();
    return;
  }

  node_.advertise<manipulation_msgs::TaskFrameFormalism>(arm_controller_ + "/command", 2);

  TF_.reset(new tf::TransformListener(node));
  notifier_.reset(new tf::MessageNotifier<geometry_msgs::PoseStamped>(
                    TF_.get(), &node,
                    boost::bind(&PlugInAction::plugMeasurementCallback, this, _1),
                    "/plug_detector/plug_pose", "outlet_pose", 100));

  tff_msg_.header.frame_id = "outlet_pose";

  node_.advertise<plugs_core::PlugInState>(action_name_ + "/state", 10);
  node_.advertise<std_msgs::Empty>(TRACKER_ACTIVATE, 1);
}

PlugInAction::~PlugInAction()
{
  node_.unadvertise(TRACKER_ACTIVATE);
}

robot_actions::ResultStatus PlugInAction::execute(const std_msgs::Int32& outlet_id, std_msgs::Empty& feedback)
{
  reset();

  ros::Time started = ros::Time::now();
  outlet_id_ = outlet_id.data;
  ROS_INFO("plug_in running on outlet %d", outlet_id_);

  ros::Duration d; d.fromSec(0.001);
  while (isActive()) {
    d.sleep();
    if(ros::Time::now() - started > ros::Duration(60.0))
    {
      ROS_DEBUG("%s: aborted. Failed to plug_in in the alotted time", action_name_.c_str());
      deactivate(robot_actions::ABORTED,feedback);
    }
    if (isPreemptRequested())
    {
      ROS_DEBUG("%s: preempted.", action_name_.c_str());
      deactivate(robot_actions::PREEMPTED, feedback);
    }
    node_.publish(TRACKER_ACTIVATE, feedback);
  }

  return waitForDeactivation(feedback);
}

void PlugInAction::reset()
{
  outlet_id_ = 0;
  last_standoff_ = 1.0e10;
  g_state_ = MEASURING;
  g_started_inserting_ = ros::Time::now();
  g_started_forcing_ = ros::Time::now();
  g_stopped_forcing_ = ros::Time::now();
}

void poseTFToMsg(const tf::Pose &p, geometry_msgs::Twist &t)
{
  t.linear.x = p.getOrigin().x();
  t.linear.y = p.getOrigin().y();
  t.linear.z = p.getOrigin().z();
  btMatrix3x3(p.getRotation()).getEulerYPR(t.angular.z, t.angular.y, t.angular.x);
}

void PlugInAction::plugMeasurementCallback(const tf::MessageNotifier<geometry_msgs::PoseStamped>::MessagePtr &msg)
{
  plugs_core::PlugInState state_msg;


  if (!isActive())
    return;

  if (isPreemptRequested()){
    deactivate(robot_actions::PREEMPTED, empty_);
    ROS_DEBUG("%s: preempted.", action_name_.c_str());
    return;
  }
  ROS_DEBUG("%s: executing.", action_name_.c_str());

  tff_msg_.header.stamp = msg->header.stamp;
  // Both are transforms from the outlet to the estimated plug pose
  geometry_msgs::PoseStamped viz_offset_msg;
  usleep(10000);
  try {
    TF_->transformPose("outlet_pose", *(msg.get()), viz_offset_msg);
    TF_->lookupTransform("outlet_pose", arm_controller_ + "/tool_frame", msg->header.stamp, mech_offset_);
  }
  catch(tf::TransformException &ex)
  {
    ROS_ERROR("%s: Error, transform exception: %s", action_name_.c_str(), ex.what());
    return;
  }

  tf::Pose viz_offset;
  tf::poseMsgToTF(viz_offset_msg.pose, viz_offset);
  poseTFToMsg(viz_offset, state_msg.viz_offset);

  // Deals with the per-outlet offsets
  {
    double offset = getOffset(outlet_id_);
    tf::Stamped<tf::Transform> high_def_in_outlet;
    TF_->lookupTransform("outlet_pose", "high_def_frame", msg->header.stamp, high_def_in_outlet);
    tf::Vector3 v = -high_def_in_outlet.getOrigin();
    v *= (1 - offset);

    tf::Pose old = viz_offset;
    viz_offset.getOrigin() += v;
    ROS_INFO("Outlet offset of %.3lf moves the estimate from (%.3lf, %.3lf, %.3lf) to (%.3lf, %.3lf, %.3lf)", offset, old.getOrigin().x(), old.getOrigin().y(), old.getOrigin().z(), viz_offset.getOrigin().x(), viz_offset.getOrigin().y(), viz_offset.getOrigin().z());
  }

  double standoff = std::max(MIN_STANDOFF, viz_offset.getOrigin().length()  * 2.5/4.0);

  // Computes the offset for movement
  tf::Pose viz_offset_desi;
  viz_offset_desi.setIdentity();
  viz_offset_desi.getOrigin().setX(-standoff);
  viz_offset_desi.getOrigin().setY(0.0);
  viz_offset_desi.getOrigin().setZ(0.0);
  poseTFToMsg(viz_offset.inverse() * viz_offset_desi, state_msg.viz_error);
  mech_offset_desi_ = viz_offset.inverse() * viz_offset_desi * mech_offset_;

  prev_state_ = g_state_;

  switch (g_state_) {
    case MEASURING:
    {
#if 0
      if (viz_offset.getOrigin().length() > 0.5 ||
          viz_offset.getRotation().getAngle() > (M_PI/6.0))
      {
        double ypr[3];
        btMatrix3x3(viz_offset.getRotation()).getEulerYPR(ypr[2], ypr[1], ypr[0]);
        ROS_ERROR("%s: Error, Crazy vision offset (%lf, (%lf, %lf, %lf))!!!.",
                  action_name_.c_str(), viz_offset.getOrigin().length(), ypr[0], ypr[1], ypr[2]);
        g_state_ = MEASURING;
        break;
      }
#endif

      tf::Transform diff = viz_offset * prev_viz_offset_.inverse();
      if (diff.getOrigin().length() > 0.002 ||
          diff.getRotation().getAngle() > 0.035)
      {
        ROS_WARN("Vision estimate of the plug wasn't stable: %lf, %lf",
                 diff.getOrigin().length(), diff.getRotation().getAngle());
        g_state_ = MEASURING;
        break;
      }

      double error = sqrt(pow(viz_offset.getOrigin().y() - viz_offset_desi.getOrigin().y(), 2) +
                          pow(viz_offset.getOrigin().z() - viz_offset_desi.getOrigin().z(), 2));
      ROS_DEBUG("%s: Error = %0.6lf.", action_name_.c_str(), error);
      //if (error < 0.002 && last_standoff_ < MIN_STANDOFF + 0.002)
      if (error < 0.002 && viz_offset.getOrigin().x() > READY_TO_INSERT)
        g_state_ = INSERTING;
      else
        g_state_ = MOVING;

      break;
    }

    case MOVING: {
      g_state_ = MEASURING;
      break;
    }

    case INSERTING:
    {
      tf::Vector3 offset = viz_offset.getOrigin() - viz_offset_desi.getOrigin();
      ROS_DEBUG("%s: Offset: (% 0.3lf, % 0.3lf, % 0.3lf)", action_name_.c_str(), offset.x(), offset.y(), offset.z());
      if (g_started_inserting_ + ros::Duration(10.0) < ros::Time::now())
      {
        //if (offset.x() > SUCCESS_THRESHOLD) // if (in_outlet)
        if (viz_offset.getOrigin().x() > READY_TO_PUSH)  // if (in_outlet)
        {
          g_state_ = FORCING;
        }
        else
        {
          g_state_ = MEASURING;
        }
      }
      break;
    }
    case FORCING:
    {
      if (ros::Time::now() > g_started_forcing_ + ros::Duration(5.0))
      {
        if (viz_offset.getOrigin().x() > FORCING_SUCCESSFUL)
          g_state_ = HOLDING;
        else {
          ROS_ERROR("Forcing occurred, but the plug wasn't in");
          g_state_ = MEASURING;
        }
      }
      break;
    }
    case HOLDING:
    {
      break;
    }
  }

  state_msg.state = prev_state_;
  state_msg.next_state = g_state_;

  if (g_state_ != prev_state_)
  {
    switch (g_state_) {
      case MEASURING:
      {
        ROS_DEBUG("MEASURING");
        if (prev_state_ == INSERTING || prev_state_ == FORCING)
        {
          measure();
        }
        break;
      }
      case MOVING:
      {
        ROS_DEBUG("MOVING");
        move();
        break;
      }
      case INSERTING:
      {
        ROS_DEBUG("INSERTING");
        insert();
        break;
      }
      case FORCING:
      {
        ROS_DEBUG("FORCING");
        force();
        break;
      }
      case HOLDING:
      {
        ROS_DEBUG("HOLDING");
        hold();
        if(battery_level_ > 90.0)
        {
          ROS_DEBUG("%s: success.", action_name_.c_str());
          deactivate(robot_actions::SUCCESS, empty_);
        }
        break;
      }
    }
  }


  last_standoff_ = standoff;
  prev_viz_offset_ = viz_offset;

  node_.publish(action_name_ + "/state", state_msg);

  return;
}

void PlugInAction::measure()
{
  if (!isActive())
    return;

  tff_msg_.mode.linear.x = 3;
  tff_msg_.mode.linear.y = 3;
  tff_msg_.mode.linear.z = 3;
  tff_msg_.mode.angular.x = 3;
  tff_msg_.mode.angular.y = 3;
  tff_msg_.mode.angular.z = 3;
  tff_msg_.value.linear.x = mech_offset_.getOrigin().x() - 0.05;  // backs off 5cm
  tff_msg_.value.linear.y = mech_offset_.getOrigin().y();
  tff_msg_.value.linear.z = mech_offset_.getOrigin().z();
  mech_offset_.getBasis().getEulerZYX(tff_msg_.value.angular.z, tff_msg_.value.angular.y, tff_msg_.value.angular.x);
  node_.publish(arm_controller_ + "/command", tff_msg_);
  g_stopped_forcing_ = ros::Time::now();
  last_standoff_ = 0.05;
  return;
}

void PlugInAction::move()
{
  if (!isActive())
    return;

  tf::Pose target = mech_offset_desi_;
  const double MAX = 0.02;
  tf::Vector3 v = target.getOrigin() - mech_offset_.getOrigin();
  if (v.length() > MAX) {
    target.setOrigin(mech_offset_.getOrigin() + MAX * v / v.length());
  }

  tff_msg_.mode.linear.x = 3;
  tff_msg_.mode.linear.y = 3;
  tff_msg_.mode.linear.z = 3;
  tff_msg_.mode.angular.x = 3;
  tff_msg_.mode.angular.y = 3;
  tff_msg_.mode.angular.z = 3;
  tff_msg_.value.linear.x = target.getOrigin().x();
  tff_msg_.value.linear.y = target.getOrigin().y();
  tff_msg_.value.linear.z = target.getOrigin().z();
  target.getBasis().getEulerZYX(tff_msg_.value.angular.z, tff_msg_.value.angular.y, tff_msg_.value.angular.x);
  ROS_DEBUG("plublishing command to hybrid controller to move");
  node_.publish(arm_controller_ + "/command", tff_msg_);
  return;

}

void PlugInAction::insert()
{
  if (!isActive())
    return;
  g_started_inserting_ = ros::Time::now();

  tff_msg_.mode.linear.x = 2;
  tff_msg_.mode.linear.y = 3;
  tff_msg_.mode.linear.z = 3;
  tff_msg_.mode.angular.x = 3;
  tff_msg_.mode.angular.y = 3;
  tff_msg_.mode.angular.z = 3;
  tff_msg_.value.linear.x = 0.003;
  tff_msg_.value.linear.y = mech_offset_desi_.getOrigin().y();
  tff_msg_.value.linear.z = mech_offset_desi_.getOrigin().z();
  mech_offset_desi_.getBasis().getEulerZYX(tff_msg_.value.angular.z, tff_msg_.value.angular.y, tff_msg_.value.angular.x);
  node_.publish(arm_controller_ + "/command", tff_msg_);

  return;
}

void PlugInAction::force()
{
  if (!isActive())
    return;

  g_started_forcing_ = ros::Time::now();

  tff_msg_.mode.linear.x = 1;
  tff_msg_.mode.linear.y = 3;
  tff_msg_.mode.linear.z = 3;
  tff_msg_.mode.angular.x = 2;
  tff_msg_.mode.angular.y = 2;
  tff_msg_.mode.angular.z = 2;
  tff_msg_.value.linear.x = 50;
  tff_msg_.value.linear.y = mech_offset_.getOrigin().y();
  tff_msg_.value.linear.z = mech_offset_.getOrigin().z();
  tff_msg_.value.angular.x = 0.0;
  tff_msg_.value.angular.y = 0.0;
  tff_msg_.value.angular.z = 0.0;
  tff_msg_.mode.linear.y = 2;
  tff_msg_.mode.linear.z = 2;
  tff_msg_.value.linear.y = 0.0;
  tff_msg_.value.linear.z = 0.0;


  tff_msg_.mode.linear.x = 1;
  tff_msg_.mode.linear.y = 3;
  tff_msg_.mode.linear.z = 3;
  tff_msg_.mode.angular.x = 3;
  tff_msg_.mode.angular.y = 3;
  tff_msg_.mode.angular.z = 3;
  tff_msg_.value.linear.x = 30;
  tff_msg_.value.linear.y = mech_offset_.getOrigin().y();
  tff_msg_.value.linear.z = mech_offset_.getOrigin().z();
  mech_offset_desi_.getBasis().getEulerZYX(tff_msg_.value.angular.z, tff_msg_.value.angular.y, tff_msg_.value.angular.x);

  node_.publish(arm_controller_ + "/command", tff_msg_);

  double base_roll = tff_msg_.value.angular.x;
  double base_pitch = tff_msg_.value.angular.y;
  while (ros::Time::now() - g_started_forcing_ < ros::Duration(5.0))
  {
    tff_msg_.value.angular.x = base_roll + 0.1 * (2.0*drand48()-1.0);
    tff_msg_.value.angular.y = base_pitch + 0.03 * (2.0*drand48()-1.0);
    node_.publish(arm_controller_ + "/command", tff_msg_);
    usleep(10000);
  }

  return;
}

void PlugInAction::hold()
{
  if (!isActive())
    return;

  tff_msg_.mode.linear.x = 1;
  tff_msg_.mode.linear.y = 3;
  tff_msg_.mode.linear.z = 3;
  tff_msg_.mode.angular.x = 3;
  tff_msg_.mode.angular.y = 3;
  tff_msg_.mode.angular.z = 3;
  tff_msg_.value.linear.x = 4;
  tff_msg_.value.linear.y = mech_offset_.getOrigin().y();
  tff_msg_.value.linear.z = mech_offset_.getOrigin().z();
  mech_offset_.getBasis().getEulerZYX(tff_msg_.value.angular.z, tff_msg_.value.angular.y, tff_msg_.value.angular.x);
  node_.publish(arm_controller_ + "/command", tff_msg_);
  return;
}

}
