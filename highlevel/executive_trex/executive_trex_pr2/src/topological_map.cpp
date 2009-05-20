/**
 * @author Conor McGann
 */
#include <executive_trex_pr2/topological_map.h>
#include <executive_trex_pr2/components.h>
#include <ros/console.h>
#include <tf/transform_datatypes.h>
#include "ConstrainedVariable.hh"
#include "Utilities.hh"
#include "Token.hh"
#include "Domains.hh"
#include "Utilities.hh"

using namespace EUROPA;
using namespace TREX;

namespace executive_trex_pr2 {


  //*******************************************************************************************
  MapInitializeFromFileConstraint::MapInitializeFromFileConstraint(const LabelStr& name,
								   const LabelStr& propagatorName,
								   const ConstraintEngineId& constraintEngine,
								   const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables), _map(NULL){
    checkError(variables.size() == 1, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    const StringDomain& dom = static_cast<const StringDomain&>(getCurrentDomain(variables[0]));
    checkError(dom.isSingleton(), dom.toString() << " is not a singleton when it should be.");
    const LabelStr fileName = dom.getSingletonValue();
    std::string fileNameStr = TREX::findFile(fileName.toString());
    std::ifstream is(fileNameStr.c_str());
    ROS_INFO("Loading the topological map from file");
    _map = new TopologicalMapAdapter(is);
  }

  MapInitializeFromFileConstraint::~MapInitializeFromFileConstraint(){
    delete _map;
  }

  //*******************************************************************************************
  MapNotifyDoorBlockedConstraint::MapNotifyDoorBlockedConstraint(const LabelStr& name,
								 const LabelStr& propagatorName,
								 const ConstraintEngineId& constraintEngine,
								 const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables), 
      _doorway(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[0]))){
    checkError(variables.size() == 1, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
  }

  void MapNotifyDoorBlockedConstraint::handleExecute(){
    if(TopologicalMapAdapter::instance() == NULL){
      debugMsg("map", "No topological map present!");
      return;
    }

    if(_doorway.isSingleton()){
      debugMsg("map", "Notification that doorway [" << _doorway.getSingletonValue() << "] is blocked.");
      TopologicalMapAdapter::instance()->observeDoorBlocked(_doorway.getSingletonValue());
    }
  }

  //*******************************************************************************************
  MapNotifyOutletBlockedConstraint::MapNotifyOutletBlockedConstraint(const LabelStr& name,
								     const LabelStr& propagatorName,
								     const ConstraintEngineId& constraintEngine,
								     const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables), 
      _outlet(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[0]))){
    checkError(variables.size() == 1, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }

  void MapNotifyOutletBlockedConstraint::handleExecute(){
    if(_outlet.isSingleton()){
      debugMsg("map", "Notification that outlet [" << _outlet.getSingletonValue() << "] is blocked.");
      TopologicalMapAdapter::instance()->observeOutletBlocked(_outlet.getSingletonValue());
    }
  }

  //*******************************************************************************************
  MapGetNearestOutletConstraint::MapGetNearestOutletConstraint(const LabelStr& name,
							       const LabelStr& propagatorName,
							       const ConstraintEngineId& constraintEngine,
							       const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _outlet(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[0]))),
     _x(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
     _y(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))){
    checkError(variables.size() == 3, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  /**
   * If the position is bound, we can make a region query. The result should be intersected on the domain.
   */
  void MapGetNearestOutletConstraint::handleExecute(){
    // Wait till inputs are bound.
    if(!_x.isSingleton() || !_y.isSingleton())
      return;

    debugMsg("map:get_nearest_outlet",  "BEFORE: "  << TREX::timeString() << toString());

    double x = _x.getSingletonValue();
    double y = _y.getSingletonValue();
    unsigned int outlet_id = TopologicalMapAdapter::instance()->getNearestOutlet(x, y);

    checkError(outlet_id > 0, "No outlet found for <" << x << ", " << y << ">");

    _outlet.set(outlet_id);

    debugMsg("map:get_nearest_outlet",  "AFTER: "  << TREX::timeString() << toString());
  }

  //*******************************************************************************************
  MapGetOutletApproachPoseConstraint::MapGetOutletApproachPoseConstraint(const LabelStr& name,
									 const LabelStr& propagatorName,
									 const ConstraintEngineId& constraintEngine,
									 const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _x(static_cast<IntervalDomain&>(getCurrentDomain(variables[0]))),
     _y(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
     _z(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))),
     _qx(static_cast<IntervalDomain&>(getCurrentDomain(variables[3]))),
     _qy(static_cast<IntervalDomain&>(getCurrentDomain(variables[4]))),
     _qz(static_cast<IntervalDomain&>(getCurrentDomain(variables[5]))),
     _qw(static_cast<IntervalDomain&>(getCurrentDomain(variables[6]))),
     _outlet_id(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[7]))){
    checkError(variables.size() == 8, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  /**
   * If the position is bound, we can make a region query. The result should be intersected on the domain.
   */
  void MapGetOutletApproachPoseConstraint::handleExecute(){

    // Wait till inputs are bound.
    if(!_outlet_id.isSingleton())
      return;

    // If outlet id is 0 then skip.
    if(_outlet_id.getSingletonValue() == 0){
      debugMsg("map:get_appoach_pose", "Ignoring outlet id since it is a NO_KEY");
      return;
    }

    debugMsg("map:get_appoach_pose",  "BEFORE: "  << TREX::timeString() << toString());

    try{
      robot_msgs::Pose pose;
      TopologicalMapAdapter::instance()->getOutletApproachPose(_outlet_id.getSingletonValue(), pose);
      _x.set(pose.position.x);
      _y.set(pose.position.y);
      _z.set(pose.position.z);
      _qx.set(pose.orientation.x);
      _qy.set(pose.orientation.y);
      _qz.set(pose.orientation.z);
      _qw.set(pose.orientation.w);
    }
    catch(...){
      debugMsg("trex:warning", "Invalid outlet id (" << _outlet_id.getSingletonValue() << ")");
      _outlet_id.empty();
    }

    debugMsg("map:get_approach_pose",  "AFTER: "  << TREX::timeString() << toString());
  }

  //*******************************************************************************************
  MapGetOutletStateConstraint::MapGetOutletStateConstraint(const LabelStr& name,
							    const LabelStr& propagatorName,
							    const ConstraintEngineId& constraintEngine,
							    const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _x(static_cast<IntervalDomain&>(getCurrentDomain(variables[0]))),
     _y(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
     _z(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))),
     _qx(static_cast<IntervalDomain&>(getCurrentDomain(variables[3]))),
     _qy(static_cast<IntervalDomain&>(getCurrentDomain(variables[4]))),
     _qz(static_cast<IntervalDomain&>(getCurrentDomain(variables[5]))),
     _qw(static_cast<IntervalDomain&>(getCurrentDomain(variables[6]))),
     _outlet(static_cast<const IntervalIntDomain&>(getCurrentDomain(variables[7]))){
    checkError(variables.size() == 8, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  /**
   * If the position is bound, we can make a region query. The result should be intersected on the domain.
   */
  void MapGetOutletStateConstraint::handleExecute(){

    // Wait till inputs are bound.
    if(!_outlet.isSingleton())
      return;

    debugMsg("map:get_outlet_state",  "BEFORE: "  << TREX::timeString() << toString());
    robot_msgs::Pose outlet_pose;
    TopologicalMapAdapter::instance()->getOutletState(_outlet.getSingletonValue(), outlet_pose);
    _x.set(outlet_pose.position.x);
    _y.set(outlet_pose.position.y);
    _z.set(outlet_pose.position.z);
    _qx.set(outlet_pose.orientation.x);
    _qy.set(outlet_pose.orientation.y);
    _qz.set(outlet_pose.orientation.z);
    _qw.set(outlet_pose.orientation.w);

    debugMsg("map:get_outlet_state",  "AFTER: "  << TREX::timeString() << toString());
  }

  //*******************************************************************************************
  MapGetNextMoveConstraint::MapGetNextMoveConstraint(const LabelStr& name,
						     const LabelStr& propagatorName,
						     const ConstraintEngineId& constraintEngine,
						     const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _next_x(static_cast<AbstractDomain&>(getCurrentDomain(variables[0]))),
     _next_y(static_cast<AbstractDomain&>(getCurrentDomain(variables[1]))),
     _next_z(static_cast<AbstractDomain&>(getCurrentDomain(variables[2]))),
     _next_qx(static_cast<AbstractDomain&>(getCurrentDomain(variables[3]))),
     _next_qy(static_cast<AbstractDomain&>(getCurrentDomain(variables[4]))),
     _next_qz(static_cast<AbstractDomain&>(getCurrentDomain(variables[5]))),
     _next_qw(static_cast<AbstractDomain&>(getCurrentDomain(variables[6]))),
     _thru_doorway(static_cast<BoolDomain&>(getCurrentDomain(variables[7]))),
     _current_x(static_cast<AbstractDomain&>(getCurrentDomain(variables[8]))),
     _current_y(static_cast<AbstractDomain&>(getCurrentDomain(variables[9]))),
     _target_x(static_cast<AbstractDomain&>(getCurrentDomain(variables[10]))),
     _target_y(static_cast<AbstractDomain&>(getCurrentDomain(variables[11]))){
    checkError(variables.size() == 12, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  //*******************************************************************************************
  void MapGetNextMoveConstraint::handleExecute(){

    // Wait till inputs are bound. 
    if(!_current_x.isSingleton() || !_current_y.isSingleton() || !_target_x.isSingleton() || !_target_y.isSingleton()) {
      debugMsg("map:get_next_move",  "Exiting as inputs are not all bound");
      return;
    }


    // If ouputs are already bound, then do nothing
    static const std::string OUTPUT_PARAMS(":x:y:z:qx:qy:qz:qw");
    if(allSingletons(getScope(), OUTPUT_PARAMS)){
      debugMsg("map:get_next_move",  "Exiting as outputs are all bound");
      return;
    }

    debugMsg("map:get_next_move",  "BEFORE: "  << TREX::timeString() << toString());

    // Get next move by evaluating all options. This will be the lowest cost
    double lowest_cost = PLUS_INFINITY;
    double next_x(0.0), next_y(0.0);

    // Obtain the values
    double current_x = _current_x.getSingletonValue();
    double current_y = _current_y.getSingletonValue();
    double target_x = _target_x.getSingletonValue();
    double target_y = _target_y.getSingletonValue();

    unsigned int this_region =  TopologicalMapAdapter::instance()->getRegion(current_x, current_y);
    condDebugMsg(this_region == 0, "map", "No region for <" << current_x << ", " << current_y <<">");
    unsigned int final_region =  TopologicalMapAdapter::instance()->getRegion(target_x, target_y);
    condDebugMsg(final_region == 0, "map", "No region for <" << target_x << ", " << target_y <<">");

    debugMsg("map:get_next_move", "Current region is " << this_region << " for <" << current_x << ", " << current_y <<">");
    debugMsg("map:get_next_move", "Target region is " << final_region << " for <" << target_x << ", " << target_y <<">");

    // If the final region is bogus, then force a conflict.
    if(final_region == 0 || this_region == 0){
      _thru_doorway.empty();
      debugMsg("map:get_next_move",  "Exiting due to invalid region points");
      return;
    }

    // If the source point and final point can be connected without traversing a region then we select based on that
    if(this_region == final_region){
      lowest_cost = TopologicalMapAdapter::instance()->cost(current_x, current_y, target_x, target_y);
      next_x = target_x;
      next_y = target_y;
      debugMsg("map:get_next_move",  "Including going directly to target since current and target are in the same region. Cost is: " << lowest_cost);
    }

    unsigned int next_connector = TopologicalMapAdapter::instance()->getNextConnector(current_x, current_y, target_x, target_y, lowest_cost, next_x, next_y);

    // If there is no next connector, and this_region != final_region, the move is infeasible
    if(next_connector == 0 && this_region != final_region){
      _thru_doorway.empty();
      debugMsg("map:get_next_move",  "Exiting since there is no move available.");
      return;
    }

    bool thru_doorway = TopologicalMapAdapter::instance()->isDoorway(current_x, current_y, next_x, next_y);

    // If not a doorway, and not the final move, then it must be an approach to a doorway. If that is the case, we must modify
    // the x and y based on the approach point
    if(next_connector != 0 && !thru_doorway && TopologicalMapAdapter::instance()->isDoorwayConnector(next_connector)){
      robot_msgs::Pose pose;
      TopologicalMapAdapter::instance()->getDoorApproachPose(next_connector, pose);
      _next_x.set(pose.position.x);
      _next_y.set(pose.position.y);
      _next_z.set(pose.position.z);
      _next_qx.set(pose.orientation.x);
      _next_qy.set(pose.orientation.y);
      _next_qz.set(pose.orientation.z);
      _next_qw.set(pose.orientation.w);
      debugMsg("map:get_next_move",  "Selecting approach for doorway connector " << next_connector << " at <" << pose.position.x << ", " << pose.position.y << ">.");
    }
    else {
      _next_x.set(next_x);
      _next_y.set(next_y);
      _next_z.set(0.0);
      _next_qx.set(0.0);
      _next_qy.set(0.0);
      _next_qz.set(0.0);
      _next_qw.set(1.0);
    }

    _thru_doorway.set(thru_doorway);

    debugMsg("map:get_next_move",  "AFTER: "  << TREX::timeString() << toString());
  }

  //*******************************************************************************************
  MapConnectorConstraint::MapConnectorConstraint(const LabelStr& name,
						 const LabelStr& propagatorName,
						 const ConstraintEngineId& constraintEngine,
						 const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _connector(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[0]))),
     _x(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
     _y(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))){
    checkError(variables.size() == 3, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  /**
   * @brief When the connector id is bound, we bind the values for x, y, and th based on a lookup in the topological map.
   * We can also bind the value for the connector id based on x and y inputs. This enforces the full relation.
   */
  void MapConnectorConstraint::handleExecute(){
    debugMsg("map:connector",  "BEFORE: "  << TREX::timeString() << toString());
    if(_connector.isSingleton()){
      unsigned int connector_id = (unsigned int) _connector.getSingletonValue();
      double x, y;

      // If we have a pose, bind the variables
      if(TopologicalMapAdapter::instance()->getConnectorPosition(connector_id, x, y)){
	_x.set(x);

	if(!_x.isEmpty()) 
	  _y.set(y);
      }
    }

    // If x and y are set, we can bind the connector.
    if(_x.isSingleton() && _y.isSingleton()){
      unsigned int connector_id = TopologicalMapAdapter::instance()->getConnector(_x.getSingletonValue(), _y.getSingletonValue());
      _connector.set(connector_id);
    }
    debugMsg("map:get_connector",  "AFTER: " << TREX::timeString()  << toString());
  }

  //*******************************************************************************************
  MapGetRegionFromPositionConstraint::MapGetRegionFromPositionConstraint(const LabelStr& name,
									 const LabelStr& propagatorName,
									 const ConstraintEngineId& constraintEngine,
									 const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _region(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[0]))),
     _x(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
     _y(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))){
    checkError(variables.size() == 3, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  /**
   * If the position is bound, we can make a region query. The result should be intersected on the domain.
   */
  void MapGetRegionFromPositionConstraint::handleExecute(){
    if(!_x.isSingleton() || !_y.isSingleton())
      return;

    debugMsg("map:get_region_from_position",  "BEFORE: "  << TREX::timeString() << toString());

    double x = _x.getSingletonValue();
    double y = _y.getSingletonValue();

    unsigned int region_id = TopologicalMapAdapter::instance()->getRegion(x, y);
    _region.set(region_id);

    debugMsg("map:get_region_from_position",  "AFTER: " << TREX::timeString()  << toString());

    condDebugMsg(_region.isEmpty(), "map:get_region_from_position", 
		 "isObstacle(" << x << ", " << y << ") == " << (TopologicalMapAdapter::instance()->isObstacle(x, y) ? "TRUE" : "FALSE"));
  } 

  //*******************************************************************************************
  MapGetDoorwayFromPointsConstraint::MapGetDoorwayFromPointsConstraint(const LabelStr& name,
									 const LabelStr& propagatorName,
									 const ConstraintEngineId& constraintEngine,
									 const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _region(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[0]))),
     _x1(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
     _y1(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))),
     _x2(static_cast<IntervalDomain&>(getCurrentDomain(variables[3]))),
     _y2(static_cast<IntervalDomain&>(getCurrentDomain(variables[4]))){
    checkError(variables.size() == 5, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  /**
   * If the position is bound, we can make a region query. The result should be intersected on the domain.
   */
  void MapGetDoorwayFromPointsConstraint::handleExecute(){
    // Wait till inputs are bound. This is a common pattern for functions and could be mapped into a base class with an input and output
    // list
    if(!_x1.isSingleton() || !_y1.isSingleton() || !_x2.isSingleton() || !_y2.isSingleton())
      return;

    debugMsg("map:get_doorway_from_points",  "BEFORE: "  << TREX::timeString() << toString());

    unsigned int region_id = TopologicalMapAdapter::instance()->getNearestDoorway(_x2.getSingletonValue(), _y2.getSingletonValue());

    // If it is not a doorway, then set to zero
    getCurrentDomain(getScope()[0]).set(region_id);

    debugMsg("map:get_doorway_from_points",  "AFTER: "  << TREX::timeString() << toString());
  }
    
  //*******************************************************************************************
  MapIsDoorwayConstraint::MapIsDoorwayConstraint(const LabelStr& name,
						 const LabelStr& propagatorName,
						 const ConstraintEngineId& constraintEngine,
						 const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _result(static_cast<BoolDomain&>(getCurrentDomain(variables[0]))),
     _region(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[1]))){
    checkError(variables.size() == 2, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
    checkError(TopologicalMapAdapter::instance() != NULL, "Failed to allocate topological map accessor. Some configuration error.");
  }
    
  void MapIsDoorwayConstraint::handleExecute(){
    // If inputs are not bound, return
    if(!_region.isSingleton())
      return;

    debugMsg("map:is_doorway",  "BEFORE: " << TREX::timeString()  << toString());

    unsigned int region_id = _region.getSingletonValue();
    bool is_doorway(true);

    TopologicalMapAdapter::instance()->isDoorway(region_id, is_doorway);
    _result.set(is_doorway);

    debugMsg("map:is_doorway",  "AFTER: " << TREX::timeString()  << toString());
  }

  //*******************************************************************************************
  MapGetDoorStateConstraint::MapGetDoorStateConstraint(const LabelStr& name,
						       const LabelStr& propagatorName,
						       const ConstraintEngineId& constraintEngine,
						       const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables),
      _token_id(TREX::getParentToken(variables[0])),
      _door_id(static_cast<IntervalIntDomain&>(getCurrentDomain(variables[1]))){
    checkError(variables.size() == 2, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
  }

  void MapGetDoorStateConstraint::handleExecute(){
    // If the door id is not a singleton then there is nothing to do
    if(!_door_id.isSingleton())
      return;

    debugMsg("map:get_door_state",  "BEFORE: " << TREX::timeString()  << toString());

    unsigned int id = (unsigned int)_door_id.getSingletonValue();

    door_msgs::Door door_state;
    if(!TopologicalMapAdapter::instance()->getDoorState(id, door_state)){
      debugMsg("map:get_door_state",  "No door message available for " << id);
      _door_id.empty();
    }
    else {
      static const LabelStr MAP_FRAME("map");
      if(!apply(MAP_FRAME, "frame_id") ||
	 !apply(door_state.frame_p1.x, "frame_p1_x") ||
	 !apply(door_state.frame_p1.y, "frame_p1_y") ||
	 !apply(door_state.frame_p1.z, "frame_p1_z") ||
	 !apply(door_state.frame_p2.x, "frame_p2_x") ||
	 !apply(door_state.frame_p2.y, "frame_p2_y") ||
	 !apply(door_state.frame_p2.z, "frame_p2_z") ||
	 !apply(door_state.height, "height") ||
	 !apply(door_state.hinge, "hinge") ||
	 !apply(door_state.rot_dir, "rot_dir") ||
	 !apply(door_state.door_p1.x, "door_p1_x") ||
	 !apply(door_state.door_p1.y, "door_p1_y") ||
	 !apply(door_state.door_p1.z, "door_p1_z") ||
	 !apply(door_state.door_p2.x, "door_p2_x") ||
	 !apply(door_state.door_p2.y, "door_p2_y") ||
	 !apply(door_state.door_p2.z, "door_p2_z") ||
	 !apply(door_state.handle.x, "handle_x") ||
	 !apply(door_state.handle.y, "handle_y") ||
	 !apply(door_state.handle.z, "handle_z")||
	 !apply(door_state.latch_state, "latch_state")) {
      }
    }
      
    debugMsg("map:get_door_state",  "AFTER: " << TREX::timeString()  << toString());
  }
  
  std::string MapGetDoorStateConstraint::toString() const {
    std::stringstream ss;
    ss << Constraint::toString() << std::endl;

    for(std::vector<ConstrainedVariableId>::const_iterator it = _token_id->parameters().begin(); it != _token_id->parameters().end(); ++it){
      ConstrainedVariableId var = *it;
      ss << var->getName().toString() << "(" << var->getKey() << ") == " << var->lastDomain().toString() << std::endl;
    }

    return ss.str();
  }

  /**
   * Reads a numeric value into a token parameter by name
   */
  bool MapGetDoorStateConstraint::apply(double value, const char* param_name){
    ConstrainedVariableId var = _token_id->getVariable(param_name);
    ROS_ASSERT(var.isValid());

    AbstractDomain& dom = getCurrentDomain(var);
    condDebugMsg(dom.isNumeric(), "map:get_door_state", "Setting " << value  << " for " << param_name << " in " << dom.toString());
    condDebugMsg(!dom.isNumeric(), "map:get_door_state", "Setting " << LabelStr(value).toString() << " for " << param_name << " in " << dom.toString());
    dom.set(value);
    return !dom.isEmpty();
  }

  //*******************************************************************************************
  MapConnectorFilter::MapConnectorFilter(const TiXmlElement& configData)
    : SOLVERS::FlawFilter(configData, true),
      _source_x(extractData(configData, "source_x")),
      _source_y(extractData(configData, "source_y")),
      _final_x(extractData(configData, "final_x")),
      _final_y(extractData(configData, "final_y")),
      _target_connector(extractData(configData, "target_connector")){}

  /**
   * The filter will exclude the entity if it is not the target variable of a token with the right structure
   * with all inputs bound so that the selection can be made based on sufficient data
   */
  bool MapConnectorFilter::test(const EntityId& entity){
    // It should be a variable. So if it is not, filter it out
    if(!ConstrainedVariableId::convertable(entity))
      return true;

    // Its name should match the target, it should be enumerated, and it should not be a singleton.
    ConstrainedVariableId var = (ConstrainedVariableId) entity;
    if(var->getName() != _target_connector || !var->lastDomain().isNumeric()){
      debugMsg("MapConnectorFilter:test", "Excluding " << var->getName().toString());
      return true;
    }

    // It should have a parent that is a token
    if(var->parent().isNoId() || !TokenId::convertable(var->parent()))
      return true;
    
    TokenId parent = (TokenId) var->parent();
    ConstrainedVariableId source_x = parent->getVariable(_source_x, false);
    ConstrainedVariableId source_y = parent->getVariable(_source_y, false);
    ConstrainedVariableId final_x = parent->getVariable(_final_x, false);
    ConstrainedVariableId final_y = parent->getVariable(_final_y, false);
  
    return(noGoodInput(parent, _source_x) ||
	   noGoodInput(parent, _source_y) ||
	   noGoodInput(parent, _final_x) ||
	   noGoodInput(parent, _final_y));
  }

  bool MapConnectorFilter::noGoodInput(const TokenId& parent_token, const LabelStr& var_name) const {
    ConstrainedVariableId var = parent_token->getVariable(var_name, false);
    bool result = true;
    if(var.isNoId()){
      debugMsg("MapConnectorFilter:test", "Excluding because of invalid source:" << var_name.toString());
    }
    else if(!var->lastDomain().isSingleton()){
      debugMsg("MapConnectorFilter:test", 
	       "Excluding because of unbound inputs on:" << 
	       var_name.toString() << "[" << var->getKey() << "] with " << var->lastDomain().toString());
    }
    else{
      result = false;
    }

    return result;
  }

  //*******************************************************************************************
  MapConnectorSelector::MapConnectorSelector(const DbClientId& client, const ConstrainedVariableId& flawedVariable, const TiXmlElement& configData, const LabelStr& explanation)
    : SOLVERS::UnboundVariableDecisionPoint(client, flawedVariable, configData, explanation),
      _source_x(extractData(configData, "source_x")),
      _source_y(extractData(configData, "source_y")),
      _final_x(extractData(configData, "final_x")),
      _final_y(extractData(configData, "final_y")),
      _target_connector(extractData(configData, "target_connector")){

    // Verify expected structure for the variable - wrt other token parameters
    checkError(flawedVariable->parent().isId() && TokenId::convertable(flawedVariable->parent()), 
	       "The variable configuration is incorrect. The Map_connector_filter must not be working. Expect a token parameter with source connector and final region.");


    // Obtain source and final 
    TokenId parent = (TokenId) flawedVariable->parent();
    ConstrainedVariableId source_x = parent->getVariable(_source_x, false);
    ConstrainedVariableId source_y = parent->getVariable(_source_y, false);
    ConstrainedVariableId final_x = parent->getVariable(_final_x, false);
    ConstrainedVariableId final_y = parent->getVariable(_final_y, false);

    // Double check source variables
    checkError(source_x.isId(), "Bad filter. Invalid source name: " << _source_x.toString() << " on token " << parent->toString());
    checkError(source_y.isId(), "Bad filter. Invalid source name: " << _source_y.toString() << " on token " << parent->toString());
    checkError(source_x->lastDomain().isSingleton(), "Bad Filter. Source is not a singleton: " << source_x->toString());
    checkError(source_y->lastDomain().isSingleton(), "Bad Filter. Source is not a singleton: " << source_y->toString());

    // Double check destination variables
    checkError(final_x.isId(), "Bad filter. Invalid final name: " << _final_x.toString() << " on token " << parent->toString());
    checkError(final_y.isId(), "Bad filter. Invalid final name: " << _final_y.toString() << " on token " << parent->toString());
    checkError(final_x->lastDomain().isSingleton(), "Bad Filter. Final is not a singleton: " << final_x->toString());
    checkError(final_y->lastDomain().isSingleton(), "Bad Filter. Final is not a singleton: " << final_y->toString());

    // Compose all the choices. 
    // Given a connector, and a target, generate the choices for all connectors
    // adjacent to this connector, and order them to minimize cost.
    double x0, y0, x1, y1;
    x0 = source_x->lastDomain().getSingletonValue();
    y0 = source_y->lastDomain().getSingletonValue();
    x1 = final_x->lastDomain().getSingletonValue();
    y1 = final_y->lastDomain().getSingletonValue();

    TopologicalMapAdapter::instance()->getLocalConnectionsForGoal(_sorted_choices, x0, y0, x1, y1);

    _sorted_choices.sort();
    _choice_iterator = _sorted_choices.begin();

    debugMsg("map", "Evaluating from <" << x0 << ", " << y0 << "> to <" << x1 << ", " << y1 << ">. " << toString());
  }

  std::string MapConnectorSelector::toString() const {
    std::stringstream ss;
    ss << "Choices are: " << std::endl;
    for(std::list<ConnectionCostPair>::const_iterator it = _sorted_choices.begin(); it != _sorted_choices.end(); ++it){
      const ConnectionCostPair& choice = *it;
      double x, y;
      TopologicalMapAdapter::instance()->getConnectorPosition(choice.id, x, y);

      if(choice.id == 0)
	ss << "<0, GOAL, " << choice.cost << ">" << std::endl;
      else
	ss << "<" << choice.id << ", (" << x << ", " << y << ") " << choice.cost << ">" << std::endl;
    }

    return ss.str();
  }

  bool MapConnectorSelector::hasNext() const { return _choice_iterator != _sorted_choices.end(); }

  double MapConnectorSelector::getNext(){
    ConnectionCostPair c = *_choice_iterator;
    debugMsg("MapConnectorSelector::getNext", "Selecting " << c.id << std::endl);
    ++_choice_iterator;
    return c.id;
  }

  /************************************************************************
   * Map Adapter implementation
   ************************************************************************/

  TopologicalMapAdapter* TopologicalMapAdapter::_singleton = NULL;

  TopologicalMapAdapter* TopologicalMapAdapter::instance(){
    return _singleton;
  }

  TopologicalMapAdapter::TopologicalMapAdapter(std::istream& in){

    if(_singleton != NULL)
      delete _singleton;

    _singleton = this;

    // Temporary fix due to mismatch between 5cm map and 2.5cm map
    _map = topological_map::TopologicalMapPtr(new topological_map::TopologicalMap(in, 1.0, 1e9, 1e9));

    debugMsg("map:initialization", toPPM());
  }

  std::string TopologicalMapAdapter::toPPM(){
    std::ofstream of("topological_map.ppm");
    _map->writePpm(of);

    return "Output map in local directory: topological_map.ppm";
  }

  TopologicalMapAdapter::TopologicalMapAdapter(const topological_map::OccupancyGrid& grid, double resolution) {

    if(_singleton != NULL)
      delete _singleton;

    _singleton = this;

    _map = topological_map::topologicalMapFromGrid(grid, resolution, 2, 1, 1, 0, "local");

    toPostScriptFile();
  }

  TopologicalMapAdapter::~TopologicalMapAdapter(){
    _singleton = NULL;
  }

  unsigned int TopologicalMapAdapter::getNearestDoorway(double x, double y){
    debugMsg("trex:timing:getNearestDoorway", "BEFORE:"  << TREX::timeString());
    const topological_map::RegionIdSet& all_regions = _map->allRegions();
    double distance = PLUS_INFINITY;
    unsigned int nearest_doorway = 0;
    for(topological_map::RegionIdSet::const_iterator it = all_regions.begin(); it != all_regions.end(); ++it){
      bool is_doorway(false);
      topological_map::RegionId region_id = *it;
      isDoorway(region_id, is_doorway);
      if(is_doorway){
	door_msgs::Door door_msg;
	getDoorState(region_id, door_msg);
	double frame_center_x = (door_msg.frame_p1.x + door_msg.frame_p2.x) / 2.0;
	double frame_center_y = (door_msg.frame_p1.y + door_msg.frame_p2.y) / 2.0;
	double distance_to_frame = sqrt(pow(x - frame_center_x, 2) + pow(y - frame_center_y, 2));
	if(distance_to_frame < distance){
	  distance = distance_to_frame;
	  nearest_doorway = region_id;
	}
      }
    }

    debugMsg("trex:timing:getNearestDoorway", "AFTER:"  << TREX::timeString());

    debugMsg("map:getNearestDoorway", 
	     "Found doorway " << nearest_doorway << " within " << distance << " meters from <" << x << ", " << y << ">");

    return nearest_doorway;
  }

  unsigned int TopologicalMapAdapter::getRegion(double x, double y){
    unsigned int result = 0;
    try{
      result = _map->containingRegion(topological_map::Point2D(x, y));
    }
    catch(...){}
    return result;
  }

  topological_map::RegionPtr TopologicalMapAdapter::getRegionCells(unsigned int region_id){
    return _map->regionCells(region_id);
  }

  unsigned int TopologicalMapAdapter::getConnector(double x, double y){
    unsigned int result = 0;
    try{
      result = _map->pointConnector(topological_map::Point2D(x, y));
    }
    catch(...){}
    return result;
  }

  bool TopologicalMapAdapter::getConnectorPosition(unsigned int connector_id, double& x, double& y){
    try{
      topological_map::Point2D p = _map->connectorPosition(connector_id);
      x = p.x;
      y = p.y;
      return true;
    }
    catch(...){}
    return false;
  }

  bool TopologicalMapAdapter::getRegionConnectors(unsigned int region_id, std::vector<unsigned int>& connectors){
    try{
      connectors = _map->adjacentConnectors(region_id);
      return true;
    }
    catch(...){}
    return false;
  }

  bool TopologicalMapAdapter::getConnectorRegions(unsigned int connector_id, unsigned int& region_a, unsigned int& region_b){
    try{
      topological_map::RegionPair pair = _map->adjacentRegions(connector_id);
      region_a = pair.first;
      region_b = pair.second;
      return true;
    }
    catch(...){}
    return false;
  }

  bool TopologicalMapAdapter::getDoorState(unsigned int doorway_id, door_msgs::Door& door_state){
    try{
      // Check if it is a valid doorway id
      bool is_doorway(false);
      if(!isDoorway(doorway_id, is_doorway) || !is_doorway)
	return false;

      // Fill the door message and return
      door_state =_map->regionDoor(doorway_id);

      // Hack to work around bug
      /*
      door_state.frame_p1.x = 13.8;
      door_state.frame_p1.y = 21.4;
      door_state.frame_p2.x = 13.8;
      door_state.frame_p2.y = 22.3;
      door_state.door_p1.x = 13.8;
      door_state.door_p1.y = 21.4;
      door_state.door_p2.x = 13.8;
      door_state.door_p2.y = 22.3;
      */
      return true;
    }
    catch(...){}
    return false;
  }


  bool TopologicalMapAdapter::isDoorway(unsigned int region_id, bool& result){
    try{
      int region_type = _map->regionType(region_id);
      result = (region_type ==  topological_map::DOORWAY);
      return true;
    }
    catch(...){}
    result = false;
    return false;
  }

  bool TopologicalMapAdapter::isObstacle(double x, double y) {
    return _map->isObstacle(topological_map::Point2D(x, y));
  }


  double TopologicalMapAdapter::cost(double from_x, double from_y, double to_x, double to_y){

    // Obtain the distance point to point
    return sqrt(pow(from_x - to_x, 2) + pow(from_y - to_y, 2));

    /*
    std::pair<bool, double> result = _map->distanceBetween(topological_map::Point2D(from_x, from_y), topological_map::Point2D(to_x, to_y));

    // If there is no path then return infinint
    if(!result.first)
      return PLUS_INFINITY;


    // Otherwise we return the euclidean distance between source and target
    return result.second;
    */
  }

  double TopologicalMapAdapter::cost(double from_x, double from_y, unsigned int connector_id){
    double x(0), y(0);

    // If there is no connector for this id then we have an infinite cost
    if(!getConnectorPosition(connector_id, x, y))
      return PLUS_INFINITY;

    return cost(from_x, from_y, x, y);
  }

  void TopologicalMapAdapter::getConnectorCosts(double x0, double y0, double x1, double y1, std::vector< std::pair<topological_map::ConnectorId, double> >& results){
    const topological_map::Point2D source_point(x0, y0);
    const topological_map::Point2D target_point(x1, y1);
    try{
      results = _map->connectorCosts(source_point, target_point);
    }
    catch(std::runtime_error e){
      ROS_ERROR(e.what()); 
    }
    catch(...){
      debugMsg("map:getConnectorCosts", "failed for <" << x0 << ", " << y0 << "> => <" << x1 << ", " << y1 << ">"); 
    }
  }

  void TopologicalMapAdapter::getLocalConnectionsForGoal(std::list<ConnectionCostPair>& results, double x0, double y0, double x1, double y1){
    unsigned int this_region =  TopologicalMapAdapter::instance()->getRegion(x0, y0);
    condDebugMsg(this_region == 0, "map", "No region for <" << x0 << ", " << y0 <<">");
    unsigned int final_region =  TopologicalMapAdapter::instance()->getRegion(x1, y1);
    condDebugMsg(final_region == 0, "map", "No region for <" << x1 << ", " << y1 <<">");

    // If the source point and final point can be connected then we consider an option of going
    // directly to the point rather than thru a connector. To figure this out we should look the region at
    // the source connector and see if it is a connector for the final region
    if(this_region == final_region)
      results.push_back(ConnectionCostPair(0, cost(x0, y0, x1, y1)));

    const topological_map::Point2D source_point(x0, y0);
    const topological_map::Point2D target_point(x1, y1);
    std::vector< std::pair<topological_map::ConnectorId, double> > connector_cost_pairs = _map->connectorCosts(source_point, target_point);
    for(std::vector< std::pair<topological_map::ConnectorId, double> >::const_iterator it = connector_cost_pairs.begin();
	it != connector_cost_pairs.end(); ++it){

      results.push_back(ConnectionCostPair(it->first, it->second));
    }

    /*
    // Now iterate over the connectors in this region and compute the heuristic cost estimate. We exclude the source connector
    // since we have just arrived here
    std::vector<unsigned int> final_region_connectors;
    getRegionConnectors(final_region, final_region_connectors);
    unsigned int source_connector = getConnector(x0, y0);
    std::vector<unsigned int> connectors;
    getRegionConnectors(this_region, connectors);
    for(std::vector<unsigned int>::const_iterator it = connectors.begin(); it != connectors.end(); ++it){
      unsigned int connector_id = *it;
      if(connector_id != source_connector){
	results.push_back(ConnectionCostPair(connector_id, cost(x0, y0, connector_id) + cost(x1, y1, connector_id)));
      }
    }
    */
  }

  void TopologicalMapAdapter::toPostScriptFile(){
    std::ofstream of("topological_map.ps");

    // Print header
    of << "%%!PS\n";
    of << "%%%%Creator: Conor McGann (Willow Garage)\n";
    of << "%%%%EndComments\n";

    of << "2 setlinewidth\n";
    of << "newpath\n";

    std::vector<unsigned int> regions;
    std::vector<unsigned int> connectors;
    unsigned int region_id = 1;
    while(getRegionConnectors(region_id, connectors)){

      // Set color based on region type
      bool is_doorway(true);
      isDoorway(region_id, is_doorway);
      if(is_doorway){
	of << "1\t0\t0\tsetrgbcolor\n";
      }
      else
	of << "0\t1\t0\tsetrgbcolor\n";

      for(unsigned int i = 0; i < connectors.size(); i++){
	double x_i, y_i;
	getConnectorPosition(connectors[i], x_i, y_i);
	for(unsigned int j = 0; j < connectors.size(); j++){
	  double x_j, y_j;
	  getConnectorPosition(connectors[j], x_j, y_j);
	  if(i != j){
	    of << x_i * 10 << "\t" << y_i * 10 << "\tmoveto\n";
	    of << x_j * 10 << "\t" << y_j * 10<< "\tlineto\n";
	  }
	}
      }

      of << "closepath stroke\n";

      region_id++;
    }

    // Footer
    of << "showpage\n%%%%EOF\n";
    of.close();
  }

  void TopologicalMapAdapter::observeDoorBlocked(unsigned int door_id){
    _map->observeDoorTraversal(door_id, false, ros::Time::now());
  }

  unsigned int TopologicalMapAdapter::getNearestOutlet(double x, double y){
    topological_map::Point2D p(x, y);
    return _map->nearestOutlet(p);
  }

  void TopologicalMapAdapter::getOutletState(unsigned int outlet_id, robot_msgs::Pose& outlet_pose){
    topological_map::OutletInfo outlet_info = _map->outletInfo(outlet_id);
    outlet_pose.position.x = outlet_info.x;
    outlet_pose.position.y = outlet_info.y;
    outlet_pose.position.z = outlet_info.z;
    outlet_pose.orientation.x = outlet_info.qx;
    outlet_pose.orientation.y = outlet_info.qy;
    outlet_pose.orientation.z = outlet_info.qz;
    outlet_pose.orientation.w = outlet_info.qw;
  }

  /**
   * Impementation uses a distance of 1 meter and an error bound of 1 meter. Assume that the outlet pose can subsequently
   * be used.
   */
  void TopologicalMapAdapter::getOutletApproachPose(unsigned int outlet_id, robot_msgs::Pose& approach_pose){
    topological_map::OutletInfo outlet_info = _map->outletInfo(outlet_id);
    topological_map::Point2D p = _map->outletApproachPosition (outlet_id, 1.0, 0.3);
    approach_pose.position.x = p.x;
    approach_pose.position.y = p.y;
    approach_pose.position.z = 0;

    double dx = outlet_info.x - approach_pose.position.x;
    double dy = outlet_info.y - approach_pose.position.y;
    double heading = atan2(dy, dx);
    tf::QuaternionTFToMsg (tf::Quaternion(heading, 0.0, 0.0), approach_pose.orientation);
  }

  /**
   * Impementation uses a distance of 1 meter
   */
  void TopologicalMapAdapter::getDoorApproachPose(unsigned int connector_id, robot_msgs::Pose& approach_pose){
    topological_map::Point2D p = _map->doorApproachPosition (connector_id, 1.0);
    approach_pose.position.x = p.x;
    approach_pose.position.y = p.y;
    approach_pose.position.z = 0;


    // Orientation to face the connector
    robot_msgs::Vector3 orientation;
    double x, y;
    TopologicalMapAdapter::instance()->getConnectorPosition(connector_id, x, y);
    orientation.x = x - p.x;
    orientation.y = y - p.y;
    orientation.z = 0;
    tf::QuaternionTFToMsg(tf::Quaternion(atan2(orientation.y,orientation.x), 0.0, 0.0), approach_pose.orientation);
  }

  void TopologicalMapAdapter::observeOutletBlocked(unsigned int outlet_id){
    _map->observeOutletBlocked(outlet_id);
  }

  bool TopologicalMapAdapter::isDoorway(double x1, double y1, double x2, double y2){
    bool is_doorway(false);
    double distance = sqrt(pow(x1-x2, 2) + pow(y1-y2, 2));
    // If points are pretty close, check if it is a doorway. If they are not close we know it is not a doorway
    if(distance < 2){
      double p_x = x1 + 0.75 * (x2-x1);
      double p_y = y1 + 0.75 * (y2-y1);
      unsigned int region_id = getRegion(p_x, p_y);
      isDoorway(region_id, is_doorway);
    }

    return is_doorway;
  }

  /**
   * Will get the next connection.
   */
  unsigned int TopologicalMapAdapter::getNextConnector(double x1, double y1, double x2, double y2, 
						       double& lowest_cost, double& next_x, double& next_y){
    // Query neighboring connectors
    unsigned int next_connector = getBestConnector(x1, y1, x2, y2, lowest_cost, next_x, next_y);

    // If there is a new connector, then we have to consider the case where the connector is at the start of a doorway. In the event that it is, then based on the
    // distance between them, we must decide if the doorway is to be crossed or not.
    if(next_connector > 0 && isDoorwayConnector(next_connector)){
      robot_msgs::Pose pose;
      getDoorApproachPose(next_connector, pose);
      if(fabs(pose.position.x - x1) < 0.10 && fabs(pose.position.y - y1) < 0.10){
	next_connector = getOtherDoorConnector(next_connector, x2, y2);
	TopologicalMapAdapter::instance()->getConnectorPosition(next_connector, next_x, next_y);
	debugMsg("map:getNextConnector", "Preferring connector " << next_connector << " at <" << next_x << ", " << next_y << "> with cost " << lowest_cost);
      }
    }

    return next_connector;
  }

  bool TopologicalMapAdapter::isDoorwayConnector(unsigned int connector_id){
    unsigned int region_a(0), region_b(0);
    bool doorway_a(false), doorway_b(false);
    getConnectorRegions(connector_id, region_a, region_b);
    isDoorway(region_a, doorway_a);
    isDoorway(region_b, doorway_b);
    return doorway_a || doorway_b;
  }

  unsigned int TopologicalMapAdapter::getOtherDoorConnector(unsigned int connector_id, double target_x, double target_y){
    unsigned int region_a(0), region_b(0), doorway_region(0);
    bool doorway_a(false), doorway_b(false);
    getConnectorRegions(connector_id, region_a, region_b);
    isDoorway(region_a, doorway_a);
    isDoorway(region_b, doorway_b);

    if(doorway_a)
      doorway_region = region_a;
    else if(doorway_b)
      doorway_region = region_b;
    else{
      debugMsg("trex:error", "Must be at least one doorway region for connector " << connector_id);
      ROS_ASSERT(doorway_a || doorway_b);
    }

    // We want to get the next connector across the doorway. The general case must admit a set of connectors but exactly 2 of them 
    std::vector<unsigned int> connectors;
    getRegionConnectors(doorway_region, connectors);
    ROS_ASSERT(connectors.size() <= 3);

    // Obtain the center of the doorway
    door_msgs::Door door_state;
    getDoorState(doorway_region, door_state);
    double c_x = (door_state.frame_p1.x + door_state.frame_p2.x) / 2;
    double c_y = (door_state.frame_p1.y + door_state.frame_p2.y) / 2;
    debugMsg("map:getOtherDoorConnector", "Trying doorway region " << doorway_region << " with center point <" << c_x << ", " << c_y << ">");
    double nx, ny, cost(PLUS_INFINITY);
    return getBestConnector(c_x, c_y, target_x, target_y, cost, nx, ny);
  }

  unsigned int TopologicalMapAdapter::getBestConnector(double x1, double y1, double x2, double y2,
						    double& lowest_cost, double& next_x, double& next_y){
    // Query neighboring connectors
    unsigned int best_connector(0);
    std::vector< std::pair<topological_map::ConnectorId, double> > connector_cost_pairs;
    ros::Time before = ros::Time::now();
    getConnectorCosts(x1, y1, x2, y2, connector_cost_pairs);
    ros::Duration elapsed = ros::Time::now() - before;
    debugMsg("map:getBestConnector", "connection_cost_latency is " << elapsed.toSec());

    // Select if there is an improvement
    for(std::vector< std::pair<topological_map::ConnectorId, double> >::const_iterator it = connector_cost_pairs.begin(); it != connector_cost_pairs.end(); ++it){
      if(it->second < lowest_cost){
	// Now prune this candidate if it is the current point
	double candidate_x(0.0), candidate_y(0.0);
	TopologicalMapAdapter::instance()->getConnectorPosition(it->first, candidate_x, candidate_y);

	if(fabs(candidate_x - x1) < 0.10 && fabs(candidate_y - y1) < 0.10)
	  continue;

	best_connector = it->first;
	lowest_cost = it->second;
	next_x = candidate_x;
	next_y = candidate_y;
	debugMsg("map:getBestConnector", "Preferring connector " << best_connector << " at <" << next_x << ", " << next_y << "> with cost " << lowest_cost);
      }
    }

    return best_connector;
  }
}
