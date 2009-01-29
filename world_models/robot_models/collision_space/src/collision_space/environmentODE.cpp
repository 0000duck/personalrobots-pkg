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

/** \author Ioan Sucan */

#include <collision_space/environmentODE.h>
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <map>

void collision_space::EnvironmentModelODE::freeMemory(void)
{ 
    for (unsigned int i = 0 ; i < m_modelsGeom.size() ; ++i)
    {
	for (unsigned int j = 0 ; j < m_modelsGeom[i].linkGeom.size() ; ++j)
	    delete m_modelsGeom[i].linkGeom[j];
	dSpaceDestroy(m_modelsGeom[i].space);
    }    
    if (m_space)
	dSpaceDestroy(m_space);
    if (m_spaceBasicGeoms)
	dSpaceDestroy(m_spaceBasicGeoms);
}

unsigned int collision_space::EnvironmentModelODE::addRobotModel(planning_models::KinematicModel *model, const std::vector<std::string> &links, double scale)
{
    unsigned int id = collision_space::EnvironmentModel::addRobotModel(model, links, scale);
    
    if (m_modelsGeom.size() <= id)
    {
	m_modelsGeom.resize(id + 1);
	m_modelsGeom[id].space = dHashSpaceCreate(0);	
	m_modelsGeom[id].scale = scale;
    }

    std::map<std::string, bool> exists;
    for (unsigned int i = 0 ; i < links.size() ; ++i)
	exists[links[i]] = true;
    
    for (unsigned int j = 0 ; j < m_models[id]->getRobotCount() ; ++j)
    {
	planning_models::KinematicModel::Robot *robot = m_models[id]->getRobot(j);
	for (unsigned int i = 0 ; i < robot->links.size() ; ++i)
	{
	    /* skip this link if we have no geometry or if the link
	       name is not specified as enabled for collision
	       checking */
	    if (!robot->links[i]->shape)
		continue;
	    if (exists.find(robot->links[i]->name) == exists.end())
		continue;
	    
	    kGeom *kg = new kGeom();
	    kg->link = robot->links[i];
	    kg->enabled = true;
	    dGeomID g = createODEGeom(m_modelsGeom[id].space, robot->links[i]->shape, scale);
	    assert(g);
	    kg->geom.push_back(g);
	    for (unsigned int k = 0 ; k < kg->link->attachedBodies.size() ; ++k)
	    {
		dGeomID ga = createODEGeom(m_modelsGeom[id].space, kg->link->attachedBodies[k]->shape, scale);
		assert(ga);
		kg->geom.push_back(ga);
	    }
	    m_modelsGeom[id].linkGeom.push_back(kg);
	}
    }
    return id;
}

dGeomID collision_space::EnvironmentModelODE::createODEGeom(dSpaceID space, planning_models::KinematicModel::Shape *shape, double scale) const
{
    dGeomID g = NULL;
    switch (shape->type)
    {
    case planning_models::KinematicModel::Shape::SPHERE:
	{
	    g = dCreateSphere(space, static_cast<planning_models::KinematicModel::Sphere*>(shape)->radius * scale);
	}
	break;
    case planning_models::KinematicModel::Shape::BOX:
	{
	    const double *size = static_cast<planning_models::KinematicModel::Box*>(shape)->size;
	    g = dCreateBox(space, size[0] * scale, size[1] * scale, size[2] * scale);
	}	
	break;
    case planning_models::KinematicModel::Shape::CYLINDER:
	{
	    g = dCreateCylinder(space, static_cast<planning_models::KinematicModel::Cylinder*>(shape)->radius * scale,
				static_cast<planning_models::KinematicModel::Cylinder*>(shape)->length * scale);
	}
	break;
    default:
	break;
    }
    return g;
}

void collision_space::EnvironmentModelODE::updateGeom(dGeomID geom, btTransform &pose) const
{
    btVector3 pos = pose.getOrigin();
    dGeomSetPosition(geom, pos.getX(), pos.getY(), pos.getZ());
    btQuaternion quat = pose.getRotation();
    dQuaternion q; 
    q[0] = quat.getW(); q[1] = quat.getX(); q[2] = quat.getY(); q[3] = quat.getZ();
    dGeomSetQuaternion(geom, q);
}

