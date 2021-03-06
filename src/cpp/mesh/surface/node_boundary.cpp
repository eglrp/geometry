#include "node_boundary.h"
#include <geometry/octree/octnode.h>
#include <geometry/octree/octtopo.h>
#include <geometry/poly_intersect/poly2d.h>
#include <mesh/partition/node_set.h>
#include <util/progress_bar.h>
#include <util/error_codes.h>
#include <util/tictoc.h>
#include <Eigen/Dense>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <string>
#include <vector>
#include <map>
#include <float.h>

/**
 * @file   node_boundary.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 * @brief  Classes used to define boundary nodes in octrees
 *
 * @section DESCRIPTION
 *
 * This file contains classes used to formulate the set of boundary
 * nodes in a given octree.  Boundary nodes are nodes that are labeled
 * interior, but are adjacent to exterior nodes.  The "is_interior()"
 * function of octdata objects is used to determine if the nodes
 * are interior or exterior.
 */

using namespace octtopo;
using namespace std;
using namespace Eigen;

/* the following constants are used for computations in this file */

#define APPROX_ZERO  0.000000001

/*--------------------------*/
/* function implementations */
/*--------------------------*/
		
node_boundary_t::node_boundary_t()
{
	this->scheme = SEG_ALL;
}

node_boundary_t::~node_boundary_t()
{ 
	this->clear();
}

int node_boundary_t::populate(const octtopo_t& topo, SEG_SCHEME segscheme)
{
	tictoc_t clk;
	int ret;

	/* set the segmentation scheme */
	this->scheme = segscheme;

	/* populate the faces structure */
	tic(clk);
	ret = this->populate_faces(topo);
	if(ret)
		return PROPEGATE_ERROR(-1, ret);

	/* populate the linkages between faces */
	ret = this->populate_face_linkages(topo);
	if(ret)
		return PROPEGATE_ERROR(-2, ret);

	/* success */
	toc(clk, "Populating boundary faces");
	return 0;
}
		
bool node_boundary_t::node_is_interior(octnode_t* node) const
{
	/* whether this node is interior or exterior depends
	 * on the current segmentation scheme for this boundary
	 * object. */
	switch(this->scheme)
	{
		default:
		case SEG_ALL:
			/* this is the default segmentation for
			 * the octnodes */
			if(node == NULL)
				return false;
			if(node->data == NULL)
				return false;
			return node->data->is_interior();

		case SEG_OBJECTS:
			/* we only want to model objects in
			 * the environment to be exterior */
			if(node == NULL)
				return true;
			if(node->data == NULL)
				return true;
			if(node->data->get_fp_room() < 0)
				return true;
			return node->data->is_interior();

		case SEG_ROOM:
			/* we want to treat objects in the environment
			 * as 'interior', and only treat large fixtures
			 * as 'exterior' */
			if(node == NULL)
				return false;
			if(node->data == NULL)
				return false;
			if(node->data->get_fp_room() >= 0)
				return true;
			return node->data->is_interior();
	}
}
		
int node_boundary_t::get_nearby_faces(const octtopo_t& topo,
		octnode_t* node, faceset_t& nfs) const
{
	octneighbors_t edges;
	vector<octnode_t*> neighs;
	pair<nodefacemap_t::const_iterator, 
		nodefacemap_t::const_iterator> range;
	nodefacemap_t::const_iterator it;
	size_t fi, i, n;
	CUBE_FACE f;
	int ret;

	/* verify input */
	if(node == NULL)
		return 0; /* no neighbors */

	/* find the node in the topology */
	ret = topo.get(node, edges);
	if(ret)
		return PROPEGATE_ERROR(-1, ret);

	/* iterate over all neighboring nodes */
	for(fi = 0; fi < NUM_FACES_PER_CUBE; fi++)
	{
		f = octtopo::all_cube_faces[fi];

		/* get the neighboring nodes on this face */
		neighs.clear();
		edges.get(f, neighs);

		/* iterate over neighboring nodes */
		n = neighs.size();
		for(i = 0; i < n; i++)
		{
			/* get the faces that abut the given node */
			range = this->node_face_map.equal_range(neighs[i]);
			for(it = range.first; it != range.second; it++)
				nfs.insert(it->second);
		}
	}

	/* success */
	return 0;
}
		
