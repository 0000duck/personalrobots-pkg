/*
 *  Gazebo - Outdoor Multi-Robot Simulator
 *  Copyright (C) 2003
 *     Nate Koenig & Andrew Howard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*
 @mainpage
   Desc: RosProsilica plugin for simulating cameras in Gazebo
   Author: John Hsu
   Date: 24 Sept 2008
   SVN info: $Id$
 @htmlinclude manifest.html
 @b RosProsilica plugin mimics after prosilica_cam package
 */

#include <algorithm>
#include <assert.h>

#include <gazebo_plugin/ros_prosilica.h>

#include <gazebo/Sensor.hh>
#include <gazebo/Model.hh>
#include <gazebo/Global.hh>
#include <gazebo/XMLConfig.hh>
#include <gazebo/Simulator.hh>
#include <gazebo/gazebo.h>
#include <gazebo/GazeboError.hh>
#include <gazebo/ControllerFactory.hh>
#include "MonoCameraSensor.hh"


#include <image_msgs/Image.h>
#include <image_msgs/CamInfo.h>
#include <image_msgs/FillImage.h>
#include <opencv_latest/CvBridge.h>
#include <diagnostic_updater/diagnostic_updater.h>

#include <cv.h>
#include <cvwimage.h>

#include <boost/scoped_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/thread.hpp>
#include <string>

#include "prosilica/prosilica.h"
#include "prosilica_cam/PolledImage.h"
#include "prosilica_cam/CamInfo.h"

using namespace gazebo;

GZ_REGISTER_DYNAMIC_CONTROLLER("ros_prosilica", RosProsilica);

////////////////////////////////////////////////////////////////////////////////
// Constructor
RosProsilica::RosProsilica(Entity *parent)
    : Controller(parent)
{
  this->myParent = dynamic_cast<MonoCameraSensor*>(this->parent);

  if (!this->myParent)
    gzthrow("RosProsilica controller requires a Camera Sensor as its parent");

  Param::Begin(&this->parameters);
  this->topicNameP = new ParamT<std::string>("topicName","stereo/raw_stereo", 0);
  this->frameNameP = new ParamT<std::string>("frameName","stereo_link", 0);
  Param::End();

  rosnode = ros::g_node; // comes from where?
  int argc = 0;
  char** argv = NULL;
  if (rosnode == NULL)
  {
    ros::init(argc,argv);
    rosnode = new ros::Node("ros_gazebo",ros::Node::DONT_HANDLE_SIGINT);
    ROS_DEBUG("Starting node in prosilica");
  }

}

////////////////////////////////////////////////////////////////////////////////
// Destructor
RosProsilica::~RosProsilica()
{
  delete this->topicNameP;
  delete this->frameNameP;
}

////////////////////////////////////////////////////////////////////////////////
// Load the controller
void RosProsilica::LoadChild(XMLConfigNode *node)
{
  this->topicNameP->Load(node);
  this->frameNameP->Load(node);
  
  this->topicName = this->topicNameP->GetValue();
  this->frameName = this->frameNameP->GetValue();

  ROS_DEBUG("prosilica topic name %s", this->topicName.c_str());
  rosnode->advertise<image_msgs::Image>(this->topicName,1);
}

////////////////////////////////////////////////////////////////////////////////
// Initialize the controller
void RosProsilica::InitChild()
{

  // set parent sensor to active automatically
  this->myParent->SetActive(true);

  // set buffer size
  this->width            = this->myParent->GetImageWidth();
  this->height           = this->myParent->GetImageHeight();
  this->depth            = 1;

}

////////////////////////////////////////////////////////////////////////////////
// Update the controller
void RosProsilica::UpdateChild()
{

  // do this first so there's chance for sensor to run 1 frame after activate
  if (this->myParent->IsActive())
    this->PutCameraData();
  else
    this->myParent->SetActive(true); // as long as this plugin is running, parent is active

}

////////////////////////////////////////////////////////////////////////////////
// Finalize the controller
void RosProsilica::FiniChild()
{
  rosnode->unadvertise(this->topicName);
  this->myParent->SetActive(false);
}

////////////////////////////////////////////////////////////////////////////////
// Put laser data to the interface
void RosProsilica::PutCameraData()
{
  const unsigned char *src;

  // Get a pointer to image data
  src = this->myParent->GetImageData(0);

  //std::cout << " updating prosilica " << this->topicName << " " << data->image_size << std::endl;
  if (src)
  {
    //double tmpT0 = Simulator::Instance()->GetWallTime();

    this->lock.lock();
    // copy data into image
    this->imageMsg.header.frame_id = this->frameName;
    this->imageMsg.header.stamp.sec = (unsigned long)floor(Simulator::Instance()->GetSimTime());
    this->imageMsg.header.stamp.nsec = (unsigned long)floor(  1e9 * (  Simulator::Instance()->GetSimTime() - this->imageMsg.header.stamp.sec) );

    //double tmpT1 = Simulator::Instance()->GetWallTime();
    //double tmpT2;

    /// @todo: don't bother if there are no subscribers
    if (this->rosnode->numSubscribers(this->topicName) > 0)
    {
      // copy from src to imageMsg
      fillImage(this->imageMsg   ,"image_raw" ,
                this->height     ,this->width ,this->depth,
                "mono"            ,"uint8"     ,
                (void*)src );

      //tmpT2 = Simulator::Instance()->GetWallTime();

      // publish to ros
      rosnode->publish(this->topicName,this->imageMsg);
    }

    //double tmpT3 = Simulator::Instance()->GetWallTime();

    this->lock.unlock();
  }

}

