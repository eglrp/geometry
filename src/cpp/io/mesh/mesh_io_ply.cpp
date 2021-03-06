#include "mesh_io.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <float.h>

/**
 * @file   mesh_io_ply.cpp
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 * @brief  This class can read in various mesh file formats
 *
 * @section DESCRIPTION
 *
 * This file contains classes used to import and
 * export vertex and triangle information from common mesh
 * file formats, specifically for Stanford Polygon (PLY) format.
 */

using namespace std;
using namespace mesh_io;

/* the following definitions relate to the PLY format */
#define MAGIC_NUMBER         "ply"
#define FORMAT_FLAG          "format"
#define SUPPORTED_VERSION    "1.0"
#define FORMAT_ASCII_FLAG    "ascii"
#define FORMAT_LE_FLAG       "binary_little_endian"
#define FORMAT_BE_FLAG       "binary_big_endian"
#define ELEMENT_FLAG         "element"
#define PROPERTY_FLAG        "property"
#define COMMENT_FLAG         "comment"
#define END_HEADER_FLAG      "end_header"

/* the following data types are used in PLY files */
#define DOUBLE_TYPE          "double"
#define FLOAT_TYPE           "float"
#define INT_TYPE             "int"
#define UCHAR_TYPE           "uchar"
#define LIST_TYPE            "list"
#define LIST_UCHAR_INT_TYPE  (LIST_TYPE " uchar int")
#define LIST_INT_INT_TYPE    (LIST_TYPE " int int")

/* the following names of elements and properties are supported */
const string vertex_names[] = {"vertex", "vert", "Vertex", "VERTEX", 
                               "Vert"};
const string face_names[]   = {"face", "Face", "FACE", "polygon", "Polygon",
                             "poly", "Poly", "POLYGON", "POLY"};

const string x_names[]              = {"x", "X"};
const string y_names[]              = {"y", "Y"};
const string z_names[]              = {"z", "Z"};
const string red_names[]            = {"red", "r", "R", "Red", "RED"};
const string green_names[]          = {"green", "g", "G", "Green", "GREEN"};
const string blue_names[]           = {"blue", "b", "B", "Blue", "BLUE"};
const string vertex_indices_names[] = {"vertex_indices", "vertex_index", 
                                     "vert_inds", "vert_indices",
                                     "vert_index"};

const size_t num_vertex_names       = (sizeof(vertex_names)
					/ sizeof(vertex_names[0]));
const size_t num_face_names         = (sizeof(face_names)
					/ sizeof(face_names[0]));

const size_t num_x_names            = (sizeof(x_names)/sizeof(x_names[0]));
const size_t num_y_names            = (sizeof(y_names)/sizeof(y_names[0]));
const size_t num_z_names            = (sizeof(z_names)/sizeof(z_names[0]));
const size_t num_red_names          = (sizeof(red_names)
					/ sizeof(red_names[0]));
const size_t num_green_names        = (sizeof(green_names)
					/ sizeof(green_names[0]));
const size_t num_blue_names         = (sizeof(blue_names)
					/ sizeof(blue_names[0]));
const size_t num_vertex_indices_names  = (sizeof(vertex_indices_names)
					/ sizeof(vertex_indices_names[0]));

/*------------------*/
/* helper functions */
/*------------------*/

/**
 * check if query string is in array
 */
inline bool string_in_arr(const string& query,const string* arr,size_t num)
{
	size_t i;
	for(i = 0; i < num; i++)
		if(query.compare(arr[i]) == 0)
			return true;
	return false;
}

/**
 * These helper functions read individual elements from the file body
 */
int read_vert(ifstream& infile, vertex_t& vert, mesh_io::FILE_FORMAT ff)
{
	float xx, yy, zz;
	unsigned char rr, gg, bb;

	/* how we serialize depends on the file format */
	switch(ff)
	{
		/* non-ply */
		default:
		case FORMAT_UNKNOWN:
		case FORMAT_OBJ:
		case FORMAT_OBJ_COLOR:
			
			cerr << "[mesh_io_ply::read_vert]\t"
			     << "unknown file format." << endl;
			return -1; /* unable to serialize to unknown */
		
		/* ply */
		case FORMAT_PLY_ASCII:
		case FORMAT_PLY_ASCII_COLOR:

			/* read in vertex (assume it's colored) */
			infile >> vert.x >> vert.y >> vert.z;
			infile >> vert.red >> vert.green >> vert.blue;
			break;

		case FORMAT_PLY_BE:
		case FORMAT_PLY_BE_COLOR:

			/* don't support big endian...yet */
			cerr << "[mesh_io_ply::read_vert]\tError, format "
			     << "is not yet supported: "
			     << ff << endl;
			return -2;

		case FORMAT_PLY_LE:
		case FORMAT_PLY_LE_COLOR:
			
			/* read in binary (assume it's colored) */
			infile.read((char*) &xx, sizeof(xx));
			infile.read((char*) &yy, sizeof(yy));
			infile.read((char*) &zz, sizeof(zz));
			infile.read((char*) &rr, sizeof(rr));
			infile.read((char*) &gg, sizeof(gg));
			infile.read((char*) &bb, sizeof(bb));
		
			/* store in vertex */
			vert.x     = xx;
			vert.y     = yy;
			vert.z     = zz;
			vert.red   = rr;
			vert.green = gg;
			vert.blue  = bb;
			break;
	}

	/* success */
	return 0;
}