void collision_space::EnvironmentModelODE::updateAttachedBodies(unsigned int model_id)
{
    const unsigned int n = m_modelsGeom[model_id].linkGeom.size();    
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	kGeom *kg = m_modelsGeom[model_id].linkGeom[i];
	
	/* clear previosly attached bodies */
	for (unsigned int k = 1 ; k < kg->geom.size() ; ++k)
	    dGeomDestroy(kg->geom[k]);
	kg->geom.resize(1);
	
	/* create new set of attached bodies */	
	const unsigned int nab = kg->link->attachedBodies.size();
	for (unsigned int k = 0 ; k < nab ; ++k)
	{
	    dGeomID ga = createODEGeom(m_modelsGeom[model_id].space, kg->link->attachedBodies[k]->shape, m_modelsGeom[model_id].scale);
	    assert(ga);
	    kg->geom.push_back(ga);
	}
    }
}

void collision_space::EnvironmentModelODE::updateRobotModel(unsigned int model_id)
{ 
    const unsigned int n = m_modelsGeom[model_id].linkGeom.size();
    
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	updateGeom(m_modelsGeom[model_id].linkGeom[i]->geom[0], m_modelsGeom[model_id].linkGeom[i]->link->globalTrans);
	const unsigned int nab = m_modelsGeom[model_id].linkGeom[i]->link->attachedBodies.size();
	for (unsigned int k = 0 ; k < nab ; ++k)
	    updateGeom(m_modelsGeom[model_id].linkGeom[i]->geom[k + 1], m_modelsGeom[model_id].linkGeom[i]->link->attachedBodies[k]->globalTrans);
    }    
}

void collision_space::EnvironmentModelODE::ODECollide2::registerSpace(dSpaceID space)
{
    int n = dSpaceGetNumGeoms(space);
    for (int i = 0 ; i < n ; ++i)
	registerGeom(dSpaceGetGeom(space, i));
}

void collision_space::EnvironmentModelODE::ODECollide2::registerGeom(dGeomID geom)
{
    Geom *g = new Geom();
    g->id = geom;
    dGeomGetAABB(geom, g->aabb);
    m_geomsX.push_back(g);
    m_geomsY.push_back(g);
    m_geomsZ.push_back(g);
    m_setup = false;
}
	
void collision_space::EnvironmentModelODE::ODECollide2::clear(void)
{
    for (unsigned int i = 0 ; i < m_geomsX.size() ; ++i)
	delete m_geomsX[i];
    m_geomsX.clear();
    m_geomsY.clear();
    m_geomsZ.clear();
    m_setup = false;
}

void collision_space::EnvironmentModelODE::ODECollide2::setup(void)
{
    if (!m_setup)
    {
	std::sort(m_geomsX.begin(), m_geomsX.end(), SortByXLow());
	std::sort(m_geomsY.begin(), m_geomsY.end(), SortByYLow());
	std::sort(m_geomsZ.begin(), m_geomsZ.end(), SortByZLow());
	m_setup = true;
    }	    
}

void collision_space::EnvironmentModelODE::ODECollide2::checkColl(std::vector<Geom*>::iterator posStart, std::vector<Geom*>::iterator posEnd,
								  Geom *g, void *data, dNearCallback *nearCallback)
{
    /* posStart now identifies the first geom which has an AABB
       that could overlap the AABB of geom on the X axis. posEnd
       identifies the first one that cannot overlap. */
    
    while (posStart < posEnd)
    {
	/* if the boxes are not disjoint along Y, Z, check further */
	if (!((*posStart)->aabb[2] > g->aabb[3] ||
	      (*posStart)->aabb[3] < g->aabb[2] ||
	      (*posStart)->aabb[4] > g->aabb[5] ||
	      (*posStart)->aabb[5] < g->aabb[4]))
	    dSpaceCollide2(g->id, (*posStart)->id, data, nearCallback);
	posStart++;
    }
}

