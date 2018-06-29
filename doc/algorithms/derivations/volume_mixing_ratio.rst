volume mixing ratio derivations
===============================

#. volume mixing ratio from number density

   =============== ======================================= ========================= ===================================
   symbol          description                             unit                      variable name
   =============== ======================================= ========================= ===================================
   :math:`n`       number density of total air             :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{x}`   number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                   (e.g. :math:`n_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppv`               `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================= ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = \frac{n_{x}}{n}


#. volume mixing ratio from mass mixing ratio

   =============== ================================= ===================== ===================================
   symbol          description                       unit                  variable name
   =============== ================================= ===================== ===================================
   :math:`M_{air}` molar mass for total air          :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`q_{x}`   mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` volume mixing ratio of quantity x :math:`ppv`           `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ================================= ===================== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = q_{x}\frac{M_{air}}{M_{x}}


#. volume mixing ratio from partial pressure

   =============== ======================================= =========== ===================================
   symbol          description                             unit        variable name
   =============== ======================================= =========== ===================================
   :math:`p`       pressure                                :math:`Pa`  `pressure {:}`
   :math:`p_{x}`   partial pressure for air component x    :math:`Pa`  `<species>_partial_pressure {:}`
                   (e.g. :math:`p_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppv` `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================= =========== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = \frac{p_{x}}{p}


#. volume mixing ratio dry air from number density

   ===================== ======================================= ========================= ===========================================
   symbol                description                             unit                      variable name
   ===================== ======================================= ========================= ===========================================
   :math:`n_{dry\_air}`  number density of dry air               :math:`\frac{molec}{m^3}` `dry_air_number_density {:}`
   :math:`n_{x}`         number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                         (e.g. :math:`n_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppv`               `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ======================================= ========================= ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \frac{n_{x}}{n_{dry\_air}}


#. volume mixing ratio dry air from mass mixing ratio dry air

   ===================== ================================= ===================== ===========================================
   symbol                description                       unit                  variable name
   ===================== ================================= ===================== ===========================================
   :math:`M_{dry\_air}`  molar mass for dry air            :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity x :math:`ppv`           `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ================================= ===================== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


#. volume mixing ratio dry air from partial pressure

   ===================== ======================================= =========== ===========================================
   symbol                description                             unit        variable name
   ===================== ======================================= =========== ===========================================
   :math:`p_{dry\_air}`  partial pressure of dry air             :math:`Pa`  `dry_air_partial_pressure {:}`
   :math:`p_{x}`         partial pressure for air component x    :math:`Pa`  `<species>_partial_pressure {:}`
                         (e.g. :math:`p_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppv` `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ======================================= =========== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \frac{p_{x}}{p_{dry\_air}}
