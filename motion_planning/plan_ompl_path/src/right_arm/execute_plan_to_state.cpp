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

/** This is a simple program that shows how to plan a motion to a state.
    More than what is necessary is presented:
    - an example of initializing the start state of the trajectory (this is not 
      mandatory; the planner will use the current state if one is not given)
    - an example of how to impose constraints on the path is also given
    - example using the trajectory controller: sending a trajectory on a topic
      or using a service (only one is needed) if using the service, waiting for
      trajectory completion is also possible
    - sending the received path to be displayed is also possible
    - checking whether a straight line path would have been valid
      (using the motion_validator node)
    - printing the path to the screen
 */

#include <kinematic_planning/KinematicStateMonitor.h>

// service for planning to a state
#include <motion_planning_srvs/KinematicPlanState.h>

// service for validating a straight line path
#include <motion_planning_srvs/ValidateKinematicPath.h>

// message sent to visualizer to display the path
#include <motion_planning_msgs/DisplayKinematicPath.h>

// messages and services to interact with the trajectory controller
#include <robot_msgs/JointTraj.h>
#include <pr2_mechanism_controllers/TrajectoryStart.h>
#include <pr2_mechanism_controllers/TrajectoryQuery.h>

static const std::string GROUPNAME = "pr2::right_arm";
    
class Example : public kinematic_planning::KinematicStateMonitor
{
public:
    
    Example(void) : kinematic_planning::KinematicStateMonitor()
    {
	// if we use the topic for sending commands to the controller, we need to advertise it
	jointCommandPublisher_ = m_nodeHandle.advertise<robot_msgs::JointTraj>("right_arm/trajectory_controller/trajectory_command", 1);
	
	// advertise the topic for displaying kinematic plans
	displayKinematicPathPublisher_ = m_nodeHandle.advertise<motion_planning_msgs::DisplayKinematicPath>("display_kinematic_path", 10);
	
	// we can send commands to the trajectory controller both by using a topic, and by using services
	use_topic_ = false;
    }

    void runExample(void)
    {
	// construct the request for the motion planner
	motion_planning_msgs::KinematicPlanStateRequest req;
	
	req.params.model_id = GROUPNAME;
	req.params.distance_metric = "L2Square";
	req.params.planner_id = "SBL";
	req.threshold = 0.1;
	req.interpolate = 1;
	req.times = 1;

	// we retrieve the current state; if we do not do this, the
	// planner will do it for us so, we do not in fact need this
	// line; we would set the starting state if we do not want to
	// plan from the current state
	currentState(req.start_state);
	
	// 7 DOF for the arm; pick a goal state (joint angles)
	req.goal_state.set_vals_size(7);
	for (unsigned int i = 0 ; i < req.goal_state.get_vals_size(); ++i)
	    req.goal_state.vals[i] = 0.0;	
	req.goal_state.vals[0] = -1.5;
	req.goal_state.vals[1] = -0.2;


	// add some constraints for the solution path
	// these constraints must be met throughout the execution of the path
	
	// this constraing only makes sure the link is within a
	// defined distance from a specific point at all times; values
	// are set large so that we always find a solution
	// all these values have to be in the base_link frame 
	req.constraints.set_pose_size(1);
	req.constraints.pose[0].type = motion_planning_msgs::PoseConstraint::POSITION_XYZ;
	req.constraints.pose[0].robot_link = "r_gripper_palm_link";
	req.constraints.pose[0].x = 1;
	req.constraints.pose[0].y = 1;
	req.constraints.pose[0].z = 1;
	req.constraints.pose[0].position_distance = 10.0; // L2Square

	// allow 1 second computation time
	req.allowed_time = 1.0;
	
	// define the service messages
	motion_planning_srvs::KinematicPlanState::Request  s_req;
	motion_planning_srvs::KinematicPlanState::Response s_res;
	s_req.value = req;

	ros::ServiceClient client = m_nodeHandle.serviceClient<motion_planning_srvs::KinematicPlanState>("plan_kinematic_path_state");
	if (client.call(s_req, s_res))
	{
	    // print the path on screen
	    printPath(s_res.value.path);
	    
	    // send the path to the visualizer
	    sendDisplay(req.start_state, s_res.value.path, GROUPNAME);

	    // check if the straight line path would have been valid
	    verifyDirectPath(req.start_state, req.constraints, req.goal_state, GROUPNAME);
	    
	    // send the path to the controller
	    if (use_topic_)
		sendArmCommand(s_res.value.path, GROUPNAME);
	    else
		sendArmCommandAndWait(s_res.value.path, GROUPNAME);
	}
	else
	    ROS_ERROR("Service 'plan_kinematic_path_state' failed");
    }
    
    void run(void)
    {
	loadRobotDescription();
	if (loadedRobot())
	{
	    sleep(1);
	    runExample();
	}
	sleep(1);
    }

protected:

    // get the current state from the StateParams instance monitored by the KinematicStateMonitor
    void currentState(motion_planning_msgs::KinematicState &state)
    {
	state.set_vals_size(m_kmodel->getModelInfo().stateDimension);
	for (unsigned int i = 0 ; i < state.get_vals_size() ; ++i)
	    state.vals[i] = m_robotState->getParams()[i];	
    }

