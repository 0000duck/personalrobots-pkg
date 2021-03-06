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

#ifndef TOPOLOGICAL_MAP_DOOR_INFO_H
#define TOPOLOGICAL_MAP_DOOR_INFO_H

#include <iostream>
#include <boost/shared_ptr.hpp>
#include <door_msgs/Door.h>
#include <ros/time.h>

namespace topological_map
{

using door_msgs::Door;
using ros::Time;

class DoorInfo
{
public:
  DoorInfo (double open_prob) : open_prob_(open_prob), last_obs_time_(0.0) {}
  DoorInfo (istream& str, double open_prob);
  Door getDoorMessage () const;
  void observeDoorMessage (const Door& msg);
  void writeToStream (ostream& str) const;
  double getOpenProb() const { return open_prob_; }
  void setOpenProb(double open_prob) { open_prob_ = open_prob; }
  Time getLastObsTime() const { return last_obs_time_; }
  void setLastObsTime(const Time& last_obs_time) { last_obs_time_ = last_obs_time; }

private:
  // For now, just store a door message
  // Eventually, add variance terms
  Door msg_;
  double open_prob_;
  Time last_obs_time_;
};

typedef boost::shared_ptr<DoorInfo> DoorInfoPtr;
DoorInfoPtr initialDoorEstimate ();


}

#endif // TOPOLOGICAL_MAP_DOOR_INFO_H
