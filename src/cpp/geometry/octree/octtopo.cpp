#include "octtopo.h"
#include "octree.h"
#include "octnode.h"
#include <util/error_codes.h>
#include <util/progress_bar.h>
#include <util/tictoc.h>
#include <Eigen/Dense>
#include <algorithm>
#include <utility>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <set>

/**
 * @file octtopo.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 * @brief  The octtopo_t class is used for computing octree topology
 *
 * @section DESCRIPTION
 *
 * This file contains the octtopo_t class, which is used to provide
 * additional representations of octree and octnode topology.  Its
 * main purpose is to allow for relative neighbor linkages between
 * adjacent nodes.
 *
 * While this class is not part of the octree structure directly, it
 * can be provided with an octree to be initialized, and used to augment
 * an existing tree structure information.
 */

using namespace std;
using namespace Eigen;
using namespace octtopo;

/* the following definitions are used for processing */

#define APPROX_ZERO 0.0000001

/*-----------------------------------------*/
/* octneighbors_t function implementations */
/*-----------------------------------------*/

octneighbors_t::octneighbors_t()
{}

octneighbors_t::octneighbors_t(const octneighbors_t& other)
{
	size_t i;
	for(i = 0; i < NUM_FACES_PER_CUBE; i++)
		this->neighs[i].insert(other.neighs[i].begin(),
					other.neighs[i].end());
}

void octneighbors_t::clear()
{ 
	size_t i;
	for(i = 0; i < NUM_FACES_PER_CUBE; i++)
		this->neighs[i].clear();
}

void octneighbors_t::add_all(const std::vector<octnode_t*>& ns, CUBE_FACE f)
{
	size_t i, n;
	n = ns.size();
	for(i = 0; i < n; i++)
		this->add(ns[i], f);
}

void octneighbors_t::get_singletons(octnode_t* ns[NUM_FACES_PER_CUBE]) const
{
	size_t i;
	for(i = 0; i < NUM_FACES_PER_CUBE; i++)
		ns[i] = (this->neighs[i].size() == 1)
				? (*(this->neighs[i].begin())) : NULL;
}
			
octneighbors_t& octneighbors_t::operator = (const octneighbors_t& other)
{
	size_t i;

	/* copy each value */
	for(i = 0; i < NUM_FACES_PER_CUBE; i++)
	{
		/* destroy existing */
		this->neighs[i].clear();
		this->neighs[i].insert(other.neighs[i].begin(),
				other.neighs[i].end());
	}

	/* return changed object */
	return (*this);
}

/*------------------------------------*/
/* octtopo_t function implementations */
/*------------------------------------*/

int octtopo_t::init(const octree_t& tree)
{
	int ret;

	/* the root node of this tree can't have any neighbors, since
	 * there are no other nodes on that level */
	this->neighs.clear();
	this->neighs.insert(pair<octnode_t*, octneighbors_t>(
			tree.get_root(), octneighbors_t()));

	/* recursively populate the map with the subnodes of this tree */
	this->init_children(tree.get_root());

	/* now that all nodes have been mapped, clean up */
	ret = this->remove_nonleafs();
	if(ret)
		return PROPEGATE_ERROR(-1, ret);

	/* success */
	return 0;
}
			
int octtopo_t::add(octnode_t* node, const octneighbors_t& neighs)
{
	pair<map<octnode_t*,octneighbors_t>::iterator, bool> it;

	/* attempt to insert values */
	it = this->neighs.insert(
			pair<octnode_t*, octneighbors_t>(node, neighs));
	if(it.second)
		return 0; /* value inserted */
	return -1; /* value already exists */
}
			
int octtopo_t::get(octnode_t* node, octneighbors_t& neighs) const
{
	map<octnode_t*, octneighbors_t>::const_iterator it;

	/* find the node */
	it = this->neighs.find(node);
	if(it == this->neighs.end())
		return -1; /* node not in structure */

	/* copy the neighbors of this node */
	neighs = it->second;
	return 0;
}
			
