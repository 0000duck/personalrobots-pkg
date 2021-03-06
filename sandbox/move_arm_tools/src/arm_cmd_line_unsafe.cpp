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
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *********************************************************************/

/* \author: Ioan Sucan */

#include <ros/ros.h>
#include <actionlib/client/simple_action_client.h>
#include <move_arm/MoveArmAction.h>
#include <move_arm/ActuateGripperAction.h>

#include <mapping_msgs/AttachedObject.h>

#include <planning_environment/monitors/kinematic_model_state_monitor.h>
#include <motion_planning_msgs/KinematicPath.h>
#include <manipulation_msgs/JointTraj.h>
#include <manipulation_srvs/IKService.h>
#include <experimental_controllers/TrajectoryStart.h>
#include <motion_planning_msgs/GetMotionPlan.h>

#include <boost/thread/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <map>

std::string goal_pose_frame = "/base_link";
// std::string planner_service= "/sbpl_planning/plan_kinematic_path";

void printHelp(void)
{
	std::cout << "Basic commands:" << std::endl;
	std::cout << "   - help       : this screen" << std::endl;
	std::cout << "   - list       : prints the list of arm joints" << std::endl;
	std::cout << "   - time       : shows the allowed execution time for arm movement" << std::endl;
	std::cout << "   - time <val> : sets the allowed execution time for arm movement" << std::endl;
	std::cout << "   - quit       : quits this program" << std::endl;
	std::cout << "Arm configuration commands:" << std::endl;
	std::cout << "   - show                    : shows the available configs" << std::endl;
	std::cout << "   - show <config>           : shows the values in <config>" << std::endl;
	std::cout << "   - view                    : shows the current config in the visualizer" << std::endl;
	std::cout << "   - view <config>           : shows <config> in the visualizer" << std::endl;
	std::cout << "   - clear                   : clears all stored configs" << std::endl;
	std::cout << "   - clear <config>          : remove stored <config>" << std::endl;
	std::cout << "   - current                 : show the values of the current configuration" << std::endl;
	std::cout << "   - current <config>        : set <config> to the current position of the arm" << std::endl;
	std::cout << "   - rand <config>           : set <config> to a random position of the arm" << std::endl;
	std::cout << "   - diff <config>           : show the difference from current position of the arm to <config>" << std::endl;
	std::cout << "   - ik <config> <x> <y> <z> : perform IK for the pose (<x>, <y>, <z>, 0, 0, 0, 1) and store it in <config>" << std::endl;
	std::cout << "   - go <config>             : sends the command <config> to the arm" << std::endl;
	std::cout << "   - go <x> <y> <z>          : move the end effector to pose (<x>, <y>, <z>, 0, 0, 0, 1)" << std::endl;
	std::cout << "   - grip <value>            : sends a command to the gripper of the arm" << std::endl;
	std::cout << "   - <config>[<idx>] = <val> : sets the joint specified by <idx> to <val> in <config>" << std::endl;
	std::cout << "   - <config2> = <config1>   : copy <config1> to <config2>" << std::endl;
	std::cout << "   - <config>                : same as show(<config>)" << std::endl;
	std::cout << "Unsafe commands:" << std::endl;
	std::cout << "   - move <x> <y> <z>        : perform IK for the pose (<x>, <y>, <z>, 0, 0, 0, 1) and"; 
	std::cout << "                               send a command straight to the arm controller"<< std::endl;
	std::cout << "   - plan <x> <y> <z>        : plan a path to pose (<x>, <y>, <z>, 0, 0, 0, 1) and then";
	std::cout << "                               send the path straight to the arm controller"<< std::endl;
}

void printJoints(const planning_environment::KinematicModelStateMonitor &km, const std::vector<std::string> &names)
{
    const std::vector<double> &bounds = km.getKinematicModel()->getStateBounds();

    for (unsigned int i = 0 ; i < names.size(); ++i)
    {
	int idx = km.getKinematicModel()->getJoint(names[i])->stateIndex;
	std::cout << "  " << i << " = " << names[i] << "  [" << bounds[idx * 2] << ", " << bounds[idx * 2 + 1] << "]" << std::endl;
    }
}

void printPose(const btTransform &p)
{
	std::cout << "  -position [x, y, z]    = [" << p.getOrigin().x() << ", " << p.getOrigin().y() << ", " << p.getOrigin().z() << "]" << std::endl;
	btQuaternion q = p.getRotation();
	std::cout << "  -rotation [x, y, z, w] = [" << q.x() << ", " << q.y() << ", " << q.z() << ", " << q.w() << "]" << std::endl;
}

void goalToState(const move_arm::MoveArmGoal &goal, planning_models::KinematicState &sp)
{
	for (unsigned int i = 0 ; i < goal.goal_constraints.joint_constraint.size() ; ++i)
	{
		sp.setParamsJoint(&goal.goal_constraints.joint_constraint[i].value[0],
											 goal.goal_constraints.joint_constraint[i].joint_name);
	}
}

btTransform effPosition(const planning_environment::KinematicModelStateMonitor &km, const move_arm::MoveArmGoal &goal)
{
	planning_models::KinematicState sp(*km.getRobotState());
	goalToState(goal, sp);
	km.getKinematicModel()->computeTransforms(sp.getParams());
	return km.getKinematicModel()->getJoint(goal.goal_constraints.joint_constraint.back().joint_name)->after->globalTrans;
}

