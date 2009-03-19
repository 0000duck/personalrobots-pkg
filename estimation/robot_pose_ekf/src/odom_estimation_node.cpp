
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

#include "odom_estimation_node.h"


using namespace MatrixWrapper;
using namespace std;
using namespace ros;
using namespace tf;


static const double EPS = 1e-5;
static const string publish_name = "odom_combined";


//#define __EKF_DEBUG_FILE__

namespace estimation
{
  // constructor
  OdomEstimationNode::OdomEstimationNode(const string& node_name)
    : ros::Node(node_name),
      node_name_(node_name),
      robot_state_(*this, true),
      odom_broadcaster_(*this),
      vo_notifier_(NULL),
      vel_desi_(2),
      vel_active_(false),
      odom_active_(false),
      imu_active_(false),
      vo_active_(false),
      odom_initializing_(false),
      imu_initializing_(false),
      vo_initializing_(false)
  {
    // paramters
    param(node_name_+"/freq", freq_, 30.0);
    param(node_name_+"/sensor_timeout", timeout_, 1.0);
    param(node_name_+"/odom_used", odom_used_, true);
    param(node_name_+"/imu_used",  imu_used_, true);
    param(node_name_+"/vo_used",   vo_used_, true);

    // advertise our estimation
    advertise<robot_msgs::PoseWithCovariance>(node_name_+"/"+publish_name, 10);

    // initialize
    filter_stamp_ = Time::now();

    // fiexed transform between camera frame and vo frame
    vo_camera_ = Transform(Quaternion(M_PI/2.0, -M_PI/2,0), Vector3(0,0,0));

    // subscribe to messages
    subscribe("cmd_vel",      vel_,  &OdomEstimationNode::velCallback,  10);

    // subscribe to odom messages
    if (odom_used_){
      ROS_INFO((node_name_+"  Odom sensor can be used").c_str());
      subscribe("odom",         odom_, &OdomEstimationNode::odomCallback, 10);
    }
    else ROS_INFO((node_name_+"  Odom sensor will NOT be used").c_str());

    // subscribe to imu messages
    if (imu_used_){
      ROS_INFO((node_name_+"  Imu sensor can be used").c_str());
      subscribe("imu_data",     imu_,  &OdomEstimationNode::imuCallback,  10);
    }
    else ROS_INFO((node_name_+"  Imu sensor will NOT be used").c_str());

    // subscribe to vo messages
    if (vo_used_){
      ROS_INFO((node_name_+"  VO sensor can be used").c_str());
      vo_notifier_ = new MessageNotifier<robot_msgs::VOPose>(&robot_state_, this,  boost::bind(&OdomEstimationNode::voCallback, this, _1), "vo", "base_link", 10);
    }
    else ROS_INFO((node_name_+"  VO sensor will NOT be used").c_str());


#ifdef __EKF_DEBUG_FILE__
    // open files for debugging
    odom_file_.open("/tmp/odom_file.txt");
    imu_file_.open("/tmp/imu_file.txt");
    vo_file_.open("/tmp/vo_file.txt");
    corr_file_.open("/tmp/corr_file.txt");
    time_file_.open("/tmp/time_file.txt");
    extra_file_.open("/tmp/extra_file.txt");
#endif
  };




  // destructor
  OdomEstimationNode::~OdomEstimationNode(){
    if (vo_notifier_) delete vo_notifier_;

#ifdef __EKF_DEBUG_FILE__
    // close files for debugging
    odom_file_.close();
    imu_file_.close();
    vo_file_.close();
    corr_file_.close();
    time_file_.close();
    extra_file_.close();
#endif
  };





  // callback function for odom data
  void OdomEstimationNode::odomCallback()
  {
    assert(odom_used_);

    // receive data 
    boost::mutex::scoped_lock lock(odom_mutex_);
    odom_stamp_ = odom_.header.stamp;
    odom_time_  = Time::now();
    odom_meas_  = Transform(Quaternion(odom_.pos.th,0,0), Vector3(odom_.pos.x, odom_.pos.y, 0));
    
    // multiplier to scale covariance
    // the smaller the residual, the more reliable odom
    double odom_multiplier;
    if (odom_.residual < 0.05)
      // residu=0.00001 --> multiplier=0.00001     residu=0.05 --> multiplier=1.0
      odom_multiplier = ((odom_.residual-0.00001)*(0.99999/0.04999))+0.00001;  
    else
      // residu=0.05 --> multiplier=1.0     residu=0.2 --> multiplier=20.0
      odom_multiplier = ((odom_.residual-0.05)*(19.0/0.15))+1.0; 
    odom_multiplier = fmax(0.00001, fmin(100.0, odom_multiplier));
    cout << "odom_multiplier = " << odom_multiplier << endl;
    my_filter_.addMeasurement(Stamped<Transform>(odom_meas_, odom_stamp_,"wheelodom", "base_footprint"), odom_multiplier);
    
    // activate odom
    if (!odom_active_) {
      if (!odom_initializing_){
	odom_initializing_ = true;
	odom_init_stamp_ = odom_stamp_;
	ROS_INFO((node_name_+"  Initializing Odom sensor").c_str());      
      }
      if ( filter_stamp_ >= odom_init_stamp_){
	odom_active_ = true;
	odom_initializing_ = false;
	ROS_INFO((node_name_+"  Odom sensor activated").c_str());      
      }
      else ROS_INFO("Waiting to activate Odom, because Odom measurements are still %f sec in the future.", 
		    (odom_init_stamp_ - filter_stamp_).toSec());
    }
    
#ifdef __EKF_DEBUG_FILE__
    // write to file
    double tmp, yaw;
    odom_meas_.getBasis().getEulerZYX(yaw, tmp, tmp);
    odom_file_ << odom_meas_.getOrigin().x() << " " << odom_meas_.getOrigin().y() << "  " << yaw << "  " << endl;
#endif
  };