bool octtopo_t::are_neighbors(octnode_t* a, octnode_t* b) const
{
	octneighbors_t a_neighs, b_neighs;
	int ret;
	size_t fi;

	/* first, check validity of arguments */
	if(a == NULL || b == NULL || a == b)
		return false; /* cannot be neighbors in these cases */

	/* get neighbors for a */
	ret = this->get(a, a_neighs);
	if(ret)
		return false; /* if a is not in topo, cannot be neighs */

	/* get neighbors for b */
	ret = this->get(b, b_neighs);
	if(ret)
		return false;

	/* search through a's neighbors, looking for b */
	for(fi = 0; fi < NUM_FACES_PER_CUBE; fi++)
		if(a_neighs.neighs[fi].count(b))
		{
			/* 'b' is a neighbor of 'a' on face fi,
			 * so verify that 'a' is a neigh of
			 * 'b' on the opposite face */
			if(b_neighs.neighs[
					octtopo::get_opposing_face(
					octtopo::all_cube_faces[fi])]
					.count(a))
				return true; /* they are neighbors */

			/* if got here, the neighboring is asymmetric,
			 * which is bad */
			cerr << "[octtopo_t::are_neighbors]\tError! Found "
			     << "asymmetric neighbors!" << endl << endl;
			return true; /* still count them as neighbors */
		}

	/* no neighbors found */
	return false;
}
			
int octtopo_t::remove_outliers(double neigh_thresh)
{
	queue<map<octnode_t*, octneighbors_t>::iterator> 
				in_to_check, out_to_check;
	map<octnode_t*, octneighbors_t>::iterator it;
	vector<octnode_t*> ns;
	progress_bar_t progbar;
	tictoc_t clk;
	size_t num_seen, fi, ni, num_neighs;
	double count, myarea, neigharea;
	bool current_in;

	/* if a threshold is given outside the valid
	 * range, then don't do anything. */
	if(neigh_thresh <= 0.5 || neigh_thresh > 1.0)
		return 0;

	/* prepare processing */
	tic(clk);
	progbar.set_name("Removing outliers");
	num_seen = 0;

	/* iterate over all available nodes, add them to our queues */
	for(it = this->neighs.begin(); it != this->neighs.end(); it++)
	{
		/* add to appropriate queue */
		if(this->node_is_interior(it->first))
			in_to_check.push(it);
		else
			out_to_check.push(it);
	}

	/* We want to check all the interior nodes first,
	 * to see if they should be flipped to exterior nodes.
	 *
	 * After we've finished with the in->out flipping,
	 * we want to check any existing exterior nodes that
	 * should be flipped to be interior. */
	while(!(in_to_check.empty() && out_to_check.empty()))
	{
		/* update progress bar */
		progbar.update(num_seen, 
			num_seen+in_to_check.size()+out_to_check.size());
		
		/* get next value */
		if(in_to_check.empty())
		{
			/* get next currently exterior node */
			it = out_to_check.front();
			out_to_check.pop();
			current_in = false;
		}
		else
		{
			/* get next currently interior node */
			it = in_to_check.front();
			in_to_check.pop();
			current_in = true;
		}
		num_seen++;

		/* make sure this value is legit */
		if(it == this->neighs.end())
		{
			progbar.clear();
			cerr << "[octtopo_t::remove_outliers]\tError! "
			     << "encountered invalid iterator in queue."
			     << endl;
			return -1;
		}

		/* check if we've already flipped this node */
		if(it->first->data == NULL)
			continue;
		if(this->node_is_interior(it->first) != current_in)
			continue; /* already flipped */

		/* get this node's total surface area */
		myarea = it->first->surface_area();

		/* get this node's neighbors */
		ns.clear();
		for(fi = 0; fi < NUM_FACES_PER_CUBE; fi++)
			it->second.get(all_cube_faces[fi], ns);

		/* iterate through the neighbors, looking for nodes
		 * whose flags disagree with the current node's flag.
		 *
		 * If there are a sufficient number of disagreeing
		 * neighbors, then this node is an outlier. */
		num_neighs = ns.size();
		count = 0;
		for(ni = 0; ni < num_neighs; ni++)
			if(this->node_is_interior(ns[ni]) != current_in)
			{
				/* this neighboring node disagrees
				 * with the current node, so count
				 * its shared area towards the total */
				neigharea = min(ns[ni]->halfwidth,
						it->first->halfwidth);
				neigharea = 4*neigharea*neigharea;

				/* weight this neighbor's vote based
				 * on the amount of shared surface area */
				count += neigharea;
			}
		count /= myarea; /* normalize by total surface area */

		/* check count against threshold */
		if(count < neigh_thresh)
			continue; /* not an outlier */

		/* This node is an outlier, so we want to flip it. */
		it->first->data->flip();

		/* We also want to then double-check all its 
		 * neighbors that used to agree (and now disagree),
		 * since they may be outliers, too. */
		for(ni = 0; ni < num_neighs; ni++)
			if(this->node_is_interior(ns[ni]) == current_in)
				in_to_check.push(
					this->neighs.find(ns[ni]));
	}
	
	/* success */
	progbar.clear();
	toc(clk, "Removing outlier nodes");
	return 0;
}
			
