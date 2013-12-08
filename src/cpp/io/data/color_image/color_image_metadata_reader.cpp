#include "color_image_metadata_reader.h"
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <string.h>
#include <util/error_codes.h>

/**
 * @file color_image_metadata_reader.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 *
 * @section DESCRIPTION
 *
 * This file contains definitions for classes used to read
 * and parse the output image metadata files generated after
 * demosaicing and time synchronization has occurred.
 *
 * These files include the file names of the jpeg images,
 * as well as the meta-information for each image,
 * such as timestamp and camera settings.
 */

using namespace std;

/* function implementations for color_image_frame_t */
color_image_frame_t::color_image_frame_t()
{
	/* set default values */
	this->image_file = "";
	this->index = -1;
	this->timestamp = -1.0;
	this->exposure = -1;
	this->gain = -1;
}

color_image_frame_t::~color_image_frame_t()
{
	/* no resources explicitly allocated */
}

int color_image_frame_t::parse(std::istream& is)
{
	stringstream ss;
	string line;
	size_t p;

	/* check that stream is still valid */
	if(is.fail())
		return -1;

	/* parse the next line from file */
	getline(is, line);
	if(line.empty())
	{
		this->image_number = -1;
		return 0;
	}

	/* parse values from line */
	ss.str(line);
	ss >> this->image_file;
	ss >> this->index;
	ss >> this->timestamp;
	ss >> this->exposure;
	ss >> this->gain;

	/* check that stream is good */
	if(is.fail())
		return -3;
	
	/* success */
	return 0;
}

/* function implementations for color_image_reader_t */

color_image_reader_t::color_image_reader_t()
{
	/* default values already set */
}

color_image_reader_t::~color_image_reader_t()
{
	/* close the file if necessary */
	this->close();
}

/* helper function:
 *
 * Removes all occurances of carriage returns
 * from the string.
 */
void remove_all_cr(string& m)
{
	size_t pos;
	
	/* remove all \r that are found */
	while(true)
	{
		/* check if there are any carriage returns
		 * on this line */
		pos = m.find('\r');
		if(pos == string::npos)
			break;

		/* remove them if present */
		m.erase(pos);
	}
}

int color_image_reader_t::open(const std::string& filename)
{
	int i;
	string m;

	/* close any open files */
	this->close();

	/* open the file */
	this->infile.open(filename.c_str());
	if(!(this->infile.is_open()))
		return -1;

	/* read the header */
	
	/* read camera hardware data */
	this->infile >> this->camera_name;
	this->infile >> this->num_images;
	this->infile >> this->jpeg_quality;
	
	getline(this->infile, this->output_dir);
	remove_all_cr(this->image_directory);

	/* read calibration info */
	this->infile >> this->calibration.length_pol;
	for(i = 0; i < this->calibration.length_pol; i++)
		this->infile >> this->calibration.pol[i];
	this->infile >> this->calibration.length_invpol;
	for(i = 0; i < this->calibration.length_invpol; i++)
		this->infile >> this->calibration.invpol[i];
	this->infile >> this->calibration.xc;
	this->infile >> this->calibration.yc;
	this->infile >> this->calibration.c;
	this->infile >> this->calibration.d;
	this->infile >> this->calibration.e;
	this->infile >> this->calibration.width;
	this->infile >> this->calibration.height;

	/* the header should end with an extra newline */
	getline(this->infile, m);
	remove_all_cr(m);
	if(m.size() > 0)
	{
		this->close();
		return -2;
	}

	/* success */
	return 0;
}

int color_image_reader_t::next(color_image_frame_t& frame)
{
	int ret;

	/* parse the next frame from the file */
	ret = frame.parse(this->infile);
	if(ret)
		return PROPEGATE_ERROR(-1, ret);

	/* success */
	return 0;
}

bool color_image_reader_t::eof() const
{
	/* check if at end of file */
	if(this->infile.is_open())
		return this->infile.eof();
	return true;
}

void color_image_reader_t::close()
{
	/* close the file if it is open */
	if(this->infile.is_open())
		this->infile.close();

	/* no other resources need freeing */
}