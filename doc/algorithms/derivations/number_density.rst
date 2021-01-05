number density derivations
==========================

   .. _derivation_number_density_of_air_component_from_mass_density:

#. number density of air component from mass density

   ================ ================================= ========================= ==============================
   symbol           description                       unit                      variable name
   ================ ================================= ========================= ==============================
   :math:`M_{x}`    molar mass of air component x     :math:`\frac{g}{mol}`
   :math:`n_{x}`    number density of air component x :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`N_A`      Avogadro constant                 :math:`\frac{1}{mol}`
   :math:`\rho_{x}` mass density of air component x   :math:`\frac{kg}{m^3}`    `<species>_density {:}`
                    (e.g. :math:`\rho_{O_{3}}`)
   ================ ================================= ========================= ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \frac{\rho_{x}N_{A}}{10^{-3}M_{x}}


   .. _derivation_number_density_of_total_air_from_mass_density:

#. number density of total air from mass density

   =============== =========================== ========================= ====================
   symbol          description                 unit                      variable name
   =============== =========================== ========================= ====================
   :math:`M_{air}` molar mass of total air     :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`       number density of total air :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`     Avogadro constant           :math:`\frac{1}{mol}`
   :math:`\rho`    mass density of total air   :math:`\frac{kg}{m^3}`    `density {:}`
   =============== =========================== ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n = \frac{\rho N_{A}}{10^{-3}M_{air}}


   .. _derivation_number_density_of_total_air_from_pressure_and_temperature:

#. number density of total air from pressure and temperature

   ========= ================== ============================ ====================
   symbol    description        unit                         variable name
   ========= ================== ============================ ====================
   :math:`k` Boltzmann constant :math:`\frac{kg m^2}{K s^2}`
   :math:`n` number density     :math:`\frac{molec}{m^3}`    `number_density {:}`
   :math:`p` pressure           :math:`Pa`                   `pressure {:}`
   :math:`T` temperature        :math:`K`                    `temperature {:}`
   ========= ================== ============================ ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n = \frac{p}{kT}


   .. _derivation_number_density_from_volume_mixing_ratio:

#. number density from volume mixing ratio

   =============== ====================================== ========================= ===================================
   symbol          description                            unit                      variable name
   =============== ====================================== ========================= ===================================
   :math:`n`       number density of total air            :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{x}`   number density of air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                   (e.g. :math:`n_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio of air component x :math:`ppv`               `<species>_volume_mixing_ratio {:}`
                   (e.g. :math:`n_{O_{3}}`)
   =============== ====================================== ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \nu_{x}n


   .. _derivation_number_density_from_volume_mixing_ratio_dry_air:

#. number density from volume mixing ratio dry air

   ===================== ====================================== ========================= ===========================================
   symbol                description                            unit                      variable name
   ===================== ====================================== ========================= ===========================================
   :math:`n_{dry\_air}`  number density of dry air              :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{x}`         number density of air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                         (e.g. :math:`n_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio of air component x :math:`ppv`               `<species>_volume_mixing_ratio_dry_air {:}`
                         (e.g. :math:`n_{O_{3}}`)
   ===================== ====================================== ========================= ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \bar{\nu}_{x}n_{dry\_air}


   .. _derivation_number_density_of_air_component_from_column_number_density:

#. number density of air component from column number density

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

     n_{x} = \frac{c_{x}}{\lvert z^{B}(2) - z^{B}(1) \rvert}


   .. _derivation_number_density_of_total_air_from_dry_air_number_density_and_H2O_number_density:

#. number density of total air from dry air number density and H2O number density

   ==================== ========================= ========================= ============================
   symbol               description               unit                      variable name
   ==================== ========================= ========================= ============================
   :math:`n`            number density            :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{dry\_air}` number density of dry air :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{H_{2}O}`   number density of H2O     :math:`\frac{molec}{m^3}` `H2O_number_density {:}`
   ==================== ========================= ========================= ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n = n_{dry\_air} + n_{H_{2}O}


   .. _derivation_number_density_of_dry_air_from_total_air_number_density_and_H2O_number_density:

#. number density of dry air from total air number density and H2O number density

   ==================== ========================= ========================= ============================
   symbol               description               unit                      variable name
   ==================== ========================= ========================= ============================
   :math:`n`            number density            :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{dry\_air}` number density of dry air :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{H_{2}O}`   number density of H2O     :math:`\frac{molec}{m^3}` `H2O_number_density {:}`
   ==================== ========================= ========================= ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n_{dry\_air} = n - n_{H_{2}O}


   .. _derivation_number_density_of_H2O_from_total_air_number_density_and_dry_air_number_density:

#. number density of H2O from total air number density and dry air number density

   ==================== ========================= ========================= ============================
   symbol               description               unit                      variable name
   ==================== ========================= ========================= ============================
   :math:`n`            number density            :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{dry\_air}` number density of dry air :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{H_{2}O}`   number density of H2O     :math:`\frac{molec}{m^3}` `H2O_number_density {:}`
   ==================== ========================= ========================= ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n_{H_{2}O} = n - n_{dry\_air}


   .. _derivation_number_density_of_total_air_from_column_number_density:

#. number density of total air from column number density

   ================ =========================================== ========================= ===========================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===========================
   :math:`c`        column number density                       :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`n`        number density                              :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n = \frac{c}{\lvert z^{B}(2) - z^{B}(1) \rvert}


   .. _derivation_number_density_of_air_component_from_partial_pressure_and_temperature:

#. number density of air component from partial pressure and temperature

   ============= =================================== ============================ ================================
   symbol        description                         unit                         variable name
   ============= =================================== ============================ ================================
   :math:`k`     Boltzmann constant                  :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{x}` number density of air component x   :math:`\frac{molec}{m^3}`    `<species>_number_density {:}`
                 (e.g. :math:`n_{O_{3}}`)
   :math:`p_{x}` partial pressure of air component x :math:`Pa`                   `<species>_partial_pressure {:}`
                 (e.g. :math:`p_{O_{3}}`)
   :math:`T`     temperature                         :math:`K`                    `temperature {:}`
   ============= =================================== ============================ ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \frac{p_{x}}{kT}


   .. _derivation_surface_number_density_of_total_air_from_surface_pressure_and_surface_temperature:

#. surface number density of total air from surface pressure and surface temperature

   ================ ====================== ============================ ============================
   symbol           description            unit                         variable name
   ================ ====================== ============================ ============================
   :math:`k`        Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{surf}` surface number density :math:`\frac{molec}{m^3}`    `surface_number_density {:}`
   :math:`p_{surf}` surface pressure       :math:`Pa`                   `surface_pressure {:}`
   :math:`T_{surf}` surface temperature    :math:`K`                    `surface_temperature {:}`
   ================ ====================== ============================ ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{surf} = \frac{p_{surf}}{kT_{surf}}
