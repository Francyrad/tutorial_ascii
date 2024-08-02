/*
  Copyright (C) 2024 by the authors of the ASPECT code.

  This file is part of ASPECT.

  ASPECT is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  ASPECT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ASPECT; see the file LICENSE.  If not see
  <http://www.gnu.org/licenses/>.
*/

#ifndef _aspect_material_model_reaction_grain_size_evolution_h
#define _aspect_material_model_reaction_grain_size_evolution_h

#include <aspect/material_model/interface.h>
#include <aspect/simulator_access.h>

namespace aspect
{
  namespace MaterialModel
  {
    using namespace dealii;

    namespace ReactionModel
    {
      /**
      * A model to calculate the change of the average grain size according to
      * different published grain size evolution models. We use the grain
      * size evolution laws described in Behn et al., 2009. Implications of grain
      * size evolution on the seismic structure of the oceanic upper mantle, Earth
      * Planet. Sci. Letters, 282, 178–189. Other material parameters are either
      * prescribed similar to the 'simple' material model, or read from data files
      * that were generated by the Perplex or Hefesto software. The material model
      * is described in more detail in Dannberg, J., Z. Eilon, U. Faul,
      * R. Gassmöller, P. Moulik, and R. Myhill (2017), The importance of
      * grain size to mantle dynamics and seismological observations,
      * Geochem. Geophys. Geosyst., 18, 3034–3061, doi:10.1002/2017GC006944.,
      * which is the canonical reference for this reaction model.
      *
      * @ingroup ReactionModel
      */
      template <int dim>
      class GrainSizeEvolution : public ::aspect::SimulatorAccess<dim>
      {
        public:
          /**
           * Receive the phase function object that is used to determine if
           * material crossed a phase transition and grain size needs to be
           * reset.
           */
          void initialize_phase_function(std::shared_ptr<MaterialUtilities::PhaseFunction<dim>> &phase_function);

          /**
           * Rate of grain size growth (Ostwald ripening) or reduction
           * (due to dynamic recrystallization and phase transformations)
           * depends on temperature, pressure, strain rate, mineral
           * phase and creep regime.
           * We use the grain size growth laws as described
           * in Behn, M. D., Hirth, G., & Elsenbeck, J. R. (2009). Implications
           * of grain size evolution on the seismic structure of the oceanic
           * upper mantle. Earth and Planetary Science Letters, 282(1), 178-189.
           *
           * For the rate of grain size reduction due to dynamic crystallization
           * there is a choice between different models as described in
           * the struct Formulation.
           *
           * @note The functions @p dislocation_creep and @p diffusion_creep currently
           * follow the interface of the respective functions in the grain size
           * material model. This may be changed in the future to match the
           * interface of the DiffusionCreep and DislocationCreep rheology modules.
           */
          void
          calculate_reaction_terms (const typename Interface<dim>::MaterialModelInputs  &in,
                                    const std::vector<double>                           &adiabatic_pressure,
                                    const std::vector<unsigned int>                     &phase_indices,
                                    const std::function<double(const double,const double,
                                                               const double,const SymmetricTensor<2,dim> &,const unsigned int,const double, const double)>          &dislocation_viscosity,
                                    const std::function<double(
                                      const double, const double, const double,const double,const double,const unsigned int)> &diffusion_viscosity,
                                    const double                                         min_eta,
                                    const double                                         max_eta,
                                    typename Interface<dim>::MaterialModelOutputs       &out) const;

          /**
           * Create the additional material model outputs produced by this reaction model.
           * In this case the function creates ShearHeatingOutputs, which reduce the amount
           * of work done to produce shear heat to account for the surface energy used to
           * reduce the grain size.
           */
          void
          create_additional_named_outputs (MaterialModel::MaterialModelOutputs<dim> &out) const;

          /**
           * Fill the additional output objects created by the function
           * create_additional_named_outputs. Note that although the plugin gets
           * write access to all AdditionalMaterialOutputs it should only modify the
           * ones it created.
           */
          void
          fill_additional_outputs (const typename MaterialModel::MaterialModelInputs<dim> &in,
                                   const typename MaterialModel::MaterialModelOutputs<dim> &out,
                                   const std::vector<unsigned int> &phase_indices,
                                   const std::vector<double> &dislocation_viscosities,
                                   std::vector<std::unique_ptr<MaterialModel::AdditionalMaterialOutputs<dim>>> &additional_outputs) const;

          /**
          * Declare the parameters this function takes through input files.
          */
          static
          void
          declare_parameters (ParameterHandler &prm);

          /**
           * Read the parameters from the parameter file.
           */
          void
          parse_parameters (ParameterHandler &prm);

