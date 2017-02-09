column volume mixing ratio derivations
======================================

#. column volume mixing ratio from column mass mixing ratio

   =============== ======================================== ====================== ==========================================
   symbol          description                              unit                   variable name
   =============== ======================================== ====================== ==========================================
   :math:`M_{air}` molar mass for total air                 :math:`\frac{g}{mol}`  `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`q_{x}`   column mass mixing ratio of quantity x   :math:`\frac{kg}{kg}`  `<species>_column_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` column volume mixing ratio of quantity x :math:`ppv`            `<species>_column_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================== ====================== ==========================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \nu_{x} = q_{x}\frac{M_{air}}{M_{x}}


#. column volume mixing ratio dry air from column mass mixing ratio dry air

   ===================== ======================================== ===================== ==================================================
   symbol                description                              unit                  variable name
   ===================== ======================================== ===================== ==================================================
   :math:`M_{dry\_air}`  molar mass for dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   column mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_column_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` column volume mixing ratio of quantity x :math:`ppv`           `<species>_column_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ======================================== ===================== ==================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


#. stratospheric column volume mixing ratio dry air from stratospheric column mass mixing ratio dry air

   ===================== ======================================== ===================== ================================================================
   symbol                description                              unit                  variable name
   ===================== ======================================== ===================== ================================================================
   :math:`M_{dry\_air}`  molar mass for dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   stratospheric column mass mixing ratio   :math:`\frac{kg}{kg}` `stratospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` stratospheric column volume mixing ratio :math:`ppv`           `stratospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================== ===================== ================================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


#. tropospheric column volume mixing ratio dry air from tropospheric column mass mixing ratio dry air

   ===================== ======================================= ===================== ===============================================================
   symbol                description                             unit                  variable name
   ===================== ======================================= ===================== ===============================================================
   :math:`M_{dry\_air}`  molar mass for dry air                  :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x          :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   tropospheric column mass mixing ratio   :math:`\frac{kg}{kg}` `tropospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` tropospheric column volume mixing ratio :math:`ppv`           `tropospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================= ===================== ===============================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}