void printConfig(const move_arm::MoveArmGoal &goal)
{
	for (unsigned int i = 0 ; i < goal.goal_constraints.joint_constraint.size(); ++i)
		std::cout << "  " << goal.goal_constraints.joint_constraint[i].joint_name << " = " << goal.goal_constraints.joint_constraint[i].value[0] << std::endl;
}

void printConfigs(const std::map<std::string, move_arm::MoveArmGoal> &goals)
{
	for (std::map<std::string, move_arm::MoveArmGoal>::const_iterator it = goals.begin() ; it != goals.end() ; ++it)
		std::cout << "  " << it->first << std::endl;
}

void setupGoal(const std::vector<std::string> &names, move_arm::MoveArmGoal &goal)
{
	goal.goal_constraints.joint_constraint.resize(names.size());
	for (unsigned int i = 0 ; i < goal.goal_constraints.joint_constraint.size(); ++i)
	{
		goal.goal_constraints.joint_constraint[i].header.stamp = ros::Time::now();
		goal.goal_constraints.joint_constraint[i].header.frame_id = "/base_link";
		goal.goal_constraints.joint_constraint[i].joint_name = names[i];
		goal.goal_constraints.joint_constraint[i].value.resize(1);
		goal.goal_constraints.joint_constraint[i].tolerance_above.resize(1);
		goal.goal_constraints.joint_constraint[i].tolerance_below.resize(1);
		goal.goal_constraints.joint_constraint[i].value[0] = 0.0;
		goal.goal_constraints.joint_constraint[i].tolerance_below[0] = 0.0;
		goal.goal_constraints.joint_constraint[i].tolerance_above[0] = 0.0;
	}
    
	goal.contacts.resize(1);
	goal.contacts[0].links.push_back("r_gripper_l_finger_link");
	goal.contacts[0].links.push_back("r_gripper_r_finger_link");
	goal.contacts[0].links.push_back("r_gripper_l_finger_tip_link");
	goal.contacts[0].links.push_back("r_gripper_r_finger_tip_link");
    //    goal.contacts[0].links.push_back("r_gripper_palm_link");
	goal.contacts[0].depth = 0.04;
	goal.contacts[0].bound.type = mapping_msgs::Object::SPHERE;
	goal.contacts[0].bound.dimensions.push_back(0.5);
    
	goal.contacts[0].pose.header.stamp = ros::Time::now();
	goal.contacts[0].pose.header.frame_id = "/base_link";
	goal.contacts[0].pose.pose.position.x = 1;
	goal.contacts[0].pose.pose.position.y = 0;
	goal.contacts[0].pose.pose.position.z = 0.5;
    
	goal.contacts[0].pose.pose.orientation.x = 0;
	goal.contacts[0].pose.pose.orientation.y = 0;
	goal.contacts[0].pose.pose.orientation.z = 0;
	goal.contacts[0].pose.pose.orientation.w = 1;
}

void setupGoalEEf(const std::string &link, const std::vector<double> &pz, move_arm::MoveArmGoal &goal)
{
	goal.goal_constraints.pose_constraint.resize(1);
	goal.goal_constraints.pose_constraint[0].type = motion_planning_msgs::PoseConstraint::POSITION_X + motion_planning_msgs::PoseConstraint::POSITION_Y + motion_planning_msgs::PoseConstraint::POSITION_Z +
			+ motion_planning_msgs::PoseConstraint::ORIENTATION_R + motion_planning_msgs::PoseConstraint::ORIENTATION_P + motion_planning_msgs::PoseConstraint::ORIENTATION_Y;
	goal.goal_constraints.pose_constraint[0].link_name = link;
	goal.goal_constraints.pose_constraint[0].pose.header.stamp = ros::Time::now();
	goal.goal_constraints.pose_constraint[0].pose.header.frame_id = "/base_link";
	goal.goal_constraints.pose_constraint[0].pose.pose.position.x = pz[0];
	goal.goal_constraints.pose_constraint[0].pose.pose.position.y = pz[1];
	goal.goal_constraints.pose_constraint[0].pose.pose.position.z = pz[2];
    
	goal.goal_constraints.pose_constraint[0].pose.pose.orientation.x = pz[3];
	goal.goal_constraints.pose_constraint[0].pose.pose.orientation.y = pz[4];
	goal.goal_constraints.pose_constraint[0].pose.pose.orientation.z = pz[5];
	goal.goal_constraints.pose_constraint[0].pose.pose.orientation.w = pz[6];
    
	goal.goal_constraints.pose_constraint[0].position_tolerance_above.x = 0.04;
	goal.goal_constraints.pose_constraint[0].position_tolerance_above.y = 0.04;
	goal.goal_constraints.pose_constraint[0].position_tolerance_above.z = 0.04;
	goal.goal_constraints.pose_constraint[0].position_tolerance_below.x = 0.04;
	goal.goal_constraints.pose_constraint[0].position_tolerance_below.y = 0.04;
	goal.goal_constraints.pose_constraint[0].position_tolerance_below.z = 0.04;
    
	goal.goal_constraints.pose_constraint[0].orientation_tolerance_above.x = 0.35;
	goal.goal_constraints.pose_constraint[0].orientation_tolerance_above.y = 0.35;
	goal.goal_constraints.pose_constraint[0].orientation_tolerance_above.z = 0.35;
	goal.goal_constraints.pose_constraint[0].orientation_tolerance_below.x = 0.35;
	goal.goal_constraints.pose_constraint[0].orientation_tolerance_below.y = 0.35;
	goal.goal_constraints.pose_constraint[0].orientation_tolerance_below.z = 0.35;
    
	goal.goal_constraints.pose_constraint[0].orientation_importance = 0.2;
/*    
	goal.contacts.resize(2);
	goal.contacts[0].links.push_back("r_gripper_l_finger_link");
	goal.contacts[0].links.push_back("r_gripper_r_finger_link");
	goal.contacts[0].links.push_back("r_gripper_l_finger_tip_link");
	goal.contacts[0].links.push_back("r_gripper_r_finger_tip_link");
    //    goal.contacts[0].links.push_back("r_gripper_palm_link");
	goal.contacts[0].depth = 0.04;
	goal.contacts[0].bound.type = mapping_msgs::Object::SPHERE;
	goal.contacts[0].bound.dimensions.push_back(0.3);
	goal.contacts[0].pose = goal.goal_constraints.pose_constraint[0].pose;

	goal.contacts[1].links.push_back("r_gripper_palm_link");
	goal.contacts[1].depth = 0.01;
	goal.contacts[1].bound.type = mapping_msgs::Object::SPHERE;
	goal.contacts[1].bound.dimensions.push_back(0.2);
	goal.contacts[1].pose = goal.goal_constraints.pose_constraint[0].pose;*/
}

