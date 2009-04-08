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

// TODO: doxygen mainpage

#include <ros/node.h>
#include <image_msgs/Image.h>
#include <image_msgs/CamInfo.h>
#include <image_msgs/FillImage.h>
#include <diagnostic_updater/diagnostic_updater.h>

#include "pr2lib.h"
#include "host_netutil.h"

static const double MODE_FPS[] = {15, 12.5, 30, 25, 15, 12.5, 60, 50, 30, 25};

class ForearmNode
{
private:
  ros::Node &node_;
  IpCamList* camera_;
  image_msgs::Image image_;

  int video_mode_;
  int width_;
  int height_;
  double desired_freq_;
  bool started_video_;

  DiagnosticUpdater<ForearmNode> diagnostic_;
  int count_;

public:
  ForearmNode(ros::Node &node)
    : node_(node), camera_(NULL), started_video_(false),
      diagnostic_(this, &node_), count_(0)
  {
    // Read parameters
    std::string if_name;
    node_.param("~if_name", if_name, std::string("eth0"));

    std::string ip_address;
    if (node_.hasParam("~ip_address"))
      node_.getParam("~ip_address", ip_address);
    else {
      ROS_FATAL("IP address not specified");
      node_.shutdown();
      return;
    }

    int port;
    node_.param("~port", port, 9090);

    std::string mode_name;
    node_.param("~video_mode", mode_name, std::string("752x480x15"));
    if (mode_name.compare("752x480x15") == 0)
      video_mode_ = MT9VMODE_752x480x15b1;
    else if (mode_name.compare("752x480x12.5") == 0)
      video_mode_ = MT9VMODE_752x480x12_5b1;
    else if (mode_name.compare("640x480x30") == 0)
      video_mode_ = MT9VMODE_640x480x30b1;
    else if (mode_name.compare("640x480x25") == 0)
      video_mode_ = MT9VMODE_640x480x25b1;
    else if (mode_name.compare("640x480x15") == 0)
      video_mode_ = MT9VMODE_640x480x15b1;
    else if (mode_name.compare("640x480x12.5") == 0)
      video_mode_ = MT9VMODE_640x480x12_5b1;
    else if (mode_name.compare("320x240x60") == 0)
      video_mode_ = MT9VMODE_320x240x60b2;
    else if (mode_name.compare("320x240x50") == 0)
      video_mode_ = MT9VMODE_320x240x50b2;
    else if (mode_name.compare("320x240x30") == 0)
      video_mode_ = MT9VMODE_320x240x30b2;
    else if (mode_name.compare("320x240x25") == 0)
      video_mode_ = MT9VMODE_320x240x25b2;
    else {
      ROS_FATAL("Unknown video mode %s", mode_name.c_str());
      node_.shutdown();
      return;
    }

    if (video_mode_ <= MT9VMODE_752x480x12_5b1)
      width_ = 752;
    else if (video_mode_ <= MT9VMODE_640x480x12_5b1)
      width_ = 640;
    else
      width_ = 320;
    height_ = (video_mode_ <= MT9VMODE_640x480x12_5b1) ? 480 : 240;
    desired_freq_ = MODE_FPS[ video_mode_ ];

    diagnostic_.addUpdater( &ForearmNode::freqStatus );
    
    // Configure camera
    configure(if_name, ip_address, port);
  }

  ~ForearmNode()
  {
    // Stop video
    if ( started_video_ && pr2StopVid(camera_) != 0 )
      ROS_ERROR("Video Stop error");
  }

