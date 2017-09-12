#pragma once


/**
 * \struct Nuisances
 * \brief Nuisance parameters
 * 
 * Nuisance parameters that affect measurements of jet corrections are defined as publicly
 * accessible data members of this structure.
 */
struct Nuisances
{
    /**
     * \brief Default constructor
     * 
     * Initializes all nuisance parameters with values that correspond to the nominal configuration
     * (usually this means zeros).
     */
    Nuisances();
    
    // No nuisance parameters are defined at the moment
};
