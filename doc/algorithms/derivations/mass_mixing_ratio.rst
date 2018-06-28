mass mixing ratio derivations
=============================

#. mass mixing ratio from mass density

   ================ ===================================== ====================== =================================
   symbol           description                           unit                   variable name
   ================ ===================================== ====================== =================================
   :math:`\rho`     mass density of total air             :math:`\frac{kg}{m^3}` `density {:}`
   :math:`\rho_{x}` mass density for air component x      :math:`\frac{kg}{m^3}` `<species>_density {:}`
                    (e.g. :math:`\rho_{O_{3}}`)
   :math:`q_{x}`    mass mixing ratio for air component x :math:`\frac{kg}{kg}`  `<species>_mass_mixing_ratio {:}`
                    with regard to total air
   ================ ===================================== ====================== =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      q_{x} = \frac{\rho_{x}}{\rho}


#. mass mixing ratio from volume mixing ratio

   =============== ================================= ===================== ===================================
   symbol          description                       unit                  variable name
   =============== ================================= ===================== ===================================
   :math:`M_{air}` molar mass for total air          :math:`\frac{g}{mol}`    `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`q_{x}`   mass mixing ratio of quantity x   :math:`\frac{kg}{kg}` `<species>_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` volume mixing ratio of quantity x :math:`ppv`           `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ================================= ===================== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      q_{x} = \nu_{x}\frac{M_{x}}{M_{air}}


#. mass mixing ratio dry air from mass density

   ======================= ===================================== ====================== =========================================
   symbol                  description                           unit                   variable name
   ======================= ===================================== ====================== =========================================
   :math:`\rho_{dry\_air}` mass density of dry air               :math:`\frac{kg}{m^3}` `dry_air_density {:}`
   :math:`\rho_{x}`        mass density for air component x      :math:`\frac{kg}{m^3}` `<species>_density {:}`
                           (e.g. :math:`\rho_{O_{3}}`)
   :math:`\bar{q}_{x}`     mass mixing ratio for air component x :math:`\frac{kg}{kg}`  `<species>_mass_mixing_ratio_dry_air {:}`
                           with regard to dry air
   ======================= ===================================== ====================== =========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      q_{x} = \frac{\rho_{x}}{\rho_{dry\_air}}


#. mass mixing ratio dry air from volume mixing ratio dry air

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

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}