  // callback function for imu data
  void OdomEstimationNode::imuCallback()
  {
    assert(imu_used_);

    // receive data
    boost::mutex::scoped_lock lock(imu_mutex_);
    imu_stamp_ = imu_.header.stamp;
    imu_time_  = Time::now();
    PoseMsgToTF(imu_.pos, imu_meas_);
    my_filter_.addMeasurement(Stamped<Transform>(imu_meas_, imu_stamp_, "imu", "base_footprint"));
    
    // activate imu
    if (!imu_active_) {
      if (!imu_initializing_){
	imu_initializing_ = true;
	imu_init_stamp_ = imu_stamp_;
	ROS_INFO((node_name_+"  Initializing Imu sensor").c_str());      
      }
      if ( filter_stamp_ >= imu_init_stamp_){
	imu_active_ = true;
	imu_initializing_ = false;
	ROS_INFO((node_name_+"  Imu sensor activated").c_str());      
      }
      else ROS_INFO("Waiting to activate IMU, because IMU measurements are still %f sec in the future.", 
		    (imu_init_stamp_ - filter_stamp_).toSec());
    }
    
#ifdef __EKF_DEBUG_FILE__
    // write to file
    double tmp, yaw;
    imu_meas_.getBasis().getEulerZYX(yaw, tmp, tmp); 
    imu_file_ << yaw << endl;
#endif
  };




  // callback function for VO data
  void OdomEstimationNode::voCallback(const MessageNotifier<robot_msgs::VOPose>::MessagePtr& vo)
  {
    assert(vo_used_);

    // get data
    boost::mutex::scoped_lock lock(vo_mutex_);
    vo_ = *vo;
    vo_stamp_ = vo_.header.stamp;
    vo_time_  = Time::now();
    robot_state_.lookupTransform("stereo_link","base_link", vo_stamp_, camera_base_);
    PoseMsgToTF(vo_.pose, vo_meas_);
    
    // initialize
    if (!vo_active_ && !vo_initializing_){
      base_vo_init_ = camera_base_.inverse() * vo_camera_.inverse() * vo_meas_.inverse();
    }
    // vo measurement as base transform
    // the more inliers, the more reliable the vo
    // inliers=200+ --> multiplier=1        inliers=100 --> multiplier=11      inliers=10 --> multiplier=20
    double vo_multiplier = 21.0-(min(200.0,(double)vo_.inliers)/10.0);
    vo_multiplier = fmax(0.00001, fmin(100.0, vo_multiplier));
    cout << "vo_multiplier = " << vo_multiplier << endl;
    Transform vo_meas_base = base_vo_init_ * vo_meas_ * vo_camera_ * camera_base_;
    my_filter_.addMeasurement(Stamped<Transform>(vo_meas_base, vo_stamp_, "vo", "base_footprint"),
			      vo_multiplier);
    
    // activate vo
    if (!vo_active_) {
      if (!vo_initializing_){
	vo_initializing_ = true;
	vo_init_stamp_ = vo_stamp_;
	ROS_INFO((node_name_+"  Initializing Vo sensor").c_str());      
      }
      if (filter_stamp_ >= vo_init_stamp_){
	vo_active_ = true;
	vo_initializing_ = false;
	ROS_INFO((node_name_+"  Vo sensor activated").c_str());      
      }
      else ROS_INFO("Waiting to activate VO, because VO measurements are still %f sec in the future.", 
		    (vo_init_stamp_ - filter_stamp_).toSec());
    }
    
#ifdef __EKF_DEBUG_FILE__
    // write to file
    double Rx, Ry, Rz;
    vo_meas_base.getBasis().getEulerZYX(Rz, Ry, Rx);
    vo_file_ << vo_meas_base.getOrigin().x() << " " << vo_meas_base.getOrigin().y() << " " << vo_meas_base.getOrigin().z() << " "
	     << Rx << " " << Ry << " " << Rz << endl;
#endif
  };




