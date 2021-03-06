/*
 * Copyright (c) 2008, Willow Garage, Inc.
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
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
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
 */

#ifndef FILTERS_SELF_SEE_H_
#define FILTERS_SELF_SEE_H_

#include <filters/filter_base.h>
#include <robot_self_filter/self_mask.h>
#include <ros/console.h>

namespace filters
{

/** \brief A filter to remove parts of the robot seen in a pointcloud
 *
 */

template <typename T>
class SelfFilter: public FilterBase <T>
{
    
public:

    /** \brief Construct the filter */
    SelfFilter(void) : sm_(tf_)
    {
    }
    
    /** \brief Destructor to clean up
     */
    virtual ~SelfFilter(void)
    {
    }
    
    virtual bool configure(void)
    {
	// keep only the points that are outside of the robot
	// for testing purposes this may be changed to true
	nh_.param("~invert", invert_, false);
	
	if (invert_)
	    ROS_INFO("Inverting filter output");
	
	return sm_.configure();
    }
    
    
    /** \brief Update the filter and return the data seperately
     * \param data_in T array with length width
     * \param data_out T array with length width
     */
    virtual bool update(const sensor_msgs::PointCloud& data_in, sensor_msgs::PointCloud& data_out)
    {
	std::vector<bool> keep(data_in.points.size());	
	sm_.maskContainment(data_in, keep);
	fillResult(data_in, keep, data_out);
	return true;
    }

    void fillResult(const sensor_msgs::PointCloud& data_in, const std::vector<bool> &keep, sensor_msgs::PointCloud& data_out)
    {
	const unsigned int np = data_in.points.size();
	
	// fill in output data 
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
	
	for (unsigned int i = 0 ; i < np ; ++i)
	    if ((keep[i] && !invert_) || (!keep[i] && invert_))
	    {
		data_out.points.push_back(data_in.points[i]);
		for (unsigned int j = 0 ; j < data_out.channels.size() ; ++j)
		    data_out.channels[j].values.push_back(data_in.channels[j].values[i]);
	    }
    }
    
    virtual bool update(const std::vector<sensor_msgs::PointCloud> & data_in, std::vector<sensor_msgs::PointCloud>& data_out)
    {
	bool result = true;
	data_out.resize(data_in.size());
	for (unsigned int i = 0 ; i < data_in.size() ; ++i)
	    if (!update(data_in[i], data_out[i]))
		result = false;
	return true;
    }
    
protected:
    
    tf::TransformListener       tf_;
    robot_self_filter::SelfMask sm_;
    
    ros::NodeHandle             nh_;
    bool                        invert_;
        
};
    
    typedef sensor_msgs::PointCloud sensor_msgs_PointCloud;
    FILTERS_REGISTER_FILTER(SelfFilter, sensor_msgs_PointCloud);
    
}

#endif //#ifndef FILTERS_SELF_SEE_H_
