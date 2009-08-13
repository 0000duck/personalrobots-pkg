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

/** \author Benjamin Cohen, Maxim Likhachev */

#include <sbpl_arm_planner/headers.h>
#include <fstream>

#define PI_CONST 3.141592653
#define DEG2RAD(d) ((d)*(PI_CONST/180.0))
#define RAD2DEG(r) ((r)*(180.0/PI_CONST))
#define SMALL_NUM  0.00000001 // anything that avoids division overflow

// cost multiplier
#define COSTMULT 1000

// distance from goal
#define MAX_EUCL_DIJK_m .03

// discretization of joint angles
#define ANGLEDELTA 360.0

#define OUTPUT_OBSTACLES 0
#define VERBOSE 0
#define OUTPUT_DEGREES 0
#define DEBUG_VALID_COORD 0
#define DEBUG_JOINTSPACE_PLANNING 0
#define DEBUG_HEURISTIC 0
// #define DEBUG 0

//for ComputeForwardKinematics_DH - to rotate DH frames to PR2 frames
#define ELBOW_JOINT 2
#define WRIST_JOINT 4
#define ENDEFF 6
#define GOAL_JOINT 9 //temp 6.22.09
#define RADIUS 7;

// number of successors to each cell (for dyjkstra's heuristic)
#define DIRECTIONS 26

int dx[DIRECTIONS] = { 1,  1,  1,  0,  0,  0, -1, -1, -1,    1,  1,  1,  0,  0, -1, -1, -1,    1,  1,  1,  0,  0,  0, -1, -1, -1};
int dy[DIRECTIONS] = { 1,  0, -1,  1,  0, -1, -1,  0,  1,    1,  0, -1,  1, -1, -1,  0,  1,    1,  0, -1,  1,  0, -1, -1,  0,  1};
int dz[DIRECTIONS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1,    0,  0,  0,  0,  0,  0,  0,  0,    1,  1,  1,  1,  1,  1,  1,  1,  1};

// #if DEBUG
    FILE* fSucc = fopen("debugsuccs.txt", "w");
// #endif

#if DEBUG_HEURISTIC
    FILE* fDeb = fopen("debug_environment.txt", "w");
    FILE* fHeur = fopen("debug_heur.txt", "w");
    FILE* fHeurExpansions = fopen("debug_heur_expansions.txt", "w");
    FILE* fHuh = fopen("time_to_goal_range.txt","a");
#endif

//Statistics
bool near_goal = false;
clock_t starttime;
double time_to_goal_region;


/**------------------------------------------------------------------------*/
                        /** State Access Functions */
/**------------------------------------------------------------------------*/
static unsigned int inthash(unsigned int key)
{
    key += (key << 12); 
    key ^= (key >> 22);
    key += (key << 4);
    key ^= (key >> 9);
    key += (key << 10);
    key ^= (key >> 2);
    key += (key << 7);
    key ^= (key >> 12);
    return key;
}

unsigned int EnvironmentROBARM3D::GETHASHBIN(short unsigned int* coord, int numofcoord)
{

    int val = 0;

    for(int i = 0; i < numofcoord; i++)
    {
        val += inthash(coord[i]) << i;
    }

    return inthash(val) & (EnvROBARM.HashTableSize-1);
}

void EnvironmentROBARM3D::PrintHashTableHist()
{
    int s0=0, s1=0, s50=0, s100=0, s200=0, s300=0, slarge=0;

    for(int  j = 0; j < EnvROBARM.HashTableSize; j++)
    {
        if((int)EnvROBARM.Coord2StateIDHashTable[j].size() == 0)
            s0++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 50)
            s1++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 100)
            s50++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 200)
            s100++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 300)
            s200++;
        else if((int)EnvROBARM.Coord2StateIDHashTable[j].size() < 400)
            s300++;
        else
            slarge++;
    }
    printf("hash table histogram: 0:%d, <50:%d, <100:%d, <200:%d, <300:%d, <400:%d >400:%d\n",
           s0,s1, s50, s100, s200,s300,slarge);
}

EnvROBARMHashEntry_t* EnvironmentROBARM3D::GetHashEntry(short unsigned int* coord, int numofcoord, short unsigned int action, bool bIsGoal)
{
//     num_GetHashEntry++;
//     clock_t currenttime = clock();

    //if it is goal
    if(bIsGoal)
    {
        return EnvROBARM.goalHashEntry;
    }

    int binid = GETHASHBIN(coord, numofcoord);

#if DEBUG
    if ((int)EnvROBARM.Coord2StateIDHashTable[binid].size() > 500)
    {
        printf("WARNING: Hash table has a bin %d (coord0=%d) of size %d\n", 
               binid, coord[0], EnvROBARM.Coord2StateIDHashTable[binid].size());
        PrintHashTableHist();
    }
#endif

    //iterate over the states in the bin and select the perfect match
    for(int ind = 0; ind < (int)EnvROBARM.Coord2StateIDHashTable[binid].size(); ind++)
    {
        int j = 0;

        if(EnvROBARMCfg.use_smooth_actions)
        {
            //first check if it is the correct action
            if (EnvROBARM.Coord2StateIDHashTable[binid][ind]->action != action)
                continue;
        }

        for(j = 0; j < numofcoord; j++)
        {
            if(EnvROBARM.Coord2StateIDHashTable[binid][ind]->coord[j] != coord[j]) 
            {
                break;
            }
        }

        if (j == numofcoord)
        {
//             time_gethash += clock()-currenttime;
            return EnvROBARM.Coord2StateIDHashTable[binid][ind];
        }
    }

//     time_gethash += clock()-currenttime;

    return NULL;
}

EnvROBARMHashEntry_t* EnvironmentROBARM3D::CreateNewHashEntry(short unsigned int* coord, int numofcoord, short unsigned int endeff[3], short unsigned int action, double orientation[3][3])
{
    int i;
    //clock_t currenttime = clock();
    EnvROBARMHashEntry_t* HashEntry = new EnvROBARMHashEntry_t;

    memcpy(HashEntry->coord, coord, numofcoord*sizeof(short unsigned int));
    memcpy(HashEntry->endeff, endeff, 3*sizeof(short unsigned int));

    if(orientation != NULL)
        memcpy(HashEntry->orientation, orientation, 9*sizeof(double));

    HashEntry->action = action;

    // assign a stateID to HashEntry to be used 
    HashEntry->stateID = EnvROBARM.StateID2CoordTable.size();

    //insert into the tables
    EnvROBARM.StateID2CoordTable.push_back(HashEntry);

    //get the hash table bin
    i = GETHASHBIN(HashEntry->coord, numofcoord);

    //insert the entry into the bin
    EnvROBARM.Coord2StateIDHashTable[i].push_back(HashEntry);

    //insert into and initialize the mappings
    int* entry = new int [NUMOFINDICES_STATEID2IND];
    StateID2IndexMapping.push_back(entry);
    for(i = 0; i < NUMOFINDICES_STATEID2IND; i++)
    {
        StateID2IndexMapping[HashEntry->stateID][i] = -1;
    }

    if(HashEntry->stateID != (int)StateID2IndexMapping.size()-1)
    {
        printf("ERROR in Env... function: last state has incorrect stateID\n");
        exit(1);	
    }

    //time_createhash += clock()-currenttime;
    return HashEntry;
}


/**------------------------------------------------------------------------*/
                       /** Heuristic Computation */
/**------------------------------------------------------------------------*/
int EnvironmentROBARM3D::XYZTO3DIND(int x, int y, int z)
{
    if(EnvROBARMCfg.lowres_collision_checking)
        return ((x) + (y)*EnvROBARMCfg.LowResEnvWidth_c + (z)*EnvROBARMCfg.LowResEnvWidth_c*EnvROBARMCfg.LowResEnvHeight_c);
    else
        return ((x) + (y)*EnvROBARMCfg.EnvWidth_c + (z)*EnvROBARMCfg.EnvWidth_c*EnvROBARMCfg.EnvHeight_c);
}

void EnvironmentROBARM3D::ReInitializeState3D(State3D* state)
{
    state->g = INFINITECOST;
    state->iterationclosed = 0;
}

void EnvironmentROBARM3D::InitializeState3D(State3D* state, short unsigned int x, short unsigned int y, short unsigned int z)
{
    state->g = INFINITECOST;
    state->iterationclosed = 0;
    state->x = x;
    state->y = y;
    state->z = z;
}

void EnvironmentROBARM3D::Create3DStateSpace(State3D**** statespace3D)
{
    int x,y,z;
    int width = EnvROBARMCfg.EnvWidth_c;
    int height = EnvROBARMCfg.EnvHeight_c;
    int depth = EnvROBARMCfg.EnvDepth_c;

    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth = EnvROBARMCfg.LowResEnvDepth_c;
    }

    //allocate a statespace for 3D search
    *statespace3D = new State3D** [width];
    for (x = 0; x < width; x++)
    {
        (*statespace3D)[x] = new State3D* [height];
        for(y = 0; y < height; y++)
        {
            (*statespace3D)[x][y] = new State3D [depth];
            for(z = 0; z < depth; z++)
            {
                InitializeState3D(&(*statespace3D)[x][y][z],x,y,z);
            }
        }
    }
}

void EnvironmentROBARM3D::Delete3DStateSpace(State3D**** statespace3D)
{
    int x,y;
    int width = EnvROBARMCfg.EnvWidth_c;
    int height = EnvROBARMCfg.EnvHeight_c;

    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
    }

    //delete the 3D statespace
    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
            delete [] (*statespace3D)[x][y];

        delete [] (*statespace3D)[x];
    }
    delete *statespace3D;
}

void EnvironmentROBARM3D::ComputeHeuristicValues()
{
#if DEBUG
    printf("Running 3D BFS to compute heuristics\n");
    clock_t currenttime = clock();
#endif

    int hsize;

		// printf("low resolution collision checking is %senabled.\n",EnvROBARMCfg.lowres_collision_checking ? "" : "not ");

    if(EnvROBARMCfg.lowres_collision_checking)
        hsize = XYZTO3DIND(EnvROBARMCfg.LowResEnvWidth_c-1, EnvROBARMCfg.LowResEnvHeight_c-1,EnvROBARMCfg.LowResEnvDepth_c-1)+1;
    else
        hsize = XYZTO3DIND(EnvROBARMCfg.EnvWidth_c-1, EnvROBARMCfg.EnvHeight_c-1,EnvROBARMCfg.EnvDepth_c-1)+1;

    //allocate memory
    EnvROBARM.Heur = new int [hsize];

    //now compute the heuristics for all of the goal locations
    State3D*** statespace3D;
    Create3DStateSpace(&statespace3D);

    Search3DwithQueue(statespace3D, EnvROBARM.Heur, EnvROBARMCfg.EndEffGoals);

    Delete3DStateSpace(&statespace3D);
#if DEBUG
    printf("completed in %.3f seconds.\n", double(clock()-currenttime) / CLOCKS_PER_SEC);
#endif
}

void EnvironmentROBARM3D::Search3DwithQueue(State3D*** statespace, int* HeurGrid, const vector< GoalPos> &Goals)
{
    State3D* ExpState;
    int newx, newy, newz, x,y,z;
    unsigned int g_temp;
		unsigned char*** Grid3D = EnvROBARMCfg.Grid3D;
    boost::shared_ptr<Voxel3d> vGrid = voxel_grid;
    short unsigned int width = EnvROBARMCfg.EnvWidth_c;
    short unsigned int height = EnvROBARMCfg.EnvHeight_c;
    short unsigned int depth = EnvROBARMCfg.EnvDepth_c;
		int radius = EnvROBARMCfg.link_radius[0] / EnvROBARMCfg.GridCellWidth;

    //use low resolution grid if enabled
    if(EnvROBARMCfg.lowres_collision_checking)
    {
			width = EnvROBARMCfg.LowResEnvWidth_c;
			height = EnvROBARMCfg.LowResEnvHeight_c;
			depth = EnvROBARMCfg.LowResEnvDepth_c;
			radius = EnvROBARMCfg.link_radius[0] / EnvROBARMCfg.LowResGridCellWidth;

			Grid3D = EnvROBARMCfg.LowResGrid3D;
			vGrid = lowres_voxel_grid;
    }

    //create a queue
    queue<State3D*> Queue;

    //initialize to infinity all
    for (x = 0; x < width; x++)
    {
			for (y = 0; y < height; y++)
			{
				for (z = 0; z < depth; z++)
				{
					HeurGrid[XYZTO3DIND(x,y,z)] = INFINITECOST;
					ReInitializeState3D(&statespace[x][y][z]);
				}
			}
    }

    //initialization - throw starting states on queue with g cost = 0
    for(unsigned int i = 0; i < Goals.size(); i++)
    {
      if(!EnvROBARMCfg.lowres_collision_checking)
      {
        statespace[Goals[i].xyz[0]][Goals[i].xyz[1]][Goals[i].xyz[2]].g = 0;
        Queue.push(&statespace[Goals[i].xyz[0]][Goals[i].xyz[1]][Goals[i].xyz[2]]);
      }
      else
      {
        statespace[Goals[i].xyz_lr[0]][Goals[i].xyz_lr[1]][Goals[i].xyz_lr[2]].g = 0;
        Queue.push(&statespace[Goals[i].xyz_lr[0]][Goals[i].xyz_lr[1]][Goals[i].xyz_lr[2]]);
      }
    }

// 		clock_t looptime = clock();

    //expand all of the states
    while((int)Queue.size() > 0)
    {
			//get the state to expand
			ExpState = Queue.front();

			Queue.pop();

			//it may be that the state is already closed
			if(ExpState->iterationclosed == 1)
					continue;

			//close it
			ExpState->iterationclosed = 1;

			//set the corresponding heuristics
			HeurGrid[XYZTO3DIND(ExpState->x, ExpState->y, ExpState->z)] = ExpState->g;

			//iterate through neighbors
			for(int d = 0; d < DIRECTIONS; d++)
			{
				newx = ExpState->x + dx[d];
				newy = ExpState->y + dy[d];
				newz = ExpState->z + dz[d];

				//make sure it is inside the map and has no obstacle
				if(0 > newx || newx >= width || 0 > newy || newy >= height || 0 > newz || newz >= depth)
					continue;

				if(!isValidCell(newx,newy,newz,radius,Grid3D, vGrid))
					continue;

				if(statespace[newx][newy][newz].iterationclosed == 0)
				{
					//insert into the stack
					Queue.push(&statespace[newx][newy][newz]);

					//set the g-value
					if (ExpState->x != newx && ExpState->y != newy && ExpState->z != newz)
							g_temp = ExpState->g + EnvROBARMCfg.cost_sqrt3_move;
					else if ((ExpState->y != newy && ExpState->z != newz) ||
										(ExpState->x != newx && ExpState->z != newz) ||
										(ExpState->x != newx && ExpState->y != newy))
							g_temp = ExpState->g + EnvROBARMCfg.cost_sqrt2_move;
					else
							g_temp = ExpState->g + EnvROBARMCfg.cost_per_cell;

					if(statespace[newx][newy][newz].g > g_temp)
							statespace[newx][newy][newz].g = g_temp;
//                 printf("%i: %u,%u,%u, g-cost: %i\n",d,newx,newy,newz,statespace[newx][newy][newz].g);
            }
        }
    }
		
// 		printf("the main dijkstra loop took %.4f seconds.\n", double(clock()-looptime) / CLOCKS_PER_SEC);
}


/**------------------------------------------------------------------------*/
                        /** Domain Specific Functions */
/**------------------------------------------------------------------------*/
void EnvironmentROBARM3D::DiscretizeAngles()
{
    for(int i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
        EnvROBARMCfg.angledelta[i] = (2.0*PI_CONST) / ANGLEDELTA;
        EnvROBARMCfg.anglevals[i] = ANGLEDELTA;
    }
}

/*
//angles are counterclockwise from 0 to 360 in radians, 0 is the center of bin 0, ...
void EnvironmentROBARM3D::ComputeContAngles(const short unsigned int coord[], double angle[])
{
    for(int i = 0; i < EnvROBARMCfg.num_joints; i++)
		  angle[i] = coord[i]*EnvROBARMCfg.angledelta[i];
}

void EnvironmentROBARM3D::ComputeCoord(const double angle[], short unsigned int coord[])
{
    double pos_angle;

    for(int i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
			//NOTE: Added 3/1/09
			pos_angle = angle[i];
			if(pos_angle < 0.0)
					pos_angle += 2*PI_CONST;

			coord[i] = (int)((pos_angle + EnvROBARMCfg.angledelta[i]*0.5)/EnvROBARMCfg.angledelta[i]);

			if(coord[i] == EnvROBARMCfg.anglevals[i])
				coord[i] = 0;
    }
}

// convert a cell in the occupancy grid to point in real world 
void EnvironmentROBARM3D::Cell2ContXYZ(int x, int y, int z, double *pX, double *pY, double *pZ)
{
    *pX = x * EnvROBARMCfg.GridCellWidth + EnvROBARMCfg.origin_m[0];
    *pY = y * EnvROBARMCfg.GridCellWidth + EnvROBARMCfg.origin_m[1];
    *pZ = z * EnvROBARMCfg.GridCellWidth + EnvROBARMCfg.origin_m[2];
}

// convert a cell in the occupancy grid to point in real world
void EnvironmentROBARM3D::Cell2ContXYZ(int x, int y, int z, double *pX, double *pY, double *pZ, double gridcell_m)
{
    *pX = x * gridcell_m + EnvROBARMCfg.origin_m[0];
    *pY = y * gridcell_m + EnvROBARMCfg.origin_m[1];
    *pZ = z * gridcell_m + EnvROBARMCfg.origin_m[2];
}

// convert a point in real world to a cell in occupancy grid 
void EnvironmentROBARM3D::ContXYZ2Cell(double x, double y, double z, short unsigned int *pX, short unsigned int *pY, short unsigned int *pZ)
{
    *pX = (int)((x - EnvROBARMCfg.origin_m[0]) / EnvROBARMCfg.GridCellWidth);
    if(*pX >= EnvROBARMCfg.EnvWidth_c) *pX = EnvROBARMCfg.EnvWidth_c-1;

    *pY = (int)((y - EnvROBARMCfg.origin_m[1]) / EnvROBARMCfg.GridCellWidth);
    if(*pX >= EnvROBARMCfg.EnvWidth_c) *pX = EnvROBARMCfg.EnvWidth_c-1;

    *pZ = (int)((z - EnvROBARMCfg.origin_m[2]) / EnvROBARMCfg.GridCellWidth);
    if(*pZ >= EnvROBARMCfg.EnvDepth_c) *pZ = EnvROBARMCfg.EnvDepth_c-1;
}

void EnvironmentROBARM3D::ContXYZ2Cell(double* xyz, double gridcellwidth, int dims_c[3], short unsigned int *pXYZ)
{
    pXYZ[0] = (int)((xyz[0] - EnvROBARMCfg.origin_m[0]) / gridcellwidth);
    if(pXYZ[0] >= dims_c[0])
      pXYZ[0] = dims_c[0]-1;

    pXYZ[1] = (int)((xyz[1] - EnvROBARMCfg.origin_m[1]) / gridcellwidth);
    if(pXYZ[1] >= dims_c[1])
      pXYZ[1] = dims_c[1]-1;

    pXYZ[2] = (int)((xyz[2] - EnvROBARMCfg.origin_m[2]) / gridcellwidth);
    if(pXYZ[2] >= dims_c[2]) 
      pXYZ[2] = dims_c[2]-1;
}
*/

void EnvironmentROBARM3D::HighResGrid2LowResGrid(short unsigned int * XYZ_hr, short unsigned int * XYZ_lr)
{
    double scale = EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth;

    XYZ_lr[0] = XYZ_hr[0] * scale;
    XYZ_lr[1] = XYZ_hr[1] * scale;
    XYZ_lr[2] = XYZ_hr[2] * scale;
}

int EnvironmentROBARM3D::cost(short unsigned int state1coord[], short unsigned int state2coord[])
{
//     if(!IsValidCoord(state1coord) || !IsValidCoord(state2coord))
//         return INFINITECOST;

#if UNIFORM_COST
    return 1*COSTMULT;
#else
//     printf("%i\n",1 * COSTMULT * (EnvROBARMCfg.Grid3D[HashEntry2->endeff[0]][HashEntry1->endeff[1]][HashEntry1->endeff[2]] + 1));
    //temporary
    return 1*COSTMULT;

#endif
}

int EnvironmentROBARM3D::cost(EnvROBARMHashEntry_t* HashEntry1, EnvROBARMHashEntry_t* HashEntry2, bool bState2IsGoal)
{

	#if UNIFORM_COST
			return 1*COSTMULT;
	#else

		if(HashEntry2->dist < 10)
			return 1 * COSTMULT * 10;
		else if(HashEntry2->dist < 20)
			return 1 * COSTMULT * 20;
		else
			return 1 * COSTMULT;
		
	#endif
}

void EnvironmentROBARM3D::GetAxisAngle(double R1[3][3], double R2[3][3], double* angle)
{
    short unsigned int x,y,k;
    double sum, R3[3][3], R1_T[3][3];

    //R1_T = transpose R1
    for(x=0; x<3; x++)
        for(y=0; y<3; y++)
            R1_T[x][y] = R1[y][x];

    //R3= R1_T * R2
    for (x=0; x<3; x++)
    {
        for (y=0; y<3; y++)
        {
            sum = 0.0;
            for (k=0; k<3; k++)
                sum += (R1_T[x][k] * R2[k][y]);

            R3[x][y] = sum;
        }
    }
    *angle = acos(((R3[0][0] + R3[1][1] + R3[2][2]) - 1.0) / 2.0);
}


/**------------------------------------------------------------------------*/
                /** Initialization Functions */
/**------------------------------------------------------------------------*/
EnvironmentROBARM3D::EnvironmentROBARM3D()
{
    /* FK & COLLISION CHECKING */
    EnvROBARMCfg.bInitialized = false;
    EnvROBARMCfg.use_DH = 1;
    EnvROBARMCfg.enforce_motor_limits = 1;
    EnvROBARMCfg.endeff_check_only = 0;
    EnvROBARMCfg.object_grasped = 0;
    EnvROBARMCfg.enforce_upright_gripper = 0;
    EnvROBARMCfg.num_joints = 7;
    EnvROBARMCfg.gripper_m[0] = .19;
    EnvROBARMCfg.gripper_m[1] = 0;
    EnvROBARMCfg.gripper_m[2] = 0;

    /* SUCCESSOR ACTIONS */
    EnvROBARMCfg.use_smooth_actions = 1;
    EnvROBARMCfg.smoothing_weight = 0.0;
    EnvROBARMCfg.HighResActionsThreshold_c = 2;
    EnvROBARMCfg.lowres_collision_checking = 0;
    EnvROBARMCfg.multires_succ_actions = 1;

    /* HEURISTIC */
    EnvROBARMCfg.dijkstra_heuristic = 1;
    EnvROBARMCfg.ApplyRPYCost_m = .1;

    /* GOALS */
    EnvROBARMCfg.bGoalIsSet = false;
    EnvROBARMCfg.gripper_orientation_moe = .0175; // 0.0125;
    EnvROBARMCfg.checkEndEffGoalOrientation = 0;

    /* COSTS & COST MAP */
    EnvROBARMCfg.variable_cell_costs = false;
    EnvROBARMCfg.padding = 0.05;
    EnvROBARMCfg.ObstacleCost = 1;  //TODO Change this to 10
    EnvROBARMCfg.medObstacleCost = 1;
    EnvROBARMCfg.lowObstacleCost = 1;
    EnvROBARMCfg.medCostRadius_c = 1;
    EnvROBARMCfg.lowCostRadius_c = 1;
    EnvROBARMCfg.CellsPerAction = -1;

    /* ENVIRONMENT */
    EnvROBARMCfg.GridCellWidth = .01;   //default map has 1cm resolution
    EnvROBARMCfg.LowResGridCellWidth = 2 * EnvROBARMCfg.GridCellWidth;

    //default map size is geared towards a planted 1 meter long robotic arm
    EnvROBARMCfg.EnvWidth_m = 1.4;
    EnvROBARMCfg.EnvHeight_m = 2;
    EnvROBARMCfg.EnvDepth_m = 2;

    EnvROBARMCfg.EnvWidth_c = EnvROBARMCfg.EnvWidth_m / EnvROBARMCfg.GridCellWidth;
    EnvROBARMCfg.EnvHeight_c = EnvROBARMCfg.EnvHeight_m / EnvROBARMCfg.GridCellWidth;
    EnvROBARMCfg.EnvDepth_c = EnvROBARMCfg.EnvDepth_m / EnvROBARMCfg.GridCellWidth;

    //Low-Res Environment for Colision Checking
    EnvROBARMCfg.LowResEnvWidth_c = EnvROBARMCfg.EnvWidth_m / EnvROBARMCfg.LowResGridCellWidth;
    EnvROBARMCfg.LowResEnvHeight_c = EnvROBARMCfg.EnvHeight_m / EnvROBARMCfg.LowResGridCellWidth;
    EnvROBARMCfg.LowResEnvDepth_c = EnvROBARMCfg.EnvDepth_m / EnvROBARMCfg.LowResGridCellWidth;

    //base of arm is usually at center of environment
    EnvROBARMCfg.BaseX_m = 0;
    EnvROBARMCfg.BaseY_m = 0;
    EnvROBARMCfg.BaseZ_m = 0;
    ContXYZ2Cell(EnvROBARMCfg.BaseX_m, EnvROBARMCfg.BaseY_m, EnvROBARMCfg.BaseZ_m, &(EnvROBARMCfg.BaseX_c), &(EnvROBARMCfg.BaseY_c), &(EnvROBARMCfg.BaseZ_c));

    EnvROBARMCfg.sum_heuristics = 1;

    /* JOINT SPACE PLANNING */
    EnvROBARMCfg.PlanInJointSpace = false;

    //hard coded temporarily (meters)
    EnvROBARMCfg.arm_length = 1.3;
    EnvROBARMCfg.link_radius.push_back(.08);
    EnvROBARMCfg.link_radius.push_back(.07);
    EnvROBARMCfg.link_radius.push_back(.06);

    EnvROBARMCfg.exact_gripper_collision_checking = false;
    EnvROBARMCfg.use_voxel3d_occupancy_grid = false;

		save_expanded_states = false;
}

void EnvironmentROBARM3D::InitializeEnvConfig()
{
    //find the discretization for each angle and store the discretization
  DiscretizeAngles();
}

bool EnvironmentROBARM3D::InitializeEnvironment()
{
  short unsigned int coord[NUMOFLINKS] = {0};
  short unsigned int endeff[3] = {0};
  double orientation[3][3];

    //initialize the map from Coord to StateID
  EnvROBARM.HashTableSize = 32*1024; //should be power of two
  EnvROBARM.Coord2StateIDHashTable = new std::vector<EnvROBARMHashEntry_t*>[EnvROBARM.HashTableSize];

    //initialize the map from StateID to Coord
  EnvROBARM.StateID2CoordTable.clear();

    //create an empty start state
  EnvROBARM.startHashEntry = CreateNewHashEntry(coord, NUMOFLINKS, endeff, 0, orientation);

    //create the goal state
  EnvROBARM.goalHashEntry = CreateNewHashEntry(coord, NUMOFLINKS, endeff, 0, NULL);

    //create the goal state
  if(EnvROBARMCfg.bGoalIsSet)
  {
    EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals[0].xyz[0];
    EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals[0].xyz[1];
    EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals[0].xyz[2];
  }

  //for now heuristics are not set
  EnvROBARM.Heur = NULL;

  return true;
}

