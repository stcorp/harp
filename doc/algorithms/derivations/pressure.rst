pressure derivations
====================

#. pressure from bounds

   ================ =========================================== ========== =======================
   symbol           description                                 unit       variable name
   ================ =========================================== ========== =======================
   :math:`p`        pressure                                    :math:`Pa` `pressure {:}`
   :math:`p^{B}(l)` pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa` `pressure_bounds {:,2}`
   ================ =========================================== ========== =======================

   The pattern `:` for the dimensions can represent `{vertical}`, or `{time,vertical}`.

   .. math::

      p = e^{\frac{ln(z^{B}(2)) + ln(z^{B}(1))}{2}}


#. pressure from altitude

   ================== ============================ ================================ ==========================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==========================
   :math:`a`          WGS84 semi-major axis        :math:`m`
   :math:`b`          WGS84 semi-minor axis        :math:`m`
   :math:`f`          WGS84 flattening             :math:`m`
   :math:`g`          gravity                      :math:`\frac{m}{s^2}`
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{surf}`   gravity at surface           :math:`\frac{m}{s^2}`
   :math:`GM`         WGS84 earth's gravitational  :math:`\frac{m^3}{s^2}`
                      constant
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`Pa`                       `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`Pa`                       `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z(i)`       altitude                     :math:`m`                        `altitude {:,vertical}`
   :math:`z_{surf}`   surface height               :math:`m`                        `surface_altitude {:}`
   :math:`\phi`       latitude                     :math:`degN`                     `latitude {:}`
   :math:`\omega`     WGS84 earth angular velocity :math:`rad/s`
   ================== ============================ ================================ ==========================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{surf}` need to use the same definition of 'surface'.

   The altitudes :math:`z(i)` are expected to be above the surface height. This should normally be the case
   since even for altitude grids that start at the surface, :math:`z_{surf}` should equal the lower altitude boundary
   :math:`z^{B}(1,1)`, whereas :math:`z(1)` should then be between :math:`z^{B}(1,1)` and :math:`z^{B}(1,2)`
   (which is generally not equal to :math:`z^{B}(1,1)`).

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)}} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         g(1) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)\frac{z_{surf}+z(1)}{2} + \frac{3}{a^2}\left(\frac{z_{surf}+z(1)}{2}\right)^2\right) \\
         g(i) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)\frac{z(i-1)+z(i)}{2} + \frac{3}{a^2}\left(\frac{z(i-1)+z(i)}{2}\right)^2\right), 1 < i \leq N \\
         p(1) & = & p_{surf}e^{-10^{-3}\frac{M_{air}(1)}{T(1)}\frac{g(1)}{R}\left(z(i)-z_{surf}\right)} \\
         p(i) & = & p(i-1)e^{-10^{-3}\frac{M_{air}(i-1)+M_{air}(i)}{T(i-1)+T(i)}\frac{g(i)}{R}\left(z(i)-z(i-1)\right)}, 1 < i \leq N
      \end{eqnarray}


#. pressure from geopotential height

   ================== ============================ ================================ ==================================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`Pa`                       `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`Pa`                       `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z_{g}(i)`   geopotential height          :math:`m`                        `geopotential_height {:,vertical}`
   :math:`z_{g,surf}` surface geopotential height  :math:`m`                        `surface_geopotential_height {:}`
   ================== ============================ ================================ ==================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{g,surf}` need to use the same definition of 'surface'.

   The geopotential heights :math:`z_{g}(i)` are expected to be above the surface geopotential height. This should
   normally be the case since even for geopotential height grids that start at the surface, :math:`z_{g,surf}` should
   equal the lower altitude boundary :math:`z^{B}_{g}(1,1)`, whereas :math:`z_{g}(1)` should then be between
   :math:`z^{B}_{g}(1,1)` and :math:`z^{B}_{g}(1,2)` (which is generally not equal to :math:`z^{B}_{g}(1,1)`).

   .. math::
      :nowrap:

      \begin{eqnarray}
         p(1) & = & p_{surf}e^{-10^{-3}\frac{M_{air}(1)}{T(1)}\frac{g_{0}}{R}\left(z_{g}(i)-z_{g,surf}\right)} \\
         p(i) & = & p(i-1)e^{-10^{-3}\frac{M_{air}(i-1)+M_{air}(i)}{T(i-1)+T(i)}\frac{g_{0}}{R}\left(z_{g}(i)-z_{g}(i-1)\right)}, 1 < i \leq N
      \end{eqnarray}


#. pressure from number density and temperature

   ========= ================== ============================ ====================
   symbol    description        unit                         variable name
   ========= ================== ============================ ====================
   :math:`k` Boltzmann constant :math:`\frac{kg m^2}{K s^2}`
   :math:`n` number density     :math:`\frac{molec}{m^3}`    `number_density {:}`
   :math:`p` pressure           :math:`Pa`                   `pressure {:}`
   :math:`T` temperature        :math:`K`                    `temperature {:}`
   ========= ================== ============================ ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p = nkT


#. surface pressure from surface number density and surface temperature

   ================ ====================== ============================ ============================
   symbol           description            unit                         variable name
   ================ ====================== ============================ ============================
   :math:`k`        Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{surf}` surface number density :math:`\frac{molec}{m^3}`    `surface_number_density {:}`
   :math:`p_{surf}` surface pressure       :math:`Pa`                   `surface_pressure {:}`
   :math:`T_{surf}` surface temperature    :math:`K`                    `surface_temperature {:}`
   ================ ====================== ============================ ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{surf} = n_{surf}kT_{surf}
