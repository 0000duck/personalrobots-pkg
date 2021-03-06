/*
 * wavefront
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**

@mainpage

@htmlinclude manifest.html

@b wavefront is a path-planning system for a robot moving in 2D.  It
implements the wavefront algorithm (described in various places; Latombe's
book is a good reference), which uses dynamic programming over an occupancy
grid map to find the lowest-cost path from the robot's pose to the goal.

This node uses part of the Player @b wavefront driver.  For detailed
documentation, consult <a
href="http://playerstage.sourceforge.net/doc/Player-cvs/player/group__driver__wavefront.html">Player
wavefront documentation</a>.  Note that this node does not actually wrap the @b
wavefront driver, but rather the underlying planner code.

The planning algorithm assumes that the robot is circular and holonomic.
Under these assumptions, it efficiently dilates obstacles and then plans
over the map as if the robot occupied a single grid cell.

If provided, laser scans are used to temporarily modify the map.  In this
way, the planner can avoid obstacles that are not in the static map.

The node also contains a controller that generates velocities for a
differential drive robot.  The intended usage is to run the node at a
modest rate, e.g., 20Hz, allowing it to replan and generate new controls
every cycle.  The controller ignores dynamics (i.e., assumes infinite
acceleration); this becomes a problem with robots that take non-trivial
time to slow down.

<hr>

@section usage Usage
@verbatim
$ wavefront
@endverbatim

<hr>

@section topic ROS topics

Subscribes to (name/type):
- @b "tf_message" tf/tfMessage: robot's pose in the "map" frame
- @b "goal" geometry_msgs/PoseStamped : goal for the robot.
- @b "scan" sensor_msgs/LaserScan : laser scans.  Used to temporarily modify the map for dynamic obstacles.

Publishes to (name / type):
- @b "cmd_vel" geometry_msgs/Twist : velocity commands to robot
- @b "state" nav_robot_actions/MoveBaseState : current planner state (e.g., goal reached, no path)
- @b "global_plan" nav_msgs/Path : current global path (for visualization)

<hr>

@section parameters ROS parameters

  - @b "~dist_eps" (double) : Goal distance tolerance (how close the robot must be to the goal before stopping), default: 1.0 meters
  - @b "~robot_radius" (double) : The robot's largest radius (the planner treats it as a circle), default: 0.175 meters
  - @b "~dist_penalty" (double) : Penalty factor for coming close to obstacles, default: 2.0
  - @b "~gui_publish_rate" (double) : Maximum rate at which scans and paths are published for visualization, default: 1.0 Hz

@todo There are many more parameters of the underlying planner, values
for which are currently hardcoded. These should be exposed as ROS
parameters.

 **/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <list>

#include "plan.h"

// roscpp
#include <ros/node.h>
#include <ros/time.h>
#include "boost/thread/mutex.hpp"

// The messages that we'll use
#include <nav_robot_actions/MoveBaseState.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Twist.h>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/LaserScan.h>
#include <nav_msgs/GetMap.h>

// For GUI debug
#include <nav_msgs/Path.h>

// For transform support
#include <tf/transform_listener.h>
#include <tf/message_notifier.h>

//Laser projection
#include "laser_geometry/laser_geometry.h"

// For time support
#include <ros/time.h>

#define ANG_NORM(a) atan2(sin((a)),cos((a)))
#define DTOR(a) ((a)*M_PI/180.0)
#define RTOD(a) ((a)*180.0/M_PI)
#define SIGN(x) (((x) < 0.0) ? -1 : 1)

// A bunch of x,y points, with a timestamp
typedef struct
{
  double* pts;
  size_t pts_num;
  ros::Time ts;
} laser_pts_t;

class WavefrontNode: public ros::Node
{
  private:
    // Plan object
    plan_t* plan;
    // Our state
    enum
    {
      NO_GOAL,
      NEW_GOAL,
      PURSUING_GOAL,
      //STUCK,
      REACHED_GOAL
    } planner_state;