bool octtopo_t::node_is_interior(octnode_t* node)
{
	/* check if node is valid */
	if(node == NULL)
		return false;

	/* check if node has any data */
	if(node->data == NULL)
		return false;

	/* check if node's data considers itself interior */
	return (node->data->is_interior());
}
			
int octtopo_t::writeobj(const string& filename) const
{
	map<octnode_t*, octneighbors_t>::const_iterator it;
	vector<octnode_t*> ns;
	vector<octnode_t*>::iterator nit;
	Vector3d p;
	ofstream outfile;
	progress_bar_t progbar;
	size_t fi, i, n;
	double hw, other_hw;

	/* open file for writing */ 
	outfile.open(filename.c_str());
	if(!(outfile.is_open()))
		return -1;

	/* prepare file */
	outfile << "# This file auto-generated by Eric Turner's" << endl
	        << "# geometry code for UC Berkeley's VIP Lab."
		<< "#" << endl
		<< "# The original octree had " << this->neighs.size()
		<< " nodes." << endl
	        << endl << endl;

	/* iterate through the nodes */
	progbar.set_name("Exporting OBJ");
	n = this->neighs.size(); i = 0;
	for(it = this->neighs.begin(); it != this->neighs.end(); it++)
	{
		/* inform user of progress */
		progbar.update(i++, n);

		/* only proceed if interior */
		if(!(this->node_is_interior(it->first)))
			continue;

		/* iterate over faces, looking for exterior neighbors */
		hw = it->first->halfwidth;
		for(fi = 0; fi < NUM_FACES_PER_CUBE; fi++)
		{
			/* get nodes neighboring on this face */
			ns.clear();
			it->second.get(all_cube_faces[fi], ns);

			/* check for external nodes */
			for(nit = ns.begin(); nit != ns.end(); nit++)
			{
				/* record neighbor's surface area */
				other_hw = (*nit)->halfwidth;

				/* ignore if interior */
				if(this->node_is_interior(*nit))
					continue;

				/* neighbor marks exterior boundary, 
				 * so export intersection of faces */
				if(other_hw < hw)
					this->writeobjface(outfile, (*nit), 
						get_opposing_face(
						all_cube_faces[fi]), false);
				else
					this->writeobjface(outfile, 
						it->first, 
						all_cube_faces[fi], true);
			}

			/* check if it is an interior node against 
			 * null space */
			if(ns.empty())
				this->writeobjface(outfile, it->first,
					all_cube_faces[fi], true);
		}
	}

	/* success */
	progbar.clear();
	outfile.close();
	return 0;
}
			
