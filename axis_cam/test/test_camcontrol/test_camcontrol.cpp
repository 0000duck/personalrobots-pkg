///////////////////////////////////////////////////////////////////////////////
// The axis_cam package provides a library that talks to Axis IP-based cameras
// as well as ROS nodes which use these libraries
//
// This file Copyright (C) 2008, Morgan Quigley
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

#include <cstdio>
#include "axis_cam/axis_cam.h"
#include "kbhit.h"

int main(int argc, char **argv)
{
  AxisCam *axis = new AxisCam("192.168.1.90");

  init_keyboard();
  int focus = 2000, iris = 2000;
  while (1)
  {
    if (_kbhit())
    {
      char c = _getch();
      if (c == 'q')
        break;
      switch(c)
      {
        case 'F':
          printf("enabling autofocus\n");
          axis->set_focus(0);
          break;
        case 'f':
          focus += 500;
          printf("focus = %d\n", focus);
          axis->set_focus(focus);
          break;
        case 'd':
          focus -= 500;
          printf("focus = %d\n", focus);
          axis->set_focus(focus);
          break;
        case 'I':
          printf("enabling autoiris\n");
          axis->set_iris(0);
          break;
        case 'i':
          iris += 500;
          printf("iris = %d\n", iris);
          axis->set_iris(iris);
          break;
        case 'u':
          iris -= 500;
          printf("iris = %d\n", iris);
          axis->set_iris(iris);
          break;
        case '?':
          printf("focus: [%d]\n", axis->get_focus());
          break;
        default:
          printf("unknown char [%c]\n", c);
      }
    }
    usleep(10000); 
  }
  close_keyboard();

  delete axis;
  return 0;
}