  tf::Stamped<btTransform> global_pose_;

    // If we can't reach the goal, note when it happened and keep trying a bit
    //ros::Time stuck_time;
    // Are we enabled?
    bool enable;
    // Current goal
    double goal[3];
  tf::Stamped<tf::Pose> goalPose_;
    // Direction that we're rotating in order to assume the goal
    // orientation
    int rotate_dir;

    bool printed_warning;

    // Have we already stopped the robot?
    bool stopped;

    // Planning parameters
    double robot_radius;
    double safety_dist;
    double max_radius;
    double dist_penalty;
    double plan_halfwidth;
    double dist_eps;
    double ang_eps;
    ros::Duration cycletime;

    ros::Time gui_path_last_publish_time;
    ros::Duration gui_publish_rate;

    // Map update paramters (for adding obstacles)
    double laser_maxrange;
    ros::Duration laser_buffer_time;
    std::list<laser_pts_t> laser_scans;
    double* laser_hitpts;
    size_t laser_hitpts_len, laser_hitpts_size;

    // Controller paramters
    double lookahead_maxdist;
    double lookahead_distweight;
    double tvmin, tvmax, avmin, avmax, amin, amax;

    // incoming/outgoing messages
    geometry_msgs::PoseStamped goalMsg;
    //Odometry odomMsg;
    nav_msgs::Path pathMsg;
    nav_robot_actions::MoveBaseState pstate;
    //Odometry prevOdom;
    bool firstodom;

    tf::MessageNotifier<sensor_msgs::LaserScan>* scan_notifier;

    // Lock for access to class members in callbacks
    boost::mutex lock;

    // Message callbacks
    void goalReceived();
    void laserReceived(const tf::MessageNotifier<sensor_msgs::LaserScan>::MessagePtr& message);

    // Internal helpers
    void sendVelCmd(double vx, double vy, double vth);

    laser_geometry::LaserProjection projector_;
  public:
    // Transform listener
    tf::TransformListener tf;

    WavefrontNode();
    virtual ~WavefrontNode();

    // Stop the robot
    void stopRobot();
    // Execute a planning cycle
    void doOneCycle();
    // Sleep for the remaining time of the cycle
    void sleep(const ros::Time& loopstart);
    // Compare two poses, tell whether they are close enough to be
    // considered the same, with tolerance
    bool comparePoses(double x1, double y1, double a1,
                      double x2, double y2, double a2);
};

#define _xy_tolerance 1e-3
#define _th_tolerance 1e-3
#define USAGE "USAGE: wavefront"

int
main(int argc, char** argv)
{
  ros::init(argc,argv);

  WavefrontNode wn;

  ros::Time now;
  while(wn.ok())
  {
    now = ros::Time::now();
    wn.doOneCycle();
    wn.sleep(now);
  }

  return(0);
}

