column number density derivations
=================================

#. total column number density for air component from partial column number density profile:

   ================ ======================================= ========================= ==============================================
   symbol           description                             unit                      variable name
   ================ ======================================= ========================= ==============================================
   :math:`c_{x}`    total column number density for air     :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile for air   :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   ================ ======================================= ========================= ==============================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{c_{x}(i)}


#. total column number density for total air from partial column number density profile:

   ============ =========================================== ========================= ====================================
   symbol       description                                 unit                      variable name
   ============ =========================================== ========================= ====================================
   :math:`c`    total column number density for total air   :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c(i)` column number density profile for total air :math:`\frac{molec}{m^2}` `column_number_density {:,vertical}`
   ============ =========================================== ========================= ====================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{c_{x}(i)}


#. column number density for total air from dry air column number density and H2O column number density

   ==================== ================================ ========================= ===================================
   symbol               description                      unit                      variable name
   ==================== ================================ ========================= ===================================
   :math:`c`            column number density            :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{dry\_air}` column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{H2O}`      column number density for H2O    :math:`\frac{molec}{m^2}` `H2O_column_number_density {:}`
   ==================== ================================ ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     c = c_{dry\_air} + c_{H2O}


#. column number density for dry air from total air column number density and H2O column number density

   ==================== ================================ ========================= ===================================
   symbol               description                      unit                      variable name
   ==================== ================================ ========================= ===================================
   :math:`c`            column number density            :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{dry\_air}` column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{H2O}`      column number density for H2O    :math:`\frac{molec}{m^2}` `H2O_column_number_density {:}`
   ==================== ================================ ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     c_{dry\_air} = c - c_{H2O}


#. column number density for H2O from total air column number density and dry air column number density

   ==================== ================================ ========================= ===================================
   symbol               description                      unit                      variable name
   ==================== ================================ ========================= ===================================
   :math:`c`            column number density            :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c_{dry\_air}` column number density of dry air :math:`\frac{molec}{m^2}` `dry_air_column_number_density {:}`
   :math:`c_{H2O}`      column number density for H2O    :math:`\frac{molec}{m^2}` `H2O_column_number_density {:}`
   ==================== ================================ ========================= ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     c_{H2O} = c - c_{dry\_air}


#. column number density for air component from number density:

   ================ =========================================== ========================= =====================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= =====================================
   :math:`c_{x}`    column number density for air component x   :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    (e.g. :math:`c_{O_{3}}`)
   :math:`n_{x}`    number density for air component x          :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c_{x} = n_{x} \lvert z^{B}(2) - z^{B}(1) \rvert


#. column number density for total air from number density:

   ================ =========================================== ========================= ===========================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===========================
   :math:`c`        column number density for total air         :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`n`        number density for total air                :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c = n \lvert z^{B}(2) - z^{B}(1) \rvert


#. column number density for air component from column mass density:

   This conversion applies to both total columns as well as partial column profiles.

   ================== ========================================= ========================= =====================================
   symbol             description                               unit                      variable name
   ================== ========================================= ========================= =====================================
   :math:`c_{x}`      column number density for air component x :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                      (e.g. :math:`n_{O_{3}}`)
   :math:`M_{x}`      molar mass for air component x            :math:`\frac{g}{mol}`
   :math:`N_A`        Avogadro constant                         :math:`\frac{1}{mol}`
   :math:`\sigma_{x}` column mass density for air component x   :math:`\frac{kg}{m^2}`    `<species>_column_density {:}`
                      (e.g. :math:`\sigma_{O_{3}}`)
   ================== ========================================= ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c_{x} = \frac{\sigma_{x}N_{A}}{10^{-3}M_{x}}


#. column number density for total air from column mass density:

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

      c = \frac{\sigma N_{A}}{10^{-3}M_{air}}