int read_face(ifstream& infile, polygon_t& face, mesh_io::FILE_FORMAT ff)
{
	size_t i, n;
	int v;
	unsigned char num;
	
	/* clear the face */
	face.clear();

	/* how we serialize depends on the file format */
	switch(ff)
	{
		/* non-ply */
		default:
		case FORMAT_UNKNOWN:
		case FORMAT_OBJ:
		case FORMAT_OBJ_COLOR:
			cerr << "[mesh_io_ply::read_face]\t"
			     << "unknown file format." << endl;
			return -1; /* unable to serialize to unknown */
		
		/* ply */
		case FORMAT_PLY_ASCII:
		case FORMAT_PLY_ASCII_COLOR:
			infile >> n; /* get size of face */
			for(i = 0; i < n; i++)
			{
				/* get next vertex */
				infile >> v;
				face.vertices.push_back(v);
			}
			break;

		case FORMAT_PLY_BE:
		case FORMAT_PLY_BE_COLOR:
			/* don't support big endian...yet */
			cerr << "[mesh_io_ply::read_face]\tError, format "
			     << "is not yet supported: "
			     << ff << endl;
			return -2;

		case FORMAT_PLY_LE:
		case FORMAT_PLY_LE_COLOR:
			/* import size of face as an unsigned char,
			 * and the face vertex indices as ints */
			infile.read((char*) &num, sizeof(num));
			n = (int) num;
			for(i = 0; i < n; i++)
			{
				infile.read((char*) &v, sizeof(v));
				face.vertices.push_back(v);
			}
			break;
	}

	/* success */
	return 0;
}

/*----------------*/
/* helper classes */
/*----------------*/

/**
 * This class is used to represent a property type from a PLY file
 *
 * Each property is defined based on a name and a type
 */
class ply_property_t
{
	public:
		/* the name of this property */
		string name;

		/* the type of this property */
		string type;
};

/**
 * This class is used to represent an element type from a PLY file
 *
 * An element is defined based on a name, and a list of properties.
 * Each property has some type associated with it
 */
class ply_element_t
{
	public:
		/* name of this element */
		string name;

		/* How many of this element are in the file */
		size_t num_elements;

		/* the properties of this element */
		vector<ply_property_t> props;
};


/*---------------------------------*/
/* mesh_t function implementations */ 
/*---------------------------------*/
		
