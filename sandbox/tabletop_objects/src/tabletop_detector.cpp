/*********************************************************************
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

// Author: Marius Muja

#include <vector>
#include <fstream>
#include <sstream>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <queue>
#include <algorithm>
#include <libgen.h> // for dirname()


#include "opencv_latest/CvBridge.h"

#include "opencv/cxcore.h"
#include "opencv/cv.h"
#include "opencv/highgui.h"


#include <point_cloud_mapping/geometry/angles.h>
#include <point_cloud_mapping/sample_consensus/sac_model_plane.h>
#include <point_cloud_mapping/sample_consensus/sac_model_oriented_plane.h>
#include <point_cloud_mapping/sample_consensus/sac_model_line.h>
#include <point_cloud_mapping/sample_consensus/sac.h>
#include <point_cloud_mapping/sample_consensus/ransac.h>
#include <point_cloud_mapping/sample_consensus/lmeds.h>
#include <point_cloud_mapping/geometry/statistics.h>
#include <point_cloud_mapping/geometry/projections.h>


#include "ros/ros.h"
#include "ros/callback_queue.h"
#include "stereo_msgs/StereoInfo.h"
#include "stereo_msgs/DisparityInfo.h"
#include "sensor_msgs/CameraInfo.h"
#include "sensor_msgs/Image.h"
#include "sensor_msgs/PointCloud.h"
#include "geometry_msgs/Point32.h"
#include "geometry_msgs/PointStamped.h"
#include "visualization_msgs/Marker.h"

#include <string>

// transform library
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>

#include "topic_synchronizer2/topic_synchronizer.h"

#include "CvStereoCamModel.h"

#include <boost/thread.hpp>

#include "chamfer_matching/chamfer_matching.h"
#include "tabletop_objects/ModelFit.h"
#include "tabletop_objects/FindObjectPoses.h"
#include "tabletop_msgs/TableTopObject.h"

//#include "flann.h"

using namespace std;

#define CV_PIXEL(type,img,x,y) (((type*)(img->imageData+y*img->widthStep))+x*img->nChannels)

class TabletopDetector
{
public:

	ros::NodeHandle nh_;

	sensor_msgs::ImageConstPtr limage_;
	sensor_msgs::ImageConstPtr rimage_;
	sensor_msgs::ImageConstPtr dimage_;
	stereo_msgs::StereoInfoConstPtr stinfo_;
	stereo_msgs::DisparityInfoConstPtr dispinfo_;
	sensor_msgs::CameraInfoConstPtr lcinfo_;
	sensor_msgs::CvBridge lbridge_;
	sensor_msgs::CvBridge rbridge_;
	sensor_msgs::CvBridge dbridge_;


	ros::Subscriber left_image_sub_;
	ros::Subscriber left_caminfo_image_sub_;
	ros::Subscriber right_image_sub_;
	ros::Subscriber disparity_sub_;
	ros::Subscriber cloud_sub_;
	ros::Subscriber dispinfo_sub_;


	ros::Publisher object_pub_;
	ros::Publisher objects_pub_;
	ros::Publisher marker_pub_;

	ros::ServiceServer service_;

	ros::ServiceClient model_fit_service_;

	sensor_msgs::PointCloudConstPtr cloud;

	IplImage* left;
	IplImage* right;
	IplImage* disp;
	IplImage* disp_clone;

	tf::TransformListener tf_;
	tf::TransformBroadcaster broadcaster_;
	TopicSynchronizer sync_;

	// display stereo images ?
	bool display_;

	int templates_no;
	int edges_high;


	int min_points_per_cluster_;
	int max_clustering_iterations_;
	double min_object_distance_;   // min distance between two objects
	double min_object_height_;
	string target_frame_;

	ChamferMatching* cm;

	ros::CallbackQueue service_queue_;
	boost::thread* service_thread_;

	boost::mutex data_lock_;
	boost::condition_variable data_cv_;
	bool got_data_;

	TabletopDetector()
	: left(NULL), right(NULL), disp(NULL), disp_clone(NULL), sync_(&TabletopDetector::syncCallback, this), got_data_(false)
	{
		// define node parameters
		nh_.param("~display", display_, false);
		nh_.param("~min_points_per_cluster", min_points_per_cluster_, 10);
		nh_.param("~max_clustering_iterations", max_clustering_iterations_, 7);
		nh_.param("~min_object_distance", min_object_distance_, 0.05);
		nh_.param("~min_object_height", min_object_height_, 0.05);
		nh_.param<string>("~target_frame", target_frame_,"odom_combined");

		string template_path;
		nh_.param<string>("~template_path", template_path,"templates.txt");

		templates_no = 10;
		edges_high = 100;

		if(display_){
			cvNamedWindow("left", CV_WINDOW_AUTOSIZE);
			cvNamedWindow("right", CV_WINDOW_AUTOSIZE);
			cvNamedWindow("disparity", CV_WINDOW_AUTOSIZE);
			cvNamedWindow("disparity_clone", CV_WINDOW_AUTOSIZE);
			cvNamedWindow("edges",1);
		}

		// subscribe to topics
		left_image_sub_ = nh_.subscribe(nh_.resolveName("stereo")+"/left/image_rect", 1, sync_.synchronize(&TabletopDetector::leftImageCallback, this));
		left_caminfo_image_sub_ = nh_.subscribe(nh_.resolveName("stereo")+"/left/cam_info", 1, sync_.synchronize(&TabletopDetector::leftCameraInfoCallback, this));
		right_image_sub_ = nh_.subscribe(nh_.resolveName("stereo")+"/right/image_rect", 1, sync_.synchronize(&TabletopDetector::rightImageCallback, this));
		disparity_sub_ = nh_.subscribe(nh_.resolveName("stereo")+"/disparity", 1, sync_.synchronize(&TabletopDetector::disparityImageCallback, this));
		cloud_sub_ = nh_.subscribe(nh_.resolveName("stereo")+"/cloud", 1, sync_.synchronize(&TabletopDetector::cloudCallback, this));
		dispinfo_sub_ = nh_.subscribe(nh_.resolveName("stereo")+"/disparity_info", 1, sync_.synchronize(&TabletopDetector::dispinfoCallback, this));

		// advertise topics
		objects_pub_ = nh_.advertise<sensor_msgs::PointCloud> ("~objects", 1);
		object_pub_ = nh_.advertise<sensor_msgs::PointCloud> ("~object", 1);
//		advertise<sensor_msgs::PointCloud> ("~outliers", 1);
//		advertise<sensor_msgs::PointCloud> ("~inliers", 1);
		marker_pub_ = nh_.advertise<visualization_msgs::Marker>("visualization_marker",1);

		ros::AdvertiseServiceOptions service_opts = ros::AdvertiseServiceOptions::create<tabletop_objects::FindObjectPoses>("table_top/find_object_poses",
							boost::bind(&TabletopDetector::findObjectPoses, this, _1, _2),ros::VoidPtr(), &service_queue_);
		service_ = nh_.advertiseService(service_opts);

//        loadTemplates(template_path);
    	service_thread_ = new boost::thread(boost::bind(&TabletopDetector::serviceThread, this));

    	model_fit_service_ = ros::service::createClient<tabletop_objects::ModelFit::Request, tabletop_objects::ModelFit::Response>("tabletop_objects/model_fit", true);

		cm = new ChamferMatching();
	}

	~TabletopDetector()
	{
		service_thread_->join();
		delete service_thread_;
		delete cm;
	}

private:

	void syncCallback()
	{
//		ROS_INFO("Sync callback");
		if (disp!=NULL) {
			cvReleaseImage(&disp);
		}
		if(dbridge_.fromImage(*dimage_)) {
			disp = cvCreateImage(cvGetSize(dbridge_.toIpl()), IPL_DEPTH_8U, 1);
			cvCvtScale(dbridge_.toIpl(), disp, 4.0/dispinfo_->dpp);
		}

//		runTabletopDetection();
		got_data_ = true;
		data_cv_.notify_all();
	}

	void leftCameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr& info)
	{
		if (got_data_) return;
//		ROS_INFO("Left caminfo callback");
		lcinfo_ = info;
	}

	void leftImageCallback(const sensor_msgs::Image::ConstPtr& image)
	{
		if (got_data_) return;
//		ROS_INFO("Left image callback");

		limage_ = image;
		if(lbridge_.fromImage(*limage_, "bgr8")) {
			left = lbridge_.toIpl();
		}
	}

	void rightImageCallback(const sensor_msgs::Image::ConstPtr& image)
	{
		if (got_data_) return;
//		ROS_INFO("Right image callback");

		rimage_ = image;
		if(rbridge_.fromImage(*rimage_, "bgr8")) {
			right = rbridge_.toIpl();
		}
	}

	void disparityImageCallback(const sensor_msgs::Image::ConstPtr& image)
	{
		if (got_data_) return;
		//		ROS_INFO("Disparity image callback");

		dimage_ = image;
	}

	void dispinfoCallback(const stereo_msgs::DisparityInfo::ConstPtr& dinfo)
	{
		if (got_data_) return;
//		ROS_INFO("Disp info callback");

		dispinfo_ = dinfo;
	}

	void cloudCallback(const sensor_msgs::PointCloud::ConstPtr& point_cloud)
	{
//		ROS_INFO("Cloud callback");
		if (got_data_) {
//			ROS_INFO("Discarding point cloud");
			return;
		}


		cloud = point_cloud;
	}


	bool findObjectPoses(tabletop_objects::FindObjectPoses::Request& req,
			tabletop_objects::FindObjectPoses::Response& resp)
	{
		got_data_ = false;
		ROS_INFO("FindObjectPoses: Service called");
		boost::unique_lock<boost::mutex> lock(data_lock_);

		while (!got_data_) {
			ROS_INFO("FindObjectPoses: Waiting for data");
			data_cv_.wait(lock);
		}

		findTableTopObjectPoses(resp.objects);

		// transform all poses to the target frame
		for (size_t i=0;i<resp.objects.size();++i) {
			geometry_msgs::PoseStamped& object_pose = resp.objects[i].object_pose;
			geometry_msgs::PoseStamped& grasp_pose = resp.objects[i].grasp_pose;


			ros::Time common_time;
			string error;
			if (tf_.getLatestCommonTime(object_pose.header.frame_id, target_frame_, common_time, &error)==tf::NO_ERROR) {
				object_pose.header.stamp = common_time;
				grasp_pose.header.stamp = common_time;
				tf_.transformPose(target_frame_, object_pose, object_pose);
				tf_.transformPose(target_frame_, grasp_pose, grasp_pose);

			}
			else {
				ROS_ERROR("Cannot transform pose from %s to %s", object_pose.header.frame_id.c_str(), target_frame_.c_str());
			}

//	        if (!tf_.canTransform(target_frame_, object_pose.header.frame_id, object_pose.header.stamp, ros::Duration(5.0))){
//	          ROS_ERROR("Cannot transform from %s to %s", object_pose.header.frame_id.c_str(), target_frame_.c_str());
//	          return false;
//	        }
//			tf_.transformPose(target_frame_, object_pose, object_pose);
//			PoseStamped& grasp_pose = resp.objects[i].grasp_pose;
//	        if (!tf_.canTransform(target_frame_, grasp_pose.header.frame_id, grasp_pose.header.stamp, ros::Duration(5.0))){
//	          ROS_ERROR("Cannot transform from %s to %s", grasp_pose.header.frame_id.c_str(), target_frame_.c_str());
//	          return false;
//	        }
//			tf_.transformPose(target_frame_, grasp_pose, grasp_pose);
		}
		return true;
	}



	void trimSpaces( string& str)
	{
		size_t startpos = str.find_first_not_of(" \t\n"); // Find the first character position after excluding leading blank spaces
		size_t endpos = str.find_last_not_of(" \t\n"); // Find the first character position from reverse af

		// if all spaces or empty return an empty string
		if(( string::npos == startpos ) || ( string::npos == endpos)) {
			str = "";
		}
		else {
			str = str.substr( startpos, endpos-startpos+1 );
		}
	}

	void loadTemplates(string path)
	{
		FILE* fin = fopen(path.c_str(),"r");

		if (fin==NULL) {
			ROS_ERROR("Cannot open template list: %s", path.c_str());
			exit(1);
		}

		char* path_ptr = strdup(path.c_str());
		string dir_path = dirname(path_ptr);


		int num;
		fscanf(fin,"%d",&num);

		for (int i=0;i<num;++i) {

			char line[512];
			float real_size;
			float image_size;

			fscanf(fin,"%s %f %f", line, &real_size, &image_size);
			string template_file = line;
			trimSpaces(template_file);
			printf("template file: %s\n",template_file.c_str());
			string template_path = dir_path + "/" + template_file;

			if (!template_file.empty()) {
				IplImage* templ = cvLoadImage(template_path.c_str(),CV_LOAD_IMAGE_GRAYSCALE);
				if (!templ) {
					ROS_ERROR("Cannot load template image: %s", template_path.c_str());
					exit(1);
				}
				cm->addTemplateFromImage(templ, 1000*image_size/real_size);  // real_size is in mm
				printf("%s %f %f %f\n", line, real_size, image_size, 1000*image_size/real_size);
				cvReleaseImage(&templ);
			}
		}

		free(path_ptr);
		fclose(fin);

	}


	bool fitSACPlane (const sensor_msgs::PointCloud& points, const vector<int> &indices,  // input
			vector<int> &inliers, vector<double> &coeff,  // output
			double dist_thresh, int min_points_per_model)
	{
		// Create and initialize the SAC model
		sample_consensus::SACModelPlane *model = new sample_consensus::SACModelPlane ();
		sample_consensus::SAC *sac             = new sample_consensus::RANSAC (model, dist_thresh);
		sac->setMaxIterations (100);
		model->setDataSet ((sensor_msgs::PointCloud*)&points, indices);

		// Search for the best plane
		if (sac->computeModel ()) {
			sac->computeCoefficients (coeff);                              // Compute the model coefficients
			sac->refineCoefficients (coeff);                             // Refine them using least-squares

			// Get the list of inliers
			model->selectWithinDistance (coeff, dist_thresh, inliers);

			if ((int)inliers.size()<min_points_per_model) {
				return false;
			}

			geometry_msgs::Point32 viewpoint;
			viewpoint.x = 0;
			viewpoint.y = 0;
			viewpoint.z = 0;
			// Flip the plane normal towards the viewpoint
			cloud_geometry::angles::flipNormalTowardsViewpoint (coeff, points.points.at(inliers[0]), viewpoint);

			ROS_INFO ("Found a planar model supported by %d inliers: [%g, %g, %g, %g]", (int)inliers.size (), coeff[0], coeff[1], coeff[2], coeff[3]);
		}
		else {
			ROS_ERROR ("Could not compute a planar model for %d points.", indices.size());
			return false;
		}

		delete sac;
		delete model;
		return true;
	}


	void filterByZBounds(const sensor_msgs::PointCloud& pc, double zmin, double zmax, sensor_msgs::PointCloud& filtered_pc, sensor_msgs::PointCloud& filtered_outside)
	{
		vector<int> indices_remove;
		for (size_t i = 0;i<pc.get_points_size();++i) {
			if (pc.points[i].z>zmax || pc.points[i].z<zmin) {
				indices_remove.push_back(i);
			}
		}
		cloud_geometry::getPointCloudOutside (pc, indices_remove, filtered_pc);
		cloud_geometry::getPointCloud(pc, indices_remove, filtered_outside);
	}


	void clearFromImage(IplImage* disp_img, const sensor_msgs::PointCloud& pc)
	{
		int xchan = -1;
		int ychan = -1;
		for(size_t i = 0;i < pc.channels.size();++i){
			if(pc.channels[i].name == "x"){
				xchan = i;
			}
			if(pc.channels[i].name == "y"){
				ychan = i;
			}
		}

		if (xchan==-1 || ychan==-1) {
			ROS_ERROR("Cannot find image coordinates in the point cloud");
			return;
		}

		// remove plane points from disparity image
		for (size_t i=0;i<pc.get_points_size();++i) {
			int x = pc.channels[xchan].values[i];
			int y = pc.channels[ychan].values[i];
			//			printf("(%d,%d)\n", x, y);
			CV_PIXEL(unsigned char, disp_img, x, y)[0] = 0;
		}
	}


	void filterTablePlane(const sensor_msgs::PointCloud& pc, vector<double>& coefficients, sensor_msgs::PointCloud& object_cloud, sensor_msgs::PointCloud& plane_cloud)
	{

		disp_clone = cvCloneImage(disp);

		sensor_msgs::PointCloud filtered_cloud;
		sensor_msgs::PointCloud filtered_outside;

		filterByZBounds(pc, 0.1, 1.2 , filtered_cloud, filtered_outside );

		clearFromImage(disp, filtered_outside);

		vector<int> indices(filtered_cloud.get_points_size());
		for (size_t i = 0;i<filtered_cloud.get_points_size();++i) {
			indices[i] = i;
		}

		vector<int> inliers;
		double dist_thresh = 0.01; // in meters
		int min_points = 200;

		fitSACPlane(filtered_cloud, indices, inliers, coefficients, dist_thresh, min_points);

		cloud_geometry::getPointCloud(filtered_cloud, inliers, plane_cloud);
		cloud_geometry::getPointCloudOutside (filtered_cloud, inliers, object_cloud);

		clearFromImage(disp, plane_cloud);
	}


	void projectToPlane(const sensor_msgs::PointCloud& objects, const vector<double>& plane, sensor_msgs::PointCloud& projected_objects)
	{
		vector<int> object_indices(objects.points.size());
		for (size_t i=0;i<objects.get_points_size();++i) {
			object_indices[i] = i;
		}

		projected_objects.header.stamp = objects.header.stamp;
		projected_objects.header.frame_id = objects.header.frame_id;

		cloud_geometry::projections::pointsToPlane(objects, object_indices, projected_objects, plane);

	}


	/**
	* \brief Finds edges in an image
	* @param img
	*/
	void doChamferMatching(IplImage *img, const vector<CvPoint>& positions, const vector<float>& scales)
	{
		// edge detection
		IplImage *gray = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
		cvCvtColor(img, gray, CV_RGB2GRAY);
		cvCanny(gray, gray, edges_high/2, edges_high);

		if (display_) {
			cvShowImage("edges", gray);
		}


//		for (int i=0;i<scales.size();++i) {
//			printf("%f, ", scales[i]);
//		}
//		printf("\n");

		ChamferMatch match = cm->matchEdgeImage(gray, LocationScaleImageRange(positions, scales));
		IplImage* left_clone = cvCloneImage(left);

	    ChamferMatch::ChamferMatches match_instances = match.getMatches();
	    for (size_t i = 0; i<match_instances.size();++i) {
	    	printf("Match with cost: %g at lcation: (%d,%d)\n", match_instances[i].cost, match_instances[i].offset.x,match_instances[i].offset.y);
	    	match.showMatch(left, i);
	    }


		if(display_){
			// show filtered disparity
			cvShowImage("disparity", disp);
			cvShowImage("disparity_clone", disp_clone);
			// show left image
			cvShowImage("left", left);
			cvShowImage("right", right);
		}

		cvCopy(left_clone, left);
		cvReleaseImage(&left_clone);

		cvReleaseImage(&gray);
	}


	/**
	 * Adds a new "table_frame" to tf
	 *
	 * @param origin - origin of the frame
	 * @param plane - the XOY plane of the new frame
	 */
	void addTableFrame(geometry_msgs::PointStamped origin, const vector<double>& plane)
	{

		btVector3 position(origin.point.x,origin.point.y,origin.point.z);

		btQuaternion orientation;
		btMatrix3x3 rotation;
		btVector3 z(plane[0],plane[1],plane[2]);
		btVector3 x(-plane[1],plane[0],0);
		x = x.normalized();
		btVector3 y = z.cross(x).normalized();
		rotation[0] = x; 	// x
		rotation[1] = y; 	// y
		rotation[2] = z; 	// z
		rotation = rotation.transpose();
		rotation.getRotation(orientation);

		tf::Transform tf_pose(orientation, position);

		// add wall_frame to tf
		tf::Stamped<tf::Pose> table_pose_frame(tf_pose, origin.header.stamp, "table_frame", origin.header.frame_id);
		ROS_INFO("Adding pose with stamp: %0.15f", origin.header.stamp.toSec());

		tf_.setTransform(table_pose_frame);

		broadcaster_.sendTransform(table_pose_frame);
	}



	void drawTableBBox(const sensor_msgs::PointCloud& cloud)
	{
		if (cloud.get_points_size()==0) return;
		float xmin = cloud.points[0].x;
		float xmax = cloud.points[0].x;
		float ymin = cloud.points[0].y;
		float ymax = cloud.points[0].y;

		for (size_t i=1;i<cloud.get_points_size();++i) {
			if (cloud.points[i].x<xmin) xmin = cloud.points[i].x;
			if (cloud.points[i].x>xmax) xmax = cloud.points[i].x;
			if (cloud.points[i].y<ymin) ymin = cloud.points[i].y;
			if (cloud.points[i].y>ymax) ymax = cloud.points[i].y;
		}

		// visualize data
		visualization_msgs::Marker marker;
		marker.header.frame_id = cloud.header.frame_id;
		marker.header.stamp = cloud.header.stamp;
		marker.ns = "tabletop_clusters";
		marker.id = 111;
		marker.type = visualization_msgs::Marker::LINE_STRIP;
		marker.action = visualization_msgs::Marker::ADD;
		marker.pose.position.x = 0;
		marker.pose.position.y = 0;
		marker.pose.position.z = 0;
		marker.pose.orientation.x = 0.0;
		marker.pose.orientation.y = 0.0;
		marker.pose.orientation.z = 0.0;
		marker.pose.orientation.w = 1.0;
		marker.scale.x = 0.001;
		marker.scale.y = 0.001;
		marker.scale.z = 0.001;
		marker.color.a = 1.0;
		marker.color.r = 1.0;
		marker.color.g = 1.0;
		marker.color.b = 0.0;

		marker.points.resize(5);
		marker.points[0].x = xmin;
		marker.points[0].y = ymin;
		marker.points[0].z = 0;

		marker.points[1].x = xmin;
		marker.points[1].y = ymax;
		marker.points[1].z = 0;

		marker.points[2].x = xmax;
		marker.points[2].y = ymax;
		marker.points[2].z = 0;

		marker.points[3].x = xmax;
		marker.points[3].y = ymin;
		marker.points[3].z = 0;

		marker.points[4].x = xmin;
		marker.points[4].y = ymin;
		marker.points[4].z = 0;

		marker_pub_.publish(marker);

	}

	void showCluster(geometry_msgs::Point32 p, float radius, int idx, const ros::Time& stamp, float* color = NULL)
	{
		float default_color[] = { 1,1,0};

		if (color==NULL) {
			color = default_color;
		}

		visualization_msgs::Marker marker;
		marker.header.frame_id = "table_frame";
		marker.header.stamp = stamp;
		marker.ns = "tabletop_clusters";
		marker.id = 200+idx;
		marker.type = visualization_msgs::Marker::SPHERE;
		marker.action = visualization_msgs::Marker::ADD;
		marker.pose.position.x = p.x;
		marker.pose.position.y = p.y;
		marker.pose.position.z = p.z;
		marker.pose.orientation.x = 0.0;
		marker.pose.orientation.y = 0.0;
		marker.pose.orientation.z = 0.0;
		marker.pose.orientation.w = 1.0;
		marker.scale.x = radius*2;
		marker.scale.y = radius*2;
		marker.scale.z = 0;
		marker.color.a = 1.0;
		marker.color.r = color[0];
		marker.color.g = color[1];
		marker.color.b = color[2];

		marker_pub_.publish(marker);
	}

	class NNGridIndexer
	{
		float xmin, xmax, ymin, ymax;
		float xd,yd;

		int resolution;

		float* grid;

	public:
		NNGridIndexer(const sensor_msgs::PointCloud& cloud)
		{
			xmin = cloud.points[0].x;
			xmax = cloud.points[0].x;
			ymin = cloud.points[0].y;
			ymax = cloud.points[0].y;
			for (size_t i=1;i<cloud.get_points_size();++i) {
				if (cloud.points[i].x<xmin) xmin = cloud.points[i].x;
				if (cloud.points[i].x>xmax) xmax = cloud.points[i].x;
				if (cloud.points[i].y<ymin) ymin = cloud.points[i].y;
				if (cloud.points[i].y>ymax) ymax = cloud.points[i].y;
			}

			resolution = 600;
			xd = (xmax-xmin)/(resolution-1);
			yd = (ymax-ymin)/(resolution-1);

			grid = new float[resolution*resolution];
			memset(grid,0,resolution*resolution*sizeof(float));

			for (size_t i=0;i<cloud.get_points_size();++i) {
				geometry_msgs::Point32 p = cloud.points[i];

				int x = int((p.x-xmin)/xd+0.5);
				int y = int((p.y-ymin)/yd+0.5);

				float *ptr = grid+x*resolution+y;
				*ptr = max(p.z,*ptr);
			}
		}

		~NNGridIndexer()
		{
			delete[] grid;
		}


		int computeMean(const geometry_msgs::Point32& p, float radius, geometry_msgs::Point32& result)
		{
			int xc = int((p.x-xmin)/xd+0.5);
			int yc = int((p.y-ymin)/yd+0.5);

			int xoffs = int((radius/xd)+0.5);
			int yoffs = int((radius/yd)+0.5);

			float xmean = 0;
			float ymean = 0;
			int count = 0;

			for (int x=xc-xoffs;x<=xc+xoffs;++x) {
				for (int y=yc-yoffs;y<=yc+yoffs;++y) {
					if (x<0 || x>=resolution) continue;
					if (y<0 || y>=resolution) continue;
					if (double((x-xc)*(x-xc))/(xoffs*xoffs)+double((y-yc)*(y-yc))/(yoffs*yoffs)>=1) continue;
					if (grid[x*resolution+y]==0) continue;

					xmean += x;
					ymean += y;
					count ++;
				}
			}


			if (count==0) return 0;

			xmean /= count;
			ymean /= count;

			result.x = xmean*xd+xmin;
			result.y = ymean*yd+ymin;

			return count;
		}

	};


	geometry_msgs::Point32 computeMean(const sensor_msgs::PointCloud& pc, const vector<int>& indices, int count)
	{
		geometry_msgs::Point32 mean;
		mean.x = 0;
		mean.y = 0;

		for (int i=0;i<count;++i) {
			int j = indices[i];
			mean.x += pc.points[j].x;
			mean.y += pc.points[j].y;
		}

		mean.x /= count;
		mean.y /= count;

		return mean;
	}


	template<typename T>
	static double dist2D(const T& a, const T& b)
	{
		return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y);
	}


	double meanShiftIteration(const sensor_msgs::PointCloud& pc, NNGridIndexer& index, vector<geometry_msgs::Point32>& centers, vector<geometry_msgs::Point32>& means, float step)
	{
		double total_dist = 0;
		for (size_t i=0;i<centers.size();++i) {

			geometry_msgs::Point32 mean;
			int count = index.computeMean(centers[i], step, mean);
			if (count==0) {
				ROS_WARN("Got empty cluster, this should not happen\n");
			}
			double dist = dist2D(mean, centers[i]);
			total_dist += dist;
			means[i] = mean;
		}
		return total_dist;
	}