void octtopo_t::writeobjface(ostream& os, octnode_t* n,
                             CUBE_FACE f, bool inside, bool usecolor) const
{
	Vector3d c;
	double hw, val;
	int r, g, b;

	/* check for null argument */
	if(n == NULL)
		return; /* don't write anything */

	/* get node properties */
	c = n->center;
	hw = n->halfwidth;
	val = n->data->get_planar_prob();

	/* set color */
	if(n->data == NULL || !usecolor)
		r = g = b = 255;
	else
	{
		r = (int) (255 * val);
		g = 0;
		b = (int) (255 * (1 - val));
	}

	/* switch based on face direction */
	switch(f)
	{
		case FACE_XMINUS:
			/* write face */
			c(0) -= hw;
			os << "v " <<  c(0) 
			    << " " << (c(1)-hw)
			    << " " << (c(2)-hw) 
			    << " " << r << " " << g << " " << b << endl
			   << "v " <<  c(0)
			    << " " << (c(1)-hw)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " <<  c(0)
			    << " " << (c(1)+hw)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " <<  c(0)
			    << " " << (c(1)+hw)
			    << " " << (c(2)-hw)
			    << " " << r << " " << g << " " << b << endl;
			break;
		case FACE_XPLUS:
			/* write face */
			c(0) += hw;
			os << "v " <<  c(0) 
			    << " " << (c(1)-hw)
			    << " " << (c(2)-hw) 
			    << " " << r << " " << g << " " << b << endl
			   << "v " <<  c(0)
			    << " " << (c(1)+hw)
			    << " " << (c(2)-hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " <<  c(0)
			    << " " << (c(1)+hw)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " <<  c(0)
			    << " " << (c(1)-hw)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl;
			break;
		case FACE_YMINUS:
			/* write face */
			c(1) -= hw;
			os << "v " << (c(0)-hw)
			    << " " <<  c(1)
			    << " " << (c(2)-hw) 
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " <<  c(1)
			    << " " << (c(2)-hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " <<  c(1)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)-hw)
			    << " " <<  c(1)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl;
			break;
		case FACE_YPLUS:
			/* write face */
			c(1) += hw;
			os << "v " << (c(0)-hw)
			    << " " <<  c(1)
			    << " " << (c(2)-hw) 
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)-hw)
			    << " " <<  c(1)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " <<  c(1)
			    << " " << (c(2)+hw)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " <<  c(1)
			    << " " << (c(2)-hw)
			    << " " << r << " " << g << " " << b << endl;
			break;
		case FACE_ZMINUS:
			/* write face */
			c(2) -= hw;
			os << "v " << (c(0)-hw)
			    << " " << (c(1)-hw)
			    << " " <<  c(2) 
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)-hw)
			    << " " << (c(1)+hw)
			    << " " <<  c(2)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " << (c(1)+hw)
			    << " " <<  c(2)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " << (c(1)-hw)
			    << " " <<  c(2)
			    << " " << r << " " << g << " " << b << endl;
			break;
		case FACE_ZPLUS:
			/* write face */
			c(2) += hw;
			os << "v " << (c(0)-hw)
			    << " " << (c(1)-hw)
			    << " " <<  c(2) 
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " << (c(1)-hw)
			    << " " <<  c(2)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)+hw)
			    << " " << (c(1)+hw)
			    << " " <<  c(2)
			    << " " << r << " " << g << " " << b << endl
			   << "v " << (c(0)-hw)
			    << " " << (c(1)+hw)
			    << " " <<  c(2)
			    << " " << r << " " << g << " " << b << endl;
			break;
	}

	/* write face elements */
	if(inside)
		os << "f -1 -2 -3 -4" << endl;
	else
		os << "f -4 -3 -2 -1" << endl;
}