int mesh_t::read_ply(const std::string& filename)
{
	ifstream infile;
	vector<ply_element_t> elements;
	vertex_t vert;
	polygon_t face;
	stringstream ss;
	string tline, field;
	bool readingheader;
	size_t elem_idx, num_elem, vert_idx, num_vert, face_idx, num_face;
	int ret;

	/* open file for reading */
	infile.open(filename.c_str(), ios_base::in | ios_base::binary);
	if(!(infile.is_open()))
	{
		cerr << "[mesh_t::ready_ply]\tUnable to open file for "
		     << "reading: " << filename << endl;
		return -1;
	}

	/* parse the header */
	readingheader = true;
	while(readingheader)
	{
		/* read the next line */
		getline(infile, tline);
		ss.clear();
		ss.str(tline);

		/* parse the line */
		ss >> field;
		if(field.empty())
			continue; /* ignore empty lines */
		else if(field.compare(MAGIC_NUMBER) == 0)
		{
			/* good! it's a ply file.
			 *
			 * Note that the way this is
			 * set up, we can still correctly
			 * parse a file without this line. */
			continue;
		}
		else if(field.compare(FORMAT_FLAG) == 0)
		{
			/* get the format of this file */
			ss >> field;

			/* check against known formats */
			if(field.compare(FORMAT_ASCII_FLAG) == 0)
				this->format = FORMAT_PLY_ASCII_COLOR;
			else if(field.compare(FORMAT_BE_FLAG) == 0)
				this->format = FORMAT_PLY_BE_COLOR;
			else if(field.compare(FORMAT_LE_FLAG) == 0)
				this->format = FORMAT_PLY_LE_COLOR;
			else
			{
				cerr << "[mesh_t::read_ply]\tUnknown PLY "
				     << "format: " << field << endl;
				return -2;
			}

			/* the version number comes next, but we don't
			 * care abou that */
		}
		else if(field.compare(END_HEADER_FLAG) == 0)
		{
			/* the header is done, exit loop */
			readingheader = false;
		}
		else if(field.compare(COMMENT_FLAG) == 0)
			continue; /* ignore comments */
		else if(field.compare(ELEMENT_FLAG) == 0)
		{
			/* add a new element to our list */
			elements.resize(elements.size()+1);

			/* store its name */
			ss >> elements.back().name;

			/* store how many there are */
			ss >> elements.back().num_elements;
		}
		else if(field.compare(PROPERTY_FLAG) == 0)
		{
			/* check that element exists */
			if(elements.empty())
			{
				cerr << "[mesh_t::read_ply]\tProperty flag "
				     << "appeared before element flag in "
				     << "header of " << filename << endl;
				return -3;
			}
			
			/* add a new property to the latest element */
			elements.back().props.resize(
					elements.back().props.size()+1);

			/* set property type */
			ss >> elements.back().props.back().type;
			if(elements.back().props.back().type.compare(
						LIST_TYPE) == 0)
			{
				/* a list has two more types, one to
				 * identify the number of elements,
				 * and another to identify each element
				 * type */
				ss >> field;
				elements.back().props.back().type +=
					(" " + field);
				ss >> field;
				elements.back().props.back().type +=
					(" " + field);
			}
			
			/* set property name */
			ss >> elements.back().props.back().name;
		}
		else
		{
			/* unknown flag */
			cerr << "[mesh_t::read_ply]\tUnable to parse "
			     << "line in header: \"" << tline << "\"" 
			     << endl;
			return -4;
		}
	}

	/* iterate over the elements in this file */
	num_elem = elements.size();
	for(elem_idx = 0; elem_idx < num_elem; elem_idx++)
	{
		/* we only care if this element is a vertex or a face */
		if(string_in_arr(elements[elem_idx].name, vertex_names,
					num_vertex_names))
		{
			/* we're reading vertices! */
			num_vert = elements[elem_idx].num_elements;

			/* check that the vertices are defined reasonably */
			if(elements[elem_idx].props.size() != 6)
			{
				cerr << "[mesh_t::read_ply]\tError!  The "
				     << "input ply file specifies its "
				     << "vertices in an unexpected way."
				     << endl << endl
				     << "This parser assumes they are:"
				     << endl << "\tx y z red green blue" 
				     << endl;
				return -5;
			}

			/* read the vertices */
			for(vert_idx = 0; vert_idx < num_vert; vert_idx++)
			{
				/* read the next vert */
				ret = read_vert(infile, vert, this->format);
				if(ret)
				{
					cerr << "[meshj_t::read_ply]\t"
					     << "Error " << ret << "!"
					     << "  could not read "
					     << "vertex #" << vert_idx
					     << endl;
					return -6;
				}

				/* store in mesh */
				this->add(vert);
			}
		}
		else if(string_in_arr(elements[elem_idx].name, face_names,
					num_face_names))
		{
			/* we're reading faces! */
			num_face = elements[elem_idx].num_elements;

			/* check that the faces are defined reasonably */
			if(elements[elem_idx].props.size() != 1)
			{
				cerr << "[mesh_t::read_ply]\tError!  The "
				     << "input ply file specifies its "
				     << "faces in an unexpected way."
				     << endl << endl
				     << "This parser assumes they are:"
				     << endl 
				     << "\tlist uchar int vertex_indices"
				     << endl;
				return -7;
			}
			
			/* read the faces */
			for(face_idx = 0; face_idx < num_face; face_idx++)
			{
				/* read the next face */
				ret = read_face(infile, face, this->format);
				if(ret)
				{
					cerr << "[meshj_t::read_ply]\t"
					     << "Error " << ret << "!"
					     << "  could not read "
					     << "face #" << face_idx
					     << endl;
					return -8;
				}

				/* store in mesh */
				this->add(face);
			}
		}
		else
		{
			/* not a recognized element.  Fail */
			cerr << "[mesh_t::read_ply]\tError!  The element "
			     << "type " << elements[elem_idx].name
			     << " is not supported, so the input PLY file "
			     << "cannot be parsed." << endl;
			return -9;
		}
	}
	
	/* clean up */
	infile.close();
	return 0;
}
	
