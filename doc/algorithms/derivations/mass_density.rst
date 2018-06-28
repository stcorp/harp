mass density derivations
========================

#. mass density for air component from number density:

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

      \rho_{x} = \frac{10^{-3}n_{x}M_{x}}{N_{A}}


#. mass density for total air from number density:

   ================ ============================ ========================= ====================
   symbol           description                  unit                      variable name
   ================ ============================ ========================= ====================
   :math:`M_{air}`  molar mass for total air     :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`        number density for total air :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`      Avogadro constant            :math:`\frac{1}{mol}`
   :math:`\rho`     mass density for total air   :math:`\frac{kg}{m^3}`    `density {:}`
   ================ ============================ ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \rho = \frac{10^{-3}n M_{air}}{N_{A}}


#. mass density for air component from column mass density:

   ================== =========================================== ====================== =====================================
   symbol             description                                 unit                   variable name
   ================== =========================================== ====================== =====================================
   :math:`z^{B}(l)`   altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho_{x}`   mass density for air component x            :math:`\frac{kg}{m^3}` `<species>_density {:}`
                      (e.g. :math:`\rho_{O_{3}}`)
   :math:`\sigma_{x}` column mass density for air component x     :math:`\frac{kg}{m^2}` `<species>_column_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   ================== =========================================== ====================== =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho_{x} = \frac{\sigma_{x}}{\lvert z^{B}(2) - z^{B}(1) \rvert}


#. mass density for total air from dry air mass density and H2O mass density

   ======================= ======================= ====================== =====================
   symbol                  description             unit                   variable name
   ======================= ======================= ====================== =====================
   :math:`\rho`            mass density            :math:`\frac{kg}{m^3}` `density {:}`
   :math:`\rho_{dry\_air}` mass density of dry air :math:`\frac{kg}{m^3}` `dry_air_density {:}`
   :math:`\rho_{H_{2}O}`   mass density for H2O    :math:`\frac{kg}{m^3}` `H2O_density {:}`
   ======================= ======================= ====================== =====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho = \rho_{dry\_air} + \rho_{H_{2}O}


#. mass density for dry air from total air mass density and H2O mass density

   ======================= ======================= ====================== =====================
   symbol                  description             unit                   variable name
   ======================= ======================= ====================== =====================
   :math:`\rho`            mass density            :math:`\frac{kg}{m^3}` `density {:}`
   :math:`\rho_{dry\_air}` mass density of dry air :math:`\frac{kg}{m^3}` `dry_air_density {:}`
   :math:`\rho_{H_{2}O}`   mass density for H2O    :math:`\frac{kg}{m^3}` `H2O_density {:}`
   ======================= ======================= ====================== =====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho_{dry\_air} = \rho - \rho_{H_{2}O}


#. mass density for H2O from total air mass density and dry air mass density

   ======================= ======================= ====================== =====================
   symbol                  description               unit                 variable name
   ======================= ======================= ====================== =====================
   :math:`\rho`            mass density            :math:`\frac{kg}{m^3}` `density {:}`
   :math:`\rho_{dry\_air}` mass density of dry air :math:`\frac{kg}{m^3}` `dry_air_density {:}`
   :math:`\rho_{H_{2}O}`   mass density for H2O    :math:`\frac{kg}{m^3}` `H2O_density {:}`
   ======================= ======================= ====================== =====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho_{H_{2}O} = \rho - \rho_{dry\_air}


#. mass density for total air from column mass density:

   ================ =========================================== ====================== =======================
   symbol           description                                 unit                   variable name
   ================ =========================================== ====================== =======================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho`     mass density for total air                  :math:`\frac{kg}{m^3}` `density {:}`
   :math:`\sigma`   column mass density for total air           :math:`\frac{kg}{m^2}` `column_density {:}`
   ================ =========================================== ====================== =======================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho = \frac{\sigma}{\lvert z^{B}(2) - z^{B}(1) \rvert}
