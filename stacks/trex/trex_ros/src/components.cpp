#include "trex_ros/components.h"
#include "GoalManager.hh"
#include "PlanDatabase.hh"
#include "Token.hh"
#include "TokenVariable.hh"
#include "TemporalAdvisor.hh"
#include "DbCore.hh"
#include "Constraints.hh"
#include "Timeline.hh"
#include "Agent.hh"
#include "Utilities.hh"
#include "trex_ros/calc_angle_diff_constraint.h"
#include "trex_ros/calc_distance_constraint.h"
#include "OrienteeringSolver.hh"
#include "Utilities.hh"
#include "LabelStr.hh"
#include <trex_ros/ros_reactor.h>
#include <tf/transform_listener.h>
#include <math.h>

namespace TREX{

  bool allSingletons(const std::vector<ConstrainedVariableId>& variables, const std::string& variables_to_check){
    bool found_non_singleton(false);
    for(std::vector<ConstrainedVariableId>::const_iterator it = variables.begin(); it != variables.end(); ++it){
      ConstrainedVariableId var = *it;
      if(!var->lastDomain().isSingleton()){
	std::string with_delimiter = ":" + var->getName().toString();
	if(variables_to_check.find(with_delimiter)  != std::string::npos){
	  found_non_singleton = true;
	  debugMsg("trex:debug:propagation", var->toLongString() << " is not a singleton.");
	}
      }
    }

    return !found_non_singleton;
  }

  tf::TransformListener& getTransformListener(){
    static tf::TransformListener tf(ros::Duration(10));
    return tf;
  }

  /**
   * @brief Handle cleanup on process termination signals.
   */
  void signalHandler(int signalNo){
    std::cout << "Handling signal..." << signalNo << std::endl;
    exit(0);
  }

  ParamEqConstraint::ParamEqConstraint(const LabelStr& name,
				       const LabelStr& propagatorName,
				       const ConstraintEngineId& constraintEngine,
				       const std::vector<ConstrainedVariableId>& variables,
				       const char* param_names)
    : Constraint(name, propagatorName, constraintEngine, makeNewScope(param_names, variables)){}
  
  
  std::vector<ConstrainedVariableId> TREX::ParamEqConstraint::makeNewScope(const char* param_names, const std::vector<ConstrainedVariableId>& variables ){
    static const char* DELIMITER(":");
    
    // If already mapped, then return without modification. This is the case when merging
    if(variables.size() > 2)
      return variables;
    
    // Otherwise swap for parameters of both tokens
    std::vector<ConstrainedVariableId> new_scope;
    LabelStr param_names_lbl(param_names);
    unsigned int param_count = param_names_lbl.countElements(DELIMITER);
    for(unsigned int i = 0; i < param_count; i++){
      LabelStr param_name = param_names_lbl.getElement(i, DELIMITER);
      ConstrainedVariableId var_a = getVariableByName(variables[0], param_name);
      ConstrainedVariableId var_b = getVariableByName(variables[1], param_name);
      checkError(var_a.isValid(), "In param_eq constrint - no variable for " << param_name.toString() << " for " << parentOf(variables[0])->toLongString() << " in /n" << toString());
      checkError(var_b.isValid(), "In param_eq constrint - no variable for " << param_name.toString() << " for " << parentOf(variables[1])->toLongString()<< " in /n" << toString());
      
      // Insert the pair
      new_scope.push_back(var_a);
      new_scope.push_back(var_b);
    }
    
    return new_scope;
  }
  
  EntityId TREX::ParamEqConstraint::parentOf(const ConstrainedVariableId& var){
    // If it has a parent, that parent should be a token
    if(var->parent().isId()){
      return TREX::getParentToken(var);
    }
    
    checkError(var->lastDomain().isSingleton(), var->toString() << " should be bound.");
    return var->lastDomain().getSingletonValue();
  }
  