        private:
          /**
           * Parameters controlling the grain size evolution.
           */
          std::vector<double> grain_growth_activation_energy;
          std::vector<double> grain_growth_activation_volume;
          std::vector<double> grain_growth_rate_constant;
          std::vector<double> grain_growth_exponent;
          double              minimum_grain_size;
          std::vector<double> reciprocal_required_strain;
          std::vector<double> recrystallized_grain_size;

          /**
           * Parameters controlling the dynamic grain recrystallization.
           */
          std::vector<double> grain_boundary_energy;
          std::vector<double> boundary_area_change_work_fraction;
          std::vector<double> geometric_constant;

          /**
            * A struct that contains information about which
            * formulation of grain size evolution should be used.
            */
          struct Formulation
          {
            /**
             * This enum lists available options that
             * determine the equations being used for grain size evolution.
             *
             * We currently support three approaches:
             *
             * 'paleowattmeter':
             * Austin, N. J., & Evans, B. (2007). Paleowattmeters: A scaling
             * relation for dynamically recrystallized grain size. Geology, 354), 343-346.).
             *
             * 'paleopiezometer':
             * Hall, C. E., Parmentier, E. M. (2003). Influence of grain size
             * evolution on convective instability. Geochemistry, Geophysics,
             * Geosystems, 4(3).
             *
             * 'pinned_grain_damage':
             * Mulyukova, E., & Bercovici, D. (2018). Collapse of passive margins
             * by lithospheric damage and plunging grain size. Earth and Planetary
             * Science Letters, 484, 341-352.
             */
            enum Kind
            {
              paleowattmeter,
              paleopiezometer,
              pinned_grain_damage
            };

            /**
             * This function translates an input string into the
             * available enum options.
             */
            static
            Kind
            parse(const std::string &input)
            {
              if (input == "paleowattmeter")
                return Formulation::paleowattmeter;
              else if (input == "paleopiezometer")
                return Formulation::paleopiezometer;
              else if (input == "pinned grain damage")
                return Formulation::pinned_grain_damage;
              else
                AssertThrow(false, ExcNotImplemented());

              return Formulation::Kind();
            }
          };

          /**
           * A variable that records the formulation of how to evolve grain size.
           * See the type of this variable for a description of available options.
          */
          typename Formulation::Kind grain_size_evolution_formulation;

          /**
          * This function returns the fraction of shear heating energy partitioned
          * into grain damage using the implementation by Mulyukova and Bercovici (2018)
          * Collapse of passive margins by lithospheric damage
          * and plunging grain size. Earth and Planetary Science Letters, 484, 341-352.
          */
          double compute_partitioning_fraction (const double temperature) const;

          /**
           * Parameters controlling the partitioning of energy
           * into grain damage in the pinned state.
           */
          double grain_size_reduction_work_fraction_exponent;
          double minimum_grain_size_reduction_work_fraction;
          double maximum_grain_size_reduction_work_fraction;
          double temperature_minimum_partitioning_power;
          double temperature_maximum_partitioning_power;

          /**
           * Functions and parameters controlling conversion from interface roughness to grain size,
           * used in pinned state formulation of grain damage. This conversion depends on the
           * proportion of the two mineral phases.
           *
           * A detailed description of this approach can be found in Appendix H.1, in Equation (8) in
           * the main manuscript, and in equation (F.28) of Bercovici and Ricard (2012).
           * Mechanisms for the generation of plate tectonics by two-phase grain-damage
           * and pinning. Physics of the Earth and Planetary Interiors 202 (2012): 27-55.
           */
          double phase_distribution;

          /**
           * The factor used to convert interface roughness into the equivalent mean grain
           * size for a given volume fraction of a mineral in the two-phase damage model.
           * "Interface roughness" (or radius of curvature) is a measure for how much
           * interface area is present in a collection of mineral grains and therefore
           * dependent on the average grain size. For a discussion see
           * Bercovici and Ricard (2012), Appendix H.1.
          */
          double roughness_to_grain_size;

          /**
           * Object that handles phase transitions.
           * Allows it to compute the phase function for each individual phase
           * transition in the model, given the temperature, pressure, depth,
           * and density gradient. This variable is a shared pointer so that
           * material model and reaction model can share the same phase function.
           * You can initialize the phase function in the material model and pass
           * it to the reaction model using the function initialize_phase_function.
           */
          std::shared_ptr<MaterialUtilities::PhaseFunction<dim>> phase_function;

          /**
           * Cache the total number of phase transitions defined in phase_function.
           */
          unsigned int n_phase_transitions;
      };
    }

  }
}

#endif