pair<faceset_t::const_iterator, faceset_t::const_iterator>
		node_boundary_t::get_neighbors(const node_face_t& f) const
{
	facemap_t::const_iterator fit;

	/* get the info for this face */
	fit = this->faces.find(f);
	if(fit == this->faces.end())
	{
		/* return an empty interval using a dummy set */
		faceset_t dummy;
		return pair<faceset_t::const_iterator,
			faceset_t::const_iterator>(dummy.end(), 
						dummy.end());
	}

	/* return the iterators to the neighbor set for this face */
	return pair<faceset_t::const_iterator,
			faceset_t::const_iterator>(
				fit->second.neighbors.begin(),
				fit->second.neighbors.end());
}
		
int node_boundary_t::writeobj(const string& filename) const
{
	facemap_t::const_iterator fit;
	progress_bar_t progbar;
	ofstream outfile;
	size_t i, n;

	/* open file for writing */
	outfile.open(filename.c_str());
	if(!(outfile.is_open()))
		return -1;

	/* prepare progress bar */
	progbar.set_name("Writing OBJ");
	i = 0; n = this->faces.size();

	/* iterate over faces */
	for(fit = this->faces.begin(); fit != this->faces.end(); fit++)
	{
		/* inform user of progress */
		progbar.update(i++, n);

		/* write face */
		fit->first.writeobj(outfile);
	}

	/* success */
	progbar.clear();
	outfile.close();
	return 0;
}

int node_boundary_t::writeobj_cliques(const std::string& filename) const
{
	facemap_t::const_iterator fit;
	faceset_t::const_iterator nit;
	ofstream outfile;
	Vector3d p, norm;
	double halfwidth;
	int num_verts;
	progress_bar_t progbar;
	size_t i, n;

	/* open file for writing */
	outfile.open(filename.c_str());
	if(!(outfile.is_open()))
		return -1;

	/* prepare progress bar */
	progbar.set_name("Writing OBJ");
	i = 0; n = this->faces.size();

	/* write out the centers of all found faces */
	for(fit = this->faces.begin(); fit != this->faces.end(); fit++)
	{
		/* inform user of progress */
		progbar.update(i++, n);

		/* store index.  Note that OBJ files index
		 * starting at 1, so should we. */
		fit->first.get_center(p);
		if(fit->first.exterior == NULL)
			outfile << "v " << p.transpose() 
			        << " 255 0 0" << endl;
		else
			outfile << "v " << p.transpose()
			        << " 255 255 255" << endl;
		
		/* write point slightly above face */
		octtopo::cube_face_normals(fit->first.direction, norm);
		halfwidth = fit->first.get_halfwidth();
		p += halfwidth*0.5*norm;
		outfile << "v " << p.transpose()
		        << " 0 0 255" << endl;

		/* iterate over the neighbors of this face */
		num_verts = 0;
		for(nit = fit->second.neighbors.begin();
				nit != fit->second.neighbors.end(); nit++)
		{
			/* ignore self-neighbors */
			if(*nit == fit->first)
			{
				progbar.clear();
				cerr << "[node_boundary_t::"
				     << "writeobj_cliques]\t"
				     << "Found selfcycle" << endl;
				continue;
			}

			/* write out point at neighbor */
			nit->get_center(p);
			outfile << "v " << p.transpose()
			        << " 0 255 0" << endl;
			num_verts++;

			/* write triangle for this neighbor */
			outfile << "f " << -1 
				<< " " << (-1-num_verts)
				<< " " << (-2-num_verts) << endl;
		}
	}

	/* clean up */
	progbar.clear();
	outfile.close();
	return 0;
}

/*----------------------------------*/
/* node_boundary_t helper functions */
/*----------------------------------*/

