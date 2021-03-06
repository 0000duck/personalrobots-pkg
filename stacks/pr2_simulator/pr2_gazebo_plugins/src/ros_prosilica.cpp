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
 @b RosProsilica plugin mimics after prosilica_camera package
 */

#include <algorithm>
#include <assert.h>

#include <pr2_gazebo_plugins/ros_prosilica.h>

#include <gazebo/Sensor.hh>
#include <gazebo/Model.hh>
#include <gazebo/Global.hh>
#include <gazebo/XMLConfig.hh>
#include <gazebo/Simulator.hh>
#include <gazebo/gazebo.h>
#include <gazebo/GazeboError.hh>
#include <gazebo/ControllerFactory.hh>
#include "gazebo/MonoCameraSensor.hh"


#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/fill_image.h>
#include <diagnostic_updater/diagnostic_updater.h>

#include <opencv_latest/CvBridge.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <cv.h>
#include <cvwimage.h>

#include <boost/scoped_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <string>

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
  this->imageTopicNameP = new ParamT<std::string>("imageTopicName","~image", 0);
  this->imageRectTopicNameP = new ParamT<std::string>("imageRectTopicName","~image_rect", 0);
  this->camInfoTopicNameP = new ParamT<std::string>("camInfoTopicName","~cam_info", 0);
  this->camInfoServiceNameP = new ParamT<std::string>("camInfoServiceName","~cam_info_service", 0);
  this->pollServiceNameP = new ParamT<std::string>("pollServiceName","~poll", 0);
  this->frameNameP = new ParamT<std::string>("frameName","prosilica_optical_frame", 0);
  // camera parameters 
  this->CxPrimeP = new ParamT<double>("CxPrime",320, 0); // for 640x480 image
  this->CxP  = new ParamT<double>("Cx" ,320, 0); // for 640x480 image
  this->CyP  = new ParamT<double>("Cy" ,240, 0); // for 640x480 image
  this->focal_lengthP  = new ParamT<double>("focal_length" ,554.256, 0); // == image_width(px) / (2*tan( hfov(radian) /2))
  this->distortion_k1P  = new ParamT<double>("distortion_k1" ,0, 0);
  this->distortion_k2P  = new ParamT<double>("distortion_k2" ,0, 0);
  this->distortion_k3P  = new ParamT<double>("distortion_k3" ,0, 0);
  this->distortion_t1P  = new ParamT<double>("distortion_t1" ,0, 0);
  this->distortion_t2P  = new ParamT<double>("distortion_t2" ,0, 0);
  Param::End();

  int argc = 0;
  char** argv = NULL;
  ros::init(argc,argv,"gazebo");
  this->rosnode_ = new ros::NodeHandle();
}

////////////////////////////////////////////////////////////////////////////////
// Destructor
RosProsilica::~RosProsilica()
{
  delete this->rosnode_;
  delete this->imageTopicNameP;
  delete this->imageRectTopicNameP;
  delete this->camInfoTopicNameP;
  delete this->camInfoServiceNameP;
  delete this->pollServiceNameP;
  delete this->frameNameP;
  delete this->CxPrimeP;
  delete this->CxP;
  delete this->CyP;
  delete this->focal_lengthP;
  delete this->distortion_k1P;
  delete this->distortion_k2P;
  delete this->distortion_k3P;
  delete this->distortion_t1P;
  delete this->distortion_t2P;
}

