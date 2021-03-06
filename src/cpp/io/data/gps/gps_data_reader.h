#ifndef GPS_DATA_READER_H
#define GPS_DATA_READER_H

/**
 * @file gps_data_reader.h
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 *
 * @section DESCRIPTION
 *
 * This file contains definitions for classes used to read and
 * parse the output data files generated by running the data
 * acquisition code with gps.  The GPS
 * generates a binary data file to be parsed.
 */

#include <fstream>
#include <istream>
#include <ostream>
#include <string>

/* the following classes are defined in this file */
class gps_frame_t;
class gps_reader_t;

/**
 * This classes defines one scan line from a Hakuyo GPS data file
 */
class gps_frame_t
{
	/* parameters */
	public:

		/* The index of this scan frame */
		unsigned int index;

		/* The system timestamp for this scan line,
		 * in clock cycles */
		unsigned long long timestamp;

		/* The data are stored as ascii strings */
		unsigned int data_size;
		char* data;

	/* functions */
	public:

		/* constructors */

		/**
		 * Initializes empty frame
		 */
		gps_frame_t();

		/**
		 * Frees all memory and resources
		 */
		~gps_frame_t();

		/* initialization */

		/**
		 * Will parse the next scan block out of a binary stream
		 *
		 * The stream is assumed to be a gps data file, positioned
		 * at the start of a scan block.  The scan block will be
		 * parsed based to populate this struct.
		 *
		 * @param is           The binary stream to read and parse
		 *
		 * @returns    Returns zero on success, non-zero on failure
		 */
		int parse(std::istream& is);
};

/**
 * This defines the gps_reader_t class to parse a binary gps data file
 */ 
class gps_reader_t
{
	/* parameters */
	private:

		/* the binary file to parse */
		std::ifstream infile;

		/* the number of frames read so far from the file */
		unsigned int next_index;

		/* the version numbers of the file */
		char major_version;
		char minor_version;

		/* scanner hardware information */
		std::string serial_num;

		/* size of file */
		unsigned int num_scans;

	/* functions */
	public:

		/* constructors */

		/**
		 * Constructs empty class
		 */
		gps_reader_t();

		/**
		 * Frees all allocated memory and resources
		 */
		~gps_reader_t();

		/**
		 * Open file for parsing. Verifies file correctly formatted.
		 *
		 * Will open the selected file, and read the header of
		 * this file.  Will only return success if the file
		 * is verified to be a gps binary data file.  After
		 * this is called, will be ready to read scan frames from
		 * file.
		 *
		 * @param filename The path the gps binary data file
		 *
		 * @returns     Returns zero on success, non-zero on failure
		 */
		int open(const std::string& filename);

		/**
		 * Returns the serial number from the open file
		 *
		 * Will return the parsed serial number from the header
		 * of the open file.
		 *
		 * @returns    Returns string of serial number
		 */
		inline std::string serial_number() const
		{
			return this->serial_num;
		};

		/**
		 * Parses the next frame from the file
		 *
		 * Will read the next frame from the file and populate
		 * the specified frame struct with its information.
		 *
		 * @param frame The struct to populate with the next frame
		 *
		 * @returns     Returns zero on success, non-zero on failure
		 */
		int next(gps_frame_t& frame);

		/**
		 * Returns true iff end of file reached.
		 *
		 * Will pass the eof flag from the file stream as
		 * the return value of this function.
		 *
		 * @returns     Returns true iff end-of-file reached
		 */
		bool eof() const;

		/**
		 * Closes the stream and frees resources
		 *
		 * Will close any open file streams and free allocated
		 * arrays in memory.
		 */
		void close();
};

#endif