int node_boundary_t::populate_faces(const octtopo_t& topo)
{
	map<octnode_t*, octneighbors_t>::const_iterator it;
	pair<facemap_t::iterator, bool> ins;
	node_face_t face;
	vector<octnode_t*> neighs;
	size_t f, i, j, n, num_nodes;
	bool found_exterior;
	progress_bar_t progbar;

	/* set up progress bar */
	progbar.set_name("Making boundary faces");

	/* now that we've stored the bounary nodes, we can determine the
	 * set of boundary faces.  Each boundary node can contribute one
	 * or more boundary faces. */
	j = 0;
	num_nodes = topo.size();
	for(it = topo.begin(); it != topo.end(); it++)
	{
		/* update user on progress */
		progbar.update(j++, num_nodes);

		/* ignore exterior nodes */
		if(!(this->node_is_interior(it->first)))
			continue;

		/* iterate over faces, looking for exterior neighbors */
		for(f = 0; f < NUM_FACES_PER_CUBE; f++)
		{
			/* prepare properties of the current face */
			face.init(it->first, NULL,
					octtopo::all_cube_faces[f]);
			
			/* get nodes neighboring on this face */
			neighs.clear();
			it->second.get(face.direction, neighs);
			
			/* if the neighs list is empty, that means that
			 * this node is abutting null space.  We want to
			 * count this as a boundary, since null space
			 * is considered exterior.  Thus, we'll add
			 * a null octnode to the list in this eventuality
			 * to account for it */
			if(neighs.empty())
				neighs.push_back(NULL);

			/* check for external nodes */
			found_exterior = false;
			n = neighs.size();
			for(i = 0; i < n; i++)
			{
				/* ignore if interior */
				if(this->node_is_interior(neighs[i]))
					continue;
				found_exterior = true; /* i'th neighbor
				                        * exterior */
				face.exterior = neighs[i];
				
				/* store face info */
				ins = this->faces.insert(pair<node_face_t,
						node_face_info_t>(
						face, node_face_info_t()));
				if(!(ins.second))
				{
					/* unable to insert face */
					progbar.clear();
					cerr << "[node_boundary_t::"
					     << "populate_faces]\tSomehow,"
					     << " current face was already"
					     << " inserted!?" << endl;
					return -1;
				}

				/* keep track of all the faces generated
				 * for each node.
				 *
				 * We want to do this for both the current
				 * node and the opposing node, since both
				 * are connected to the face. */
				if(neighs[i] != NULL)
					this->node_face_map.insert(
						std::pair<octnode_t*,
							node_face_t>(
							neighs[i], face));
			}
			
			/* record current node in mapping */
			if(found_exterior)
				this->node_face_map.insert(
					std::pair<octnode_t*,node_face_t>(
						it->first, face));
		}
	}

	/* success */
	progbar.clear();
	return 0;
}
		
int node_boundary_t::populate_face_linkages(const octtopo_t& topo)
{
	facemap_t::iterator fit;
	faceset_t nearby_faces;
	faceset_t::iterator nit;
	node_face_t face;
	Vector3d fp, np, normal;
	progress_bar_t progbar;
	size_t j, num_faces;
	int ret;

	/* populate linkages between faces using node_face_map
	 *
	 * faces should be neighbors iff one of the following:
	 *	- they share a node (and their f values are not same 
	 * 				or opposing)
	 * 	- they are on neighboring nodes and have same f value
	 */

	/* prepare progress bar */
	progbar.set_name("Linking node faces");

	/* iterate all the faces to connect */
	j = 0;
	num_faces = this->faces.size();
	for(fit = this->faces.begin(); fit != this->faces.end(); fit++)
	{
		/* update user on progress */
		progbar.update(j++, num_faces);

		/* get the current face */
		face = fit->first;

		/* get all nearby faces that have the potential to
		 * be linked to this face */
		nearby_faces.clear();
		ret = this->get_nearby_faces(topo, face.interior, 
					nearby_faces);
		if(ret)
			return PROPEGATE_ERROR(-1, ret);
		ret = this->get_nearby_faces(topo, face.exterior,
					nearby_faces);
		if(ret)
			return PROPEGATE_ERROR(-2, ret);

		/* for each potential face, check if it should be linked */
		for(nit = nearby_faces.begin(); 
				nit != nearby_faces.end(); nit++)
		{
			/* check edge case of the faces being the same */
			if(face == *nit)
				continue; /* don't want self-linkages */

			/* can't be neighbors if they don't share
			 * an edge */
			if(!(face.shares_edge_with(*nit)))
				continue;

			/* check if they have the same interior or exterior
			 * nodes.  If so, then they are definitely
			 * candidates to be linked */
			if(face.interior == nit->interior)
			{
				/* guaranteed to link if their exteriors
				 * neighbor */
				
				/* if got here, then the two faces
				 * share an edge, and should be 
				 * linked */
				fit->second.neighbors.insert(*nit);
			}
			else if(face.exterior == nit->exterior)
			{
				/* guaranteed to link if their interiors
				 * are neighbors */
				
				/* if got here, then the two faces
				 * share an edge, and should be 
				 * linked */
				fit->second.neighbors.insert(*nit);
			}
			else
			{
				/* since the faces don't share either
				 * node, that means that all of the
				 * following must be true for them
				 * to link:
				 *
				 * 	- their interiors must neighbor
				 * 	- their exteriors must neighbor
				 * 	- faces must be same direction
				 * 	- faces on the same plane
				 */
				if(!(topo.are_neighbors(face.interior,
							nit->interior)))
					continue; /* not int. neighbors */
				if(!(topo.are_neighbors(face.exterior,
							nit->exterior)))
					continue; /* not ext. neighbors */
				if(face.direction != nit->direction)
					continue; /* not same direction */

				face.get_center(fp);
				nit->get_center(np);
				octtopo::cube_face_normals(face.direction,
								normal);
				if(abs(normal.dot(fp - np)) > APPROX_ZERO)
					continue; /* not coplanar */

				/* if got here, then the two faces
				 * share an edge, and should be 
				 * linked */
				fit->second.neighbors.insert(*nit);
			}
		}
	}

	/* success */
	progbar.clear();
	return 0;
}