void collision_space::EnvironmentModelODE::ODECollide2::collide(dGeomID geom, void *data, dNearCallback *nearCallback)
{
    static const int CUTOFF = 100;

    assert(m_setup);

    Geom g;
    g.id = geom;
    dGeomGetAABB(geom, g.aabb);
    
    std::vector<Geom*>::iterator posStart1 = std::lower_bound(m_geomsX.begin(), m_geomsX.end(), &g, SortByXTest());
    if (posStart1 != m_geomsX.end())
    {
	std::vector<Geom*>::iterator posEnd1 = std::upper_bound(posStart1, m_geomsX.end(), &g, SortByXTest());
	int                          d1      = posEnd1 - posStart1;
	
	/* Doing two binary searches on the sorted-by-y array takes
	   log(n) time, which should be around 12 steps. Each step
	   should be just a few ops, so a cut-off like 100 is
	   appropriate. */
	if (d1 > CUTOFF)
	{
	    std::vector<Geom*>::iterator posStart2 = std::lower_bound(m_geomsY.begin(), m_geomsY.end(), &g, SortByYTest());
	    if (posStart2 != m_geomsY.end())
	    {
		std::vector<Geom*>::iterator posEnd2 = std::upper_bound(posStart2, m_geomsY.end(), &g, SortByYTest());
		int                          d2      = posEnd2 - posStart2;
		
		if (d2 > CUTOFF)
		{
		    std::vector<Geom*>::iterator posStart3 = std::lower_bound(m_geomsZ.begin(), m_geomsZ.end(), &g, SortByZTest());
		    if (posStart3 != m_geomsZ.end())
		    {
			std::vector<Geom*>::iterator posEnd3 = std::upper_bound(posStart3, m_geomsZ.end(), &g, SortByZTest());
			int                          d3      = posEnd3 - posStart3;
			if (d3 > CUTOFF)
			{
			    if (d3 <= d2 && d3 <= d1)
				checkColl(posStart3, posEnd3, &g, data, nearCallback);
			    else
				if (d2 <= d3 && d2 <= d1)
				    checkColl(posStart2, posEnd2, &g, data, nearCallback);
				else
				    checkColl(posStart1, posEnd1, &g, data, nearCallback);
			}
			else
			    checkColl(posStart3, posEnd3, &g, data, nearCallback);   
		    }
		}
		else
		    checkColl(posStart2, posEnd2, &g, data, nearCallback);   
	    }
	}
	else 
	    checkColl(posStart1, posEnd1, &g, data, nearCallback);
    }
}

dSpaceID collision_space::EnvironmentModelODE::getODESpace(void) const
{
    return m_space;
}

dSpaceID collision_space::EnvironmentModelODE::getODEBasicGeomSpace(void) const
{
    return m_spaceBasicGeoms;
}

dSpaceID collision_space::EnvironmentModelODE::getModelODESpace(unsigned int model_id) const
{
    return m_modelsGeom[model_id].space;    
}

struct CollisionData
{
    CollisionData(void)
    {
	done = false;
	collides = false;
	max_contacts = 0;
	contacts = NULL;
	link1 = link2 = NULL;
    }
    
    bool                                                        done;
    
    bool                                                        collides;
    unsigned int                                                max_contacts;
    std::vector<collision_space::EnvironmentModelODE::Contact> *contacts;
    
    planning_models::KinematicModel::Link                      *link1;
    planning_models::KinematicModel::Link                      *link2;
};

static void nearCallbackFn(void *data, dGeomID o1, dGeomID o2)
{
    CollisionData *cdata = reinterpret_cast<CollisionData*>(data);

    if (cdata->done)
	return;
    
    if (cdata->contacts)
    {
	static const int MAX_CONTACTS = 3;
	dContact contact[MAX_CONTACTS];
	int numc = dCollide (o1, o2, MAX_CONTACTS,
			     &contact[0].geom, sizeof(dContact));
	if (numc)
	{
	    cdata->collides = true;
	    for (int i = 0 ; i < numc ; ++i)
	    {
		if (cdata->contacts->size() < cdata->max_contacts)
		{
		    collision_space::EnvironmentModelODE::Contact add;
		    
		    add.pos.setX(contact[i].geom.pos[0]);
		    add.pos.setY(contact[i].geom.pos[1]);
		    add.pos.setZ(contact[i].geom.pos[2]);
		    
		    add.normal.setX(contact[i].geom.normal[0]);
		    add.normal.setY(contact[i].geom.normal[1]);
		    add.normal.setZ(contact[i].geom.normal[2]);
		    
		    add.depth = contact[i].geom.depth;
		    
		    add.link1 = cdata->link1;
		    add.link2 = cdata->link2;
		    
		    cdata->contacts->push_back(add);
		}
		else
		    break;
	    }
	}
	if (cdata->contacts->size() >= cdata->max_contacts)
	    cdata->done = true;
    }
    else
    {
	static const int MAX_CONTACTS = 1;    
	dContact contact[MAX_CONTACTS];
	int numc = dCollide (o1, o2, MAX_CONTACTS,
			     &contact[0].geom, sizeof(dContact));
	if (numc)
	{
	    cdata->collides = true;
	    cdata->done = true;
	}
    }
}

