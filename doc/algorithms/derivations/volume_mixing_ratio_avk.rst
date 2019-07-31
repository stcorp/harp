volume mixing ratio averaging kernel derivations
================================================

#. volume mixing ratio AVK from number density AVK:

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

      A^{\nu}_{x}(i,j) = \begin{cases}
        n(i) \neq 0, & A^{n}_{x}(i,j) \frac{n(j)}{n(i)} \\
        n(i) = 0, & 0
      \end{cases}


#. volume mixing ratio dry air AVK from number density AVK:

   ============================== ===================================================== =================================== =================================================================
   symbol                         description                                           unit                                variable name
   ============================== ===================================================== =================================== =================================================================
   :math:`A^{n}_{x}(i,j)`         AVK of number density profile of air                  :math:`\frac{molec/m^3}{molec/m^3}` `<species>_number_density_avk {:,vertical,vertical}`
                                  component x (e.g. :math:`A^{n}_{O_{3}}(i,j)`)
   :math:`A^{\bar{\nu}}_{x}(i,j)` AVK of volume mixing ratio profile of air component   :math:`\frac{ppv}{ppv}`             `<species>_volume_mixing_ratio_dry_air_avk {:,vertical,vertical}`
                                  x with regard to dry air
                                  (e.g. :math:`A^{\bar{\nu}}_{O_{3}}(i,j)`)
   :math:`n_{dry_air}(i)`         number density profile of dry air                     :math:`\frac{molec}{m^3}`           `dry_air_number_density {:,vertical}`
   ============================== ===================================================== =================================== =================================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      A^{\bar{\nu}}_{x}(i,j) = \begin{cases}
        n_{dry_air}(i) \neq 0, & A^{n}_{x}(i,j) \frac{n_{dry_air}(j)}{n_{dry_air}(i)} \\
        n_{dry_air}(i) = 0, & 0
      \end{cases}
