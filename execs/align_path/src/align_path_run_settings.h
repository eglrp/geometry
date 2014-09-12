#ifndef ALIGN_PATH_RUN_SETTINGS_H
#define ALIGN_PATH_RUN_SETTINGS_H

/**
 * @file   align_path_run_settings.h
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 * @brief  Gets user-defined run settings for align_path program
 *
 * @section DESCRIPTION
 *
 * This file contains classes used to parse and store
 * user-defined run parameters and settings for the 
 * align_path program.  This is a wrapper class around
 * cmd_args_t, which is used to parse command-line
 * arguments.
 */

#include <string>
#include <vector>

/**
 * This class is used to store run settings for the align_path program
 */
class align_path_run_settings_t
{
	/* parameters */
	public:

		/* the following files are necessary for
		 * this program */

		/**
		 * The hardware xml configuration file
		 */
		std::string configfile;

		/**
		 * The timestamp synchronization file
		 */
		std::string timefile;

		/**
		 * The ic4 data file to parse
		 */
		std::string ic4file;

		/**
		 * The input .mad file to parse for path information
		 */
		std::string input_path;

		/**
		 * The output.mad file to write to
		 */
		std::string output_path;

	/* functions */
	public:

		/**
		 * Creates an empty object
		 */
		align_path_run_settings_t();

		/**
		 * Parses settings from command-line
		 *
		 * Will parse the command-line arguments to get all
		 * the necessary settings.  This may also include
		 * parsing xml settings files that were passed on
		 * the command-line.
		 *
		 * @param argc  Number of command-line arguments
		 * @param argv  The command-line arguments given to main()
		 *
		 * @return      Returns zero on success, non-zero on failure
		 */
		int parse(int argc, char** argv);
};

#endif