  void configure(const std::string &if_name, const std::string &ip_address, int port)
  {
    // Create a new IpCamList to hold the camera list
    IpCamList camList;
    pr2CamListInit(&camList);

    // Discover any connected cameras, wait for 0.5 second for replies
    if( pr2Discover(if_name.c_str(), &camList, SEC_TO_USEC(0.5)) == -1) {
      ROS_FATAL("Discover error");
      node_.shutdown();
      return;
    }

    if (pr2CamListNumEntries(&camList) == 0) {
      ROS_FATAL("No cameras found");
      node_.shutdown();
      return;
    }

    // Open camera with requested serial number, or first camera found if none requested
    int index = 0;
    if (node_.hasParam("~serial_number")) {
      int sn;
      node_.getParam("~serial_number", sn);
      index = pr2CamListFind(&camList, sn);
      if (index == -1) {
        ROS_FATAL("Couldn't find camera with S/N %i", sn);
        node_.shutdown();
        return;
      }
    }
    camera_ = pr2CamListGetEntry(&camList, index);

    // Configure the camera with its IP address, wait up to 500ms for completion
    int retval = pr2Configure(camera_, ip_address.c_str(), SEC_TO_USEC(0.5));
    if (retval != 0) {
      if (retval == ERR_CONFIG_ARPFAIL) {
        ROS_WARN("Unable to create ARP entry (are you root?), continuing anyway");
      } else {
        ROS_FATAL("IP address configuration failed");
        node_.shutdown();
        return;
      }
    }
    ROS_INFO("Configured camera #%d, S/N #%u, IP address %s", 0,
             camera_->serial, ip_address.c_str());

    // We are going to receive the video on this host, so we need our own MAC address
    sockaddr localMac;
    if ( wgEthGetLocalMac(camera_->ifName, &localMac) != 0 ) {
      ROS_FATAL("Unable to get local MAC address for interface %s", camera_->ifName);
      node_.shutdown();
      return;
    }

    // We also need our local IP address
    in_addr localIp;
    if ( wgIpGetLocalAddr(camera_->ifName, &localIp) != 0) {
      ROS_FATAL("Unable to get local IP address for interface %s", camera_->ifName);
      node_.shutdown();
      return;
    }

    // Select a video mode
    if ( pr2ImagerModeSelect( camera_, video_mode_ ) != 0) {
      ROS_FATAL("Mode select error");
      node_.shutdown();
      return;
    }

    // Start video; send it to specified host port
    if ( pr2StartVid( camera_, (uint8_t *)&(localMac.sa_data[0]),
                      inet_ntoa(localIp), port) != 0 ) {
      ROS_FATAL("Video start error");
      node_.shutdown();
      return;
    }
    started_video_ = true;

    // Receive frames through callback
    // TODO: start this in separate thread?
    node_.advertise<image_msgs::Image>("~image_raw", 1);
    pr2VidReceive( camera_->ifName, port, height_, width_,
                   &ForearmNode::frameHandler, this );
  }

  void freqStatus(robot_msgs::DiagnosticStatus& status)
  {
    status.name = "Frequency Status";

    double freq = (double)(count_)/diagnostic_.getPeriod();

    if (freq < (.9*desired_freq_))
    {
      status.level = 2;
      status.message = "Desired frequency not met";
    }
    else
    {
      status.level = 0;
      status.message = "Desired frequency met";
    }

    status.set_values_size(3);
    status.values[0].label = "Images in interval";
    status.values[0].value = count_;
    status.values[1].label = "Desired frequency";
    status.values[1].value = desired_freq_;
    status.values[2].label = "Actual frequency";
    status.values[2].value = freq;

    count_ = 0;
  }

private:
  void publishImage(size_t width, size_t height, uint8_t *frameData)
  {
    fillImage(image_, "image", height, width, 1, "bayer_bggr", "uint8", frameData);
    node_.publish("~image_raw", image_);
  }
  
  static int frameHandler(size_t width, size_t height, uint8_t *frameData,
                          PacketEOF *eofInfo, void *userData)
  {
    ForearmNode &fa_node = *(ForearmNode*)userData;
    if (!fa_node.node_.ok())
      return 1;
    
    if (eofInfo == NULL) {
      ROS_WARN("Frame was missing EOF, no frame information available");
      return 0;
    }

    // Check for short packet (video lines were missing)
    if (eofInfo->header.line_number == IMAGER_LINENO_SHORT) {
      ROS_WARN("Short frame (video lines were missing)");
      return 0;
    }

    fa_node.publishImage(width, height, frameData);
    fa_node.count_++;
    fa_node.diagnostic_.update();

    return 0;
  }
};

int main(int argc, char **argv)
{
  ros::init(argc, argv);
  ros::Node n("forearm_node");
  ForearmNode fn(n);
  //n.spin();

  return 0;
}
