#include "merge_run_settings.h"
#include <mesh/floorplan/floorplan.h>
#include <mesh/refine/object_refiner.h>
#include <mesh/refine/octree_padder.h>
#include <geometry/octree/octree.h>
#include <geometry/shapes/extruded_poly.h>
#include <util/progress_bar.h>
#include <util/error_codes.h>
#include <util/tictoc.h>
#include <iostream>

/**
 * @file main.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 *
 * @section DESCRIPTION
 *
 * merge_fp_oct will add floorplan information to a specified octree.
 */

using namespace std;

/* function headers */

int import_all_fps(octree_t& tree, const merge_run_settings_t& args);
int import_fp(const std::string& fp, octree_t& tree,
			unsigned int& num_rooms);
void clear_fp(octree_t& tree);

/* function implementations */

/**
 * The main function for this program
 */
int main(int argc, char** argv)
{
	merge_run_settings_t args;
	octree_t tree;
	object_refiner_t refiner;
	tictoc_t clk;
	int ret;

	/* parse the given parameters */
	ret = args.parse(argc, argv);
	if(ret)
	{
		cerr << "[main]\tError " << ret << ": "
		     << "Could not parse parameters" << endl;
		return 1;
	}

	/* import octree */
	tic(clk);
	ret = tree.parse(args.input_octfile);
	if(ret)
	{
		cerr << "[main]\tError " << ret << ": "
		     << "Unable to parse input tree: " << args.input_octfile
		     << endl;
		return 2;
	}
	toc(clk, "Importing octree");

	/* pad the tree to make further processing easier */
	tic(clk);
	octree_padder::pad(tree);
	toc(clk, "Padding octree");
	
	/* Merge floorplan info into tree 
	 *
	 * We need to do this in order to determine which volumes
	 * need to be refined.
	 */
	ret = import_all_fps(tree, args);
	if(ret)
	{
		cerr << "[main]\tError " << ret << ": "
		     << "Unable to import floorplans." << endl;
		return 3;
	}

	/* if we specify an increased depth for object nodes, then
	 * recarve those areas */
	if(args.object_refine_depth > 0)
	{	
		/* refine the tree at the location of objects in 
		 * the environment */
		ret = refiner.init(args.object_refine_depth,
				args.input_chunklistfile,
				args.input_wedgefile,
				args.input_carvemapfile,
				args.interpolate);
		if(ret)
		{
			/* inform user of initialization error */
			cerr << "[main]\tError " << ret << ": "
			     << "Unable to initialize refiner." << endl;
			return 4;
		}
		ret = refiner.refine(tree);
		if(ret)
		{
			/* inform user of processing error */
			cerr << "[main]\tError " << ret << ": "
			     << "Unable to refine octree." << endl;
			return 5;
		}
	
		/* pad the tree to make further processing easier */
		tic(clk);
		octree_padder::pad(tree);
		toc(clk, "Padding octree");

		/* once again, merge floorplan info into tree, in
		 * order to relabel the volumes that were refined */
		ret = import_all_fps(tree, args);
		if(ret)
		{
			cerr << "[main]\tError " << ret << ": "
			     << "Unable to import floorplans again." 
			     << endl;
			return 6;
		}
	}

	/* export the octree to destination */
	tic(clk);
	ret = tree.serialize(args.output_octfile);
	if(ret)
	{
		/* unable to write to file, inform user */
		cerr << "[main]\tError " << ret
		     << ": Unable to write to output file "
		     << args.output_octfile << endl;
		return 7;
	}
	toc(clk, "Exporting octree");	

	/* success */
	return 0;
}

/*---------------------------------*/
/* helper function implementations */
/*---------------------------------*/

/**
 * Imports all floorplan information into a carved tree
 *
 * After this call, the tree's floorplan information will be
 * replaced with the info specified in the list of floorplans
 * represented in the settings object given.
 *
 * @param tree      The tree to modify
 * @param args      The settings to use
 *
 * @return          Returns zero on success, non-zero on failure.
 */
