/*
 * Copyright (c) 2008, Maxim Likhachev
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
 *     * Neither the name of the University of Pennsylvania nor the names of its
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
#ifndef __HEADERS_H_
#define __HEADERS_H_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <vector>
#include <queue>

using namespace std;

#include "config.h"

#if MEM_CHECK == 1
#define _CRTDBG_MAP_ALLOC 
#define CRTDBG_MAP_ALLOC
#endif

#include <stdlib.h> //have to go after the defines above

#if MEM_CHECK == 1
#include <crtdbg.h>
#endif

#include <sbpl_arm_planner/utils/key.h>
#include <sbpl_arm_planner/utils/mdpconfig.h>
#include <sbpl_arm_planner/utils/mdp.h>
#include "planners/planner.h"
#include "discrete_space_information/environment.h"
#include <sbpl_arm_planner/robarm3d/environment_robarm3d.h>
#include <sbpl_arm_planner/utils/list.h>
#include <sbpl_arm_planner/utils/heap.h>
#include "planners/VI/viplanner.h"
#include "planners/ARAStar/araplanner.h"
#include "planners/ADStar/adplanner.h"
#include <sbpl_arm_planner/utils/utils.h>
#include <angles/angles.h>
#include <robot_kinematics/robot_kinematics.h>
#include <unistd.h>
#include <ros/node.h>

using namespace robot_kinematics;
using namespace KDL;

#endif

