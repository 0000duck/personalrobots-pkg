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

#include <image_publisher/image_publisher.h>

#include <sensor_msgs/image_encodings.h>

#include <opencv/cvwimage.h>
#include <opencv/highgui.h>

ImagePublisher::ImagePublisher(const ros::NodeHandle& node_handle)
  : node_handle_(node_handle)
{
}

ImagePublisher::ImagePublisher(const std::string& topic, const ros::NodeHandle& node_handle,
                               bool republishing)
  : node_handle_(node_handle)
{
  advertise(topic, republishing);
}

ImagePublisher::~ImagePublisher()
{
}

void ImagePublisher::advertise(const std::string& topic, bool republishing)
{
  republishing_ = republishing;
  if (!republishing_)
    image_pub_ = node_handle_.advertise<sensor_msgs::Image>(topic, 1);
  thumbnail_pub_ = node_handle_.advertise<sensor_msgs::Image>(topic + "_thumbnail", 1);
  compressed_pub_ = node_handle_.advertise<sensor_msgs::CompressedImage>(topic + "_compressed", 1);
}

uint32_t ImagePublisher::getNumSubscribers() const
{
  return image_pub_.getNumSubscribers() + thumbnail_pub_.getNumSubscribers()
    + compressed_pub_.getNumSubscribers();
}

std::string ImagePublisher::getTopic() const
{
  return image_pub_.getTopic();
}

std::string ImagePublisher::getTopicThumbnail() const
{
  return thumbnail_pub_.getTopic();
}

std::string ImagePublisher::getTopicCompressed() const
{
  return compressed_pub_.getTopic();
}

void ImagePublisher::publish(const sensor_msgs::Image& message) const
{
  if (!republishing_)
    image_pub_.publish(message);
  
  uint32_t thumb_subscribers = thumbnail_pub_.getNumSubscribers();
  uint32_t compressed_subscribers = compressed_pub_.getNumSubscribers();
  if (thumb_subscribers == 0 && compressed_subscribers == 0)
    return;

  // @todo: this probably misses some cases
  if (cv_bridge_.encoding_as_fmt(message.encoding) == "GRAY") {
    if (!cv_bridge_.fromImage(message, sensor_msgs::image_encodings::MONO8)) {
      ROS_ERROR("Could not convert image from %s to mono8", message.encoding.c_str());
      return;
    }
  }
  else if (!cv_bridge_.fromImage(message, sensor_msgs::image_encodings::RGB8)) {
    ROS_ERROR("Could not convert image from %s to rgb8", message.encoding.c_str());
    return;
  }
  
  if (thumb_subscribers > 0) {
    publishThumbnailImage(message);
  }

  if (compressed_subscribers > 0) {
    publishCompressedImage(message);
  }
}

void ImagePublisher::publish(const sensor_msgs::ImageConstPtr& message) const
{
  publish(*message);
}

void ImagePublisher::shutdown()
{
  image_pub_.shutdown();
  thumbnail_pub_.shutdown();
  compressed_pub_.shutdown();
}

void ImagePublisher::publishThumbnailImage(const sensor_msgs::Image& message) const
{
  sensor_msgs::Image thumbnail;
  thumbnail.header = message.header;
  
  // Update settings from parameter server
  int thumbnail_size = 128;
  node_handle_.getParam("thumbnail_size", thumbnail_size, true);

  // Rescale image
  const IplImage* image = cv_bridge_.toIpl();
  int width = image->width;
  int height = image->height;
  float aspect = std::sqrt((float)width / height);
  int scaled_width  = thumbnail_size * aspect + 0.5;
  int scaled_height = thumbnail_size / aspect + 0.5;

  cv::WImageBuffer_b buffer(scaled_width, scaled_height, image->nChannels);
  cvResize(image, buffer.Ipl());

  // Set up message and publish
  if (sensor_msgs::CvBridge::fromIpltoRosImage(buffer.Ipl(), thumbnail)) {
    thumbnail.encoding = message.encoding; // otherwise RGB8->TYPE_8UC1, etc.
    thumbnail_pub_.publish(thumbnail);
  } else {
    ROS_ERROR("Unable to create thumbnail image message");
  }
}

void ImagePublisher::publishCompressedImage(const sensor_msgs::Image& message) const
{
  sensor_msgs::CompressedImage compressed;
  compressed.header = message.header;
  
  // Update settings from parameter server
  int params[3] = {0};
  std::string format;
  if (!node_handle_.getParam("compression_type", format, true))
    format = "jpeg";
  if (format == "jpeg") {
    params[0] = CV_IMWRITE_JPEG_QUALITY;
    params[1] = 80; // default: 80% quality
  }
  else if (format == "png") {
    params[0] = CV_IMWRITE_PNG_COMPRESSION;
    params[1] = 9; // default: maximum compression
  }
  else {
    ROS_ERROR("Unknown compression type '%s', valid options are 'jpeg' and 'png'",
              format.c_str());
    return;
  }
  node_handle_.getParam("compression_level", params[1], true);
  std::string extension = '.' + format;

  // Compress image
  const IplImage* image = cv_bridge_.toIpl();
  CvMat* buf = cvEncodeImage(extension.c_str(), image, params);

  // Set up message and publish
  compressed.data.resize(buf->width);
  memcpy(&compressed.data[0], buf->data.ptr, buf->width);
  cvReleaseMat(&buf);

  compressed_pub_.publish(compressed);
}