void setConfig(const planning_models::KinematicState *_sp, const std::vector<std::string> &names, move_arm::MoveArmGoal &goal)
{
	setupGoal(names, goal);
	planning_models::KinematicState sp(*_sp);
	sp.enforceBounds();
	for (unsigned int i = 0 ; i < names.size() ; ++i)
	{
		goal.goal_constraints.joint_constraint[i].value[0] =
				sp.getParamsJoint(goal.goal_constraints.joint_constraint[i].joint_name)[0];
	}
}

void getIK(bool r, ros::NodeHandle &nh, planning_environment::KinematicModelStateMonitor &km, move_arm::MoveArmGoal &goal, planning_models::KinematicState &sp,
					 const std::vector<std::string> &names, double x, double y, double z)
{
	ros::ServiceClient client = nh.serviceClient<manipulation_srvs::IKService>("arm_ik");
	manipulation_srvs::IKService::Request request;
	manipulation_srvs::IKService::Response response;
	request.data.pose_stamped.header.stamp = ros::Time::now();
	request.data.pose_stamped.header.frame_id = km.getFrameId();
	request.data.pose_stamped.pose.position.x = x;
	request.data.pose_stamped.pose.position.y = y;
	request.data.pose_stamped.pose.position.z = z;
	request.data.pose_stamped.pose.orientation.x = 0;
	request.data.pose_stamped.pose.orientation.y = 0;
	request.data.pose_stamped.pose.orientation.z = 0;
	request.data.pose_stamped.pose.orientation.w = 1;
	request.data.joint_names = names;
    
	planning_models::KinematicState rs(*km.getRobotState());
	if (r)
		rs.randomState();
    
	for(unsigned int i = 0; i < names.size() ; ++i)
	{
		const double *params = rs.getParamsJoint(names[i]);
		const unsigned int u = km.getKinematicModel()->getJoint(names[i])->usedParams;
		for (unsigned int j = 0 ; j < u ; ++j)
			request.data.positions.push_back(params[j]);
	}
	if (client.call(request, response))
	{
		sp = *km.getRobotState();
		unsigned int n = 0;
		for (unsigned int i = 0 ; i < names.size() ; ++i)
		{
			unsigned int u = km.getKinematicModel()->getJoint(names[i])->usedParams;
			for (unsigned int j = 0 ; j < u ; ++j)
			{
				std::vector<double> params(response.solution.begin() + n, response.solution.begin() + n + u);
				sp.setParamsJoint(params, names[i]);
				std::cout << names[i] << " = " << params[0] << std::endl;
			}
			n += u;
		}
		setConfig(&sp, names, goal);
		std::cout << "Success!" << std::endl;
	}
	else
		std::cerr << "IK Failed" << std::endl;
}