/*--------------------------------------*/
/* node_face_t function implementations */
/*--------------------------------------*/

bool node_face_t::shares_edge_with(const node_face_t& other) const
{
	octtopo::CUBE_FACE oppf;
	double ax[2];
	double ay[2];
	double bx[2];
	double by[2];
	double hw, ohw;
	Vector3d center, othercenter, disp, disp_a, disp_perp, manhat;
	Vector3d norm, othernorm, axis;

	/* get the cube side that this face resides on */
	oppf = octtopo::get_opposing_face(this->direction);
	if(oppf == other.direction)
		return false; /* opposing faces cannot share an edge */
	
	/* the following initialize the variables that may
	 * be used */
	ax[0]=ax[1] = ay[0]=ay[1] = bx[0]=bx[1] = by[0]=by[1] = 0.0;
	hw  = this->get_halfwidth();
	ohw = other.get_halfwidth();
	this->get_center(center);
	other.get_center(othercenter);

	/* check for case where they face the same direction */
	if(this->direction == other.direction)
	{
		/* face the same direction, problem is now 2D */
		switch(this->direction)
		{
			case FACE_XMINUS:
			case FACE_XPLUS:
				ax[0] = center(1)-hw;
				ax[1] = center(1)+hw;
				ay[0] = center(2)-hw;
				ay[1] = center(2)+hw;
				bx[0] = othercenter(1)-ohw;
				bx[1] = othercenter(1)+ohw;
				by[0] = othercenter(2)-ohw;
				by[1] = othercenter(2)+ohw;
				break;

			case FACE_YMINUS:
			case FACE_YPLUS:
				ax[0] = center(2)-hw;
				ax[1] = center(2)+hw;
				ay[0] = center(0)-hw;
				ay[1] = center(0)+hw;
				bx[0] = othercenter(2)-ohw;
				bx[1] = othercenter(2)+ohw;
				by[0] = othercenter(0)-ohw;
				by[1] = othercenter(0)+ohw;
				break;

			case FACE_ZMINUS:
			case FACE_ZPLUS:
				ax[0] = center(0)-hw;
				ax[1] = center(0)+hw;
				ay[0] = center(1)-hw;
				ay[1] = center(1)+hw;
				bx[0] = othercenter(0)-ohw;
				bx[1] = othercenter(0)+ohw;
				by[0] = othercenter(1)-ohw;
				by[1] = othercenter(1)+ohw;
				break;
		}

		/* determine if edges touch */
		return poly2d::aabb_pair_abut(ax,ay,bx,by,APPROX_ZERO); 
	}
	
	/* the faces are orthogonal to each other.  We can determine
	 * if these touch based on analysis corresponding to each
	 * face's normal. */
	octtopo::cube_face_normals(this->direction, norm);
	octtopo::cube_face_normals(other.direction, othernorm);
	disp = center - othercenter; /* displacement betwen faces */
	axis = norm.cross(othernorm); /* axis of edge, if exists */
	disp_a = disp.dot(axis) * axis; /* displacement along axis */
	disp_perp = disp - disp_a;  /* disp. component perp to axis */

	/* displacement we expect if they share an edge */
	manhat = norm*ohw - othernorm*hw;
	
	/* the faces, if they are touching, can either be bending inward
	 * or outward.  We can check if it is one of these two
	 * possibilities with the following: */
	if((manhat - disp_perp).squaredNorm() > APPROX_ZERO
		&& (manhat + disp_perp).squaredNorm() > APPROX_ZERO)
	{
		/* neither scenario checks out, so this cannot
		 * be a shared edge */
		return false;
	}

	/* now that we know that both faces share an edge defined by
	 * axis, we want to make sure they occupy an overlapping
	 * segment of that line, which can be done as follows */
	if(disp_a.norm() < max(hw, ohw))
		return true; /* the faces occupy overlapping 
		              * segments on axis */

	/* if got here, then the faces don't share an edge */
	return false;
}

