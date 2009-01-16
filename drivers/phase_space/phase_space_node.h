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

//! \author Vijay Pradeep

/****
 * This driver provides a ros-interface for the Phase Space Impulse positioning system.
 * www.PhaseSpace.com
 */

#ifndef PHASE_SPACE_NODE_H_
#define PHASE_SPACE_NODE_H_

#include "ros/node.h"

// PhaseSpace API
#include "owl.h"

// Messages
#include "robot_msgs/MocapSnapshot.h"
#include "robot_msgs/MocapMarker.h"
#include "robot_msgs/MocapBody.h"

namespace phase_space
{

class PhaseSpaceNode : public ros::Node
{
public :

  PhaseSpaceNode() ;
  ~PhaseSpaceNode() ;
  
  void startOwlClient() ;
  void shutdownOwlClient() ;
  void startStreaming() ;
  bool spin() ;

private :
  int grabMarkers(robot_msgs::MocapSnapshot& snapshot) ;
  int grabBodies (robot_msgs::MocapSnapshot& snapshot) ;
  void grabTime  (robot_msgs::MocapSnapshot& snapshot) ;
  void copyMarkerToMessage(const OWLMarker& owl_marker, robot_msgs::MocapMarker& msg_marker) ;
  void copyBodyToMessage(const OWLRigid& owl_body, robot_msgs::MocapBody& msg_body) ;
  void owlPrintError(const char *s, int n) ;
  void dispSnapshot(const robot_msgs::MocapSnapshot& snapshot) ;
} ;

}

#endif /* PHASE_SPACE_NODE_H_ */
