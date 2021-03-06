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

#ifndef PLANNING_ENVIRONMENT_MONITORS_COLLISION_SPACE_MONITOR_
#define PLANNING_ENVIRONMENT_MONITORS_COLLISION_SPACE_MONITOR_

#include "planning_environment/models/collision_models.h"
#include "planning_environment/monitors/kinematic_model_state_monitor.h"

#include <motion_planning_msgs/AcceptableContact.h>
#include <mapping_msgs/CollisionMap.h>
#include <mapping_msgs/ObjectInMap.h>
#include <boost/thread/mutex.hpp>

namespace planning_environment
{

    /** \brief @b CollisionSpaceMonitor is a class which in addition to being aware of a robot model,
	is also aware of a collision space.
    */
    class CollisionSpaceMonitor : public KinematicModelStateMonitor
    {
    public:
	
	CollisionSpaceMonitor(CollisionModels *cm, tf::TransformListener *tf, std::string frame_id) : KinematicModelStateMonitor(static_cast<RobotModels*>(cm), tf, frame_id)
	{
	    cm_ = cm;
	    setupCSM();
	}
	
	CollisionSpaceMonitor(CollisionModels *cm, tf::TransformListener *tf) : KinematicModelStateMonitor(static_cast<RobotModels*>(cm), tf)
	{
	    cm_ = cm;
	    setupCSM();
	}
	
	virtual ~CollisionSpaceMonitor(void)
	{
	    if (objectInMapFilter_)
		delete objectInMapFilter_;
	    if (objectInMapSubscriber_)
		delete objectInMapSubscriber_;
	    if (collisionMapFilter_)
		delete collisionMapFilter_;
	    if (collisionMapSubscriber_)
		delete collisionMapSubscriber_;
	    if (collisionMapUpdateFilter_)
		delete collisionMapUpdateFilter_;
	    if (collisionMapUpdateSubscriber_)
		delete collisionMapUpdateSubscriber_;
	}

	/** \brief Start the environment monitor. By default, the monitor is started after creation */
	void startEnvironmentMonitor(void);
	
	/** \brief Stop the environment monitor. */
	void stopEnvironmentMonitor(void);
	
	/** \brief Check if the environment monitor is currently started */
	bool isEnvironmentMonitorStarted(void) const
	{
	    return envMonitorStarted_;
	}	

	/** \brief Return the instance of the environment model maintained */
	collision_space::EnvironmentModel* getEnvironmentModel(void) const
	{
	    return collisionSpace_;
	}
	
	/** \brief Return the instance of collision models that is being used */
	CollisionModels* getCollisionModels(void) const
	{
	    return cm_;
	}
	
	/** \brief Define a callback for before updating a map */
	void setOnBeforeMapUpdateCallback(const boost::function<void(const mapping_msgs::CollisionMapConstPtr, bool)> &callback)
	{
	    onBeforeMapUpdate_ = callback;
	}

	/** \brief Define a callback for after updating a map */
	void setOnAfterMapUpdateCallback(const boost::function<void(const mapping_msgs::CollisionMapConstPtr, bool)> &callback)
	{
	    onAfterMapUpdate_ = callback;
	}

	/** \brief Define a callback for updates to the objects in the map */
	void setOnObjectInMapUpdateCallback(const boost::function<void(const mapping_msgs::ObjectInMapConstPtr)> &callback)
	{
	    onObjectInMapUpdate_ = callback;
	}

	/** \brief Return true if  map has been received */
	bool haveMap(void) const
	{
	    return haveMap_;
	}
	
	/** \brief Return true if a map update has been received in the last sec seconds. If sec < 10us, this function always returns true. */
	bool isMapUpdated(double sec) const;
	
	/** \brief Wait until a map is received */
	void waitForMap(void) const;	

	/** \brief Return the last update time for the map */
	const ros::Time& lastMapUpdate(void) const
	{
	    return lastMapUpdate_;
	}
	
	/** \brief Returns the padding used for pointclouds (for collision checking) */
	double getPointCloudPadd(void) const
	{
	    return pointcloud_padd_;
	}
	
	/** \brief Convert an allowed contact message into the structure accepted by the collision space */
	bool computeAllowedContact(const motion_planning_msgs::AcceptableContact &allowedContactMsg, collision_space::EnvironmentModel::AllowedContact &allowedContact) const;
	
	/** \brief This function provides the means to obtain a collision map (the set of boxes)
	 *  from the current environment model */
	void recoverCollisionMap(mapping_msgs::CollisionMap &cmap);

	/** \brief If the user modified some instance of an environment model, this function provides the means to obtain a collision map (the set of boxes)
	 *  from that environment model */
	void recoverCollisionMap(const collision_space::EnvironmentModel *env, mapping_msgs::CollisionMap &cmap);

	
	/** \brief This function provides the means to
	    obtain a set of objects in map (all objects that are not
	    in the namespace the collision map was added to) from the current environment */
	void recoverObjectsInMap(std::vector<mapping_msgs::ObjectInMap> &omap);
	
	/** \brief If the user modified some instance of an
	    environment model, this function provides the means to
	    obtain a set of objects in map (all objects that are not
	    in the namespace the collision map was added to) */
	void recoverObjectsInMap(const collision_space::EnvironmentModel *env, std::vector<mapping_msgs::ObjectInMap> &omap);
	
    protected:
	
	void setupCSM(void);
	void updateCollisionSpace(const mapping_msgs::CollisionMapConstPtr &collisionMap, bool clear);
	void collisionMapAsSpheres(const mapping_msgs::CollisionMapConstPtr &collisionMap,
				   std::vector<shapes::Shape*> &spheres, std::vector<btTransform> &poses);
	void collisionMapAsBoxes(const mapping_msgs::CollisionMapConstPtr &collisionMap,
				 std::vector<shapes::Shape*> &boxes, std::vector<btTransform> &poses);
	void collisionMapCallback(const mapping_msgs::CollisionMapConstPtr &collisionMap);
	void collisionMapUpdateCallback(const mapping_msgs::CollisionMapConstPtr &collisionMap);
	void objectInMapCallback(const mapping_msgs::ObjectInMapConstPtr &objectInMap);
	virtual bool attachObject(const mapping_msgs::AttachedObjectConstPtr &attachedObject);

	CollisionModels                                                *cm_;
	collision_space::EnvironmentModel                              *collisionSpace_;
	boost::mutex                                                    mapUpdateLock_;
	double                                                          pointcloud_padd_;
	
	bool                                                            envMonitorStarted_;
	
	bool                                                            haveMap_;
	ros::Time                                                       lastMapUpdate_;	
	
	message_filters::Subscriber<mapping_msgs::CollisionMap>        *collisionMapSubscriber_;
	tf::MessageFilter<mapping_msgs::CollisionMap>                  *collisionMapFilter_;
	message_filters::Subscriber<mapping_msgs::CollisionMap>        *collisionMapUpdateSubscriber_;
	tf::MessageFilter<mapping_msgs::CollisionMap>                  *collisionMapUpdateFilter_;
	message_filters::Subscriber<mapping_msgs::ObjectInMap>         *objectInMapSubscriber_;
	tf::MessageFilter<mapping_msgs::ObjectInMap>                   *objectInMapFilter_;
	
	boost::function<void(const mapping_msgs::CollisionMapConstPtr, bool)> onBeforeMapUpdate_;
	boost::function<void(const mapping_msgs::CollisionMapConstPtr, bool)> onAfterMapUpdate_;
	boost::function<void(const mapping_msgs::ObjectInMapConstPtr)>        onObjectInMapUpdate_;
    
    };
    
	
}

#endif

