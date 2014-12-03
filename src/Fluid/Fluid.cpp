#include "Fluid.hpp"
#include "Constants.hpp"
#include "GridFactory.hpp"
#include "MPI_Wrapper.hpp"

Star::Location calcContainingCore(int x, int xl, int xr, int rank) {
	Star::Location containing_core = Star::Location::HERE;
	if (x < xl)
		containing_core = Star::Location::LEFT;
	else if (x > xr)
		containing_core = Star::Location::RIGHT;
	return containing_core;
}

void Fluid::initialise(std::shared_ptr<Constants> c, FluidParameters fp) {
	consts = std::move(c);
	heatCapacityRatio = fp.heatCapacityRatio;
	massFractionH = fp.massFractionH;
}

void Fluid::initialiseGrid(GridParameters gp, StarParameters sp) {
	grid = GridFactory::createGrid(consts, gp, sp.position, sp.windCellRadius);

	Star::Location containing_core = Star::Location::HERE;
	if (sp.position[0] < grid.getLeftX())
		containing_core = Star::Location::LEFT;
	else if (sp.position[0] > grid.getRightX())
		containing_core = Star::Location::RIGHT;

	star.initialise(consts, sp, containing_core, grid.dx);
	star.setWindCells(grid.getWindCells());
}

/*
void Fluid::initialiseStar(StarParameters sp) {
	int containing_core = (int)(sp.position[0]/grid3D.coreCells[0]);
	std::array<double, 3> delta_x = grid.dx;

	star.initialise(consts, sp, containing_core, delta_x);

	std::pair<CellContainer, CellContainer> wind_and_causal = grid3D.zipList(star.xc, star.getWindCellRadius());
	star.setWindCells(wind_and_causal.first);
	star.setCausalCells(wind_and_causal.second);
}
*/
double Fluid::calcTemperature(double hii, double pre, double den) const {
	double mu_inv = massFractionH*(hii + 1.0) + (1.0 - massFractionH)*0.25;
	return (pre/den)*(1.0/mu_inv)/consts->specificGasConstant;
}

double Fluid::calcSoundSpeed(double gamma, double pre, double den) {
	return std::sqrt(gamma*pre/den);
}

void Fluid::advSolution(const double dt) {
	for (GridCell& cell : grid.getCells()) {
		for (int i = 0; i < UID::N; ++i) {
			cell.U[i] += dt*cell.UDOT[i];
			cell.UDOT[i] = 0;
		}
	}
}

void Fluid::fixSolution() {
	for (GridCell& cell : grid.getCells()) {
		if (!std::isfinite(cell.U[UID::DEN]) || !std::isfinite(cell.U[UID::PRE]))
			throw std::runtime_error("Fluid::fixSolution(): Density = " + std::to_string(cell.U[UID::DEN]) + ", Energy =" + std::to_string(cell.U[UID::PRE]));

		double hii = std::max(std::min(cell.U[UID::HII]/cell.U[UID::DEN], 1.0), 0.0);
		double v[3];

		double den = std::max(cell.U[UID::DEN], consts->dfloor);

		for (int dim = 0; dim < consts->nd; ++dim)
			v[dim] = cell.U[UID::VEL+dim]/cell.U[UID::DEN];

		double ke = 0.0;
		for(int dim = 0; dim < consts->nd; ++dim)
			ke += v[dim]*v[dim];
		ke *= 0.5*cell.U[UID::DEN];

		double pre = (cell.U[UID::PRE] - ke)*(cell.heatCapacityRatio - 1.0);
		ke *= den/cell.U[UID::DEN];

		if (pre < consts->pfloor) {
			pre = consts->pfloor;
		}

		double mu_inv = massFractionH*(hii + 1.0) + (1.0 - massFractionH)*0.25;
		double temperature = pre/(mu_inv*consts->specificGasConstant*den);
		if (temperature < consts->tfloor) {
			pre = mu_inv*consts->specificGasConstant*den*consts->tfloor;
		}

		cell.U[UID::DEN] = den;
		cell.U[UID::PRE] = pre/(cell.heatCapacityRatio - 1.0) + ke;
		cell.U[UID::HII] = hii*den;
		for (int dim = 0; dim < consts->nd; ++dim)
			cell.U[UID::VEL+dim] = den*v[dim];

		if (cell.U[UID::DEN] == 0 || cell.U[UID::PRE] == 0)
			throw std::runtime_error("Fluid::fixSolution: density or pressure is zero.\n" + cell.printInfo());

		for (double& v : cell.U) {
			if (v != v || std::isinf(v))
				throw std::runtime_error("Fluid::fixSolution: invalid value.\n" + cell.printInfo());
		}
	}
}

void Fluid::fixPrimitives() {
	for (GridCell& cell : getGrid().getCells()) {
		cell.Q[UID::HII] = std::max(std::min(cell.Q[UID::HII], 1.0), 0.0);
		cell.Q[UID::DEN] = std::max(cell.Q[UID::DEN], consts->dfloor);
		cell.Q[UID::PRE] = std::max(cell.Q[UID::PRE], consts->pfloor);
		double mu_inv = massFractionH*(cell.Q[UID::HII] + 1.0) + (1.0 - massFractionH)*0.25;
		double temperature = cell.Q[UID::PRE]/(mu_inv*consts->specificGasConstant*cell.Q[UID::DEN]);
		if (temperature < consts->tfloor) {
			cell.Q[UID::PRE] = mu_inv*consts->specificGasConstant*cell.U[UID::DEN]*consts->tfloor;
		}
	}
}

void Fluid::globalWfromU() const {
	for (GridCell& cell : grid.getCells())
		std::copy(std::begin(cell.U), std::end(cell.U), std::begin(cell.W));
}

void Fluid::globalUfromW() const {
	for(GridCell& cell : grid.getCells())
		std::copy(std::begin(cell.W), std::end(cell.W), std::begin(cell.U));
}

void Fluid::globalQfromU() const {
	for(GridCell& cell : grid.getCells())
		QfromU(cell.Q, cell.U, cell.heatCapacityRatio, consts->nd);
}

void Fluid::globalUfromQ() const {
	for(GridCell& cell : grid.getCells())
		UfromQ(cell.U, cell.Q, cell.heatCapacityRatio, consts->nd);
}

double Fluid::max(UID::ID id) const {
	double ret = 0;
	bool first = false;
	for (GridCell& cell : grid.getCells()) {
		if (first) {
			first = false;
			ret = cell.Q[id];
		}
		else {
			ret = cell.Q[id] > ret ? cell.Q[id] : ret;
		}
	}
	return ret;
}

double Fluid::maxTemperature() const {
	double ret = 0;
	for (GridCell& cell : grid.getCells()) {
		double T = calcTemperature(cell.Q[UID::HII], cell.Q[UID::PRE], cell.Q[UID::DEN]);
		ret = T > ret ? T : ret;
	}
	return ret;
}

double Fluid::minTemperature() const {
	double ret = 1.0e20;
	for (GridCell& cell : grid.getCells()) {
		double T = calcTemperature(cell.Q[UID::HII], cell.Q[UID::PRE], cell.Q[UID::DEN]);
		ret = T < ret ? T : ret;
	}
	return ret;
}

Grid& Fluid::getGrid() {
	return grid;
}

const Grid& Fluid::getGrid() const {
	return grid;
}

Star& Fluid::getStar() {
	return star;
}

const Star& Fluid::getStar() const {
	return star;
}