  ConstrainedVariableId TREX::ParamEqConstraint::getVariableByName(const ConstrainedVariableId& var, const LabelStr& param_name){
    // If it has a parent, that parent should be a token
    if(var->parent().isId()){
      const TokenId& token = TREX::getParentToken(var);
      return token->getVariable(param_name);
    }
    
    checkError(var->lastDomain().isSingleton(), var->toString() << " should be bound.");
    ObjectId object = var->lastDomain().getSingletonValue();
    std::string object_name = object->getName().toString() + "." + param_name.toString();
    return object->getVariable(object_name.c_str());
  }
  
  void TREX::ParamEqConstraint::handleExecute(){
    debugMsg("trex:debug:propagation:param_eq",  "BEFORE: " << toString());
    
    unsigned int param_count = getScope().size() / 2;
    for(unsigned int i = 0; i < param_count; i++){
      unsigned int index = i * 2;
      AbstractDomain& dom_a = getCurrentDomain(getScope()[index]);
      AbstractDomain& dom_b = getCurrentDomain(getScope()[index+1]);
      
      if(dom_a.intersect(dom_b) && dom_a.isEmpty())
	return;
    }
    
    debugMsg("trex:debug:propagation:param_eq",  "AFTER: " << toString());
  }
  


  class CalcOrConstraint: public Constraint {
  public:
    CalcOrConstraint(const LabelStr& name,
			const LabelStr& propagatorName,
			const ConstraintEngineId& constraintEngine,
			const std::vector<ConstrainedVariableId>& variables)
      : Constraint(name, propagatorName, constraintEngine, variables)
    {}

  protected:

    void handleExecute(){
      debugMsg("trex:debug:propagation:calc_or",  "BEFORE: " << toString());
      AbstractDomain& target = static_cast<BoolDomain&>(getCurrentDomain(getScope()[0]));
      unsigned int true_count = 0;
      unsigned int bound_count = 0;
      for (unsigned int i = 1; i < getScope().size(); i++){
	ConstrainedVariableId var = getScope()[i];
	const BoolDomain& dom = static_cast<const BoolDomain&>(var->lastDomain());
	if(dom.isSingleton()){
	  bound_count++;
	  if(dom.getSingletonValue() == true)
	    true_count++;
	}
      }

      if(true_count > 0){
	target.set(1);
      }
      else if(bound_count == (getScope().size() - 1)){
	target.set(0);
      }

      debugMsg("trex:debug:propagation:calc_or",  "AFTER: " << toString());
    }
  };


 
  class PointMsgEqConstraint: public ParamEqConstraint {
  public:
    PointMsgEqConstraint(const LabelStr& name,
			const LabelStr& propagatorName,
			const ConstraintEngineId& constraintEngine,
			const std::vector<ConstrainedVariableId>& variables)
      : ParamEqConstraint(name, propagatorName, constraintEngine, variables,
			  "frame_id:time_stamp:x:y:z")
    {}
  };

  class PoseMsgEqConstraint: public ParamEqConstraint {
  public:
    PoseMsgEqConstraint(const LabelStr& name,
			const LabelStr& propagatorName,
			const ConstraintEngineId& constraintEngine,
			const std::vector<ConstrainedVariableId>& variables)
      : ParamEqConstraint(name, propagatorName, constraintEngine, variables,
			  "frame_id:time_stamp:x:y:z:qx:qy:qz:qw")
    {}
  };


  class TFGetRobotPoseConstraint: public Constraint {
  public:
    TFGetRobotPoseConstraint(const LabelStr& name,
			const LabelStr& propagatorName,
			const ConstraintEngineId& constraintEngine,
			const std::vector<ConstrainedVariableId>& variables)
      : Constraint(name, propagatorName, constraintEngine, variables),
	_x(static_cast<IntervalDomain&>(getCurrentDomain(variables[0]))),
	_y(static_cast<IntervalDomain&>(getCurrentDomain(variables[1]))),
	_z(static_cast<IntervalDomain&>(getCurrentDomain(variables[2]))),
	_qx(static_cast<IntervalDomain&>(getCurrentDomain(variables[3]))),
	_qy(static_cast<IntervalDomain&>(getCurrentDomain(variables[4]))),
	_qz(static_cast<IntervalDomain&>(getCurrentDomain(variables[5]))),
	_qw(static_cast<IntervalDomain&>(getCurrentDomain(variables[6]))),
	_frame_id(static_cast<StringDomain&>(getCurrentDomain(variables[7]))),
	_frame(LabelStr(_frame_id.getSingletonValue()).toString()),
	_propagated(false){
      checkError(_frame_id.isSingleton(), "The frame has not been specified for tf to get robot pose. See model for error." << _frame_id.toString());
      condDebugMsg(!_frame_id.isSingleton(), "trex:error:tf_get_robot_pose",  "Frame has not been specified" << variables[7]->toLongString());
    }