int import_all_fps(octree_t& tree, const merge_run_settings_t& args)
{
	unsigned int i, num_fps, num_rooms;
	tictoc_t clk;
	int ret;

	/* clear any floorplan info */
	tic(clk);
	clear_fp(tree);
	toc(clk, "Clearing octree room info");

	/* apply the provided floorplan info to this tree */
	num_fps = args.fpfiles.size();
	num_rooms = 0;
	for(i = 0; i < num_fps; i++)
	{
		/* apply current floorplan */
		ret = import_fp(args.fpfiles[i], tree, num_rooms);
		if(ret)
		{
			ret = PROPEGATE_ERROR(-1, ret);
			cerr << "[import_all_fps]\tError " << ret << ": "
			     << "Unable to import fp #" << i << endl;
			return ret;
		}
	}

	/* success */
	return 0;
}

/**
 * Imports floor plan information into a carved tree
 *
 * After carving, calling this function will parse
 * a floorplan and import its room information into
 * the tree.
 *
 * @param fpfile    The input .fp file to parse and use
 * @param tree      The tree to modify
 * @param num_rooms Number of rooms so far.  This will be modified. 
 *
 * @return     Returns zero on success, non-zero on failure.
 */
int import_fp(const std::string& fpfile, octree_t& tree,
              unsigned int& num_rooms)
{
	fp::floorplan_t f;
	extruded_poly_t poly;
	progress_bar_t progbar;
	tictoc_t clk;
	unsigned int i, n;
	int ret;

	/* read in floor plan */
	tic(clk);
	ret = f.import_from_fp(fpfile);
	if(ret)
		return PROPEGATE_ERROR(-1, ret);
	toc(clk, "Reading floor plan file");

	/* iterate over the rooms of this floorplan, and generate
	 * a shape object for each room */
	tic(clk);
	n = f.rooms.size();
	progbar.set_name("Merging floor plan");
	for(i = 0; i < n; i++)
	{
		/* show progress to user */
		progbar.update(i, n);

		/* create shape */
		poly.init(f, num_rooms + i, i);
	
		/* subdivide tree to make sure boundary of room is
		 * preserved at high-res */
		poly.set_hollow(true);
		ret = tree.subdivide(poly);
		if(ret)
		{
			/* an error occurred during subdivision */
			progbar.clear();
			return PROPEGATE_ERROR(-2, ret);
		}

		/* import into tree */
		poly.set_hollow(false);
		ret = tree.insert(poly);
		if(ret)
		{
			/* an error occurred during insert */
			progbar.clear();
			return PROPEGATE_ERROR(-3, ret);
		}

		/* simplify tree, since inserting this room may
		 * have carved additional nodes */
		tree.get_root()->simplify_recur();
	}

	/* update number of rooms in building */
	num_rooms += n;
	progbar.clear();
	toc(clk, "Merging floor plans");

	/* success */
	return 0;
}

/**
 * Will recursively iterate through all subnodes of this octnode
 *
 * This helper function will iterate through this node's children
 * and clear floorplan information from populated data structures.
 *
 * @param node   The node to modify
 */
void clear_fp_recur(octnode_t* node)
{
	unsigned int i;

	/* check if this node has data */
	if(node->data != NULL)
		node->data->set_fp_room(-1); /* clear room info */

	/* recurse over children */
	for(i = 0; i < CHILDREN_PER_NODE; i++)
		if(node->children[i] != NULL)
			clear_fp_recur(node->children[i]);
}

/**
 * Will clear all room info from the given tree
 *
 * Will iterate over the nodes of the tree, and remove
 * any floorplan room info.
 *
 * @param tree   The tree to modify
 */
void clear_fp(octree_t& tree)
{
	/* recursively modify the tree, starting at the root */
	clear_fp_recur(tree.get_root());
}