bool EnvironmentROBARM3D::InitializeEnv(const char* sEnvFile)
{
//     FILE* fCfg = fopen(sEnvFile, "r");
//     ParseYAMLFile(sEnvFile);
//     exit(1);

    //parse the parameter file - temporary
    char parFile[] = "params.cfg";
    FILE* fCfg = fopen(parFile, "r");
    if(fCfg == NULL)
        printf("ERROR: unable to open %s.....using defaults.\n", parFile);

    ReadParamsFile(fCfg);
    fclose(fCfg);

    //parse the configuration file
    fCfg = fopen(sEnvFile, "r");
    if(fCfg == NULL)
    {
        printf("ERROR: unable to open %s\n", sEnvFile);
        exit(1);
    }
    ReadConfiguration(fCfg);
    fclose(fCfg);

    //fetch the robot arm description
    std::ifstream ifs("pr2_desc.xml");
    std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EnvROBARMCfg.robot_desc = str;

    if(!InitGeneral())
        return false;

    EnvROBARMCfg.exact_gripper_collision_checking = false;

    //initialize the angles of the start states
    if(!SetStartJointConfig(EnvROBARMCfg.LinkStartAngles_d, 0))
      printf("[InitializeEnvironment] Start state was not created.\n");

    //set Environment is Initialized flag(so fk can be used)
    EnvROBARMCfg.bInitialized = true;

    //set goals
    if(EnvROBARMCfg.PlanInJointSpace)
    {
      vector<vector<double> > tolerance(1,vector<double>(NUMOFLINKS,.175));
      if(!SetGoalConfiguration(EnvROBARMCfg.ParsedGoals, tolerance, tolerance))
      {
          printf("Failed to set joint space goals.\n");
          exit(1);
      }
    }
    else
    {
      vector<int> type(1,0);
      type[0] = (1 | 2 | 4);
      if(!SetGoalPosition(EnvROBARMCfg.ParsedGoals,EnvROBARMCfg.ParsedGoalTolerance,type))
      {
        printf("Failed to set goal positions.\n");
        exit(1);
      }
    }

#if VERBOSE
    //output environment data
    PrintConfiguration(stdout);
#endif

#if DEBUG
    PrintConfiguration(fDeb);
#endif

    //for statistics purposes
    starttime = clock();

    return true;
}

bool EnvironmentROBARM3D::InitEnvFromFilePtr(FILE* eCfg, FILE* pCfg, const std::string &robot_desc)
{
    if(robot_desc.empty())
    {
      printf("robot description file is empty.\n");
      return false;
    }

    //parse the parameter file
    ReadParamsFile(pCfg);

    //parse the environment configuration file
    ReadConfiguration(eCfg);

    EnvROBARMCfg.robot_desc = robot_desc;

    if(!InitGeneral())
      return false;

		#if VERBOSE
				//output environment data
				PrintConfiguration(stdout);
		#endif

    //set Environment is Initialized flag(so fk can be used)
    EnvROBARMCfg.bInitialized = true;

		#if DEBUG
				PrintConfiguration(fDeb);
		#endif

    EnvROBARMCfg.exact_gripper_collision_checking = true;

    //for statistics purposes
    starttime = clock();

    printf("[InitEnvFromFilePtr] Environment has been initialized.\n");

		double angles[NUMOFLINKS] = {0};
		double x,y,z;
		short unsigned int a,b,c;
		for(int j = 0; j < 9; j++)
		{
		  ComputeForwardKinematics_ROS(angles, j, &(x), &(y), &(z));
			ContXYZ2Cell(x,y,z,&a,&b,&c);
		  printf("%i: %.3f %.3f %.3f (%u %u %u)\n",j,x,y,z,a,b,c);
		}
		
    return true;
}

bool EnvironmentROBARM3D::InitGeneral()
{
    //initialize forward kinematics
    if (!EnvROBARMCfg.use_DH)
    {
      //initialize kdl chain
      if(!InitKDLChain(EnvROBARMCfg.robot_desc))
				return false;
    }
    else
        //pre-compute DH Transformations
        ComputeDHTransformations();

    //temporary location for this
    EnvROBARMCfg.Grid3D_dims[0] = EnvROBARMCfg.EnvWidth_c;
    EnvROBARMCfg.Grid3D_dims[1] = EnvROBARMCfg.EnvHeight_c;
    EnvROBARMCfg.Grid3D_dims[2] = EnvROBARMCfg.EnvDepth_c;
    EnvROBARMCfg.LowResGrid3D_dims[0] = EnvROBARMCfg.LowResEnvWidth_c;
    EnvROBARMCfg.LowResGrid3D_dims[1] = EnvROBARMCfg.LowResEnvHeight_c;
    EnvROBARMCfg.LowResGrid3D_dims[2] = EnvROBARMCfg.LowResEnvDepth_c;

		if(EnvROBARMCfg.lowres_collision_checking)
		{
			EnvROBARMCfg.upper_arm_radius = EnvROBARMCfg.link_radius.at(0) / EnvROBARMCfg.LowResGridCellWidth;
			EnvROBARMCfg.forearm_radius = EnvROBARMCfg.link_radius.at(1) / EnvROBARMCfg.LowResGridCellWidth;
			EnvROBARMCfg.gripper_radius = EnvROBARMCfg.link_radius.at(2) / EnvROBARMCfg.LowResGridCellWidth;
		}
		else
		{
			EnvROBARMCfg.upper_arm_radius = EnvROBARMCfg.link_radius.at(0) / EnvROBARMCfg.GridCellWidth;
			EnvROBARMCfg.forearm_radius = EnvROBARMCfg.link_radius.at(1) / EnvROBARMCfg.GridCellWidth;
			EnvROBARMCfg.gripper_radius = EnvROBARMCfg.link_radius.at(2) / EnvROBARMCfg.GridCellWidth;
		}
		
    //mark continuous joints (temporary hack)
    for(int i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
      if(EnvROBARMCfg.joints[i].positive_limit == 0 && EnvROBARMCfg.joints[i].negative_limit == 0)
        EnvROBARMCfg.joints[i].cont = true;
      else
		    EnvROBARMCfg.joints[i].cont = false;
    }

    //initialize other parameters of the environment
    InitializeEnvConfig();

    //initialize Environment
    if(InitializeEnvironment() == false)
        return false;

    //pre-compute action-to-action costs
    ComputeActionCosts();

    //compute the cost per cell to be used by heuristic
    ComputeCostPerCell();

    return true;
}

bool EnvironmentROBARM3D::InitializeMDPCfg(MDPConfig *MDPCfg)
{
	//initialize MDPCfg with the start and goal ids	
    MDPCfg->goalstateid = EnvROBARM.goalHashEntry->stateID;
    MDPCfg->startstateid = EnvROBARM.startHashEntry->stateID;

    return true;
}

double EnvironmentROBARM3D::GetEpsilon()
{
  return EnvROBARMCfg.epsilon;
}