WavefrontNode::WavefrontNode() :
        ros::Node("wavefront"),
        planner_state(NO_GOAL),
        enable(true),
        rotate_dir(0),
        printed_warning(false),
        stopped(false),
        robot_radius(0.175), // overridden by param retrieval below!
        safety_dist(0.05),
        max_radius(2.0),
        dist_penalty(2.0),   // overridden by param retrieval below!
        plan_halfwidth(5.0),
        dist_eps(1.0),       // overridden by param retrieval below!
        ang_eps(DTOR(4.0)),
        cycletime(0.1),
        laser_maxrange(4.0),
        laser_buffer_time(ros::Duration().fromSec(3)),
        lookahead_maxdist(2.0),
        lookahead_distweight(5.0),
        tvmin(0.2),
        tvmax(0.75),
        avmin(DTOR(10.0)),
        avmax(DTOR(80.0)),
        amin(DTOR(10.0)),
        amax(DTOR(40.0)),
        tf(*this, true, ros::Duration(10)) // cache for 10 sec, no extrapolation
{
  // Initialize global pose. Will be set in control loop based on actual data.
  ///\todo does this need to be initialized?  global_pose_.setIdentity();

  // set a few parameters. leave defaults just as in the ctor initializer list
  param("~dist_eps", dist_eps, 1.0);
  param("~robot_radius", robot_radius, 0.175);
  param("~dist_penalty", dist_penalty, 2.0);
  double tmp;
  param("~gui_publish_rate", tmp, 1.0);
  gui_publish_rate = ros::Duration(1.0/tmp);

  // get map via RPC
  nav_msgs::GetMap::Request  req;
  nav_msgs::GetMap::Response resp;
  puts("Requesting the map...");
  while(!ros::service::call("/static_map", req, resp))
  {
    puts("request failed; trying again...");
    ros::Duration d(1.0);
    d.sleep();
  }
  printf("Received a %d X %d map @ %.3f m/pix\n",
         resp.map.info.width,
         resp.map.info.height,
         resp.map.info.resolution);
  char* mapdata;
  int sx, sy;
  sx = resp.map.info.width;
  sy = resp.map.info.height;
  // Convert to player format
  mapdata = new char[sx*sy];
  for(int i=0;i<sx*sy;i++)
  {
    if(resp.map.data[i] == 0)
      mapdata[i] = -1;
    else if(resp.map.data[i] == 100)
      mapdata[i] = +1;
    else
      mapdata[i] = 0;
  }

  assert((this->plan = plan_alloc(this->robot_radius+this->safety_dist,
                                  this->robot_radius+this->safety_dist,
                                  this->max_radius,
                                  this->dist_penalty,0.5)));

  // allocate space for map cells
  assert(this->plan->cells == NULL);
  assert((this->plan->cells =
          (plan_cell_t*)realloc(this->plan->cells,
                                (sx * sy * sizeof(plan_cell_t)))));

  // Copy over obstacle information from the image data that we read
  for(int j=0;j<sy;j++)
  {
    for(int i=0;i<sx;i++)
      this->plan->cells[i+j*sx].occ_state = mapdata[i+j*sx];
  }
  delete[] mapdata;

  this->plan->scale = resp.map.info.resolution;
  this->plan->size_x = sx;
  this->plan->size_y = sy;
  this->plan->origin_x = 0.0;
  this->plan->origin_y = 0.0;

  // Do initialization
  plan_init(this->plan);

  // Compute cspace over static map
  plan_compute_cspace(this->plan);
  printf("done computing c-space\n");

  this->laser_hitpts_size = this->laser_hitpts_len = 0;
  this->laser_hitpts = NULL;

  this->firstodom = true;

  advertise<nav_robot_actions::MoveBaseState>("state",1);
  advertise<nav_msgs::Path>("global_plan",1);
  advertise<geometry_msgs::Twist>("cmd_vel",1);
  subscribe("goal", goalMsg, &WavefrontNode::goalReceived,1);

  scan_notifier = new tf::MessageNotifier<sensor_msgs::LaserScan>(&tf, this, boost::bind(&WavefrontNode::laserReceived, this, _1), "scan", "/map", 1);
}

WavefrontNode::~WavefrontNode()
{
  plan_free(this->plan);

  delete scan_notifier;
}

