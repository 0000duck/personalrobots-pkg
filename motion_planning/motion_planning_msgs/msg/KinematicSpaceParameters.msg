# This message contains a set of parameters useful in setting up a
# kinematic space for planning

# The model (defined in pr2_defs/planning/planning.yaml)
# for which the motion planner should plan for
string model_id

# A string that identifies which distance metric the planner should use
string distance_metric

# Lower coordinate for a box defining the volume in the workspace the
# motion planner is active in.  If planning in 2D, only first 2 values
# (x, y) of the points are used.
geometry_msgs/PointStamped volumeMin


# Higher coordinate for the box described above
geometry_msgs/PointStamped volumeMax

# A list of allowed contacts
AcceptableContact[] contacts
