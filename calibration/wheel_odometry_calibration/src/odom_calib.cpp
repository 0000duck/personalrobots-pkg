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

#include "odom_calib.h"

using namespace ros;
using namespace tf;
using namespace std;


static const double MAX_ROT_VEL = 0.5;
static const double MAX_TRANS_VEL = 0.4;
static const double MAX_DURATION = 15.0;

namespace calibration
{
  // constructor
  odom_calib::odom_calib()
    : ros::Node("odom_calibration"),
      _odom_active(false),
      _imu_active(false),
      _completed(false)
  {
    // advertise the velocity commands
    advertise<robot_msgs::PoseDot>("cmd_vel",10);

    // subscribe to messages
    subscribe("odom",            _odom, &odom_calib::odom_callback, 10);
    subscribe("imu_data",        _imu,  &odom_calib::imu_callback,  10);
  };



  // destructor
  odom_calib::~odom_calib(){};



  // callback function for odom data
  void odom_calib::odom_callback()
  {
    _odom_mutex.lock();
    if (!_odom_active){
      _odom_begin = tf::getYaw(_odom.pose_with_covariance.pose.orientation);
      _odom_end   = tf::getYaw(_odom.pose_with_covariance.pose.orientation);
      _odom_active = true;
    }
    else{
      double tmp = tf::getYaw(_odom.pose_with_covariance.pose.orientation);
      AngleOverflowCorrect(tmp, _odom_end);
      _odom_end = tmp;
    }
    _odom_mutex.unlock();
  };



  // callback function for imu data
  void odom_calib::imu_callback()
  {
    _imu_mutex.lock();
    double tmp, yaw;  Transform tf;
    poseMsgToTF(_imu.pose_with_rates.pose, tf);
    tf.getBasis().getEulerZYX(yaw, tmp, tmp);

    if (!_imu_active){
      _imu_begin = yaw;
      _imu_end   = yaw;
      _imu_active = true;
    }
    else{
      double tmp = yaw;
      AngleOverflowCorrect(tmp, _imu_end);
      _imu_end = tmp;
    }
    _imu_mutex.unlock();
  };




  // correct for angle overflow
  void odom_calib::AngleOverflowCorrect(double& a, double ref)
  {
    while ((a-ref) >  M_PI) a -= 2*M_PI;
    while ((a-ref) < -M_PI) a += 2*M_PI;
  };



  void odom_calib::start()
  {
    // get parameters
    param("odom_calibration/rot_vel",_rot_vel, 0.0);
    param("odom_calibration/trans_vel",_trans_vel, 0.0);
    param("odom_calibration/duration",_duration, 0.0);
    _rot_vel    = max(min(_rot_vel,   MAX_ROT_VEL),-MAX_ROT_VEL);
    _trans_vel  = max(min(_trans_vel, MAX_TRANS_VEL),-MAX_TRANS_VEL);
    _duration   = max(min(_duration,  MAX_DURATION),-MAX_DURATION);

    ROS_INFO("(Odometry Calibration)  Will rotate at %f [deg/sec], and translate at %f [m/sec]", _rot_vel*180/M_PI, _trans_vel);
    ROS_INFO("(Odometry Calibration)  Will move during at %f seconds", _duration);

    // wait for sensor measurements from odom and imu
    while (!(_odom_active && _imu_active)){
      _vel.vel.vx = 0; _vel.vel.vy = 0; _vel.vel.vz = 0;
      _vel.ang_vel.vx = 0; _vel.ang_vel.vy = 0; _vel.ang_vel.vz = 0;
      publish("cmd_vel", _vel);
      if (!_odom_active)
	ROS_INFO("Waiting for wheel odometry to start");
      if (!_imu_active)
	ROS_INFO("Waiting for imu sensor to start");
      usleep(1e6);
    }

    // get time
    _time_begin = Time::now();
  }




  void odom_calib::spin()
  {
    ROS_INFO("Running calibration of wheel radius");
    while (!_completed){
      // still moving
      Duration duration = Time::now() - _time_begin;
      if (duration.toSec() <= _duration){
	_vel.vel.vx = _trans_vel;
	_vel.ang_vel.vz = _rot_vel;
      }
      // finished turning
      else{
	_completed = true;
	_vel.vel.vx = 0;
	_vel.ang_vel.vz = 0;
      }
      publish("cmd_vel", _vel);
      usleep(50000);
    }
  }


  void odom_calib::stop()
  {
    // give robot time to stop
    for (unsigned int i=0; i<10; i++){
      _vel.vel.vx = 0;
      _vel.ang_vel.vz = 0;
      publish("cmd_vel", _vel);
      usleep(50000);
    }

    _imu_mutex.lock();
    _odom_mutex.lock();

    // rotation
    double d_imu  = _imu_end  - _imu_begin;
    double d_odom = _odom_end - _odom_begin;
    ROS_INFO("Rotated imu  %f degrees", d_imu*180/M_PI);
    ROS_INFO("Rotated wheel odom %f degrees", d_odom*180/M_PI);
    ROS_INFO("Sending correction ratio %f to controller", d_imu / d_odom);

    // send results to base server
    _srv_snd.radius_multiplier =  d_imu / d_odom;
    if (service::call("base_controller/set_wheel_radius_multiplier", _srv_snd, _srv_rsp)) 
      ROS_INFO("Correction ratio seccessfully sent");
    else
      ROS_INFO("Failed to send correction ratio");

    _odom_mutex.unlock();
    _imu_mutex.unlock();
  }

}; // namespace






// ----------
// -- MAIN --
// ----------
using namespace calibration;
int main(int argc, char **argv)
{
  // Initialize ROS
  ros::init(argc, argv);

  // calibrate
  odom_calib my_odom_calibration_node;

  my_odom_calibration_node.start();
  my_odom_calibration_node.spin();
  my_odom_calibration_node.stop();

  // Clean up
  
  return 0;
}
