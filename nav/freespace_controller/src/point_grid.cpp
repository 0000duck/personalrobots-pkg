/*********************************************************************
*
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
*
* Author: Eitan Marder-Eppstein
*********************************************************************/

#include <freespace_controller/point_grid.h>
#include <sys/time.h>

using namespace std;
using namespace std_msgs;

PointGrid::PointGrid(double size_x, double size_y, double resolution, Point2DFloat32 origin) :
  resolution_(resolution), origin_(origin)
{
  width_ = (int) ((size_x - origin_.x)/resolution_);
  height_ = (int) ((size_y - origin_.y)/resolution_);
  cells_.resize(width_ * height_);
}

bool PointGrid::legalFootprint(const Point2DFloat32& position, const vector<Point2DFloat32>& footprint, 
    double inner_square_radius, double outer_square_radius){
  //get all the points inside the circumscribed square of the robot footprint
  Point2DFloat32 c_lower_left, c_upper_right;
  c_lower_left.x = position.x - outer_square_radius;
  c_lower_left.y = position.y - outer_square_radius;

  c_upper_right.x = position.x + outer_square_radius;
  c_upper_right.y = position.y + outer_square_radius;

  //This may return points that are still outside of the cirumscribed square because it returns the cells
  //contained by the range
  vector< list<Point32>* > points = getPointsInRange(c_lower_left, c_upper_right);

  //if there are no points in the circumscribed square... we don't have to check against the footprint
  if(points.size() == 0)
    return true;

  //we'll also check against the inscribed square
  Point2DFloat32 i_lower_left, i_upper_right;
  i_lower_left.x = position.x - inner_square_radius;
  i_lower_left.y = position.y - inner_square_radius;

  i_upper_right.x = position.x + inner_square_radius;
  i_upper_right.y = position.y + inner_square_radius;

  //if there are points, we have to do a more expensive check
  for(unsigned int i = 0; i < points.size(); ++i){
    list<Point32>* cell_points = points[i];
    if(cell_points != NULL){
      for(list<Point32>::iterator it = cell_points->begin(); it != cell_points->end(); ++it){
        const Point32& pt = *it;
        //first, we'll check to make sure we're in the outer square
        //printf("(%.2f, %.2f) ... l(%.2f, %.2f) ... u(%.2f, %.2f)\n", pt.x, pt.y, c_lower_left.x, c_lower_left.y, c_upper_right.x, c_upper_right.y);
        if(pt.x > c_lower_left.x && pt.x < c_upper_right.x && pt.y > c_lower_left.y && pt.y < c_upper_right.y){
          //do a quick check to see if the point lies in the inner square of the robot
          if(pt.x > i_lower_left.x && pt.x < i_upper_right.x && pt.y > i_lower_left.y && pt.y < i_upper_right.y)
            return false;

          //now we really have to do a full footprint check on the point
          if(ptInPolygon(pt, footprint))
            return false;
        }
      }
    }
  }

  //if we get through all the points and none of them are in the footprint it's legal
  return true;
}
bool PointGrid::ptInPolygon(const Point32& pt, const vector<Point2DFloat32>& poly){
  if(poly.size() < 3)
    return false;

  //a point is in a polygon iff the orientation of the point
  //with respect to sides of the polygon is the same for every
  //side of the polygon
  bool all_left = false;
  bool all_right = false;
  for(unsigned int i = 0; i < poly.size() - 1; ++i){
    //if pt left of a->b
    if(orient(poly[i], poly[i + 1], pt) < 0){
      if(all_right)
        return false;
      all_left = true;
    }
    //if pt right of a->b
    else{
      if(all_left)
        return false;
      all_right = true;
    }
  }
  //also need to check the last point with the first point
  if(orient(poly[poly.size() - 1], poly[0], pt) < 0){
    if(all_right)
      return false;
  }
  else{
    if(all_left)
      return false;
  }

  return true;
}

vector< list<Point32>* > PointGrid::getPointsInRange(Point2DFloat32 lower_left, Point2DFloat32 upper_right){
  vector< list<Point32>* > points;

  //compute the other corners of the box so we can get cells indicies for them
  Point2DFloat32 upper_left, lower_right;
  upper_left.x = lower_left.x;
  upper_left.y = upper_right.y;
  lower_right.x = upper_right.x;
  lower_right.y = lower_left.y;

  //get the grid coordinates of the cells matching the corners of the range
  unsigned int gx, gy;
  
  //if the grid coordinates are outside the bounds of the grid... return
  if(!gridCoords(lower_left, gx, gy))
    return points;
  //get the associated index
  unsigned int lower_left_index = gridIndex(gx, gy);
  
  if(!gridCoords(lower_right, gx, gy))
    return points;
  unsigned int lower_right_index = gridIndex(gx, gy);

  if(!gridCoords(upper_left, gx, gy))
    return points;
  unsigned int upper_left_index = gridIndex(gx, gy);

  //compute x_steps and y_steps
  unsigned int x_steps = lower_right_index - lower_left_index + 1;
  unsigned int y_steps = (upper_left_index - lower_left_index) / width_ + 1;
  /*
   * (0, 0) ---------------------- (width, 0)
   *  |                               |
   *  |                               |
   *  |                               |
   *  |                               |
   *  |                               |
   * (0, height) ----------------- (width, height)
  */
  //get an iterator
  vector< list<Point32> >::iterator cell_iterator = cells_.begin() + lower_left_index;
  //printf("Index: %d, Width: %d, x_steps: %d, y_steps: %d\n", lower_left_index, width_, x_steps, y_steps);
  for(unsigned int i = 0; i < y_steps; ++i){
    for(unsigned int j = 0; j < x_steps; ++j){
      list<Point32>& cell = *cell_iterator;
      //if the cell contains any points... we need to push them back to our list
      if(cell.size() > 0){
        points.push_back(&cell);
      }
      if(j < x_steps - 1)
        cell_iterator++; //move a cell in the x direction
    }
    cell_iterator += width_ - (x_steps - 1); //move down a row
  }

  return points;
}

