/*
  Copyright (C) 2011 - 2023 by the authors of the ASPECT code.

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


#include "heat_flux_statistics_gradient.h"
#include <aspect/utilities.h>
#include <aspect/geometry_model/interface.h>

#include <deal.II/base/quadrature_lib.h>
#include <deal.II/fe/fe_values.h>


namespace aspect
{
  namespace Postprocess
  {

    template <int dim>
    std::pair<std::string,std::string>
    HeatFluxStatisticsGradient<dim>::execute (TableHandler &statistics)
    {
      // create a quadrature formula based on the temperature element alone.
      const QGauss<dim-1> quadrature_formula (this->get_fe().base_element(this->introspection().base_elements.temperature).degree+1);

      FEFaceValues<dim> fe_face_values (this->get_mapping(),
                                        this->get_fe(),
                                        quadrature_formula,
                                        update_gradients      | update_values |
                                        update_normal_vectors |
                                        update_quadrature_points       | update_JxW_values);

      std::vector<Tensor<1,dim>> temperature_gradients (quadrature_formula.size());
      std::vector<std::vector<double>> composition_values (this->n_compositional_fields(),std::vector<double> (quadrature_formula.size()));

      std::map<types::boundary_id, double> local_boundary_fluxes;

      MaterialModel::MaterialModelInputs<dim> in(fe_face_values.n_quadrature_points, this->n_compositional_fields());
      MaterialModel::MaterialModelOutputs<dim> out(fe_face_values.n_quadrature_points, this->n_compositional_fields());

      // for every surface face on which it makes sense to compute a
      // heat flux and that is owned by this processor,
      // integrate the normal heat flux given by the formula
      //   j =  - k * n . grad T
      //
      // for the spherical shell geometry, note that for the inner boundary,
      // the normal vector points *into* the core, i.e. we compute the flux
      // *out* of the mantle, not into it. we fix this when we add the local
      // contribution to the global flux
      for (const auto &cell : this->get_dof_handler().active_cell_iterators())
        if (cell->is_locally_owned())
          for (unsigned int f=0; f<GeometryInfo<dim>::faces_per_cell; ++f)
            if (cell->at_boundary(f))
              {
                fe_face_values.reinit (cell, f);
                in.reinit(fe_face_values, cell, this->introspection(), this->get_solution());

                this->get_material_model().evaluate(in, out);

                // Get the temperature gradients from the solution.
                fe_face_values[this->introspection().extractors.temperature].get_function_gradients (this->get_solution(), temperature_gradients);

                double local_normal_flux = 0;
                for (unsigned int q=0; q<fe_face_values.n_quadrature_points; ++q)
                  {
                    const double thermal_conductivity
                      = out.thermal_conductivities[q];

                    local_normal_flux
                    +=
                      -thermal_conductivity *
                      (temperature_gradients[q] *
                       fe_face_values.normal_vector(q)) *
                      fe_face_values.JxW(q);
                  }

                const types::boundary_id boundary_indicator
                  = cell->face(f)->boundary_id();
                local_boundary_fluxes[boundary_indicator] += local_normal_flux;
              }

      // now communicate to get the global values
      std::map<types::boundary_id, double> global_boundary_fluxes;
      {
        // first collect local values in the same order in which they are listed
        // in the set of boundary indicators
        const std::set<types::boundary_id>
        boundary_indicators
          = this->get_geometry_model().get_used_boundary_indicators ();
        std::vector<double> local_values;
        for (std::set<types::boundary_id>::const_iterator
             p = boundary_indicators.begin();
             p != boundary_indicators.end(); ++p)
          local_values.push_back (local_boundary_fluxes[*p]);

        // then collect contributions from all processors
        std::vector<double> global_values (local_values.size());
        Utilities::MPI::sum (local_values, this->get_mpi_communicator(), global_values);

        // and now take them apart into the global map again
        unsigned int index = 0;
        for (std::set<types::boundary_id>::const_iterator
             p = boundary_indicators.begin();
             p != boundary_indicators.end(); ++p, ++index)
          global_boundary_fluxes[*p] = global_values[index];
      }

      // now add all of the computed heat fluxes to the statistics object
      // and create a single string that can be output to the screen
      std::ostringstream screen_text;
      unsigned int index = 0;
      for (std::map<types::boundary_id, double>::const_iterator
           p = global_boundary_fluxes.begin();
           p != global_boundary_fluxes.end(); ++p, ++index)
        {
          const std::string name = "Outward heat flux (gradient) through boundary with indicator "
                                   + Utilities::int_to_string(p->first)
                                   + aspect::Utilities::parenthesize_if_nonempty(this->get_geometry_model()
                                                                                 .translate_id_to_symbol_name (p->first))
                                   + " (W)";
          statistics.add_value (name, p->second);

          // also make sure that the other columns filled by the this object
          // all show up with sufficient accuracy and in scientific notation
          statistics.set_precision (name, 8);
          statistics.set_scientific (name, true);

          // finally have something for the screen
          screen_text.precision(4);
          screen_text << p->second << " W"
                      << (index == global_boundary_fluxes.size()-1 ? "" : ", ");
        }

      return std::pair<std::string, std::string> ("Heat fluxes (gradient) through boundary parts:",
                                                  screen_text.str());
    }
  }
}


// explicit instantiations
namespace aspect
{
  namespace Postprocess
  {
    ASPECT_REGISTER_POSTPROCESSOR(HeatFluxStatisticsGradient,
                                  "heat flux statistics gradient",
                                  "A postprocessor that computes some statistics about "
                                  "the (conductive) heat flux across boundaries. For each boundary "
                                  "indicator (see your geometry description for which boundary "
                                  "indicators are used), the heat flux is computed in outward "
                                  "direction, i.e., from the domain to the outside, using the "
                                  "formula $\\int_{\\Gamma_i} -k \\nabla T \\cdot \\mathbf n$ "
                                  "where $\\Gamma_i$ is the part of the boundary with indicator $i$, "
                                  "$k$ is the thermal conductivity as reported by the material model, "
                                  "$T$ is the temperature, and $\\mathbf n$ is the outward normal. "
                                  "Note that the quantity so computed does not include any energy "
                                  "transported across the boundary by material transport in cases "
                                  "where $\\mathbf u \\cdot \\mathbf n \\neq 0$. The point-wise "
                                  "heat flux can be obtained from the heat flux map postprocessor, "
                                  "which outputs the heat flux to a file, or the heat flux map "
                                  "visualization postprocessor, which outputs the heat flux for "
                                  "visualization. "
                                  "\n\n"
                                  "As stated, this postprocessor computes the \\textit{outbound} heat "
                                  "flux. If you "
                                  "are interested in the opposite direction, for example from "
                                  "the core into the mantle when the domain describes the "
                                  "mantle, then you need to multiply the result by -1."
                                  "\n\n"
                                  "\\note{In geodynamics, the term ``heat flux'' is often understood "
                                  "to be the quantity $- k \\nabla T$, which is really a heat "
                                  "flux \\textit{density}, i.e., a vector-valued field. In contrast "
                                  "to this, the current postprocessor only computes the integrated "
                                  "flux over each part of the boundary. Consequently, the units of "
                                  "the quantity computed here are $W=\\frac{J}{s}$.}"
                                  "\n\n"
                                  "The ``heat flux densities'' postprocessor computes the same "
                                  "quantity as the one here, but divided by the area of "
                                  "the surface.")
  }
}
