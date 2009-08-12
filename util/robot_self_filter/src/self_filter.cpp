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

#include <ros/ros.h>
#include "robot_self_filter/self_mask.h"
#include <tf/message_notifier.h>
#include <sstream>
    
class SelfFilter
{
public:

    SelfFilter(void)
    {
	nh_.param<std::string>("~sensor_frame", sensor_frame_, std::string());

	std::vector<std::string> links;	
	std::string link_names;
	nh_.param<std::string>("~self_see_links", link_names, std::string());
	std::stringstream ss(link_names);
	while (ss.good() && !ss.eof())
	{
	    std::string link;
	    ss >> link;
	    links.push_back(link);
	}
	double padd;
	nh_.param<double>("~self_see_padd", padd, 0.0);
	double scale;
	nh_.param<double>("~self_see_scale", scale, 1.0);
	
	sf_ = new robot_self_filter::SelfMask(tf_, links, scale, padd);
	
	std::vector<std::string> frames;
	sf_->getLinkNames(frames);
	if (!sensor_frame_.empty())
	    frames.push_back(sensor_frame_);

	mn_ = new tf::MessageNotifier<sensor_msgs::PointCloud>(tf_, boost::bind(&SelfFilter::cloudCallback, this, _1), "cloud_in", "", 1);
	mn_->setTargetFrame(frames);
	
	nh_.param<std::string>("~annotate", annotate_, std::string());
	pointCloudPublisher_ = nh_.advertise<sensor_msgs::PointCloud>("cloud_out", 1);
	if (!annotate_.empty())
	    ROS_INFO("Self filter is adding annotation channel '%s'", annotate_.c_str());
	if (!sensor_frame_.empty())
	    ROS_INFO("Self filter is removing shadow points for sensor in frame '%s'", sensor_frame_.c_str());
    }
    
    ~SelfFilter(void)
    {
	if (mn_)
	    delete mn_;
	if (sf_)
	    delete sf_;
    }
    
private:
    
    void cloudCallback(const sensor_msgs::PointCloudConstPtr &cloud)
    {
	sensor_msgs::PointCloud out;

	ROS_DEBUG("Got pointcloud that is %f seconds old", (ros::Time::now() - cloud->header.stamp).toSec());
	std::vector<int> mask;
	ros::WallTime tm = ros::WallTime::now();
	
	if (sensor_frame_.empty())
	    sf_->maskContainment(*cloud, mask);
	else
	    sf_->maskIntersection(*cloud, sensor_frame_, mask);
	
	double sec = (ros::WallTime::now() - tm).toSec();

	fillResult(*cloud, mask, out);
	pointCloudPublisher_.publish(out);
	ROS_DEBUG("Self filter: reduced %d points to %d points in %f seconds", (int)cloud->points.size(), (int)out.points.size(), sec);
    }

    void fillResult(const sensor_msgs::PointCloud& data_in, const std::vector<int> &keep, sensor_msgs::PointCloud& data_out)
    {
	const unsigned int np = data_in.points.size();

	// fill in output data with points that are NOT on the robot
	data_out.header = data_in.header;	  
	
	data_out.points.resize(0);
	data_out.points.reserve(np);
	
	data_out.channels.resize(data_in.channels.size());
	for (unsigned int i = 0 ; i < data_out.channels.size() ; ++i)
	{
	    ROS_ASSERT(data_in.channels[i].values.size() == data_in.points.size());
	    data_out.channels[i].name = data_in.channels[i].name;
	    data_out.channels[i].values.reserve(data_in.channels[i].values.size());
	}
	
	int c = -1;
	if (!annotate_.empty())
	{
	    // add annotation for points
	    for (unsigned int i = 0 ; i < data_out.channels.size() ; ++i)
		if (data_out.channels[i].name == annotate_)
		{
		    c = i;
		    break;
		}
	    if (c < 0)
	    {
		c = data_out.channels.size();
		data_out.channels.resize(c + 1);
		data_out.channels[c].name = annotate_;
	    }
	    data_out.channels[c].values.reserve(np);
	}

	for (unsigned int i = 0 ; i < np ; ++i)
	    if (annotate_.empty())
	    {
		if (keep[i] == robot_self_filter::OUTSIDE)
		{
		    data_out.points.push_back(data_in.points[i]);
		    for (unsigned int j = 0 ; j < data_out.channels.size() ; ++j)
			data_out.channels[j].values.push_back(data_in.channels[j].values[i]);
		}
	    }
	    else
	    {
		data_out.points.push_back(data_in.points[i]);
		for (unsigned int j = 0 ; j < data_out.channels.size() ; ++j)
		{
		    if ((int)j == c)
		    {
			float flag = 0.0;
			if (keep[i] != robot_self_filter::SHADOW)
			    flag = keep[i] == robot_self_filter::OUTSIDE ? 1.0f : -1.0f;			
			data_out.channels[c].values.push_back(flag);
		    }
		    else
			data_out.channels[j].values.push_back(data_in.channels[j].values[i]);
		}
	    }

    }
    
    tf::TransformListener                         tf_;
    robot_self_filter::SelfMask                  *sf_;
    tf::MessageNotifier<sensor_msgs::PointCloud> *mn_;
    ros::Publisher                                pointCloudPublisher_;
    std::string                                   sensor_frame_;
    ros::NodeHandle                               nh_;
    std::string                                   annotate_;
};

    
int main(int argc, char **argv)
{
    ros::init(argc, argv, "self_filter", ros::init_options::AnonymousName);

    SelfFilter s;
    ros::spin();
    
    return 0;
}
