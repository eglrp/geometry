#ifndef D_IMAGER_DATA_READER_H
#define D_IMAGER_DATA_READER_H

/**
 * @file d_imager_data_reader.h
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 *
 * @section DESCRIPTION
 *
 * This file describes classes used to read and parse the data files
 * generated by running the Panasonic D-Imager during a data collection.
 */

#include <istream>
#include <fstream>
#include <string>

/* the following classes are defined in this file */
class d_imager_frame_t;
class d_imager_reader_t;

/**
 * This class defines one data frame of a Panasonic D-Imager
 */
class d_imager_frame_t
{
	/* parameters */
	public:

		/* the following parameters are defined by
		 * the scanner, and do not change from frame-to-frame */
		int image_width;  /* dimensions of depth image */
		int image_height; /* dimensions of depth image */

		/* the following parameters represent the data of 
		 * this scan frame */
		int index;     /* the index of this scan (starts at zero) */
		unsigned long long timestamp; /* windows time in cycles */

		/* the following arrays represent the pointcloud.  Each
		 * point will have coordinates <x,y,z> in this array,
		 * in units of millimeters */
		short* xdat;
		short* ydat;
		short* zdat;

		/* the intensity of the depth image is represented with
		 * unsigned values in the range of [0, USHORT_MAX] */
		unsigned short* ndat;

	/* functions */
	public:

		/* constructors */

		/**
		 * Default constructor initializes empty frame
		 */
		d_imager_frame_t();

		/**
		 * Frees all allocated memory for this frame
		 */
		~d_imager_frame_t();

		/**
		 * Initializes this frame's resolution
		 *
		 * Will specify the image resolution for this frame,
		 * which specifies how many points are defined in the
		 * frame's pointcloud as well.
		 *
		 * @param w  The width of the frame image
		 * @param h  The height of the frame image
		 *
		 * @returns  Returns zero on success, non-zero on failure
		 */
		int init_resolution(int w, int h);

		/**
		 * Parses the stream for the next frame
		 *
		 * Will parse the specified stream to retrieve
		 * the next frame data, which will be used to
		 * populate this struct.
		 *
		 * This function will only modify the xdat,ydat,zdat,ndat
		 * values.  It will not modify the index or the frame-
		 * independant values.
		 *
		 * @param is The binary input stream to read from
		 *
		 * @returns  Returns zero on success, non-zero on failure
		 */
		int parse(std::istream& is);
};

/**
 * The d_imager_reader_t class is used to parse D-Imager data files
 */
class d_imager_reader_t
{
	/* parameters */
	private:

		/* the input file to parse */
		std::ifstream infile;

		/* Scanner metadata */
		int image_width;  /* image dimensions */
		int image_height; /* image dimensions */
		int fps;  /* the frames-per-second reported */
		int freq; /* the frequency mode of the scanner */

		/* the number of scans that should exist in the file */
		unsigned int num_scans;

		/* counters */
		int read_so_far;

	/* functions */
	public:

		/* constructors */

		/**
		 * Constructs empty reader
		 */
		d_imager_reader_t();

		/**
		 * Deallocates all memory and resources
		 */
		~d_imager_reader_t();

		/**
		 * Retrieves the fps value from file
		 *
		 * This function will return the fps value indicated
		 * in the opened file.
		 *
		 * @returns  Returns the fps value from file
		 */
		inline int get_fps() const
		{
			return this->fps;
		};

		inline unsigned int get_num_scans() const {
			return this->num_scans;
		};

		/**
		 * Opens file for reading, parses the header
		 *
		 * This function will attempt to read from the specified
		 * file, and verify that the file is appropriate by
		 * parsing the header information.
		 *
		 * @param filename  The path to the file to parse
		 *
		 * @returns    Returns zero on success, non-zero on failure
		 */
		int open(const std::string& filename);
		
		/**
		 * Parses the next frame from this file, stores in struct
		 *
		 * Will parse the next available frame from the datafile
		 * associated with this reader, and store it in the
		 * argument frame object.
		 *
		 * @param frame Where to store the resulting frame
		 *
		 * @returns     Returns zero on success, non-zero on failure
		 */
		int next(d_imager_frame_t& frame);

		/**
		 * Returns true if the end of file has been reached.
		 *
		 * This acts the same as any other file reader's eof()
		 *
		 * @returns  Returns true iff end of file reached
		 */
		bool eof() const;

		/**
		 * Closes the open file
		 *
		 * If this reader is open, will close the file stream
		 */
		void close();
};

#endif
