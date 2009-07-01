#ifndef __M3N_LOGGING_H__
#define __M3N_LOGGING_H__
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

#include <sstream>
#include <map>
#include <vector>

#include <ros/ros.h>

#include <functional_m3n/random_field.h>

using namespace std;

void printClassificationRates(const map<unsigned int, RandomField::Node*>& nodes, const map<unsigned int,
    unsigned int>& inferred_labels, const vector<unsigned int>& labels)
{
  // label -> counters
  map<unsigned int, unsigned int> total_label_count;
  map<unsigned int, unsigned int> correct_label_count;

  unsigned int nbr_correct;

  unsigned int curr_node_id = 0;
  unsigned int curr_gt_label = 0;
  unsigned int curr_infer_label = 0;
  map<unsigned int, RandomField::Node*>::const_iterator iter_nodes;
  for (iter_nodes = nodes.begin(); iter_nodes != nodes.end() ; iter_nodes++)
  {
    curr_node_id = iter_nodes->first;
    curr_gt_label = iter_nodes->second->getLabel();
    curr_infer_label = inferred_labels.find(curr_node_id)->second;

    total_label_count[curr_gt_label]++;

    if (curr_gt_label == curr_infer_label)
    {
      nbr_correct++;
      correct_label_count[curr_gt_label]++;
    }
  }

  ROS_INFO("Total correct: %u/%u=%f", nbr_correct, nodes.size(), static_cast<double>(nbr_correct)/static_cast<double>(nodes.size()));
  stringstream ss;
  ss << "Label distribution. ";
  unsigned int curr_label = 0;
  for (unsigned int i = 0 ; i < labels.size() ; i++)
  {
    curr_label = labels[i];
    ss << "[" << curr_label << ": " << correct_label_count[curr_label] << "/" << total_label_count[curr_label] << "]  ";
  }
  ROS_INFO("%s", ss.str());
}

#endif
