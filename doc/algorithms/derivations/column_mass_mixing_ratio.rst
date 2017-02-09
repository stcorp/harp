column mass mixing ratio derivations
====================================

#. column mass mixing ratio from column volume mixing ratio

   =============== ======================================== ===================== ==========================================
   symbol          description                              unit                  variable name
   =============== ======================================== ===================== ==========================================
   :math:`M_{air}` molar mass for total air                 :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`q_{x}`   column mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_column_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` column volume mixing ratio of quantity x :math:`ppv`           `<species>_column_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================== ===================== ==========================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      q_{x} = \nu_{x}\frac{M_{x}}{M_{air}}


#. column mass mixing ratio dry air from column volume mixing ratio dry air

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

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}


#. stratospheric column mass mixing ratio dry air from stratospheric column volume mixing ratio dry air

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

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}


#. tropospheric column mass mixing ratio dry air from tropospheric column volume mixing ratio dry air

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

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}