void node_face_t::get_center(Eigen::Vector3d& p) const
{
	CUBE_FACE f;
	double hw;

	/* determine starting node */
	if(this->exterior == NULL 
			|| this->exterior->halfwidth 
			 > this->interior->halfwidth)
	{
		/* the interior node dictates position */
		f  = this->direction;
		hw = this->interior->halfwidth;
		p  = this->interior->center;
	}
	else
	{
		/* the exterior node dictates position */
		f  = octtopo::get_opposing_face(this->direction);
		hw = this->exterior->halfwidth;
		p  = this->exterior->center;
	}

	/* get face center position based on node center position */
	switch(f)
	{
		case FACE_ZMINUS:
			p(2) -= hw;
			break;
		case FACE_YMINUS:
			p(1) -= hw;
			break;
		case FACE_XMINUS:
			p(0) -= hw;
			break;
		case FACE_XPLUS:
			p(0) += hw;
			break;
		case FACE_YPLUS:
			p(1) += hw;
			break;
		case FACE_ZPLUS:
			p(2) += hw;
			break;
	}
}

double node_face_t::get_halfwidth() const
{
	/* whichever node (either the interor or exterior) that
	 * borders this face has the smaller halfwidth will
	 * dictate the halfwidth of this face */
	if(this->exterior == NULL)
		return this->interior->halfwidth;
	else if(this->interior->halfwidth < this->exterior->halfwidth)
		return this->interior->halfwidth;
	return this->exterior->halfwidth;
}
		
double node_face_t::get_area() const
{
	double hw;

	/* compute the halfwidth of this face */
	hw = this->get_halfwidth();

	/* compute the area */
	return 4*hw*hw;
}

double node_face_t::get_planarity() const
{	
	double mu_i, planar_i, hw_i, mu_e, planar_e, hw_e, splitval, s;
	
	/* check validity of this face */
	if(this->interior == NULL || this->interior->data == NULL
		|| (this->exterior != NULL 
			&& this->exterior->data == NULL))
	{
		/* invalid data given */
		cerr << "[node_face_t::get_planarity]\t"
		     << "Given invalid face" << endl;
		return 0;
	}
	
	/* get the values we need from the originating octdata structs */
	mu_i     = this->interior->data->get_probability();
	planar_i = this->interior->data->get_planar_prob();
	hw_i     = this->interior->halfwidth;
	if(this->exterior == NULL)
	{
		/* exterior is a null node, which should be counted
		 * as an unobserved external node.
		 *
		 * This means that our only planarity estimate is
		 * from the interior node, so just use that. */
		return planar_i;
	}
	else
	{
		/* exterior node exists, get its data */
		mu_e     = this->exterior->data->get_probability();
		planar_e = this->exterior->data->get_planar_prob();
		hw_e     = this->exterior->halfwidth;
	}

	/* set the probability value at which we expect the face
	 * to lie at.  If this face is separating an interior node
	 * and an exterior node, then this value should be the
	 * probability threshold between these two sets (i.e. 0.5).
	 *
	 * However, if this faces separates two interior nodes or
	 * two exterior nodes, then we should use a different value */
	splitval = 0.5;
	if( (mu_e < splitval) == (mu_i < splitval) )
	{
		/* both sides are exterior or both sides are interior,
		 * so it doesn't make sense to perform the split
		 * using the probability value.  For this situation,
		 * just take the weighted average planarity based
		 * on the distance of each node from the face. */
		return ( (hw_e * planar_i) + (hw_i * planar_e) )
					/ (hw_e + hw_i);
	}

	/* In order to get the planarity value for the face,
	 * we perform a weighted average between the values for
	 * the two nodes that define this face.  The first step 
	 * is to find this weighting with respect to the distance
	 * of the isosurface between the node centers.  This
	 * value can be determined with:
	 *
	 * 	s = (p_i - 0.5) / (p_i - p_e)
	 *
	 * Where s is the weighting for the exterior node, and (1-s)
	 * is the weighting for the interior node.
	 */
	s = (mu_i - splitval) / (mu_i - mu_e);

	/* weight the node's values */
	return s*planar_e + (1-s)*planar_i; 
}
		 