////////////////////////////////////////////////////////////////////////////////
// Load the controller
void RosProsilica::LoadChild(XMLConfigNode *node)
{
  this->imageTopicNameP->Load(node);
  this->imageRectTopicNameP->Load(node);
  this->camInfoTopicNameP->Load(node);
  this->camInfoServiceNameP->Load(node);
  this->pollServiceNameP->Load(node);
  this->frameNameP->Load(node);
  this->CxPrimeP->Load(node);
  this->CxP->Load(node);
  this->CyP->Load(node);
  this->focal_lengthP->Load(node);
  this->distortion_k1P->Load(node);
  this->distortion_k2P->Load(node);
  this->distortion_k3P->Load(node);
  this->distortion_t1P->Load(node);
  this->distortion_t2P->Load(node);
  
  this->imageTopicName = this->imageTopicNameP->GetValue();
  this->imageRectTopicName = this->imageRectTopicNameP->GetValue();
  this->camInfoTopicName = this->camInfoTopicNameP->GetValue();
  this->camInfoServiceName = this->camInfoServiceNameP->GetValue();
  this->pollServiceName = this->pollServiceNameP->GetValue();
  this->frameName = this->frameNameP->GetValue();
  this->CxPrime = this->CxPrimeP->GetValue();
  this->Cx = this->CxP->GetValue();
  this->Cy = this->CyP->GetValue();
  this->focal_length = this->focal_lengthP->GetValue();
  this->distortion_k1 = this->distortion_k1P->GetValue();
  this->distortion_k2 = this->distortion_k2P->GetValue();
  this->distortion_k3 = this->distortion_k3P->GetValue();
  this->distortion_t1 = this->distortion_t1P->GetValue();
  this->distortion_t2 = this->distortion_t2P->GetValue();

  ROS_DEBUG("prosilica image topic name %s", this->imageTopicName.c_str());
  this->image_pub_ = this->rosnode_->advertise<sensor_msgs::Image>(this->imageTopicName,1);
  this->image_rect_pub_ = this->rosnode_->advertise<sensor_msgs::Image>(this->imageRectTopicName,1);
  this->cam_info_pub_ = this->rosnode_->advertise<sensor_msgs::CameraInfo>(this->camInfoTopicName,1);
  this->cam_info_ser_ = this->rosnode_->advertiseService(this->camInfoServiceName,&RosProsilica::camInfoService, this);
  this->poll_ser_ = this->rosnode_->advertiseService(this->pollServiceName,&RosProsilica::triggeredGrab, this);
}

