#ifndef BMP_DATA_READER_H
#define BMP_DATA_READER_H

/**
 * @file bmp_data_reader.h
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 *
 * @section DESCRIPTION
 *
 * This file contains definitions for classes used to read and
 * parse the output data files generated by running the data
 * acquisition code with barometer.  The BMP barometer
 * generates a binary data file to be parsed.
 */

#include <fstream>
#include <istream>
#include <string>

/* the following classes are defined in this file */
class bmp_frame_t;
class bmp_reader_t;

/* the following definitions are used to parse the file */
#define NUM_CALIBRATION_COEFFICIENTS 11

/**
 * This classes defines one scan line from a Hakuyo BMP data file
 */
class bmp_frame_t
{
	/* parameters */
	public:

		/* The index of this scan frame */
		unsigned int index;

		/* Each reading from the barometer has a corresponding
		 * timestamp, since temperature and pressure measurements
		 * are made at different rates 
		 *
		 * All values have been converted to host endianness,
		 * which has been assumed to be little-endian */
		
		/* the temperature timestamp and reading */
		double temp_timestamp; /* units: seconds */
		unsigned short temp; /* uncalibrated */

		/* the pressure timestamp and reading */
		double pressure_timestamp; /* units: seconds */
		unsigned short pressure;
		unsigned char pressure_xlsb; /* uncalibrated */

	/* functions */
	public:

		/* constructors */

		/**
		 * Initializes empty frame
		 */
		bmp_frame_t();

		/**
		 * Frees all memory and resources
		 */
		~bmp_frame_t();

		/* initialization */

		/**
		 * Will parse the next scan block out of a binary stream
		 *
		 * The stream is assumed to be a bmp data file, positioned
		 * at the start of a scan block.  The scan block will be
		 * parsed based to populate this struct.
		 *
		 * @param is           The binary stream to read and parse
		 * @param conversion_to_seconds    Timestamp units
		 * @param oversampling             Oversampling used
		 *
		 * @returns    Returns zero on success, non-zero on failure
		 */
		int parse(std::istream& is, double conversion_to_seconds,
		          unsigned char oversampling);
};

/**
 * This defines the bmp_reader_t class to parse a binary barometer data file
 */ 
class bmp_reader_t
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

		/* metadata about the capture */
		unsigned short calib_coeffs[NUM_CALIBRATION_COEFFICIENTS];
		unsigned char oversampling;
		double conversion_to_seconds;

		/* size of file */
		unsigned int num_scans;

	/* functions */
	public:

		/* constructors */

		/**
		 * Constructs empty class
		 */
		bmp_reader_t();

		/**
		 * Frees all allocated memory and resources
		 */
		~bmp_reader_t();

		/**
		 * Open file for parsing. Verifies file correctly formatted.
		 *
		 * Will open the selected file, and read the header of
		 * this file.  Will only return success if the file
		 * is verified to be a barometer binary data file.  After
		 * this is called, will be ready to read scan frames from
		 * file.
		 *
		 * @param filename The path the barometer binary data file
		 *
		 * @returns     Returns zero on success, non-zero on failure
		 */
		int open(const std::string& filename);

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
		int next(bmp_frame_t& frame);

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