  private:
    virtual void handleExecute(){
      debugMsg("trex:debug:propagation:tf_get_robot_pose",  "BEFORE: " << TREX::timeString() << toString());

      // Obtain pose if not already called
      if(!_propagated){
	getPose(_pose);
      }

      if(_propagated){
	debugMsg("trex:debug:propagation:tf_get_robot_pose", "Compute pose <" <<
		 _pose.getOrigin().x() << ", " <<
		 _pose.getOrigin().y() << ", " <<
		 _pose.getOrigin().z() << ", " <<
		 _pose.getRotation().x() << ", " <<
		 _pose.getRotation().y() << ", " <<
		 _pose.getRotation().z() << ", " <<
		 _pose.getRotation().w() << ", >");

	getCurrentDomain(getScope()[0]).set(_pose.getOrigin().x());
	getCurrentDomain(getScope()[1]).set(_pose.getOrigin().y());
	getCurrentDomain(getScope()[2]).set(_pose.getOrigin().z());
	getCurrentDomain(getScope()[3]).set(_pose.getRotation().x());
	getCurrentDomain(getScope()[4]).set(_pose.getRotation().y());
	getCurrentDomain(getScope()[5]).set(_pose.getRotation().z());
	getCurrentDomain(getScope()[6]).set(_pose.getRotation().w());
      }
      debugMsg("trex:debug:propagation:tf_get_robot_pose",  "AFTER: " << TREX::timeString() << toString());
    }

    virtual void setSource(const ConstraintId& sourceConstraint) {
      const TFGetRobotPoseConstraint* source_ptr = (TFGetRobotPoseConstraint*) sourceConstraint;
      checkError(source_ptr != NULL, "Could not cast " << sourceConstraint->toLongString());
      _pose = source_ptr->_pose;
      _propagated = source_ptr->_propagated;
    }

    void getPose(tf::Stamped<tf::Pose>& pose){
      tf::Stamped<tf::Pose> robot_pose;
      robot_pose.setIdentity();
      robot_pose.frame_id_ = "base_footprint";

      try{
	debugMsg("trex:debug:timing:get_pose",  "A: " << TREX::timeString());
	tf::TransformListener& tfl = getTransformListener();
	debugMsg("trex:debug:timing:get_pose",  "B: " << TREX::timeString());
	robot_pose.stamp_ = ros::Time();
	tfl.canTransform(_frame, "base_footprint", robot_pose.stamp_, ros::Duration(3.0));
	tfl.transformPose(_frame, robot_pose, pose);
	_propagated = true;
	debugMsg("trex:debug:timing:get_pose",  "C: " << TREX::timeString());
      }
      catch(tf::LookupException& ex) {
	ROS_ERROR("No Transform available Error: %s\n", ex.what());
	return;
      }
      catch(tf::ConnectivityException& ex) {
	ROS_ERROR("Connectivity Error: %s\n", ex.what());
	return;
      }
      catch(tf::ExtrapolationException& ex) {
	ROS_ERROR("Extrapolation Error: %s\n", ex.what());
      }
    }

    IntervalDomain& _x, _y, _z, _qx, _qy, _qz, _qw; // Pose is the output
    StringDomain& _frame_id;
    const std::string _frame;
    bool _propagated;
    tf::Stamped<tf::Pose> _pose;
  };

  class AllBoundsSetConstraint: public Constraint{
  public:
    AllBoundsSetConstraint(const LabelStr& name,
			   const LabelStr& propagatorName,
			   const ConstraintEngineId& constraintEngine,
			   const std::vector<ConstrainedVariableId>& variables);