void octtopo_t::init_children(octnode_t* node)
{
	octneighbors_t ns[CHILDREN_PER_NODE];
	octnode_t* uncles[NUM_FACES_PER_CUBE];
	octnode_t* cousins[CHILDREN_PER_NODE];
	unsigned int i;

	/* only need to process if given node is non-leaf */
	if(node->isleaf())
		return;

	/* get all existing neighbors of current node */
	this->neighs[node].get_singletons(uncles);

	/* each child of this node must be added as a special case,
	 * based on the relative position of the child with respect to
	 * the other nodes in play (which includes children of the current
	 * node's neighbors).
	 *
	 * The ordering of octnodes can be found here 
	 * (taken from octnode.h):
	 * 
	 * 		y
	 *              ^
	 *       1      |      0
	 *              |
	 * -------------+-------------> x	(top, z+)
	 *              |
	 *       2      |      3
	 *              |
	 *
	 * 		y
	 *              ^
	 *       5      |      4
	 *              |
	 * -------------+-------------> x	(bottom, z-)
	 *              |
	 *       6      |      7
	 *              |
	 */

	/* internal linkages of this node's children */

	/* top level */
	ns[0].add(node->children[1], FACE_XMINUS);
	ns[1].add(node->children[0], FACE_XPLUS);
	ns[1].add(node->children[2], FACE_YMINUS);
	ns[2].add(node->children[1], FACE_YPLUS);
	ns[2].add(node->children[3], FACE_XPLUS);
	ns[3].add(node->children[2], FACE_XMINUS);
	ns[3].add(node->children[0], FACE_YPLUS);
	ns[0].add(node->children[3], FACE_YMINUS);
	
	/* bottom level */
	ns[4].add(node->children[5], FACE_XMINUS);
	ns[5].add(node->children[4], FACE_XPLUS);
	ns[5].add(node->children[6], FACE_YMINUS);
	ns[6].add(node->children[5], FACE_YPLUS);
	ns[6].add(node->children[7], FACE_XPLUS);
	ns[7].add(node->children[6], FACE_XMINUS);
	ns[7].add(node->children[4], FACE_YPLUS);
	ns[4].add(node->children[7], FACE_YMINUS);

	/* up-down */
	ns[0].add(node->children[4], FACE_ZMINUS);
	ns[1].add(node->children[5], FACE_ZMINUS);
	ns[2].add(node->children[6], FACE_ZMINUS);
	ns[3].add(node->children[7], FACE_ZMINUS);
	ns[4].add(node->children[0], FACE_ZPLUS);
	ns[5].add(node->children[1], FACE_ZPLUS);
	ns[6].add(node->children[2], FACE_ZPLUS);
	ns[7].add(node->children[3], FACE_ZPLUS);

	/* external linkages to other nodes on current level */
	
	/* cousins x-plus */
	octtopo_t::get_children_of(cousins, uncles[FACE_XPLUS]);
	ns[0].add(cousins[1], FACE_XPLUS);
	ns[3].add(cousins[2], FACE_XPLUS);
	ns[4].add(cousins[5], FACE_XPLUS);
	ns[7].add(cousins[6], FACE_XPLUS);

	/* cousins x-minus */
	octtopo_t::get_children_of(cousins, uncles[FACE_XMINUS]);
	ns[1].add(cousins[0], FACE_XMINUS);
	ns[2].add(cousins[3], FACE_XMINUS);
	ns[5].add(cousins[4], FACE_XMINUS);
	ns[6].add(cousins[7], FACE_XMINUS);
	
	/* cousins y-plus */
	octtopo_t::get_children_of(cousins, uncles[FACE_YPLUS]);
	ns[0].add(cousins[3], FACE_YPLUS);
	ns[1].add(cousins[2], FACE_YPLUS);
	ns[4].add(cousins[7], FACE_YPLUS);
	ns[5].add(cousins[6], FACE_YPLUS);
	
	/* cousins y-minus */
	octtopo_t::get_children_of(cousins, uncles[FACE_YMINUS]);
	ns[3].add(cousins[0], FACE_YMINUS);
	ns[2].add(cousins[1], FACE_YMINUS);
	ns[7].add(cousins[4], FACE_YMINUS);
	ns[6].add(cousins[5], FACE_YMINUS);
	
	/* cousins z-plus */
	octtopo_t::get_children_of(cousins, uncles[FACE_ZPLUS]);
	ns[0].add(cousins[4], FACE_ZPLUS);
	ns[1].add(cousins[5], FACE_ZPLUS);
	ns[2].add(cousins[6], FACE_ZPLUS);
	ns[3].add(cousins[7], FACE_ZPLUS);
	
	/* cousins z-minus */
	octtopo_t::get_children_of(cousins, uncles[FACE_ZMINUS]);
	ns[4].add(cousins[0], FACE_ZMINUS);
	ns[5].add(cousins[1], FACE_ZMINUS);
	ns[6].add(cousins[2], FACE_ZMINUS);
	ns[7].add(cousins[3], FACE_ZMINUS);

	/* For each child, check if it exists and pair it appropriately
	 * with the populated neighbor object
	 */
	for(i = 0; i < CHILDREN_PER_NODE; i++)
	{
		/* check if exists */
		if(node->children[i] == NULL)
			continue; /* move on to next child */

		/* aggressively add this child into neighbor map */
		this->neighs[node->children[i]] = ns[i];
	
		/* recurse */
		this->init_children(node->children[i]);
	}
}
			
