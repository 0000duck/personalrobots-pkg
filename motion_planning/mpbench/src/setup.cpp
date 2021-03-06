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

/** \file setup.cpp  */

#include "setup.h"
#include "parse.h"
#include "world.h"
#include <costmap_2d/cost_values.h>
#include <sfl/util/strutil.hpp>

#ifdef MPBENCH_HAVE_NETPGM
extern "C" {
#include <stdio.h>
  // pgm.h is not very friendly with system headers... need to undef max() and min() afterwards
#include <pgm.h>
#undef max
#undef min
}
#endif // MPBENCH_HAVE_NETPGM

using sfl::to_string;
using namespace boost;
using namespace std;


namespace {  
  
  void drawDots(mpbench::Setup & setup,
		double hall, double door)
  {
    setup.legacyDrawPoint(   0,     0);
    setup.legacyDrawPoint(   0,  hall);
    setup.legacyDrawPoint(hall,     0);
    setup.legacyDrawPoint(hall,  hall);
    setup.legacyDrawPoint(door,  door);
    
    double const tol_xy(0.5 * door);
    double const tol_th(M_PI);
    setup.legacyAddTask("left to right", true,
			0, 0.5 * hall, 0,
			hall, 0.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("right to left", false,
			hall, 0.5 * hall, 0,
			0, 0.5 * hall, 0, tol_xy, tol_th);
  }
  
  
  void drawSquare(mpbench::Setup & setup,
		  double hall, double door)
  {
    setup.legacyDrawLine(   0,     0,  hall,     0);
    setup.legacyDrawLine(   0,     0,     0,  hall);
    setup.legacyDrawLine(   0,  hall,  hall,  hall);
    setup.legacyDrawLine(hall,     0,  hall,  hall);

    double const tol_xy(0.5 * door);
    double const tol_th(M_PI);
    setup.legacyAddTask("left to right", true,
			door, 0.5 * hall,   M_PI / 4,
			hall - door, 0.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("right to left", false,
			hall - door, 0.5 * hall, - M_PI / 4,
			door, 0.5 * hall, 0, tol_xy, tol_th);
  }
  
  
  void drawOffice1(mpbench::Setup & setup,
		   double hall, double door)
  {
    // outer bounding box
    setup.legacyDrawLine(       0,         0,  3 * hall,         0);
    setup.legacyDrawLine(       0,         0,         0,  5 * hall);
    setup.legacyDrawLine(       0,  5 * hall,  3 * hall,  5 * hall);
    setup.legacyDrawLine(3 * hall,         0,  3 * hall,  5 * hall);
    
    // two long walls along y-axis, each with a door near the northern end
    setup.legacyDrawLine(    hall,  3 * hall,      hall,  5 * hall - 2 * door);
    setup.legacyDrawLine(    hall,  5 * hall,      hall,  5 * hall -     door);
    setup.legacyDrawLine(2 * hall,      hall,  2 * hall,  5 * hall - 2 * door);
    setup.legacyDrawLine(2 * hall,  5 * hall,  2 * hall,  5 * hall -     door);
    
    // some shorter walls along x-axis
    setup.legacyDrawLine(         0,      hall,  0.5 * hall,      hall);
    setup.legacyDrawLine(         0,  2 * hall,  0.5 * hall,  2 * hall);
    setup.legacyDrawLine(1.5 * hall,      hall,  2   * hall,      hall);
    
    // y-axis wall with two office doors
    setup.legacyDrawLine(0.5*hall,              0,  0.5*hall,    hall - 2*door);
    setup.legacyDrawLine(0.5*hall,  hall -   door,  0.5*hall,    hall +   door);
    setup.legacyDrawLine(0.5*hall,  hall + 2*door,  0.5*hall,  2*hall         );
    
    double const tol_xy(0.25 * door);
    double const tol_th(M_PI);
    setup.legacyAddTask("through door or around hall", true,
			0.5 * hall, 4.5 * hall, 0,
			1.5 * hall, 4.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("through door or around hall, return trip", false,
			1.5 * hall, 4.5 * hall, 0,
			0.5 * hall, 4.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("through door or around long hall", true,
			1.5 * hall, 4.5 * hall, 0,
			2.5 * hall, 4.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("through door or around long hall, return trip", false,
			2.5 * hall, 4.5 * hall, 0,
			1.5 * hall, 4.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("office to office", true,
			0.25 * hall, 0.5 * hall, 0,
			0.25 * hall, 1.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("office to office, return trip", false,
			0.25 * hall, 1.5 * hall, 0,
			0.25 * hall, 0.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("office to hall", true,
			0.25 * hall, 0.5 * hall, 0,
			hall,       hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("office to hall, return trip", false,
			hall,        hall, 0,
			0.25 * hall, 0.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("hall to office", true,
			hall,       hall, 0,
			0.25 * hall, 1.5 * hall, 0, tol_xy, tol_th);
    setup.legacyAddTask("hall to office, return trip", false,
			0.25 * hall, 1.5 * hall, 0,
			hall,       hall, 0, tol_xy, tol_th);
  }
  
  
  void drawCubicle(mpbench::Setup & setup,
		   bool incremental,
		   bool minimal,
		   double hall, double door,
		   size_t ncube)
  {
    if (ncube < 2)
      ncube = 2;
    
    double const alpha(M_PI / (4 * (ncube - 1)));
    double const R0(hall / alpha);
    double const R1(R0 + hall);
    double const yoff(0.5 * hall * R1 / R0);
    
    // lowermost wall
    setup.legacyDrawLine(R0,
			 - 0.5 * hall + yoff,
			 R1 * cos( - alpha / 2),
			 R1 * sin( - alpha / 2) + yoff);
    
    for (size_t ii(0); ii < ncube; ++ii) {
      double const ux(cos(ii * alpha));
      double const uy(sin(ii * alpha));
      double const nx(-uy);
      double const ny(ux);
      
      // door
      setup.legacyDrawLine(R0 * ux - 0.5 * hall * nx,
			   R0 * uy - 0.5 * hall * ny + yoff,
			   R0 * ux - 0.5 * door * nx,
			   R0 * uy - 0.5 * door * ny + yoff);
      setup.legacyDrawLine(R0 * ux + 0.5 * door * nx,
			   R0 * uy + 0.5 * door * ny + yoff,
			   R0 * ux + 0.5 * hall * nx,
			   R0 * uy + 0.5 * hall * ny + yoff);
      // upper wall
      setup.legacyDrawLine(R0 * ux + 0.5 * hall * nx,
			   R0 * uy + 0.5 * hall * ny + yoff,
			   R1 * cos((ii + 0.5) * alpha),
			   R1 * sin((ii + 0.5) * alpha) + yoff);
      // hind wall
      setup.legacyDrawLine(R1 * cos((ii + 0.5) * alpha),
			   R1 * sin((ii + 0.5) * alpha) + yoff,
			   R1 * cos((ii - 0.5) * alpha),
			   R1 * sin((ii - 0.5) * alpha) + yoff);
    }
    
    double const tol_xy(0.25 * door);
    double const tol_th(M_PI);
    double const gx(0.5 * R0 * cos(M_PI / 8));
    double const gy(0.5 * R0 * sin(M_PI / 8) + yoff);
    double const alloc_time(0.001);
    for (size_t ii(0); ii < ncube; ++ii) {
      ostringstream os;
      os << "from hall to cubicle " << ii;
      if (incremental) {
	mpbench::episode::taskspec foo(os.str(),
				       mpglue::goalspec((R0 + hall / 2) * cos(ii * alpha),
								  (R0 + hall / 2) * sin(ii * alpha) + yoff,
								  0, tol_xy, tol_th));
	foo.start.push_back(mpglue::startspec(true, true, false, 3600, gx, gy, 0));
	foo.start.push_back(mpglue::startspec(false, false, true, alloc_time, gx, gy, 0));
	setup.addTask(foo);
      }
      else
	setup.legacyAddTask(os.str(), true, gx, gy, 0,
			    (R0 + hall / 2) * cos(ii * alpha),
			    (R0 + hall / 2) * sin(ii * alpha) + yoff,
			    0, tol_xy, tol_th);
      if (minimal)
	break;
      os << ", return trip";
      if (incremental) {
	mpbench::episode::taskspec foo(os.str(),
				       mpglue::goalspec(gx, gy, 0, tol_xy, tol_th));
	foo.start.push_back(mpglue::startspec(true, true, false, 3600,
							(R0 + hall / 2) * cos(ii * alpha),
							(R0 + hall / 2) * sin(ii * alpha) + yoff,
							0));
	foo.start.push_back(mpglue::startspec(false, false, true, alloc_time,
							(R0 + hall / 2) * cos(ii * alpha),
							(R0 + hall / 2) * sin(ii * alpha) + yoff,
							0));
	setup.addTask(foo);
      }
      else
	setup.legacyAddTask(os.str(), false,
			    (R0 + hall / 2) * cos(ii * alpha),
			    (R0 + hall / 2) * sin(ii * alpha) + yoff,
			    0, gx, gy, 0, tol_xy, tol_th);
    }
  }
  
  
#ifdef MPBENCH_HAVE_NETPGM
  void readNetPGM(mpbench::Setup & setup,
		  FILE * pgmfile,
		  unsigned int obstacle_gray,
		  bool invert_gray,
		  double resolution)
  {
    // not sure how to properly handle this, the doc does not say
    // whether it supports being initialized more than once...
    static bool pgm_has_been_initialized(false);
    if ( ! pgm_has_been_initialized) {
      static int fake_argc(1);
      static char * fake_arg("foo");
      pgm_init(&fake_argc, &fake_arg);
      pgm_has_been_initialized = true;
    }
    
    int ncols, nrows;
    gray maxval;
    int format;
    pgm_readpgminit(pgmfile, &ncols, &nrows, &maxval, &format);
    gray * row(pgm_allocrow(ncols));
    for (int ii(nrows - 1); ii >= 0; --ii) {
      pgm_readpgmrow(pgmfile, row, ncols, maxval, format);
      for (int jj(ncols - 1); jj >= 0; --jj)
	if ((       invert_gray  && (obstacle_gray >= row[jj]))
	    || (( ! invert_gray) && (obstacle_gray <= row[jj])))
	  setup.legacyDrawPoint(jj * resolution, ii * resolution);
    }
    pgm_freerow(row);
  }
#endif // MPBENCH_HAVE_NETPGM
  
  
  class OfficeBenchmark
    : public mpbench::Setup
  {
  protected:
    OfficeBenchmark(mpbench::SetupOptions const & options,
		    std::ostream * verbose_os, std::ostream * debug_os);
    
  public:
    static shared_ptr<OfficeBenchmark> create(mpbench::SetupOptions const & options,
					      std::ostream * verbose_os,
					      std::ostream * debug_os);
    
    virtual void dumpSubDescription(std::ostream & os,
				    std::string const & prefix) const;
    
    double door_width;
    double hall_width;
  };
  
  
  mpbench::Setup * createNetPGMBenchmark(mpbench::SetupOptions const & opt,
					 std::ostream * verbose_os, std::ostream * debug_os)
    throw(runtime_error);
  
  void readXML(string const & xmlFileName, mpbench::Setup * setup,
	       std::ostream * verbose_os, std::ostream * debug_os)
    throw(runtime_error);  
  
  mpbench::Setup * createXMLBenchmark(mpbench::SetupOptions const & opt,
				      std::ostream * verbose_os, std::ostream * debug_os)
    throw(runtime_error);
  
}


namespace mpbench {
  
  
  Setup::
  Setup(SetupOptions const & options,
	std::ostream * verbose_os,
	std::ostream * debug_os)
    : opt_(options),
      verbose_os_(verbose_os),
      debug_os_(debug_os),
      world_(new World(options, verbose_os, debug_os))
  {
  }
  
  
  Setup::
  ~Setup()
  {
#undef HUNT_SEGFAULT_AT_EXIT
#ifdef HUNT_SEGFAULT_AT_EXIT
    cerr << "DBG ~mpbench::Setup() clearing planners\n";
    planner_.clear();
    cerr << "DBG ~mpbench::Setup() resetting footprint\n";
    footprint_.reset();
    cerr << "DBG ~mpbench::Setup() resetting world\n";
    world_.reset();
    cerr << "DBG ~mpbench::Setup() clearing tasklist\n";
    tasklist_.clear();
    cerr << "DBG ~mpbench::Setup() DONE\n";
#endif // HUNT_SEGFAULT_AT_EXIT
  }
  
  
  void Setup::
  legacyDrawLine(double x0, double y0, double x1, double y1)
  {
    getWorld()->drawLine(0, true, x0, y0, x1, y1);
  }
  
  
  void Setup::
  legacyDrawPoint(double xx, double yy)
  {
    getWorld()->drawPoint(0, true, xx, yy);
  }
  
  
  void Setup::
  legacyAddTask(std::string const & description,
		bool from_scratch,
		double start_x, double start_y, double start_th, 
		double goal_x, double goal_y, double goal_th, 
		double goal_tol_xy, double goal_tol_th)
  {
    shared_ptr<episode::taskspec>
      setup(new episode::taskspec(description, mpglue::goalspec(goal_x, goal_y, goal_th,
								 goal_tol_xy, goal_tol_th)));
    setup->start.push_back(mpglue::startspec(from_scratch,
					      false, false, numeric_limits<double>::max(),
					      start_x, start_y, start_th));
    tasklist_.push_back(setup);
  }
  
  
  void Setup::
  addTask(episode::taskspec const & setup)
  {
    shared_ptr<episode::taskspec> foo(new episode::taskspec(setup));
    tasklist_.push_back(foo);
  }
  
  
  tasklist_t const & Setup::
  getTasks() const
  {
    return tasklist_;
  }
  
}

using namespace mpbench;

namespace {
  
  OfficeBenchmark::
  OfficeBenchmark(SetupOptions const & options,
		  std::ostream * verbose_os, std::ostream * debug_os)
    : Setup(options, verbose_os, debug_os),
      door_width(1.2),
      hall_width(3)
  {
    if (sfl::token_to(options.world_tok, 2, door_width) && debug_os)
      *debug_os << "mpbench::OfficeBenchmark: door_width set to " << door_width
		<< "\n" << flush;
    if (sfl::token_to(options.world_tok, 3, hall_width) && debug_os)
      *debug_os << "mpbench::OfficeBenchmark: hall_width set to " << hall_width
		<< "\n" << flush;
  }
  
  
  shared_ptr<OfficeBenchmark> OfficeBenchmark::
  create(SetupOptions const & options,
	 std::ostream * verbose_os,
	 std::ostream * debug_os)
  {
    shared_ptr<OfficeBenchmark> setup;
    string name;
    if ( ! sfl::token_to(options.world_tok, 1, name)) {
      if (verbose_os)
	*verbose_os << "OfficeBenchmark::create(): could not extract name from spec\n" << flush;
      return setup;
    }
    setup.reset(new OfficeBenchmark(options, verbose_os, debug_os));
    if ("dots" == name)
      drawDots(*setup, setup->hall_width, setup->door_width);
    else if ("square" == name)
      drawSquare(*setup, setup->hall_width, setup->door_width);
    else if ("office1" == name)
      drawOffice1(*setup, setup->hall_width, setup->door_width);
    else if ("cubicle" == name)
      drawCubicle(*setup, false, false, setup->hall_width, setup->door_width, 3);
    else if ("cubicle2" == name)
      drawCubicle(*setup, true, false, setup->hall_width, setup->door_width, 3);
    else if ("cubicle3" == name)
      drawCubicle(*setup, true, true, setup->hall_width, setup->door_width, 3);
    else {
      setup.reset();
      if (verbose_os)
	*verbose_os << "OfficeBenchmark::create(): invalid name \"" << name << "\", use one of:\n"
		    << "  dots: 4 dots in a square of hall_width side length\n"
		    << "  square: a square of hall_width side length\n"
		    << "  office1: two offices, two hallways, some doors\n" << flush;
    }
    return setup;
  }
  
  
  void OfficeBenchmark::
  dumpSubDescription(std::ostream & os,
		     std::string const & prefix) const
  {
    os << prefix << "door_width:           " << door_width << "\n"
       << prefix << "hall_width:           " << hall_width << "\n";
  }

}

namespace mpbench {  
  
  
  void SetupOptions::
  dump(std::ostream & os, std::string const & title, std::string const & prefix) const
  {
    if ( ! title.empty())
      os << title << "\n";
    os << prefix << "world spec:                   " << world_spec << "\n"
       << prefix << "costmap spec:                 " << costmap_spec << "\n"
       << prefix << "costmap_name:                 " << costmap_name << "\n"
       << prefix << "costmap_resolution:           " << costmap_resolution << "\n"
       << prefix << "costmap_inscribed_radius:     " << costmap_inscribed_radius << "\n"
       << prefix << "costmap_circumscribed_radius: " << costmap_circumscribed_radius << "\n"
       << prefix << "costmap_inflation_radius:     " << costmap_inflation_radius << "\n"
       << prefix << "costmap_obstacle_cost:        " << costmap_obstacle_cost << "\n";
    mpglue::requestspec::dump(os, "", prefix);
  }
  
  
  void Setup::
  dumpDescription(std::ostream & os, std::string const & title, std::string const & prefix) const
  {
    opt_.dump(os, title, prefix);
    dumpSubDescription(os, prefix);
    os << prefix << "tasks:\n";
    for (tasklist_t::const_iterator it(tasklist_.begin()); it != tasklist_.end(); ++it) {
      os << prefix << "  - " << (*it)->description;
      if ((*it)->start.empty())
	os << prefix << " (no episode)\n";
      else if (1 == (*it)->start.size())
	os << prefix << " (1 episode)\n";
      else
	os << prefix << " (" << (*it)->start.size() << " episodes)\n";
      os << prefix << "    goal " << (*it)->goal.px << "  " << (*it)->goal.py << "  "
	 << (*it)->goal.pth << "  " << (*it)->goal.tol_xy << "  " << (*it)->goal.tol_th << "\n";
      for (vector<mpglue::startspec>::const_iterator is((*it)->start.begin());
	   is != (*it)->start.end(); ++is)
	os << prefix << "    start " << to_string(is->from_scratch)
	   << " " << to_string(is->use_initial_solution)
	   << " " << to_string(is->allow_iteration)
	   << " " << is->alloc_time
	   << " " << is->px << "  " << is->py << "  " << is->pth << "\n";
      if ((*it)->door)
	os << prefix << "    door " << (*it)->door->px
	   << " " << (*it)->door->py << " " << (*it)->door->th_shut << " " << (*it)->door->th_open
	   << " " << (*it)->door->width << " " << (*it)->door->dhandle << "\n";
      else
	os << prefix << "    no door\n";
    }
  }
  
  
  void Setup::
  dumpSubDescription(std::ostream & os,
		     std::string const & prefix) const
  {
    // nop
  }
  
  
  namespace episode {
    
    
    taskspec::
    taskspec(std::string const & _description, mpglue::goalspec const & _goal)
      : description(_description),
	goal(_goal)
    {
    }
    
    
    taskspec::
    taskspec(std::string const & _description, mpglue::goalspec const & _goal, mpglue::doorspec const & _door)
      : description(_description),
	goal(_goal),
	door(new mpglue::doorspec(_door))
    {
    }
    
    
    taskspec::
    taskspec(taskspec const & orig)
      : description(orig.description),
	goal(orig.goal),
	start(orig.start)
    {
      if (orig.door)
	door.reset(new mpglue::doorspec(*orig.door));
    }
    
  }
  
  
  boost::shared_ptr<mpglue::CostmapPlanner> Setup::
  getPlanner(size_t task_id) throw(std::exception)
  {
    if (planner_.size() <= task_id) {
      if (debug_os_)
	*debug_os_ << "mpbench::Setup::getPlanner(): making space for task " << task_id << "\n";
      planner_.resize(task_id + 1);
    }
    
    shared_ptr<mpglue::CostmapPlanner> & planner(planner_[task_id]);
    if (planner)
      return planner;
    
    if (verbose_os_)
      *verbose_os_ << "mpbench::Setup::getPlanner(): allocating planner for task " << task_id
		   << " with spec " << opt_.planner_spec << "\n";
    
    shared_ptr<episode::taskspec> spec(tasklist_[task_id]);
    if ( ! spec)
      throw runtime_error("BUG??? in mpbench::Setup::getPlanner(" + to_string(task_id)
			  + "): no task spec for that ID");
    boost::shared_ptr<World> world(getWorld());
    planner.reset(createCostmapPlanner(opt_,
				       world->getCostmap(task_id),
				       world->getIndexTransform(),
				       getFootprint(),
				       spec->door.get(),
				       verbose_os_));
    
    if ( ! planner)
      throw runtime_error("mpbench::Setup::getPlanner(" + to_string(task_id)
			  + "): failed to create planner from spec \""
			  + opt_.planner_spec + "\"");
    
    return planner;
  }
  
  
  SetupOptions::
  SetupOptions(std::string const & _world_spec,
	       std::string const & planner_spec,
	       std::string const & robot_spec,
	       std::string const & _costmap_spec)
    : mpglue::requestspec(planner_spec, robot_spec),
      world_spec(_world_spec),
      costmap_spec(_costmap_spec),
      costmap_name("sfl"),
      costmap_resolution(0.05),
      costmap_inscribed_radius(0.325),
      costmap_circumscribed_radius(0.46),
      costmap_inflation_radius(0.55),
      costmap_obstacle_cost(costmap_2d::INSCRIBED_INFLATED_OBSTACLE)
  {
    sfl::tokenize(world_spec, ':', world_tok);
    sfl::tokenize(costmap_spec, ':', costmap_tok);
    
    sfl::token_to(costmap_tok, 0, costmap_name);
    if (sfl::token_to(costmap_tok, 1, costmap_resolution))
      costmap_resolution *= 1e-3;
    if (sfl::token_to(costmap_tok, 2, costmap_inscribed_radius))
      costmap_inscribed_radius *= 1e-3;
    if (sfl::token_to(costmap_tok, 3, costmap_circumscribed_radius))
      costmap_circumscribed_radius *= 1e-3;
    if (sfl::token_to(costmap_tok, 4, costmap_inflation_radius))
      costmap_inflation_radius *= 1e-3;
    sfl::token_to(costmap_tok, 5, costmap_obstacle_cost);
  }


  void SetupOptions::
  help(std::ostream & os, std::string const & title, std::string const & prefix)
  {
    if ( ! title.empty())
      os << title << "\n";
    os << prefix << "\navailable world specs:\n"
       << prefix << "  hc  : name [: door_width [: hall_width ]]\n"
       << prefix << "        name can be dots, square, office1, cubicle, cubcile2, cubicle3\n"
       << prefix << "        door_width and hall_width are in meters (default 1.2 and 3)\n"
       << prefix << "  pgm : obst_gray : pgm_filename [: xml_filename ]\n"
       << prefix << "        obst_gray is the gray level at which obstacles get inserted\n"
       << prefix << "            default is 64, use negative numbers to invert the scale\n"
       << prefix << "        xml_filename is optional, but needed to define tasks etc\n"
       << prefix << "  xml : xml_filename\n"
       << prefix << "\navailable costmap specs:\n"
       << prefix << "  sfl | ros [: resolution [: inscribed [: circumscribed [: inflation ]]]]\n"
       << prefix << "        all lengths and radii in millimeters\n"
       << prefix << "        defaults: resol. 50mm, inscr. 325mm, circ. 460mm, infl. 550mm\n";
    mpglue::requestspec::help(os, "", prefix);
  }
  
  
  shared_ptr<Setup> Setup::
  create(std::string const & world_spec,
	 std::string const & planner_spec,
	 std::string const & robot_spec,
	 std::string const & costmap_spec,
	 std::ostream * verbose_os,
	 std::ostream * debug_os) throw(std::runtime_error)
  {
    SetupOptions const opt(SetupOptions(world_spec,
					planner_spec,
					robot_spec,
					costmap_spec));
    
    if (verbose_os)
      *verbose_os << "creating world from \"" << opt.world_spec << "\"\n" << flush;
    
    shared_ptr<Setup> setup;
    if (opt.world_tok.empty())
      throw runtime_error("mpbench::Setup::create(): could not extract world format from \""
			  + opt.world_spec + "\"");
    if ("hc" == opt.world_tok[0])
      setup = OfficeBenchmark::create(opt, verbose_os, debug_os);
    else if ("pgm" == opt.world_tok[0])
      setup.reset(createNetPGMBenchmark(opt, verbose_os, debug_os));
    else if ("xml" == opt.world_tok[0])
      setup.reset(createXMLBenchmark(opt, verbose_os, debug_os));
    else
      throw runtime_error("mpbench::Setup::create(): invalid format \"" + opt.world_tok[0]
			  + "\" in spec \"" + opt.world_spec + "\"");
    
    if (verbose_os)
      *verbose_os << "finished creating setup\n" << flush;
    return setup;
  }
  
  
  mpglue::footprint_t const & Setup::
  getFootprint() const
  {
    if ( ! footprint_) {
      footprint_.reset(new mpglue::footprint_t());
      mpglue::initSimpleFootprint(*footprint_,
				  opt_.robot_inscribed_radius,
				  opt_.robot_circumscribed_radius);
    }
    return *footprint_;
  }
  
}

namespace {
  
  mpbench::Setup * createNetPGMBenchmark(SetupOptions const & opt,
					 std::ostream * verbose_os, std::ostream * debug_os)
    throw(runtime_error)
  {
    mpbench::Setup * bench(0);

#ifndef MPBENCH_HAVE_NETPGM
    
    if (verbose_os)
      *verbose_os << "ERROR in createNetPGMBenchmark(): no support for netpgm!\n"
		  << "  Install the netpbm libraries and headers and recompile.\n"
		  << "  For example under Ubuntu Feisty the package\n"
		  << "  libnetpbm10-dev does the trick.\n";
    throw runtime_error("sorry, no support for netpgm");
    
#else // MPBENCH_HAVE_NETPGM
    
    if (opt.world_tok.size() < 3) {
      ostringstream os;
      os << "createNetPGMBenchmark(): no PGM gray level and filename in world spec \""
	 << opt.world_spec << "\"";
      if (verbose_os)
	*verbose_os << "ERROR in " << os.str() << "\n";
      throw runtime_error(os.str());
    }
    
    unsigned int pgm_obstacle_gray(64);
    bool pgm_invert_gray(true);
    int obst_gray;
    if (sfl::token_to(opt.world_tok, 1, obst_gray)) {
      if (0 > obst_gray) {
	pgm_invert_gray = false;
	obst_gray = -obst_gray;
      }
      if (numeric_limits<gray>::max() <= obst_gray)
	pgm_obstacle_gray = numeric_limits<gray>::max();
      else
	pgm_obstacle_gray = obst_gray;
    }
    
    FILE * pgmfile(fopen(opt.world_tok[2].c_str(), "rb"));
    if ( ! pgmfile) {
      ostringstream os;
      os << "createNetPGMBenchmark(): fopen(" << opt.world_tok[2] << "): " << strerror(errno);
      if (verbose_os)
	*verbose_os << "ERROR in " << os.str() << "\n";
      throw runtime_error(os.str());
    }
    bench = new mpbench::Setup(opt, verbose_os, debug_os);
    if (verbose_os)
      *verbose_os << "createNetPGMBenchmark(): translating PGM file " << opt.world_tok[2] << "\n";
    readNetPGM(*bench, pgmfile, pgm_obstacle_gray, pgm_invert_gray,
	       opt.costmap_resolution);
    
    if (opt.world_tok.size() > 3)
      readXML(opt.world_tok[3], bench, verbose_os, debug_os);
    
#endif // MPBENCH_HAVE_NETPGM
    
    return bench;
  }
  
  
  void readXML(string const & xmlFileName, mpbench::Setup * setup,
	       std::ostream * verbose_os, std::ostream * debug_os)
    throw(runtime_error)
  {
    
#ifndef MPBENCH_HAVE_EXPAT
    
    if (verbose_os)
      *verbose_os << "no support for XML!\n"
		  << "  Install the expat library and recompile.\n"
		  << "  For example under Ubuntu Feisty the package\n"
		  << "  libexpat1-dev does the trick.\n";
    throw runtime_error("no support for XML (need expat library)");
    
#else // MPBENCH_HAVE_EXPAT
    
    mpbench::SetupParser sp;
    sp.Parse(xmlFileName, setup, verbose_os, debug_os);
    
#endif // MPBENCH_HAVE_EXPAT    
    
  }
  
  
  mpbench::Setup * createXMLBenchmark(SetupOptions const & opt,
				      std::ostream * verbose_os, std::ostream * debug_os)
    throw(runtime_error)
  {
    mpbench::Setup * bench(0);
    
#ifndef MPBENCH_HAVE_EXPAT
    
    if (verbose_os)
      *verbose_os << "no support for XML!\n"
		  << "  Install the expat library and recompile.\n"
		  << "  For example under Ubuntu Feisty the package\n"
		  << "  libexpat1-dev does the trick.\n";
    throw runtime_error("sorry, no support for XML");
    
#else // MPBENCH_HAVE_EXPAT
    
    if (opt.world_tok.size() < 2) {
      ostringstream os;
      os << "createXMLBenchmark(): no XML filename in world spec \"" << opt.world_spec << "\"";
      if (verbose_os)
	*verbose_os << "ERROR in " << os.str() << "\n";
      throw runtime_error(os.str());
    }
    
    bench = new mpbench::Setup(opt, verbose_os, debug_os);
    if (verbose_os)
      *verbose_os << "createXMLBenchmark(): parsing " << opt.world_tok[1] << "\n";
    readXML(opt.world_tok[1], bench, verbose_os, debug_os);
    
#endif // MPBENCH_HAVE_EXPAT
    
    return bench;
  }

}