/** \brief parse a text file that includes the environment description and start and goals */
void EnvironmentROBARM3D::ReadConfiguration(FILE* fCfg)
{
  char sTemp[1024];
  int i;
  double obs[6];

  fscanf(fCfg, "%s", sTemp);
  while(!feof(fCfg) && strlen(sTemp) != 0)
  {
    if(strcmp(sTemp, "environmentsize(meters):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.EnvWidth_m = atof(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.EnvHeight_m = atof(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.EnvDepth_m = atof(sTemp);
    }
    else if(strcmp(sTemp, "discretization(cells):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.EnvWidth_c = atoi(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.EnvHeight_c = atoi(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.EnvDepth_c = atoi(sTemp);

            //Low-Res Environment for Colision Checking
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.LowResEnvWidth_c = atoi(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.LowResEnvHeight_c = atoi(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.LowResEnvDepth_c = atoi(sTemp);

            //set additional parameters
      EnvROBARMCfg.GridCellWidth = EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.EnvWidth_c);
      EnvROBARMCfg.LowResGridCellWidth = EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.LowResEnvWidth_c);

            //temporary - I have no clue why the commented line doesn't work
      double a = (EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.EnvWidth_c));
      double b = (EnvROBARMCfg.EnvHeight_m / double(EnvROBARMCfg.EnvHeight_c));
      double c = (EnvROBARMCfg.EnvDepth_m / double(EnvROBARMCfg.EnvDepth_c));
      printf("%f %f %f\n",a,b,c);
      if((a != b) || (a != c) || (b != c))
//             if(((EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.EnvWidth_c)) != (EnvROBARMCfg.EnvHeight_m / double(EnvROBARMCfg.EnvHeight_c))) ||
// 	       ((EnvROBARMCfg.EnvWidth_m / double(EnvROBARMCfg.EnvWidth_c)) != (EnvROBARMCfg.EnvDepth_m / double(EnvROBARMCfg.EnvDepth_c))))
      {
				printf("ERROR: The cell must be square\n");
				exit(1);
      }

      //allocate memory for environment grid
			InitializeEnvGrid();
    }
		else if(strcmp(sTemp, "origin_xyz(meters):") == 0)
		{
			fscanf(fCfg, "%s", sTemp);
			EnvROBARMCfg.origin_m[0] = atof(sTemp);
			fscanf(fCfg, "%s", sTemp);
			EnvROBARMCfg.origin_m[1] = atof(sTemp);
			fscanf(fCfg, "%s", sTemp);
			EnvROBARMCfg.origin_m[2] = atof(sTemp);
		}
    else if(strcmp(sTemp, "basexyz(cells):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.BaseX_c = atoi(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.BaseY_c = atoi(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.BaseZ_c = atoi(sTemp);

            //convert shoulder base from cell to real world coords
      Cell2ContXYZ(EnvROBARMCfg.BaseX_c, EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c, &(EnvROBARMCfg.BaseX_m), &(EnvROBARMCfg.BaseY_m), &(EnvROBARMCfg.BaseZ_m)); 
    }
    else if(strcmp(sTemp, "basexyz(meters):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.BaseX_m = atof(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.BaseY_m = atof(sTemp);
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.BaseZ_m = atof(sTemp);
      ContXYZ2Cell(EnvROBARMCfg.BaseX_m, EnvROBARMCfg.BaseY_m, EnvROBARMCfg.BaseZ_m, &(EnvROBARMCfg.BaseX_c), &(EnvROBARMCfg.BaseY_c), &(EnvROBARMCfg.BaseZ_c)); 
			printf("BASE: %u %u %u\n", (EnvROBARMCfg.BaseX_c), (EnvROBARMCfg.BaseY_c), (EnvROBARMCfg.BaseZ_c));
    }
    else if(strcmp(sTemp, "number_of_planning_joints:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.num_joints = atof(sTemp);

      EnvROBARMCfg.joints.resize(EnvROBARMCfg.num_joints);
    }
    else if(strcmp(sTemp, "linkstartangles(degrees):") == 0)
    {
      for(i = 0; i < EnvROBARMCfg.num_joints; i++)
      {
	fscanf(fCfg, "%s", sTemp);
	EnvROBARMCfg.LinkStartAngles_d[i] = atof(sTemp);
      }
    }
    else if(strcmp(sTemp, "linkstartangles(radians):") == 0)
    {
      for(i = 0; i < EnvROBARMCfg.num_joints; i++)
      {
	fscanf(fCfg, "%s", sTemp);
	EnvROBARMCfg.LinkStartAngles_d[i] = RAD2DEG(atof(sTemp));
      }
    }
    else if(strcmp(sTemp, "endeffectorgoal(meters):") == 0)
    {
      EnvROBARMCfg.PlanInJointSpace = false;

      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.ParsedGoals.resize(atoi(sTemp));

      for(unsigned int i = 0; i < EnvROBARMCfg.ParsedGoals.size(); i++)
      {
	EnvROBARMCfg.ParsedGoals[i].resize(6);

	for(unsigned int k = 0; k < 6; k++)
	{
	  fscanf(fCfg, "%s", sTemp);
	  EnvROBARMCfg.ParsedGoals[i][k] = atof(sTemp);
	}
      }
    }
    else if(strcmp(sTemp, "goal_tolerance(meters,radians):") == 0)
    {
      EnvROBARMCfg.PlanInJointSpace = false;

      EnvROBARMCfg.ParsedGoalTolerance.resize(1);
      EnvROBARMCfg.ParsedGoalTolerance[0].resize(2);

	  //distance tolerance (m)
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.ParsedGoalTolerance[0][0] = atof(sTemp);
	  //orientation tolerance (rad)
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.ParsedGoalTolerance[0][1] = atof(sTemp);
    }
    else if(strcmp(sTemp, "jointspacegoal(radians):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.ParsedGoals.resize(atoi(sTemp));

      for(unsigned int i = 0; i < EnvROBARMCfg.ParsedGoals.size(); i++)
      {
	EnvROBARMCfg.ParsedGoals[i].resize(NUMOFLINKS);

	for(unsigned int k = 0; k < NUMOFLINKS; k++)
	{
	  fscanf(fCfg, "%s", sTemp);
	  EnvROBARMCfg.ParsedGoals[i][k] = atof(sTemp);
	}
      }
      EnvROBARMCfg.PlanInJointSpace = true;
    }
    else if(strcmp(sTemp, "linktwist(degrees):") == 0)
    {
      for(i = 0; i < NUMOFLINKS; i++)
      {
	fscanf(fCfg, "%s", sTemp);
	EnvROBARMCfg.DH_alpha[i] = PI_CONST*(atof(sTemp)/180.0);
      }
    }
    else if(strcmp(sTemp, "linklength(meters):") == 0)
    {
      for(i = 0; i < NUMOFLINKS; i++)
      {
	fscanf(fCfg, "%s", sTemp);
	EnvROBARMCfg.DH_a[i] = atof(sTemp);
      }
    }
    else if(strcmp(sTemp, "linkoffset(meters):") == 0)
    {
      for(i = 0; i < NUMOFLINKS; i++)
      {
	fscanf(fCfg, "%s", sTemp);
	EnvROBARMCfg.DH_d[i] = atof(sTemp);
      }
    }
    else if(strcmp(sTemp, "jointangle(degrees):") == 0)
    {
      for(i = 0; i < NUMOFLINKS; i++)
      {
	fscanf(fCfg, "%s", sTemp);
	EnvROBARMCfg.DH_theta[i] = DEG2RAD(atof(sTemp));
      }
    }
    else if(strcmp(sTemp,"posmotorlimits(degrees):") == 0)
    {
      for(i = 0; i < EnvROBARMCfg.num_joints; i++)
      {
	fscanf(fCfg, "%s", sTemp);
// 	    EnvROBARMCfg.PosMotorLimits[i] = DEG2RAD(atof(sTemp));
	EnvROBARMCfg.joints[i].positive_limit = DEG2RAD(atof(sTemp));
      }
    }
    else if(strcmp(sTemp,"posmotorlimits(radians):") == 0)
    {
      for(i = 0; i < EnvROBARMCfg.num_joints; i++)
      {
	fscanf(fCfg, "%s", sTemp);
// 	    EnvROBARMCfg.PosMotorLimits[i] = atof(sTemp);
	EnvROBARMCfg.joints[i].positive_limit = atof(sTemp);
      }
    }
    else if(strcmp(sTemp,"negmotorlimits(degrees):") == 0)
    {
      for(i = 0; i < EnvROBARMCfg.num_joints; i++)
      {
	fscanf(fCfg, "%s", sTemp);
// 	    EnvROBARMCfg.NegMotorLimits[i] = DEG2RAD(atof(sTemp));
	EnvROBARMCfg.joints[i].negative_limit = DEG2RAD(atof(sTemp));
      }
    }
    else if(strcmp(sTemp,"negmotorlimits(radians):") == 0)
    {
      for(i = 0; i < EnvROBARMCfg.num_joints; i++)
      {
	fscanf(fCfg, "%s", sTemp);
// 	    EnvROBARMCfg.NegMotorLimits[i] = atof(sTemp);
	EnvROBARMCfg.joints[i].negative_limit = atof(sTemp);
      }
    }
    else if(strcmp(sTemp, "environment:") == 0)
    {
      if(EnvROBARMCfg.use_voxel3d_occupancy_grid)
      {
				printf("obstacles will not be parsed in the environment configuration file because the voxel3d is being used.\n");
				return;
      }

      fscanf(fCfg, "%s", sTemp);
      while(!feof(fCfg) && strlen(sTemp) != 0)
      {
				if(strcmp(sTemp, "cube(meters):") == 0)
				{
					for(i = 0; i < 6; i++)
					{
						fscanf(fCfg, "%s", sTemp);
						obs[i] = atof(sTemp);
					}
					AddObstacleToGrid(obs, 0, EnvROBARMCfg.Grid3D, EnvROBARMCfg.GridCellWidth);
			
					if(EnvROBARMCfg.lowres_collision_checking)
						AddObstacleToGrid(obs, 0, EnvROBARMCfg.LowResGrid3D, EnvROBARMCfg.LowResGridCellWidth);
				}
				else if (strcmp(sTemp, "cube(cells):") == 0)
				{
					for(i = 0; i < 6; i++)
					{
						fscanf(fCfg, "%s", sTemp);
						obs[i] = atof(sTemp);
					}
					AddObstacleToGrid(obs, 1, EnvROBARMCfg.Grid3D, EnvROBARMCfg.GridCellWidth);
			
					if(EnvROBARMCfg.lowres_collision_checking)
						AddObstacleToGrid(obs, 1, EnvROBARMCfg.LowResGrid3D, EnvROBARMCfg.LowResGridCellWidth);
				}
				else
				{
					printf("ERROR: Environment Config file contains unknown object type.\n");
					exit(1);
				}
				fscanf(fCfg, "%s", sTemp);
      }
    }
    else
    {
      printf("ERROR: Unknown parameter name in environment config file: %s.\n", sTemp);
    }
    fscanf(fCfg, "%s", sTemp);
  }

//     printf("Parsed environment config file.\n");
}

/** \brief parse a text file that contains planner options (motion primitives) */
void EnvironmentROBARM3D::ReadParamsFile(FILE* fCfg)
{
  char sTemp[1024];
  int x,y,nrows,ncols;

  fscanf(fCfg, "%s", sTemp);
  while(!feof(fCfg) && strlen(sTemp) != 0)
  {
    if(strcmp(sTemp, "Use_DH_for_FK:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.use_DH = atoi(sTemp);
    }
    else if(strcmp(sTemp, "Enforce_Motor_Limits:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.enforce_motor_limits = atoi(sTemp);
    }
    else if(strcmp(sTemp, "Use_Dijkstra_for_Heuristic:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.dijkstra_heuristic = atoi(sTemp);
    }
    else if(strcmp(sTemp, "Collision_Checking_on_EndEff_only:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.endeff_check_only = atoi(sTemp);
    }
    else if(strcmp(sTemp, "Obstacle_Padding(meters):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.padding = atof(sTemp);
    }
    else if(strcmp(sTemp, "Smoothing_Weight:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.smoothing_weight = atof(sTemp);
    }
    else if(strcmp(sTemp, "Use_Path_Smoothing:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.use_smooth_actions = atoi(sTemp);
    }
    else if(strcmp(sTemp, "Keep_Gripper_Upright_for_Whole_Path:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.enforce_upright_gripper = atoi(sTemp);
    }
    else if(strcmp(sTemp, "Check_Orientation_of_EndEff_at_Goal:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.checkEndEffGoalOrientation = atoi(sTemp);
    }
    else if(strcmp(sTemp,"ARAPlanner_Epsilon:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.epsilon = atof(sTemp);
    }
    else if(strcmp(sTemp,"Length_of_Grasped_Cylinder(meters):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.grasped_object_length_m = atof(sTemp);
    }
    else if(strcmp(sTemp,"Cylinder_in_Gripper:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.object_grasped = atoi(sTemp);
    }
//         else if(strcmp(sTemp,"EndEffGoal_MarginOfError(meters):") == 0)
//         {
//             fscanf(fCfg, "%s", sTemp);
//             EnvROBARMCfg.goal_moe_m = atof(sTemp);
//         }
    else if(strcmp(sTemp,"LowRes_Collision_Checking:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.lowres_collision_checking = atoi(sTemp);
    }
    else if(strcmp(sTemp,"MultiRes_Successor_Actions:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.multires_succ_actions = atoi(sTemp);
    }
    else if(strcmp(sTemp,"Increasing_Cell_Costs_Near_Obstacles:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.variable_cell_costs = atoi(sTemp);
    }
    else if(strcmp(sTemp,"HighResActions_Distance_Threshold(cells):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.HighResActionsThreshold_c = atof(sTemp);
    }
    else if(strcmp(sTemp,"HighResActions_Distance_Threshold(radians):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.HighResActionsThreshold_r = atof(sTemp);
    }
    else if(strcmp(sTemp,"Dist_from_goal_to_plan_for_RPY(meters):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.ApplyRPYCost_m = atof(sTemp);
    }
    else if(strcmp(sTemp,"Use_selective_actions(js_planner_only):") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.use_selective_actions = atoi(sTemp);
    }
    else if(strcmp(sTemp,"PlanInJointSpace:") == 0)
    {
      fscanf(fCfg, "%s", sTemp);
      EnvROBARMCfg.PlanInJointSpace = atoi(sTemp);
    }
        //parse actions - it must be the last thing in the file
    else if(strcmp(sTemp, "Actions:") == 0)
    {
      break;
    }
    else
    {
      printf("Error: Invalid Field name (%s) in parameter file.\n",sTemp);
      exit(1);
    }
    fscanf(fCfg, "%s", sTemp);
  }

    // parse successor actions
  fscanf(fCfg, "%s", sTemp);
  nrows = atoi(sTemp);
  fscanf(fCfg, "%s", sTemp);
  ncols = atoi(sTemp);

  EnvROBARMCfg.nLowResActions = ncols * 2;
  EnvROBARMCfg.nSuccActions = nrows * 2;

    //initialize EnvROBARM.SuccActions & parse config file
  EnvROBARMCfg.SuccActions = new double* [EnvROBARMCfg.nSuccActions];
  for (x=0; x < EnvROBARMCfg.nSuccActions; x+=2)
  {
    EnvROBARMCfg.SuccActions[x] = new double [NUMOFLINKS];
    EnvROBARMCfg.SuccActions[x+1] = new double [NUMOFLINKS];

    for(y=0; y < NUMOFLINKS; y++)
    {
      fscanf(fCfg, "%s", sTemp);
      if(!feof(fCfg) && strlen(sTemp) != 0)
      {
	EnvROBARMCfg.SuccActions[x][y] = atof(sTemp);
	EnvROBARMCfg.SuccActions[x+1][y] = -1 * atof(sTemp);
      }
      else
      {
	printf("ERROR: End of parameter file reached prematurely. Check for newline.\n");
	exit(1);
      }
    }
  }
}

void EnvironmentROBARM3D::ParseYAMLFile(const char* sParamFile)
{
//   std::ifstream fin(sParamFile);
//   YAML::Parser parser(fin);
// 
//   printf("parsin\n");
//   YAML::Node doc;
//   parser.GetNextDocument(doc);
// 
//   printf("reading\n");
// 
//   std::string output;
//   doc ["width_m"] >> output;
//   printf("%s\n",output.c_str());

//   for(YAML::Iterator it=doc.begin();it!=doc.end();++it)
//   {
//     std::string key, value;
//     it.first() >> key;
//     it.second() >> value;
//     std::cout << "Key: " << key << ", value: " << value << std::endl;
//   }
}

bool EnvironmentROBARM3D::SetEnvParameter(std::string param, double value)
{
//     if(EnvROBARMCfg.bInitialized == true)
//       printf("Warning: some parameters must be set before initialization of the environment\n");

  printf("setting param %s to %2.1f\n", param.c_str(), value);

  if(param.compare("useDHforFK") == 0)
  {
    EnvROBARMCfg.use_DH = value;
  }
  else if(param.compare("enforceMotorLimits") == 0)
  {
    EnvROBARMCfg.enforce_motor_limits = value;
  }
  else if(param.compare("useDijkstraHeuristic") == 0)
  {
    EnvROBARMCfg.dijkstra_heuristic = value;
  }
  else if(param.compare("paddingSize") == 0)
  {
    EnvROBARMCfg.padding = value;
  }
  else if(param.compare("smoothingWeight") == 0)
  {
    EnvROBARMCfg.smoothing_weight = value;
  }
  else if(param.compare("usePathSmoothing") == 0)
  {
    EnvROBARMCfg.use_smooth_actions = value;
  }
  else if(param.compare("uprightGripperOnly") == 0)
  {
    EnvROBARMCfg.enforce_upright_gripper = value;
  }
  else if(param.compare("use6DOFGoal") == 0)
  {
    EnvROBARMCfg.checkEndEffGoalOrientation = value;
  }
  else if(param.compare("useFastCollisionChecking") == 0)
  {
    EnvROBARMCfg.lowres_collision_checking = value;
  }
  else if(param.compare("useMultiResActions") == 0)
  {
//     EnvROBARMCfg.multires_succ_actions = value;
  }
  else if(param.compare("useHigherCostsNearObstacles") == 0)
  {
    EnvROBARMCfg.variable_cell_costs = value;
  }
  else if(param.compare("useVoxel3dForOccupancyGrid") == 0)
  {
    EnvROBARMCfg.use_voxel3d_occupancy_grid = value;
  }
  else if(param.compare("exactGripperCollisionChecking") == 0)
  {
    EnvROBARMCfg.exact_gripper_collision_checking = value;
  }
  else if(param.compare("useMultiResolutionMotionPrimitives") == 0)
  {
    EnvROBARMCfg.multires_succ_actions = value;
  }
	else if(param.compare("saveExpandedStateIDs") == 0)
	{
		save_expanded_states = value;
	}
  else
  {
    printf("ERROR: invalid param %s\n", param.c_str());
    return false;
  }

  return true;
}


/**------------------------------------------------------------------------*/
                /** Interface with SBPL Planners */
/**------------------------------------------------------------------------*/
int EnvironmentROBARM3D::GetFromToHeuristic(int FromStateID, int ToStateID)
{
    if(EnvROBARMCfg.PlanInJointSpace)
        return GetJointSpaceHeuristic(FromStateID,ToStateID);

#if USE_HEUR==0
    return 0;
#endif

    int heur = 0, closest_goal = 0;
    double FromEndEff_m[3];
    double edist_to_goal_m, heur_axis_angle;

    //get X, Y, Z for the state
    EnvROBARMHashEntry_t* FromHashEntry = EnvROBARM.StateID2CoordTable[FromStateID];
    short unsigned int FromEndEff[3] = {FromHashEntry->endeff[0], FromHashEntry->endeff[1], FromHashEntry->endeff[2]};

    //distance to closest goal in meters
    Cell2ContXYZ(FromHashEntry->endeff[0],FromHashEntry->endeff[1],FromHashEntry->endeff[2],&(FromEndEff_m[0]),&(FromEndEff_m[1]),&(FromEndEff_m[2]));
    edist_to_goal_m = GetDistToClosestGoal(FromEndEff_m, &closest_goal);

    //get distance heuristic
    if(EnvROBARMCfg.dijkstra_heuristic)
    {
        if(EnvROBARMCfg.lowres_collision_checking)
        {
            HighResGrid2LowResGrid(FromHashEntry->endeff, FromEndEff);

            //fetch precomputed heuristic
            heur = EnvROBARM.Heur[XYZTO3DIND(FromEndEff[0], FromEndEff[1], FromEndEff[2])];

//             double dist_to_goal_c = GetDistToClosestGoal(FromHashEntry->endeff, &closest_goal);
// 
//             //if the distance to the goal is very small, then we need a more informative heuristic when using the lowres collision detection
//             if(dist_to_goal_c * EnvROBARMCfg.GridCellWidth < MAX_EUCL_DIJK_m)
//             {
//                 if ((dist_to_goal_c * EnvROBARMCfg.cost_per_cell) > heur)
//                     heur =  dist_to_goal_c * EnvROBARMCfg.cost_per_cell;
//             }
        }
        else
            //fetch precomputed heuristic
            heur = EnvROBARM.Heur[XYZTO3DIND(FromEndEff[0], FromEndEff[1], FromEndEff[2])];
    }
    else
    {
        heur = edist_to_goal_m * 1000.0 * EnvROBARMCfg.cost_per_mm;
    }

    //get orientation heuristic
    if(EnvROBARMCfg.checkEndEffGoalOrientation)
    {
      if(FromHashEntry->axis_angle > EnvROBARMCfg.EndEffGoals[closest_goal].rpy_tolerance)
        heur_axis_angle = (FromHashEntry->axis_angle - EnvROBARMCfg.EndEffGoals[0].rpy_tolerance) * EnvROBARMCfg.cost_per_rad;
      else
	  heur_axis_angle = FromHashEntry->axis_angle * EnvROBARMCfg.cost_per_rad;

      if(EnvROBARMCfg.sum_heuristics)
      {
	heur += heur_axis_angle;
      }
      else
      {
	//if close enough to goal, use max heuristic
	if(edist_to_goal_m < EnvROBARMCfg.ApplyRPYCost_m)
	{
	  if(heur_axis_angle > heur)
	      heur = heur_axis_angle;
	}
      }
    }
    return heur;
}

//for debugging. remove later
int EnvironmentROBARM3D::GetFromToHeuristic(int FromStateID, int ToStateID, FILE* fOut)
{
    if(EnvROBARMCfg.PlanInJointSpace)
        return GetJointSpaceHeuristic(FromStateID,ToStateID);

#if USE_HEUR==0
    return 0;
#endif

    int heur = 0, closest_goal;
    double FromEndEff_m[3];
    double edist_to_goal_m, heur_axis_angle;

    //NOTE: for plotting only
    double heur_dij = 0.0, heur_eucl, max_heur;

    //get X, Y, Z for the state
    EnvROBARMHashEntry_t* FromHashEntry = EnvROBARM.StateID2CoordTable[FromStateID];
    short unsigned int FromEndEff[3] = {FromHashEntry->endeff[0], FromHashEntry->endeff[1], FromHashEntry->endeff[2]};

    //distance to closest goal in meters
    Cell2ContXYZ(FromHashEntry->endeff[0],FromHashEntry->endeff[1],FromHashEntry->endeff[2],&(FromEndEff_m[0]),&(FromEndEff_m[1]),&(FromEndEff_m[2]));
    edist_to_goal_m = GetDistToClosestGoal(FromEndEff_m, &closest_goal);

    //get distance heuristic
    if(EnvROBARMCfg.dijkstra_heuristic)
    {
        if(EnvROBARMCfg.lowres_collision_checking)
        {
            HighResGrid2LowResGrid(FromHashEntry->endeff, FromEndEff);

            //fetch precomputed heuristic
            heur = EnvROBARM.Heur[XYZTO3DIND(FromEndEff[0], FromEndEff[1], FromEndEff[2])];

            double dist_to_goal_c = GetDistToClosestGoal(FromHashEntry->endeff, &closest_goal);

            //if the distance to the goal is very small, then we need a more informative heuristic when using the lowres collision detection
            if(dist_to_goal_c * EnvROBARMCfg.GridCellWidth < MAX_EUCL_DIJK_m)
            {
                if ((dist_to_goal_c * EnvROBARMCfg.cost_per_cell) > heur)
                    heur =  dist_to_goal_c * EnvROBARMCfg.cost_per_cell;
            }
        }
        else
            //fetch precomputed heuristic
            heur = EnvROBARM.Heur[XYZTO3DIND(FromEndEff[0], FromEndEff[1], FromEndEff[2])];


        //NOTE: for plotting, remove later
        heur_dij = heur;
    }
    else
    {
        heur = edist_to_goal_m * 1000.0 * EnvROBARMCfg.cost_per_mm;
    }

    //NOTE: for plotting, remove later
    heur_eucl = edist_to_goal_m * 1000.0 * EnvROBARMCfg.cost_per_mm;

    //get orientation heuristic
    if(EnvROBARMCfg.checkEndEffGoalOrientation)
    {

      if(FromHashEntry->axis_angle > EnvROBARMCfg.EndEffGoals[0].rpy_tolerance)
          heur_axis_angle = (FromHashEntry->axis_angle - EnvROBARMCfg.EndEffGoals[0].rpy_tolerance) * EnvROBARMCfg.cost_per_rad;
        else
            heur_axis_angle = FromHashEntry->axis_angle * EnvROBARMCfg.cost_per_rad;

        //if close enough to goal, apply cost to gripper orientation
        if(edist_to_goal_m < EnvROBARMCfg.ApplyRPYCost_m)
        {
            //use max(heuristic_xyz, heuristic_rpy)
            if(heur_axis_angle > heur)
            {
                max_heur = heur_axis_angle;
                heur = heur_axis_angle;
            }

            else
            {
                max_heur = heur;
            }
        }
    fprintf(fOut,"%f %f %f %f %d;\n", heur_dij, heur_eucl, heur_axis_angle,heur_axis_angle + heur_dij,heur);
    }

    return heur;
}

int EnvironmentROBARM3D::GetGoalHeuristic(int stateID)
{
#if USE_HEUR==0
    return 0;
#endif

#if DEBUG
    if(stateID >= (int)EnvROBARM.StateID2CoordTable.size())
    {
        printf("ERROR in EnvROBARM... function: stateID illegal\n");
        exit(1);
    }
#endif

    //define this function if it used in the planner (heuristic forward search would use it)
    printf("ERROR in EnvROBARM..function: GetGoalHeuristic is undefined\n");
    exit(1);
}

int EnvironmentROBARM3D::GetStartHeuristic(int stateID)
{
#if USE_HEUR==0
    return 0;
#endif

#if DEBUG
    if(stateID >= (int)EnvROBARM.StateID2CoordTable.size())
    {
        printf("ERROR in EnvROBARM... function: stateID illegal\n");
        exit(1);
    }
#endif

    //define this function if it used in the planner (heuristic backward search would use it)
    printf("ERROR in EnvROBARM... function: GetStartHeuristic is undefined\n");
    exit(1);

    return 0;
}

int EnvironmentROBARM3D::SizeofCreatedEnv()
{
    return EnvROBARM.StateID2CoordTable.size();
	
}

void EnvironmentROBARM3D::PrintState(int stateID, bool bVerbose, FILE* fOut /*=NULL*/)
{
    bool bLocal = false;

#if DEBUG
    if(stateID >= (int)EnvROBARM.StateID2CoordTable.size())
    {
        printf("ERROR in EnvROBARM... function: stateID illegal (2)\n");
        exit(1);
    }
#endif

    if(fOut == NULL)
        fOut = stdout;

    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[stateID];

    bool bGoal = false;
    if(stateID == EnvROBARM.goalHashEntry->stateID)
        bGoal = true;

    if(stateID == EnvROBARM.goalHashEntry->stateID && bVerbose)
    {
        fprintf(fOut, "the state is a goal state\n");
        bGoal = true;
    }

    if(bLocal)
    {
        printangles(fOut, HashEntry->coord, bGoal, bVerbose, true);
    }
    else
    {
        PrintAnglesWithAction(fOut, HashEntry, bGoal, bVerbose, false);
    }
}

//get the goal as a successor of source state at given cost
//if costtogoal = -1 then the succ is chosen
void EnvironmentROBARM3D::PrintSuccGoal(int SourceStateID, int costtogoal, bool bVerbose, bool bLocal /*=false*/, FILE* fOut /*=NULL*/)
{
    printf("[PrintSuccGoal] WARNING: This function has not been updated to deal with multiple goals.\n");

    short unsigned int succcoord[NUMOFLINKS];
    double angles[NUMOFLINKS],orientation[3][3];
    short unsigned int endeff[3],wrist[3],elbow[3];
    int i, inc;

    if(fOut == NULL)
        fOut = stdout;

    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[SourceStateID];

    //default coords of successor
    for(i = 0; i < NUMOFLINKS; i++)
        succcoord[i] = HashEntry->coord[i];	

    //iterate through successors of s
    for (i = 0; i < NUMOFLINKS; i++)
    {
        //increase and decrease in ith angle
        for(inc = -1; inc < 2; inc = inc+2)
        {
            if(inc == -1)
            {
                if(HashEntry->coord[i] == 0)
                    succcoord[i] = EnvROBARMCfg.anglevals[i]-1;
                else
                    succcoord[i] = HashEntry->coord[i] + inc;
            }
            else
            {
                succcoord[i] = (HashEntry->coord[i] + inc)%
                        EnvROBARMCfg.anglevals[i];
            }

            ComputeContAngles(succcoord, angles);
            ComputeEndEffectorPos(angles,endeff,wrist,elbow,orientation);

            //skip invalid successors
            if(!IsValidCoord(succcoord,endeff,wrist,elbow,orientation))
                continue;

            if(endeff[0] == EnvROBARMCfg.EndEffGoals[0].xyz[0] && endeff[1] == EnvROBARMCfg.EndEffGoals[0].xyz[1] && endeff[2] ==  EnvROBARMCfg.EndEffGoals[0].xyz[2])
            {
                if(cost(HashEntry->coord,succcoord) == costtogoal || costtogoal == -1)
                {

                    if(bVerbose)
                        fprintf(fOut, "the state is a goal state\n");
                    printangles(fOut, succcoord, true, bVerbose, bLocal);
                    return;
                }
            }
        }

        //restore it back
        succcoord[i] = HashEntry->coord[i];
    }
}

void EnvironmentROBARM3D::PrintEnv_Config(FILE* fOut)
{
	//implement this if the planner needs to print out EnvROBARM. configuration
	
    printf("ERROR in EnvROBARM... function: PrintEnv_Config is undefined\n");
    exit(1);
}

void EnvironmentROBARM3D::PrintHeader(FILE* fOut)
{
  fprintf(fOut, "PrintHeader() has not yet been defined.\n");
}

/** \brief Get successors when searching towards a cartesian goal. */
void EnvironmentROBARM3D::GetSuccs(int SourceStateID, vector<int>* SuccIDV, vector<int>* CostV)
{
	#if DEBUG_HEURISTIC
			GetFromToHeuristic(SourceStateID,EnvROBARM.goalHashEntry->stateID, fHeurExpansions);
	#endif

	if(EnvROBARMCfg.PlanInJointSpace)
	{
		GetJointSpaceSuccs(SourceStateID, SuccIDV, CostV);
		return;
	}

	if(!EnvROBARMCfg.bGoalIsSet)
		return;

	int i, a, closest_goal;
	unsigned char dist;
	short unsigned int k, succcoord[EnvROBARMCfg.num_joints], wrist[3], elbow[3], endeff[3];
	double axis_angle, orientation [3][3], angles[EnvROBARMCfg.num_joints], s_angles[EnvROBARMCfg.num_joints];

	//to support two sets of succesor actions
	int actions_i_min = 0, actions_i_max = EnvROBARMCfg.nLowResActions;

	//clear the successor array
	SuccIDV->clear();
	CostV->clear();

	//goal state should be absorbing
	if(SourceStateID == EnvROBARM.goalHashEntry->stateID)
	{
		// printf("goal state is absorbing...\n");
		return;
	}

	//get X, Y, Z for the state
	EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[SourceStateID];
	ComputeContAngles(HashEntry->coord, s_angles);

	//default coords of successor
	for(i = 0; i < EnvROBARMCfg.num_joints; i++)
			succcoord[i] = HashEntry->coord[i];

	ComputeContAngles(succcoord, angles);

	//check if cell is close to enough to goal to use higher resolution actions
	if(EnvROBARMCfg.multires_succ_actions)
	{
		if (GetDistToClosestGoal(HashEntry->endeff,&closest_goal) <= EnvROBARMCfg.HighResActionsThreshold_c)
		{
			actions_i_min = EnvROBARMCfg.nLowResActions;
			actions_i_max = EnvROBARMCfg.nSuccActions;
		}
	}

	#if DEBUG
		fprintf(fDeb, "\nstate %d: %.2f %.2f %.2f %.2f %.2f %.2f %.2f   endeff: %d %d %d\n",SourceStateID,
				angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6], HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2]);
	
		fprintf(fSucc, "\nstate %d: %.2f %.2f %.2f %.2f %.2f %.2f %.2f   endeff: %d %d %d\n",SourceStateID,
				angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6], HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2]);
	#endif

	//iterate through successors of s (possible actions)
	for (i = actions_i_min; i < actions_i_max; i++)
	{
		for(a = 0; a < EnvROBARMCfg.num_joints; a++)
		{
			if((HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) < 0)
				succcoord[a] = ((EnvROBARMCfg.anglevals[a] + HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) % EnvROBARMCfg.anglevals[a]);
			else
				succcoord[a] = ((int)(HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) % EnvROBARMCfg.anglevals[a]);
		}

		//get the successor
		EnvROBARMHashEntry_t* OutHashEntry;
		bool bSuccisGoal = false;

		//have to create a new entry
		ComputeContAngles(succcoord, angles);

		//get forward kinematics
		if(!ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation))
		{
		#if DEBUG
				fprintf(fDeb,"fk invalid: %.2f %.2f %.2f %.2f %.2f %.2f %.2f   endeff (%i %i %i) wrist(%i %i %i)\n",
								angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6],endeff[0],endeff[1],endeff[2],wrist[0],wrist[1],wrist[2]);
		#endif
				continue;
		}

		// 	fprintf(fSucc, "%5i: action: %i axis_angle: %.3f  endeff: %3d %3d %3d\n",
		// 		OutHashEntry->stateID, i, axis_angle,OutHashEntry->endeff[0],OutHashEntry->endeff[1],OutHashEntry->endeff[2]);

		//do collision checking
		if(!IsValidCoord(succcoord, endeff, wrist, elbow, orientation, dist))
		{
		#if DEBUG
				fprintf(fDeb,"cc invalid: %.2f %.2f %.2f %.2f %.2f %.2f %.2f   endeff (%i %i %i) wrist(%i %i %i)\n",
								angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6],endeff[0],endeff[1],endeff[2],wrist[0],wrist[1],wrist[2]);
		#endif
				continue;
		}

		GetAxisAngle(orientation, EnvROBARM.goalHashEntry->orientation, &axis_angle);

		for(k = 0; k < EnvROBARMCfg.EndEffGoals.size(); k++)
		{
			if(isGoalPosition(endeff, orientation, EnvROBARMCfg.EndEffGoals[k], axis_angle))
			{
				bSuccisGoal = true;
				// printf("goal succ is generated\n");
				for (int j = 0; j < NUMOFLINKS; j++)
				{
					EnvROBARMCfg.goalcoords[j] = succcoord[j];
					EnvROBARM.goalHashEntry->coord[j] = succcoord[j];
				}
				EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals[k].xyz[0];
				EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals[k].xyz[1];
				EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals[k].xyz[2];
				EnvROBARM.goalHashEntry->action = i;
				EnvROBARM.goalHashEntry->axis_angle = axis_angle;
			}
		}

		//check if hash entry already exists, if not then create one
		if((OutHashEntry = GetHashEntry(succcoord, EnvROBARMCfg.num_joints, i, bSuccisGoal)) == NULL)
		{
				OutHashEntry = CreateNewHashEntry(succcoord, EnvROBARMCfg.num_joints, endeff, i, orientation);
				OutHashEntry->axis_angle = axis_angle;
				OutHashEntry->dist = dist;
				
				fprintf(fSucc, "%5i: action: %i dist: %u cost: %d endeff: %3d %3d %3d\n",
								OutHashEntry->stateID, i,OutHashEntry->dist, cost(HashEntry,OutHashEntry,bSuccisGoal),OutHashEntry->endeff[0],OutHashEntry->endeff[1],OutHashEntry->endeff[2]);
		}

		//put successor on successor list with the proper cost
		SuccIDV->push_back(OutHashEntry->stateID);
		CostV->push_back(cost(HashEntry,OutHashEntry,bSuccisGoal) + EnvROBARMCfg.ActiontoActionCosts[HashEntry->action][OutHashEntry->action]);

		#if DEBUG
			fprintf(fSucc, "%5i: action: %i axis_angle: %.3f  endeff: %3d %3d %3d\n",
					OutHashEntry->stateID, i, axis_angle,
					OutHashEntry->endeff[0],OutHashEntry->endeff[1],OutHashEntry->endeff[2]);
		#endif
	}
	
	if(save_expanded_states)
		expanded_states.push_back(SourceStateID);
}

/** \brief Get successors when searching towards a joint space goal. */
void EnvironmentROBARM3D::GetJointSpaceSuccs(int SourceStateID, vector<int>* SuccIDV, vector<int>* CostV)
{
  short unsigned int succcoord[NUMOFLINKS];
  short unsigned int k, wrist[3], elbow[3], endeff[3];
  double orientation [3][3];
  int i, a;
  double angles[NUMOFLINKS], s_angles[NUMOFLINKS];

  //to support two sets of succesor actions
  int actions_i_min = 0, actions_i_max = EnvROBARMCfg.nLowResActions;

  //clear the successor array
  SuccIDV->clear();
  CostV->clear();

  //check if there is an actual goal to plan to (should not have to be done here...think of better place)
  if(EnvROBARMCfg.JointSpaceGoals.size() == 0 || !EnvROBARMCfg.bGoalIsSet)
    return;

  //goal state should be absorbing
  if(SourceStateID == EnvROBARM.goalHashEntry->stateID)
  {
    printf("goal state is absorbing...\n");
    return;
  }

  //get X, Y, Z for the state
  EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[SourceStateID];
  ComputeContAngles(HashEntry->coord, s_angles);

  //default coords of successor
  for(i = 0; i < EnvROBARMCfg.num_joints; i++)
    succcoord[i] = HashEntry->coord[i];

  ComputeContAngles(succcoord, angles);

  //check if cell is close to enough to goal to use higher resolution actions
  if(EnvROBARMCfg.multires_succ_actions)
  {
    if (getDistanceToGoal(s_angles) <= EnvROBARMCfg.HighResActionsThreshold_r)
    {
      actions_i_min = EnvROBARMCfg.nLowResActions;
      actions_i_max = EnvROBARMCfg.nSuccActions;
      // printf("%.3f radians away from goal.\n", getDistanceToGoal(s_angles));
    }
  }

#if DEBUG_JOINTSPACE_PLANNING
  printf("\n%i:\n",SourceStateID);
#endif

  //iterate through successors of s (possible actions)
  for (i = actions_i_min; i < actions_i_max; i++)
  {
    //don't use motion primitives that only change joints that are already within the goal tolerance
    if(EnvROBARMCfg.use_selective_actions)
      if(EnvROBARMCfg.uselessActions[i])
	continue;

    for(a = 0; a < EnvROBARMCfg.num_joints; a++)
    {
      if((HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) < 0)
	succcoord[a] = ((EnvROBARMCfg.anglevals[a] + HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) % EnvROBARMCfg.anglevals[a]);
      else
	succcoord[a] = ((int)(HashEntry->coord[a] + int(EnvROBARMCfg.SuccActions[i][a])) % EnvROBARMCfg.anglevals[a]);
    }

    //get the successor
    EnvROBARMHashEntry_t* OutHashEntry;
    bool bSuccisGoal = false;

    //have to create a new entry
    ComputeContAngles(succcoord, angles);

    //get forward kinematics
    if(!ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation))
    {
      // printf("[GetJointSpaceSuccs] FK for endeff (%u %u %u) successor is not valid.\n",endeff[0],endeff[1],endeff[2]);
      continue;
    }

    //do collision checking
    if(!IsValidCoord(succcoord, endeff, wrist, elbow, orientation))
    {
      // printf("[GetJointSpaceSuccs] endeff (%u %u %u) successor is not valid.\n",endeff[0],endeff[1],endeff[2]);
      continue;
    }

    //check if at goal configuration
    for(k = 0; k < EnvROBARMCfg.JointSpaceGoals.size(); k++)
    {
      if(isGoalState(angles, EnvROBARMCfg.JointSpaceGoals[k]))
      {
	bSuccisGoal = true;

	// printf("goal succ is generated\n");
	for (a = 0; a < EnvROBARMCfg.num_joints; a++)
	{
	  EnvROBARMCfg.goalcoords[a] = succcoord[a];
	  EnvROBARM.goalHashEntry->coord[a] = succcoord[a];
	}

	EnvROBARM.goalHashEntry->endeff[0] = endeff[0];
	EnvROBARM.goalHashEntry->endeff[1] = endeff[1];
	EnvROBARM.goalHashEntry->endeff[2] = endeff[2];
	EnvROBARM.goalHashEntry->action = i;
      }
    }

    //check if hash entry already exists, if not then create one
    if((OutHashEntry = GetHashEntry(succcoord, EnvROBARMCfg.num_joints, i, bSuccisGoal)) == NULL)
    {
      OutHashEntry = CreateNewHashEntry(succcoord, EnvROBARMCfg.num_joints, endeff, i, orientation);
    }

    //put successor on successor list with cost
    SuccIDV->push_back(OutHashEntry->stateID);

    CostV->push_back(cost(HashEntry,OutHashEntry,bSuccisGoal) +
	EnvROBARMCfg.ActiontoActionCosts[HashEntry->action][OutHashEntry->action]);
  }
}

int EnvironmentROBARM3D::GetJointSpaceHeuristic(int FromStateID, int ToStateID)
{
  //ToStateID is not used because this function computes the heuristic to the closest goal

#if USE_HEUR==0
  return 0;
#endif

  int a, h = 0, closest_goal = 0;
  double FromAngles[NUMOFLINKS], ang_diff = 0, sum = 0, min_dist = 100000000;

  EnvROBARMHashEntry_t* FromHashEntry = EnvROBARM.StateID2CoordTable[FromStateID];
  ComputeContAngles(FromHashEntry->coord, FromAngles);

  //find euclidean distance to closest goal
  for(unsigned int k = 0; k < EnvROBARMCfg.JointSpaceGoals.size(); k++)
  {
    sum = 0;
    for(a = 0; a < EnvROBARMCfg.num_joints; a++)
    {
      if(!EnvROBARMCfg.joints[a].cont)
      {
				if(!angles::shortest_angular_distance_with_limits(FromAngles[a],  EnvROBARMCfg.JointSpaceGoals[k].pos[a], EnvROBARMCfg.joints[a].negative_limit, EnvROBARMCfg.joints[a].positive_limit, ang_diff))
				{
					printf("[GetJointSpaceHeuristic] %i: from %f -> to %f does not lie within the bounds [%f %f].\n",
									a, FromAngles[a], EnvROBARMCfg.JointSpaceGoals[k].pos[a], EnvROBARMCfg.joints[a].negative_limit, EnvROBARMCfg.joints[a].positive_limit);
					ang_diff = 0; // 2.0*M_PI;
				}
				else
	       ang_diff = angles::shortest_angular_distance(FromAngles[a],  EnvROBARMCfg.JointSpaceGoals[k].pos[a]);
      }

      sum += (ang_diff*ang_diff);
    }

    if(sqrt(sum) < min_dist)
    {
      min_dist = sqrt(sum);
      closest_goal = k;
    }
  }

  //heuristic = angular euclidean distance (radians)  x  Cost per radian
  h = min_dist * EnvROBARMCfg.cost_per_rad_js;

#if DEBUG_JOINTSPACE_PLANNING
  printf("%i:  %.3f %.3f %.3f %.3f %.3f %.3f %.3f  -->  %.3f %.3f %.3f %.3f %.3f %.3f %.3f  ssd(rad): %.3f  h: %i\n",
	 FromStateID,FromAngles[0],FromAngles[1],FromAngles[2],FromAngles[3],FromAngles[4],FromAngles[5],FromAngles[6],
  EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[0],EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[1],
  EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[2],EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[3],
  EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[4],EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[5],
  EnvROBARMCfg.JointSpaceGoals[closest_goal].pos[6],min_dist,h);
#endif
  return h;
}

void EnvironmentROBARM3D::GetPreds(int TargetStateID, vector<int>* PredIDV, vector<int>* CostV)
{

    printf("ERROR in EnvROBARM... function: GetPreds is undefined\n");
    exit(1);
}

/** \brief check if an end effector position and orientation is within the goal tolerance */
bool EnvironmentROBARM3D::isGoalPosition(const short unsigned int endeff[3], const double orientation[3][3], const GoalPos &goal, const double &axis_angle)
{
  bool bIsGoal = false;

  // x,y,z of position is considered
  if((goal.type & 0x00000007) == 7)
  {
    if(sqrt((endeff[0] - goal.xyz[0]) * (endeff[0] - goal.xyz[0]) + (endeff[1] - goal.xyz[1]) * (endeff[1] - goal.xyz[1]) + (endeff[2] - goal.xyz[2]) * (endeff[2] - goal.xyz[2])) <= goal.xyz_tolerance)
      bIsGoal = true;
	}
  // only x,y of position is considered
  else if((goal.type & 0x00000007) == 3)
  {
    if(sqrt((endeff[0] - goal.xyz[0]) * (endeff[0] - goal.xyz[0]) + (endeff[1] - goal.xyz[1]) * (endeff[1] - goal.xyz[1])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }
    // only x,z of position is considered
  else if((goal.type & 0x00000007) == 5)
  {
    if(sqrt((endeff[0] - goal.xyz[0]) * (endeff[0] - goal.xyz[0]) + (endeff[2] - goal.xyz[2]) * (endeff[2] - goal.xyz[2])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }
    // only y,z of position is considered
  else if((goal.type & 0x00000007) == 6)
  {
    if(sqrt((endeff[1] - goal.xyz[1]) * (endeff[1] - goal.xyz[1]) + (endeff[2] - goal.xyz[2]) * (endeff[2] - goal.xyz[2])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }
  // only x of position is considered
  else if((goal.type & 0x00000007) == 1)
  {
    if(sqrt((endeff[0] - goal.xyz[0]) * (endeff[0] - goal.xyz[0])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }
  // only y of position is considered
  else if((goal.type & 0x00000007) == 2)
  {
    if(sqrt((endeff[1] - goal.xyz[1]) * (endeff[1] - goal.xyz[1])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }
  // only z of position is considered
  else if((goal.type & 0x00000007) == 4)
  {
    if(sqrt((endeff[2] - goal.xyz[2]) * (endeff[2] - goal.xyz[2])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }
  else
  {
    printf("Invalid goal type. (type = %d). Assuming x,y,z of position should be constrained.\n",goal.type);
    if(sqrt((endeff[0] - goal.xyz[0]) * (endeff[0] - goal.xyz[0]) + (endeff[1] - goal.xyz[1]) * (endeff[1] - goal.xyz[1]) + (endeff[2] - goal.xyz[2]) * (endeff[2] - goal.xyz[2])) <= goal.xyz_tolerance)
      bIsGoal = true;
  }

 // check orientation
 if(bIsGoal)
 {
    //log the amount of time required for the search to get close to the goal
    if(!near_goal)
    {
      time_to_goal_region = (clock() - starttime) / (double)CLOCKS_PER_SEC;
      near_goal = true;
      printf("Search is within %d cells of the goal after %.4f sec.\n", goal.xyz_tolerance, time_to_goal_region);
    }
    
		// roll, pitch, yaw of orientation is considered
		if((goal.type & 0xFFFFFFF8) == 56)
    {
      if(fabs(axis_angle) <= goal.rpy_tolerance)
      {
        return true;
      }
		}
		
    // orientation does not matter
    else if((goal.type & 0xFFFFFFF8) == 0)
    {
      return true;
    }
    else
		{
			printf("Invalid goal orientation type. (type = %hx).\n", goal.type);
			printf("goal.type & 0xFFFFFFF0 = %hx\n",goal.type & 0xFFFFFFF8);
      return false;
		}
  }
	
	return false;
}

/** \brief check if a joint configuration is within the goal tolerance */
bool EnvironmentROBARM3D::isGoalState(const double angles[], const GoalConfig &goal)
{
  for(unsigned int a = 0; a < NUMOFLINKS; a++)
  {
    if(fabs(angles::shortest_angular_distance(angles[a], goal.pos[a])) > goal.tolerance_above[a])
      return false;
  }

  return true;
}

void EnvironmentROBARM3D::SetAllActionsandAllOutcomes(CMDPSTATE* state)
{
    printf("ERROR in EnvROBARM..function: SetAllActionsandOutcomes is undefined\n");
    exit(1);
}

void EnvironmentROBARM3D::SetAllPreds(CMDPSTATE* state)
{
    //implement this if the planner needs access to predecessors
    printf("ERROR in EnvROBARM... function: SetAllPreds is undefined\n");
    exit(1);
}

int EnvironmentROBARM3D::GetEdgeCost(int FromStateID, int ToStateID)
{

#if DEBUG
    if(FromStateID >= (int)EnvROBARM.StateID2CoordTable.size() 
       || ToStateID >= (int)EnvROBARM.StateID2CoordTable.size())
    {
        printf("ERROR in EnvROBARM... function: stateID illegal\n");
        exit(1);
    }
#endif

    //get X, Y for the state
    EnvROBARMHashEntry_t* FromHashEntry = EnvROBARM.StateID2CoordTable[FromStateID];
    EnvROBARMHashEntry_t* ToHashEntry = EnvROBARM.StateID2CoordTable[ToStateID];

    return cost(FromHashEntry, ToHashEntry, false);
}

bool EnvironmentROBARM3D::AreEquivalent(int State1ID, int State2ID)
{
    EnvROBARMHashEntry_t* HashEntry1 = EnvROBARM.StateID2CoordTable[State1ID];
    EnvROBARMHashEntry_t* HashEntry2 = EnvROBARM.StateID2CoordTable[State2ID];

    for(short unsigned int i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
        if(HashEntry1->coord[i] != HashEntry2->coord[i])
            return false;
    }
    if(HashEntry1->action != HashEntry2->action)
        return false;

    return true;
}

/** \brief set the initial manipulator joints motor positions */
bool EnvironmentROBARM3D::SetStartJointConfig(const double angles[], bool bRad)
{
    double startangles[NUMOFLINKS];
    short unsigned int elbow[3] = {0}, wrist[3] = {0};

    //set initial joint configuration
    if(bRad) //input is in radians
    {
        for(unsigned int i = 0; i < EnvROBARMCfg.num_joints; i++)
        {
            if(angles[i] < 0)
                EnvROBARMCfg.LinkStartAngles_d[i] = RAD2DEG(angles[i] + PI_CONST*2.0);
            else
                EnvROBARMCfg.LinkStartAngles_d[i] = RAD2DEG(angles[i]);
        }
	// compute arm position in environment
	ComputeCoord(angles, EnvROBARM.startHashEntry->coord);

	//get joint positions of starting configuration
	if(!ComputeEndEffectorPos(angles, EnvROBARM.startHashEntry->endeff, wrist, elbow, EnvROBARM.startHashEntry->orientation))
	  printf("[ComputeEndEffectorPos] Start position is invalid. Attempting to plan anyway.\n");
    }
    else //input is in degrees
    {
        for(unsigned int i = 0; i < EnvROBARMCfg.num_joints; i++)
        {
            if(angles[i] < 0)
                EnvROBARMCfg.LinkStartAngles_d[i] =  angles[i] + 360.0;
            else
                EnvROBARMCfg.LinkStartAngles_d[i] =  angles[i];
        }
	// initialize the angles of the start states
	for(int i = 0; i < EnvROBARMCfg.num_joints; i++)
	  startangles[i] = DEG2RAD(EnvROBARMCfg.LinkStartAngles_d[i]);

	// compute arm position in environment
	ComputeCoord(startangles, EnvROBARM.startHashEntry->coord);
	
	//get joint positions of starting configuration
	if(!ComputeEndEffectorPos(startangles, EnvROBARM.startHashEntry->endeff, wrist, elbow, EnvROBARM.startHashEntry->orientation))
	  printf("[ComputeEndEffectorPos] Start position is invalid.\n");
    }

    return true;
}

/** \brief set goal position(s)  - goal is described in meters, radians --> {x,y,z,r,p,y} */
bool EnvironmentROBARM3D::SetGoalPosition(const std::vector <std::vector<double> > &goals, const std::vector<std::vector<double> > &tolerances, const std::vector<int> &type)
{
  if(!EnvROBARMCfg.bInitialized)
  {
    printf("Cannot set goal position because environment is not initialized.\n");
    return false;
  }

  EnvROBARMCfg.PlanInJointSpace = false;

	EnvROBARMCfg.mCopyingGrid.lock();
	UpdateEnvironment();
	EnvROBARMCfg.mCopyingGrid.unlock();

  if(EnvROBARMCfg.lowres_collision_checking)
  {
    EnvROBARMCfg.upper_arm_radius = EnvROBARMCfg.link_radius[0] / EnvROBARMCfg.LowResGridCellWidth + 0.5;
    EnvROBARMCfg.forearm_radius = EnvROBARMCfg.link_radius[1] / EnvROBARMCfg.LowResGridCellWidth + 0.5;
    EnvROBARMCfg.gripper_radius  = EnvROBARMCfg.link_radius[2] / EnvROBARMCfg.LowResGridCellWidth + 0.5;
  }
  else
  {
    EnvROBARMCfg.upper_arm_radius = EnvROBARMCfg.link_radius[0] / EnvROBARMCfg.GridCellWidth + 0.5;
    EnvROBARMCfg.forearm_radius = EnvROBARMCfg.link_radius[1] / EnvROBARMCfg.GridCellWidth + 0.5;
    EnvROBARMCfg.gripper_radius  = EnvROBARMCfg.link_radius[2] / EnvROBARMCfg.GridCellWidth + 0.5;
  }

  //resize goal list
  EnvROBARMCfg.EndEffGoals.resize(goals.size());

  for(unsigned int i = 0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
  {
    EnvROBARMCfg.EndEffGoals[i].pos[0] = goals[i][0];
    EnvROBARMCfg.EndEffGoals[i].pos[1] = goals[i][1];
    EnvROBARMCfg.EndEffGoals[i].pos[2] = goals[i][2];

    EnvROBARMCfg.EndEffGoals[i].rpy[0] = goals[i][3];
    EnvROBARMCfg.EndEffGoals[i].rpy[1] = goals[i][4];
    EnvROBARMCfg.EndEffGoals[i].rpy[2] = goals[i][5];

    //convert goal position from meters to cells
    ContXYZ2Cell(EnvROBARMCfg.EndEffGoals[i].pos[0],EnvROBARMCfg.EndEffGoals[i].pos[1],EnvROBARMCfg.EndEffGoals[i].pos[2],
                 &(EnvROBARMCfg.EndEffGoals[i].xyz[0]), &(EnvROBARMCfg.EndEffGoals[i].xyz[1]), &(EnvROBARMCfg.EndEffGoals[i].xyz[2]));

    EnvROBARMCfg.EndEffGoals[i].xyz_lr[0] = EnvROBARMCfg.EndEffGoals[i].xyz[0] * (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
    EnvROBARMCfg.EndEffGoals[i].xyz_lr[1] = EnvROBARMCfg.EndEffGoals[i].xyz[1] * (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);
    EnvROBARMCfg.EndEffGoals[i].xyz_lr[2] = EnvROBARMCfg.EndEffGoals[i].xyz[2] * (EnvROBARMCfg.GridCellWidth / EnvROBARMCfg.LowResGridCellWidth);

    // check if goals are within arms length
    if(fabs(EnvROBARMCfg.EndEffGoals[i].pos[0] - EnvROBARMCfg.BaseX_m) >= EnvROBARMCfg.arm_length ||
       fabs(EnvROBARMCfg.EndEffGoals[i].pos[1] - EnvROBARMCfg.BaseY_m) >= EnvROBARMCfg.arm_length ||
       fabs(EnvROBARMCfg.EndEffGoals[i].pos[2] - EnvROBARMCfg.BaseZ_m) >= EnvROBARMCfg.arm_length)
    {
      printf("End effector goal position (%.2f %.2f %.2f) is out of bounds.\n",EnvROBARMCfg.EndEffGoals[i].pos[0],EnvROBARMCfg.EndEffGoals[i].pos[1],EnvROBARMCfg.EndEffGoals[i].pos[2]);

      EnvROBARMCfg.EndEffGoals.erase(EnvROBARMCfg.EndEffGoals.begin()+i);

      if(EnvROBARMCfg.EndEffGoals.size() <= 1)
      {
        EnvROBARMCfg.bGoalIsSet  = false;
        return false;
      }
      else
      {
        printf("[SetGoalPosition] Removed goal %i. %i goal(s) remaining.\n", i, EnvROBARMCfg.EndEffGoals.size());
        continue;
      }

      i--;  //temporary hack
    }

//     if(EnvROBARMCfg.Grid3D[EnvROBARMCfg.EndEffGoals[i].xyz[0]][EnvROBARMCfg.EndEffGoals[i].xyz[1]][EnvROBARMCfg.EndEffGoals[i].xyz[2]] >= EnvROBARMCfg.ObstacleCost)
    if(!isValidCell((EnvROBARMCfg.EndEffGoals[i].xyz[0]), (EnvROBARMCfg.EndEffGoals[i].xyz[1]), (EnvROBARMCfg.EndEffGoals[i].xyz[2]), 8, EnvROBARMCfg.Grid3D, voxel_grid))
    {
			int xyz[3] = {(int)(EnvROBARMCfg.EndEffGoals[i].xyz[0]), (int)(EnvROBARMCfg.EndEffGoals[i].xyz[1]), (int)(EnvROBARMCfg.EndEffGoals[i].xyz[2])};
			printf("End effector goal position (%.2f %.2f %.2f) is on an invalid cell (dist = %u).\n",EnvROBARMCfg.EndEffGoals[i].pos[0],EnvROBARMCfg.EndEffGoals[i].pos[1],EnvROBARMCfg.EndEffGoals[i].pos[2],
						 getCell(xyz, EnvROBARMCfg.Grid3D));

      EnvROBARMCfg.EndEffGoals.erase(EnvROBARMCfg.EndEffGoals.begin()+i);

      if(EnvROBARMCfg.EndEffGoals.size() <= 1)
      {
        EnvROBARMCfg.bGoalIsSet  = false;
        return false;
      }
      else
      {
        printf("[SetGoalPosition] Removed goal %i. %i goal(s) remaining.\n", i, EnvROBARMCfg.EndEffGoals.size());
      }

      i--;  //temporary hack
    }
  }

  //set goal tolerance
  SetGoalPositionTolerance(tolerances);

  //set goal type
  SetGoalPositionType(type);

  //compute cost per cell
  if(EnvROBARMCfg.cost_per_cell == -1)
    ComputeCostPerCell();

  // pre-compute heuristics with new goal
  if(EnvROBARMCfg.dijkstra_heuristic)
    ComputeHeuristicValues();

  // set goal hash entry
  EnvROBARM.goalHashEntry->endeff[0] = EnvROBARMCfg.EndEffGoals[0].xyz[0];
  EnvROBARM.goalHashEntry->endeff[1] = EnvROBARMCfg.EndEffGoals[0].xyz[1];
  EnvROBARM.goalHashEntry->endeff[2] = EnvROBARMCfg.EndEffGoals[0].xyz[2];
  EnvROBARM.goalHashEntry->action = 0;
  RPY2Rot(EnvROBARMCfg.EndEffGoals[0].rpy[0],EnvROBARMCfg.EndEffGoals[0].rpy[1],EnvROBARMCfg.EndEffGoals[0].rpy[2],EnvROBARM.goalHashEntry->orientation);
  for(unsigned int i=0; i<EnvROBARMCfg.num_joints; i++)
    EnvROBARM.goalHashEntry->coord[i] = 0;

  EnvROBARMCfg.bGoalIsSet = true;

	expanded_states.clear();
// 	expanded_states.reserve(5000);

//   for(unsigned int i = 0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
//     printf("goal %i:  grid: %u %u %u (cells)  xyz:%.2f %.2f %.2f (meters)  (tol: %.3f) rpy: %1.2f %1.2f %1.2f (radians) (tol: %.3f) type: %d \n", i ,
//            EnvROBARMCfg.EndEffGoals[i].xyz[0], EnvROBARMCfg.EndEffGoals[i].xyz[1],EnvROBARMCfg.EndEffGoals[i].xyz[2],
// 	   EnvROBARMCfg.EndEffGoals[i].pos[0],EnvROBARMCfg.EndEffGoals[i].pos[1],EnvROBARMCfg.EndEffGoals[i].pos[2],tolerances[i][0],
//             EnvROBARMCfg.EndEffGoals[i].rpy[0],EnvROBARMCfg.EndEffGoals[i].rpy[1],EnvROBARMCfg.EndEffGoals[i].rpy[2], tolerances[i][1], type[i]);

//   printf("\n\nVOXEL GRID:\n");
//   int z = 37;
//   for(int x=0; x < EnvROBARMCfg.LowResEnvHeight_c; x++)
//   {
//     for(int y = 0; y < EnvROBARMCfg.LowResEnvDepth_c; y++)
//     {
//       printf("%u ",getVoxel(x,y,z,lowres_voxel_grid));
//     }
//     printf("\n");
//   }
//   
//   printf("\n\nHEURISTIC: \n");
//   z = 37;
//   for(int x=0; x < EnvROBARMCfg.EnvHeight_c; x++)
//   {
//     for(int y = 0; y < EnvROBARMCfg.EnvDepth_c; y++)
//     {
//       printf("%i ",EnvROBARM.Heur[XYZTO3DIND(x,y,z)]/COSTMULT);
//     }
//     printf("\n");
//   }
  return true;
}

/** \brief set goal position tolerance(s) - tolerance is described in meters, radians */
void EnvironmentROBARM3D::SetGoalPositionTolerance(const std::vector <std::vector<double> > &tolerance)
{
  for(unsigned int i = 0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
  {
    if(tolerance.size() >= i)
    {
        EnvROBARMCfg.EndEffGoals[i].xyz_tolerance = tolerance[i][0] / EnvROBARMCfg.GridCellWidth;
        EnvROBARMCfg.EndEffGoals[i].rpy_tolerance = tolerance[i][1];
    }
    else
    {
      EnvROBARMCfg.EndEffGoals[i].xyz_tolerance = tolerance[0][0] / EnvROBARMCfg.GridCellWidth;
      EnvROBARMCfg.EndEffGoals[i].rpy_tolerance = tolerance[0][1];
    }
  }
}

/** \brief set goal position type(s) - type can be described ~/ros/ros-pkg/motion_planning/motion_planning_msgs/msg/PoseConstraint.msg */
void EnvironmentROBARM3D::SetGoalPositionType(const std::vector <int> &goal_type)
{
  for(unsigned int i = 0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
  {
    if(goal_type.size() > 0)
    {
      if(goal_type.size() >= i)
        EnvROBARMCfg.EndEffGoals[i].type = goal_type[i];
      else
        EnvROBARMCfg.EndEffGoals[i].type = goal_type[0];
    }
    else
      printf("goal type vector is empty. TODO: enter defaults.\n");
  }
}

/** \brief set goal configuration(s) - goal is in radians --> {j1, j2, j3, ...} */
bool EnvironmentROBARM3D::SetGoalConfiguration(const std::vector <std::vector<double> > &goals, const std::vector<std::vector<double> > &tolerance_above, const std::vector<std::vector<double> > &tolerance_below)
{
  if(!EnvROBARMCfg.bInitialized)
  {
    printf("Cannot set goal position because environment is not initialized.\n");
    return false;
  }

  if(goals.size() <= 0)
  {
    printf("[SetGoalConfiguration] Goal vector is empty. No goals set.\n");
    return false;
  }

  double angles[NUMOFLINKS], orientation[3][3];
  short unsigned  int i, k, coord[NUMOFLINKS], elbow[3] = {0}, wrist[3] = {0}, endeff[3] = {0};
  EnvROBARMCfg.PlanInJointSpace = true;

	EnvROBARMCfg.mCopyingGrid.lock();
	UpdateEnvironment();
	EnvROBARMCfg.mCopyingGrid.unlock();

  //resize goal list
  EnvROBARMCfg.JointSpaceGoals.resize(goals.size());

  for(i = 0; i < EnvROBARMCfg.JointSpaceGoals.size(); i++)
  {
    EnvROBARMCfg.JointSpaceGoals[i].pos.resize(goals[i].size());
    EnvROBARMCfg.JointSpaceGoals[i].pos = goals[i];

    for(k = 0; k < EnvROBARMCfg.num_joints; k++)
    {
      EnvROBARMCfg.JointSpaceGoals[i].pos[k] = goals[i][k];
      angles[k] = EnvROBARMCfg.JointSpaceGoals[i].pos[k];

      if(EnvROBARMCfg.JointSpaceGoals[i].pos[k] < 0)
        angles[k] = 2*PI_CONST + EnvROBARMCfg.JointSpaceGoals[i].pos[k];
    }

    //get joint positions of starting configuration
    if(!ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation))
    {
      printf("[SetGoalConfiguration] goal: %.2f %.2f %.2f %.2f %.2f %.2f %.2f is out of bounds.\n",
	    angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6]);
      printf("[SetGoalConfiguration] goal: endeff(%u %u %u) wrist(%u %u %u) elbow(%u %u %u)\n",
	    endeff[0],endeff[1],endeff[2],wrist[0],wrist[1],wrist[2],elbow[0],elbow[1],elbow[2]);

      EnvROBARMCfg.JointSpaceGoals.erase(EnvROBARMCfg.JointSpaceGoals.begin()+i);
      if(EnvROBARMCfg.JointSpaceGoals.empty())
      {
				EnvROBARMCfg.bGoalIsSet  = false;
				return false;
      }
      else
      {
				printf("[SetGoalConfiguration] Removed goal %i. %i goal(s) remaining.\n", i, EnvROBARMCfg.JointSpaceGoals.size());
				continue;
      }
    }

		// store end effector position at goal (for collision checking)
		EnvROBARMCfg.JointSpaceGoals[i].xyz[0] = endeff[0];
		EnvROBARMCfg.JointSpaceGoals[i].xyz[1] = endeff[1];
		EnvROBARMCfg.JointSpaceGoals[i].xyz[2] = endeff[2];

    //get coords
    if(EnvROBARMCfg.JointSpaceGoals[i].pos[k] < 0)
      angles[k] = 2*PI_CONST + EnvROBARMCfg.JointSpaceGoals[i].pos[k];
    ComputeCoord(angles, coord);

    //check if goal position is valid
    if(!IsValidCoord(coord,endeff,wrist,elbow,orientation))
    {
      printf("[SetGoalConfiguration] goal angles: %.2f %.2f %.2f %.2f %.2f %.2f %.2f is invalid.\n",
	    angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6]);
      printf("[SetGoalConfiguration] goal: endeff(%u %u %u) wrist(%u %u %u) elbow(%u %u %u)\n",
	    endeff[0],endeff[1],endeff[2],wrist[0],wrist[1],wrist[2],elbow[0],elbow[1],elbow[2]);

      EnvROBARMCfg.JointSpaceGoals.erase(EnvROBARMCfg.JointSpaceGoals.begin()+i);
      if(EnvROBARMCfg.JointSpaceGoals.empty())
      {
				EnvROBARMCfg.bGoalIsSet  = false;
				return false;
      }
      else
      {
				printf("[SetGoalConfiguration] Removed goal %i. %i goal(s) remaining.\n", i, EnvROBARMCfg.JointSpaceGoals.size());
				continue;
      }

      i--;
    }

    //set goalHashEntry info just for kicks
    if(i == 0)
    {
      EnvROBARM.goalHashEntry->endeff[0] = endeff[0];
      EnvROBARM.goalHashEntry->endeff[1] = endeff[1];
      EnvROBARM.goalHashEntry->endeff[2] = endeff[2];

      for(k=0; k < EnvROBARMCfg.num_joints; k++)
      {
      	EnvROBARM.goalHashEntry->coord[k] = coord[k];
      }
    }
    //set goal tolerances
    SetGoalConfigurationTolerance(tolerance_above, tolerance_below);

    #if DEBUG
      printf("[SetGoalConfiguration] goal: %.2f %.2f %.2f %.2f  %.2f %.2f %.2f tolerance: %.3f %.3f \n",
	     EnvROBARMCfg.JointSpaceGoals[0].pos[0],EnvROBARMCfg.JointSpaceGoals[0].pos[1],EnvROBARMCfg.JointSpaceGoals[0].pos[2],
              EnvROBARMCfg.JointSpaceGoals[0].pos[3],EnvROBARMCfg.JointSpaceGoals[0].pos[4],EnvROBARMCfg.JointSpaceGoals[0].pos[5],EnvROBARMCfg.JointSpaceGoals[0].pos[6],
              EnvROBARMCfg.JointSpaceGoals[0].tolerance_above[0],EnvROBARMCfg.JointSpaceGoals[0].tolerance_below[0]);
    #endif
  }

  //if one of the joints starts out within the goal tolerance then remove any successors that change that joint angle
  if(EnvROBARMCfg.use_selective_actions)
  {
    ComputeContAngles(EnvROBARM.startHashEntry->coord, angles);
    EnvROBARMCfg.uselessActions.resize(EnvROBARMCfg.nSuccActions, false);
    double diff;
    for(i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
      angles::shortest_angular_distance_with_limits(angles[i], EnvROBARMCfg.JointSpaceGoals[0].pos[i], EnvROBARMCfg.joints[i].negative_limit, EnvROBARMCfg.joints[i].positive_limit, diff);
      printf("%i: diff: %.4f tolerances: %.4f, %.4f\n",i, diff,EnvROBARMCfg.JointSpaceGoals[0].tolerance_below[i],EnvROBARMCfg.JointSpaceGoals[0].tolerance_above[i]);
      if(fabs(diff) <= EnvROBARMCfg.JointSpaceGoals[0].tolerance_below[i] && fabs(diff) <= EnvROBARMCfg.JointSpaceGoals[0].tolerance_above[i])
      {
	printf("joint %i will not be planned for.\n",i);
	for(k = 0; k < EnvROBARMCfg.nSuccActions; k++)
	{
	  if(EnvROBARMCfg.SuccActions[k][i] != 0)
	  {
	    EnvROBARMCfg.uselessActions[k] = true;
	    printf("action #%i will not be used.\n",k);
	  }
	}
      }
    }
  }

  EnvROBARMCfg.bGoalIsSet = true;

  return true;
}

/** \brief set goal configuration tolerance(s) - tolerance is in radians */
void EnvironmentROBARM3D::SetGoalConfigurationTolerance(const std::vector<std::vector<double> > &tolerance_above, const std::vector<std::vector<double> > &tolerance_below)
{
  for(unsigned int i = 0; i < EnvROBARMCfg.JointSpaceGoals.size(); i++)
  {
    if(tolerance_above.size() >= i)
    {
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_above.resize(tolerance_above[i].size());
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_above = tolerance_above[i];
    }
    else
    {
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_above.resize(tolerance_above[0].size());
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_above = tolerance_above[0];
    }

    if(tolerance_below.size() >= i)
    {
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_below.resize(tolerance_below[i].size());
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_below = tolerance_below[i];
    }
    else
    {
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_below.resize(tolerance_below[0].size());
      EnvROBARMCfg.JointSpaceGoals[i].tolerance_below = tolerance_below[0];
    }
  }
}

/** \brief convert StateIDs to angles in radians */
void EnvironmentROBARM3D::StateID2Angles(int stateID, double* angles_r)
{
    bool bGoal = false;
    EnvROBARMHashEntry_t* HashEntry = EnvROBARM.StateID2CoordTable[stateID];

    if(stateID == EnvROBARM.goalHashEntry->stateID)
        bGoal = true;

    if(bGoal)
        ComputeContAngles(EnvROBARMCfg.goalcoords, angles_r);
    else
        ComputeContAngles(HashEntry->coord, angles_r);

    //convert angles from positive values in radians (from 0->6.28) to centered around 0 (not really needed)
    for (int i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
        if(angles_r[i] >= PI_CONST)
            angles_r[i] = -2.0*PI_CONST + angles_r[i];
    }
}


/**------------------------------------------------------------------------*/
                        /** Printing Routines */
/**------------------------------------------------------------------------*/
void EnvironmentROBARM3D::PrintHeurGrid()
{
    int x,y,z;	
    int width = EnvROBARMCfg.EnvWidth_c;
    int height = EnvROBARMCfg.EnvHeight_c;
    int depth = EnvROBARMCfg.EnvDepth_c;
    if(EnvROBARMCfg.lowres_collision_checking)
    {
        width = EnvROBARMCfg.LowResEnvWidth_c;
        height = EnvROBARMCfg.LowResEnvHeight_c;
        depth =  EnvROBARMCfg.LowResEnvDepth_c;
    }

    for (x = 0; x < width; x++)
    {
        printf("\nx = %i\n",x);
        for (y = 0; y < height; y++)
        {
            for(z = 0; z < depth; z++)
            {
                printf("%3.0i ", EnvROBARM.Heur[XYZTO3DIND(x,y,z)]/COSTMULT);
            }
            printf("\n");
        }
        printf("\n");
    }
}

void EnvironmentROBARM3D::printangles(FILE* fOut, short unsigned int* coord, bool bGoal, bool bVerbose, bool bLocal)
{
    double angles[NUMOFLINKS];
    int dangles[NUMOFLINKS];
    int i;
    short unsigned int endeff[3];

    ComputeContAngles(coord, angles);

    if(bVerbose)
    {
        for (i = 0; i < NUMOFLINKS; i++)
        {
            dangles[i] = angles[i]*(180/PI_CONST) + 0.999999;
        }
    }

    if(bVerbose)
        fprintf(fOut, "angles: ");
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%i ", dangles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%i ", dangles[i]-dangles[i-1]);
            else
                fprintf(fOut, "%i ", dangles[i]);
        }
    }


    if(bGoal)
    {
        endeff[0] = EnvROBARM.goalHashEntry->endeff[0];
        endeff[1] = EnvROBARM.goalHashEntry->endeff[1];
        endeff[2] = EnvROBARM.goalHashEntry->endeff[2];
    }
    else
    {
        ComputeEndEffectorPos(angles, endeff);
    }

    if(bVerbose)
        fprintf(fOut, "   endeff: %d %d %d", endeff[0],endeff[1],endeff[2]);
    else
        fprintf(fOut, "%d %d %d", endeff[0],endeff[1],endeff[2]);

    fprintf(fOut, "\n");
}

void EnvironmentROBARM3D::PrintAnglesWithAction(FILE* fOut, EnvROBARMHashEntry_t* HashEntry, bool bGoal, bool bVerbose, bool bLocal)
{
    double angles[NUMOFLINKS];
    int dangles[NUMOFLINKS];
    int i;

    //convert to angles
    ComputeContAngles(HashEntry->coord, angles);

//     if(bGoal)
//         ComputeContAngles(EnvROBARMCfg.goalcoords, angles);
//     else
//         ComputeContAngles(HashEntry->coord, angles);

    //convert to degrees
    if(bVerbose)
    {
        for (i = 0; i < NUMOFLINKS; i++)
        {
            dangles[i] = angles[i]*(180.0/PI_CONST) + 0.999999;
        }
    }

    if(bVerbose)
        fprintf(fOut, "angles: ");

#if OUTPUT_DEGREES
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%-3i ", dangles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%-3i ", dangles[i]-dangles[i-1]);
            else
                fprintf(fOut, "%-3i ", dangles[i]);
        }
    }
#else
    for(i = 0; i < NUMOFLINKS; i++)
    {
        if(!bLocal)
            fprintf(fOut, "%-.3f ", angles[i]);
        else
        {
            if(i > 0)
                fprintf(fOut, "%-.3f ", angles[i]-angles[i-1]);
            else
                fprintf(fOut, "%-.3f ", angles[i]);
        }
    }
#endif

    if(bVerbose)
        fprintf(fOut, "  endeff: %-2d %-2d %-2d   action: %d", HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2], HashEntry->action);
    else
        fprintf(fOut, "%-2d %-2d %-2d %-2d", HashEntry->endeff[0],HashEntry->endeff[1],HashEntry->endeff[2],HashEntry->action);

    fprintf(fOut, "\n");
}

void EnvironmentROBARM3D::PrintConfiguration(FILE* fOut)
{
    unsigned int i;
    double pX, pY, pZ;
    double start_angles[NUMOFLINKS], orientation[3][3],angle[NUMOFLINKS] = {0};
    double roll,pitch,yaw;
    short unsigned int wrist[3],elbow[3],endeff[3];

    fprintf(fOut, "\n------------------------Environment Parameters------------------------\n");
    fprintf(fOut, "Grid Cell Width: %.2f cm\n",EnvROBARMCfg.GridCellWidth*100);
    fprintf(fOut, "Use Low Resolution Collision Checking: %i\n", EnvROBARMCfg.lowres_collision_checking);
    fprintf(fOut, "Use DH Convention for Forward Kinematics: %i\n", EnvROBARMCfg.use_DH);
    fprintf(fOut, "Use Dijkstra for Heuristic Function: %i\n", EnvROBARMCfg.dijkstra_heuristic);
    fprintf(fOut, "Smoothness Weight: %.3f\n", EnvROBARMCfg.smoothing_weight);
    fprintf(fOut, "Obstacle Padding(meters): %.2f\n", EnvROBARMCfg.padding);
    fprintf(fOut, "Distance from goal to use max(H_orientation, H_position)(meters): %.2f\n", EnvROBARMCfg.ApplyRPYCost_m);
    fprintf(fOut, "\n");

    fprintf(fOut, "------------------------Start & Goal------------------------\n");
    ComputeContAngles(EnvROBARM.startHashEntry->coord, angle);
    fprintf(fOut, "Start:");
    for(i=0; i<7; i++)
        fprintf(fOut, "%1.3f  ", angle[i]);
    fprintf(fOut,"\n");

    Cell2ContXYZ(EnvROBARMCfg.BaseX_c,EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c,&pX, &pY, &pZ);
    fprintf(fOut, "Shoulder Base: %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",EnvROBARMCfg.BaseX_c,EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c,pX,pY,pZ);

    for(i = 0; i < NUMOFLINKS; i++)
        start_angles[i] = DEG2RAD(EnvROBARMCfg.LinkStartAngles_d[i]);

    ComputeEndEffectorPos(start_angles, endeff, wrist, elbow,orientation);
    Cell2ContXYZ(elbow[0],elbow[1],elbow[2],&pX, &pY, &pZ);
    fprintf(fOut, "Elbow Start:   %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",elbow[0],elbow[1],elbow[2],pX,pY,pZ);

    Cell2ContXYZ(wrist[0],wrist[1],wrist[2],&pX, &pY, &pZ);
    fprintf(fOut, "Wrist Start:   %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",wrist[0],wrist[1],wrist[2],pX,pY,pZ);

    Cell2ContXYZ(endeff[0],endeff[1],endeff[2], &pX, &pY, &pZ);
    getRPY(EnvROBARM.startHashEntry->orientation,&roll,&pitch,&yaw,1);
    fprintf(fOut, "End Effector Start: %i %i %i (cells) --> %.3f %.3f %.3f (meters)  rpy: %1.5f %1.5f %1.5f (rad)\n",endeff[0],endeff[1],endeff[2],pX,pY,pZ, roll,pitch,yaw);


    fprintf(fOut, "\nGoal(s): \n");
    if(EnvROBARMCfg.bGoalIsSet)
    {
        for(i = 0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
            fprintf(fOut, "End Effector Goal:  %i %i %i (cells) --> %.3f %.3f %.3f (meters)  rpy: %1.2f %1.2f %1.2f (rad)\n",
                    EnvROBARMCfg.EndEffGoals[i].xyz[0], EnvROBARMCfg.EndEffGoals[i].xyz[1],EnvROBARMCfg.EndEffGoals[i].xyz[2],
                    EnvROBARMCfg.EndEffGoals[i].pos[0],EnvROBARMCfg.EndEffGoals[i].pos[1],EnvROBARMCfg.EndEffGoals[i].pos[2],
                    EnvROBARMCfg.EndEffGoals[i].rpy[0],EnvROBARMCfg.EndEffGoals[i].rpy[1],EnvROBARMCfg.EndEffGoals[i].rpy[2]);
    }


#if OUTPUT_OBSTACLES
    int x, y, z;
    int sum = 0;
    fprintf(fOut, "\nObstacles:\n");
    for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
        for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
    {
        for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
        {
            sum += EnvROBARMCfg.Grid3D[x][y][z];
            if (EnvROBARMCfg.Grid3D[x][y][z] >= EnvROBARMCfg.ObstacleCost)
                fprintf(fOut, "(%i,%i,%i) ",x,y,z);
        }
    }
    fprintf(fOut, "\nThe occupancy grid contains %i obstacle cells.\n",sum); 
#endif

    fprintf(fOut, "\n------------------------DH Parameters------------------------\n");
    fprintf(fOut, "LinkTwist: ");
    for(i=0; i < NUMOFLINKS; i++) 
        fprintf(fOut, "%.2f  ",EnvROBARMCfg.DH_alpha[i]);
    fprintf(fOut, "\nLinkLength: ");
    for(i=0; i < NUMOFLINKS; i++) 
        fprintf(fOut, "%.2f  ",EnvROBARMCfg.DH_a[i]);
    fprintf(fOut, "\nLinkOffset: ");
    for(i=0; i < NUMOFLINKS; i++) 
        fprintf(fOut, "%.2f  ",EnvROBARMCfg.DH_d[i]);
    fprintf(fOut, "\nJointAngles: ");
    for(i=0; i < NUMOFLINKS; i++) 
        fprintf(fOut, "%.2f  ",EnvROBARMCfg.DH_theta[i]);

    fprintf(fOut, "\n\nMotor Limits:\n");
    fprintf(fOut, "PosMotorLimits: ");
    for(i=0; i < NUMOFLINKS; i++) 
        fprintf(fOut, "%1.2f  ",EnvROBARMCfg.PosMotorLimits[i]);
    fprintf(fOut, "\nNegMotorLimits: ");
    for(i=0; i < NUMOFLINKS; i++) 
        fprintf(fOut, "%1.2f  ",EnvROBARMCfg.NegMotorLimits[i]);
    fprintf(fOut,"\n");

    fprintf(fOut, "\n------------------------Motion Primitives------------------------\n");
    OutputActions(fOut);

    fprintf(fOut,"\n");

    if(EnvROBARMCfg.use_smooth_actions)
    {
        OutputActionCostTable(fOut);
        fprintf(fOut,"\n");
    }

    fprintf(fOut, "\n------------------------Costs------------------------\n");
    fprintf(fOut, "Cells per Action: %1.3f\n", EnvROBARMCfg.CellsPerAction);
    fprintf(fOut, "Cost per Cell: %.2f\n", EnvROBARMCfg.cost_per_cell);
    fprintf(fOut, "Cost per sqrt(2) Cells: %.2f\n",EnvROBARMCfg.cost_sqrt2_move);
    fprintf(fOut, "Cost per sqrt(3) Cells: %.2f\n",EnvROBARMCfg.cost_sqrt3_move);
    fprintf(fOut, "Cost per millimeter: %.2f\n", EnvROBARMCfg.cost_per_mm);
    fprintf(fOut, "Cost per Radian:  %.4f\n",EnvROBARMCfg.cost_per_rad);

    fprintf(fOut, "\n");
    fprintf(fOut, "\n------------------------Planning------------------------\n");
}

void EnvironmentROBARM3D::PrintAbridgedConfiguration()
{
    double pX, pY, pZ;
    double start_angles[NUMOFLINKS];
    short unsigned int endeff[3];

    ComputeEndEffectorPos(start_angles, endeff);
    Cell2ContXYZ(endeff[0],endeff[1],endeff[2], &pX, &pY, &pZ);
    printf("End Effector Start: %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",endeff[0],endeff[1],endeff[2],pX,pY,pZ);

    for(unsigned int i = 0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
        printf("End Effector Goal:  %i %i %i (cells) --> %.3f %.3f %.3f (meters)\n",
               EnvROBARMCfg.EndEffGoals[i].xyz[0], EnvROBARMCfg.EndEffGoals[i].xyz[1],EnvROBARMCfg.EndEffGoals[i].xyz[2],
               EnvROBARMCfg.EndEffGoals[i].pos[0],EnvROBARMCfg.EndEffGoals[i].pos[1],EnvROBARMCfg.EndEffGoals[i].pos[2]);

    printf("\n");
}

//display action cost table
void EnvironmentROBARM3D::OutputActionCostTable(FILE* fOut)
{
    int x,y;
    fprintf(fOut,"Action Cost Table\n");
    for (x = 0; x < EnvROBARMCfg.nSuccActions; x++)
    {
        fprintf(fOut,"%i: ",x); 
        for (y = 0; y < EnvROBARMCfg.nSuccActions; y++)
            fprintf(fOut,"%i  ",EnvROBARMCfg.ActiontoActionCosts[x][y]);
        fprintf(fOut,"\n");
    }
}

//display successor actions
void EnvironmentROBARM3D::OutputActions(FILE* fOut)
{
    int x,y;
    fprintf(fOut,"Successor Actions\n");
    for (x=0; x < EnvROBARMCfg.nSuccActions; x++)
    {
        fprintf(fOut,"%i:  ",x);
        for(y=0; y < NUMOFLINKS; y++)
            fprintf(fOut,"%.1f  ",EnvROBARMCfg.SuccActions[x][y]);

        fprintf(fOut,"\n");
    }
}

void EnvironmentROBARM3D::OutputPlanningStats()
{
    printf("\n# Possible Actions: %d\n",EnvROBARMCfg.nSuccActions);
//     printf("# GetHashEntry Calls: %i  computed in %3.2f sec\n",num_GetHashEntry,time_gethash/(double)CLOCKS_PER_SEC); 
//     printf("# Forward Kinematic Computations: %i  Time per Computation: %.3f (usec)\n",num_forwardkinematics,(DH_time/(double)CLOCKS_PER_SEC)*1000000 / num_forwardkinematics);
    printf("Cost per Cell: %.2f   *sqrt(2): %.2f   *sqrt(3):  %.2f\n",EnvROBARMCfg.cost_per_cell,EnvROBARMCfg.cost_sqrt2_move,EnvROBARMCfg.cost_sqrt3_move);

//     if(EnvROBARMCfg.use_DH)
//         printf("DH Convention: %3.2f sec\n", DH_time/(double)CLOCKS_PER_SEC);
//     else
//         printf("Kinematics Library: %3.2f sec\n", KL_time/(double)CLOCKS_PER_SEC);

//     printf("Collision Checking: %3.2f sec\n", check_collision_time/(double)CLOCKS_PER_SEC);
    printf("\n");
}

std::vector<int> EnvironmentROBARM3D::debugExpandedStates()
{
	return expanded_states;
}

/**------------------------------------------------------------------------*/
                        /** Forward Kinematics */
/**------------------------------------------------------------------------*/
/** \brief Initialize the KDL Chain used for forward kinematics */
bool EnvironmentROBARM3D::InitKDLChain(const string &fKDL)
{
  KDL::Tree my_tree;
//   std::map<string, string> segment_joint_mapping;
  if (!KDL::treeFromString(fKDL, my_tree))
  {
    printf("Failed to parse tree from manipulator description file.\n");
    return false;;
  }
//   bool Tree::getChain(const std::string& chain_root, const std::string& chain_tip, Chain& chain)const
//   std::vector<std::string> links;
  if (!my_tree.getChain("torso_lift_link", "r_wrist_roll_link", EnvROBARMCfg.arm_chain))
  {
    printf("Error: could not fetch the KDL chain for the desired manipulator. Exiting.\n"); 
    return false;
  }
  EnvROBARMCfg.jnt_to_pose_solver = new KDL::ChainFkSolverPos_recursive(EnvROBARMCfg.arm_chain);
  EnvROBARMCfg.jnt_pos_in.resize(EnvROBARMCfg.arm_chain.getNrOfJoints());

  return true;
}

/** \brief Pre-compute DH matrices for all planning frames with NUM_TRANS_MATRICES degree resolution */
void EnvironmentROBARM3D::ComputeDHTransformations()
{
    int i,n,theta;
    double c_alpha,s_alpha;

    //precompute cos and sin tables
    for (i = 0; i < 360; i++)
    {
        EnvROBARMCfg.cos_r[i] = cos(DEG2RAD(i));
        EnvROBARMCfg.sin_r[i] = sin(DEG2RAD(i));
    }

    // assign transformation matrices 
    for (n = 0; n < NUMOFLINKS; n++)
    {
        c_alpha = cos(EnvROBARMCfg.DH_alpha[n]);
        s_alpha = sin(EnvROBARMCfg.DH_alpha[n]);

        theta=0;
        for (i = 0; i < NUM_TRANS_MATRICES*4; i+=4)
        {
// 	  EnvROBARMCfg.T_DH[i][0][n] = cos(theta*M_PI/180.0);
// 	  EnvROBARMCfg.T_DH[i][1][n] = -sin(theta*M_PI/180.0)*c_alpha; 
// 	  EnvROBARMCfg.T_DH[i][2][n] = sin(theta*M_PI/180.0)*s_alpha;
// 	  EnvROBARMCfg.T_DH[i][3][n] = EnvROBARMCfg.DH_a[n]*cos(theta*M_PI/180.0);
// 
// 	  EnvROBARMCfg.T_DH[i+1][0][n] = sin(theta*M_PI/180.0);
// 	  EnvROBARMCfg.T_DH[i+1][1][n] = cos(theta*M_PI/180.0)*c_alpha;
// 	  EnvROBARMCfg.T_DH[i+1][2][n] = -cos(theta*M_PI/180.0)*s_alpha;
// 	  EnvROBARMCfg.T_DH[i+1][3][n] = EnvROBARMCfg.DH_a[n]*sin(theta*M_PI/180.0);
// 
// 
// 	  EnvROBARMCfg.T_DH[i+2][0][n] = 0.0;
// 	  EnvROBARMCfg.T_DH[i+2][1][n] = s_alpha;
// 	  EnvROBARMCfg.T_DH[i+2][2][n] = c_alpha;
// 	  EnvROBARMCfg.T_DH[i+2][3][n] = EnvROBARMCfg.DH_d[n];
// 
// 	  EnvROBARMCfg.T_DH[i+3][0][n] = 0.0;
// 	  EnvROBARMCfg.T_DH[i+3][1][n] = 0.0;
// 	  EnvROBARMCfg.T_DH[i+3][2][n] = 0.0;
// 	  EnvROBARMCfg.T_DH[i+3][3][n] = 1.0;

            EnvROBARMCfg.T_DH[i][0][n] = EnvROBARMCfg.cos_r[theta];
            EnvROBARMCfg.T_DH[i][1][n] = -EnvROBARMCfg.sin_r[theta]*c_alpha; 
            EnvROBARMCfg.T_DH[i][2][n] = EnvROBARMCfg.sin_r[theta]*s_alpha;
            EnvROBARMCfg.T_DH[i][3][n] = EnvROBARMCfg.DH_a[n]*EnvROBARMCfg.cos_r[theta];

            EnvROBARMCfg.T_DH[i+1][0][n] = EnvROBARMCfg.sin_r[theta];
            EnvROBARMCfg.T_DH[i+1][1][n] = EnvROBARMCfg.cos_r[theta]*c_alpha;
            EnvROBARMCfg.T_DH[i+1][2][n] = -EnvROBARMCfg.cos_r[theta]*s_alpha;
            EnvROBARMCfg.T_DH[i+1][3][n] = EnvROBARMCfg.DH_a[n]*EnvROBARMCfg.sin_r[theta];


            EnvROBARMCfg.T_DH[i+2][0][n] = 0.0;
            EnvROBARMCfg.T_DH[i+2][1][n] = s_alpha;
            EnvROBARMCfg.T_DH[i+2][2][n] = c_alpha;
            EnvROBARMCfg.T_DH[i+2][3][n] = EnvROBARMCfg.DH_d[n];

            EnvROBARMCfg.T_DH[i+3][0][n] = 0.0;
            EnvROBARMCfg.T_DH[i+3][1][n] = 0.0;
            EnvROBARMCfg.T_DH[i+3][2][n] = 0.0;
            EnvROBARMCfg.T_DH[i+3][3][n] = 1.0;

            theta++;
        }
    }
}

/** compute forward kinematics using the DH convention and the pre-computed DH matrices*/
void EnvironmentROBARM3D::ComputeForwardKinematics_DH(const double angles[])
{
    double sum, theta[NUMOFLINKS];
    int t,x,y,k;
    double temp3[4][4],temp5[4][4], temp7[4][4];
    double R_elbow[4][4] = {{0, 0, -1, 0},
                            {-1, 0, 0, 0},
                            {0, 1, 0, 0},
                            {0, 0, 0, 1}};
    double R_wrist[4][4] = {{0, 0, -1, 0},
                            {-1, 0, 0, 0},
                            {0, 1, 0, 0},
                            {0, 0, 0, 1}};
    double R_endeff[4][4] = {{0, 0, -1, 0},
                            {0, 1, 0, 0},
                            {1, 0, 0, 0},
                            {0, 0, 0, 1}};

    //add any theta offsets to the joint position angles   <--move this into  next loop
    for(x=0; x < NUMOFLINKS; x++)
        theta[x] = angles[x] + EnvROBARMCfg.DH_theta[x];

    // compute transformation matrices and premultiply
    for(int i = 0; i < NUMOFLINKS; i++)
    {
        //convert theta to degrees
        theta[i]=theta[i]*(180.0/PI_CONST);

        //make sure theta is not negative
        if (theta[i] < 0)
            theta[i] = 360.0+theta[i];

        //round to nearest integer
        t = theta[i] + 0.5;

        // bug fix! this is a stupid design....redo this function
        if(t >= 360)
            t = 0;

        //multiply by 4 to get to correct position in T_DH
        t = t*4;

        //multiply by previous transformations to put in shoulder frame
        if(i > 0)
        {
            for (x=0; x<4; x++)
            {
                for (y=0; y<4; y++)
                {
                    sum = 0;
                    for (k=0; k<4; k++)
                        sum += EnvROBARM.Trans[x][k][i-1] * EnvROBARMCfg.T_DH[t+k][y][i];

                    EnvROBARM.Trans[x][y][i] = sum;
                }
            }

            //elbow - rotate axis to be the same as the PR2's coordinate system
            if(i == ELBOW_JOINT)
            {
//             printf("elbow:\n");
                for (x=0; x<4; x++)
                {
                    for (y=0; y<4; y++)
                    {
                        sum = 0;
                        for (k=0; k<4; k++)
                            sum += EnvROBARM.Trans[x][k][i] * R_elbow[k][y];

//                         printf("%.2f ",sum);
                        temp3[x][y] = sum;
                    }
//                 printf("\n");
                }
            }
            else if(i == WRIST_JOINT)
            {
//                 printf("wrist:\n");
                for (x=0; x<4; x++)
                {
                    for (y=0; y<4; y++)
                    {
                        sum = 0;
                        for (k=0; k<4; k++)
                            sum += EnvROBARM.Trans[x][k][i] * R_wrist[k][y];

//                         printf("%.2f ",sum);
                        temp5[x][y] = sum;
                    }
//                     printf("\n");
                }
            }
            else if(i == ENDEFF)
            {
//                 printf("endeff:\n");
                for (x=0; x<4; x++)
                {
                    for (y=0; y<4; y++)
                    {
                        sum = 0;
                        for (k=0; k<4; k++)
                            sum += EnvROBARM.Trans[x][k][i] * R_endeff[k][y];

//                         printf("%.2f ",sum);
                        temp7[x][y] = sum;
                    }
//                     printf("\n");
                }

                //copy it over to the
//                 printf("EnvROBARM.Trans[x][y][7]: \n");
                for (x=0; x<4; x++)
                {
                    for (y=0; y<4; y++)
                    {
                        EnvROBARM.Trans[x][y][ELBOW_JOINT] = temp3[x][y];
                        EnvROBARM.Trans[x][y][WRIST_JOINT] = temp5[x][y];
                        EnvROBARM.Trans[x][y][ENDEFF] = temp7[x][y];
//                         printf("%.2f ",EnvROBARM.Trans[x][y][7]);
                    }
//                     printf("\n");
                }
            }
        }
        else
        {
            for (x=0; x<4; x++)
                for (y=0; y<4; y++)
                    EnvROBARM.Trans[x][y][0] = EnvROBARMCfg.T_DH[t+x][y][0];
        }
    }
}

/** Compute forward kinematics for an array of joints using a KDL chain */
void EnvironmentROBARM3D::ComputeForwardKinematics_ROS(const double angles[], int f_num, double *x, double *y, double*z)
{
  for(int i = 0; i < EnvROBARMCfg.num_joints; i++)
    EnvROBARMCfg.jnt_pos_in(i)=angles[i];

  if(EnvROBARMCfg.jnt_to_pose_solver->JntToCart(EnvROBARMCfg.jnt_pos_in, EnvROBARMCfg.p_out, f_num) < 0)
  {
    printf("JntToCart returned < 0. Exiting.\n");
    exit(1);
  }

  if(f_num == GOAL_JOINT)
  {
    for(int i = 0; i < 3; i++)
      for(int j = 0; j < 3; j++)
	EnvROBARM.Trans[i][j][ENDEFF] = EnvROBARMCfg.p_out.M(i,j);
  }

  //translate xyz coordinates from shoulder frame to base frame
  *x = EnvROBARMCfg.p_out.p[0] + EnvROBARMCfg.BaseX_m;
  *y = EnvROBARMCfg.p_out.p[1] + EnvROBARMCfg.BaseY_m;
  *z = EnvROBARMCfg.p_out.p[2] + EnvROBARMCfg.BaseZ_m;
}

//returns 1 if end effector within space, 0 otherwise
int EnvironmentROBARM3D::ComputeEndEffectorPos(const double angles[], short unsigned int endeff[3], short unsigned int wrist[3], short unsigned int elbow[3], double orientation[3][3])
{
  double x,y,z;
  int retval = 1;

    //convert angles from positive values in radians (from 0->6.28) to centered around 0 (not really needed)
//     for (int i = 0; i < NUMOFLINKS; i++)
//     {
//         if(angles[i] >= PI_CONST)
//             angles[i] = -2.0*PI_CONST + angles[i];
//     }

    //use DH or Kinematics Library for forward kinematics
  if(EnvROBARMCfg.use_DH)
  {
    ComputeForwardKinematics_DH(angles);

        //get position of elbow
    x = EnvROBARM.Trans[0][3][ELBOW_JOINT] + EnvROBARMCfg.BaseX_m;
    y = EnvROBARM.Trans[1][3][ELBOW_JOINT] + EnvROBARMCfg.BaseY_m;
    z = EnvROBARM.Trans[2][3][ELBOW_JOINT] + EnvROBARMCfg.BaseZ_m;
    ContXYZ2Cell(x, y, z, &(elbow[0]), &(elbow[1]), &(elbow[2]));

        //get position of wrist
    x = EnvROBARM.Trans[0][3][WRIST_JOINT] + EnvROBARMCfg.BaseX_m;
    y = EnvROBARM.Trans[1][3][WRIST_JOINT] + EnvROBARMCfg.BaseY_m;
    z = EnvROBARM.Trans[2][3][WRIST_JOINT] + EnvROBARMCfg.BaseZ_m;
    ContXYZ2Cell(x, y, z, &(wrist[0]), &(wrist[1]), &(wrist[2]));

        //get position of gripper
    x = EnvROBARM.Trans[0][3][ENDEFF] + EnvROBARMCfg.BaseX_m;
    y = EnvROBARM.Trans[1][3][ENDEFF] + EnvROBARMCfg.BaseY_m;
    z = EnvROBARM.Trans[2][3][ENDEFF] + EnvROBARMCfg.BaseZ_m;
    ContXYZ2Cell(x, y, z, &(endeff[0]),&(endeff[1]),&(endeff[2]));

        // if end effector is out of bounds then return 0
    if(endeff[0] >= EnvROBARMCfg.EnvWidth_c || endeff[1] >= EnvROBARMCfg.EnvHeight_c || endeff[2] >= EnvROBARMCfg.EnvDepth_c)
      retval =  0;
  }
  else    //use KDL Chain
  {
        //get position of elbow
    ComputeForwardKinematics_ROS(angles, 5, &x, &y, &z);
    ContXYZ2Cell(x, y, z, &(elbow[0]), &(elbow[1]), &(elbow[2]));

        //get position of wrist
    ComputeForwardKinematics_ROS(angles, 7, &x, &y, &z);
    ContXYZ2Cell(x, y, z, &(wrist[0]), &(wrist[1]), &(wrist[2]));

        //get position of r_wrist_roll_joint
    ComputeForwardKinematics_ROS(angles, GOAL_JOINT, &x, &y, &z);
    ContXYZ2Cell(x, y, z, &(endeff[0]), &(endeff[1]), &(endeff[2]));

/*
    for(short unsigned int a = 0; a < EnvROBARMCfg.num_joints; ++a)
    {
    ComputeForwardKinematics_ROS(angles, a, &x, &y, &z);
    ContXYZ2Cell(x, y, z, &(joints[a][0]), &(joints[a][1]), &(joints[a][2]));
  }
*/

        // check upper bounds
    if(endeff[0] >= EnvROBARMCfg.EnvWidth_c || endeff[1] >= EnvROBARMCfg.EnvHeight_c || endeff[2] >= EnvROBARMCfg.EnvDepth_c)
      retval =  0;
  }

    //store the orientation of gripper
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      orientation[i][j] = EnvROBARM.Trans[i][j][ENDEFF];

    // check if orientation of gripper is upright (for the whole path)
  if(EnvROBARMCfg.enforce_upright_gripper)
    if (EnvROBARM.Trans[2][0][7] < 1.0 - EnvROBARMCfg.gripper_orientation_moe)
      retval = 0;

  return retval;
}

//returns end effector position in meters
int EnvironmentROBARM3D::ComputeEndEffectorPos(const double angles[], double endeff_m[3])
{
  int retval = 1;

    //convert angles from positive values in radians (from 0->6.28) to centered around 0
//     for (int i = 0; i < NUMOFLINKS; i++)
//     {
//         if(angles[i] >= PI_CONST)
//             angles[i] = -2.0*PI_CONST + angles[i];
//     }

    //use DH or Kinematics Library for forward kinematics
  if(EnvROBARMCfg.use_DH)
  {
    ComputeForwardKinematics_DH(angles);

        //get position of tip of gripper
    endeff_m[0] = EnvROBARM.Trans[0][3][ENDEFF] + EnvROBARMCfg.BaseX_m;
    endeff_m[1] = EnvROBARM.Trans[1][3][ENDEFF] + EnvROBARMCfg.BaseY_m;
    endeff_m[2] = EnvROBARM.Trans[2][3][ENDEFF] + EnvROBARMCfg.BaseZ_m;
  }
  else
    ComputeForwardKinematics_ROS(angles, GOAL_JOINT, &(endeff_m[0]), &(endeff_m[1]), &(endeff_m[2]));

  return retval;
}

//returns end effector position in meters
int EnvironmentROBARM3D::ComputeEndEffectorPos(const double angles[], double endeff_m[3], double rpy_r[3])
{
  int retval = 1;
  double orientation[3][3];

  //use DH or Kinematics Library for forward kinematics
  if(EnvROBARMCfg.use_DH)
  {
    ComputeForwardKinematics_DH(angles);
    //get position of tip of gripper
    endeff_m[0] = EnvROBARM.Trans[0][3][ENDEFF] + EnvROBARMCfg.BaseX_m;
    endeff_m[1] = EnvROBARM.Trans[1][3][ENDEFF] + EnvROBARMCfg.BaseY_m;
    endeff_m[2] = EnvROBARM.Trans[2][3][ENDEFF] + EnvROBARMCfg.BaseZ_m;
  }
  else
    ComputeForwardKinematics_ROS(angles, GOAL_JOINT, &(endeff_m[0]), &(endeff_m[1]), &(endeff_m[2]));

  //store the orientation of gripper
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      orientation[i][j] = EnvROBARM.Trans[i][j][ENDEFF];

  getRPY(orientation, &(rpy_r[0]), &(rpy_r[1]), &(rpy_r[2]), 1);

  return retval;
}

//returns end effector position in cells
int EnvironmentROBARM3D::ComputeEndEffectorPos(const double angles[], short unsigned int endeff[3])
{
  int retval = 1;
  double endeff_m[3];

    //convert angles from positive values in radians (from 0->6.28) to centered around 0
//     for (int i = 0; i < NUMOFLINKS; i++)
//     {
//         if(angles[i] >= PI_CONST)
//             angles[i] = -2.0*PI_CONST + angles[i];
//     }

    //use DH or Kinematics Library for forward kinematics
  if(EnvROBARMCfg.use_DH)
  {
    ComputeForwardKinematics_DH(angles);

        //get position of tip of gripper
    endeff_m[0] = EnvROBARM.Trans[0][3][ENDEFF] + EnvROBARMCfg.BaseX_m;
    endeff_m[1] = EnvROBARM.Trans[1][3][ENDEFF] + EnvROBARMCfg.BaseY_m;
    endeff_m[2] = EnvROBARM.Trans[2][3][ENDEFF] + EnvROBARMCfg.BaseZ_m;
  }
  else
    ComputeForwardKinematics_ROS(angles, GOAL_JOINT, &(endeff_m[0]), &(endeff_m[1]), &(endeff_m[2]));

  ContXYZ2Cell(endeff_m[0],endeff_m[1],endeff_m[2], &(endeff[0]),&(endeff[1]),&(endeff[2]));
  return retval;
}

/** convert RPY into a rotation matrix */
void EnvironmentROBARM3D::RPY2Rot(double roll, double pitch, double yaw, double Rot[3][3])
{
    double cosr, cosp, cosy, sinr, sinp, siny;

    cosr = cos(roll);
    cosp = cos(pitch);
    cosy = cos(yaw);
    sinr = sin(roll);
    sinp = sin(pitch);
    siny = sin(yaw);

    Rot[0][0] = cosp*cosy;
    Rot[0][1] = cosy*sinp*sinr - siny*cosr;
    Rot[0][2] = cosy*sinp*cosr + siny*sinr;   

    Rot[1][0] = siny*cosp;
    Rot[1][1] = siny*sinp*sinr + cosy*cosr;
    Rot[1][2] = siny*sinp*cosr - cosy*sinr;

    Rot[2][0] = -sinp;
    Rot[2][1] = cosp*sinr;
    Rot[2][2] = cosp*cosr;
}

/** convert a rotation matrix into the RPY format (copied from getEulerZYX() in Bullet physics library) */
void EnvironmentROBARM3D::getRPY(double Rot[3][3], double* roll, double* pitch, double* yaw, int solution_number)
{
    double delta,rpy1[3],rpy2[3];

     // Check that pitch is not at a singularity
    if(fabs(Rot[0][2]) >= 1)
    {
        rpy1[2]  = 0;
        rpy2[2]  = 0;

        // From difference of angles formula
        delta = atan2(Rot[0][0], Rot[2][0]);
        if(Rot[0][2] > 0)   //gimbal locked up
        {
            rpy1[1] = PI_CONST / 2.0;
            rpy2[1] = PI_CONST / 2.0;
            rpy1[0] = rpy1[1] + delta;
            rpy2[0] = rpy2[1] + delta;
        }
        else // gimbal locked down
        {
            rpy1[1] = -PI_CONST / 2.0;
            rpy2[1] = -PI_CONST / 2.0;
            rpy1[0] = -rpy1[1] + delta;
            rpy2[0] = -rpy2[1] + delta;
        }
    }
    else
    {
        rpy1[1] = -asin(Rot[0][2]);
        rpy2[1] = PI_CONST - rpy1[1];


        rpy1[0] = atan2(Rot[1][2]/cos(rpy1[1]),
                        Rot[2][2]/cos(rpy1[1]));

        rpy2[0] = atan2(Rot[1][2]/cos(rpy2[1]),
                        Rot[2][2]/cos(rpy2[1]));

        rpy1[2] = atan2(Rot[0][1]/cos(rpy1[1]),
                        Rot[0][0]/cos(rpy1[1]));

        rpy2[2] = atan2(Rot[0][1]/cos(rpy2[1]),
                        Rot[0][0]/cos(rpy2[1]));
    }

    if (solution_number == 1)
    {
        *yaw = rpy1[2];
        *pitch = rpy1[1];
        *roll = rpy1[0];
    }
    else
    {
        *yaw = rpy2[2];
        *pitch = rpy2[1];
        *roll = rpy2[0];
    }
}

/*
void EnvironmentROBARM3D::getValidPositions(int num_pos, FILE* fOut)
{
    double jnt_pos_in[7];
    double xyz[3];
    short unsigned int endeff[3], wrist[3], elbow[3];
    double r,p,y,orientation[3][3];

    // initialize a random seed
    srand ( time(NULL) );

    for(int n = 0; n < num_pos; n++)
    {
        for(int i=0; i < 7; i++)
            jnt_pos_in[i] = gen_rand(std::max(EnvROBARMCfg.NegMotorLimits[i],-M_PI), std::min(EnvROBARMCfg.PosMotorLimits[i], M_PI));


        //compute forward kinematics
        ComputeEndEffectorPos(jnt_pos_in, xyz);
        ComputeEndEffectorPos(jnt_pos_in, endeff, wrist, elbow, orientation);

        //get roll,pitch,yaw
        getRPY(orientation,&r,&p,&y,1);

        //write to text file
        fprintf(fOut, "%.2f %.2f %.2f %.2f %.2f %.2f\n",xyz[0],xyz[1],xyz[2],r,p,y);
    }
}
*/

double EnvironmentROBARM3D::gen_rand(double min, double max)
{
  int rand_num = rand()%100+1;
  double result = min + (double)((max-min)*rand_num)/101.0;
  return result;
}


/**------------------------------------------------------------------------*/
                              /** Cost */
/**------------------------------------------------------------------------*/
/** \brief pre-compute action costs */
void EnvironmentROBARM3D::ComputeActionCosts()
{
  int i,x,y;
  double temp = 0.0;
  EnvROBARMCfg.ActiontoActionCosts = new int* [EnvROBARMCfg.nSuccActions];
  for (x = 0; x < EnvROBARMCfg.nSuccActions; x++)
  {
    EnvROBARMCfg.ActiontoActionCosts[x] = new int[EnvROBARMCfg.nSuccActions];
    for (y = 0; y < EnvROBARMCfg.nSuccActions; y++)
    {
      temp = 0.0;
      for (i = 0; i < NUMOFLINKS; i++)
      {
	temp += ((EnvROBARMCfg.SuccActions[x][i]-EnvROBARMCfg.SuccActions[y][i])*(EnvROBARMCfg.SuccActions[x][i]-EnvROBARMCfg.SuccActions[y][i]));
      }
      EnvROBARMCfg.ActiontoActionCosts[x][y] = temp * COSTMULT * EnvROBARMCfg.smoothing_weight * EnvROBARMCfg.use_smooth_actions;
    }
  }
}

/** \brief pre-compute cost per cell traversed and cost per radian */
void EnvironmentROBARM3D::ComputeCostPerCell()
{
  double angles[NUMOFLINKS],start_angles[NUMOFLINKS]={0};
  double eucl_dist, max_dist = 0, angle_r;
  int largest_action=0;
  double start_endeff_m[3], endeff_m[3];
  double gridcell_size;

  if(EnvROBARMCfg.lowres_collision_checking)
    gridcell_size = EnvROBARMCfg.LowResGridCellWidth;
  else
    gridcell_size = EnvROBARMCfg.GridCellWidth;

    //starting at zeroed angles, find end effector position after each action
  ComputeEndEffectorPos(start_angles,start_endeff_m);

    //iterate through all possible actions and find the one with the minimum cost per cell
  for (int i = 0; i < EnvROBARMCfg.nSuccActions; i++)
  {
    for(int j = 0; j < EnvROBARMCfg.num_joints; j++)
      angles[j] = DEG2RAD(EnvROBARMCfg.SuccActions[i][j]);// * (360.0/ANGLEDELTA));

        //starting at zeroed angles, find end effector position after each action
    ComputeEndEffectorPos(angles,endeff_m);

    eucl_dist = sqrt((start_endeff_m[0]-endeff_m[0])*(start_endeff_m[0]-endeff_m[0]) +
	(start_endeff_m[1]-endeff_m[1])*(start_endeff_m[1]-endeff_m[1]) +
	(start_endeff_m[2]-endeff_m[2])*(start_endeff_m[2]-endeff_m[2]));

    if (eucl_dist > max_dist)
    {
      max_dist = eucl_dist;
      largest_action = i;
    }
  }

  EnvROBARMCfg.CellsPerAction = max_dist/gridcell_size;
  EnvROBARMCfg.cost_per_cell = COSTMULT/EnvROBARMCfg.CellsPerAction;
  EnvROBARMCfg.cost_sqrt2_move = sqrt(2.0)*EnvROBARMCfg.cost_per_cell;
  EnvROBARMCfg.cost_sqrt3_move = sqrt(3.0)*EnvROBARMCfg.cost_per_cell;
  EnvROBARMCfg.cost_per_mm = EnvROBARMCfg.cost_per_cell / (gridcell_size * 1000);

    //compute cost per radian for the joint space planner (this was moved here in a hackish way)
  double max_angle = 0;
  for (int i = 0; i < EnvROBARMCfg.nSuccActions; i++)
  {
    eucl_dist = 0;

    for(int a = 0; a < NUMOFLINKS; a++)
    {
      angle_r = DEG2RAD(EnvROBARMCfg.SuccActions[i][a]);
      if(angle_r >= PI_CONST)
	angle_r = -2.0*PI_CONST + angle_r;

      eucl_dist += angles::shortest_angular_distance(angle_r, 0.0) * angles::shortest_angular_distance(angle_r, 0.0);
    }

    eucl_dist = sqrt(eucl_dist);

    if (eucl_dist > max_angle)
    {
      max_angle = eucl_dist;
      largest_action = i;
    }
  }

  EnvROBARMCfg.cost_per_rad_js = COSTMULT * max_angle;
//   printf("Cost per Radian (joint space planning):  %.3f\n",EnvROBARMCfg.cost_per_rad_js);

  ComputeCostPerRadian();
}

/** \brief compute cost per radian (orientation-wise) for the cartesian planner */
void EnvironmentROBARM3D::ComputeCostPerRadian()
{
  short unsigned int endeff[3],wrist[3],elbow[3], largest_action = 0;
  double axis_angle,max_axis_angle = 0.0;
  double angles[NUMOFLINKS] ={0}, initial_orientation[3][3], orientation[3][3];

    //starting at zeroed joint positions, find end effector position after each action
  ComputeEndEffectorPos(angles,endeff,wrist,elbow,initial_orientation);

    //iterate through all possible actions and find the one with the largest change in orientation (minimum cost per radian)
  for (int i = 0; i < EnvROBARMCfg.nSuccActions; i++)
  {
    for(int j = 0; j < NUMOFLINKS; j++)
      angles[j] = DEG2RAD(EnvROBARMCfg.SuccActions[i][j]);

    ComputeEndEffectorPos(angles,endeff,wrist,elbow,orientation);

    GetAxisAngle(orientation, initial_orientation, &axis_angle);

    if(axis_angle > max_axis_angle)
    {
      max_axis_angle = axis_angle;
      largest_action = i;
    }
  }

  EnvROBARMCfg.cost_per_rad = COSTMULT / max_axis_angle;
//   printf("\n[ComputeCostPerRadian] Largest Action: %u  Max Axis Angle: %.4f Cost/Rad: %.4f\n",largest_action,max_axis_angle, EnvROBARMCfg.cost_per_rad);
}


/**------------------------------------------------------------------------*/
                        /** Distance Functions */
/**------------------------------------------------------------------------*/
int EnvironmentROBARM3D::GetDistToClosestGoal(short unsigned int* xyz, int* goal_num)
{
  unsigned int i,ind = 0;
  int dist, min_dist = 10000000;

  for(i=0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
  {
    dist = sqrt((EnvROBARMCfg.EndEffGoals[i].xyz[0] - xyz[0])*(EnvROBARMCfg.EndEffGoals[i].xyz[0] - xyz[0]) +
	(EnvROBARMCfg.EndEffGoals[i].xyz[1] - xyz[1])*(EnvROBARMCfg.EndEffGoals[i].xyz[1] - xyz[1]) +
	(EnvROBARMCfg.EndEffGoals[i].xyz[2] - xyz[2])*(EnvROBARMCfg.EndEffGoals[i].xyz[2] - xyz[2]));
    if(dist < min_dist)
    {
      ind = i;
      min_dist = dist;
    }
  }

  (*goal_num) = ind;
  return min_dist;
}

double EnvironmentROBARM3D::GetDistToClosestGoal(double *xyz, int* goal_num)
{
  unsigned int i,ind = 0;
  double dist, min_dist = 10000000;

  for(i=0; i < EnvROBARMCfg.EndEffGoals.size(); i++)
  {
    dist = sqrt((EnvROBARMCfg.EndEffGoals[i].pos[0] - xyz[0])*(EnvROBARMCfg.EndEffGoals[i].pos[0] - xyz[0]) +
	(EnvROBARMCfg.EndEffGoals[i].pos[1] - xyz[1])*(EnvROBARMCfg.EndEffGoals[i].pos[1] - xyz[1]) +
	(EnvROBARMCfg.EndEffGoals[i].pos[2] - xyz[2])*(EnvROBARMCfg.EndEffGoals[i].pos[2] - xyz[2]));
    if(dist < min_dist)
    {
      ind = i;
      min_dist = dist;
    }
  }

  (*goal_num) = ind;
  return min_dist;
}

double EnvironmentROBARM3D::getAngularEuclDist(double rpy1[3], double rpy2[3])
{
  double rdiff = fabs(angles::shortest_angular_distance(rpy1[0], rpy2[0]));
  double pdiff = fabs(angles::shortest_angular_distance(rpy1[1], rpy2[1]));
  double ydiff = fabs(angles::shortest_angular_distance(rpy1[2], rpy2[2]));

  return sqrt(rdiff*rdiff + pdiff*pdiff + ydiff*ydiff);
}

double EnvironmentROBARM3D::getDistanceToGoal(const double angles[])
{
  int a;
  double ang_diff = 0, sum = 0, min_dist = 100000000;

  //find euclidean distance to closest goal
  for(unsigned int k = 0; k < EnvROBARMCfg.JointSpaceGoals.size(); k++)
  {
    sum = 0;
    for(a = 0; a < EnvROBARMCfg.num_joints; a++)
    {
      if(!EnvROBARMCfg.joints[a].cont)
      {
	if(!angles::shortest_angular_distance_with_limits(angles[a],  EnvROBARMCfg.JointSpaceGoals[k].pos[a], EnvROBARMCfg.joints[a].negative_limit, EnvROBARMCfg.joints[a].positive_limit, ang_diff))
	{
	  printf("[GetJointSpaceHeuristic]%i: from %f -> to %f does not lie within the bounds [%f %f].\n",
		 a, angles[a],  EnvROBARMCfg.JointSpaceGoals[k].pos[a], EnvROBARMCfg.joints[a].negative_limit, EnvROBARMCfg.joints[a].positive_limit);
	  ang_diff = 2.0*M_PI;
	}
	else
	  ang_diff = angles::shortest_angular_distance(angles[a],  EnvROBARMCfg.JointSpaceGoals[k].pos[a]);
      }

      sum += (ang_diff*ang_diff);
    }

    if(sqrt(sum) < min_dist)
      min_dist = sqrt(sum);

  }

  return min_dist;
}

double EnvironmentROBARM3D::distanceBetween3DLineSegments(const short unsigned int l1a[],const short unsigned int l1b[],
				                          const short unsigned int l2a[],const short unsigned int l2b[])
{
  // Copyright 2001, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.

  double u[3];
  double v[3];
  double w[3];
  double dP[3];

  u[0] = l1b[0] - l1a[0];
  u[1] = l1b[1] - l1a[1];
  u[2] = l1b[2] - l1a[2];

  v[0] = l2b[0] - l2a[0];
  v[1] = l2b[1] - l2a[1];
  v[2] = l2b[2] - l2a[2];

  w[0] = l1a[0] - l2a[0];
  w[1] = l1a[1] - l2a[1];
  w[2] = l1a[2] - l2a[2];

  double a = u[0] * u[0] + u[1] * u[1] + u[2] * u[2]; // always >= 0
  double b = u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; // dot(u,v);
  double c = v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; // dot(v,v);        // always >= 0
  double d = u[0] * w[0] + u[1] * w[1] + u[2] * w[2]; // dot(u,w);
  double e = v[0] * w[0] + v[1] * w[1] + v[2] * w[2]; // dot(v,w);
  double D = a*c - b*b;       // always >= 0
  double sc, sN, sD = D;      // sc = sN / sD, default sD = D >= 0
  double tc, tN, tD = D;      // tc = tN / tD, default tD = D >= 0

    // compute the line parameters of the two closest points
  if (D < SMALL_NUM) { // the lines are almost parallel
    sN = 0.0;        // force using point P0 on segment S1
    sD = 1.0;        // to prevent possible division by 0.0 later
    tN = e;
    tD = c;
  }
  else {                // get the closest points on the infinite lines
    sN = (b*e - c*d);
    tN = (a*e - b*d);
    if (sN < 0.0) {       // sc < 0 => the s=0 edge is visible
      sN = 0.0;
      tN = e;
      tD = c;
    }
    else if (sN > sD) {  // sc > 1 => the s=1 edge is visible
      sN = sD;
      tN = e + b;
      tD = c;
    }
  }

  if (tN < 0.0) {           // tc < 0 => the t=0 edge is visible
    tN = 0.0;
        // recompute sc for this edge
    if (-d < 0.0)
      sN = 0.0;
    else if (-d > a)
      sN = sD;
    else {
      sN = -d;
      sD = a;
    }
  }
  else if (tN > tD) {      // tc > 1 => the t=1 edge is visible
    tN = tD;
        // recompute sc for this edge
    if ((-d + b) < 0.0)
      sN = 0;
    else if ((-d + b) > a)
      sN = sD;
    else {
      sN = (-d + b);
      sD = a;
    }
  }

  // finally do the division to get sc and tc
  sc = (fabs(sN) < SMALL_NUM ? 0.0 : sN / sD);
  tc = (fabs(tN) < SMALL_NUM ? 0.0 : tN / tD);

  // get the difference of the two closest points
  // dP = w + (sc * u) - (tc * v);  // = S1(sc) - S2(tc)

  dP[0] = w[0] + (sc * u[0]) - (tc * v[0]);
  dP[1] = w[1] + (sc * u[1]) - (tc * v[1]);
  dP[2] = w[2] + (sc * u[2]) - (tc * v[2]);

  return  sqrt(dP[0]*dP[0] + dP[1]*dP[1] + dP[2]*dP[2]);   // return the closest distance
}


/**------------------------------------------------------------------------*/
                          /** Occupancy Grid */
/**------------------------------------------------------------------------*/
void EnvironmentROBARM3D::AddObstaclesToEnv(double**obstacles, int numobstacles)
{
  int i,p;
  double obs[6];

  for(i = 0; i < numobstacles; i++)
  {
        //check if obstacle is within the arm's reach - stupid way of doing this - change it
    if(EnvROBARMCfg.arm_length < fabs(obstacles[i][0]) - obstacles[i][3] - EnvROBARMCfg.padding ||
       EnvROBARMCfg.arm_length < fabs(obstacles[i][1]) - obstacles[i][4] - EnvROBARMCfg.padding ||
       EnvROBARMCfg.arm_length < fabs(obstacles[i][2]) - obstacles[i][5] - EnvROBARMCfg.padding)
    {
      printf("[AddObstaclesToEnv] Obstacle %i, centered at (%1.3f %1.3f %1.3f), is out of the arm's workspace.\n",i,obstacles[i][0],obstacles[i][1],obstacles[i][2]);
      continue;
    }

    for(p = 0; p < 6; p++)
      obs[p] = obstacles[i][p];

    AddObstacleToGrid(obs,0, EnvROBARMCfg.Grid3D, EnvROBARMCfg.GridCellWidth);

    if(EnvROBARMCfg.lowres_collision_checking)
      AddObstacleToGrid(obs,0, EnvROBARMCfg.LowResGrid3D, EnvROBARMCfg.LowResGridCellWidth);

    printf("[AddObstaclesToEnv] Obstacle %i was added to the environment.\n",i);
  }
}

void EnvironmentROBARM3D::AddObstacles(const vector<vector <double> > &obstacles)
{
  unsigned int i, p, cubes_added=0;
  double obs[6];

  if(EnvROBARMCfg.mCopyingGrid.try_lock())
  {
    // printf("[AddObstacles] Grabbed ownership of the grid mutex. Obstacles will be added.\n");

    for(i = 0; i < obstacles.size(); i++)
    {
      //check if obstacle has 6 parameters --> {X,Y,Z,W,H,D}
      if(obstacles[i].size() < 6)
      {
	printf("[AddObstacles] Obstacles %i has too few parameters.Skipping it.\n",i);
	continue;
      }

      //throw out obstacles if they are too small
      if(obstacles[i][3] <.001 || obstacles[i][4] <.001 || obstacles[i][5] <.001)
      {
        // printf("[AddObstacles] Obstacle %i is too small.Skipping it.\n",i);
	continue;
      }

      //check if obstacle is within the arm's reach - stupid way of doing this - change it
      if(EnvROBARMCfg.arm_length < fabs(obstacles[i][0]) - obstacles[i][3] - EnvROBARMCfg.padding ||
	 EnvROBARMCfg.arm_length < fabs(obstacles[i][1]) - obstacles[i][4] - EnvROBARMCfg.padding ||
	 EnvROBARMCfg.arm_length < fabs(obstacles[i][2]) - obstacles[i][5] - EnvROBARMCfg.padding)
      {
//         printf("[AddObstacles] Obstacle %i, centered at (%1.3f %1.3f %1.3f), is out of the arm's workspace.\n",i,obstacles[i][0],obstacles[i][1],obstacles[i][2]);
	continue;
      }
      else
      {
	for(p = 0; p < 6; p++)
	  obs[p] = obstacles[i][p];

        // add new obstacle to the temporary maps
	AddObstacleToGrid(obs,0, EnvROBARMCfg.Grid3D_temp, EnvROBARMCfg.GridCellWidth);

	if(EnvROBARMCfg.lowres_collision_checking)
	  AddObstacleToGrid(obs,0, EnvROBARMCfg.LowResGrid3D_temp, EnvROBARMCfg.LowResGridCellWidth);

        // printf("[AddObstacles] Obstacle %i: (%.2f %.2f %.2f) was added to the environment.\n",i,obs[0],obs[1],obs[2]);
	cubes_added++;
      }
    }

    EnvROBARMCfg.mCopyingGrid.unlock();
  }
  else
  {
    // printf("[AddObstacles] Tried to get mutex lock but failed. Obstacles were not added.\n");
    return;
  }
}

void EnvironmentROBARM3D::InitializeEnvGrid()
{
  int x, y, z;

	// High-Res Grid - allocate the 3D environment & fill set all cells to zero
	EnvROBARMCfg.Grid3D = new unsigned char** [EnvROBARMCfg.EnvWidth_c];
	EnvROBARMCfg.Grid3D_temp = new unsigned char** [EnvROBARMCfg.EnvWidth_c];
  for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
  {
		EnvROBARMCfg.Grid3D[x] = new unsigned char* [EnvROBARMCfg.EnvHeight_c];
		EnvROBARMCfg.Grid3D_temp[x] = new unsigned char* [EnvROBARMCfg.EnvHeight_c];
    for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
    {
			EnvROBARMCfg.Grid3D[x][y] = new unsigned char [EnvROBARMCfg.EnvDepth_c];
			EnvROBARMCfg.Grid3D_temp[x][y] = new unsigned char [EnvROBARMCfg.EnvDepth_c];
      for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
      {
				EnvROBARMCfg.Grid3D[x][y][z] = 255;
				EnvROBARMCfg.Grid3D_temp[x][y][z] = 255;
      }
    }
  }

	//Low-Res Grid - for faster collision checking
  if(EnvROBARMCfg.lowres_collision_checking)
  {
		// Low-Res Grid - allocate the 3D environment & fill set all cells to zero
		EnvROBARMCfg.LowResGrid3D = new unsigned char** [EnvROBARMCfg.LowResEnvWidth_c];
    EnvROBARMCfg.LowResGrid3D_temp = new unsigned char** [EnvROBARMCfg.LowResEnvWidth_c];
    for (x = 0; x < EnvROBARMCfg.LowResEnvWidth_c; x++)
    {
			EnvROBARMCfg.LowResGrid3D[x] = new unsigned char* [EnvROBARMCfg.LowResEnvHeight_c];
			EnvROBARMCfg.LowResGrid3D_temp[x] = new unsigned char* [EnvROBARMCfg.LowResEnvHeight_c];
      for (y = 0; y < EnvROBARMCfg.LowResEnvHeight_c; y++)
      {
        EnvROBARMCfg.LowResGrid3D[x][y] = new unsigned char [EnvROBARMCfg.LowResEnvDepth_c];
				EnvROBARMCfg.LowResGrid3D_temp[x][y] = new unsigned char [EnvROBARMCfg.LowResEnvDepth_c];
				for (z = 0; z < EnvROBARMCfg.LowResEnvDepth_c; z++)
				{
					EnvROBARMCfg.LowResGrid3D[x][y][z] = 255;
					EnvROBARMCfg.LowResGrid3D_temp[x][y][z] = 255;
				}
      }
    }
#if DEBUG
    printf("Allocated LowResGrid3D.\n");
#endif
  }
  printf("[sbpl_arm_planner] initialized occupancy grid\n");
}

std::vector<std::vector<double> >* EnvironmentROBARM3D::getCollisionMap()
{
//     for(unsigned int i=0; i < EnvROBARMCfg.cubes.size(); i++)
//     {
//         printf("[getCollisionMap] cube %i: ",i);
//         for(unsigned int k=0; k < EnvROBARMCfg.cubes[i].size(); k++)
//             printf("%.3f ",EnvROBARMCfg.cubes[i][k]);
//         printf("\n");
//     }

  if(EnvROBARMCfg.mCopyingGrid.try_lock())
  {
    EnvROBARMCfg.sbpl_cubes = EnvROBARMCfg.cubes;
    EnvROBARMCfg.mCopyingGrid.unlock();
  }
  else
  {
    printf("[getCollisionMap] Could not acquire mutex...\n");
  }
  return &(EnvROBARMCfg.sbpl_cubes);
}

void EnvironmentROBARM3D::UpdateEnvironment()
{	
	if(EnvROBARMCfg.use_voxel3d_occupancy_grid)
	{
		clock_t start_cp = clock();
		copyVoxelGrid(voxel_grid, EnvROBARMCfg.Grid3D);
		if(EnvROBARMCfg.lowres_collision_checking)
			copyVoxelGrid(lowres_voxel_grid, EnvROBARMCfg.LowResGrid3D);
		printf("copying of voxel to grid took %lf seconds\n", (clock() - start_cp) / (double)CLOCKS_PER_SEC);
		return;
	}

  int x, y, z;
  int b = 0, c = 0;
  EnvROBARMCfg.sbpl_cubes = EnvROBARMCfg.cubes;
  for (x = 0; x < EnvROBARMCfg.EnvWidth_c; ++x)
  {
    for (y = 0; y < EnvROBARMCfg.EnvHeight_c; ++y)
    {
      for (z = 0; z < EnvROBARMCfg.EnvDepth_c; ++z)
      {
				EnvROBARMCfg.Grid3D[x][y][z] = EnvROBARMCfg.Grid3D_temp[x][y][z];
			
				if(EnvROBARMCfg.Grid3D[x][y][z] > 0)
					c++;
      }
    }
  }

  if(EnvROBARMCfg.lowres_collision_checking)
  {
    for (x = 0; x < EnvROBARMCfg.LowResEnvWidth_c; ++x)
    {
      for (y = 0; y < EnvROBARMCfg.LowResEnvHeight_c; ++y)
      {
				for (z = 0; z < EnvROBARMCfg.LowResEnvDepth_c; ++z)
				{
					EnvROBARMCfg.LowResGrid3D[x][y][z] = EnvROBARMCfg.LowResGrid3D_temp[x][y][z];
			
					if(EnvROBARMCfg.LowResGrid3D[x][y][z] > 0)
						b++;
				}
      }
    }
  }
	//   printf("[UpdateEnvironment] There are %d obstacle cells in the low resolution planning grid.\n",b);
  //   printf("[UpdateEnvironment] There are %d obstacle cells in the high resolution planning grid.\n",c);
 
}

/* //add obstacles to grid (right now just supports cubes)

void EnvironmentROBARM3D::AddObstacleToGrid(double* obstacle, int type, char*** grid, double gridcell_m)
{
    int x, y, z, pX_max, pX_min, pY_max, pY_min, pZ_max, pZ_min;
    int padding_c = EnvROBARMCfg.padding*2 / gridcell_m + 0.5;
    short unsigned int obstacle_c[6] = {0};
    int dims_c[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};

    if(gridcell_m == EnvROBARMCfg.LowResGridCellWidth)
{
        dims_c[0] = EnvROBARMCfg.LowResEnvWidth_c;
        dims_c[1] = EnvROBARMCfg.LowResEnvHeight_c;
        dims_c[2] = EnvROBARMCfg.LowResEnvDepth_c;
}

    //cube(meters)
    if (type == 0)
{
        double obs[3] = {obstacle[0],obstacle[1],obstacle[2]};
        short unsigned int obs_c[3] = {obstacle_c[0],obstacle_c[1],obstacle_c[2]};

        ContXYZ2Cell(obs, gridcell_m, dims_c, obs_c);
        //ContXYZ2Cell(obstacle[0],obstacle[1],obstacle[2],&(obstacle_c[0]),&(obstacle_c[1]),&(obstacle_c[2]));

        //get dimensions (width,height,depth)
        obstacle_c[3] = abs(obstacle[3]+EnvROBARMCfg.padding*2) / gridcell_m + 0.5;
        obstacle_c[4] = abs(obstacle[4]+EnvROBARMCfg.padding*2) / gridcell_m + 0.5;
        obstacle_c[5] = abs(obstacle[5]+ EnvROBARMCfg.padding*2) / gridcell_m + 0.5;

        pX_max = obs_c[0] + obstacle_c[3]/2;
        pX_min = obs_c[0] - obstacle_c[3]/2;
        pY_max = obs_c[1] + obstacle_c[4]/2;
        pY_min = obs_c[1] - obstacle_c[4]/2;
        pZ_max = obs_c[2] + obstacle_c[5]/2;
        pZ_min = obs_c[2] - obstacle_c[5]/2;
}
    //cube(cells)
    else if(type == 1)
{
        //convert to short unsigned int from double
        obstacle_c[0] = obstacle[0];
        obstacle_c[1] = obstacle[1];
        obstacle_c[2] = obstacle[2];
        obstacle_c[3] = abs(obstacle[3] + padding_c);
        obstacle_c[4] = abs(obstacle[4] + padding_c);
        obstacle_c[5] = abs(obstacle[5] + padding_c);

        pX_max = obstacle_c[0] + obstacle_c[3]/2;
        pX_min = obstacle_c[0] - obstacle_c[3]/2;
        pY_max = obstacle_c[1] + obstacle_c[4]/2;
        pY_min = obstacle_c[1] - obstacle_c[4]/2;
        pZ_max = obstacle_c[2] + obstacle_c[5]/2;
        pZ_min = obstacle_c[2] - obstacle_c[5]/2;
}
    else
{
        printf("Error: Attempted to add undefined type of obstacle to grid.\n");
        return; 
}

    // bounds checking, cutoff object if out of bounds
    if(pX_max > dims_c[0] - 1)
        pX_max = dims_c[0] - 1;
    if (pX_min < 0)
        pX_min = 0;
    if(pY_max > dims_c[1] - 1)
        pY_max = dims_c[1] - 1;
    if (pY_min < 0)
        pY_min = 0;
    if(pZ_max > dims_c[2] - 1)
        pZ_max = dims_c[2] - 1;
    if (pZ_min < 0)
        pZ_min = 0;

    // assign the cells occupying the obstacle to 1
    int b = 0;
    for (y = pY_min; y <= pY_max; y++)
{
        for (x = pX_min; x <= pX_max; x++)
{
            for (z = pZ_min; z <= pZ_max; z++)
{
                grid[x][y][z] = 1;
                b++;
}
}
}
//     printf("%i %i %i %i %i %i\n", obstacle_c[0],obstacle_c[1],obstacle_c[2],obstacle_c[3],obstacle_c[4],obstacle_c[5]);
//     printf("Obstacle %i cells\n",b);
}
*/

void EnvironmentROBARM3D::AddObstacleToGrid(double* obstacle, int type, unsigned char*** grid, double gridcell_m)
{

  int x, y, z, pX_max, pX_min, pY_max, pY_min, pZ_max, pZ_min;
  int padding_c = (EnvROBARMCfg.padding / gridcell_m) + 0.5;
  short unsigned int obstacle_c[6] = {0}, obs_c[3] = {0};
  int dims_c[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};
  double ob[3];

  if(gridcell_m == EnvROBARMCfg.LowResGridCellWidth)
  {
    dims_c[0] = EnvROBARMCfg.LowResEnvWidth_c;
    dims_c[1] = EnvROBARMCfg.LowResEnvHeight_c;
    dims_c[2] = EnvROBARMCfg.LowResEnvDepth_c;
  }

    //cube(meters)
  if (type == 0)
  {
    double obs[3] = {obstacle[0],obstacle[1],obstacle[2]};
    ContXYZ2Cell(obs, gridcell_m, dims_c, obs_c);

        //get dimensions of obstacles in cells (width,height,depth)
    obstacle_c[3] = obstacle[3] / gridcell_m + 0.5;
    obstacle_c[4] = obstacle[4] / gridcell_m + 0.5;
    obstacle_c[5] = obstacle[5] / gridcell_m + 0.5;

    pX_max = obs_c[0] + obstacle_c[3]/2 + padding_c;
    pX_min = obs_c[0] - obstacle_c[3]/2 - padding_c;
    pY_max = obs_c[1] + obstacle_c[4]/2 + padding_c;
    pY_min = obs_c[1] - obstacle_c[4]/2 - padding_c;
    pZ_max = obs_c[2] + obstacle_c[5]/2 + padding_c;
    pZ_min = obs_c[2] - obstacle_c[5]/2 - padding_c;
  }

    //cube(cells)
  else if(type == 1)
  {
        //convert to short unsigned int from double
    obstacle_c[0] = obstacle[0];
    obstacle_c[1] = obstacle[1];
    obstacle_c[2] = obstacle[2];
    obstacle_c[3] = abs(obstacle[3] + padding_c);
    obstacle_c[4] = abs(obstacle[4] + padding_c);
    obstacle_c[5] = abs(obstacle[5] + padding_c);

    pX_max = obstacle_c[0] + obstacle_c[3]/2;
    pX_min = obstacle_c[0] - obstacle_c[3]/2;
    pY_max = obstacle_c[1] + obstacle_c[4]/2;
    pY_min = obstacle_c[1] - obstacle_c[4]/2;
    pZ_max = obstacle_c[2] + obstacle_c[5]/2;
    pZ_min = obstacle_c[2] - obstacle_c[5]/2;
  }
  else
  {
    printf("Error: Attempted to add undefined type of obstacle to grid.\n");
    return; 
  }

    // bounds checking, cutoff object if out of bounds
  if(pX_max > dims_c[0] - 1)
    pX_max = dims_c[0] - 1;
  if (pX_min < 0)
    pX_min = 0;
  if(pY_max > dims_c[1] - 1)
    pY_max = dims_c[1] - 1;
  if (pY_min < 0)
    pY_min = 0;
  if(pZ_max > dims_c[2] - 1)
    pZ_max = dims_c[2] - 1;
  if (pZ_min < 0)
    pZ_min = 0;

//     printf("[AddObstacleToGrid] %.2f %.2f %.2f (meters)  -> %d %d %d (cells)\n",obstacle[0],obstacle[1],obstacle[2],obs_c[0],obs_c[1],obs_c[2]);
//     printf("[AddObstacleToGrid] %d %d %d %d %d %d (cells)\n",pX_min,pX_max,pY_min,pY_max,pZ_min,pZ_max);

    // assign the cells occupying the obstacle to ObstacleCost
  for (x = pX_min; x <= pX_max; x++)
  {
    for (y = pY_min; y <= pY_max; y++)
    {
      for (z = pZ_min; z <= pZ_max; z++)
      {
	grid[x][y][z] = EnvROBARMCfg.ObstacleCost;
      }
    }
  }

    //output collision map debugging  - stupid way of doing this but fine for now!
  if(EnvROBARMCfg.lowres_collision_checking && gridcell_m == EnvROBARMCfg.GridCellWidth)
  {
    Cell2ContXYZ(obs_c[0],obs_c[1],obs_c[2],&(ob[0]),&(ob[1]),&(ob[2]));
    EnvROBARMCfg.cubes.resize(EnvROBARMCfg.cubes.size()+1);
    EnvROBARMCfg.cubes.back().resize(6);
    EnvROBARMCfg.cubes.back()[0] = ob[0];
    EnvROBARMCfg.cubes.back()[1] = ob[1];
    EnvROBARMCfg.cubes.back()[2] = ob[2];
    EnvROBARMCfg.cubes.back()[3] = ((double)pX_max - (double)pX_min)*gridcell_m;
    EnvROBARMCfg.cubes.back()[4] = ((double)pY_max - (double)pY_min)*gridcell_m;
    EnvROBARMCfg.cubes.back()[5] = ((double)pZ_max - (double)pZ_min)*gridcell_m;
  }
  else if(!EnvROBARMCfg.lowres_collision_checking)
  {
    Cell2ContXYZ(obs_c[0],obs_c[1],obs_c[2],&(ob[0]),&(ob[1]),&(ob[2]));
    EnvROBARMCfg.cubes.resize(EnvROBARMCfg.cubes.size()+1);
    EnvROBARMCfg.cubes.back().resize(6);
    EnvROBARMCfg.cubes.back()[0] = ob[0];
    EnvROBARMCfg.cubes.back()[1] = ob[1];
    EnvROBARMCfg.cubes.back()[2] = ob[2];
    EnvROBARMCfg.cubes.back()[3] = ((double)pX_max - (double)pX_min)*gridcell_m;
    EnvROBARMCfg.cubes.back()[4] = ((double)pY_max - (double)pY_min)*gridcell_m;
    EnvROBARMCfg.cubes.back()[5] = ((double)pZ_max - (double)pZ_min)*gridcell_m;
  }
/*
    //variable cell costs
  if(EnvROBARMCfg.variable_cell_costs)
  {
        //apply cost to cells close to obstacles
  pX_min -= EnvROBARMCfg.medCostRadius_c;
  pX_max += EnvROBARMCfg.medCostRadius_c;
  pY_min -= EnvROBARMCfg.medCostRadius_c;
  pY_max += EnvROBARMCfg.medCostRadius_c;
  pZ_min -= EnvROBARMCfg.medCostRadius_c;
  pZ_max += EnvROBARMCfg.medCostRadius_c;

        // bounds checking, cutoff object if out of bounds
  if(pX_max > dims_c[0] - 1)
  pX_max = dims_c[0] - 1;
  if (pX_min < 0)
  pX_min = 0;
  if(pY_max > dims_c[1] - 1)
  pY_max = dims_c[1] - 1;
  if (pY_min < 0)
  pY_min = 0;
  if(pZ_max > dims_c[2] - 1)
  pZ_max = dims_c[2] - 1;
  if (pZ_min < 0)
  pZ_min = 0;

        // assign the cells close to the obstacle to medObstacleCost
  for (y = pY_min; y <= pY_max; y++)
  {
  for (x = pX_min; x <= pX_max; x++)
  {
  for (z = pZ_min; z <= pZ_max; z++)
  {
  if(grid[x][y][z] < EnvROBARMCfg.medObstacleCost)
  {
  grid[x][y][z] = EnvROBARMCfg.medObstacleCost;
}
}
}
}

        //apply cost to cells less close to obstacles
  pX_min -= EnvROBARMCfg.lowCostRadius_c;
  pX_max += EnvROBARMCfg.lowCostRadius_c;
  pY_min -= EnvROBARMCfg.lowCostRadius_c;
  pY_max += EnvROBARMCfg.lowCostRadius_c;
  pZ_min -= EnvROBARMCfg.lowCostRadius_c;
  pZ_max += EnvROBARMCfg.lowCostRadius_c;

        // bounds checking, cutoff object if out of bounds
  if(pX_max > dims_c[0] - 1)
  pX_max = dims_c[0] - 1;
  if (pX_min < 0)
  pX_min = 0;
  if(pY_max > dims_c[1] - 1)
  pY_max = dims_c[1] - 1;
  if (pY_min < 0)
  pY_min = 0;
  if(pZ_max > dims_c[2] - 1)
  pZ_max = dims_c[2] - 1;
  if (pZ_min < 0)
  pZ_min = 0;

        // assign the cells close to the obstacle to lowObstacleCost
  for (y = pY_min; y <= pY_max; y++)
  {
  for (x = pX_min; x <= pX_max; x++)
  {
  for (z = pZ_min; z <= pZ_max; z++)
  {
  if(grid[x][y][z] < EnvROBARMCfg.lowObstacleCost)
  {
  grid[x][y][z] = EnvROBARMCfg.lowObstacleCost;
}
}
}
}
}
*/
}

void EnvironmentROBARM3D::ClearEnv()
{
  int x, y, z;

    //clear temporary collision map
  if (EnvROBARMCfg.mCopyingGrid.try_lock())
  {
//         printf("[ClearEnv] Clearing environment of all obstacles.\n");

    EnvROBARMCfg.cubes.clear();

        // set all cells to zero
    for (x = 0; x < EnvROBARMCfg.EnvWidth_c; x++)
    {
      for (y = 0; y < EnvROBARMCfg.EnvHeight_c; y++)
      {
	for (z = 0; z < EnvROBARMCfg.EnvDepth_c; z++)
	{
	  EnvROBARMCfg.Grid3D_temp[x][y][z] = 0;
	}
      }
    }

        // set all cells to zero in lowres grid
    if(EnvROBARMCfg.lowres_collision_checking)
    {
      for (x = 0; x < EnvROBARMCfg.LowResEnvWidth_c; x++)
      {
	for (y = 0; y < EnvROBARMCfg.LowResEnvHeight_c; y++)
	{
	  for (z = 0; z < EnvROBARMCfg.LowResEnvDepth_c; z++)
	  {
	    EnvROBARMCfg.LowResGrid3D_temp[x][y][z] = 0;
	  }
	}
      }
    }
    EnvROBARMCfg.mCopyingGrid.unlock();
  }
  else
  {
    printf("[ClearEnv] Tried to get mutex lock but failed. Environment was not cleared.\n");
    return;
  }
}

void EnvironmentROBARM3D::copyVoxelGrid(const boost::shared_ptr<Voxel3d> &voxel_grid, unsigned char *** Grid3D)
{
  int sizeX,sizeY,sizeZ;
  double resolution;
  voxel_grid->getDimensions(&sizeX,&sizeY,&sizeZ,&resolution);

  for(int x = 0; x < sizeX; ++x)
  {
    for(int y = 0; y < sizeY; ++y)
    {
      for(int z = 0; z < sizeZ; ++z)
      {
      	Grid3D[x][y][z] = getVoxel(x,y,z,voxel_grid);
      }
    }
  }
}

bool EnvironmentROBARM3D::updateVoxelGrid(const boost::shared_ptr<Voxel3d> envgrid, const boost::shared_ptr<Voxel3d> lowres_envgrid)
{
  // point the local planning grid to the inputted grid
  if(EnvROBARMCfg.mCopyingGrid.try_lock())
  {
    voxel_grid = envgrid;
    if(EnvROBARMCfg.lowres_collision_checking)
      lowres_voxel_grid = lowres_envgrid;
    EnvROBARMCfg.mCopyingGrid.unlock();
  }
  else
  {
    printf("[updateVoxelGrid] Could not acquire mutex...\n");
    return false;
  }

  return true;
}


/**------------------------------------------------------------------------*/
                          /** Collision Checking */
/**------------------------------------------------------------------------*/
//if pTestedCells is NULL, then the tested points are not saved and it is more
//efficient as it returns as soon as it sees first invalid point
int EnvironmentROBARM3D::IsValidLineSegment(short unsigned int x0, short unsigned int y0, short unsigned int z0, short unsigned int x1,
					    short unsigned int y1, short unsigned int z1, unsigned char*** Grid3D, vector<CELLV>* pTestedCells)
{
  printf("entering old isValidLineSeg\n");
  bresenham_param_t params;
  int nXYZ[3]; // int nX, nY, nZ;
  int retvalue = 1;
  CELLV tempcell;

  short unsigned int width = EnvROBARMCfg.EnvWidth_c;
  short unsigned int height = EnvROBARMCfg.EnvHeight_c;
  short unsigned int depth = EnvROBARMCfg.EnvDepth_c;
  boost::shared_ptr<Voxel3d> vGrid = voxel_grid;

  //use low resolution grid if enabled
  if(EnvROBARMCfg.lowres_collision_checking)
  {
    width = EnvROBARMCfg.LowResEnvWidth_c;
    height = EnvROBARMCfg.LowResEnvHeight_c;
    depth = EnvROBARMCfg.LowResEnvDepth_c;
    vGrid = lowres_voxel_grid;
  }

  //make sure the line segment is inside the environment - no need to check < 0
  if(x0 >= width || x1 >= width ||
     y0 >= height || y1 >= height ||
     z0 >= depth || z1 >= depth)
  {
    return 0;
  }

  //iterate through the points on the segment
  get_bresenham_parameters3d(x0, y0, z0, x1, y1, z1, &params);
  do {
    get_current_point3d(&params, &(nXYZ[0]), &(nXYZ[1]), &(nXYZ[2]));

		if(!isValidCell(nXYZ, EnvROBARMCfg.upper_arm_radius, Grid3D, vGrid))
    {
      if(pTestedCells == NULL)
				return 0;
			else
        retvalue = 0;
    }
	
    //insert the tested point
    if(pTestedCells)
    {
      tempcell.bIsObstacle = isValidCell(nXYZ, EnvROBARMCfg.upper_arm_radius, Grid3D, vGrid);
      tempcell.x = nXYZ[0];
      tempcell.y = nXYZ[1];
      tempcell.z = nXYZ[2];
      pTestedCells->push_back(tempcell);
    }
  } while (get_next_point3d(&params));

  printf("exiting old isValidLineSeg\n");
  return retvalue;
}

int EnvironmentROBARM3D::IsValidLineSegment(const short unsigned int a[], const short unsigned int b[], int radius,
					    vector<CELLV>* pTestedCells, unsigned char *** Grid3D, const boost::shared_ptr<Voxel3d> vGrid)
{
//   printf("entering isValidLineSeg\n");
  bresenham_param_t params;
  int nXYZ[3];
  int retvalue = 1;
  CELLV tempcell;

  //iterate through the points on the segment
  get_bresenham_parameters3d(a[0], a[1], a[2], b[0], b[1], b[2], &params);
  do {
    get_current_point3d(&params, &(nXYZ[0]), &(nXYZ[1]), &(nXYZ[2]));

		if(!isValidCell(nXYZ, radius, Grid3D, vGrid))
    {
      if(pTestedCells == NULL)
				return 0;
			else
				retvalue = 0;
    }

    //insert the tested point
    if(pTestedCells)
    {
      tempcell.bIsObstacle = isValidCell(nXYZ, radius, Grid3D, vGrid);
      tempcell.x = nXYZ[0];
      tempcell.y = nXYZ[1];
      tempcell.z = nXYZ[2];
      pTestedCells->push_back(tempcell);
    }
  } while (get_next_point3d(&params));

//   printf("exiting isValidLineSeg\n");
  return retvalue;
}

unsigned char EnvironmentROBARM3D::IsValidLineSegment(const short unsigned int a[],const short unsigned int b[],const unsigned int radius,vector<CELLV>* pTestedCells, unsigned char *** Grid3D)
{
	bresenham_param_t params;
	int nXYZ[3];
	unsigned char cell_val, min_dist = 255;
	CELLV tempcell;

  //iterate through the points on the segment
	get_bresenham_parameters3d(a[0], a[1], a[2], b[0], b[1], b[2], &params);
	do {
		get_current_point3d(&params, &(nXYZ[0]), &(nXYZ[1]), &(nXYZ[2]));

		cell_val = getCell(nXYZ,Grid3D);
		if(cell_val <= radius)
		{
			if(pTestedCells == NULL)
				return 0;
			else
				min_dist = 0;
		}

		if(cell_val < min_dist)
			min_dist = cell_val;

    //insert the tested point
		if(pTestedCells)
		{
			if(cell_val <= radius)
				tempcell.bIsObstacle = true;
			else
				tempcell.bIsObstacle = false;
			tempcell.x = nXYZ[0];
			tempcell.y = nXYZ[1];
			tempcell.z = nXYZ[2];
			pTestedCells->push_back(tempcell);
		}
	} while (get_next_point3d(&params));

	return min_dist;
}

int EnvironmentROBARM3D::IsValidLineSegment(const short unsigned int xyz0[], const short unsigned int xyz1[], const int &radius, unsigned char*** Grid3D, const int grid_dims[], vector<CELLV>* pTestedCells)
{
  bresenham_param_t params;
  int nXYZ[3];
  int retvalue = 1;
  CELLV tempcell;

  printf("WARNING: NOT IN USE YET\n");
  //make sure the line segment is inside the environment
  if(xyz0[0] >= grid_dims[0] || xyz1[0] >= grid_dims[0] ||
     xyz0[1] >= grid_dims[1] || xyz1[1] >= grid_dims[1] ||
     xyz0[2] >= grid_dims[2] || xyz1[2] >= grid_dims[2])
    return 0;

  //iterate through the points on the segment
  get_bresenham_parameters3d(xyz0[0], xyz0[1], xyz0[2], xyz1[0], xyz1[1], xyz1[2], &params);
  do {
    get_current_point3d(&params, &(nXYZ[0]), &(nXYZ[1]), &(nXYZ[2]));

    if(!isValidCell(nXYZ, radius, Grid3D, voxel_grid))
    {
      if(pTestedCells == NULL)
	return 0;
      else
	retvalue = 0;
    }

    //insert the tested point
    if(pTestedCells)
    {
      tempcell.bIsObstacle = isValidCell(nXYZ, radius, Grid3D, voxel_grid);
      tempcell.x = nXYZ[0];
      tempcell.y = nXYZ[1];
      tempcell.z = nXYZ[2];
      pTestedCells->push_back(tempcell);
    }
  } while (get_next_point3d(&params));

  return retvalue;
}

int EnvironmentROBARM3D::IsValidCoord(short unsigned int coord[], short unsigned int endeff_pos[3], short unsigned int wrist_pos[3], short unsigned int elbow_pos[3], double orientation[3][3])
{
  int grid_dims[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};
  short unsigned int endeff[3] = {endeff_pos[0],endeff_pos[1],endeff_pos[2]};
  short unsigned int wrist[3] = {wrist_pos[0],wrist_pos[1],wrist_pos[2]};
  short unsigned int elbow[3] = {elbow_pos[0], elbow_pos[1], elbow_pos[2]};
  short unsigned int shoulder[3] = {EnvROBARMCfg.BaseX_c, EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c};
  short unsigned int gripper[3];

  //use high resolution grid by default
  unsigned char*** Grid3D = EnvROBARMCfg.Grid3D;
  double grid_cell_m = EnvROBARMCfg.GridCellWidth;
  double angles[NUMOFLINKS];
  int retvalue = 1;
  vector<CELLV>* pTestedCells = NULL;
  ComputeContAngles(coord, angles);

  boost::shared_ptr<Voxel3d> vGrid = voxel_grid;

  //for low resolution collision checking
  if(EnvROBARMCfg.lowres_collision_checking)
  {
    grid_dims[0] = EnvROBARMCfg.LowResEnvWidth_c;
    grid_dims[1] = EnvROBARMCfg.LowResEnvHeight_c;
    grid_dims[2] = EnvROBARMCfg.LowResEnvDepth_c;
    Grid3D = EnvROBARMCfg.LowResGrid3D;
    grid_cell_m = EnvROBARMCfg.LowResGridCellWidth;

    HighResGrid2LowResGrid(endeff, endeff);
    HighResGrid2LowResGrid(wrist, wrist);
    HighResGrid2LowResGrid(elbow, elbow);
    HighResGrid2LowResGrid(shoulder, shoulder);
    vGrid = lowres_voxel_grid;
  }

    // check motor position limits
  if(EnvROBARMCfg.enforce_motor_limits)
  {
      // check if joint position angle is unreachable
    for (int i = 0; i < EnvROBARMCfg.num_joints; i++)
    {
      if(!EnvROBARMCfg.joints[i].cont)
      {
				// normalize angle
				if(angles[i] >= M_PI)
					angles[i] += -2.0*M_PI;

				if(angles[i] > EnvROBARMCfg.joints[i].positive_limit || angles[i] < EnvROBARMCfg.joints[i].negative_limit)
				{
				#if DEBUG_VALID_COORD
						printf("[IsValidCoord] joint %i: %s limit failed: %.3f [%.3f, %3f]\n", i, EnvROBARMCfg.joints[i].name.c_str(), angles[i], EnvROBARMCfg.joints[i].negative_limit,EnvROBARMCfg.joints[i].positive_limit);
				#endif
					return 0;
				}
      }
    }
  }

  //bounds checking on upper bound of map - should really only check end effector not other joints
  if(endeff[0] >= grid_dims[0] || endeff[1] >= grid_dims[1] || endeff[2] >= grid_dims[2] ||
     wrist[0] >= grid_dims[0] || wrist[1] >= grid_dims[1] || wrist[2] >= grid_dims[2] ||
     elbow[0] >= grid_dims[0] || elbow[1] >= grid_dims[1] || elbow[2] >= grid_dims[2])
  {
  #if DEBUG_VALID_COORD
      printf("[IsValidCoord] One of the joints is outside of the grid dimensions (%i %i %i).\n", grid_dims[0],grid_dims[1],grid_dims[2]);
      printf("[IsValidCoord] elbow: (%i %i %i)\n",elbow[0],elbow[1],elbow[2]);
      printf("[IsValidCoord] wrist: (%i %i %i)\n",wrist[0],wrist[1],wrist[2]);
      printf("[IsValidCoord] endeff: (%i %i %i)\n",endeff[0],endeff[1],endeff[2]);
      printf("[IsValidCoord] gripper: (%i %i %i)\n",gripper[0],gripper[1],gripper[2]);
  #endif
    return 0;
  }

  //check if joints are hitting obstacle
//   if(!isValidCell(endeff,6,Grid3D,vGrid) || !isValidCell(wrist,6,Grid3D,vGrid) || !isValidCell(elbow,6,Grid3D,vGrid))
//     return 0;

    //check the validity of the corresponding arm links (line segments)
/*  if(!IsValidLineSegment(shoulder, elbow, EnvROBARMCfg.upper_arm_radius, pTestedCells, Grid3D, vGrid) ||
      !IsValidLineSegment(elbow,wrist, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D, vGrid) ||
      !IsValidLineSegment(wrist,endeff, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D, vGrid))
  {
    if(pTestedCells == NULL)
    {
    #if DEBUG_VALID_COORD
			printf("[IsValidCoord] shoulder: (%i %i %i) --> elbow: (%i %i %i)\n",shoulder[0],shoulder[1],shoulder[2],elbow[0],elbow[1],elbow[2]);
      printf("[IsValidCoord] wrist: (%i %i %i)\n",wrist[0],wrist[1],wrist[2]);
      printf("[IsValidCoord] endeff: (%i %i %i)\n",endeff[0],endeff[1],endeff[2]);
      printf("[IsValidCoord] Arm link collision checking failed.\n");
    #endif
      return 0;
    }
    else
    {
    #if DEBUG_VALID_COORD
		  printf("[IsValidCoord] Arm link collision checking failed..\n");
    #endif
      retvalue = 0;
    }
  }
*/

	if(!IsValidLineSegment(shoulder, elbow, EnvROBARMCfg.upper_arm_radius, pTestedCells, Grid3D, vGrid))
	{
		if(pTestedCells == NULL)
		{
		#if DEBUG_VALID_COORD
			printf("[IsValidCoord] shoulder: (%i %i %i) --> elbow: (%i %i %i) is in collision\n",shoulder[0],shoulder[1],shoulder[2],elbow[0],elbow[1],elbow[2]);
		#endif
			return 0;
		}
		else
		{
		#if DEBUG_VALID_COORD
			printf("[IsValidCoord] Arm link collision checking failed..\n");
		#endif
			retvalue = 0;
		}
	}

	pTestedCells = NULL;

	if(!IsValidLineSegment(elbow,wrist, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D, vGrid))
	{
		if(pTestedCells == NULL)
		{
		#if DEBUG_VALID_COORD
			printf("[IsValidCoord] elbow: (%i %i %i) --> wrist: (%i %i %i) is in collision\n",elbow[0],elbow[1],elbow[2],wrist[0],wrist[1],wrist[2]);
		#endif
			return 0;
		}
		else
		{
		#if DEBUG_VALID_COORD
			printf("[IsValidCoord] elbow: (%i %i %i) --> wrist: (%i %i %i) is in collision\n",elbow[0],elbow[1],elbow[2],wrist[0],wrist[1],wrist[2]);
		#endif
		  retvalue = 0;
		}
	}

	pTestedCells = NULL;

	if(!IsValidLineSegment(wrist,endeff, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D, vGrid))
	{
		if(pTestedCells == NULL)
		{
		#if DEBUG_VALID_COORD
			printf("[IsValidCoord] wrist: (%i %i %i) --> endeff: (%i %i %i) is in collision\n",wrist[0],wrist[1],wrist[2],endeff[0],endeff[1],endeff[2]);
		#endif
			return 0;
		}
		else
		{
		#if DEBUG_VALID_COORD
			printf("[IsValidCoord] wrist: (%i %i %i) --> endeff: (%i %i %i) is in collision\n",wrist[0],wrist[1],wrist[2],endeff[0],endeff[1],endeff[2]);
		#endif
			retvalue = 0;
		}
	}

//   clock_t pm_start;

  //check gripper for collision
	if(!EnvROBARMCfg.exact_gripper_collision_checking)
  {
    //get position of gripper in torso_lift_link frame
    gripper[0] = (endeff[0] + (orientation[0][0]*EnvROBARMCfg.gripper_m[0] + orientation[0][1]*EnvROBARMCfg.gripper_m[1] + orientation[0][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
    gripper[1] = (endeff[1] + (orientation[1][0]*EnvROBARMCfg.gripper_m[0] + orientation[1][1]*EnvROBARMCfg.gripper_m[1] + orientation[1][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
    gripper[2] = (endeff[2] + (orientation[2][0]*EnvROBARMCfg.gripper_m[0] + orientation[2][1]*EnvROBARMCfg.gripper_m[1] + orientation[2][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;;

		if(gripper[0] >= grid_dims[0] || gripper[1] >= grid_dims[1] || gripper[2] >= grid_dims[2] || !isValidCell(gripper,EnvROBARMCfg.gripper_radius,Grid3D,vGrid))
      return 0;

    if(!IsValidLineSegment(endeff, gripper, EnvROBARMCfg.gripper_radius, pTestedCells, Grid3D, vGrid))
      return 0;
  }
  else
  {
    int num = 0;
		if(GetDistToClosestGoal(endeff_pos, &num) <= .3/EnvROBARMCfg.GridCellWidth)
    { 
//       printf("planning monitor: distance of endeff(%u %u %u) to goal #%i is %i\n", endeff_pos[0],endeff_pos[1],endeff_pos[2], num, GetDistToClosestGoal(endeff_pos, &num));
      // printf("checking %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6]);
//       pm_start = clock();
      if(!planning_monitor->areLinksValid(angles))
				return 0;
//       printf("planning monitor took %lf seconds to check the gripper for collisions\n",(clock() - pm_start) / (double)CLOCKS_PER_SEC);
    }
    else
    {
//       printf("basic check: distance of endeff(%u %u %u) to goal #%i is %i\n", endeff_pos[0],endeff_pos[1],endeff_pos[2], num, GetDistToClosestGoal(endeff_pos, &num));
      //get position of gripper in torso_lift_link frame
      gripper[0] = (endeff[0] + (orientation[0][0]*EnvROBARMCfg.gripper_m[0] + orientation[0][1]*EnvROBARMCfg.gripper_m[1] + orientation[0][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
      gripper[1] = (endeff[1] + (orientation[1][0]*EnvROBARMCfg.gripper_m[0] + orientation[1][1]*EnvROBARMCfg.gripper_m[1] + orientation[1][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
      gripper[2] = (endeff[2] + (orientation[2][0]*EnvROBARMCfg.gripper_m[0] + orientation[2][1]*EnvROBARMCfg.gripper_m[1] + orientation[2][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;;

			if(gripper[0] >= grid_dims[0] || gripper[1] >= grid_dims[1] || gripper[2] >= grid_dims[2] || !isValidCell(gripper,EnvROBARMCfg.gripper_radius,Grid3D,vGrid))
				return 0;

			pTestedCells = NULL;
      if(!IsValidLineSegment(endeff, gripper, EnvROBARMCfg.gripper_radius, pTestedCells, Grid3D, vGrid))
				return 0;
    }
  }

  return retvalue;
}

int EnvironmentROBARM3D::IsValidCoord(short unsigned int coord[], short unsigned int endeff_pos[3], short unsigned int wrist_pos[3], short unsigned int elbow_pos[3], double orientation[3][3], unsigned char &dist)
{
	int grid_dims[3] = {EnvROBARMCfg.EnvWidth_c, EnvROBARMCfg.EnvHeight_c, EnvROBARMCfg.EnvDepth_c};
	short unsigned int endeff[3] = {endeff_pos[0],endeff_pos[1],endeff_pos[2]};
	short unsigned int wrist[3] = {wrist_pos[0],wrist_pos[1],wrist_pos[2]};
	short unsigned int elbow[3] = {elbow_pos[0], elbow_pos[1], elbow_pos[2]};
	short unsigned int shoulder[3] = {EnvROBARMCfg.BaseX_c, EnvROBARMCfg.BaseY_c, EnvROBARMCfg.BaseZ_c};
	short unsigned int gripper[3];

  //use high resolution grid by default
	unsigned char*** Grid3D = EnvROBARMCfg.Grid3D;
	double grid_cell_m = EnvROBARMCfg.GridCellWidth;
	double angles[NUMOFLINKS];
	int retvalue = 1;
	vector<CELLV>* pTestedCells = NULL;
	ComputeContAngles(coord, angles);

	boost::shared_ptr<Voxel3d> vGrid = voxel_grid;

  //for low resolution collision checking
	if(EnvROBARMCfg.lowres_collision_checking)
	{
		grid_dims[0] = EnvROBARMCfg.LowResEnvWidth_c;
		grid_dims[1] = EnvROBARMCfg.LowResEnvHeight_c;
		grid_dims[2] = EnvROBARMCfg.LowResEnvDepth_c;
		Grid3D = EnvROBARMCfg.LowResGrid3D;
		grid_cell_m = EnvROBARMCfg.LowResGridCellWidth;

		HighResGrid2LowResGrid(endeff, endeff);
		HighResGrid2LowResGrid(wrist, wrist);
		HighResGrid2LowResGrid(elbow, elbow);
		HighResGrid2LowResGrid(shoulder, shoulder);
		vGrid = lowres_voxel_grid;
	}

    // check motor position limits
	if(EnvROBARMCfg.enforce_motor_limits)
	{
      // check if joint position angle is unreachable
		for (int i = 0; i < EnvROBARMCfg.num_joints; i++)
		{
			if(!EnvROBARMCfg.joints[i].cont)
			{
				// normalize angle
				if(angles[i] >= M_PI)
					angles[i] += -2.0*M_PI;

				if(angles[i] > EnvROBARMCfg.joints[i].positive_limit || angles[i] < EnvROBARMCfg.joints[i].negative_limit)
				{
#if DEBUG_VALID_COORD
					printf("[IsValidCoord] joint %i: %s limit failed: %.3f [%.3f, %3f]\n", i, EnvROBARMCfg.joints[i].name.c_str(), angles[i], EnvROBARMCfg.joints[i].negative_limit,EnvROBARMCfg.joints[i].positive_limit);
#endif
					return 0;
				}
			}
		}
	}

  //bounds checking on upper bound of map - should really only check end effector not other joints
	if(endeff[0] >= grid_dims[0] || endeff[1] >= grid_dims[1] || endeff[2] >= grid_dims[2] ||
			wrist[0] >= grid_dims[0] || wrist[1] >= grid_dims[1] || wrist[2] >= grid_dims[2] ||
			elbow[0] >= grid_dims[0] || elbow[1] >= grid_dims[1] || elbow[2] >= grid_dims[2])
	{
#if DEBUG_VALID_COORD
		printf("[IsValidCoord] One of the joints is outside of the grid dimensions (%i %i %i).\n", grid_dims[0],grid_dims[1],grid_dims[2]);
		printf("[IsValidCoord] elbow: (%i %i %i)\n",elbow[0],elbow[1],elbow[2]);
		printf("[IsValidCoord] wrist: (%i %i %i)\n",wrist[0],wrist[1],wrist[2]);
		printf("[IsValidCoord] endeff: (%i %i %i)\n",endeff[0],endeff[1],endeff[2]);
		printf("[IsValidCoord] gripper: (%i %i %i)\n",gripper[0],gripper[1],gripper[2]);
#endif
		return 0;
	}

  //check if joints are hitting obstacle
//   if(!isValidCell(endeff,6,Grid3D,vGrid) || !isValidCell(wrist,6,Grid3D,vGrid) || !isValidCell(elbow,6,Grid3D,vGrid))
//     return 0;

    //check the validity of the corresponding arm links (line segments)
/*  if(!IsValidLineSegment(shoulder, elbow, EnvROBARMCfg.upper_arm_radius, pTestedCells, Grid3D, vGrid) ||
	!IsValidLineSegment(elbow,wrist, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D, vGrid) ||
	!IsValidLineSegment(wrist,endeff, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D, vGrid))
	{
	if(pTestedCells == NULL)
	{
#if DEBUG_VALID_COORD
	printf("[IsValidCoord] shoulder: (%i %i %i) --> elbow: (%i %i %i)\n",shoulder[0],shoulder[1],shoulder[2],elbow[0],elbow[1],elbow[2]);
	printf("[IsValidCoord] wrist: (%i %i %i)\n",wrist[0],wrist[1],wrist[2]);
	printf("[IsValidCoord] endeff: (%i %i %i)\n",endeff[0],endeff[1],endeff[2]);
	printf("[IsValidCoord] Arm link collision checking failed.\n");
#endif
	return 0;
}
	else
	{
#if DEBUG_VALID_COORD
	printf("[IsValidCoord] Arm link collision checking failed..\n");
#endif
	retvalue = 0;
}
}
*/
	dist = IsValidLineSegment(shoulder, elbow, EnvROBARMCfg.upper_arm_radius, pTestedCells, Grid3D);
	if(dist <= EnvROBARMCfg.upper_arm_radius)
	{
		if(pTestedCells == NULL)
		{
#if DEBUG_VALID_COORD
			printf("[IsValidCoord] shoulder: (%i %i %i) --> elbow: (%i %i %i) is in collision\n",shoulder[0],shoulder[1],shoulder[2],elbow[0],elbow[1],elbow[2]);
#endif
			return 0;
		}
		else
		{
#if DEBUG_VALID_COORD
			printf("[IsValidCoord] Arm link collision checking failed..\n");
#endif
			retvalue = 0;
		}
	}

	pTestedCells = NULL;

	dist = IsValidLineSegment(elbow,wrist, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D);
	if(dist <= EnvROBARMCfg.forearm_radius)
	{
		if(pTestedCells == NULL)
		{
#if DEBUG_VALID_COORD
			printf("[IsValidCoord] elbow: (%i %i %i) --> wrist: (%i %i %i) is in collision\n",elbow[0],elbow[1],elbow[2],wrist[0],wrist[1],wrist[2]);
#endif
			return 0;
		}
		else
		{
#if DEBUG_VALID_COORD
			printf("[IsValidCoord] elbow: (%i %i %i) --> wrist: (%i %i %i) is in collision\n",elbow[0],elbow[1],elbow[2],wrist[0],wrist[1],wrist[2]);
#endif
			retvalue = 0;
		}
	}

	pTestedCells = NULL;

	dist = IsValidLineSegment(wrist,endeff, EnvROBARMCfg.forearm_radius, pTestedCells, Grid3D);
	if(dist <= EnvROBARMCfg.forearm_radius)
	{
		if(pTestedCells == NULL)
		{
#if DEBUG_VALID_COORD
			printf("[IsValidCoord] wrist: (%i %i %i) --> endeff: (%i %i %i) is in collision\n",wrist[0],wrist[1],wrist[2],endeff[0],endeff[1],endeff[2]);
#endif
			return 0;
		}
		else
		{
#if DEBUG_VALID_COORD
			printf("[IsValidCoord] wrist: (%i %i %i) --> endeff: (%i %i %i) is in collision\n",wrist[0],wrist[1],wrist[2],endeff[0],endeff[1],endeff[2]);
#endif
			retvalue = 0;
		}
	}

//   clock_t pm_start;

  //check gripper for collision
	if(!EnvROBARMCfg.exact_gripper_collision_checking)
	{
    //get position of gripper in torso_lift_link frame
		gripper[0] = (endeff[0] + (orientation[0][0]*EnvROBARMCfg.gripper_m[0] + orientation[0][1]*EnvROBARMCfg.gripper_m[1] + orientation[0][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
		gripper[1] = (endeff[1] + (orientation[1][0]*EnvROBARMCfg.gripper_m[0] + orientation[1][1]*EnvROBARMCfg.gripper_m[1] + orientation[1][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
		gripper[2] = (endeff[2] + (orientation[2][0]*EnvROBARMCfg.gripper_m[0] + orientation[2][1]*EnvROBARMCfg.gripper_m[1] + orientation[2][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;;

		if(gripper[0] >= grid_dims[0] || gripper[1] >= grid_dims[1] || gripper[2] >= grid_dims[2] || !isValidCell(gripper,EnvROBARMCfg.gripper_radius,Grid3D,vGrid))
			return 0;

		dist = IsValidLineSegment(endeff, gripper, EnvROBARMCfg.gripper_radius, pTestedCells, Grid3D);
		if(dist <= EnvROBARMCfg.gripper_radius)
			return 0;
	}
	else
	{
		int num = 0;
		if(GetDistToClosestGoal(endeff_pos, &num) <= .3/EnvROBARMCfg.GridCellWidth)
		{ 
//       printf("planning monitor: distance of endeff(%u %u %u) to goal #%i is %i\n", endeff_pos[0],endeff_pos[1],endeff_pos[2], num, GetDistToClosestGoal(endeff_pos, &num));
      // printf("checking %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",angles[0],angles[1],angles[2],angles[3],angles[4],angles[5],angles[6]);
//       pm_start = clock();
			if(!planning_monitor->areLinksValid(angles))
				return 0;
//       printf("planning monitor took %lf seconds to check the gripper for collisions\n",(clock() - pm_start) / (double)CLOCKS_PER_SEC);
		}
		else
		{
//       printf("basic check: distance of endeff(%u %u %u) to goal #%i is %i\n", endeff_pos[0],endeff_pos[1],endeff_pos[2], num, GetDistToClosestGoal(endeff_pos, &num));
      //get position of gripper in torso_lift_link frame
			gripper[0] = (endeff[0] + (orientation[0][0]*EnvROBARMCfg.gripper_m[0] + orientation[0][1]*EnvROBARMCfg.gripper_m[1] + orientation[0][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
			gripper[1] = (endeff[1] + (orientation[1][0]*EnvROBARMCfg.gripper_m[0] + orientation[1][1]*EnvROBARMCfg.gripper_m[1] + orientation[1][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
			gripper[2] = (endeff[2] + (orientation[2][0]*EnvROBARMCfg.gripper_m[0] + orientation[2][1]*EnvROBARMCfg.gripper_m[1] + orientation[2][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;;

			if(gripper[0] >= grid_dims[0] || gripper[1] >= grid_dims[1] || gripper[2] >= grid_dims[2] || !isValidCell(gripper,EnvROBARMCfg.gripper_radius,Grid3D,vGrid))
				return 0;

			pTestedCells = NULL;
			dist = IsValidLineSegment(endeff, gripper, EnvROBARMCfg.gripper_radius, pTestedCells, Grid3D);
			if(dist <= EnvROBARMCfg.gripper_radius)
				return 0;
		}
	}

	return retvalue;
}

/*
int EnvironmentROBARM3D::IsValidCoord(const short unsigned int coord[], const std::vector<std::vector<short unsigned int> > &joints, double orientation[3][3], char ***Grid, const short unsigned int grid_dims[])
{
  double *angles = new double [EnvROBARMCfg.num_joints];
  int retvalue = 1;
  short unsigned int gripper[3];
  vector<CELLV>* pTestedCells = NULL;
  ComputeContAngles(coord, angles);

  int radius = 6;

  // check motor position limits
  if(EnvROBARMCfg.enforce_motor_limits)
{
    //convert angles from positive values in radians (from 0->6.28) to centered around 0
    for (int i = 0; i < EnvROBARMCfg.num_joints; i++)
{
      if(angles[i] >= PI_CONST)
	angles[i] = -2.0*PI_CONST + angles[i];

      // check if joint position angle is unreachable
      if (angles[i] > EnvROBARMCfg.PosMotorLimits[i] || angles[i] < EnvROBARMCfg.NegMotorLimits[i])
	return 0;
}
}

  //get position of gripper in torso_lift_link frame
//   gripper[0] = (endeff[0] + (orientation[0][0]*EnvROBARMCfg.gripper_m[0] + orientation[0][1]*EnvROBARMCfg.gripper_m[1] + orientation[0][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
//   gripper[1] = (endeff[1] + (orientation[1][0]*EnvROBARMCfg.gripper_m[0] + orientation[1][1]*EnvROBARMCfg.gripper_m[1] + orientation[1][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;
//   gripper[2] = (endeff[2] + (orientation[2][0]*EnvROBARMCfg.gripper_m[0] + orientation[2][1]*EnvROBARMCfg.gripper_m[1] + orientation[2][2]*EnvROBARMCfg.gripper_m[2]) / grid_cell_m) + 0.5;;

  for(unsigned int a = 0; a < joints.size(); ++a)
{
    //check if joint is out of bounds
    if(joints[a][0] >= grid_dims[0] || joints[a][1] >= grid_dims[1] || joints[a][2] >= grid_dims[2])
      return 0;

    //check if joint is hitting obstacle
    if(!isValidCell(joints[a], radius, Grid))
      return 0;

    //check if link is hitting obstacle
    if(a > 0)
{
      if(!IsValidLineSegment(joints[a-1], joints[a], radius, Grid, grid_dims, pTestedCells))
{
	if(pTestedCells == NULL)
	    return 0;
          else
            retvalue = 0;
}
}
}

  delete angles;

  // printf("[IsValidCoord] endeff: (%u %u %u) gripper (%u %u %u) \n",endeff[0],endeff[1],endeff[2],gripper[0],gripper[1],gripper[2]);
  return retvalue;
}
*/

void EnvironmentROBARM3D::initPlanningMonitor(sbpl_arm_planner_node::pm_wrapper *pm)
{
  planning_monitor = pm;
}

bool EnvironmentROBARM3D::isValidJointConfiguration(const double * angles)
{
  short unsigned int endeff[3], wrist[3], elbow[3], coord[NUMOFLINKS];
  double orientation[3][3];

  ComputeEndEffectorPos(angles,endeff,wrist,elbow,orientation);

  ComputeCoord(angles, coord);

  return IsValidCoord(coord,endeff,wrist,elbow,orientation);
}

bool EnvironmentROBARM3D::isPathValid(double** path, int num_waypoints)
{
  double angles[NUMOFLINKS], orientation[3][3];
  short unsigned int endeff[3], wrist[3], elbow[3], coord[NUMOFLINKS];

    //loop through each waypoint of the path and check if it's valid
  for(int i = 0; i < num_waypoints; i++)
  {
        //compute FK
    ComputeEndEffectorPos(angles, endeff, wrist, elbow, orientation);

    for(int k = 0; k < NUMOFLINKS; k++)
    {
      angles[k] = path[i][k]*(180.0/PI_CONST);

      if (angles[k] < 0)
	     angles[k] += 360;
    }

        //compute coords
    ComputeCoord(angles, coord);

        //check if valid
    if(!IsValidCoord(coord,endeff, wrist, elbow, orientation))
    {
      printf("[isPathValid] Path is invalid.\n");
      return false;
    }
  }

  printf("Path is valid.\n");
  return true;
}

