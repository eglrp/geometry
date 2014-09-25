#include "octsurf_run_settings.h"
#include <iostream>

/**
 * @file   main.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 * @brief  Exports octrees (.oct) to various output files
 *
 * @section DESCRIPTION
 *
 * This is the main file for the octree surface reconstruction program
 * (octsurf) which will generate a surface reconstruction of a building
 * interior environment from an octree generated by procarve.
 */

using namespace std;

/* function implementations */

#include <io/octree/tree_exporter.h>
#include <io/octree/vox_writer.h>
#include <io/octree/sof_io.h>
#include <geometry/octree/octree.h>
#include <mesh/refine/octree_padder.h>

/**
 * The main function for this program
 */
int main(int argc, char** argv)
{
	octsurf_run_settings_t args;
	octree_t tree;
	int ret;

	/* parse the given parameters */
	ret = args.parse(argc, argv);
	if(ret)
	{
		cerr << "[main]\tError " << ret << ": "
		     << "Could not parse parameters" << endl;
		return 1;
	}

	/* initialize */
	ret = tree.parse(args.octfiles[0]);
	if(ret)
	{
		cerr << "[main]\tError " << ret << ": "
		     << "Unable to read octfile." << endl;
		return 2;
	}

	/* export */
	switch(args.output_format)
	{
		default:
			/* unable to export to this file format */
			cerr << "[main]\tUnknown file extension provided"
			     << " for output file: " << args.outfile
			     << endl;
			break;
		case FORMAT_VOX:
			/* export tree as vox file */
			ret = vox_writer_t::write(args.outfile, tree);
			if(ret)
			{
				cerr << "[main]\tError " << ret << ": "
				     << "Unable to export to vox" << endl;
				return 3;
			}
			break;
		case FORMAT_SOF:
			/* export tree as sof file */
			ret = sof_io::writesof(tree, args.outfile);
			if(ret)
			{
				cerr << "[main]\tError " << ret << ": "
				     << "Unable to export to sof" << endl;
				return 4;
			}
			break;
		case FORMAT_SOG:
			/* export tree as sog file */
			ret = sof_io::writesog(tree, args.outfile);
			if(ret)
			{
				cerr << "[main]\tError " << ret << ": "
				     << "Unable to export to sog" << endl;
				return 5;
			}
			break;
		case FORMAT_PLY:
			/* export ply file of node faces */
			octree_padder::pad(tree);
			if(args.export_node_faces)
				ret = tree_exporter::export_node_faces(
						args.outfile, tree);
			else
				ret = tree_exporter::export_dense_mesh(
						args.outfile, tree);
			if(ret)
			{
				cerr << "[main]\tError " << ret << ": "
				     << "Unable to export to ply" << endl;
				return 6;
			}
			break;
		case FORMAT_OBJ:
			/* export basic obj file */
			octree_padder::pad(tree);
			if(args.export_node_faces)
				ret = tree_exporter::export_node_faces(
						args.outfile, tree);
			else if(args.export_regions)
				ret = tree_exporter::export_regions(
						args.outfile, tree);
			else if(args.export_obj_leafs)
				ret = tree_exporter::export_leafs_to_obj(
						args.outfile, tree);
			else if(args.export_corners)
				ret = tree_exporter::export_corners_to_obj(
						args.outfile, tree);
			else
				ret = tree_exporter::export_dense_mesh(
						args.outfile, tree);
			if(ret)
			{
				cerr << "[main]\tError " << ret << ": "
				     << "Unable to export to obj" << endl;
				return 7;
			}
			break;
		case FORMAT_TXT:
			/* export useful stats to txt */
			ret = tree_exporter::export_stats_to_txt(
					args.outfile, tree);
			if(ret)
			{
				cerr << "[main]\tError " << ret << ": "
				     << "Unable to export to txt" << endl;
				return 8;
			}
			break;
	}

	/* success */
	return 0;
}