#define CLUSTER_RADIUS 0.15/2


	void filterClusters(const sensor_msgs::PointCloud& cloud, vector<geometry_msgs::Point32>& centers, vector<sensor_msgs::PointCloud>& clusters)
	{
		// compute cluster heights (use to prune clusters)
		vector<double> cluster_heights(centers.size(), 0);
		// compute cluster heights
		for(size_t i=0;i<cloud.get_points_size();++i) {
			for (size_t j=0;j<centers.size();++j) {
				if (dist2D(cloud.points[i], centers[j])<CLUSTER_RADIUS*CLUSTER_RADIUS) {
					cluster_heights[j] = max(cluster_heights[j], (double)cloud.points[i].z);
				}
			}
		}


		// remove overlapping clusters
		vector<geometry_msgs::Point32> centers_pruned;
		for (size_t i=0;i<centers.size();++i) {
			if (cluster_heights[i]>min_object_height_) {

				bool duplicate = false;
				// check if duplicate cluster
				for (size_t j=0;j<centers_pruned.size();++j) {
					if (dist2D(centers_pruned[j],centers[i])<min_object_distance_*min_object_distance_) {
						duplicate = true;
						break;
					}
				}
				if (!duplicate) {
					centers_pruned.push_back(centers[i]);
					centers_pruned.back().z = cluster_heights[i];
				}
			}
		}


		// compute clusters
		sensor_msgs::PointCloud object;
		object.header.frame_id = cloud.header.frame_id;
		object.header.stamp = cloud.header.stamp;

		for (size_t k=0;k<centers_pruned.size();++k) {
			object.points.clear();
			for (size_t i=0;i<cloud.get_points_size();++i) {
				if (dist2D(centers_pruned[k],cloud.points[i])< CLUSTER_RADIUS*CLUSTER_RADIUS ) {
					object.points.push_back(cloud.points[i]);
				}
			}
			clusters.push_back(object);
		}

		centers = centers_pruned;

		ROS_INFO("Number of clusters: %d\n", centers_pruned.size());
	}



	void findTabletopClusters(const sensor_msgs::PointCloud& cloud, vector<geometry_msgs::Point32>& centers, vector<sensor_msgs::PointCloud>& clusters)
	{
		if (cloud.get_points_size()==0) return;

#if 0
		// initialize clusters using kmeans
		const int NUM_CLUSTERS = 30;

		int count = cloud.get_points_size();
		CvMat* points = cvCreateMat( count, 2, CV_32FC1 );
		CvMat* labels = cvCreateMat( count, 1, CV_32SC1 );
		CvMat* centers_ = cvCreateMat( NUM_CLUSTERS, 2, CV_32FC1 );

		for (int i=0;i<count;++i) {
			float* ptr = (float*)(points->data.ptr + i * points->step);
			ptr[0] = cloud.points[i].x;
			ptr[1] = cloud.points[i].y;
		}

		cvKMeans2(points, NUM_CLUSTERS, labels, cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 10, 0.005 ),5,0,0,centers_);


		float step = CLUSTER_RADIUS;
		NNGridIndexer index(cloud);
