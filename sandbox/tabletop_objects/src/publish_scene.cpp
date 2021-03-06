/*
 * Copyright (c) 2008 Radu Bogdan Rusu <rusu -=- cs.tum.edu>
 *
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
 *
 * $Id$
 *
 */

/**
@mainpage

\author Marius Muja

@b publish_scenene is a node that publishes the PCD (Point Cloud Data) files and images captured with stereo_capture node.
		It is derived from the pcd_generator node.

**/

// ROS core
#include <ros/node.h>

#include "opencv/cxcore.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"

#include <sensor_msgs/PointCloud.h>

#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <stereo_msgs/DisparityInfo.h>

#include <opencv_latest/CvBridge.h>
#include <sensor_msgs/fill_image.h>
#include <std_msgs/String.h>

#include <point_cloud_mapping/cloud_io.h>

#include <tf/transform_broadcaster.h>

using namespace std;
using namespace sensor_msgs;
using namespace stereo_msgs;

class PublishScene
{
protected:
	string tf_frame_;
	ros::Node& node_;
	tf::TransformBroadcaster broadcaster_;
	tf::Stamped<tf::Transform> transform_;


	IplImage* left_img_;
	IplImage* right_img_;
	IplImage* disp_img_;

	Image left_;
	Image right_;
	Image disp_;
	CameraInfo left_caminfo_;
	DisparityInfo dispinfo_;


	boost::mutex data_mutex;
	boost::condition_variable new_data;

public:

	// ROS messages
	sensor_msgs::PointCloud msg_cloud_;

	string prefix_;
	string cloud_topic_;
	string left_topic_;
	string right_topic_;
	string disp_topic_;
	string left_caminfo_topic_;
	string dispinfo_topic_;
	double rate_;


	std_msgs::String msg_prefix;

	PublishScene (ros::Node& anode) : tf_frame_ ("base_link"), node_ (anode),
	transform_ (btTransform (btQuaternion (0, 0, 0), btVector3 (0, 0, 0)), ros::Time::now (), tf_frame_)
	{
		// Maximum number of outgoing messages to be queued for delivery to subscribers = 1
		cloud_topic_ = "stereo/cloud";
		left_topic_ = "stereo/left/image_rect";
		right_topic_ = "stereo/right/image_rect";
		disp_topic_ = "stereo/disparity";
		left_caminfo_topic_ = "stereo/left/cam_info";
		dispinfo_topic_ = "stereo/disparity_info";
		node_.advertise<sensor_msgs::PointCloud> (cloud_topic_.c_str (), 1);
		node_.advertise<sensor_msgs::Image> (left_topic_.c_str (), 1);
		node_.advertise<sensor_msgs::Image> (right_topic_.c_str (), 1);
		node_.advertise<sensor_msgs::Image> (disp_topic_.c_str (), 1);
		node_.advertise<sensor_msgs::CameraInfo> (left_caminfo_topic_.c_str (), 1);
		node_.advertise<stereo_msgs::DisparityInfo> (dispinfo_topic_.c_str (), 1);
		ROS_INFO ("Publishing data on topic %s.", node_.mapName (cloud_topic_).c_str ());


		node_.subscribe("~prefix", msg_prefix, &PublishScene::newPrefix, this, 1);

		left_img_ = NULL;
		right_img_ = NULL;
		disp_img_ = NULL;

	}

	~PublishScene() {

		if (left_img_!=NULL)
			cvReleaseImage(&left_img_);
		if (right_img_!=NULL)
			cvReleaseImage(&right_img_);
		if (disp_img_!=NULL)
			cvReleaseImage(&disp_img_);
	}


	void getFilenames(const string& prefix, string& cloud_pcd, string& left_name, string& right_name, string& disp_name)
	{
		if (prefix.find(".T.")!=string::npos) {
			cloud_pcd = prefix + ".C.pcd";
			left_name = prefix + ".L.png";
			right_name = prefix + ".R.png";
			disp_name = prefix + ".D.png";
		}
		else {
			cloud_pcd = prefix + ".c.pcd";
			left_name = prefix + ".l.png";
			right_name = prefix + ".r.png";
			disp_name = prefix + ".d.png";
		}
	}

	void newPrefix()
	{
		boost::lock_guard<boost::mutex> lock(data_mutex);

		ROS_INFO("Got new prefix");
		prefix_ = msg_prefix.data;
		int success = loadData();

		if (success<0) {
			ROS_ERROR("Cannot load data");
		}
		else {
			new_data.notify_all();
		}
	}