void
WavefrontNode::goalReceived()
{
  this->lock.lock();
  // Got a new goal message; handle it
  this->enable = 1;

  if(this->enable){
    tf::poseStampedMsgToTF(goalMsg, this->goalPose_);


    // Populate goal data
    this->goal[0] = this->goalPose_.getOrigin().getX();
    this->goal[1] = this->goalPose_.getOrigin().getY();
    double yaw, pitch, roll;
    this->goalPose_.getBasis().getEulerZYX(yaw, pitch, roll);
    this->goal[2] = yaw;
    this->planner_state = NEW_GOAL;

    printf("got new goal: %.3f %.3f %.3f\n",
	   goal[0],
	   goal[1],
	   RTOD(goal[2]));

    // Set state for actively pursuing a goal
    this->pstate.status.value = this->pstate.status.ACTIVE;
  }
  else {
    // Clear goal data
    this->planner_state = NO_GOAL;

    // Set state inactive
    this->pstate.status.value = this->pstate.status.SUCCESS;
  }

  geometry_msgs::PoseStamped pose_msg;
  tf::poseStampedTFToMsg(global_pose_, pose_msg);

  // Fill out and publish response
  this->pstate.feedback = pose_msg;
  this->pstate.goal = goalMsg;
  publish("state",this->pstate);
  this->lock.unlock();
}

void
WavefrontNode::laserReceived(const tf::MessageNotifier<sensor_msgs::LaserScan>::MessagePtr& message)
{
	// Assemble a point cloud, in the laser's frame
	sensor_msgs::PointCloud local_cloud;
	projector_.projectLaser(*message, local_cloud, laser_maxrange);

	// Convert to a point cloud in the map frame
	sensor_msgs::PointCloud global_cloud;

	try
	{
		this->tf.transformPointCloud("/map", local_cloud, global_cloud);
	}
	catch(tf::LookupException& ex)
	{
		puts("no global->local Tx yet (point cloud)");
		printf("%s\n", ex.what());
		return;
	}
	catch(tf::ConnectivityException& ex)
	{
		puts("no global->local Tx yet (point cloud)");
		printf("%s\n", ex.what());
		return;
	}

	// Convert from point cloud to array of doubles formatted XYXYXY...
	// TODO: increase efficiency by reducing number of data transformations
	laser_pts_t pts;
	pts.pts_num = global_cloud.points.size();
	pts.pts = new double[pts.pts_num*2];
	assert(pts.pts);
	pts.ts = global_cloud.header.stamp;
	for(unsigned int i=0;i<global_cloud.points.size();i++)
	{
		pts.pts[2*i] = global_cloud.points[i].x;
		pts.pts[2*i+1] = global_cloud.points[i].y;
	}

	// Add the new point set to our list
	this->laser_scans.push_back(pts);

	// Remove anything that's too old from the laser_scans list
	// Also count how many points we have
	unsigned int hitpt_cnt=0;
	for(std::list<laser_pts_t>::iterator it = this->laser_scans.begin();
			it != this->laser_scans.end();
			it++)
	{
		if((message->header.stamp - it->ts) > this->laser_buffer_time)
		{
			delete[] it->pts;
			it = this->laser_scans.erase(it);
			it--;
		}
		else
			hitpt_cnt += it->pts_num;
	}

  // Lock here because we're operating on the laser_hitpts array, which is
  // used in another thread.
  this->lock.lock();
  // allocate more space as necessary
  if(this->laser_hitpts_size < hitpt_cnt)
  {
    this->laser_hitpts_size = hitpt_cnt;
    this->laser_hitpts =
            (double*)realloc(this->laser_hitpts,
                             2*this->laser_hitpts_size*sizeof(double));
    assert(this->laser_hitpts);
  }

  // Copy all of the current hitpts into the laser_hitpts array, from where
  // they will be copied into the planner, via plan_set_obstacles(), in
  // doOneCycle()
  this->laser_hitpts_len = 0;
  for(std::list<laser_pts_t>::iterator it = this->laser_scans.begin();
      it != this->laser_scans.end();
      it++)
  {
    memcpy(this->laser_hitpts + this->laser_hitpts_len * 2,
           it->pts, it->pts_num * 2 * sizeof(double));
    this->laser_hitpts_len += it->pts_num;
  }

  this->lock.unlock();
}

void
WavefrontNode::stopRobot()
{
  if(!this->stopped)
  {
    // TODO: should we send more than once, or perhaps use RPC for this?
    this->sendVelCmd(0.0,0.0,0.0);
    this->stopped = true;
  }
}