#. column number density for air component from volume mixing ratio:

   ================== =========================================== ================================ =====================================
   symbol             description                                 unit                             variable name
   ================== =========================================== ================================ =====================================
   :math:`a`          WGS84 semi-major axis                       :math:`m`
   :math:`b`          WGS84 semi-minor axis                       :math:`m`
   :math:`c_{x}`      column number density for air component x   :math:`\frac{molec}{m^2}`        `<species>_column_number_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   :math:`f`          WGS84 flattening                            :math:`m`
   :math:`g`          gravity                                     :math:`\frac{m}{s^2}`
   :math:`g_{0}`      mean earth gravity                          :math:`\frac{m}{s^2}`
   :math:`g_{surf}`   gravity at surface                          :math:`\frac{m}{s^2}`
   :math:`GM`         WGS84 earth's gravitational constant        :math:`\frac{m^3}{s^2}`
   :math:`M_{air}`    molar mass of total air                     :math:`\frac{g}{mol}`            `molar_mass {:}`
   :math:`N_A`        Avogadro constant                           :math:`\frac{1}{mol}`
   :math:`p`          pressure                                    :math:`Pa`
   :math:`p_{0}`      standard pressure                           :math:`Pa`
   :math:`p^{B}(l)`   pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa`                       `pressure_bounds {:,2}`
   :math:`R`          universal gas constant                      :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T_{0}`      standard temperature                        :math:`K`
   :math:`z`          altitude                                    :math:`m`
   :math:`\nu_{x}`    volume mixing ratio of quantity x           :math:`ppv`                      `<species>_volume_mixing_ratio {:}`
                      with regard to total air
   :math:`\phi`       latitude                                    :math:`degN`                     `latitude {:}`
   :math:`\omega`     WGS84 earth angular velocity                :math:`rad/s`
   ================== =========================================== ================================ =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)}} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         p & = & e^{\frac{\ln(p^{B}(2)) + \ln(p^{B}(1))}{2}} \\
         z & = & -\frac{RT_{0}}{10^{-3}M_{air}g_{0}}\ln(\frac{p}{p_{0}}) \\
         g & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2\right) \\
         c_{x} & = & -\nu_{x}\frac{N_A}{10^{-3}M_{air}g}(p^{B}(2)-p^{B}(1))
      \end{eqnarray}


#. column number density for air component from volume mixing ratio dry air:

   ===================== =========================================== ================================ ===========================================
   symbol                description                                 unit                             variable name
   ===================== =========================================== ================================ ===========================================
   :math:`a`             WGS84 semi-major axis                       :math:`m`
   :math:`b`             WGS84 semi-minor axis                       :math:`m`
   :math:`c_{x}`         column number density for air component x   :math:`\frac{molec}{m^2}`        `<species>_column_number_density {:}`
                         (e.g. :math:`c_{O_{3}}`)
   :math:`f`             WGS84 flattening                            :math:`m`
   :math:`g`             gravity                                     :math:`\frac{m}{s^2}`
   :math:`g_{0}`         mean earth gravity                          :math:`\frac{m}{s^2}`
   :math:`g_{surf}`      gravity at surface                          :math:`\frac{m}{s^2}`
   :math:`GM`            WGS84 earth's gravitational constant        :math:`\frac{m^3}{s^2}`
   :math:`M_{dry\_air}`  molar mass for dry air                      :math:`\frac{g}{mol}`
   :math:`N_A`           Avogadro constant                           :math:`\frac{1}{mol}`
   :math:`p`             pressure                                    :math:`Pa`
   :math:`p_{0}`         standard pressure                           :math:`Pa`
   :math:`p^{B}(l)`      pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa`                       `pressure_bounds {:,2}`
   :math:`R`             universal gas constant                      :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T_{0}`         standard temperature                        :math:`K`
   :math:`z`             altitude                                    :math:`m`
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity x           :math:`ppv`                      `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\phi`          latitude                                    :math:`degN`                     `latitude {:}`
   :math:`\omega`        WGS84 earth angular velocity                :math:`rad/s`
   ===================== =========================================== ================================ ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)}} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         p & = & e^{\frac{\ln(p^{B}(2)) + \ln(p^{B}(1))}{2}} \\
         z & = & -\frac{RT_{0}}{10^{-3}M_{dry\_air}g_{0}}\ln(\frac{p}{p_{0}}) \\
         g & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2\right) \\
         c_{x} & = & -\bar{\nu}_{x}\frac{N_A}{10^{-3}M_{dry\_air}g}(p^{B}(2)-p^{B}(1))
      \end{eqnarray}
