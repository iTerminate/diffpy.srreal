/*****************************************************************************
*
* diffpy.srreal     by DANSE Diffraction group
*                   Simon J. L. Billinge
*                   (c) 2009 Trustees of the Columbia University
*                   in the City of New York.  All rights reserved.
*
* File coded by:    Pavol Juhas
*
* See AUTHORS.txt for a list of people who contributed.
* See LICENSE.txt for license information.
*
******************************************************************************
*
* class DiffPyStructureAdapter -- adapter to the Structure class from the
*     Python diffpy.Structure package.
* class DiffPyStructureBondGenerator -- related bond generator
*     
*
* $Id$
*
*****************************************************************************/

#include <cassert>
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>
#include <diffpy/srreal/DiffPyStructureAdapter.hpp>
#include <diffpy/srreal/Lattice.hpp>
#include <diffpy/srreal/PointsInSphere.hpp>

using namespace std;
using namespace boost;
using namespace diffpy::srreal;

//////////////////////////////////////////////////////////////////////////////
// class DiffPyStructureAdapter
//////////////////////////////////////////////////////////////////////////////

// Constructor ---------------------------------------------------------------

DiffPyStructureAdapter::DiffPyStructureAdapter(const python::object& dpstru)
{
    mdpstructure = &dpstru;
    this->fetchPythonData();
}

// Public Methods ------------------------------------------------------------

int DiffPyStructureAdapter::countSites() const
{
    return mcartesian_positions.size();
}


BaseBondGenerator* DiffPyStructureAdapter::createBondGenerator() const
{
    BaseBondGenerator* bnds = new DiffPyStructureBondGenerator(this);
    return bnds;
}

// Protected Methods ---------------------------------------------------------

const Lattice& DiffPyStructureAdapter::getLattice() const
{
    assert(mlattice.get());
    return *mlattice;
}


const R3::Vector& DiffPyStructureAdapter::siteCartesianPosition(int idx) const
{
    assert(0 <= idx && idx < this->countSites());
    return mcartesian_positions[idx];
}


bool DiffPyStructureAdapter::siteAnisotropy(int idx) const
{
    assert(0 <= idx && idx < this->countSites());
    return manisotropies[idx];
}


const R3::Matrix& DiffPyStructureAdapter::siteCartesianUij(int idx) const
{
    assert(0 <= idx && idx < this->countSites());
    return mcartesian_uijs[idx];
}


const string& DiffPyStructureAdapter::siteAtomType(int idx) const
{
    assert(0 <= idx && idx < this->countSites());
    return matomtypes[idx];
}


void DiffPyStructureAdapter::fetchPythonData()
{
    // mlattice
    python::object lattice;
    lattice = mdpstructure->attr("lattice");
    double a, b, c, alpha, beta, gamma;
    a = python::extract<double>(lattice.attr("a"));
    b = python::extract<double>(lattice.attr("b"));
    c = python::extract<double>(lattice.attr("c"));
    alpha = python::extract<double>(lattice.attr("alpha"));
    beta = python::extract<double>(lattice.attr("beta"));
    gamma = python::extract<double>(lattice.attr("gamma"));
    // atom properties
    mcartesian_positions.clear();
    manisotropies.clear();
    mcartesian_uijs.clear();
    matomtypes.clear();
    int num_atoms = python::len(*mdpstructure);
    for (int i = 0; i < num_atoms; ++i)
    {
        python::object ai;
        ai = mdpstructure[i];
        // mcartesian_positions
        python::object xyz = ai.attr("xyz");
        double x, y, z;
        x = python::extract<double>(xyz[0]);
        y = python::extract<double>(xyz[1]);
        z = python::extract<double>(xyz[2]);
        R3::Vector xyz_frac(x, y, z);
        R3::Vector xyz_cartn = mlattice->cartesian(xyz_frac);
        mcartesian_positions.push_back(xyz_cartn);
        // manisotropies
        bool aniso;
        aniso = python::extract<bool>(ai.attr("anisotropy"));
        manisotropies.push_back(aniso);
        // mcartesian_uijs
        R3::Matrix Ufrac;
        python::object uflat = ai.attr("U").attr("flat");
        python::stl_input_iterator<double> ufirst(uflat), ulast;
        std::copy(ufirst, ulast, Ufrac.data());
        R3::Matrix Ucart = mlattice->cartesianMatrix(Ufrac);
        mcartesian_uijs.push_back(Ucart);
        // matomtypes
        string atp = python::extract<string>(ai.attr("element"));
        matomtypes.push_back(atp);
    }
    assert(int(mcartesian_positions.size()) == this->countSites());
    assert(int(mcartesian_positions.size()) == this->countSites());
    assert(int(manisotropies.size()) == this->countSites());
    assert(int(mcartesian_uijs.size()) == this->countSites());
    assert(int(matomtypes.size()) == this->countSites());
}

