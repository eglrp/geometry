#ifndef RANDOM_CARVER_H
#define RANDOM_CARVER_H

/**
 * @file random_carver.h
 * @author Eric Turner <elturner@eecs.berkeley.edu>
 *
 * @section DESCRIPTION
 *
 * This file defines the random_carver_t class, which is used
 * to create probabilistic models of the position of scan points,
 * and insert those models into an octree.
 */

#include <geometry/system_path.h>
#include <geometry/octree/octree.h>
#include <string>

/**
 * The random_carver_t class builds an octree from range scans.
 *
 * the random_carver_t class will populate an octree
 * in a probabilistic manner.  The parameters for our the probability
 * distributions are used to model a gaussian distribution for the
 * final position of each point in world coordinates, and to compute
 * the probability that the ray for each point passes through
 * a given voxel in the tree.
 */
class random_carver_t
{
	/* parameters */
	private:

		/* the path of the system through space. */
		system_path_t path;

		/* The octree holds the output representation of the
		 * space carving */
		octree_t tree;

		/* algorithm parameters */

		/* The clock error represents the uncertainty (std. dev.)
		 * of the system clock when timestamping hardware sensors.
		 * It is expressed in units of seconds. */
		double clock_uncertainty;

	/* functions */
	public:

		/**
		 * Constructs empty object
		 */
		random_carver_t();

		/**
		 * Initializes this object with specified data sources
		 *
		 * Given localization information and processing parameters,
		 * this function call will prepare this object to perform
		 * probabilistic carving of the scanned volume.
		 *
		 * @param madfile   The path file for this dataset
		 * @param confile   The xml hardware config file
		 * @param res       The carve resolution, in meters
		 * @param clk_err   The system clock uncertainty, in secs
		 *
		 * @return     Returns zero on success, non-zero on failure.
		 */
		int init(const std::string& madfile,
		         const std::string& confile,
		         double res, double clk_err);

		/**
		 * Carves all input scan points into the octree
		 *
		 * Given the path to a .fss scan file, will parse this
		 * file and import all scan points into the octree, using
		 * each scan to define which portions of the volume are
		 * interior and which are exterior.
		 *
		 * @param fssfile   The input scan file to parse and use
		 *
		 * @return    Returns zero on success, non-zero on failure.
		 */
		int carve(const std::string& fssfile);

		/**
		 * Exports the stored octree to a .oct file
		 *
		 * Will serialize this object's octree to the specified
		 * .oct file.  This file will contain the computed
		 * volumetric information.
		 *
		 * @param octfile   The path to the .oct file to export
		 *
		 * @return    Returns zero on success, non-zero on failure.
		 */
		int serialize(const std::string& octfile) const;
};

#endif
