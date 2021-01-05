column volume mixing ratio derivations
======================================

   .. _derivation_column_volume_mixing_ratio_from_column_number_density:

#. column volume mixing ratio from column number density

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

      \nu_{x} = \frac{c_{x}}{c}


   .. _derivation_column_volume_mixing_ratio_dry_air_from_column_number_density:

#. column volume mixing ratio dry air from column number density

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

      \bar{\nu}_{x} = \frac{c_{x}}{c_{dry\_air}}


   .. _derivation_column_volume_mixing_ratio_from_column_mass_mixing_ratio:

#. column volume mixing ratio from column mass mixing ratio

   =============== ======================================== ====================== ==========================================
   symbol          description                              unit                   variable name
   =============== ======================================== ====================== ==========================================
   :math:`M_{air}` molar mass of total air                  :math:`\frac{g}{mol}`  `molar_mass {:}`
   :math:`M_{x}`   molar mass of air component x            :math:`\frac{g}{mol}`
   :math:`q_{x}`   column mass mixing ratio of quantity x   :math:`\frac{kg}{kg}`  `<species>_column_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` column volume mixing ratio of quantity x :math:`ppv`            `<species>_column_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================== ====================== ==========================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \nu_{x} = q_{x}\frac{M_{air}}{M_{x}}


   .. _derivation_column_volume_mixing_ratio_dry_air_from_column_mass_mixing_ratio_dry_air:

#. column volume mixing ratio dry air from column mass mixing ratio dry air

   ===================== ======================================== ===================== ==================================================
   symbol                description                              unit                  variable name
   ===================== ======================================== ===================== ==================================================
   :math:`M_{dry\_air}`  molar mass of dry air                    :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass of air component x            :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   column mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_column_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` column volume mixing ratio of quantity x :math:`ppv`           `<species>_column_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ======================================== ===================== ==================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


   .. _derivation_stratospheric_column_volume_mixing_ratio_dry_air_from_stratospheric_column_mass_mixing_ratio_dry_air:

#. stratospheric column volume mixing ratio dry air from stratospheric column mass mixing ratio dry air

   ===================== ======================================== ===================== ================================================================
   symbol                description                              unit                  variable name
   ===================== ======================================== ===================== ================================================================
   :math:`M_{dry\_air}`  molar mass of dry air                    :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass of air component x            :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   stratospheric column mass mixing ratio   :math:`\frac{kg}{kg}` `stratospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` stratospheric column volume mixing ratio :math:`ppv`           `stratospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================== ===================== ================================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


   .. _derivation_tropospheric_column_volume_mixing_ratio_dry_air_from_tropospheric_column_mass_mixing_ratio_dry_air:

#. tropospheric column volume mixing ratio dry air from tropospheric column mass mixing ratio dry air

   ===================== ======================================= ===================== ===============================================================
   symbol                description                             unit                  variable name
   ===================== ======================================= ===================== ===============================================================
   :math:`M_{dry\_air}`  molar mass of dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass of air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   tropospheric column mass mixing ratio   :math:`\frac{kg}{kg}` `tropospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` tropospheric column volume mixing ratio :math:`ppv`           `tropospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================= ===================== ===============================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}
