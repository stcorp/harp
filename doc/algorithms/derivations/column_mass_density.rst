column mass density derivations
===============================

#. column mass density for total air from dry air column mass density and H2O column mass density

   ========================= ============================== ====================== ============================
   symbol                    description                    unit                   variable name
   ========================= ============================== ====================== ============================
   :math:`\sigma`            column mass density            :math:`\frac{kg}{m^2}` `column_density {:}`
   :math:`\sigma_{dry\_air}` column mass density of dry air :math:`\frac{kg}{m^2}` `dry_air_column_density {:}`
   :math:`\sigma_{H_{2}O}`   column mass density for H2O    :math:`\frac{kg}{m^2}` `H2O_column_density {:}`
   ========================= ============================== ====================== ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma = \sigma_{dry\_air} + \sigma_{H_{2}O}


#. column mass density for dry air from total air column mass density and H2O column mass density

   ========================= ============================== ====================== ============================
   symbol                    description                    unit                   variable name
   ========================= ============================== ====================== ============================
   :math:`\sigma`            column mass density            :math:`\frac{kg}{m^2}` `column_density {:}`
   :math:`\sigma_{dry\_air}` column mass density of dry air :math:`\frac{kg}{m^2}` `dry_air_column_density {:}`
   :math:`\sigma_{H_{2}O}`   column mass density for H2O    :math:`\frac{kg}{m^2}` `H2O_column_density {:}`
   ========================= ============================== ====================== ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{dry\_air} = \sigma - \sigma_{H_{2}O}


#. column mass density for H2O from total air column mass density and dry air column mass density

   ========================= ============================== ====================== ============================
   symbol                    description                    unit                   variable name
   ========================= ============================== ====================== ============================
   :math:`\sigma`            column mass density            :math:`\frac{kg}{m^2}` `column_density {:}`
   :math:`\sigma_{dry\_air}` column mass density of dry air :math:`\frac{kg}{m^2}` `dry_air_column_density {:}`
   :math:`\sigma_{H_{2}O}`   column mass density for H2O    :math:`\frac{kg}{m^2}` `H2O_column_density {:}`
   ========================= ============================== ====================== ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{H_{2}O} = \sigma - \sigma_{dry\_air}


#. column mass density for air component from mass density:

   ================== =========================================== ====================== ==============================
   symbol             description                                 unit                   variable name
   ================== =========================================== ====================== ==============================
   :math:`z^{B}(l)`   altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho_{x}`   mass density for air component x            :math:`\frac{kg}{m^3}` `<species>_density {:}`
                      (e.g. :math:`\rho_{O_{3}}`)
   :math:`\sigma_{x}` column mass density for air component x     :math:`\frac{kg}{m^2}` `<species>_column_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   ================== =========================================== ====================== ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{x} = \rho_{x} \lvert z^{B}(2) - z^{B}(1) \rvert


#. column mass density for total air from mass density:

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

     \sigma = \rho \lvert z^{B}(2) - z^{B}(1) \rvert


#. column mass density for air component from column number density:

   This conversion applies to both total columns as well as partial column profiles.

   ================== ========================================= ========================= =====================================
   symbol             description                               unit                      variable name
   ================== ========================================= ========================= =====================================
   :math:`c_{x}`      column number density for air component x :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   :math:`M_{x}`      molar mass for air component x            :math:`\frac{g}{mol}`
   :math:`N_A`        Avogadro constant                         :math:`\frac{1}{mol}`
   :math:`\sigma_{x}` column mass density for air component x   :math:`\frac{kg}{m^2}`    `<species>_column_density {:}`
                      (e.g. :math:`\sigma_{O_{3}}`)
   ================== ========================================= ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma_{x} = \frac{10^{-3}c_{x}M_{x}}{N_{A}}


#. column mass density for total air from column number density:

   This conversion applies to both total columns as well as partial column profiles.

   =============== =================================== ========================= ===========================
   symbol          description                         unit                      variable name
   =============== =================================== ========================= ===========================
   :math:`c`       column number density for total air :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`M_{air}` molar mass for total air            :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`N_A`     Avogadro constant                   :math:`\frac{1}{mol}`
   :math:`\sigma`  column mass density for total air   :math:`\frac{kg}{m^2}`    `column_density {:}`
   =============== =================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma = \frac{10^{-3}c M_{air}}{N_{A}}
