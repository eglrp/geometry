#include "generate_scanorama_run_settings.h"
#include <xmlreader/xmlsettings.h>
#include <util/cmd_args.h>
#include <util/tictoc.h>
#include <util/error_codes.h>
#include <iostream>
#include <string>
#include <vector>

/**
 * @file   generate_scanorama_run_settings.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 * @brief  Gets user-defined run settings for generate_scanorama program
 *
 * @section DESCRIPTION
 *
 * This file contains classes used to parse and store
 * user-defined run parameters and settings for the
 * generate_scanorama program.  This is a wrapper class around
 * cmd_args_t, which is used to parse command-line
 * arguments.
 */

using namespace std;

/* the command-line flags to check for */

#define SETTINGS_FILE    "-s"
#define CONFIGFILE_FLAG  "-c"
#define PATHFILE_FLAG    "-p"
#define MODELFILE_FLAG   "-m"
#define FISHEYE_FLAG     "-f"
#define OUTFILE_FLAG     "-o"

/* the xml parameters to look for */

#define XML_NUM_ROWS     "scanorama_num_rows"
#define XML_NUM_COLS     "scanorama_num_cols"
#define XML_BLENDWIDTH   "scanorama_blendwidth"
#define XML_SPACING_DIST "scanorama_spacing_dist"

/*--------------------------*/
/* function implementations */
/*--------------------------*/
		
generate_scanorama_run_settings_t::generate_scanorama_run_settings_t()
{
	/* initialize fields to default values */
	this->xml_config   = "";
	this->pathfile     = "";
	this->modelfile    = "";
	this->cam_metafiles.clear();
	this->cam_calibfiles.clear();
	this->cam_imgdirs.clear();
	this->num_rows     = 1000;
	this->num_cols     = 2000;
	this->spacing_dist = 1.0;
	this->ptx_outfile  = "";
}

int generate_scanorama_run_settings_t::parse(int argc, char** argv)
{
	cmd_args_t args;
	XmlSettings settings;
	vector<string> files;
	size_t i, num_cams;
	tictoc_t clk;
	int ret;

	/* populate args with what we expect on the command-line */
	tic(clk);
	args.set_program_description("This program generates scanoramas "
			"for camera positions in the specified dataset.  "
			"Scanoramas are a point cloud representation that "
			"is used to indicate a panoramic image with depth "
			"at each pixel.");
	args.add(SETTINGS_FILE, "The xml settings file that defines "
			"parameters used for this scanorama generation.",
			false, 1);
	args.add(CONFIGFILE_FLAG, "The hardware configuration .xml file "
			"for this dataset.", false, 1);
	args.add(PATHFILE_FLAG, "The path trajectory file (either .mad or "
			".noisypath) for this dataset.", false, 1);
	args.add(MODELFILE_FLAG, "The model geometry file (.obj, .ply) for "
			"this dataset.", false, 1);
	args.add(FISHEYE_FLAG, "Specifies a set of fisheye images to use "
			"to color the output.  Expects three arguments:"
			"\n\n\t"
			"<color metadata file> <fisheye calib file> "
			"<image folder>\n\nThe metadata file should be "
			"the output file after bayer converting the images."
			"  The calibration file should be a binary .dat "
			"file representing the ocam calib results.  The "
			"image directory should be the same on that is "
			"referenced by the metadata file.\n\n"
			"Use this flag multiple times to specify multiple "
			"sets of images from different cameras.", true, 3);
	args.add(OUTFILE_FLAG, "The prefix file path of where to store the "
			"output scanorama files (.ptx).  So, if the value "
			"specified is:\n\n\t\"foo/bar/scan_\"\n\n"
			"then the exported files will be:\n\n"
			"\tfoo/bar/scan_00000000.ptx\n"
			"\tfoo/bar/scan_00000001.ptx\n"
			"\t...",false,1);

	/* parse the command-line arguments */
	ret = args.parse(argc, argv);
	if(ret)
	{
		/* unable to parse command-line arguments.  Inform
		 * user and quit. */
		ret = PROPEGATE_ERROR(-1, ret);
		cerr << "[generate_scanorama_run_settings_t::parse]\t"
		     << "Unable to parse command-line arguments:  "
		     << "Error " << ret << endl;
		return ret;
	}

	/* populate this object with what was parsed from
	 * the command-line */
	this->xml_config  = args.get_val(CONFIGFILE_FLAG);
	this->pathfile    = args.get_val(PATHFILE_FLAG);
	this->modelfile   = args.get_val(MODELFILE_FLAG);
	files.clear(); args.tag_seen(FISHEYE_FLAG, files);
	this->ptx_outfile = args.get_val(OUTFILE_FLAG);
	
	/* sort the files associated with the camera imagery */
	num_cams = files.size() / 3;
	for(i = 0; i < num_cams; i++)
	{
		this->cam_metafiles.push_back(  files[3*i]     );
		this->cam_calibfiles.push_back( files[3*i + 1] );
		this->cam_imgdirs.push_back(    files[3*i + 2] );
	}

	/* import settings from xml settings file */
	if(!settings.read(args.get_val(SETTINGS_FILE)))
	{
		/* unable to parse settings file */
		ret = PROPEGATE_ERROR(-2, ret);
		cerr << "[generate_scanorama_run_settings_t::parse]\t"
		     << "Error " << ret << ": Unable to parse xml "
		     << "settings file for this program." << endl;
		return ret;
	}

	/* read in values from settings file */
	if(settings.is_prop(XML_NUM_ROWS))
		this->num_rows     = settings.getAsUint(XML_NUM_ROWS);
	if(settings.is_prop(XML_NUM_COLS))
		this->num_cols     = settings.getAsUint(XML_NUM_COLS);
	if(settings.is_prop(XML_BLENDWIDTH))
		this->blendwidth   = settings.getAsDouble(XML_BLENDWIDTH);
	if(settings.is_prop(XML_SPACING_DIST))
		this->spacing_dist = settings.getAsDouble(XML_SPACING_DIST);

	/* we successfully populated this structure, so return */
	toc(clk, "Importing settings");
	return 0;
}