int mesh_t::write_ply(const std::string& filename, FILE_FORMAT ff) const
{
	ofstream outfile;
	size_t i, n;
	int ret;

	/* attempt to open file for writing.  The nature of the output
	 * file depends on the format */
	switch(ff)
	{
		default:
			cerr << "[mesh_t::write_ply]\tNot valid PLY file "
			     << "format: " << ff << endl;
			return -1;
		case FORMAT_PLY_ASCII:
		case FORMAT_PLY_ASCII_COLOR:
			outfile.open(filename.c_str());
			break;
		case FORMAT_PLY_BE:
		case FORMAT_PLY_BE_COLOR:
		case FORMAT_PLY_LE:
		case FORMAT_PLY_LE_COLOR:
			outfile.open(filename.c_str(), 
					ios_base::out | ios_base::binary);
			break;
	}
	
	/* check file */
	if(!(outfile.is_open()))
	{
		cerr << "[mesh_t::write_ply]\tUnable to open file for "
		     << "writing: " << filename << endl;
		return -2;
	}

	/* magic number */
	outfile << MAGIC_NUMBER << endl;

	/* specify format */
	switch(ff)
	{
		default:
			cerr << "[mesh_t::write_ply]\tNot valid PLY file "
			     << "format: " << ff << endl;
			return -3;
		case FORMAT_PLY_ASCII:
		case FORMAT_PLY_ASCII_COLOR:
			outfile << FORMAT_FLAG       << " "
				<< FORMAT_ASCII_FLAG << " "
				<< SUPPORTED_VERSION << endl;
			break;
		case FORMAT_PLY_BE:
		case FORMAT_PLY_BE_COLOR:
			outfile << FORMAT_FLAG       << " "
				<< FORMAT_BE_FLAG    << " "
				<< SUPPORTED_VERSION << endl;
			break;
		case FORMAT_PLY_LE:
		case FORMAT_PLY_LE_COLOR:
			outfile << FORMAT_FLAG       << " "
				<< FORMAT_LE_FLAG    << " "
				<< SUPPORTED_VERSION << endl;
			break;
	}

	/* specify vertex format */
	outfile << ELEMENT_FLAG    << " "
		<< vertex_names[0] << " " << this->vertices.size() << endl
	        << PROPERTY_FLAG   << " "
		<< FLOAT_TYPE     << " "
		<< x_names[0]      << endl
	        << PROPERTY_FLAG   << " "
		<< FLOAT_TYPE     << " "
		<< y_names[0]      << endl
	        << PROPERTY_FLAG   << " "
		<< FLOAT_TYPE     << " " 
		<< z_names[0]      << endl;

	/* add color info if desired */
	switch(ff)
	{
		case FORMAT_PLY_ASCII_COLOR:
		case FORMAT_PLY_BE_COLOR:
		case FORMAT_PLY_LE_COLOR:
			outfile << PROPERTY_FLAG   << " "
				<< UCHAR_TYPE        << " " 
				<< red_names[0]    << endl
			        << PROPERTY_FLAG   << " "
				<< UCHAR_TYPE        << " " 
				<< green_names[0]  << endl
			        << PROPERTY_FLAG   << " "
				<< UCHAR_TYPE        << " " 
				<< blue_names[0]   << endl;
			break;
		default:
			/* no color info */
			break;
	}

	/* specify faces */
	outfile << ELEMENT_FLAG            << " "
	        << face_names[0]           << " " 
		<< this->polygons.size()   << endl
	        << PROPERTY_FLAG           << " "
		<< LIST_UCHAR_INT_TYPE     << " "
		<< vertex_indices_names[0] << endl;

	/* end header information */
	outfile << END_HEADER_FLAG << endl;

	/* write out vertices */
	n = this->vertices.size();
	for(i = 0; i < n; i++)
	{
		/* export this vertex */
		ret = this->vertices[i].serialize(outfile, ff);
		if(ret)
		{
			/* report error */
			cerr << "[mesh_t::write_ply]\tError " << ret << ": "
			     << "Unable to write vertex #" << i << endl;
			return -4;
		}
	}

	/* write out faces */
	n = this->polygons.size();
	for(i = 0; i < n; i++)
	{
		/* export this polygon */
		ret = this->polygons[i].serialize(outfile, ff);
		if(ret)
		{
			/* report error */
			cerr << "[mesh_t::write_ply]\tError " << ret << ": "
			     << "Unable to write polygon #" << i << endl;
			return -5;
		}
	}

	/* clean up */
	outfile.close();
	return 0;
}