bool collision_space::EnvironmentModelODE::getCollisionContacts(unsigned int model_id, std::vector<Contact> &contacts, unsigned int max_count)
{
    CollisionData cdata;
    cdata.contacts = &contacts;
    cdata.max_contacts = max_count;
    contacts.clear();
    testCollision(model_id, reinterpret_cast<void*>(&cdata));
    return cdata.collides;
}

bool collision_space::EnvironmentModelODE::isCollision(unsigned int model_id)
{
    CollisionData cdata;
    testCollision(model_id, reinterpret_cast<void*>(&cdata));
    return cdata.collides;
}

void collision_space::EnvironmentModelODE::testCollision(unsigned int model_id, void *data)
{
    CollisionData *cdata = reinterpret_cast<CollisionData*>(data);
    
    /* check self collision */
    if (m_selfCollision)
    {
	for (int i = m_modelsGeom[model_id].selfCollision.size() - 1 ; i >= 0 && !cdata->done ; --i)
	{
	    const std::vector<unsigned int> &vec = m_modelsGeom[model_id].selfCollision[i];
	    unsigned int n = vec.size();
	    
	    for (unsigned int j = 0 ; j < n && !cdata->done ; ++j)
	    {
		cdata->link1 = m_modelsGeom[model_id].linkGeom[vec[j]]->link;
		const unsigned int njg = m_modelsGeom[model_id].linkGeom[vec[j]]->geom.size();
		
		for (unsigned int k = j + 1 ; k < n && !cdata->done; ++k)
		{
		    // dSpaceCollide2 expects AABBs to be computed, so
		    // we force that by calling dGeomGetAABB. Since we
		    // get the data anyway, we attempt to speed things
		    // up using it.
		    
		    const unsigned int nkg = m_modelsGeom[model_id].linkGeom[vec[k]]->geom.size();
		    cdata->link2 = m_modelsGeom[model_id].linkGeom[vec[k]]->link;

		    /* this will account for attached bodies as well */
		    for (unsigned int jg = 0 ; jg < njg && !cdata->done; ++jg)
			for (unsigned int kg = 0 ; kg < nkg && !cdata->done; ++kg)
			{
			    dGeomID g1 = m_modelsGeom[model_id].linkGeom[vec[j]]->geom[jg];
			    dGeomID g2 = m_modelsGeom[model_id].linkGeom[vec[k]]->geom[kg];
			    
			    dReal aabb1[6], aabb2[6];		    
			    dGeomGetAABB(g1, aabb1);
			    dGeomGetAABB(g2, aabb2);
			    
			    if (!(aabb1[2] > aabb2[3] ||
				  aabb1[3] < aabb2[2] ||
				  aabb1[4] > aabb2[5] ||
				  aabb1[5] < aabb2[4])) 
				dSpaceCollide2(g1, g2, data, nearCallbackFn);
			    
			    if (cdata->collides && m_verbose)
				printf("Self-collision between '%s' and '%s'\n",
				       m_modelsGeom[model_id].linkGeom[vec[j]]->link->name.c_str(), m_modelsGeom[model_id].linkGeom[vec[k]]->link->name.c_str());
			}
		}
	    }
	}
    }
    
    /* check collision with standalone ode bodies */

    if (!cdata->done)
    {
	cdata->link2 = NULL;
	for (int i = m_modelsGeom[model_id].linkGeom.size() - 1 ; i >= 0 && !cdata->done ; --i)
	{
	    /* skip disabled bodies */
	    if (!m_modelsGeom[model_id].linkGeom[i]->enabled)
		continue;
	    const unsigned int ng = m_modelsGeom[model_id].linkGeom[i]->geom.size();
	    cdata->link1 = m_modelsGeom[model_id].linkGeom[i]->link;	    

	    for (unsigned int ig = 0 ; ig < ng && !cdata->done ; ++ig)
	    {
		dGeomID g1 = m_modelsGeom[model_id].linkGeom[i]->geom[ig];
		dReal aabb1[6];
		dGeomGetAABB(g1, aabb1);
		for (int j = m_basicGeoms.size() - 1 ; j >= 0 ; --j)
		{
		    dGeomID g2 = m_basicGeoms[j];
		    dReal aabb2[6];
		    dGeomGetAABB(g2, aabb2);
		    
		    if (!(aabb1[2] > aabb2[3] ||
			  aabb1[3] < aabb2[2] ||
			  aabb1[4] > aabb2[5] ||
			  aabb1[5] < aabb2[4]))
			dSpaceCollide2(g1, g2, data, nearCallbackFn);
		    
		    if (cdata->collides && m_verbose)
			printf("Collision between static body and link '%s'\n",
			       m_modelsGeom[model_id].linkGeom[i]->link->name.c_str());
		}
	    }
	}	
    }
    
    /* check collision with pointclouds */
    
    if (!cdata->done)
    {	
	cdata->link2 = NULL;
	m_collide2.setup();
	for (int i = m_modelsGeom[model_id].linkGeom.size() - 1 ; i >= 0 && !cdata->done ; --i)
	    if (m_modelsGeom[model_id].linkGeom[i]->enabled)
	    {
		const unsigned int ng = m_modelsGeom[model_id].linkGeom[i]->geom.size();
		cdata->link1 = m_modelsGeom[model_id].linkGeom[i]->link;	    
		for (unsigned int ig = 0 ; ig < ng && !cdata->done ; ++ig)
		{
		    m_collide2.collide(m_modelsGeom[model_id].linkGeom[i]->geom[ig], data, nearCallbackFn);
		    if (cdata->collides && m_verbose)
			printf("Collision between dynamic body and link '%s'\n",
			       m_modelsGeom[model_id].linkGeom[i]->link->name.c_str());
		}
	    }
    }
    
    cdata->done = true;
}

