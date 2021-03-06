/**
 * Provides the GridCell and GridJoin classes.
 * @file GridCell.hpp
 *
 * @author Harrison Steggles
 * @date 13/01/2014 - the first version.
 * @date 16/01/2014 - removed old Boundary class.
 * @date 04/02/2014 - GridCell now has pointer to next GridCell in causal iteration (nextcausal). UL & UR unused states removed.
 * @date 04/02/2014 - arguments now passed by const reference when appropriate.
 * @date 12/02/2014 - printInfo method added for debugging.
 * @date 13/02/2014 - GridCell now has pointers to nearest neighbouring GridCells, their associated weights for interpolating
 * column density, the shell volume and cell crossing length used in radiative transfer methods. Get a speed-up of 27% by storing
 * these rather than calculating them at each step.
 * @date 24/11/2014 - Conservative, Primitive and Flux conversion methods are now located here.
 * @date 24/11/2014 - GridCell and GridJoin now have IntrusiveContainer<GridCell>::intrusive_hook members so they can be contained in
 * an IntrusiveContainer<GridCell>. The next and
 * @date 24/11/2014 - raw arrays are now std::arrays.
 * @date 24/11/2014 - each GridCell has a heatCapacityRatio.
 */

#ifndef GRIDCELL_HPP_
#define GRIDCELL_HPP_

#include <array>
#include <string>

#include "Torch/Common.hpp"

void UfromQ(FluidArray& u, const FluidArray& q, double gamma, int nd);
void QfromU(FluidArray& q, const FluidArray& u, double gamma, int nd);
void FfromQ(FluidArray& f, const FluidArray& u, double gamma, int nd, int dim);
void FfromU(FluidArray& f, const FluidArray& u, double gamma, int nd, int dim);

class GridJoin;
class GridCell;
/**
 * @class GridCell
 *
 * @brief A GridCell holds fluid and radiation state information and geometric properties (volume, shell volume and cell path length).
 *
 * @version 0.8, 24/11/2014
 */
class GridCell {
public:
	std::array<int, 3> rjoinID = std::array<int, 3> {{ -1, -1, -1 }}; //!< Contains pointers to GridJoins that lie on the right side of this GridCell.
	std::array<int, 3> ljoinID = std::array<int, 3> {{ -1, -1, -1 }}; //!< Contains pointers to GridJoins that lie on the left side of this GridCell.
	std::array<int, 3> rightID = std::array<int, 3> {{ -1, -1, -1 }}; //!< Contains pointers to GridCells that lie on the right side of this GridCell.
	std::array<int, 3> leftID = std::array<int, 3> {{ -1, -1, -1 }}; //!< Contains pointers to GridCells that lie on the left side of this GridCell.
	std::array<double, 3> GRAV;
	FluidArray UDOT; //!< Contains rate of change of conservative fluid variable values.
	FluidArray U; //!< Contains conservative fluid variable values.
	FluidArray Q; //!< Contains primitive fluid variable values.
	FluidArray W; //!< Contains a copy of GridCell::U for 2nd order time-stepping.
	RadArray R; //!< Contains radiation variable values: optical depth in cell and along path of the ray from source.
	ThermoArray T; //!< Contains thermadynamic variable values.
	HeatArray H;
	Vec3 xc = Vec3{{ -10, -10, -10 }}; //!< Grid coordinates for this GridCell.
	Array2D<double, 3, UID::N> QL; //!< Reconstructed states on left faces.
	Array2D<double, 3, UID::N> QR; //!< Reconstructed states on right faces.
	double vol = 0; //!< Volume of GridCell.
	double ds = 0; //!< Length of segment of the ray that passed from the Star through the centre of this cell.
	double shellVol = 0; //!< Volume of spherical shell with radius from Star to this GridCell and width this->ds.
	double heatCapacityRatio = 0;
	double m_soundSpeed = 0;
	double T_min = 0; //!< Minimum temperature of this cell set by initial conditions.
	std::array<int, 4> neighbourIDs = std::array<int, 4> {{ -1, -1, -1, -1 }}; //!< the GridCell IDs (see Grid) of the neighbouring GridCells that are used to calculate this cell's optical depth.
	std::array<double, 4> neighbourWeights = std::array<double, 4> {{ 0, 0, 0, 0 }}; //!< Weighting of each neighbouring cell's contribution to the optical depth to this cell.

	// Structors.
	GridCell();

	// Debugging.
	std::string printCoords() const;
	std::string printInfo() const;

	// Getters/Setters.
	void set_U(const int index, const double value);
	void set_xcs(const double x, const double y, const double z);
	double get_xc(const int i);
	double get_U(const int index);
	void setSoundSpeed(double a);
	double getSoundSpeed() const;

	// Misc. methods.
	double temperature(const double massFracH, const double specGasConst) const;
};

/**
 * @class GridJoin
 * @brief The GridJoin class contains flux information.
 *
 * A GridJoin holds links to GridCells that lie either side of it and contains flux information. The GridCells either side of this will receive the flux in this GridJoin, which is calculated by the Riemann solver in the Hydro integrator.
 *
 * @see GridCell
 */
class GridJoin{
public:
	GridJoin();

	int lcellID = -1; //!< Pointer to GridCell on the left.
	int rcellID = -1; //!< Pointer to GridCell on the right.
	FluidArray F; //!< Contains flux to be added to GridJoin::rcell and subtracted from GridJoin::lcell fluid variables (GridCell::U).
	Vec3 xj = Vec3{{ 0, 0, 0 }}; //!< The grid coordinates of the GridJoin in a Grid.
	double area = 0; //!< The area of the GridJoin.
};

#endif // GRIDCELL_HPP_