  private:
    void handleExecute();
    std::string toString() const;

    const TokenId _token;
  };

  class FloorFunction: public Constraint{
  public:
    FloorFunction(const LabelStr& name,
		  const LabelStr& propagatorName,
		  const ConstraintEngineId& constraintEngine,
		  const std::vector<ConstrainedVariableId>& variables);

  private:
    void handleExecute();
    AbstractDomain& m_target;
    const AbstractDomain& m_source;
  };

  class PlugsGetOffsetPose: public Constraint{
  public:
    PlugsGetOffsetPose(const LabelStr& name, const LabelStr& propagatorName,
                       const ConstraintEngineId& constraintEngine,
                       const std::vector<ConstrainedVariableId>& vars);
  private:
    void handleExecute();
    const AbstractDomain &x, &y, &z, &qx, &qy, &qz, &qw, &dist;
    AbstractDomain &x2, &y2, &z2, &qx2, &qy2, &qz2, &qw2;
  };


  class NearestLocation: public Constraint{
  public:
    NearestLocation(const LabelStr& name,
		    const LabelStr& propagatorName,
		    const ConstraintEngineId& constraintEngine,
		    const std::vector<ConstrainedVariableId>& variables);

  private:
    void handleExecute();
    const AbstractDomain& m_x;
    const AbstractDomain& m_y;
    ObjectDomain& m_location;
  };

  class RandomSelection: public Constraint{
  public:
    RandomSelection(const LabelStr& name,
		    const LabelStr& propagatorName,
		    const ConstraintEngineId& constraintEngine,
		    const std::vector<ConstrainedVariableId>& variables);

  private:
    void handleExecute();
    AbstractDomain& m_target;
  };

  std::vector<SchemaFunction> _g_ros_schemas;

  class ROSSchema: public Assembly::Schema {
  public:
    ROSSchema(bool playback):m_playback(playback){}

