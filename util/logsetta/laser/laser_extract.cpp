///////////////////////////////////////////////////////////////////////////////
// The logsetta package provides log translators for various other robotics
// software frameworks
//
// Copyright (C) 2008, Morgan Quigley
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice, 
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright 
//     notice, this list of conditions and the following disclaimer in the 
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of Stanford University nor the names of its 
//     contributors may be used to endorse or promote products derived from 
//     this software without specific prior written permission.
//   
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////

#include "sensor_msgs/LaserScan.h"
#include <string>
#include "rosrecord/Player.h"

void scan_callback(std::string name, sensor_msgs::LaserScan *scan, 
                   ros::Time t, ros::Time t_no_use, void* f)
{
  FILE* file = (FILE*)f;

  fprintf(file, "%.5f %.5f %.5f %.5f %.5f %.5f %.5f %d ",
          t.toSec(),
          scan->header.stamp.toSec(),
          scan->angle_min,
          scan->angle_max,
          scan->angle_increment,
          scan->range_min,
          scan->range_max,
          scan->ranges.size());
  for (uint32_t i = 0; i < scan->ranges.size(); i++)
  {
    fprintf(file, "%.3f ", scan->ranges[i]);
    fprintf(file, "%d ", (scan->intensities.size() > i ? (int)scan->intensities[i] : 0));
  }
  fprintf(file, "\n");
}

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    printf("usage: laser_extract LOG MESSAGE_NAME\n");
    return 1;
  }

  ros::record::Player player;
  player.open(std::string(argv[1]), ros::Time());
  FILE* file = fopen((std::string(argv[2])+".txt").c_str() , "w");
  player.addHandler<sensor_msgs::LaserScan>(std::string(argv[2]), &scan_callback, file);
  while(player.nextMsg()) { }
  fclose(file);
  return 0;
}