void octtopo_t::get_children_of(octnode_t* cs[CHILDREN_PER_NODE], 
					octnode_t* p)
{
	bool isleaf;
	size_t i;

	/* iterate over child pointers */
	isleaf = (p == NULL || p->isleaf());
	for(i = 0; i < CHILDREN_PER_NODE; i++)
	{
		/* check if no children exist */
		if(isleaf)
			cs[i] = p; /* store parent */
		else /* at least some children exist */
			cs[i] = p->children[i]; /* store child */
	}
}
			
int octtopo_t::remove_nonleafs()
{
	map<octnode_t*, octneighbors_t>::iterator it, nit;
	set<octnode_t*> to_remove;
	set<octnode_t*>::iterator sit;
	CUBE_FACE face, opp;
	size_t fi;
	bool isleaf;

	/* iterate through the neighbors, removing references to non-leafs
	 * and verifying that references between leafs are bidirectional */
	for(it = this->neighs.begin(); it != this->neighs.end(); it++)
	{
		/* check if this node is a leaf */
		isleaf = it->first->isleaf();

		/* we will want to remove this node */
		if(!isleaf)
			to_remove.insert(it->first);
		
		/* iterate through the neighbors of the 
		 * current node */
		for(fi = 0; fi < NUM_FACES_PER_CUBE; fi++)
		{
			/* iterate through neighbors on this face */
			face = all_cube_faces[fi];
			for(sit = it->second.neighs[face].begin();
				sit != it->second.neighs[face].end(); sit++)
			{
				/* get neighbor */
				nit = this->neighs.find(*sit);
				if(nit == this->neighs.end())
					return -1; /* ERROR */

				/* this neighbor's reference of the
				 * current node should be on the opposing
				 * face */
				opp = get_opposing_face(face);

				/* if the current node is a leaf, we want
				 * to keep it and add a reference to its
				 * neighbor.  if the current node is not
				 * a leaf, then we want to remove it and
				 * remove any references from the neighbor
				 */
				if(isleaf)
					nit->second.add(it->first, opp);
				else
					nit->second.remove(it->first, opp);

			}
		}
	}

	/* remove the non-leaf nodes in the map */
	for(sit = to_remove.begin(); sit != to_remove.end(); sit++)
		this->neighs.erase(*sit);
	to_remove.clear();

	/* success */
	return 0;
}
			
