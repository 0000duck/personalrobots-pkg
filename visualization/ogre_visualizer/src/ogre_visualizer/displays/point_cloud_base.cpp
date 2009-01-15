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

#include "point_cloud_base.h"
#include "common.h"
#include "ros_topic_property.h"
#include "properties/property.h"
#include "properties/property_manager.h"

#include "ros/node.h"
#include <ros/time.h>
#include "ogre_tools/point_cloud.h"

#include <tf/transform_listener.h>

#include <OgreSceneNode.h>
#include <OgreSceneManager.h>

namespace ogre_vis
{

PointCloudBase::CloudInfo::CloudInfo(Ogre::SceneManager* scene_manager)
: cloud_(NULL)
, scene_node_(NULL)
, scene_manager_(scene_manager)
, time_(0.0f)
{}

PointCloudBase::CloudInfo::~CloudInfo()
{
  delete cloud_;

  if (scene_node_)
  {
    scene_manager_->destroySceneNode(scene_node_->getName());
  }
}

PointCloudBase::PointCloudBase( const std::string& name, VisualizationManager* manager )
: Display( name, manager )
, min_color_( 0.0f, 0.0f, 0.0f )
, max_color_( 1.0f, 1.0f, 1.0f )
, min_intensity_(0.0f)
, max_intensity_(4096.0f)
, auto_compute_intensity_bounds_(true)
, intensity_bounds_changed_(false)
, style_( Billboards )
, channel_color_idx_( -1 )
, billboard_size_( 0.01 )
, point_decay_time_(0.0f)
, billboard_size_property_( NULL )
, min_color_property_( NULL )
, max_color_property_( NULL )
, auto_compute_intensity_bounds_property_( NULL )
, min_intensity_property_( NULL )
, max_intensity_property_( NULL )
, style_property_( NULL )
, channel_property_( NULL )
, decay_time_property_( NULL )
{
  setStyle( style_ );
  setBillboardSize( billboard_size_ );
  setChannelColorIndex ( channel_color_idx_ );
}

PointCloudBase::~PointCloudBase()
{
  transform_thread_destroy_ = true;
  transform_cond_.notify_all();
  transform_thread_.join();
}

void PointCloudBase::setMaxColor( const Color& color )
{
  max_color_ = color;

  if ( max_color_property_ )
  {
    max_color_property_->changed();
  }

  causeRender();
}

void PointCloudBase::setMinColor( const Color& color )
{
  min_color_ = color;

  if ( min_color_property_ )
  {
    min_color_property_->changed();
  }

  causeRender();
}

void PointCloudBase::setMinIntensity( float val )
{
  min_intensity_ = val;
  if (min_intensity_ > max_intensity_)
  {
    min_intensity_ = max_intensity_;
  }

  if (min_intensity_property_)
  {
    min_intensity_property_->changed();
  }

  causeRender();
}

void PointCloudBase::setMaxIntensity( float val )
{
  max_intensity_ = val;
  if (max_intensity_ < min_intensity_)
  {
    max_intensity_ = min_intensity_;
  }

  if (max_intensity_property_)
  {
    max_intensity_property_->changed();
  }

  causeRender();
}

void PointCloudBase::setDecayTime( float time )
{
  point_decay_time_ = time;

  if ( decay_time_property_ )
  {
    decay_time_property_->changed();
  }

  causeRender();
}

void PointCloudBase::setAutoComputeIntensityBounds(bool compute)
{
  auto_compute_intensity_bounds_ = compute;

  if (auto_compute_intensity_bounds_property_)
  {
    auto_compute_intensity_bounds_property_->changed();
  }

  causeRender();
}

void PointCloudBase::setStyle( int style )
{
  ROS_ASSERT( style < StyleCount );

  {
    RenderAutoLock renderLock( this );

    style_ = style;

    boost::mutex::scoped_lock lock(clouds_mutex_);
    D_CloudInfo::iterator it = clouds_.begin();
    D_CloudInfo::iterator end = clouds_.end();
    for (; it != end; ++it)
    {
      (*it)->cloud_->setUsePoints(style == Points);
    }
  }

  if ( style_property_ )
  {
    style_property_->changed();
  }

  causeRender();
}

/** \brief Set the channel color index. Called through the \a channel_property_ callback.
  * \param channel_color_idx the index of the channel to be rendered
  */
void
  PointCloudBase::setChannelColorIndex (int channel_color_idx)
{
  ROS_ASSERT (channel_color_idx < ChannelRenderCount);

  channel_color_idx_ = channel_color_idx;
}

void PointCloudBase::setBillboardSize( float size )
{
  {
    RenderAutoLock renderLock( this );

    billboard_size_ = size;

    boost::mutex::scoped_lock lock(clouds_mutex_);
    D_CloudInfo::iterator it = clouds_.begin();
    D_CloudInfo::iterator end = clouds_.end();
    for (; it != end; ++it)
    {
      (*it)->cloud_->setBillboardDimensions( size, size );
    }
  }

  causeRender();

  if ( billboard_size_property_ )
  {
    billboard_size_property_->changed();
  }

  causeRender();
}

void PointCloudBase::onEnable()
{
  transform_thread_destroy_ = false;
  transform_thread_ = boost::thread(boost::bind(&PointCloudBase::transformThreadFunc, this));
}

void PointCloudBase::onDisable()
{
  transform_thread_destroy_ = true;
  transform_cond_.notify_all();
  transform_thread_.join();

  boost::mutex::scoped_lock clouds_lock(clouds_mutex_);
  boost::mutex::scoped_lock messages_lock(message_queue_mutex_);

  clouds_.clear();
  message_queue_.clear();
  clouds_to_delete_.clear();

  while (!transform_queue_.empty())
  {
    transform_queue_.pop();
  }
}

void PointCloudBase::update(float dt)
{
  if (intensity_bounds_changed_)
  {
    setMinIntensity(min_intensity_);
    setMaxIntensity(max_intensity_);
    intensity_bounds_changed_ = false;
  }

  V_PointCloud messages;
  {
    boost::mutex::scoped_lock lock(message_queue_mutex_);
    messages.swap(message_queue_);
  }

  V_PointCloud::iterator message_it = messages.begin();
  V_PointCloud::iterator message_end = messages.end();
  for (; message_it != message_end; ++message_it)
  {
    processMessage(*message_it);
  }
  messages.clear();

  {
    boost::mutex::scoped_lock lock(clouds_mutex_);

    D_CloudInfo::iterator cloud_it = clouds_.begin();
    D_CloudInfo::iterator cloud_end = clouds_.end();
    for (;cloud_it != cloud_end; ++cloud_it)
    {
      const CloudInfoPtr& info = *cloud_it;

      info->time_ += dt;
    }

    if (point_decay_time_ > 0.0f)
    {
      while (!clouds_.empty() && clouds_.front()->time_ > point_decay_time_)
      {
        clouds_.pop_front();
      }
    }
  }

  {
    boost::mutex::scoped_lock lock(clouds_to_delete_mutex_);
    clouds_to_delete_.clear();
  }
}

void transformIntensity( float val, ogre_tools::PointCloud::Point& point, const Color& min_color, float min_intensity, float max_intensity, float diff_intensity )
{
  float normalized_intensity = diff_intensity > 0.0f ? ( val - min_intensity ) / diff_intensity : 1.0f;
  normalized_intensity = std::min(1.0f, std::max(0.0f, normalized_intensity));
  point.r_ = point.r_*normalized_intensity + min_color.r_*(1.0f - normalized_intensity);
  point.g_ = point.g_*normalized_intensity + min_color.g_*(1.0f - normalized_intensity);
  point.b_ = point.b_*normalized_intensity + min_color.b_*(1.0f - normalized_intensity);
}

void transformRGB( float val, ogre_tools::PointCloud::Point& point, const Color&, float, float, float )
{
  int rgb = *reinterpret_cast<int*>(&val);
  point.r_ = ((rgb >> 16) & 0xff) / 255.0f;
  point.g_ = ((rgb >> 8) & 0xff) / 255.0f;
  point.b_ = (rgb & 0xff) / 255.0f;
}

void transformR( float val, ogre_tools::PointCloud::Point& point, const Color&, float, float, float )
{
  point.r_ = val;
}

void transformG( float val, ogre_tools::PointCloud::Point& point, const Color&, float, float, float )
{
  point.g_ = val;
}

void transformB( float val, ogre_tools::PointCloud::Point& point, const Color&, float, float, float )
{
  point.b_ = val;
}

void PointCloudBase::processMessage(const boost::shared_ptr<std_msgs::PointCloud>& cloud)
{
  CloudInfoPtr info;

  if (point_decay_time_ == 0.0f) // reuse old cloud
  {
    boost::mutex::scoped_lock lock(clouds_mutex_);

    if (clouds_.size() == 1)
    {
      info = clouds_.front();
    }
    else
    {
      info = CloudInfoPtr(new CloudInfo(scene_manager_));
    }

    clouds_.clear();
  }
  else
  {
    info = CloudInfoPtr(new CloudInfo(scene_manager_));
  }

  // Get the channels that we could potentially render
  channel_property_->clear ();
  typedef std::vector<std_msgs::ChannelFloat32> V_Chan;
  V_Chan::iterator chan_it = cloud->chan.begin();
  V_Chan::iterator chan_end = cloud->chan.end();
  uint32_t index = 0;
  for ( ; chan_it != chan_end; ++chan_it, ++index )
  {
    std_msgs::ChannelFloat32& chan = *chan_it;
    if (chan.name == "intensity" || chan.name == "intensities")
      channel_property_->addOption ("Intensity", Intensity);

    if (chan.name == "rgb" || chan.name == "r")
      channel_property_->addOption ("Color (RGB)", ColorRGBSpace);

    if (chan.name == "nx")
      channel_property_->addOption ("Normal Sphere", NormalSphere);

    if (chan.name == "curvature" || chan.name == "curvatures")
      channel_property_->addOption ("Curvature", Curvature);
  }
  if ( channel_property_ )
  {
    channel_property_->changed();
  }

  info->message_ = cloud;
  info->time_ = 0;
  if (!info->scene_node_)
  {
    ROS_ASSERT(!info->cloud_);
    info->scene_node_ = scene_manager_->getRootSceneNode()->createChildSceneNode();
    info->cloud_ = new ogre_tools::PointCloud( scene_manager_, info->scene_node_ );
  }
  // TODO: these two should actually happen after the cloud has been transformed, back in the main thread
  info->cloud_->setUsePoints( style_ == Points );
  info->cloud_->setBillboardDimensions( billboard_size_, billboard_size_ );

  boost::mutex::scoped_lock lock(transform_queue_mutex_);
  transform_queue_.push(info);
  transform_cond_.notify_all();
}

void PointCloudBase::transformThreadFunc()
{
  while (!transform_thread_destroy_)
  {
    CloudInfoPtr info;

    {
      boost::mutex::scoped_lock lock(transform_queue_mutex_);

      while (!transform_thread_destroy_ && transform_queue_.empty())
      {
        transform_cond_.wait(lock);
      }

      if (transform_thread_destroy_)
      {
        return;
      }

      info = transform_queue_.front();
      transform_queue_.pop();
    }

    transformCloud(info);

    {
      boost::mutex::scoped_lock lock(clouds_mutex_);

      if (point_decay_time_ == 0.0f)
      {
        boost::mutex::scoped_lock lock(clouds_to_delete_mutex_);
        clouds_to_delete_.insert(clouds_to_delete_.begin(), clouds_.begin(), clouds_.end());
        clouds_.clear();
      }

      clouds_.push_back(info);
      // Clear the info shared pointer while we still have the clouds mutex locked
      // So that we will never delete it from this thread
      info = CloudInfoPtr();
      causeRender();
    }
  }
}

void PointCloudBase::transformCloud(const CloudInfoPtr& info)
{
  const boost::shared_ptr<std_msgs::PointCloud>& cloud = info->message_;

  std::string frame_id = cloud->header.frame_id;
  if ( frame_id.empty() )
  {
    frame_id = fixed_frame_;
  }

  tf::Stamped<tf::Pose> pose( btTransform( btQuaternion( 0, 0, 0 ), btVector3( 0, 0, 0 ) ), cloud->header.stamp, frame_id );

  try
  {
    tf_->transformPose( fixed_frame_, pose, pose );
  }
  catch(tf::TransformException& e)
  {
    ROS_ERROR( "Error transforming point cloud '%s' from frame '%s' to frame '%s'\n", name_.c_str(), frame_id.c_str(), fixed_frame_.c_str() );
  }

  Ogre::Vector3 position( pose.getOrigin().x(), pose.getOrigin().y(), pose.getOrigin().z() );
  robotToOgre( position );

  btScalar yaw, pitch, roll;
  pose.getBasis().getEulerZYX( yaw, pitch, roll );

  Ogre::Matrix3 orientation( ogreMatrixFromRobotEulers( yaw, pitch, roll ) );

  typedef std::vector<std_msgs::ChannelFloat32> V_Chan;
  typedef std::vector<bool> V_bool;

  V_bool valid_channels(cloud->chan.size());
  uint32_t point_count = cloud->get_pts_size();
  V_Chan::iterator chan_it = cloud->chan.begin();
  V_Chan::iterator chan_end = cloud->chan.end();
  uint32_t index = 0;

  bool use_normals_as_coordinates = false;

  for ( ; chan_it != chan_end; ++chan_it, ++index )
  {
    std_msgs::ChannelFloat32& chan = *chan_it;
    uint32_t val_count = chan.vals.size();
    bool channel_size_correct = val_count == point_count;
    ROS_ERROR_COND(!channel_size_correct, "Point cloud '%s' has channel with fewer values than points (%d values, %d points)", name_.c_str(), val_count, point_count);

    valid_channels[index] = channel_size_correct;

    // Check for intensities
    if ( auto_compute_intensity_bounds_ && channel_size_correct && ( chan.name == "intensity" || chan.name == "intensities" ) &&
         channel_color_idx_ == Intensity )
    {
      min_intensity_ = 999999.0f;
      max_intensity_ = -999999.0f;
      for(uint32_t i = 0; i < point_count; i++)
      {
        float& intensity = chan.vals[i];
        // arbitrarily cap to 4096 for now
        intensity = std::min( intensity, 4096.0f );
        min_intensity_ = std::min( min_intensity_, intensity );
        max_intensity_ = std::max( max_intensity_, intensity );
      }

      intensity_bounds_changed_ = true;
    }

    // Check for curvatures
    else if ( auto_compute_intensity_bounds_ && channel_size_correct && ( chan.name == "curvature" || chan.name == "curvatures" ) &&
              channel_color_idx_ == Curvature )
    {
      min_intensity_ = 999999.0f;
      max_intensity_ = -999999.0f;
      for(uint32_t i = 0; i < point_count; i++)
      {
        float& intensity = chan.vals[i];
        // arbitrarily cap to 4096 for now
        intensity = std::min( intensity, 4096.0f );
        min_intensity_ = std::min( min_intensity_, intensity );
        max_intensity_ = std::max( max_intensity_, intensity );
      }

      intensity_bounds_changed_ = true;
    }
    else if ( chan.name == "nx" && channel_color_idx_ == NormalSphere )
      use_normals_as_coordinates = true;
  }

  // Look for point normals
  int nx_idx = getROSCloudChannelIndex (cloud, std::string ("nx"));
  int ny_idx = getROSCloudChannelIndex (cloud, std::string ("ny"));
  int nz_idx = getROSCloudChannelIndex (cloud, std::string ("nz"));
  if ( use_normals_as_coordinates && (ny_idx == -1 || nz_idx == -1) )
  {
    ROS_WARN ("Normal information requested via 'nx', but 'ny' and 'nz' channels are not present!");
    use_normals_as_coordinates = false;
  }

  float diff_intensity = max_intensity_ - min_intensity_;

  typedef std::vector< ogre_tools::PointCloud::Point > V_Point;
  V_Point points;
  points.resize( point_count );
  for(uint32_t i = 0; i < point_count; i++)
  {
    ogre_tools::PointCloud::Point& current_point = points[ i ];

    if (use_normals_as_coordinates && nx_idx != -1 && ny_idx != -1 && nz_idx != -1)
    {
      current_point.x_ = cloud->chan[nx_idx].vals[i];
      current_point.y_ = cloud->chan[ny_idx].vals[i];
      current_point.z_ = cloud->chan[nz_idx].vals[i];
    }
    else          // Use normal 3D x-y-z coordinates
    {
      current_point.x_ = cloud->pts[i].x;
      current_point.y_ = cloud->pts[i].y;
      current_point.z_ = cloud->pts[i].z;
    }
    current_point.r_ = max_color_.r_;
    current_point.g_ = max_color_.g_;
    current_point.b_ = max_color_.b_;
  }

  chan_it = cloud->chan.begin();
  index = 0;
  for ( ; chan_it != chan_end; ++chan_it, ++index )
  {
    if ( !valid_channels[index] )
    {
      continue;
    }

    std_msgs::ChannelFloat32& chan = *chan_it;

    enum ChannelType
    {
      CT_INTENSITY,
      CT_RGB,
      CT_R,
      CT_G,
      CT_B,

      CT_COUNT
    };

    ChannelType type = CT_INTENSITY;
    if ( chan.name == "intensity" || chan.name == "intensities" || chan.name == "curvatures" || chan.name == "curvature" )
    {
      type = CT_INTENSITY;
    }
    else if ( chan.name == "rgb" )
    {
      type = CT_RGB;
    }
    else if ( chan.name == "r" )
    {
      type = CT_R;
    }
    else if ( chan.name == "g" )
    {
      type = CT_G;
    }
    else if ( chan.name == "b" )
    {
      type = CT_B;
    }
    else
    {
      continue;
    }

    typedef void (*TransformFunc)(float, ogre_tools::PointCloud::Point&, const Color&, float, float, float);
    TransformFunc funcs[CT_COUNT] =
    {
      transformIntensity,
      transformRGB,
      transformR,
      transformG,
      transformB
    };

    for(uint32_t i = 0; i < point_count; i++)
    {
      ogre_tools::PointCloud::Point& current_point = points[ i ];
      if ( ( channel_color_idx_ == Intensity && (chan.name == "intensity" || chan.name == "intensities") ) ||
           ( channel_color_idx_ == Curvature && (chan.name == "curvature" || chan.name == "curvatures") ) ||
           ( channel_color_idx_ == ColorRGBSpace && (chan.name == "rgb" || chan.name == "r" || chan.name == "g" || chan.name == "b") )
         )
      {
        funcs[type]( chan.vals[i], current_point, min_color_, min_intensity_, max_intensity_, diff_intensity );
      }
    }
  }

  {
    RenderAutoLock renderLock( this );

    info->scene_node_->setPosition( position );
    info->scene_node_->setOrientation( orientation );

    info->cloud_->clear();

    if ( !points.empty() )
    {
      info->cloud_->addPoints( &points.front(), points.size() );
    }
  }

  causeRender();

}

void PointCloudBase::addMessage(const boost::shared_ptr<std_msgs::PointCloud>& cloud)
{
  boost::mutex::scoped_lock lock(message_queue_mutex_);
  message_queue_.push_back(cloud);
}

void PointCloudBase::fixedFrameChanged()
{
  reset();
}

void PointCloudBase::createProperties()
{
  style_property_ = property_manager_->createProperty<EnumProperty>( "Style", property_prefix_, boost::bind( &PointCloudBase::getStyle, this ),
                                                                     boost::bind( &PointCloudBase::setStyle, this, _1 ), parent_category_, this );
  style_property_->addOption( "Billboards", Billboards );
  style_property_->addOption( "Points", Points );

  channel_property_ = property_manager_->createProperty<EnumProperty>( "Channel", property_prefix_, boost::bind( &PointCloudBase::getChannelColorIndex, this ),
                                                                     boost::bind( &PointCloudBase::setChannelColorIndex, this, _1 ), parent_category_, this );

  max_color_property_ = property_manager_->createProperty<ColorProperty>( "Max Color", property_prefix_, boost::bind( &PointCloudBase::getMaxColor, this ),
                                                                        boost::bind( &PointCloudBase::setMaxColor, this, _1 ), parent_category_, this );
  min_color_property_ = property_manager_->createProperty<ColorProperty>( "Min Color", property_prefix_, boost::bind( &PointCloudBase::getMinColor, this ),
                                                                          boost::bind( &PointCloudBase::setMinColor, this, _1 ), parent_category_, this );
  // legacy "Color" support... convert it to max color
  max_color_property_->addLegacyName("Color");

  billboard_size_property_ = property_manager_->createProperty<FloatProperty>( "Billboard Size", property_prefix_, boost::bind( &PointCloudBase::getBillboardSize, this ),
                                                                                boost::bind( &PointCloudBase::setBillboardSize, this, _1 ), parent_category_, this );
  billboard_size_property_->setMin( 0.0001 );

  auto_compute_intensity_bounds_property_ = property_manager_->createProperty<BoolProperty>( "Autocompute Intensity Bounds", property_prefix_, boost::bind( &PointCloudBase::getAutoComputeIntensityBounds, this ),
                                                                            boost::bind( &PointCloudBase::setAutoComputeIntensityBounds, this, _1 ), parent_category_, this );
  max_intensity_property_ = property_manager_->createProperty<FloatProperty>( "Max Intensity", property_prefix_, boost::bind( &PointCloudBase::getMaxIntensity, this ),
                                                                          boost::bind( &PointCloudBase::setMaxIntensity, this, _1 ), parent_category_, this );
  min_intensity_property_ = property_manager_->createProperty<FloatProperty>( "Min Intensity", property_prefix_, boost::bind( &PointCloudBase::getMinIntensity, this ),
                                                                          boost::bind( &PointCloudBase::setMinIntensity, this, _1 ), parent_category_, this );

  decay_time_property_ = property_manager_->createProperty<FloatProperty>( "Decay Time", property_prefix_, boost::bind( &PointCloudBase::getDecayTime, this ),
                                                                           boost::bind( &PointCloudBase::setDecayTime, this, _1 ), parent_category_, this );
}

void PointCloudBase::reset()
{
  transform_thread_destroy_ = true;
  transform_cond_.notify_all();
  transform_thread_.join();

  {
    // transform thread should no longer be running, so no need to lock the mutex
    while (!transform_queue_.empty())
    {
      transform_queue_.pop();
    }

    RenderAutoLock renderLock( this );

    boost::mutex::scoped_lock clouds_lock(clouds_mutex_);
    clouds_.clear();

    boost::mutex::scoped_lock message_lock(message_queue_mutex_);
    message_queue_.clear();
  }

  transform_thread_destroy_ = false;
  transform_thread_ = boost::thread(boost::bind(&PointCloudBase::transformThreadFunc, this));
}

} // namespace ogre_vis