  // callback function for vel data
  void OdomEstimationNode::velCallback()
  {
    // receive data
    boost::mutex::scoped_lock lock(vel_mutex_);
    vel_desi_(1) = vel_.vel.vx;   vel_desi_(2) = vel_.ang_vel.vz;

    // active
    //if (!vel_active_) vel_active_ = true;
  };





  // filter loop
  void OdomEstimationNode::spin()
  {
    while (ok()){
      boost::mutex::scoped_lock odom_lock(odom_mutex_);
      boost::mutex::scoped_lock imu_lock(imu_mutex_);
      boost::mutex::scoped_lock vo_lock(vo_mutex_);

      // check for timing problems
      if ( (odom_initializing_ || odom_active_) && (imu_initializing_ || imu_active_) ){
	double diff = fabs( Duration(odom_stamp_ - imu_stamp_).toSec() );
	if (diff > 1.0) ROS_ERROR("SYSTEM HAS TIMING PROBLEMS: timestamps of odometry and imu are %f seconds apart.", diff);
      }

      // initial value for filter stamp; keep this stamp when no sensors are active
      filter_stamp_ = Time::now();

      // check which sensors are still active
      if ((odom_active_ || odom_initializing_) && 
	  (Time::now() - odom_time_).toSec() > timeout_){
	odom_active_ = false; odom_initializing_ = false;
	ROS_INFO((node_name_+"  Odom sensor not active any more").c_str());
      }
      if ((imu_active_ || imu_initializing_) && 
	  (Time::now() - imu_time_).toSec() > timeout_){
	imu_active_ = false;  imu_initializing_ = false;
	ROS_INFO((node_name_+"  Imu sensor not active any more").c_str());
      }
      if ((vo_active_ || vo_initializing_) && 
	  (Time::now() - vo_time_).toSec() > timeout_){
	vo_active_ = false;  vo_initializing_ = false;
	ROS_INFO((node_name_+"  VO sensor not active any more").c_str());
      }

      // only update filter when one of the sensors is active
      if (odom_active_ || imu_active_ || vo_active_){

	// update filter at time where all sensor measurements are available
	if (odom_active_)  filter_stamp_ = min(filter_stamp_, odom_stamp_);
	if (imu_active_)   filter_stamp_ = min(filter_stamp_, imu_stamp_);
	if (vo_active_)    filter_stamp_ = min(filter_stamp_, vo_stamp_);

	// update filter
	if ( my_filter_.isInitialized() )  {
	  if (my_filter_.update(odom_active_, imu_active_, vo_active_,  filter_stamp_)){

	    // output most recent estimate and relative covariance
	    my_filter_.getEstimate(output_);
	    publish(node_name_+"/"+publish_name, output_);

	    // broadcast most recent estimate to TransformArray
	    Stamped<Transform> tmp;
	    my_filter_.getEstimate(ros::Time(), tmp);
	    if(!vo_active_)
	      tmp.getOrigin().setZ(0.0);
	    odom_broadcaster_.sendTransform(Stamped<Transform>(tmp, tmp.stamp_, "base_footprint", publish_name));

#ifdef __EKF_DEBUG_FILE__
	    // write to file
	    ColumnVector estimate; 
	    my_filter_.getEstimate(estimate);
	    for (unsigned int i=1; i<=6; i++)
	      corr_file_ << estimate(i) << " ";
	    corr_file_ << endl;
#endif
	  }
	}

	  
	// initialize filer with odometry frame
	if ( odom_active_ && !my_filter_.isInitialized()){
	  my_filter_.initialize(odom_meas_, odom_stamp_);
	  ROS_INFO((node_name_+"  Fiter initialized").c_str());
	}
      }
      vo_lock.unlock();  imu_lock.unlock();  odom_lock.unlock();
      
      // sleep
      usleep(1e6/freq_);
    }
  };


}; // namespace






// ----------
// -- MAIN --
// ----------
using namespace estimation;
int main(int argc, char **argv)
{
  // Initialize ROS
  ros::init(argc, argv);

  // get node name from arguments
  string node_name("robot_pose_ekf");
  if (argc > 2){
    ROS_INFO((node_name+"  Too many arguments for robot_pose_ekf.").c_str());
    assert(0);
  }
  if (argc == 2)
    node_name = argv[1];
  ROS_INFO((node_name+"  Creating robot pose ekf with name %s").c_str(), node_name.c_str());

  // create filter class
  OdomEstimationNode my_filter_node(node_name);

  // wait for filter to finish
  my_filter_node.spin();

  // Clean up
  
  return 0;
}
