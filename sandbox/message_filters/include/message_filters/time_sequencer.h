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

#ifndef MESSAGE_FILTERS_TIME_SEQUENCER_H
#define MESSAGE_FILTERS_TIME_SEQUENCER_H

#include <boost/noncopyable.hpp>

#include <ros/ros.h>

#include "connection.h"

namespace message_filters
{

/**
 * \class TimeSequencer
 *
 * \brief Sequences messages based on the timestamp of their header.
 *
 * The TimeSequencer object is templated on the type of message being sequenced.
 *
 * \section behavior BEHAVIOR

 * At construction, the TimeSequencer takes a ros::Duration
 * "delay" which specifies how long to queue up messages to
 * provide a time sequencing over them.  As messages arrive they are
 * sorted according to their time stamps.  A callback for a message is
 * never invoked until the messages' time stamp is out of date by at
 * least delay.  However, for all messages which are out of date
 * by at least delay, their callback are invoked and guaranteed
 * to be in temporal order.  If a message arrives from a time \b prior
 * to a message which has already had its callback invoked, it is
 * thrown away.
 *
 * \section connections CONNECTIONS
 *
 * TimeSequencer's input and output connections are both of the same signature as roscpp subscription callbacks, ie.
\verbatim
void callback(const boost::shared_ptr<M const>&);
\endverbatim
 *
 */
template<class M>
class TimeSequencer : public boost::noncopyable
{
public:
  typedef boost::shared_ptr<M const> MConstPtr ;
  typedef boost::function<void(const MConstPtr&)> Callback;
  typedef boost::signal<void(const MConstPtr&)> Signal;

  /**
   * \brief Constructor
   * \param f A filter to connect this sequencer's input to
   * \param delay The minimum time to hold a message before passing it through.
   * \param update_rate The rate at which to check for messages which have passed "delay"
   * \param queue_size The number of messages to store
   * \param nh (optional) The NodeHandle to use to create the ros::Timer that runs at update_rate
   */
  template<class F>
  TimeSequencer(F& f, ros::Duration delay, ros::Duration update_rate, uint32_t queue_size, ros::NodeHandle nh = ros::NodeHandle())
  : delay_(delay)
  , update_rate_(update_rate)
  , queue_size_(queue_size)
  , nh_(nh)
  {
    init();
    connectTo(f);
  }

  /**
   * \brief Constructor
   *
   * This version of the constructor does not take a filter immediately.  You can connect to a filter later with the connectTo() function
   *
   * \param delay The minimum time to hold a message before passing it through.
   * \param update_rate The rate at which to check for messages which have passed "delay"
   * \param queue_size The number of messages to store
   * \param nh (optional) The NodeHandle to use to create the ros::Timer that runs at update_rate
   */
  TimeSequencer(ros::Duration delay, ros::Duration update_rate, uint32_t queue_size, ros::NodeHandle nh = ros::NodeHandle())
  : delay_(delay)
  , update_rate_(update_rate)
  , queue_size_(queue_size)
  , nh_(nh)
  {
    init();
  }

  /**
   * \brief Connect this filter's input to another filter's output.
   */
  template<class F>
  void connectTo(F& f)
  {
    incoming_connection_.disconnect();
    incoming_connection_ = f.connect(boost::bind(&TimeSequencer::cb, this, _1));
  }

  ~TimeSequencer()
  {
    update_timer_.stop();
    incoming_connection_.disconnect();
  }

  Connection connect(const Callback& callback)
  {
    boost::mutex::scoped_lock lock(signal_mutex_);
    return Connection(boost::bind(&TimeSequencer::disconnect, this, _1), signal_.connect(callback));
  }

  void add(const MConstPtr& msg)
  {
    boost::mutex::scoped_lock lock(messages_mutex_);
    if (msg->header.stamp < last_time_)
    {
      return;
    }

    messages_.insert(msg);

    if (queue_size_ != 0 && messages_.size() > queue_size_)
    {
      messages_.erase(*messages_.begin());
    }
  }

private:
  class MessageSort
  {
  public:
    bool operator()(const MConstPtr& lhs, const MConstPtr& rhs) const
    {
      return lhs->header.stamp < rhs->header.stamp;
    }
  };
  typedef std::multiset<MConstPtr, MessageSort> S_Message;
  typedef std::vector<MConstPtr> V_Message;

  void cb(const MConstPtr& msg)
  {
    add(msg);
  }

  void disconnect(const Connection& c)
  {
    boost::mutex::scoped_lock lock(signal_mutex_);
    c.getBoostConnection().disconnect();
  }

  void dispatch()
  {
    V_Message to_call;

    {
      boost::mutex::scoped_lock lock(messages_mutex_);

      while (!messages_.empty())
      {
        const MConstPtr& m = *messages_.begin();
        if (m->header.stamp + delay_ <= ros::Time::now())
        {
          last_time_ = m->header.stamp;
          to_call.push_back(m);
          messages_.erase(messages_.begin());
        }
        else
        {
          break;
        }
      }
    }

    {
      boost::mutex::scoped_lock lock(signal_mutex_);

      typename V_Message::iterator it = to_call.begin();
      typename V_Message::iterator end = to_call.end();
      for (; it != end; ++it)
      {
        signal_(*it);
      }
    }
  }

  void update(const ros::TimerEvent&)
  {
    dispatch();
  }

  void init()
  {
    update_timer_ = nh_.createTimer(update_rate_, &TimeSequencer::update, this);
  }



  ros::Duration delay_;
  ros::Duration update_rate_;
  uint32_t queue_size_;
  ros::NodeHandle nh_;

  ros::Timer update_timer_;

  Connection incoming_connection_;
  Signal signal_;
  boost::mutex signal_mutex_;


  S_Message messages_;
  boost::mutex messages_mutex_;
  ros::Time last_time_;
};

}

#endif