////////////////////////////////////////////////////////////////////////////////
// service call to return camera info
bool RosProsilica::camInfoService(prosilica_camera::CameraInfo::Request &req,
                                  prosilica_camera::CameraInfo::Response &res)
{
  // should return the cam info for the entire camera frame
  this->camInfoMsg = &res.cam_info;
  // fill CameraInfo
  this->camInfoMsg->header.frame_id = this->frameName;
  this->camInfoMsg->header.stamp    = ros::Time((unsigned long)floor(Simulator::Instance()->GetSimTime()));
  this->camInfoMsg->height = this->myParent->GetImageHeight();
  this->camInfoMsg->width  = this->myParent->GetImageWidth() ;
  // distortion
  this->camInfoMsg->D[0] = 0.0;
  this->camInfoMsg->D[1] = 0.0;
  this->camInfoMsg->D[2] = 0.0;
  this->camInfoMsg->D[3] = 0.0;
  this->camInfoMsg->D[4] = 0.0;
  // original camera matrix
  this->camInfoMsg->K[0] = this->focal_length;
  this->camInfoMsg->K[1] = 0.0;
  this->camInfoMsg->K[2] = this->Cx;
  this->camInfoMsg->K[3] = 0.0;
  this->camInfoMsg->K[4] = this->focal_length;
  this->camInfoMsg->K[5] = this->Cy;
  this->camInfoMsg->K[6] = 0.0;
  this->camInfoMsg->K[7] = 0.0;
  this->camInfoMsg->K[8] = 1.0;
  // rectification
  this->camInfoMsg->R[0] = 1.0;
  this->camInfoMsg->R[1] = 0.0;
  this->camInfoMsg->R[2] = 0.0;
  this->camInfoMsg->R[3] = 0.0;
  this->camInfoMsg->R[4] = 1.0;
  this->camInfoMsg->R[5] = 0.0;
  this->camInfoMsg->R[6] = 0.0;
  this->camInfoMsg->R[7] = 0.0;
  this->camInfoMsg->R[8] = 1.0;
  // camera projection matrix (same as camera matrix due to lack of distortion/rectification) (is this generated?)
  this->camInfoMsg->P[0] = this->focal_length;
  this->camInfoMsg->P[1] = 0.0;
  this->camInfoMsg->P[2] = this->Cx;
  this->camInfoMsg->P[3] = 0.0;
  this->camInfoMsg->P[4] = 0.0;
  this->camInfoMsg->P[5] = this->focal_length;
  this->camInfoMsg->P[6] = this->Cy;
  this->camInfoMsg->P[7] = 0.0;
  this->camInfoMsg->P[8] = 0.0;
  this->camInfoMsg->P[9] = 0.0;
  this->camInfoMsg->P[10] = 1.0;
  this->camInfoMsg->P[11] = 0.0;
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// service call to grab an image
bool RosProsilica::triggeredGrab(prosilica_camera::PolledImage::Request &req,
                                 prosilica_camera::PolledImage::Response &res)
{

  if (req.region_x <= 0 || req.region_y <= 0 || req.width <= 0 || req.height <= 0)
  {
    req.region_x = 0;
    req.region_y = 0;
    req.width = this->width;
    req.height = this->height;
  }
  boost::recursive_mutex::scoped_lock lock(*Simulator::Instance()->GetMRMutex());

  const unsigned char *src;

  // Get a pointer to image data
  src = this->myParent->GetImageData(0);

  if (src)
  {
    this->lock.lock();

    // fill CameraInfo
    this->roiCameraInfoMsg = &res.cam_info;
    this->roiCameraInfoMsg->header.frame_id = this->frameName;
    this->roiCameraInfoMsg->header.stamp    = ros::Time((unsigned long)floor(Simulator::Instance()->GetSimTime()));
    this->roiCameraInfoMsg->width  = req.width; //this->myParent->GetImageWidth() ;
    this->roiCameraInfoMsg->height = req.height; //this->myParent->GetImageHeight();
    // distortion
    this->roiCameraInfoMsg->D[0] = 0.0;
    this->roiCameraInfoMsg->D[1] = 0.0;
    this->roiCameraInfoMsg->D[2] = 0.0;
    this->roiCameraInfoMsg->D[3] = 0.0;
    this->roiCameraInfoMsg->D[4] = 0.0;
    // original camera matrix
    this->roiCameraInfoMsg->K[0] = this->focal_length;
    this->roiCameraInfoMsg->K[1] = 0.0;
    this->roiCameraInfoMsg->K[2] = this->Cx - req.region_x;
    this->roiCameraInfoMsg->K[3] = 0.0;
    this->roiCameraInfoMsg->K[4] = this->focal_length;
    this->roiCameraInfoMsg->K[5] = this->Cy - req.region_y;
    this->roiCameraInfoMsg->K[6] = 0.0;
    this->roiCameraInfoMsg->K[7] = 0.0;
    this->roiCameraInfoMsg->K[8] = 1.0;
    // rectification
    this->roiCameraInfoMsg->R[0] = 1.0;
    this->roiCameraInfoMsg->R[1] = 0.0;
    this->roiCameraInfoMsg->R[2] = 0.0;
    this->roiCameraInfoMsg->R[3] = 0.0;
    this->roiCameraInfoMsg->R[4] = 1.0;
    this->roiCameraInfoMsg->R[5] = 0.0;
    this->roiCameraInfoMsg->R[6] = 0.0;
    this->roiCameraInfoMsg->R[7] = 0.0;
    this->roiCameraInfoMsg->R[8] = 1.0;
    // camera projection matrix (same as camera matrix due to lack of distortion/rectification) (is this generated?)
    this->roiCameraInfoMsg->P[0] = this->focal_length;
    this->roiCameraInfoMsg->P[1] = 0.0;
    this->roiCameraInfoMsg->P[2] = this->Cx - req.region_x;
    this->roiCameraInfoMsg->P[3] = 0.0;
    this->roiCameraInfoMsg->P[4] = 0.0;
    this->roiCameraInfoMsg->P[5] = this->focal_length;
    this->roiCameraInfoMsg->P[6] = this->Cy - req.region_y;
    this->roiCameraInfoMsg->P[7] = 0.0;
    this->roiCameraInfoMsg->P[8] = 0.0;
    this->roiCameraInfoMsg->P[9] = 0.0;
    this->roiCameraInfoMsg->P[10] = 1.0;
    this->roiCameraInfoMsg->P[11] = 0.0;


    // copy data into image
    this->imageMsg.header.frame_id    = this->frameName;
    this->imageMsg.header.stamp       = ros::Time((unsigned long)floor(Simulator::Instance()->GetSimTime()));

    // copy data into ROI image
    this->roiImageMsg = &res.image;
    this->roiImageMsg->header.frame_id = this->frameName;
    this->roiImageMsg->header.stamp    = ros::Time((unsigned long)floor(Simulator::Instance()->GetSimTime()));

    // copy from src to imageMsg
    fillImage(this->imageMsg,
              this->type,
              this->height,
              this->width,
              this->skip*this->width,
              (void*)src );
    // publish to ros, thumbnails and rect image?
    /// @todo: don't bother if there are no subscribers
    //if (this->image_pub_.getNumSubscribers() > 0)
      this->image_pub_.publish(this->imageMsg);
    //if (this->image_rect_pub_.getNumSubscribers() > 0)
      this->image_rect_pub_.publish(this->imageMsg);

    //sensor_msgs::CvBridge img_bridge_(&this->imageMsg);
    //IplImage* cv_image;
    //img_bridge_.to_cv( &cv_image );

    sensor_msgs::CvBridge img_bridge_;
    img_bridge_.fromImage(this->imageMsg);

    //cvNamedWindow("showme",CV_WINDOW_AUTOSIZE);
    //cvSetMouseCallback("showme", &RosProsilica::mouse_cb, this);
    //cvStartWindowThread();

    //cvShowImage("showme",img_bridge_.toIpl());

    cvSetImageROI(img_bridge_.toIpl(),cvRect(req.region_x,req.region_y,req.width,req.height));
    IplImage *roi = cvCreateImage(cvSize(req.width,req.height),
                                 img_bridge_.toIpl()->depth,
                                 img_bridge_.toIpl()->nChannels);
    cvCopy(img_bridge_.toIpl(),roi);

    img_bridge_.fromIpltoRosImage(roi,*this->roiImageMsg);

    cvReleaseImage(&roi);


    this->lock.unlock();
  }

  return true;


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
  this->depth            = this->myParent->GetImageDepth();
  //ROS_INFO("image format in urdf is %s\n",this->myParent->GetImageFormat().c_str());
  if (this->myParent->GetImageFormat() == "L8")
  {
    this->type = sensor_msgs::image_encodings::MONO8;
    this->skip = 1;
  }
  else if (this->myParent->GetImageFormat() == "R8G8B8")
  {
    this->type = sensor_msgs::image_encodings::RGB8;
    this->skip = 3;
  }
  else if (this->myParent->GetImageFormat() == "B8G8R8")
  {
    this->type = sensor_msgs::image_encodings::BGR8;
    this->skip = 3;
  }
  else
  {
    ROS_ERROR("Unsupported Gazebo ImageFormat\n");
    this->type = sensor_msgs::image_encodings::BGR8;
    this->skip = 3;
  }


}

////////////////////////////////////////////////////////////////////////////////
// Update the controller
void RosProsilica::UpdateChild()
{

  // do nothing as we are using service, maybe consider thumbnailing

  // if (this->myParent->IsActive())
  //   this->PutCameraDataWithROI();
  // else
  //   this->myParent->SetActive(true); // as long as this plugin is running, parent is active

}

////////////////////////////////////////////////////////////////////////////////
// Finalize the controller
void RosProsilica::FiniChild()
{
  this->myParent->SetActive(false);
}

////////////////////////////////////////////////////////////////////////////////
// Put laser data to the interface
void RosProsilica::PutCameraDataWithROI(int x, int y, int w, int h)
{

}