// Declare this globally, so that it never gets desctructed (message
// desctruction causes master disconnect)
geometry_msgs::Twist* cmdvel;

void
WavefrontNode::sendVelCmd(double vx, double vy, double vth)
{
  if(!cmdvel)
    cmdvel = new geometry_msgs::Twist();
  cmdvel->linear.x = vx;
  cmdvel->angular.z = vth;
  this->ros::Node::publish("cmd_vel", *cmdvel);
  if(vx || vy || vth)
    this->stopped = false;
}

// Execute a planning cycle
void
WavefrontNode::doOneCycle()
{
  // Get the current robot pose in the map frame
  //convert!

  tf::Stamped<tf::Pose> robotPose;
  robotPose.setIdentity();
  robotPose.frame_id_ = "base_link";
  robotPose.stamp_ = ros::Time(); // request most recent pose
  //robotPose.time = laserMsg.header.stamp.sec * (uint64_t)1000000000ULL +
  //        laserMsg.header.stamp.nsec; ///HACKE FIXME we should be able to get time somewhere else
  try
  {
    this->tf.transformPose("/map", robotPose, global_pose_);
  }
  catch(tf::LookupException& ex)
  {
    puts("L: no global->local Tx yet");
    this->stopRobot();
    return;
  }
  catch(tf::ConnectivityException& ex)
    {
      puts("C: no global->local Tx yet");
      printf("%s\n", ex.what());
      this->stopRobot();
      return;
    }
  catch(tf::ExtrapolationException& ex)
  {
    // this should never happen
    puts("WARNING: extrapolation failed!");
    printf("%s",ex.what());
    this->stopRobot();
    return;
  }

  if(!this->enable)
  {
    this->stopRobot();
    return;
  }

  this->lock.lock();
  switch(this->planner_state)
  {
    case NO_GOAL:
      //puts("no goal");
      this->stopRobot();
      break;
    case REACHED_GOAL:
      //puts("still done");
      this->stopRobot();
      break;
    case NEW_GOAL:
    case PURSUING_GOAL:
    //case STUCK:
      {

        double yaw,pitch,roll; //fixme make cleaner namespace
        btMatrix3x3 mat =  global_pose_.getBasis();
        mat.getEulerZYX(yaw, pitch, roll);

        // Are we done?
        if(plan_check_done(this->plan,
                           global_pose_.getOrigin().x(), global_pose_.getOrigin().y(), yaw,
                           this->goal[0], this->goal[1], this->goal[2],
                           this->dist_eps, this->ang_eps))
        {
          puts("done");
          this->rotate_dir = 0;
          this->stopRobot();
          this->planner_state = REACHED_GOAL;
          break;
        }

        //printf("setting %d hit points\n", this->laser_hitpts_len);
        plan_set_obstacles(this->plan,
                           this->laser_hitpts,
                           this->laser_hitpts_len);

        bool plan_valid = true;

        // Try a local plan
        if((this->planner_state == NEW_GOAL) ||
           (plan_do_local(this->plan, global_pose_.getOrigin().x(), global_pose_.getOrigin().y(),
                         this->plan_halfwidth) < 0))
        {
          // Fallback on global plan
          if(plan_do_global(this->plan, global_pose_.getOrigin().x(), global_pose_.getOrigin().y(),
                            this->goal[0], this->goal[1]) < 0)
          {
            // no global plan
            plan_valid = false;
            this->stopRobot();

            /*
            if (this->planner_state != STUCK)
            {
              printf("we're stuck. let's let the laser buffer empty "
                     "and see if that will free us.\n");
              this->planner_state = STUCK;
              this->stuck_time = ros::Time::now();
            }
            else
            {
              if (ros::Time::now() >
                  this->stuck_time + 1.5 * this->laser_buffer_time.to_double())
              {
                puts("global plan failed");
                this->planner_state = NO_GOAL; //NO_GOAL;
              }
            }
            */
          }
          else
          {
            // global plan succeeded; now try the local plan again
            this->printed_warning = false;
            if(plan_do_local(this->plan, global_pose_.getOrigin().x(), global_pose_.getOrigin().y(),
                             this->plan_halfwidth) < 0)
            {
              // no local plan; better luck next time through
              this->stopRobot();
              plan_valid = false;
            }
          }
        }

        if(this->planner_state == NEW_GOAL) // || this->planner_state == STUCK)
          this->planner_state = PURSUING_GOAL;

        if(!plan_valid)
          break;

        // We have a valid local plan.  Now compute controls
        double vx, va;

        //    double yaw,pitch,roll; //used temporarily earlier fixme make cleaner
        //btMatrix3x3
        mat =  global_pose_.getBasis();
        mat.getEulerZYX(yaw, pitch, roll);

        if(plan_compute_diffdrive_cmds(this->plan, &vx, &va,
                                       &this->rotate_dir,
                                       global_pose_.getOrigin().x(), global_pose_.getOrigin().y(),
                                       yaw,
                                       this->goal[0], this->goal[1],
                                       this->goal[2],
                                       this->dist_eps, this->ang_eps,
                                       this->lookahead_maxdist,
                                       this->lookahead_distweight,
                                       this->tvmin, this->tvmax,
                                       this->avmin, this->avmax,
                                       this->amin, this->amax) < 0)
        {
          this->stopRobot();
          puts("Failed to find a carrot");
          break;
        }

        assert(this->plan->path_count);

        ros::Time now = ros::Time::now();
        if((now - gui_path_last_publish_time) >= gui_publish_rate)
        {
          this->pathMsg.header.frame_id = "map";
          this->pathMsg.set_poses_size(this->plan->path_count);

          for(int i=0;i<this->plan->path_count;i++)
          {
            this->pathMsg.poses[i].pose.position.x =
                    PLAN_WXGX(this->plan,this->plan->path[i]->ci);
            this->pathMsg.poses[i].pose.position.y =
                    PLAN_WYGY(this->plan,this->plan->path[i]->cj);
            this->pathMsg.poses[i].pose.position.z = 0;
          }
          publish("global_plan", pathMsg);

          gui_path_last_publish_time = now;
        }

        //printf("computed velocities: %.3f %.3f\n", vx, RTOD(va));
        this->sendVelCmd(vx, 0.0, va);

        break;
      }
    default:
      assert(0);
  }
  if(this->enable && (this->planner_state == PURSUING_GOAL))
    this->pstate.status.value = this->pstate.status.ACTIVE;
  else
    this->pstate.status.value = this->pstate.status.SUCCESS;

  geometry_msgs::PoseStamped pose_out;
  tf::poseStampedTFToMsg(global_pose_, pose_out);
  this->pstate.feedback = pose_out;
  tf::poseStampedTFToMsg(this->goalPose_, pose_out);
  this->pstate.goal = pose_out;

  publish("state",this->pstate);

  this->lock.unlock();
}

// Sleep for the remaining time of the cycle
void
WavefrontNode::sleep(const ros::Time& loopstart)
{
  ros::Time now = ros::Time::now();
  ros::Duration diff = this->cycletime - (now - loopstart);
  if(diff.toSec() <= 0.0)
    puts("Wavefront missed deadline and not sleeping; check machine load");
  else
    diff.sleep();
}

bool
WavefrontNode::comparePoses(double x1, double y1, double a1,
                            double x2, double y2, double a2)
{
  bool res;
  if((fabs(x2-x1) <= _xy_tolerance) &&
     (fabs(y2-y1) <= _xy_tolerance) &&
     (fabs(ANG_NORM(ANG_NORM(a2)-ANG_NORM(a1))) <= _th_tolerance))
    res = true;
  else
    res = false;
  return(res);
}