int octtopo_t::verify() const
{
	map<octnode_t*, octneighbors_t>::const_iterator it, opp_it;
	vector<octnode_t*> ns, ns_opp;
	vector<octnode_t*>::iterator nit;
	octnode_t* curr, *neigh;
	double width_sum, dist;
	CUBE_FACE opp;
	size_t i;
	int ret;

	if(this->neighs.empty())
	{
		/* not an error, but make a note */
		cerr << "[octtopo_t::verify]\tWARNING: empty map" << endl;
	}

	/* iterate over each node in the map */
	for(it = this->neighs.begin(); it != this->neighs.end(); it++)
	{
		/* check for null nodes */
		if(it->first == NULL)
		{
			/* notify user of error */
			ret = -1;
			cerr << "[octtopo_t::verify]\tERROR " << ret << ": "
			     << "Encountered null node in map!" << endl;
			return ret;
		}

		/* iterate over faces of current node */
		for(i = 0; i < NUM_FACES_PER_CUBE; i++)
		{
			/* get neighbors on this face */
			ns.clear(); it->second.get(all_cube_faces[i], ns);

			/* check for non-singletons */
			if(ns.size() > 1 && !(it->first->isleaf()))
			{
				/* notify user of error */
				ret = -2;
				cerr << "[octtopo_t::verify]\t"
				     << "ERROR " << ret << ": "
				     << "Encountered " << ns.size() << " "
				     << "neighbors on face "
				     << all_cube_faces[i]
				     << " of node " << it->first << endl;
				return ret;
			}
			
			/* iterate over neighbors on this face */
			opp = get_opposing_face(all_cube_faces[i]);
			for(nit = ns.begin(); nit != ns.end(); nit++)
			{
				/* make sure not null */
				if(*nit == NULL)
				{
					/* notify user of error */
					ret = -3;
					cerr << "[octtopo_t::verify]\t"
					     << "ERROR " << ret << ": "
					     << (it->first) << " has null "
					     << "neighbor on "
					     << all_cube_faces[i]
					     << endl;
					return ret;
				}

				/* make sure we have no autoloops */
				if(*nit == it->first)
				{
					/* notify user of error */
					ret = -4;
					cerr << "[octtopo_t::verify]\t"
					     << "ERROR " << ret << ": "
					     << (*nit) << " neighbors "
					     << "itself on face "
					     << all_cube_faces[i]
					     << endl;
					return ret;
				}

				/* get neighbor's neighbors, and check
				 * if current node is among them */
				opp_it = this->neighs.find(*nit);
				if(opp_it == this->neighs.end())
				{
					/* notify user of error */
					ret = -5;
					cerr << "[octtopo_t::verify]\t"
					     << "ERROR " << ret << ": "
					     << (*nit) << " not in map "
					     << "even though "
					     << it->first << " thinks it "
					     << "neighbors on "
					     << all_cube_faces[i]
					     << endl;
					return ret;
				}

				ns_opp.clear(); opp_it->second.get(opp,
								ns_opp);
				if(find(ns_opp.begin(), ns_opp.end(), 
						it->first) == ns_opp.end())
				{
					/* notify user of error */
					ret = -6;
					cerr << "[octtopo_t::verify]\t"
					     << "ERROR " << ret << ": "
					     << it->first << " claims "
					     << "neighbor on "
					     << all_cube_faces[i]
					     << " is " << (*nit) << " but "
					     << " this node's neighbors "
					     << "on " << opp << " does not "
					     << "show " << it->first
					     << endl;
					return ret;
				}

				/* check that these nodes are geometrically
				 * touching, by comparing their displacement
				 * with their widths */
				curr = it->first;
				neigh = opp_it->first;
				width_sum = curr->halfwidth
						+ neigh->halfwidth;
				dist = 0;
				switch(all_cube_faces[i])
				{
					/* x-direction */
					case FACE_XMINUS:
					case FACE_XPLUS:
						dist = curr->center(0)
							- neigh->center(0);
						break;

					/* y-direction */
					case FACE_YMINUS:
					case FACE_YPLUS:
						dist = curr->center(1)
							- neigh->center(1);
						break;
					
					/* z-direction */
					case FACE_ZMINUS:
					case FACE_ZPLUS:
						dist = curr->center(2)
							- neigh->center(2);
						break;
				}
				dist = fabs(dist);
				if(fabs(width_sum - dist) > APPROX_ZERO)
				{
					/* the two nodes don't share a face,
					 * which means the neighbor linkage
					 * has been incorrectly assigned */

					/* notify user of error */
					ret = -7;
					cerr << "[octtopo_t::verify]\t"
					     << "ERROR " << ret << ": "
					     << curr << " and " << neigh
					     << " think they neighbor, but "
					     << "their relative geometry is"
					     << ":" << endl
					     << curr->center.transpose()
					     << " with hw = "
					     << curr->halfwidth << endl
					     << neigh->center.transpose()
					     << " with hw = "
					     << neigh->halfwidth << endl
					     << endl;
					return ret;
				}
			}
		}
	}

	/* success */
	return 0;
}
