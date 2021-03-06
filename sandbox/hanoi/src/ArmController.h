#ifndef ARMCONTROLLER_H
#define ARMCONTROLLER_H

#include <vector>
#include <string>

#include <ros/ros.h>
#include <move_arm/ActuateGripperAction.h>
#include <actionlib/client/simple_action_client.h>
#include <actionlib_msgs/GoalStatusArray.h>


#include <planning_environment/monitors/kinematic_model_state_monitor.h>
#include <motion_planning_msgs/KinematicPath.h>

#include <pr2_mechanism_msgs/MechanismState.h>
#include <pr2_mechanism_msgs/JointState.h>

#include "hanoi/ArmStatus.h"
#include "hanoi/ArmCmd.h"

class ArmController
{
  /// \brief Constructor
  public: ArmController(ros::NodeHandle &nh);

  /// \brief Destructor
  public: virtual ~ArmController();

  /// \brief Arm controller command
  public: void CmdCB(const hanoi::ArmCmdConstPtr &msg);

  /// \brief Joint state callback
  public: void MechanismStateCB(const sensor_msgs::JointStateConstPtr &msg);

  /// \brief Get the move arm status message
  public: void MoveArmStatusCB(const actionlib_msgs::GoalStatusArrayConstPtr &msg);

  /// \brief Plan to a goal location
  private: void PlanToGoal(float x, float y, float z, 
                           float qx, float qy, float qz, float qw);

  /// \brief IK to goal
  private: void IKToGoal(float x, float y, float z, 
                         float qx, float qy, float qz, float qw);

  /// \brief Close the gripper
  private: void CloseGripper();

  /// \brief Open the gripper
  private: void OpenGripper();

  /// \brief Return true if the gripper is open
  private: bool GripperIsOpen();

  /// \brief Return true if the gripper is closed
  private: bool GripperIsClosed();

  private: void PrintJoints();

  private: ros::NodeHandle nodeHandle_;
  private: tf::TransformListener tf_;
  private: planning_environment::RobotModels rm_;
  private: planning_environment::KinematicModelStateMonitor *km_;

  private: ros::Subscriber armCmdSubscriber_;
  private: ros::Subscriber jointStateSubscriber_;
  private: ros::Subscriber moveArmStatusSubscriber_;

  private: ros::Publisher statusPublisher_;
  private: ros::Publisher positionArmPublisher_;

  private: actionlib::SimpleActionClient<move_arm::ActuateGripperAction> *gripperAction_;

  private: hanoi::ArmCmd armCmd_;

  private: std::string arm_;
  private: std::vector<std::string> names_;

  private: ros::Time moveArmGoalId_;

  private: sensor_msgs::JointState jointstates_;

  private: ros::Publisher visPublisher_; 

  private: actionlib_msgs::GoalStatusArray moveArmStatus_;

  private: std::string myState_;

  private: float goalx_, goaly_, goalz_, goalqx_, goalqy_, goalqz_, goalqw_;
};

#endif
