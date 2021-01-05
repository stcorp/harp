column number density derivations
=================================

   .. _derivation_total_column_number_density_of_air_component_from_partial_column_number_density_profile:

#. total column number density of air component from partial column number density profile

   ================ ======================================= ========================= ==============================================
   symbol           description                             unit                      variable name
   ================ ======================================= ========================= ==============================================
   :math:`c_{x}`    total column number density of air      :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile of air    :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   ================ ======================================= ========================= ==============================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{c_{x}(i)}


   .. _derivation_total_column_number_density_of_total_air_from_partial_column_number_density_profile:

#. total column number density of total air from partial column number density profile

   ============ ========================================== ========================= ====================================
   symbol       description                                unit                      variable name
   ============ ========================================== ========================= ====================================
   :math:`c`    total column number density of total air   :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c(i)` column number density profile of total air :math:`\frac{molec}{m^2}` `column_number_density {:,vertical}`
   ============ ========================================== ========================= ====================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{c_{x}(i)}


   .. _derivation_tropospheric_column_number_density_of_air_component_from_partial_column_number_density_profile_and_altitude:

#. tropospheric column number density of air component from partial column number density profile and altitude

   ================ =========================================== ========================= ==================================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ==================================================
   :math:`c_{x}`    tropospheric column number density of air   :math:`\frac{molec}{m^2}` `tropospheric_<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile of air        :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   :math:`z_{TP}`   tropopause altitude                         :math:`m`                 `tropopause_altitude {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ==================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{\begin{cases}
        z^{B}(2) \leq z_{TP}, & c_{x}(i) \\
        z^{B}(1) < z_{TP} < z^{B}(2), & c_{x}(i) \frac{z_{TP} - z^{B}(1)}{z^{B}(2) - z^{B}(1)} \\
        z_{TP} \leq z^{B}(1), & 0
      \end{cases}}


   .. _derivation_stratospheric_column_number_density_of_air_component_from_partial_column_number_density_profile_and_altitude:

#. stratospheric column number density of air component from partial column number density profile and altitude

   ================ =========================================== ========================= ===================================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===================================================
   :math:`c_{x}`    stratospheric column number density of air  :math:`\frac{molec}{m^2}` `stratospheric_<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile of air        :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   :math:`z_{TP}`   tropopause altitude                         :math:`m`                 `tropopause_altitude {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{\begin{cases}
        z^{B}(2) \leq z_{TP}, & 0 \\
        z^{B}(1) < z_{TP} < z^{B}(2), & c_{x}(i) \frac{z^{B}(2) - z_{TP}}{z^{B}(2) - z^{B}(1)} \\
        z_{TP} \leq z^{B}(1), & c_{x}(i)
      \end{cases}}


   .. _derivation_tropospheric_column_number_density_of_air_component_from_partial_column_number_density_profile_and_pressure:

#. tropospheric column number density of air component from partial column number density profile and pressure

   ================ =========================================== ========================= ==================================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ==================================================
   :math:`c_{x}`    tropospheric column number density of air   :math:`\frac{molec}{m^2}` `tropospheric_<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile of air        :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   :math:`p_{TP}`   tropopause pressure                         :math:`Pa`                `tropopause_pressure {:}`
   :math:`p^{B}(l)` pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa`                `pressure_bounds {:,2}`
   ================ =========================================== ========================= ==================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{\begin{cases}
        p^{B}(2) \geq p_{TP}, & c_{x}(i) \\
        p^{B}(1) > p_{TP} > p^{B}(2), & c_{x}(i) \frac{\ln(p^{B}(1)) - \ln(p_{TP})}{\ln(p^{B}(1)) - \ln(p^{B}(2))} \\
        p_{TP} \geq p^{B}(1), & 0
      \end{cases}}


   .. _derivation_stratospheric_column_number_density_of_air_component_from_partial_column_number_density_profile_and_pressure:

#. stratospheric column number density of air component from partial column number density profile and pressure

   ================ =========================================== ========================= ===================================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===================================================
   :math:`c_{x}`    stratospheric column number density of air  :math:`\frac{molec}{m^2}` `stratospheric_<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile of air        :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   :math:`p_{TP}`   tropopause pressure                         :math:`Pa`                `tropopause_pressure {:}`
   :math:`p^{B}(l)` pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa`                `pressure_bounds {:,2}`
   ================ =========================================== ========================= ===================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{\begin{cases}
        p^{B}(2) \geq p_{TP}, & 0 \\
        p^{B}(1) > p_{TP} > p^{B}(2), & c_{x}(i) \frac{\ln(p_{TP}) - \ln(p^{B}(2))}{\ln(p^{B}(1)) - \ln(p^{B}(2))} \\
        p_{TP} \geq p^{B}(1), & c_{x}(i)
      \end{cases}}


   .. _derivation_column_number_density_of_total_air_from_dry_air_column_number_density_and_H2O_column_number_density:

#. column number density of total air from dry air column number density and H2O column number density

   ==================== ================================ ========================= ===================================
   symbol               description                      unit                      variable name
   ==================== ================================ ========================= ===================================
   :math:`c`            column number density            :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{dry\_air}` column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{H_{2}O}`   column number density of H2O     :math:`\frac{molec}{m^2}` `H2O_column_number_density {:}`
   ==================== ================================ ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     c = c_{dry\_air} + c_{H_{2}O}


   .. _derivation_column_number_density_of_dry_air_from_total_air_column_number_density_and_H2O_column_number_density:

#. column number density of dry air from total air column number density and H2O column number density

   ==================== ================================ ========================= ===================================
   symbol               description                      unit                      variable name
   ==================== ================================ ========================= ===================================
   :math:`c`            column number density            :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{dry\_air}` column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{H_{2}O}`   column number density of H2O     :math:`\frac{molec}{m^2}` `H2O_column_number_density {:}`
   ==================== ================================ ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     c_{dry\_air} = c - c_{H_{2}O}


   .. _derivation_column_number_density_of_H2O_from_total_air_column_number_density_and_dry_air_column_number_density:

#. column number density of H2O from total air column number density and dry air column number density

   ==================== ================================ ========================= ===================================
   symbol               description                      unit                      variable name
   ==================== ================================ ========================= ===================================
   :math:`c`            column number density            :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{dry\_air}` column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{H_{2}O}`   column number density of H2O     :math:`\frac{molec}{m^2}` `H2O_column_number_density {:}`
   ==================== ================================ ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     c_{H_{2}O} = c - c_{dry\_air}


   .. _derivation_column_number_density_of_air_component_from_number_density:

#. column number density of air component from number density

   ================ =========================================== ========================= =====================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= =====================================
   :math:`c_{x}`    column number density of air component x    :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    (e.g. :math:`c_{O_{3}}`)
   :math:`n_{x}`    number density of air component x           :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c_{x} = n_{x} \lvert z^{B}(2) - z^{B}(1) \rvert


   .. _derivation_column_number_density_of_total_air_from_number_density:

#. column number density of total air from number density

   ================ =========================================== ========================= ===========================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===========================
   :math:`c`        column number density of total air          :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`n`        number density of total air                 :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c = n \lvert z^{B}(2) - z^{B}(1) \rvert


   .. _derivation_column_number_density_of_air_component_from_column_mass_density:

#. column number density of air component from column mass density

   This conversion applies to both total columns as well as partial column profiles.

   ================== ======================================== ========================= =====================================
   symbol             description                              unit                      variable name
   ================== ======================================== ========================= =====================================
   :math:`c_{x}`      column number density of air component x :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                      (e.g. :math:`n_{O_{3}}`)
   :math:`M_{x}`      molar mass of air component x            :math:`\frac{g}{mol}`
   :math:`N_A`        Avogadro constant                        :math:`\frac{1}{mol}`
   :math:`\sigma_{x}` column mass density of air component x   :math:`\frac{kg}{m^2}`    `<species>_column_density {:}`
                      (e.g. :math:`\sigma_{O_{3}}`)
   ================== ======================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c_{x} = \frac{\sigma_{x}N_{A}}{10^{-3}M_{x}}


   .. _derivation_column_number_density_of_total_air_from_column_mass_density:

#. column number density of total air from column mass density

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

      c = \frac{\sigma N_{A}}{10^{-3}M_{air}}


   .. _derivation_column_number_density_from_column_volume_mixing_ratio:

#. column number density from column volume mixing ratio

   =============== ======================================== ========================= ==========================================
   symbol          description                              unit                      variable name
   =============== ======================================== ========================= ==========================================
   :math:`c`       total column number density of total air :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{x}`   total column number density of air       :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                   component x (e.g. :math:`c_{O_{3}}`)
   :math:`\nu_{x}` column volume mixing ratio of            :math:`ppv`               `<species>_column_volume_mixing_ratio {:}`
                   quantity x with regard to total air
   =============== ======================================== ========================= ==========================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \nu_{x}c


   .. _derivation_column_number_density_from_column_volume_mixing_ratio_dry_air:

#. column number density from column volume mixing ratio dry air

   ===================== ====================================== ========================= ==================================================
   symbol                description                            unit                      variable name
   ===================== ====================================== ========================= ==================================================
   :math:`c_{dry\_air}`  total column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{x}`         total column number density of air     :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                         component x (e.g. :math:`c_{O_{3}}`)
   :math:`\bar{\nu}_{x}` column volume mixing ratio of          :math:`ppv`               `<species>_column_volume_mixing_ratio_dry_air {:}`
                         quantity x with regard to dry air
   ===================== ====================================== ========================= ==================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \bar{\nu}_{x}c_{dry\_air}


   .. _derivation_column_number_density_of_air_component_from_volume_mixing_ratio:

#. column number density of air component from volume mixing ratio

   ================== =========================================== ================================ =====================================
   symbol             description                                 unit                             variable name
   ================== =========================================== ================================ =====================================
   :math:`a`          WGS84 semi-major axis                       :math:`m`
   :math:`b`          WGS84 semi-minor axis                       :math:`m`
   :math:`c_{x}`      column number density of air component x    :math:`\frac{molec}{m^2}`        `<species>_column_number_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   :math:`f`          WGS84 flattening                            :math:`m`
   :math:`g`          normal gravity at sea level                 :math:`\frac{m}{s^2}`
   :math:`g_{0}`      mean earth gravity                          :math:`\frac{m}{s^2}`
   :math:`g_{h}`      gravity at specific height                  :math:`\frac{m}{s^2}`
   :math:`GM`         WGS84 earth's gravitational constant        :math:`\frac{m^3}{s^2}`
   :math:`M_{air}`    molar mass of total air                     :math:`\frac{g}{mol}`            `molar_mass {:}`
   :math:`N_A`        Avogadro constant                           :math:`\frac{1}{mol}`
   :math:`p`          pressure                                    :math:`Pa`
   :math:`p_{0}`      standard pressure                           :math:`Pa`
   :math:`p^{B}(l)`   pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa`                       `pressure_bounds {:,2}`
   :math:`R`          universal gas constant                      :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T_{0}`      standard temperature                        :math:`K`
   :math:`z`          altitude                                    :math:`m`
   :math:`\nu_{x}`    volume mixing ratio of quantity x           :math:`ppv`                      `<species>_volume_mixing_ratio {:}`
                      with regard to total air
   :math:`\phi`       latitude                                    :math:`degN`                     `latitude {:}`
   :math:`\omega`     WGS84 earth angular velocity                :math:`rad/s`
   ================== =========================================== ================================ =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)}} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         p & = & e^{\frac{\ln(p^{B}(2)) + \ln(p^{B}(1))}{2}} \\
         z & = & -\frac{RT_{0}}{10^{-3}M_{air}g_{0}}\ln(\frac{p}{p_{0}}) \\
         g_{h} & = & g \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2\right) \\
         c_{x} & = & -\nu_{x}\frac{N_A}{10^{-3}M_{air}g_{h}}(p^{B}(2)-p^{B}(1))
      \end{eqnarray}
