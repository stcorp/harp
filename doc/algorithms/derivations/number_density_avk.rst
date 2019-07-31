number density averaging kernel derivations
===========================================

#. number density AVK of air component from column number density AVK:

   ====================== ============================================= =================================== ===========================================================
   symbol                 description                                   unit                                variable name
   ====================== ============================================= =================================== ===========================================================
   :math:`A^{c}_{x}(i,j)` AVK of column number density profile of air   :math:`\frac{molec/m^2}{molec/m^2}` `<species>_column_number_density_avk {:,vertical,vertical}`
                          component x (e.g. :math:`A^{c}_{O_{3}}(i,j)`)
   :math:`A^{n}_{x}(i,j)` AVK of number density profile of air          :math:`\frac{molec/m^3}{molec/m^3}` `<species>_number_density_avk {:,vertical,vertical}`
                          component x (e.g. :math:`A^{n}_{O_{3}}(i,j)`)
   :math:`z^{B}(i,l)`     altitude boundaries (:math:`l \in \{1,2\}`)   :math:`m`                           `altitude_bounds {:,vertical,2}`
   ====================== ============================================= =================================== ===========================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      A^{n}_{x}(i,j) = \begin{cases}
        z^{B}(i,1) \neq z^{B}(i,2), & A^{c}_{x}(i,j) \frac{\lvert z^{B}(j,2) - z^{B}(j,1) \rvert}{\lvert z^{B}(i,2) - z^{B}(i,1) \rvert} \\
        z^{B}(i,1) = z^{B}(i,2), & 0
      \end{cases}


#. number density AVK from volume mixing ratio AVK:

   ======================== =============================================== =================================== =========================================================
   symbol                   description                                     unit                                variable name
   ======================== =============================================== =================================== =========================================================
   :math:`A^{n}_{x}(i,j)`   AVK of number density profile of air            :math:`\frac{molec/m^3}{molec/m^3}` `<species>_number_density_avk {:,vertical,vertical}`
                            component x (e.g. :math:`A^{n}_{O_{3}}(i,j)`)
   :math:`A^{\nu}_{x}(i,j)` AVK of volume mixing ratio profile of air       :math:`\frac{ppv}{ppv}`             `<species>_volume_mixing_ratio_avk {:,vertical,vertical}`
                            component x (e.g. :math:`A^{\nu}_{O_{3}}(i,j)`)
   :math:`n(i)`             number density profile of total air             :math:`\frac{molec}{m^3}`           `number_density {:,vertical}`
   ======================== =============================================== =================================== =========================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      A^{n}_{x}(i,j) = \begin{cases}
        n(j) \neq 0, & A^{\nu}_{x}(i,j) \frac{n(i)}{n(j)} \\
        n(j) = 0, & 0
      \end{cases}


#. number density AVK from volume mixing ratio dry air AVK:

   ============================== ===================================================== =================================== =================================================================
   symbol                         description                                           unit                                variable name
   ============================== ===================================================== =================================== =================================================================
   :math:`A^{n}_{x}(i,j)`         AVK of number density profile of air                  :math:`\frac{molec/m^3}{molec/m^3}` `<species>_number_density_avk {:,vertical,vertical}`
                                  component x (e.g. :math:`A^{n}_{O_{3}}(i,j)`)
   :math:`A^{\bar{\nu}}_{x}(i,j)` AVK of volume mixing ratio profile of air             :math:`\frac{ppv}{ppv}`             `<species>_volume_mixing_ratio_dry_air_avk {:,vertical,vertical}`
                                  component x (e.g. :math:`A^{\bar{\nu}}_{O_{3}}(i,j)`)
   :math:`n_{dry_air}(i)`         number density profile of dry air                     :math:`\frac{molec}{m^3}`           `dry_air_number_density {:,vertical}`
   ============================== ===================================================== =================================== =================================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      A^{n}_{x}(i,j) = \begin{cases}
        n_{dry_air}(j) \neq 0, & A^{\bar{\nu}}_{x}(i,j) \frac{n_{dry_air}(i)}{n_{dry_air}(j)} \\
        n_{dry_air}(j) = 0, & 0
      \end{cases}