void collision_space::EnvironmentModelODE::addPointCloud(unsigned int n, const double *points)
{
    for (unsigned int i = 0 ; i < n ; ++i)
    {
	unsigned int i4 = i * 4;
	dGeomID g = dCreateSphere(m_space, points[i4 + 3]);
	dGeomSetPosition(g, points[i4], points[i4 + 1], points[i4 + 2]);
	m_collide2.registerGeom(g);
    }
    m_collide2.setup();
}

void collision_space::EnvironmentModelODE::addStaticPlane(double a, double b, double c, double d)
{
    dGeomID g = dCreatePlane(m_spaceBasicGeoms, a, b, c, d);
    m_basicGeoms.push_back(g);
}

void collision_space::EnvironmentModelODE::clearObstacles(void)
{
    m_collide2.clear();
    if (m_space)
	dSpaceDestroy(m_space);
    m_space = dHashSpaceCreate(0);
    m_collide2.setup();
}

void collision_space::EnvironmentModelODE::addSelfCollisionGroup(unsigned int model_id, std::vector<std::string> &links)
{
    if (model_id < m_modelsGeom.size())
    {
	unsigned int pos = m_modelsGeom[model_id].selfCollision.size();
	m_modelsGeom[model_id].selfCollision.resize(pos + 1);
	for (unsigned int i = 0 ; i < links.size() ; ++i)
	{
	    for (unsigned int j = 0 ; j < m_modelsGeom[model_id].linkGeom.size() ; ++j)
		if (m_modelsGeom[model_id].linkGeom[j]->link->name == links[i])
		    m_modelsGeom[model_id].selfCollision[pos].push_back(j);
	}
    }
}

int collision_space::EnvironmentModelODE::setCollisionCheck(unsigned int model_id, const std::string &link, bool state)
{ 
    int result = -1;
    if (model_id < m_modelsGeom.size())
    {
	for (unsigned int j = 0 ; j < m_modelsGeom[model_id].linkGeom.size() ; ++j)
	{
	    if (m_modelsGeom[model_id].linkGeom[j]->link->name == link)
	    {
		result = m_modelsGeom[model_id].linkGeom[j]->enabled ? 1 : 0;
		m_modelsGeom[model_id].linkGeom[j]->enabled = state;
		break;
	    }
	}
    }
    return result;    
}