void goToIK(ros::NodeHandle &nh,  planning_environment::KinematicModelStateMonitor &km, const std::vector<std::string> &names, double x, double y, double z)
{
	ros::ServiceClient client = nh.serviceClient<manipulation_srvs::IKService>("arm_ik");
	ros::ServiceClient arm_ctrl = nh.serviceClient<experimental_controllers::TrajectoryStart>("/r_arm_joint_waypoint_controller/TrajectoryStart", true);
	manipulation_srvs::IKService::Request request;
	manipulation_srvs::IKService::Response response;
	request.data.pose_stamped.header.stamp = ros::Time::now();
	request.data.pose_stamped.header.frame_id = km.getFrameId();
	request.data.pose_stamped.pose.position.x = x;
	request.data.pose_stamped.pose.position.y = y;
	request.data.pose_stamped.pose.position.z = z;
	request.data.pose_stamped.pose.orientation.x = 0;
	request.data.pose_stamped.pose.orientation.y = 0;
	request.data.pose_stamped.pose.orientation.z = 0;
	request.data.pose_stamped.pose.orientation.w = 1;
	request.data.joint_names = names;
	manipulation_msgs::JointTraj traj;
	experimental_controllers::TrajectoryStart::Request  send_traj_start_req;
	experimental_controllers::TrajectoryStart::Response send_traj_start_res;
	
	planning_models::KinematicState robot_state(*km.getRobotState());
	for(unsigned int i = 0; i < names.size() ; ++i)
	{
		const unsigned int u = km.getKinematicModel()->getJoint(names[i])->usedParams;
		for (unsigned int j = 0 ; j < u ; ++j)
			request.data.positions.push_back(*(robot_state.getParamsJoint(names[i])));
	}
	
	if (client.call(request, response))
	{
		ROS_INFO("Received an IK solution");
		traj.names = names;
		traj.points.resize(2);
		traj.points[0].time = 0.2;
		traj.points[1].time = 0.8;
					
		for(unsigned  k = 0; k < names.size(); ++k)
		{
			traj.points[0].positions.push_back(request.data.positions[k]);
			traj.points[1].positions.push_back(response.solution[k]);
		}
			
		send_traj_start_req.traj = traj;	    
		send_traj_start_req.hastiming = 0;
		send_traj_start_req.requesttiming = 0;
	    
		if (arm_ctrl.call(send_traj_start_req, send_traj_start_res))
		{
			int trajectoryId = send_traj_start_res.trajectoryid;
			if (trajectoryId < 0)
				ROS_ERROR("Invalid trajectory id: %d", trajectoryId);
			else
				ROS_INFO("Sent trajectory %d to controller", trajectoryId);
		}
		else
			ROS_ERROR("Unable to start trajectory controller");
		
		std::cout << "Success!" << std::endl;
	}
	else
		std::cerr << "IK Failed" << std::endl;
}

int goToPlan(ros::NodeHandle &nh, planning_environment::KinematicModelStateMonitor &km, move_arm::MoveArmGoal &goal, const std::vector<std::string> &names, std::string &planner_service)
{
	ros::ServiceClient arm_ctrl = nh.serviceClient<experimental_controllers::TrajectoryStart>("/r_arm_joint_waypoint_controller/TrajectoryStart", true);
	ros::ServiceClient clientPlan = nh.serviceClient<motion_planning_msgs::GetMotionPlan>(planner_service, true);
	
	motion_planning_msgs::GetMotionPlan::Request request;
	motion_planning_msgs::GetMotionPlan::Response response;
	
	manipulation_msgs::JointTraj traj;
	experimental_controllers::TrajectoryStart::Request  send_traj_start_req;
	experimental_controllers::TrajectoryStart::Response send_traj_start_res;
	
	planning_models::KinematicState robot_state(*km.getRobotState());
	
	// fill start state
	request.set_start_state_size(names.size());
	request.start_state[0].header.seq = 5;
	request.start_state[0].header.stamp = ros::Time::now();
	request.start_state[0].header.frame_id = goal_pose_frame;
	for(unsigned int i = 0; i < names.size() ; ++i)
	{
		request.start_state[i].joint_name = names[i];
		request.start_state[i].header = request.start_state[0].header;
		const unsigned int u = km.getKinematicModel()->getJoint(names[i])->usedParams;
		
		for (unsigned int j = 0 ; j < u; ++j)
			request.start_state[i].value.push_back(*(robot_state.getParamsJoint(names[i])));
	}
	
	// fill goal & path constraints
	request.goal_constraints = goal.goal_constraints;
	request.path_constraints = goal.path_constraints;


	if (clientPlan.call(request, response))
	{
		ROS_DEBUG("Received a plan with %i waypoints", response.path.states.size());
		traj.names = names;
		traj.points.resize(response.path.states.size());
				
		for(unsigned i = 0; i < traj.points.size(); ++i)
		{
			traj.points[i].time = response.path.times[i];
			traj.points[i].set_positions_size(names.size());
			for(unsigned  k = 0; k < names.size(); ++k)
			{
				traj.points[i].positions[k] = response.path.states[i].vals[k];
			}
		}
			
		send_traj_start_req.traj = traj; 
		send_traj_start_req.hastiming = 1;
		send_traj_start_req.requesttiming = 0;

		if (arm_ctrl.call(send_traj_start_req, send_traj_start_res))
		{
			ROS_INFO("Sent Trajectory");
			int trajectoryId = send_traj_start_res.trajectoryid;
			if (trajectoryId < 0)
				ROS_ERROR("Invalid trajectory id: %d", trajectoryId);
			else
				ROS_INFO("Sent trajectory %d to controller", trajectoryId);
		}
		else
			ROS_ERROR("Unable to start trajectory controller");
		
		sleep(6);
		std::cout << "Success!" << std::endl;
	}
	else
	{
		std::cerr << "Planning Failed" << std::endl;
		return 0;
	}
	
	return 1;
}

void diffConfig(const planning_environment::KinematicModelStateMonitor &km, move_arm::MoveArmGoal &goal)
{
	std::vector<std::string> names;
	for (unsigned int i = 0 ; i < goal.goal_constraints.joint_constraint.size(); ++i)
	{
		std::cout << "  " << goal.goal_constraints.joint_constraint[i].joint_name << " = "
				<< goal.goal_constraints.joint_constraint[i].value[0] - km.getRobotState()->getParamsJoint(goal.goal_constraints.joint_constraint[i].joint_name)[0]
				<< std::endl;
		names.push_back(goal.goal_constraints.joint_constraint[i].joint_name);
	}
    
	btTransform pose1 = effPosition(km, goal);
	move_arm::MoveArmGoal temp;
	setConfig(km.getRobotState(), names, temp);
	btTransform pose2 = effPosition(km, temp);
	std::cout << std::endl;
	double dist = pose1.getOrigin().distance(pose2.getOrigin());
	std::cout << "  -position distance: " << dist << std::endl;
	double angle = pose1.getRotation().angle(pose2.getRotation());
	std::cout << "  -rotation distance: " << angle << std::endl;
}