    void registerComponents(const Assembly& assembly){
      Assembly::Schema::registerComponents(assembly);
      ConstraintEngineId constraintEngine = ((ConstraintEngine*) assembly.getComponent("ConstraintEngine"))->getId();

      // Constraint Registration
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::CalcOrConstraint, "calc_or", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::SubsetOfConstraint, "in", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::CalcDistanceConstraint, "calcDistance", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), FloorFunction, "calcFloor", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), PlugsGetOffsetPose, "plugs_get_offset_pose", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::NearestLocation, "nearestReachableLocation", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::RandomSelection, "randomSelect", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), CalcAngleDiffConstraint, "calcAngleDiff", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::AllBoundsSetConstraint, "all_bounds_set", "Default");

      // Eq Constr
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::PointMsgEqConstraint, "eq_point_msg", "Default");
      REGISTER_CONSTRAINT(constraintEngine->getCESchema(), TREX::PoseMsgEqConstraint, "eq_pose_msg", "Default");

      //Create User-defined stuff.
      for (unsigned int i = 0; i < _g_ros_schemas.size(); i++) {
	_g_ros_schemas[i](m_playback, assembly);
      }
    }

  private:
    const bool m_playback;
  };


  void initROSExecutive(bool playback){
    initTREX();
    new TeleoReactor::ConcreteFactory<trex_ros::ROSReactor>("ROSReactor");
    new ROSSchema(playback);
  }

  //*******************************************************************************************
  AllBoundsSetConstraint::AllBoundsSetConstraint(const LabelStr& name,
					 const LabelStr& propagatorName,
					 const ConstraintEngineId& constraintEngine,
					 const std::vector<ConstrainedVariableId>& variables)
    :Constraint(name, propagatorName, constraintEngine, variables),
     _token(TREX::getParentToken(variables[0])){
    checkError(variables.size() == 1, "Invalid signature for " << name.toString() << ". Check the constraint signature in the model.");
  }

  /**
   * Will fire if any bounds are not singletons
   */
  void AllBoundsSetConstraint::handleExecute(){
    debugMsg("trex:propagation:all_bounds_set",  "BEFORE: " << toString());

    // Iterate over all parameters of the source token. Any with a name match will be intersected
    for(std::vector<ConstrainedVariableId>::const_iterator it = _token->parameters().begin(); it != _token->parameters().end(); ++it){
      ConstrainedVariableId v = *it;
      if(!v->lastDomain().isSingleton()){
	debugMsg("trex:warning:propagation:all_bounds_set",  "Required parameter is unbound:" << v->toLongString());
	getCurrentDomain(v).empty();
	return;
      }
    }

    debugMsg("trex:propagation:all_bounds_set",  "AFTER: " << toString());
  }

  std::string AllBoundsSetConstraint::toString() const {
    std::stringstream sstr;

    sstr << Entity::toString() << std::endl;

    unsigned int i = 0;
    for(std::vector<ConstrainedVariableId>::const_iterator it = _token->parameters().begin(); it != _token->parameters().end(); ++it){
      ConstrainedVariableId v = *it;
      sstr << " ARG[" << i++ << "]:" << v->toLongString() << std::endl;
    }

    return sstr.str();
  }
  //*******************************************************************************************
  FloorFunction::FloorFunction(const LabelStr& name,
		     const LabelStr& propagatorName,
		     const ConstraintEngineId& constraintEngine,
		     const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables),
      m_target(getCurrentDomain(m_variables[0])),
      m_source(getCurrentDomain(m_variables[1])){
    checkError(m_variables.size() == 2, "Exactly 2 parameters required. ");
  }

  void FloorFunction::handleExecute(){
    if(m_source.isSingleton()){
      int f = (int) m_source.getSingletonValue();
      m_target.set(f);
    }
  }


  PlugsGetOffsetPose::PlugsGetOffsetPose(const LabelStr& name,
                                         const LabelStr& propagatorName,
                                         const ConstraintEngineId& constraintEngine,
                                         const std::vector<ConstrainedVariableId>& vars)
    : Constraint(name, propagatorName, constraintEngine, vars),
      x(getCurrentDomain(vars[0])),
      y(getCurrentDomain(vars[1])),
      z(getCurrentDomain(vars[2])),
      qx(getCurrentDomain(vars[3])),
      qy(getCurrentDomain(vars[4])),
      qz(getCurrentDomain(vars[5])),
      qw(getCurrentDomain(vars[6])),
      dist(getCurrentDomain(vars[7])),
      x2(getCurrentDomain(vars[8])),
      y2(getCurrentDomain(vars[9])),
      z2(getCurrentDomain(vars[10])),
      qx2(getCurrentDomain(vars[11])),
      qy2(getCurrentDomain(vars[12])),
      qz2(getCurrentDomain(vars[13])),
      qw2(getCurrentDomain(vars[14]))
  {
    checkError(vars.size()==15, "Invalid arg count: " << vars.size());
  }

  void PlugsGetOffsetPose::handleExecute()
  {
    using robot_msgs::Pose;
    debugMsg("trex:debug:propagation:world_model:plugs_get_offset_pose", "BEFORE: " << toString());
    if (x.isSingleton() && y.isSingleton() && z.isSingleton() && qx.isSingleton() &&
        qy.isSingleton() && qz.isSingleton() && qw.isSingleton() && dist.isSingleton()) {
      Pose outletPose;
      outletPose.position.x=x.getSingletonValue();
      outletPose.position.y=y.getSingletonValue();
      outletPose.position.z=z.getSingletonValue();
      outletPose.orientation.x=qx.getSingletonValue();
      outletPose.orientation.y=qy.getSingletonValue();
      outletPose.orientation.z=qz.getSingletonValue();
      outletPose.orientation.w=qw.getSingletonValue();


      // Determines the desired base position
      tf::Pose outlet_pose_tf;
      tf::PoseMsgToTF(outletPose, outlet_pose_tf);
      tf::Pose desi_offset(tf::Quaternion(0,0,0), tf::Vector3(-dist.getSingletonValue(), 0.2, 0.0));
      tf::Pose target = outlet_pose_tf * desi_offset;
      Pose targetPose;
      tf::PoseTFToMsg(target, targetPose);


      x2.set(targetPose.position.x);
      y2.set(targetPose.position.y);
      z2.set(targetPose.position.z);

      qx2.set(targetPose.orientation.x);
      qy2.set(targetPose.orientation.y);
      qz2.set(targetPose.orientation.z);
      qw2.set(targetPose.orientation.w);
    }
    debugMsg("trex:debug:propagation:world_model:plugs_get_offset_pose", "AFTER: " << toString());
  }





  NearestLocation::NearestLocation(const LabelStr& name,
				   const LabelStr& propagatorName,
				   const ConstraintEngineId& constraintEngine,
				   const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables),
      m_x(getCurrentDomain(variables[0])),
      m_y(getCurrentDomain(variables[1])),
      m_location(static_cast<ObjectDomain&>(getCurrentDomain(variables[2]))){
    checkError(variables.size() == 3, "Invalid Arg Count: " << variables.size());
  }

  /**
   * Should wait till inputs are bound, then iterate over the locations and select the nearest one.
   */
  void NearestLocation::handleExecute() {
    unsigned int iterations = 0;
    double minDistance = PLUS_INFINITY;

    debugMsg("trex:debug:propagation:world_model:nearest_location",  "BEFORE: " << toString());

    if(m_x.isSingleton() && m_y.isSingleton()){
      std::list<ObjectId> locations = m_location.makeObjectList();
      ObjectId nearestLocation = locations.front();
      for(std::list<ObjectId>::const_iterator it = locations.begin(); it != locations.end(); ++it){
	iterations++;
	ObjectId location = *it;
	ConstrainedVariableId x = location->getVariables()[0];
	ConstrainedVariableId y = location->getVariables()[1];
	checkError(x.isId(), "No variable for x");
	checkError(y.isId(), "No variable for y");
	checkError(x->lastDomain().isSingleton(), "Object variable for x should be bound but is not. " << x->toString());
	checkError(y->lastDomain().isSingleton(), "Object variable for y should be bound but is not. " << y->toString());
	double dx = m_x.getSingletonValue() - x->lastDomain().getSingletonValue();
	double dy = m_y.getSingletonValue() - y->lastDomain().getSingletonValue();
	double distance = sqrt(pow(dx, 2) + pow(dy, 2));

	// Should we promote?
	if(distance < minDistance){
	  nearestLocation = location;
	  minDistance = distance;
	}
      }

      m_location.set(nearestLocation);
    }

    debugMsg("trex:debug:propagation:world_model:nearest_location",  "AFTER: " << toString()
	      <<  std::endl << std::endl << "After " << iterations << " iterations, found a location within " << minDistance << " meters.");
  }


  RandomSelection::RandomSelection(const LabelStr& name,
				   const LabelStr& propagatorName,
				   const ConstraintEngineId& constraintEngine,
				   const std::vector<ConstrainedVariableId>& variables)
    : Constraint(name, propagatorName, constraintEngine, variables),
      m_target(getCurrentDomain(variables[0])){

    static bool initialized(false);
    checkError(variables.size() == 1, "Invalid Arg Count: " << variables.size());

    // Initialize random seed
    if(!initialized){
      srand(0);
      initialized = true;
    }
  }

  /**
   * Randomly choose a value from the propagated domain
   */
  void RandomSelection::handleExecute() {

    debugMsg("trex:propagation:random_select",  "BEFORE: " << toString());

    if(m_target.isEnumerated()){
      std::list<double> values;
      m_target.getValues(values);
      int selectionId = rand() % values.size();
      std::list<double>::const_iterator it = values.begin();
      while(selectionId > 0 && it != values.end()){
	selectionId--;
	++it;
      }
      m_target.set(*it);
    }
    else if(m_target.isFinite()){
      unsigned int delta = (unsigned int) (m_target.getUpperBound() - m_target.getLowerBound());
      int selectionId = rand() % delta;
      m_target.set(selectionId + m_target.getLowerBound());
    }
    else {// Do something
      m_target.set(m_target.getLowerBound());
    }

    debugMsg("trex:propagation:random_select",  "AFTER: " << toString());
  }
}
