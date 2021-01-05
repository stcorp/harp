partial pressure derivations
============================

   .. _derivation_partial_pressure_from_number_density_and_temperature:

#. partial pressure from number density and temperature

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

      p_{x} = n_{x}kT


   .. _derivation_partial_pressure_from_volume_mixing_ratio:

#. partial pressure from volume mixing ratio

   =============== ====================================== =========== ===================================
   symbol          description                            unit        variable name
   =============== ====================================== =========== ===================================
   :math:`p`       pressure                               :math:`Pa`  `pressure {:}`
   :math:`p_{x}`   partial pressure of air component x    :math:`Pa`  `<species>_partial_pressure {:}`
                   (e.g. :math:`p_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio {:}`
                   (e.g. :math:`\nu_{O_{3}}`)
   =============== ====================================== =========== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{x} = \nu_{x}p


   .. _derivation_partial_pressure_from_volume_mixing_ratio_dry_air:

#. partial pressure from volume mixing ratio dry air

   ===================== ====================================== =========== ===========================================
   symbol                description                            unit        variable name
   ===================== ====================================== =========== ===========================================
   :math:`p_{x}`         partial pressure of air component x    :math:`Pa`  `<species>_partial_pressure {:}`
                         (e.g. :math:`p_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio_dry_air {:}`
                         (e.g. :math:`\nu_{O_{3}}`)
   ===================== ====================================== =========== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{x} = \bar{\nu}_{x}p_{dry\_air}