void node_face_t::get_isosurface_pos(Vector3d& p) const
{
	double mu_i, int_hw, mu_e, ext_hw, mu_s, splitval;
	Vector3d normal;

	/* check validity of argument */
	if(this->interior == NULL || this->interior->data == NULL
		|| (this->exterior != NULL 
			&& this->exterior->data == NULL))
	{
		/* invalid data given */
		cerr << "[node_face_t::get_isosurface_pos]\t"
		     << "Given invalid face" << endl;
		return;
	}
	
	/* get the values we need from the originating octdata structs */
	mu_i  = this->interior->data->get_probability();
	int_hw = this->interior->halfwidth;
	if(this->exterior == NULL)
	{
		/* exterior is a null node, which should be counted
		 * as an unobserved external node */
		mu_e   = 0.5; /* value given to unobserved nodes */
		ext_hw = 0;
	}
	else
	{
		/* exterior node exists, get its data */
		mu_e   = this->exterior->data->get_probability();
		ext_hw = this->exterior->halfwidth;
	}
	
	/* get properties of the face */
	this->get_center(p);
	splitval = 0.5; /* we want to split on threshold probability val */
	if( (mu_e < splitval) == (mu_i < splitval) )
	{
		/* this face separates two interior nodes or two
		 * exterior nodes, so it does not make sense to
		 * find the split location based on probability.
		 *
		 * Just report the face position as the geometric center */
		return;
	}

	/* retrieve the remaining properties of the face */
	octtopo::cube_face_normals(this->direction, normal);
	p -= int_hw * normal; /* aligned with interior node center */

	/* the face's position is based on where we expect the 0.5 value
	 * to be if we interpolated the pdf between the centers of the
	 * two nodes given.
	 *
	 * The isosurface position would be:
	 *
	 * 	s = (p_i - 0.5) / (p_i - p_e)
	 *	p = <interior center> + norm * s * (int_hw + ext_hw)
	 *
	 * (remember, the norm points from the interior into the exterior)
	 *
	 * Note that:
	 *
	 * 	p_i ~ Guass(mu_i, var_i)
	 * 	p_e ~ Gauss(mu_e, var_e)
	 *
	 * We want to compute the expected position for the 
	 * isosurface mark, which would be:
	 *
	 * 	s = (mu_i - 0.5) / (mu_i - mu_e)
	 *
	 * (due to linearization approximation used)
	 *
	 * So we can set the actual isosurface position to be:
	 */
	mu_s = (mu_i - splitval) / (mu_i - mu_e);
	p += normal*mu_s*(int_hw + ext_hw);
}