//		vector<int> indices(cloud.get_points_size());
		vector<geometry_msgs::Point32> centers(NUM_CLUSTERS);
		vector<geometry_msgs::Point32> means(NUM_CLUSTERS);
		geometry_msgs::Point32 p;
		double total_dist = 0;
		for (int i=0;i<NUM_CLUSTERS;++i) {
			float* ptr = (float*)(centers_->data.ptr + i * centers_->step);
			p.x = ptr[0];
			p.y = ptr[1];
			centers[i] = p;
			geometry_msgs::Point32 mean;
			int count = index.computeMean(p, step, mean);
			means[i]= mean;
			total_dist += dist2D(mean, p);
		}

		cvReleaseMat(&points);
		cvReleaseMat(&labels);
		cvReleaseMat(&centers_);

#else

		// get x,y ranges
		float xmin = cloud.points[0].x;
		float xmax = cloud.points[0].x;
		float ymin = cloud.points[0].y;
		float ymax = cloud.points[0].y;

		for (size_t i=1;i<cloud.get_points_size();++i) {
			if (cloud.points[i].x<xmin) xmin = cloud.points[i].x;
			if (cloud.points[i].x>xmax) xmax = cloud.points[i].x;
			if (cloud.points[i].y<ymin) ymin = cloud.points[i].y;
			if (cloud.points[i].y>ymax) ymax = cloud.points[i].y;
		}

		float step = CLUSTER_RADIUS;

		NNGridIndexer index(cloud);

		// getting the initial centers