//////////////////////////////////////////////////////////////////////////////
// class DiffPyStructureBondGenerator
//////////////////////////////////////////////////////////////////////////////

// Constructor ---------------------------------------------------------------

DiffPyStructureBondGenerator::DiffPyStructureBondGenerator(
        const DiffPyStructureAdapter* adpt) : BaseBondGenerator(adpt)
{
    mdpstructure = adpt;
    const Lattice& L = mdpstructure->getLattice();
    msphere.reset(new PointsInSphere(0.0, 0.0, L));
    this->includeSelfPairs(true);
}

// Public Methods ------------------------------------------------------------

const R3::Vector& DiffPyStructureBondGenerator::r0() const
{
    return mr0ucv;
}


const R3::Vector& DiffPyStructureBondGenerator::r1() const
{
    static R3::Vector rv;
    rv = mr1ucv + msphere->r();
    return rv;
}


double DiffPyStructureBondGenerator::msd0() const
{
    const R3::Matrix& Uijcartn = mdpstructure->siteCartesianUij(msite_anchor);
    const R3::Vector& s = this->r01();
    bool anisotropy = mdpstructure->siteAnisotropy(msite_anchor);
    double rv = meanSquareDisplacement(Uijcartn, s, anisotropy);
    return rv;
}


double DiffPyStructureBondGenerator::msd1() const
{
    const R3::Matrix& Uijcartn = mdpstructure->siteCartesianUij(msite_current);
    const R3::Vector& s = this->r01();
    bool anisotropy = mdpstructure->siteAnisotropy(msite_current);
    double rv = meanSquareDisplacement(Uijcartn, s, anisotropy);
    return rv;
}

//////////////////////////////////////////////////////////////////////////////
// Routines
//////////////////////////////////////////////////////////////////////////////

namespace diffpy {
namespace srreal {

double meanSquareDisplacement(const R3::Matrix& Uijcartn,
        const R3::Vector& s, bool anisotropy)
{
    double rv;
    if (anisotropy)
    {
        assert(R3::norm(s) > 0);
        assert(Uijcartn(0,1) == Uijcartn(1,0));
        assert(Uijcartn(0,2) == Uijcartn(2,0));
        assert(Uijcartn(1,2) == Uijcartn(2,1));
        static R3::Vector sn;
        sn = s / R3::norm(s);
        rv = Uijcartn(0,0) * sn(0) * sn(0) +
             Uijcartn(1,1) * sn(1) * sn(1) +
             Uijcartn(2,2) * sn(2) * sn(2) +
             2 * Uijcartn(0,1) * sn(0) * sn(1) +
             2 * Uijcartn(0,2) * sn(0) * sn(2) +
             2 * Uijcartn(1,2) * sn(1) * sn(2);
    }
    else
    {
        assert(Uijcartn(0,0) == Uijcartn(1,1));
        assert(Uijcartn(0,0) == Uijcartn(2,2));
        rv = Uijcartn(0,0);
    }
    return rv;
}

}   // namespace srreal
}   // namespace diffpy
        
// End of file