double node_face_t::get_pos_variance() const
{
	double mu_i, var_i, int_hw, mu_e, var_e, ext_hw, mu_s, var_s;
	double splitval, ss, var_p;

	/* check validity of argument */
	if(this->interior == NULL || this->interior->data == NULL
		|| (this->exterior != NULL 
			&& this->exterior->data == NULL))
	{
		/* invalid data given */
		cerr << "[node_face_t::get_pos_variance]\t"
		     << "Given invalid face" << endl;
		return DBL_MAX;
	}
	splitval = 0.5; /* value given to unobserved nodes */

	/* get the values we need from the originating octdata structs */
	mu_i  = this->interior->data->get_probability(); /* mean interior
	                                                  * value */
	var_i = this->interior->data->get_uncertainty(); /* variance of 
	                                              * interior value */
	int_hw = this->interior->halfwidth;
	if(this->exterior == NULL)
	{
		/* exterior is a null node, which should be counted
		 * as an unobserved external node */
		mu_e   = splitval;
		var_e  = 1.0; /* maximum variance for a value in [0,1] */
		ext_hw = 0;
	}
	else
	{
		/* exterior node exists, get its data */
		mu_e   = this->exterior->data->get_probability();
		var_e  = this->exterior->data->get_uncertainty();
		ext_hw = this->exterior->halfwidth;
	}

	/* check if this face is hidden or not */
	if( (mu_i < splitval) == (mu_e < splitval) )
	{
		/* This is a hidden face, which means it separates
		 * two nodes that are both either interior or exterior.
		 * This means that using their probability values to
		 * separate them doesn't make sense.  Instead, we should
		 * assume that the center position can be anywhere in
		 * the domain of these nodes.
		 *
		 * Treated as a continuous uniform random variable
		 * over the range [-int_hw, ext_hw], we can compute
		 * the variance of this position. */
		var_p = (ext_hw - int_hw);
		return (var_p * var_p / 12.0);
	}

	/* the face's position is based on where we expect the 0.5 value
	 * to be if we interpolated the pdf between the centers of the
	 * two nodes given.
	 *
	 * The isosurface position would be:
	 *
	 * 	s = (p_i - 0.5) / (p_i - p_e)
	 *	p = <interior center> + norm * s * (int_hw + ext_hw)
	 *
	 * (remember, the norm points from the interior into the exterior)
	 *
	 * Note that:
	 *
	 * 	p_i ~ Guass(mu_i, var_i)
	 * 	p_e ~ Gauss(mu_e, var_e)
	 *
	 * We want to compute the variance of s.  For ease of computation,
	 * we linearize s(p_i,p_e) about (p_i,p_e) = (mu_i,mu_e), which
	 * gives us:
	 *
	 * 	mu_s  = (mu_i - 0.5) / (mu_i - mu_e)
	 * 	var_s = (1-mu_s^2)*var_i + mu_s^2*var_e
	 * 	      - 2*(0.5-mu_e)*(0.5-mu_i)/(mu_i-mu_e)^2*cov(p_i,p_e)
	 *
	 * If we assume that p_i is indepentent from p_e, then:
	 *
	 * 	var_s = (1-mu_s^2)*var_i + mu_s^2*var_e
	 */
	mu_s = (mu_i - splitval) / (mu_i - mu_e);
	ss = mu_s*mu_s;
	var_s = (1-ss)*var_i + ss*var_e;

	/* now we need to scale this quantity based on the size of the
	 * nodes being analyzed for this face */
	ss = (int_hw + ext_hw);
	var_p = ss*ss*var_s; /* for variance, multiply by squared coef. */
	
	/* return the computed value */
	return var_p;
}


void node_face_t::writeobj(std::ostream& os) const
{
	this->writeobj(os, 255, 255, 255);
}
		
void node_face_t::writeobj(std::ostream& os, double v) const
{
	int r, g, b;

	/* make sure bounds are valid for input value */
	if(v < 0)
		v = 0;
	if(v > 1)
		v = 1;
	
	/* compute color based on value */
	r = (int) (255 * v);
	g = 0;
	b = (int) (255 * (1-v));

	/* write the face */
	this->writeobj(os, r, g, b);
}

void node_face_t::writeobj(std::ostream& os, int r, int g, int b) const
{
	Vector3d p, center;
	double hw;

	/* get geometry of face */
	this->get_center(p);
	hw = this->get_halfwidth();

	/* export center position */
	center = p;
	//this->get_isosurface_pos(center);
	os << "v " << center(0) << " " << center(1) << " " << center(2)
	   <<  " " << r << " " << g << " " << b << endl;

	/* draw vertices based on orientation of face */
	switch(this->direction)
	{
		case FACE_ZMINUS:
			os << "v " << (p(0)-hw)
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)   ) 
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)-hw)
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl;
			break;
		case FACE_ZPLUS:
			os << "v " << (p(0)-hw)
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)-hw)
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)   )
			   <<  " " << r << " " << g << " " << b << endl;
			break;
		case FACE_YMINUS:
			os << "v " << (p(0)-hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)-hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl;
			break;
		case FACE_YPLUS:
			os << "v " << (p(0)-hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)-hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)+hw)
			   <<  " " << (p(1)   ) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl;
			break;
		case FACE_XMINUS:
			os << "v " << (p(0)   )
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)   )
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)   )
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)   )
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl;
			break;
		case FACE_XPLUS:
			os << "v " << (p(0)   )
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)   )
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)-hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)   )
			   <<  " " << (p(1)+hw) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl
			   << "v " << (p(0)   )
			   <<  " " << (p(1)-hw) 
			   <<  " " << (p(2)+hw)
			   <<  " " << r << " " << g << " " << b << endl;
			break;
	}

	/* draw the faces, oriented ccw inward */
	os << "f -5 -1 -2" << endl
	   << "f -5 -2 -3" << endl
	   << "f -5 -3 -4" << endl
	   << "f -5 -4 -1" << endl;
}