void viewState(ros::Publisher &view, const planning_environment::KinematicModelStateMonitor &km, const planning_models::KinematicState &st)
{
	motion_planning_msgs::KinematicPath kp;
    
	kp.header.frame_id = km.getFrameId();
	kp.header.stamp = km.lastJointStateUpdate();
    
    // fill in start state with current one
	std::vector<const planning_models::KinematicModel::Joint*> joints;
	km.getKinematicModel()->getJoints(joints);
    
	kp.start_state.resize(joints.size());
	for (unsigned int i = 0 ; i < joints.size() ; ++i)
	{
		kp.start_state[i].header.frame_id = km.getFrameId();
		kp.start_state[i].header.stamp = km.lastJointStateUpdate();
		kp.start_state[i].joint_name = joints[i]->name;
		st.copyParamsJoint(kp.start_state[i].value, joints[i]->name);
	}
	view.publish(kp);
}

void setConfigJoint(const unsigned int pos, const double value, move_arm::MoveArmGoal &goal)
{
	goal.goal_constraints.joint_constraint[pos].value[0] = value;
}

void spinThread(void)
{
	ros::spin();
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "cmd_line_move_arm", ros::init_options::NoSigintHandler | ros::init_options::AnonymousName);
    
	std::string arm = "r";
	if (argc >= 2)
		if (argv[1][0] == 'l')
			arm = "l";
    
	ros::NodeHandle nh;
	std::string planner_service;
	
	std::string group = arm == "r" ? "right_arm" : "left_arm";
	actionlib::SimpleActionClient<move_arm::MoveArmAction> move_arm(nh, "move_" + group);
	actionlib::SimpleActionClient<move_arm::ActuateGripperAction> gripper(nh, "actuate_gripper_" + group);
	ros::Publisher view = nh.advertise<motion_planning_msgs::KinematicPath>("executing_kinematic_path", 1);
	ros::Publisher pubAttach = nh.advertise<mapping_msgs::AttachedObject>("attach_object", 1);
	nh.param<std::string>("~planner_service",planner_service,"sbpl_planning/plan_kinematic_path");
    
	std::map<std::string, move_arm::MoveArmGoal> goals;
    
	std::vector<std::string> names(7);
	names[0] = arm + "_shoulder_pan_joint";
	names[1] = arm + "_shoulder_lift_joint";
	names[2] = arm + "_upper_arm_roll_joint";
	names[3] = arm + "_elbow_flex_joint";
	names[4] = arm + "_forearm_roll_joint";
	names[5] = arm + "_wrist_flex_joint";
	names[6] = arm + "_wrist_roll_joint";
    
    
	planning_environment::RobotModels rm("robot_description");
	if (!rm.loadedModels())
		return 0;
    
	boost::thread th(&spinThread);
	tf::TransformListener tf;
	planning_environment::KinematicModelStateMonitor km(&rm, &tf);
	km.waitForState();
    
	std::cout << std::endl << std::endl << "Using joints:" << std::endl;
	printJoints(km, names);
	std::cout << std::endl;
	double allowed_time = 50.0;
    
	while (nh.ok() && std::cin.good() && !std::cin.eof())
	{
		std::cout << "command> ";
		std::string cmd;
		std::getline(std::cin, cmd);
		boost::trim(cmd);
		ros::spinOnce();

		if (!nh.ok())
			break;

		if (cmd == "")
			continue;

		if (cmd == "help")
			printHelp();
		else
			if (cmd == "list")
				printJoints(km, names);
		else
			if (cmd == "quit")
				break;
		else
			if (cmd == "show")
				printConfigs(goals);
		else
			if (cmd == "current")
		{
			move_arm::MoveArmGoal temp;
			setConfig(km.getRobotState(), names, temp);
			printConfig(temp);
			std::cout << std::endl;
			btTransform p = effPosition(km, temp);
			printPose(p);
		}
		else
			if (cmd == "att0")
		{
			mapping_msgs::AttachedObject ao;
			ao.header.frame_id = "r_wrist_roll_link";
			ao.header.stamp = ros::Time::now();
			ao.link_name = "r_wrist_roll_link";
			pubAttach.publish(ao);
		}
		else
			if (cmd == "att1")
		{
			mapping_msgs::AttachedObject ao;
			ao.header.frame_id = "r_wrist_roll_link";
			ao.header.stamp = ros::Time::now();
			ao.link_name = "r_wrist_roll_link";
	    
			mapping_msgs::Object object;
			object.type = mapping_msgs::Object::CYLINDER;
			object.dimensions.push_back(0.02); // 4 cm diam
			object.dimensions.push_back(1.3); // 48 inch
	    
	    // identity transform should place the object in the center
			geometry_msgs::Pose pose;
			pose.position.x = 0.16;
			pose.position.y = 0;
			pose.position.z = 0;
	    
			pose.orientation.x = 0;
			pose.orientation.y = 0;
			pose.orientation.z = 0;
			pose.orientation.w = 1;
	    
			ao.objects.push_back(object);
			ao.poses.push_back(pose);
	    
			ao.touch_links.push_back("r_gripper_l_finger_link");
			ao.touch_links.push_back("r_gripper_r_finger_link");
			ao.touch_links.push_back("r_gripper_l_finger_tip_link");
			ao.touch_links.push_back("r_gripper_r_finger_tip_link");
			ao.touch_links.push_back("r_gripper_palm_link");
	    
			pubAttach.publish(ao);
		}
		else
			if (cmd == "time")
		{
			std::cout << "  Allowed execution time is " << allowed_time << " seconds" << std::endl;
		}
		else
			if (cmd.substr(0, 5) == "time " && cmd.length() > 5)
		{
			std::stringstream ss(cmd.substr(5));
			if (ss.good() && !ss.eof())
				ss >> allowed_time;
			else
				std::cerr << "Unable to parse time value " << ss.str() << std::endl;
			if (allowed_time <= 0.0)
				allowed_time = 10.0;
			std::cout << "  Allowed execution time is " << allowed_time << " seconds" << std::endl;
		}
		else
			if (cmd == "clear")
				goals.clear();
		else
			if (cmd.length() > 6 && cmd.substr(0, 6) == "clear ")
		{
			std::string config = cmd.substr(6);
			boost::trim(config);
			if (goals.find(config) == goals.end())
				std::cout << "Configuration '" << config << "' not found" << std::endl;
			else
				goals.erase(config);
		}
		else
			if (cmd == "view")
		{
			planning_models::KinematicState st(*km.getRobotState());
			viewState(view, km, st);
		}
		else
			if (cmd.length() > 4 && cmd.substr(0, 4) == "fwd ")
		{
			std::stringstream ss(cmd.substr(4));
			double fwd = 0.0;
			if (ss.good() && !ss.eof())
			{
				ss >> fwd;
		
				manipulation_msgs::JointTraj traj;	    
				traj.names = names;
				traj.points.resize(2);
				traj.points[0].time = 0;
				traj.points[1].time = 3;
		
				km.getRobotState()->copyParamsJoints(traj.points[0].positions, names);


				move_arm::MoveArmGoal temp;
				setConfig(km.getRobotState(), names, temp);
				btTransform p = effPosition(km, temp);
		
				planning_models::KinematicState sp(*km.getRobotState());
				getIK(false, nh, km, temp, sp, names, p.getOrigin().x() + fwd, p.getOrigin().y(), p.getOrigin().z());
		
				sp.copyParamsJoints(traj.points[1].positions, names);

				std::cout << "Executing forward path for " << fwd << "m" << std::endl;
				ros::ServiceClient clientStart = nh.serviceClient<experimental_controllers::TrajectoryStart>("/r_arm_joint_waypoint_controller/TrajectoryStart");
		
				experimental_controllers::TrajectoryStart::Request  send_traj_start_req;
				experimental_controllers::TrajectoryStart::Response send_traj_start_res;
				send_traj_start_req.traj = traj;
				send_traj_start_req.hastiming = 0;
				send_traj_start_req.requesttiming = 0;
				if (clientStart.call(send_traj_start_req, send_traj_start_res))
					std::cout << "Success!" << std::endl;
				else
					std::cout << "Failure!" << std::endl;
			}
			else
				std::cerr << "Unable to parse value " << ss.str() << std::endl;
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "view ")
		{
			std::string config = cmd.substr(5);
			boost::trim(config);
			if (goals.find(config) == goals.end())
				std::cout << "Configuration '" << config << "' not found" << std::endl;
			else
			{
				planning_models::KinematicState st(*km.getRobotState());
				goalToState(goals[config], st);
				viewState(view, km, st);
			}
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "diff ")
		{
			std::string config = cmd.substr(5);
			boost::trim(config);
			if (goals.find(config) == goals.end())
				std::cout << "Configuration '" << config << "' not found" << std::endl;
			else
				diffConfig(km, goals[config]);
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "show ")
		{
			std::string config = cmd.substr(5);
			boost::trim(config);
			if (goals.find(config) == goals.end())
				std::cout << "Configuration '" << config << "' not found" << std::endl;
			else
			{
				printConfig(goals[config]);
				std::cout << std::endl;
				btTransform p = effPosition(km, goals[config]);
				printPose(p);
			}
		}
		else
			if (cmd.length() > 8 && cmd.substr(0, 8) == "current ")
		{
			std::string config = cmd.substr(8);
			boost::trim(config);
			setConfig(km.getRobotState(), names, goals[config]);
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "rand ")
		{
			std::string config = cmd.substr(5);
			boost::trim(config);
			planning_models::KinematicState *sp = new planning_models::KinematicState(km.getKinematicModel());
			sp->randomState();
			setConfig(sp, names, goals[config]);
			delete sp;
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "move ")
		{
			std::stringstream ss(cmd.substr(5));
			double x, y, z;
			bool err = true;
	
			if (ss.good() && !ss.eof())
			{
				ss >> x;
				if (ss.good() && !ss.eof())
				{
					ss >> y;
					if (ss.good() && !ss.eof())
					{
						ss >> z;
							
						goToIK(nh, km, names, x, y, z);
						err = false;
					}
				}
			}
			if (err)
				std::cout << "goToIK didn't work" << std::endl;
		}
		else
			if (cmd.length() > 3 && cmd.substr(0, 3) == "ik ")
		{
			std::stringstream ss(cmd.substr(3));
			std::string config;

			if (ss.good() && !ss.eof())
				ss >> config;

			if (config.empty())
				std::cerr << "Configuration name required. See 'help'" << std::endl;
			else
			{
				double x, y, z;
				bool err = true;

				if (ss.good() && !ss.eof())
				{
					ss >> x;
					if (ss.good() && !ss.eof())
					{
						ss >> y;
						if (ss.good() && !ss.eof())
						{
							ss >> z;
							err = false;
							std::cout << "Performing IK to " << x << ", " << y << ", " << z << ", 0, 0, 0, 1..." << std::endl;
							planning_models::KinematicState sp(*km.getRobotState());
							getIK(true, nh, km, goals[config], sp, names, x, y, z);
						}
					}
				}
				if (err)
					std::cerr << "Unable to parse IK position" << std::endl;
			}
		}
		else
			if (cmd.length() > 2 && cmd.substr(0, 2) == "s ")
		{
			std::string state = cmd.substr(2);
			boost::trim(state);
			std::string fnm = getenv("HOME") + ("/states/" + state);
			std::ifstream in(fnm.c_str());
	    
			planning_models::KinematicState sp(*km.getRobotState());
			std::vector<double> params;
			while (in.good() && !in.eof())
			{
				double v;
				in >> v;
				params.push_back(v);
			}
			in.close();
			params.resize(7);
			sp.setParamsGroup(params, group);
			move_arm::MoveArmGoal g;
			setConfig(&sp, names, g);
	    
			printConfig(g);
			std::cout << std::endl;
			btTransform p = effPosition(km, g);
			printPose(p);
	    
			std::cout << "Moving to saved state " << state << "..." << std::endl;

			bool finished_within_time;
			move_arm.sendGoal(g);
			finished_within_time = move_arm.waitForGoalToFinish(ros::Duration(allowed_time));
	    
			if (!finished_within_time)
			{
				move_arm.cancelGoal();
				std::cerr << "Timed out achieving goal" << std::endl;
			}
			else
				std::cout << "Final state is " << move_arm.getTerminalState().toString() << std::endl;
		}
		else
			if (cmd.length() > 2 && cmd.substr(0, 2) == "p ")
		{
			std::string tmp = cmd.substr(2);
			boost::trim(tmp);
			std::stringstream ss(tmp);
			std::string path;
			ss >> path;
			double time;
			ss >> time;
			if (time <= 0.001)
				std::cout << "Time is to small" << std::endl;
			else
			{
				std::string fnm = getenv("HOME") + ("/paths/" + path);
				std::ifstream in(fnm.c_str());
		
				manipulation_msgs::JointTraj traj;	    
				traj.names = names;
				while (in.good() && !in.eof())
				{
					std::vector<double> params;
					for (int i = 0 ; i < 7 && in.good() && !in.eof(); ++i)
					{
						double v;
						in >> v;
						params.push_back(v);
						std::cout << v << " ";
					}
					std::cout << std::endl;
					if (params.size() != 7)
						break;
		    
					unsigned int pz = traj.points.size();		
					traj.points.resize(pz + 1);
					traj.points[pz].positions = params;
					traj.points[pz].time = time * pz;
				}
				in.close();
		
				std::cout << "Executing path " << fnm << " with " << traj.points.size() << " points at " << time << " seconds apart  ..." << std::endl;
				ros::ServiceClient clientStart = nh.serviceClient<experimental_controllers::TrajectoryStart>("/r_arm_joint_waypoint_controller/TrajectoryStart");
		
				experimental_controllers::TrajectoryStart::Request  send_traj_start_req;
				experimental_controllers::TrajectoryStart::Response send_traj_start_res;
				send_traj_start_req.traj = traj;
				send_traj_start_req.hastiming = 0;
				send_traj_start_req.requesttiming = 0;
				if (clientStart.call(send_traj_start_req, send_traj_start_res))
					std::cout << "Success!" << std::endl;
				else
					std::cout << "Failure!" << std::endl;
			}
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "plan ")
		{
			std::string config = cmd.substr(5);
			boost::trim(config);
			if (goals.find(config) == goals.end())
			{
				std::stringstream ss(config);
				std::vector<double> nrs;
				while (ss.good() && !ss.eof())
				{
					double value;
					ss >> value;
					nrs.push_back(value);
				}
		
				bool err = true;
				if (nrs.size() == 3)
				{
					nrs.push_back(0);
					nrs.push_back(0);
					nrs.push_back(0);
					nrs.push_back(1);
				}
		
				if (nrs.size() == 7)
				{
					err = false;
		    
					std::string link = km.getKinematicModel()->getJoint(names.back())->after->name;
					std::cout << "Moving " << link << " to " << nrs[0] << ", " << nrs[1] << ", " << nrs[2] << ", " <<
							nrs[3] << ", " << nrs[4] << ", " << nrs[5] << ", " << nrs[6] << "..." << std::endl;
					move_arm::MoveArmGoal g;
					setupGoalEEf(link, nrs, g);
		    
					if(!goToPlan(nh, km, g, names, planner_service))
					{
						err = true;
						std::cout << "Configuration '" << config << "' not found" << std::endl;
					}
				}
			}
			else
			{
				std::cout << "Moving to " << config << "..." << std::endl;
				if(!goToPlan(nh, km, goals[config], names, planner_service))
					std::cout << "No path returned" << std::endl;
				else
					std::cout << "Final state is " << move_arm.getTerminalState().toString() << std::endl;
			}
		}
		else
			if (cmd.length() > 3 && cmd.substr(0, 3) == "go ")
		{
			std::string config = cmd.substr(3);
			boost::trim(config);
			if (goals.find(config) == goals.end())
			{
				std::stringstream ss(config);
				std::vector<double> nrs;
				while (ss.good() && !ss.eof())
				{
					double value;
					ss >> value;
					nrs.push_back(value);
				}
		
				bool err = true;
				if (nrs.size() == 3)
				{
					nrs.push_back(0);
					nrs.push_back(0);
					nrs.push_back(0);
					nrs.push_back(1);
				}
		
				if (nrs.size() == 7)
				{
					err = false;
		    
					std::string link = km.getKinematicModel()->getJoint(names.back())->after->name;
					std::cout << "Moving " << link << " to " << nrs[0] << ", " << nrs[1] << ", " << nrs[2] << ", " <<
							nrs[3] << ", " << nrs[4] << ", " << nrs[5] << ", " << nrs[6] << "..." << std::endl;
					move_arm::MoveArmGoal g;
					setupGoalEEf(link, nrs, g);
		    
		    
					bool finished_within_time;
					move_arm.sendGoal(g);
					finished_within_time = move_arm.waitForGoalToFinish(ros::Duration(allowed_time));
		    
					if (!finished_within_time)
					{
						move_arm.cancelGoal();
						std::cerr << "Timed out achieving goal" << std::endl;
					}
					else
						std::cout << "Final state is " << move_arm.getTerminalState().toString() << std::endl;
				}
				if (err)
					std::cout << "Configuration '" << config << "' not found" << std::endl;
			}
	    
			else
			{
				std::cout << "Moving to " << config << "..." << std::endl;
		
				bool finished_within_time;
				move_arm.sendGoal(goals[config]);
				finished_within_time = move_arm.waitForGoalToFinish(ros::Duration(allowed_time));
		
				if (!finished_within_time)
				{
					move_arm.cancelGoal();
					std::cerr << "Timed out achieving goal" << std::endl;
				}
				else
					std::cout << "Final state is " << move_arm.getTerminalState().toString() << std::endl;
			}
		}
		else
			if (cmd.length() > 5 && cmd.find_first_of("[") != std::string::npos)
		{
			std::size_t p1 = cmd.find_first_of("[");
			std::size_t p2 = cmd.find_last_of("]");
			std::size_t p3 = cmd.find_first_of("=");
			if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos && p2 > p1 + 1 && p3 > p2)
			{
				std::string config = cmd.substr(0, p1);
				boost::trim(config);
				if (goals.find(config) == goals.end())
					std::cout << "Configuration '" << config << "' not found" << std::endl;
				else
				{
					std::stringstream ss(cmd.substr(p1 + 1, p2 - p1 - 1));
					unsigned int joint = names.size();
					if (ss.good() && !ss.eof())
						ss >> joint;
					if (joint >= names.size())
						std::cerr << "Joint index out of bounds" << std::endl;
					else
					{
						std::stringstream ss(cmd.substr(p3 + 1));
						if (ss.good() && !ss.eof())
						{
							double value;
							ss >> value;
							setConfigJoint(joint, value, goals[config]);
						}
						else
							std::cerr << "Cannot parse value: '" << ss.str() << "'" << std::endl;
					}
				}
			}
			else
				std::cerr << "Incorrect command format" << std::endl;
		}
		else
			if (cmd.length() > 2 && cmd.find_first_of("=") != std::string::npos)
		{
			std::size_t p = cmd.find_first_of("=");
			std::string c1 = cmd.substr(0, p);
			std::string c2 = cmd.substr(p + 1);
			boost::trim(c1);
			boost::trim(c2);
			if (goals.find(c2) != goals.end())
				goals[c1] = goals[c2];
			else
				std::cout << "Configuration '" << c2 << "' not found" << std::endl;
		}
		else
			if (cmd.length() > 5 && cmd.substr(0, 5) == "grip ")
		{
			std::stringstream ss(cmd.substr(5));
			if (ss.good() && !ss.eof())
			{
				move_arm::ActuateGripperGoal g;
				ss >> g.data;
		
				gripper.sendGoal(g);
				bool finished_before_timeout = gripper.waitForGoalToFinish(ros::Duration(allowed_time));
				if (finished_before_timeout)
					std::cout << "Final state is " << gripper.getTerminalState().toString() << std::endl;
				else
				{
					gripper.cancelGoal();
					std::cerr << "Failed achieving goal" << std::endl;
				}
			}
			else
				std::cerr << "A floating point value expected but '" << cmd.substr(5) << "' was given" << std::endl;
		}
		else
			if (goals.find(cmd) != goals.end())
		{
			printConfig(goals[cmd]);
			std::cout << std::endl;
			btTransform p = effPosition(km, goals[cmd]);
			printPose(p);
		}
		else
			std::cerr << "Unknown command. Try 'help'" << std::endl;
	}

	return 0;
}
