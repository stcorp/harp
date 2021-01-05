molar mass derivations
======================

   .. _derivation_molar_mass_of_total_air_from_density_and_number_density:

#. molar mass of total air from density and number density

   =============== ======================= ========================= ====================
   symbol          description             unit                      variable name
   =============== ======================= ========================= ====================
   :math:`M_{air}` molar mass of total air :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`       number density          :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`     Avogadro constant       :math:`\frac{1}{mol}`
   :math:`\rho`    mass density            :math:`\frac{kg}{m^3}`    `density {:}`
   =============== ======================= ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      M_{air} = 10^{3}\frac{\rho N_{A}}{n}


   .. _derivation_molar_mass_of_total_air_from_H2O_mass_mixing_ratio:

#. molar mass of total air from H2O mass mixing ratio

   ==================== ======================== ===================== ===========================
   symbol               description              unit                  variable name
   ==================== ======================== ===================== ===========================
   :math:`M_{air}`      molar mass of total air  :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{dry\_air}` molar mass of dry air    :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O        :math:`\frac{g}{mol}`
   :math:`q_{H_{2}O}`   mass mixing ratio of H2O :math:`\frac{kg}{kg}` `H2O_mass_mixing_ratio {:}`
   ==================== ======================== ===================== ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      M_{air} = \frac{M_{H_{2}O}M_{dry\_air}}{\left(1-q_{H_{2}O}\right)M_{H_{2}O} + q_{H_{2}O}M_{dry\_air}}


   .. _derivation_molar_mass_of_total_air_from_H2O_volume_mixing_ratio:

#. molar mass of total air from H2O volume mixing ratio

   ==================== ======================== ===================== =============================
   symbol               description              unit                  variable name
   ==================== ======================== ===================== =============================
   :math:`M_{air}`      molar mass of total air  :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{dry\_air}` molar mass of dry air    :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O        :math:`\frac{g}{mol}`
   :math:`\nu_{H_{2}O}` mass mixing ratio of H2O :math:`ppv`           `H2O_volume_mixing_ratio {:}`
   ==================== ======================== ===================== =============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      M_{air} = M_{dry\_air}\left(1 - \nu_{H_{2}O}\right) + M_{H_{2}O}\nu_{H_{2}O}
