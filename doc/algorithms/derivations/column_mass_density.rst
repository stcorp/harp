column mass density derivations
===============================

   .. _derivation_column_mass_density_of_total_air_from_dry_air_column_mass_density_and_H2O_column_mass_density:

#. column mass density of total air from dry air column mass density and H2O column mass density

   ========================= ============================== ====================== ============================
   symbol                    description                    unit                   variable name
   ========================= ============================== ====================== ============================
   :math:`\sigma`            column mass density            :math:`\frac{kg}{m^2}` `column_density {:}`
   :math:`\sigma_{dry\_air}` column mass density of dry air :math:`\frac{kg}{m^2}` `dry_air_column_density {:}`
   :math:`\sigma_{H_{2}O}`   column mass density of H2O     :math:`\frac{kg}{m^2}` `H2O_column_density {:}`
   ========================= ============================== ====================== ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma = \sigma_{dry\_air} + \sigma_{H_{2}O}


   .. _derivation_column_mass_density_of_dry_air_from_total_air_column_mass_density_and_H2O_column_mass_density:

#. column mass density of dry air from total air column mass density and H2O column mass density

   ========================= ============================== ====================== ============================
   symbol                    description                    unit                   variable name
   ========================= ============================== ====================== ============================
   :math:`\sigma`            column mass density            :math:`\frac{kg}{m^2}` `column_density {:}`
   :math:`\sigma_{dry\_air}` column mass density of dry air :math:`\frac{kg}{m^2}` `dry_air_column_density {:}`
   :math:`\sigma_{H_{2}O}`   column mass density of H2O     :math:`\frac{kg}{m^2}` `H2O_column_density {:}`
   ========================= ============================== ====================== ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{dry\_air} = \sigma - \sigma_{H_{2}O}


   .. _derivation_column_mass_density_of_H2O_from_total_air_column_mass_density_and_dry_air_column_mass_density:

#. column mass density of H2O from total air column mass density and dry air column mass density

   ========================= ============================== ====================== ============================
   symbol                    description                    unit                   variable name
   ========================= ============================== ====================== ============================
   :math:`\sigma`            column mass density            :math:`\frac{kg}{m^2}` `column_density {:}`
   :math:`\sigma_{dry\_air}` column mass density of dry air :math:`\frac{kg}{m^2}` `dry_air_column_density {:}`
   :math:`\sigma_{H_{2}O}`   column mass density of H2O     :math:`\frac{kg}{m^2}` `H2O_column_density {:}`
   ========================= ============================== ====================== ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{H_{2}O} = \sigma - \sigma_{dry\_air}


   .. _derivation_column_mass_density_of_air_component_from_mass_density:

#. column mass density of air component from mass density

   ================== =========================================== ====================== ==============================
   symbol             description                                 unit                   variable name
   ================== =========================================== ====================== ==============================
   :math:`z^{B}(l)`   altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho_{x}`   mass density of air component x             :math:`\frac{kg}{m^3}` `<species>_density {:}`
                      (e.g. :math:`\rho_{O_{3}}`)
   :math:`\sigma_{x}` column mass density of air component x      :math:`\frac{kg}{m^2}` `<species>_column_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   ================== =========================================== ====================== ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{x} = \rho_{x} \lvert z^{B}(2) - z^{B}(1) \rvert


   .. _derivation_column_mass_density_of_total_air_from_mass_density:

#. column mass density of total air from mass density

   ================ =========================================== ====================== =======================
   symbol           description                                 unit                   variable name
   ================ =========================================== ====================== =======================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho`     mass density of total  air                  :math:`\frac{kg}{m^3}` `density {:}`
   :math:`\sigma`   column mass density of total air            :math:`\frac{kg}{m^2}` `column_density {:}`
   ================ =========================================== ====================== =======================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma = \rho \lvert z^{B}(2) - z^{B}(1) \rvert


   .. _derivation_column_mass_density_of_air_component_from_column_number_density:

#. column mass density of air component from column number density

   This conversion applies to both total columns as well as partial column profiles.

   ================== ======================================== ========================= =====================================
   symbol             description                              unit                      variable name
   ================== ======================================== ========================= =====================================
   :math:`c_{x}`      column number density of air component x :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   :math:`M_{x}`      molar mass of air component x            :math:`\frac{g}{mol}`
   :math:`N_A`        Avogadro constant                        :math:`\frac{1}{mol}`
   :math:`\sigma_{x}` column mass density of air component x   :math:`\frac{kg}{m^2}`    `<species>_column_density {:}`
                      (e.g. :math:`\sigma_{O_{3}}`)
   ================== ======================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma_{x} = \frac{10^{-3}c_{x}M_{x}}{N_{A}}


   .. _derivation_column_mass_density_of_total_air_from_column_number_density:

#. column mass density of total air from column number density

   This conversion applies to both total columns as well as partial column profiles.

   =============== ================================== ========================= ===========================
   symbol          description                        unit                      variable name
   =============== ================================== ========================= ===========================
   :math:`c`       column number density of total air :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`M_{air}` molar mass of total air            :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`N_A`     Avogadro constant                  :math:`\frac{1}{mol}`
   :math:`\sigma`  column mass density of total air   :math:`\frac{kg}{m^2}`    `column_density {:}`
   =============== ================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma = \frac{10^{-3}c M_{air}}{N_{A}}


   .. _derivation_column_mass_density_of_total_air_from_pressure_profile_and_surface_pressure:

#. column mass density of total air from pressure profile and surface pressure

   ================== ================================ ====================== ================================
   symbol             description                      unit                   variable name
   ================== ================================ ====================== ================================
   :math:`\bar{g}`    mean gravity of profile          :math:`\frac{m}{s^2}`
   :math:`g`          nominal gravity at sea evel      :math:`\frac{m}{s^2}`
   :math:`g_{h}`      gravity at specific heiht        :math:`\frac{m}{s^2}`
   :math:`p^{B}(i,l)` pressure boundaries              :math:`Pa`             `pressure_bounds {:,vertical,2}`
                      (:math:`l \in \{1,2\}`)
   :math:`p_{surf}`   surface pressure                 :math:`Pa`             `surface_pressure {:}`
   :math:`R`          local earth curvature radius     :math:`m`
   :math:`z(i)`       altitude                         :math:`m`              `altitude {:,vertical}`
   :math:`\phi`       latitude                         :math:`degN`           `latitude {:}`
   :math:`\sigma`     column mass density of total air :math:`\frac{kg}{m^2}` `column_density {:}`
   ================== ================================ ====================== ================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         g_{h}(i) & = & g\left(\frac{R}{R + z(i)}\right)^2 \\
         \bar{g} & = & \frac{\sum_{i}{p^{B}(i,0)-p^{B}(i,1)}}{\sum_{i}{\frac{p^{B}(i,0)-p^{B}(i,1)}{g_{h}(i)}}} \\
         \sigma & = & \frac{p_{surf}}{\bar{g}}
      \end{eqnarray}
