#***********************************************************
#* Software License Agreement (BSD License)
#*
#*  Copyright (c) 2009, Willow Garage, Inc.
#*  All rights reserved.
#*
#*  Redistribution and use in source and binary forms, with or without
#*  modification, are permitted provided that the following conditions
#*  are met:
#*
#*   * Redistributions of source code must retain the above copyright
#*     notice, this list of conditions and the following disclaimer.
#*   * Redistributions in binary form must reproduce the above
#*     copyright notice, this list of conditions and the following
#*     disclaimer in the documentation and/or other materials provided
#*     with the distribution.
#*   * Neither the name of the Willow Garage nor the names of its
#*     contributors may be used to endorse or promote products derived
#*     from this software without specific prior written permission.
#*
#*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
#*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#*  POSSIBILITY OF SUCH DAMAGE.
#***********************************************************

import rospy

import threading
import time
import Queue

from std_msgs.msg import Empty
from robot_actions.msg import NoArgumentsActionState
from robot_actions.msg import ActionStatus

RESET = ActionStatus.RESET
ACTIVE = ActionStatus.ACTIVE
PREEMPTED = ActionStatus.PREEMPTED
SUCCESS = ActionStatus.SUCCESS
ABORTED = ActionStatus.ABORTED

class Action:

  def __init__(self, node_name, goalmsg, statemsg, feedbackmsg, rate = 10.0):

    self.nn = node_name
    self.rate = rate

    self.status = RESET
    self.preempted = False
    self.goalmsg = goalmsg
    self.statemsg = statemsg
    self.feedbackmsg = feedbackmsg

    self.goal  = goalmsg()
    self.feedback  = feedbackmsg()

    self.feedback_queue = Queue.Queue()

  def run(self):

    self.goals_sub_ = rospy.Subscriber(self.nn + "/activate", self.goalmsg, self.onGoal)
    self.pre_sub_ = rospy.Subscriber(self.nn + "/preempt", Empty, self.onPreempt)
    self.state_pub_ = rospy.Publisher(self.nn + "/feedback", self.statemsg)

    self.thread0 = threading.Thread(target = self.actionStatusTick)
    self.thread0.start()
    self.feedback_thread = threading.Thread(target = self.actionStatusPublisher)
    self.feedback_thread.start()

  def onGoal(self, goal):

    self.goal = goal
    self.preempted = False
    self.status = ACTIVE
    self.update()
    r = self.execute(goal)
    assert r in [ SUCCESS, PREEMPTED, ABORTED ]
    self.status = r
    self.update()

  def onPreempt(self, msg):
    self.preempted = True

  def isPreemptRequested(self):
    return self.preempted

  def update(self):
    """ Push the current state onto the feedback message queue """
    msg = self.statemsg()
    msg.status.value = self.status
    msg.goal = self.goal
    msg.feedback = self.feedback
    self.feedback_queue.put(msg)

  def actionStatusTick(self):

    while not rospy.is_shutdown():
      self.update()
      time.sleep(1.0 / self.rate)
    self.update() # Wake up the publisher thread

  def actionStatusPublisher(self):

    while not rospy.is_shutdown():
      msg = self.feedback_queue.get()
      self.state_pub_.publish(msg)

  def execute(self, goal):
    assert 0, "Users of Action should override the execute method"