    // convert a kinematic path message to a trajectory for the controller
    void getTrajectoryMsg(const motion_planning_msgs::KinematicPath &path, robot_msgs::JointTraj &traj)
    {	
        traj.set_points_size(path.get_states_size());	
	for (unsigned int i = 0 ; i < path.get_states_size() ; ++i)
        {	    
            traj.points[i].set_positions_size(path.states[i].get_vals_size());	    
            for (unsigned int j = 0 ; j < path.states[i].get_vals_size() ; ++j)
                traj.points[i].positions[j] = path.states[i].vals[j];
            traj.points[i].time = 0.0;	    
        }	
    }
    
    // send a command to the trajectory controller using a topic
    void sendArmCommand(const motion_planning_msgs::KinematicPath &path, const std::string &model)
    {
	robot_msgs::JointTraj traj;
	getTrajectoryMsg(path, traj);
	jointCommandPublisher_.publish(traj);
	ROS_INFO("Sent trajectory to controller (using a topic)");
    }

    // send a command to the trajectory controller using a service; 
    // also demo how to wait for the trajectory to finish executing
    void sendArmCommandAndWait(const motion_planning_msgs::KinematicPath &path, const std::string &model)
    {	
	pr2_mechanism_controllers::TrajectoryStart::Request  send_traj_start_req;	
	pr2_mechanism_controllers::TrajectoryStart::Response send_traj_start_res;
	pr2_mechanism_controllers::TrajectoryQuery::Request  send_traj_query_req;	
	pr2_mechanism_controllers::TrajectoryQuery::Response send_traj_query_res;

	robot_msgs::JointTraj traj;	
        getTrajectoryMsg(path, traj);
        send_traj_start_req.traj = traj;
	
        int traj_done = -1;	
	ros::ServiceClient clientStart = m_nodeHandle.serviceClient<pr2_mechanism_controllers::TrajectoryStart>("right_arm/trajectory_controller/TrajectoryStart");
	ros::ServiceClient clientQuery = m_nodeHandle.serviceClient<pr2_mechanism_controllers::TrajectoryStart>("right_arm/trajectory_controller/TrajectoryQuery");
        if (clientStart.call(send_traj_start_req, send_traj_start_res))
        {	    
            ROS_INFO("Sent trajectory to controller (using a service)");
            send_traj_query_req.trajectoryid =  send_traj_start_res.trajectoryid;	    
            while(!(traj_done == send_traj_query_res.State_Done || traj_done == send_traj_query_res.State_Failed))
            {		
		if(clientQuery.call(send_traj_query_req,  send_traj_query_res))
                {		    
                    traj_done = send_traj_query_res.done;		    
                }		
                else
                {		    
                    ROS_ERROR("Trajectory query failed");		    
                }
	    }
	    ROS_INFO("Trajectory execution is complete");
	}
    }
    
    // send a message that can be used to display the kinematic path
    void sendDisplay(const motion_planning_msgs::KinematicState &start, const motion_planning_msgs::KinematicPath &path, const std::string &model)
    {
	motion_planning_msgs::DisplayKinematicPath dpath;
	dpath.frame_id = "base_link";
	dpath.model_name = model;
	dpath.start_state = start;
	dpath.path = path;
	displayKinematicPathPublisher_.publish(dpath);
	ROS_INFO("Sent planned path to display");
    }
    
    void printPath(const motion_planning_msgs::KinematicPath &path)
    {
	printf("Path with %d states\n", (int)path.states.size());	
	for (unsigned int i = 0 ; i < path.states.size() ; ++i)
	{
	    for (unsigned int j = 0 ; j < path.states[i].vals.size() ; ++j)
		printf("%f ", path.states[i].vals[j]);	    
	    printf("\n");
	}
	printf("\n");
    }
    
    // check if straight line path is valid (motion_validator node)
    bool verifyDirectPath(const motion_planning_msgs::KinematicState &start, const motion_planning_msgs::KinematicConstraints &cstrs,
			  const motion_planning_msgs::KinematicState &goal, const std::string &model)
    {  
	motion_planning_srvs::ValidateKinematicPath::Request  req;	    
	motion_planning_srvs::ValidateKinematicPath::Response res;
	req.model_id = model;	    
	req.start_state = start;	    
	req.constraints = cstrs;
	req.goal_state = goal;

	ros::ServiceClient client = m_nodeHandle.serviceClient<motion_planning_srvs::ValidateKinematicPath>("validate_path");
	if (client.call(req, res))
	{		
	    if (res.valid)
		ROS_INFO("Direct path is valid");		
	    else
		ROS_INFO("Direct path is not valid");
	}
	else
	    ROS_INFO("Service 'validate_path' not available (or failed)");
	
	return res.valid;
    }

private:

    bool           use_topic_;    
    ros::Publisher jointCommandPublisher_;
    ros::Publisher displayKinematicPathPublisher_;

};


int main(int argc, char **argv)
{  
    ros::init(argc, argv, "example_execute_plan_to_state");

    Example plan;
    plan.run();
    
    return 0;    
}