//		vector<geometry_msgs::Point32> centers;
		vector<geometry_msgs::Point32> means;
		geometry_msgs::Point32 p;

		// layout initial clusters in a grid
		double total_dist = 0;
		for (double x = xmin;x<xmax;x+=step/2) {
			for (double y = ymin;y<ymax;y+=step/2) {
				p.x = x;
				p.y = y;

				geometry_msgs::Point32 mean;
				int found = index.computeMean(p, step, mean);

				if (found>min_points_per_cluster_) {
					centers.push_back(p);
					means.push_back(mean);
					total_dist += dist2D(mean, p);
				}
			}
		}

#endif

		int iter = 0;
		// mean-shift
		bool odd = true;
		while (total_dist>0.001) {

			if (odd) {
				total_dist = meanShiftIteration(cloud, index, means, centers, step);
			}
			else {
				total_dist = meanShiftIteration(cloud, index, centers, means, step);
			}
			odd = !odd;
			iter++;

			if (iter>max_clustering_iterations_) break;
		}

		filterClusters(cloud, centers, clusters);




//		printf("Total dist: %f\n", total_dist);

//		for (size_t i=0;i<clusters.size();++i) {
//			showCluster(clusters[i], step, i, cloud.header.stamp);
//		}

	}


	/**
	 * Projects 3D point into image plane
	 * @param cam_info Camera info containing camera projection matrix
	 * @param point The 3D point
	 * @return Projected point
	 */
	geometry_msgs::Point project3DPointIntoImage(const sensor_msgs::CameraInfo& cam_info, geometry_msgs::PointStamped point)
	{
		geometry_msgs::PointStamped image_point;
		tf_.transformPoint(cam_info.header.frame_id, point, image_point);
		geometry_msgs::Point pp; // projected point

		pp.x = cam_info.P[0]*image_point.point.x+
				cam_info.P[1]*image_point.point.y+
				cam_info.P[2]*image_point.point.z+
				cam_info.P[3];
		pp.y = cam_info.P[4]*image_point.point.x+
				cam_info.P[5]*image_point.point.y+
				cam_info.P[6]*image_point.point.z+
				cam_info.P[7];
		pp.z = cam_info.P[8]*image_point.point.x+
				cam_info.P[9]*image_point.point.y+
				cam_info.P[10]*image_point.point.z+
				cam_info.P[11];

		pp.x /= pp.z;
		pp.y /= pp.z;
		pp.z = 1;

		return pp;
	}

	void projectClusters(const sensor_msgs::PointCloud& objects_table_frame, const vector<geometry_msgs::Point32>& clusters)
	{
		const sensor_msgs::CameraInfo& lcinfo = *lcinfo_;

		geometry_msgs::Point pp[8];
		for (size_t i=0;i<clusters.size();++i) {
			geometry_msgs::PointStamped ps;
			ps.header.frame_id = objects_table_frame.header.frame_id;
			ps.header.stamp = objects_table_frame.header.stamp;
			ps.point.z = 0;


			ps.point.x = clusters[i].x - CLUSTER_RADIUS;
			ps.point.y = clusters[i].y - CLUSTER_RADIUS;
			pp[0] = project3DPointIntoImage(lcinfo, ps);

			ps.point.x = clusters[i].x - CLUSTER_RADIUS;
			ps.point.y = clusters[i].y + CLUSTER_RADIUS;
			pp[1] = project3DPointIntoImage(lcinfo, ps);

			ps.point.x = clusters[i].x + CLUSTER_RADIUS;
			ps.point.y = clusters[i].y + CLUSTER_RADIUS;
			pp[2] = project3DPointIntoImage(lcinfo, ps);

			ps.point.x = clusters[i].x + CLUSTER_RADIUS;
			ps.point.y = clusters[i].y - CLUSTER_RADIUS;
			pp[3] = project3DPointIntoImage(lcinfo, ps);

			ps.point.z = clusters[i].z;
			ps.point.x = clusters[i].x - CLUSTER_RADIUS;
			ps.point.y = clusters[i].y - CLUSTER_RADIUS;
			pp[4] = project3DPointIntoImage(lcinfo, ps);

			ps.point.x = clusters[i].x - CLUSTER_RADIUS;
			ps.point.y = clusters[i].y + CLUSTER_RADIUS;
			pp[5] = project3DPointIntoImage(lcinfo, ps);

			ps.point.x = clusters[i].x + CLUSTER_RADIUS;
			ps.point.y = clusters[i].y + CLUSTER_RADIUS;
			pp[6] = project3DPointIntoImage(lcinfo, ps);

			ps.point.x = clusters[i].x + CLUSTER_RADIUS;
			ps.point.y = clusters[i].y - CLUSTER_RADIUS;
			pp[7] = project3DPointIntoImage(lcinfo, ps);

//			cvLine(left, cvPoint(pp[0].x+0.5,pp[0].y+0.5), cvPoint(pp[1].x+0.5,pp[1].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[1].x+0.5,pp[1].y+0.5), cvPoint(pp[2].x+0.5,pp[2].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[2].x+0.5,pp[2].y+0.5), cvPoint(pp[3].x+0.5,pp[3].y+0.5), CV_RGB(0,255,0));
			cvLine(left, cvPoint(pp[3].x+0.5,pp[3].y+0.5), cvPoint(pp[0].x+0.5,pp[0].y+0.5), CV_RGB(0,255,0));

//			cvLine(left, cvPoint(pp[4].x+0.5,pp[4].y+0.5), cvPoint(pp[5].x+0.5,pp[5].y+0.5), CV_RGB(0,255,0));
			cvLine(left, cvPoint(pp[5].x+0.5,pp[5].y+0.5), cvPoint(pp[6].x+0.5,pp[6].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[6].x+0.5,pp[6].y+0.5), cvPoint(pp[7].x+0.5,pp[7].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[7].x+0.5,pp[7].y+0.5), cvPoint(pp[4].x+0.5,pp[4].y+0.5), CV_RGB(0,255,0));
//
//			cvLine(left, cvPoint(pp[0].x+0.5,pp[0].y+0.5), cvPoint(pp[4].x+0.5,pp[4].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[1].x+0.5,pp[1].y+0.5), cvPoint(pp[5].x+0.5,pp[5].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[2].x+0.5,pp[2].y+0.5), cvPoint(pp[6].x+0.5,pp[6].y+0.5), CV_RGB(0,255,0));
//			cvLine(left, cvPoint(pp[3].x+0.5,pp[3].y+0.5), cvPoint(pp[7].x+0.5,pp[7].y+0.5), CV_RGB(0,255,0));
			cvLine(left, cvPoint(pp[0].x+0.5,pp[0].y+0.5), cvPoint(pp[5].x+0.5,pp[5].y+0.5), CV_RGB(0,255,0));
			cvLine(left, cvPoint(pp[3].x+0.5,pp[3].y+0.5), cvPoint(pp[6].x+0.5,pp[6].y+0.5), CV_RGB(0,255,0));


			ps.point.x = clusters[i].x;
			ps.point.y = clusters[i].y;
			ps.point.z = clusters[i].z/2;

			geometry_msgs::Point center_pp = project3DPointIntoImage(lcinfo, ps);

			cvCircle(left, cvPoint(int(center_pp.x), int(center_pp.y)), 5, CV_RGB(0,255,0));

		}
	}


	void fitModels(const vector<sensor_msgs::PointCloud>& clouds, vector<tabletop_msgs::TableTopObject>& objects)
	{

		int count = 0;
		objects.resize(clouds.size());
		for (size_t k=0;k<clouds.size();++k) {

			if (display_) {
				// publish point cloud
				object_pub_.publish(clouds[k]);
			}

			tabletop_objects::ModelFit::Request req;
			tabletop_objects::ModelFit::Response resp;

			req.cloud = clouds[k];
			if (model_fit_service_.call(req, resp)) {
				ROS_INFO("Service call succeeded");

				// TODO: change this to real-world average distance error
				if (resp.score<5.0) {
					objects[count++] = resp.object;
				}
			}
			else {
				ROS_INFO("Service call failed");
			}
		}

		objects.resize(count);
	}


	void findTableTopObjectPoses(vector<tabletop_msgs::TableTopObject>& objects)
	{
		// find the table plane
		vector<double> plane;
		sensor_msgs::PointCloud objects_pc;
		sensor_msgs::PointCloud plane_pc;
		filterTablePlane(*cloud, plane,objects_pc,plane_pc);

		// project outliers (the objects on the table) to the table plane
		sensor_msgs::PointCloud projected_objects;
		projectToPlane(objects_pc, plane, projected_objects);


		// add table frame to tf
		geometry_msgs::PointStamped table_point;
		table_point.header.frame_id = projected_objects.header.frame_id;
		table_point.header.stamp = projected_objects.header.stamp;
		table_point.point.x = projected_objects.points[0].x;
		table_point.point.y = projected_objects.points[0].y;
		table_point.point.z = projected_objects.points[0].z;
		addTableFrame(table_point,plane);

		sleep(1);
		for (int i=0;i<1000;++i) {
			ros::spinOnce();
		}

/*	        if (!tf_.canTransform("table_frame", objects_pc.header.frame_id, objects_pc.header.stamp, ros::Duration(5.0))){
	          ROS_ERROR("Cannot transform from %s to %s",  objects_pc.header.frame_id.c_str(), "table_frame");
	          return;
	        }
*/

		ROS_INFO("Calling transformCloud with stamp: %0.15f", objects_pc.header.stamp.toSec());
		// transform all the objects into the table frame
		sensor_msgs::PointCloud objects_table_frame;
		tf_.transformPointCloud("table_frame", objects_pc, objects_table_frame);

		if (display_) {
			drawTableBBox(objects_table_frame);
		}

		// cluster objects
		vector<geometry_msgs::Point32> centers;
		vector<sensor_msgs::PointCloud> clouds;
		findTabletopClusters(objects_table_frame, centers, clouds);

		fitModels(clouds, objects);

		objects_pub_.publish(objects_table_frame);
	}


	void findObjectPositionsFromStereo(vector<CvPoint>& locations, vector<float>& scales)
	{
		// find the table plane
		vector<double> plane;
		sensor_msgs::PointCloud objects_pc;
		sensor_msgs::PointCloud plane_pc;
		filterTablePlane(*cloud, plane,objects_pc,plane_pc);

		// project outliers (the object on teh table) to the table plane
		sensor_msgs::PointCloud projected_objects;
		projectToPlane(objects_pc, plane, projected_objects);


		// add table frame to tf
		geometry_msgs::PointStamped table_point;
		table_point.header.frame_id = projected_objects.header.frame_id;
		table_point.header.stamp = projected_objects.header.stamp;
		table_point.point.x = projected_objects.points[0].x;
		table_point.point.y = projected_objects.points[0].y;
		table_point.point.z = projected_objects.points[0].z;
		addTableFrame(table_point,plane);

		// transform all the objects into the table frame
		sensor_msgs::PointCloud objects_table_frame;
		tf_.transformPointCloud("table_frame", objects_pc, objects_table_frame);

		if (display_) {
			drawTableBBox(objects_table_frame);
		}

		// cluster objects
		vector<geometry_msgs::Point32> centers;
		vector<sensor_msgs::PointCloud> objects;
		findTabletopClusters(objects_table_frame, centers, objects);

//		vector<geometry_msgs::PoseStamped> poses;
//		fitModels(objects, poses);


		objects_pub_.publish(objects_table_frame);


		// reproject bboxes in image
//		projectClusters(objects_table_frame, clusters);

		const sensor_msgs::CameraInfo& lcinfo = *lcinfo_;

		locations.resize(centers.size());
		scales.resize(centers.size());
		for (size_t i=0;i<centers.size();++i) {
			geometry_msgs::PointStamped ps;
			ps.header.frame_id = objects_table_frame.header.frame_id;
			ps.header.stamp = objects_table_frame.header.stamp;

			// compute location
			ps.point.x = centers[i].x;
			ps.point.y = centers[i].y;
			ps.point.z = centers[i].z/2;
			geometry_msgs::Point pp = project3DPointIntoImage(lcinfo, ps);

			locations[i].x = int(pp.x);
			locations[i].y = int(pp.y);

			// compute scale
			ps.point.z = 0;
			geometry_msgs::Point pp1 = project3DPointIntoImage(lcinfo, ps);
			ps.point.z = centers[i].z;
			geometry_msgs::Point pp2 = project3DPointIntoImage(lcinfo, ps);

			float dist = sqrt(dist2D(pp1,pp2));
			scales[i] = dist/centers[i].z;  // pixels per meter

//			printf("Pixel height: %f\n", dist);
//			printf("Real height: %f\n", centers[i].z);
//			printf("Scale: %f\n", scales[i]);

//			cvCircle(left, locations[i], 5, CV_RGB(0,255,0));
		}


	}



	void runtTabletopDetection()
	{
		// acquire cv_mutex lock
		//        boost::unique_lock<boost::mutex> images_lock(cv_mutex);

		// goes to sleep until some images arrive
		//        images_ready.wait(images_lock);
		//        printf("Woke up, processing images\n");

		vector<CvPoint> positions;
		vector<float> scales;
		findObjectPositionsFromStereo(positions, scales);

		ROS_INFO("runTabletopDetection done");
//		doChamferMatching(left, positions, scales);

	}


public:

	void serviceThread()
	{
		ROS_INFO_STREAM("Starting thread " << boost::this_thread::get_id());
		ros::NodeHandle n;

		ros::Rate r(10);
		while (n.ok()) {
			service_queue_.callAvailable();
			r.sleep();
		}
	}



	/**
	* Needed for OpenCV event loop, to show images
	* @return
	*/
	bool spin()
	{
		while (nh_.ok())
		{
			int key = cvWaitKey(100)&0x00FF;
			if(key == 27) //ESC
				break;

			ros::spinOnce();
		}

		return true;
	}


};


int main(int argc, char **argv)
{
	ros::init(argc, argv, "tabletop_detector");

	TabletopDetector node;
	node.spin();

	return 0;
}

