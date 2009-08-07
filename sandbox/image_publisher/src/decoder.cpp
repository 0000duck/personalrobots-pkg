/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2009, Willow Garage, Inc.
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

#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CompressedImage.h>
#include <opencv_latest/CvBridge.h>
#include <opencv/cvwimage.h>
#include <opencv/highgui.h>

ros::Publisher g_decompressed_pub;

void imageCB(const sensor_msgs::CompressedImageConstPtr &msg)
{
  const CvMat compressed = cvMat(1, msg->data.size(), CV_8UC1,
                                 const_cast<unsigned char*>(&msg->data[0]));
  cv::WImageBuffer_b decompressed( cvDecodeImage(&compressed, CV_LOAD_IMAGE_ANYCOLOR) );
  sensor_msgs::Image image;
  if ( sensor_msgs::CvBridge::fromIpltoRosImage(decompressed.Ipl(), image) ) {
    image.header = msg->header;
    if (decompressed.Channels() == 1) {
      image.encoding = "mono";
    }
    else if (decompressed.Channels() == 3) {
      image.encoding = "rgb";
    }
    else {
      ROS_ERROR("Unsupported number of channels: %i", decompressed.Channels());
      return;
    }
    g_decompressed_pub.publish(image);
  }
  else {
    ROS_ERROR("Unable to create image message");
  }
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "decoder", ros::init_options::AnonymousName);
  ros::NodeHandle n;
  g_decompressed_pub = n.advertise<sensor_msgs::Image>("decompressed", 1);
  ros::Subscriber compressed_sub = n.subscribe("compressed", 1, &imageCB);

  ros::spin();

  return 0;
}
