volume mixing ratio derivations
===============================

   .. _derivation_volume_mixing_ratio_from_number_density:

#. volume mixing ratio from number density

   =============== ====================================== ========================= ===================================
   symbol          description                            unit                      variable name
   =============== ====================================== ========================= ===================================
   :math:`n`       number density of total air            :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{x}`   number density of air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                   (e.g. :math:`n_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio if air component x :math:`ppv`               `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ====================================== ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = \frac{n_{x}}{n}


   .. _derivation_volume_mixing_ratio_from_mass_mixing_ratio:

#. volume mixing ratio from mass mixing ratio

   =============== ================================= ===================== ===================================
   symbol          description                       unit                  variable name
   =============== ================================= ===================== ===================================
   :math:`M_{air}` molar mass of total air           :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{x}`   molar mass of air component x     :math:`\frac{g}{mol}`
   :math:`q_{x}`   mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` volume mixing ratio of quantity x :math:`ppv`           `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ================================= ===================== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = q_{x}\frac{M_{air}}{M_{x}}


   .. _derivation_volume_mixing_ratio_from_partial_pressure:

#. volume mixing ratio from partial pressure

   =============== ====================================== =========== ===================================
   symbol          description                            unit        variable name
   =============== ====================================== =========== ===================================
   :math:`p`       pressure                               :math:`Pa`  `pressure {:}`
   :math:`p_{x}`   partial pressure of air component x    :math:`Pa`  `<species>_partial_pressure {:}`
                   (e.g. :math:`p_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ====================================== =========== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = \frac{p_{x}}{p}


   .. _derivation_volume_mixing_ratio_from_volume_mixing_ratio_dry_air:

#. volume mixing ratio from volume mixing ratio dry air

   ====================== ====================================== =========== ===========================================
   symbol                 description                            unit        variable name
   ====================== ====================================== =========== ===========================================
   :math:`\nu_{x}`        volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio {:}`
                          with regard to total air
   :math:`\nu_{dry\_air}` volume mixing ratio of dry air with    :math:`ppv` `dry_air_volume_mixing_ratio {:}`
                          regard to total air
   :math:`\bar{\nu}_{x}`  volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio_dry_air {:}`
                          with regard to dry air
   ====================== ====================================== =========== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = \bar{\nu}_{x}\nu_{dry\_air}


   .. _derivation_volume_mixing_ratio_dry_air_from_number_density:

#. volume mixing ratio dry air from number density

   ===================== ====================================== ========================= ===========================================
   symbol                description                            unit                      variable name
   ===================== ====================================== ========================= ===========================================
   :math:`n_{dry\_air}`  number density of dry air              :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{x}`         number density of air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                         (e.g. :math:`n_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio of air component x :math:`ppv`               `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ====================================== ========================= ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \frac{n_{x}}{n_{dry\_air}}


   .. _derivation_volume_mixing_ratio_dry_air_from_mass_mixing_ratio_dry_air:

#. volume mixing ratio dry air from mass mixing ratio dry air

   ===================== ================================= ===================== ===========================================
   symbol                description                       unit                  variable name
   ===================== ================================= ===================== ===========================================
   :math:`M_{dry\_air}`  molar mass of dry air             :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass of air component x     :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity x :math:`ppv`           `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ================================= ===================== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


   .. _derivation_volume_mixing_ratio_dry_air_from_partial_pressure:

#. volume mixing ratio dry air from partial pressure

   ===================== ====================================== =========== ===========================================
   symbol                description                            unit        variable name
   ===================== ====================================== =========== ===========================================
   :math:`p_{dry\_air}`  partial pressure of dry air            :math:`Pa`  `dry_air_partial_pressure {:}`
   :math:`p_{x}`         partial pressure of air component x    :math:`Pa`  `<species>_partial_pressure {:}`
                         (e.g. :math:`p_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ====================================== =========== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \frac{p_{x}}{p_{dry\_air}}


   .. _derivation_volume_mixing_ratio_dry_air_from_volume_mixing_ratio:

#. volume mixing ratio dry air from volume mixing ratio

   ====================== ====================================== =========== ===========================================
   symbol                 description                            unit        variable name
   ====================== ====================================== =========== ===========================================
   :math:`\nu_{x}`        volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio {:}`
                          with regard to total air
   :math:`\nu_{dry\_air}` volume mixing ratio of dry air with    :math:`ppv` `dry_air_volume_mixing_ratio {:}`
                          regard to total air
   :math:`\bar{\nu}_{x}`  volume mixing ratio of air component x :math:`ppv` `<species>_volume_mixing_ratio_dry_air {:}`
                          with regard to dry air
   ====================== ====================================== =========== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \frac{\nu_{x}}{\nu_{dry\_air}}


   .. _derivation_dry_air_volume_mixing_ratio_from_H2O_volume_mixing_ratio:

#. dry air volume mixing ratio from H2O volume mixing ratio

   ====================== ============================== =========== =================================
   symbol                 description                    unit        variable name
   ====================== ============================== =========== =================================
   :math:`\nu_{H_{2}O}`   volume mixing ratio of H2O     :math:`ppv` `H2O_volume_mixing_ratio {:}`
                          with regard to total air
   :math:`\nu_{dry\_air}` volume mixing ratio of dry air :math:`ppv` `dry_air_volume_mixing_ratio {:}`
                          with regard to total air
   ====================== ============================== =========== =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{dry\_air} = 1 - \nu_{H_{2}O}


   .. _derivation_H2O_volume_mixing_ratio_from_dry_air_volume_mixing_ratio:

#. H2O volume mixing ratio from dry air volume mixing ratio

   ====================== ============================== =========== =================================
   symbol                 description                    unit        variable name
   ====================== ============================== =========== =================================
   :math:`\nu_{H_{2}O}`   volume mixing ratio of H2O     :math:`ppv` `H2O_volume_mixing_ratio {:}`
                          with regard to total air
   :math:`\nu_{dry\_air}` volume mixing ratio of dry air :math:`ppv` `dry_air_volume_mixing_ratio {:}`
                          with regard to total air
   ====================== ============================== =========== =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{H_{2}O} = 1 - \nu_{dry\_air}
