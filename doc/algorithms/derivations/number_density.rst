number density derivations
==========================

#. number density for air component from mass density:

   ================ ================================== ========================= ==============================
   symbol           description                        unit                      variable name
   ================ ================================== ========================= ==============================
   :math:`M_{x}`    molar mass for air component x     :math:`\frac{g}{mol}`
   :math:`n_{x}`    number density for air component x :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`N_A`      Avogadro constant                  :math:`\frac{1}{mol}`
   :math:`\rho_{x}` mass density for air component x   :math:`\frac{kg}{m^3}`    `<species>_density {:}`
                    (e.g. :math:`\rho_{O_{3}}`)
   ================ ================================== ========================= ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \frac{\rho_{x}N_{A}}{10^{-3}M_{x}}


#. number density for total air from mass density:

   =============== ============================ ========================= ====================
   symbol          description                  unit                      variable name
   =============== ============================ ========================= ====================
   :math:`M_{air}` molar mass for total air     :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`       number density for total air :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`     Avogadro constant            :math:`\frac{1}{mol}`
   :math:`\rho`    mass density for total air   :math:`\frac{kg}{m^3}`    `density {:}`
   =============== ============================ ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n = \frac{\rho N_{A}}{10^{-3}M_{air}}


#. number density for total air from pressure and temperature

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


#. number density from volume mixing ratio

   =============== ======================================= ========================= ==================================
   symbol          description                             unit                      variable name
   =============== ======================================= ========================= ==================================
   :math:`n_{air}` number density of total air             :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{x}`   number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                   (e.g. :math:`n_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppv`               `<species>_volum_mixing_ratio {:}`
                   (e.g. :math:`n_{O_{3}}`)
   =============== ======================================= ========================= ==================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \nu_{x}n_{air}


#. number density from volume mixing ratio dry air

   ===================== ======================================= ========================= ==========================================
   symbol                description                             unit                      variable name
   ===================== ======================================= ========================= ==========================================
   :math:`n_{dry\_air}`  number density of dry air               :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{x}`         number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                         (e.g. :math:`n_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppv`               `<species>_volum_mixing_ratio_dry_air {:}`
                         (e.g. :math:`n_{O_{3}}`)
   ===================== ======================================= ========================= ==========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \bar{\nu}_{x}n_{dry\_air}


#. number density for air component from column number density

   ================ =========================================== ========================= =====================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= =====================================
   :math:`c_{x}`    column number density for air component x   :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    (e.g. :math:`c_{O_{3}}`)
   :math:`n_{x}`    number density for air component x          :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n_{x} = \frac{c_{x}}{\lvert z^{B}(2) - z^{B}(1) \rvert}


#. number density for total air column number density

   ================ =========================================== ========================= ===========================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===========================
   :math:`c`        column number density for air component x   :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`n`        number density for air component x          :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n = \frac{c}{\lvert z^{B}(2) - z^{B}(1) \rvert}


#. number density for air component from partial pressure and temperature

   ============= ==================================== ============================ ================================
   symbol        description                          unit                         variable name
   ============= ==================================== ============================ ================================
   :math:`k`     Boltzmann constant                   :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{x}` number density for air component x   :math:`\frac{molec}{m^3}`    `<species>_number_density {:}`
                 (e.g. :math:`n_{O_{3}}`)
   :math:`p_{x}` partial pressure for air component x :math:`Pa`                   `<species>_partial_pressure {:}`
                 (e.g. :math:`p_{O_{3}}`)
   :math:`T`     temperature                          :math:`K`                    `temperature {:}`
   ============= ==================================== ============================ ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \frac{p_{x}}{kT}


#. surface number density for total air from surface pressure and surface temperature

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