void PointGrid::insert(Point32 pt){
  //get the grid coordinates of the point
  unsigned int gx, gy;
  
  //if the grid coordinates are outside the bounds of the grid... return
  if(!gridCoords(pt, gx, gy))
    return;
  //get the associated index
  unsigned int pt_index = gridIndex(gx, gy);

  //insert the point into the grid at the correct location
  cells_[pt_index].push_back(pt);
  //printf("Index: %d, size: %d\n", pt_index, cells_[pt_index].size());
}

void PointGrid::removePointsInPolygon(const vector<Point2DFloat32> poly){
  if(poly.size() == 0)
    return;

  Point2DFloat32 lower_left, upper_right;
  lower_left.x = poly[0].x;
  lower_left.y = poly[0].y;
  upper_right.x = poly[0].x;
  upper_right.y = poly[0].y;

  //compute the containing square of the polygon
  for(unsigned int i = 1; i < poly.size(); ++i){
    lower_left.x = min(lower_left.x, poly[i].x);
    lower_left.y = min(lower_left.y, poly[i].y);
    upper_right.x = max(upper_right.x, poly[i].x);
    upper_right.y = max(upper_right.y, poly[i].y);
  }

  printf("Lower: (%.2f, %.2f), Upper: (%.2f, %.2f)\n", lower_left.x, lower_left.y, upper_right.x, upper_right.y);
  vector< list<Point32>* > points = getPointsInRange(lower_left, upper_right);

  //if there are no points in the containing square... we don't have to do anything
  if(points.size() == 0)
    return;

  //if there are points, we have to check them against the polygon explicitly to remove them
  for(unsigned int i = 0; i < points.size(); ++i){
    list<Point32>* cell_points = points[i];
    if(cell_points != NULL){
      list<Point32>::iterator it = cell_points->begin();
      while(it != cell_points->end()){
        const Point32& pt = *it;

        //check if the point is in the polygon and if it is, erase it from the grid
        if(ptInPolygon(pt, poly)){
          it = cell_points->erase(it);
        }
        else
          it++;
      }
    }
  }
}

void printPoint(Point32 pt){
  printf("(%.2f, %.2f, %.2f)", pt.x, pt.y, pt.z);
}

int main(int argc, char** argv){
  Point2DFloat32 origin;
  origin.x = 0.0;
  origin.y = 0.0;
  PointGrid pg(50.0, 50.0, 0.2, origin);
  /*
  double x = 10.0;
  double y = 10.0;
  for(int i = 0; i < 100; ++i){
    for(int j = 0; j < 100; ++j){
      Point32 pt;
      pt.x = x;
      pt.y = y;
      pt.z = 1.0;
      pg.insert(pt);
      x += .03;
    }
    y += .03;
    x = 10.0;
  }
  */
  vector<Point2DFloat32> footprint;
  Point2DFloat32 pt;

  pt.x = 1.0;
  pt.y = 1.0;
  footprint.push_back(pt);

  pt.x = 1.0;
  pt.y = 1.65;
  footprint.push_back(pt);

  pt.x = 1.325;
  pt.y = 1.75;
  footprint.push_back(pt);

  pt.x = 1.65;
  pt.y = 1.65;
  footprint.push_back(pt);

  pt.x = 1.65;
  pt.y = 1.0;
  footprint.push_back(pt);

  pt.x = 1.325;
  pt.y = 1.325;

  Point32 point;
  point.x = 1.00;
  point.y = 1.01;
  point.z = 1.0;

  for(unsigned int i = 0; i < 10000000; ++i){
    pg.insert(point);
  }
  //pg.removePointsInPolygon(footprint);
  
  struct timeval start, end;
  double start_t, end_t, t_diff;
  printf("Starting\n");
  gettimeofday(&start, NULL);
  bool legal = pg.legalFootprint(pt, footprint, 0.0, .95);
  gettimeofday(&end, NULL);
  start_t = start.tv_sec + double(start.tv_usec) / 1e6;
  end_t = end.tv_sec + double(end.tv_usec) / 1e6;
  t_diff = end_t - start_t;
  printf("Footprint calc: %.9f \n", t_diff);

  if(legal)
    printf("Legal footprint\n");
  else
    printf("Illegal footprint\n");

  return 0;
}