	int loadData()
	{
		if (prefix_ == "") {
			return -1;
		}

		string cloud_pcd;
		string left_name;
		string right_name;
		string disp_name;
		getFilenames(prefix_, cloud_pcd, left_name, right_name, disp_name);

		ROS_INFO("Loading point cloud data: %s", cloud_pcd.c_str());
		if (cloud_io::loadPCDFile (cloud_pcd.c_str (), msg_cloud_) == -1) {
			return -2;
		}

		ROS_INFO("Loading right image: %s", left_name.c_str());
		left_img_ = cvLoadImage(left_name.c_str());
		ROS_INFO("Loading left image: %s", right_name.c_str());
		right_img_ = cvLoadImage(right_name.c_str());
		ROS_INFO("Loading disparity image: %s", disp_name.c_str());
		disp_img_ = cvLoadImage(disp_name.c_str(),CV_LOAD_IMAGE_GRAYSCALE);

		return 0;
	}


	void IplToImage(IplImage* cv_image, Image& ros_image, string name)
	{
		if (cv_image->nChannels==1) {
		  fillImage(ros_image, "mono8",
			    cv_image->height, cv_image->width, cv_image->width,
			    cv_image->imageData );
		}
		else if (cv_image->nChannels==3) {
		  fillImage(ros_image, "rgb8",
			    cv_image->height, cv_image->width, cv_image->width * 3,
			    cv_image->imageData );

		}

	}

	////////////////////////////////////////////////////////////////////////////////
	// Spin (!)
	bool spin ()
	{
//		double interval = rate_ * 1e+6;
		while (node_.ok ())
		{

			boost::unique_lock<boost::mutex> lock(data_mutex);

			new_data.wait(lock);

			ros::Time stamp = ros::Time::now();

			// transform
			transform_.stamp_ = stamp;
			broadcaster_.sendTransform (transform_);

			// point cloud
			ROS_INFO ("Publishing data (%d points) on topic %s in frame %s.", (int)msg_cloud_.points.size (), node_.mapName (cloud_topic_).c_str (), msg_cloud_.header.frame_id.c_str ());
			msg_cloud_.header.stamp = stamp;
			msg_cloud_.header.frame_id = tf_frame_;
			node_.publish (cloud_topic_.c_str(), msg_cloud_);

			ROS_INFO ("Publishing left image, channels: %d", left_img_->nChannels);
			IplToImage(left_img_, left_, "left");
			left_.header.stamp = stamp;
			left_.header.frame_id = tf_frame_;
			node_.publish (left_topic_.c_str(), left_);

			ROS_INFO ("Publishing right image");
			IplToImage(right_img_, right_, "right");
			right_.header.stamp = stamp;
			right_.header.frame_id = tf_frame_;
			node_.publish (right_topic_.c_str(), right_);

			ROS_INFO ("Publishing disparity image, channels: %d", disp_img_->nChannels);
			IplToImage(disp_img_, disp_, "disp");
			disp_.header.stamp = stamp;
			disp_.header.frame_id = tf_frame_;
			node_.publish (disp_topic_.c_str(), disp_);

			left_caminfo_.P[0] = -1;
			left_caminfo_.header.stamp = stamp;
			left_caminfo_.header.frame_id = tf_frame_;
			left_caminfo_.P[0] = 725.00002432;
			left_caminfo_.P[1] = 0.0;
			left_caminfo_.P[2] = 321.35299836;
			left_caminfo_.P[3] = 0.0;
			left_caminfo_.P[4] = 0.0;
			left_caminfo_.P[5] = 725.00002432;
			left_caminfo_.P[6] = 210.43089442;
			left_caminfo_.P[7] = 0;
			left_caminfo_.P[8] = 0;
			left_caminfo_.P[9] = 0;
			left_caminfo_.P[10] = 1;
			left_caminfo_.P[11] = 0;
			node_.publish (left_caminfo_topic_.c_str(), left_caminfo_);

			dispinfo_.header.stamp = stamp;
			dispinfo_.header.frame_id = tf_frame_;
			dispinfo_.dpp = 4.0;
			node_.publish(dispinfo_topic_.c_str(), dispinfo_);

//			sensor_msgs::CvBridge disp_bridge;
//			disp_bridge.fromImage(disp_);
//			cvNamedWindow("disparity", 1);
//			cvShowImage("disparity", disp_bridge.toIpl());

//			if (interval == 0)                      // We only publish once if a 0 seconds interval is given
//				break;
//			cvWaitKey(100);
//			usleep (interval);
		}

		return (true);
	}


};

/* ---[ */
int
main (int argc, char** argv)
{

	ros::init (argc, argv);
	ros::Node ros_node ("publish_scene");

	PublishScene c (ros_node);
	c.spin ();

	return (0);
}
/* ]--- */
