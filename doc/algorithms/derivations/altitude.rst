altitude derivations
====================

#. altitude from geopotential height

   ================= ============================ ===================== =========================
   symbol            description                  unit                  variable name
   ================= ============================ ===================== =========================
   :math:`g_{0}`     mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{wgs84}` gravity at WGS84 ellipsoid   :math:`\frac{m}{s^2}`
   :math:`R_{wgs84}` local earth curvature radius :math:`m`
                     at WGS84 ellipsoid
   :math:`z`         altitude                     :math:`m`             `altitude {:}`
   :math:`z_{g}`     geopotential height          :math:`m`             `geopotential_height {:}`
   :math:`\phi`      latitude                     :math:`degN`          `latitude {:}`
   ================= ============================ ===================== =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{time}`, `{time,vertical}`, or no dimensions at all.

   This equation approximates the mean sea level gravity and radius by that of the reference ellipsoid.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{wgs84} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R_{wgs84} & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z & = & \frac{g_{0}R_{wgs84}z_{g}}{g_{wgs84}R_{wgs84} - g_{0}z_{g}}
      \end{eqnarray}


#. altitude from bounds

   ================ =========================================== ========= =======================
   symbol           description                                 unit      variable name
   ================ =========================================== ========= =======================
   :math:`z`        altitude                                    :math:`m` `altitude {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m` `altitude_bounds {:,2}`
   ================ =========================================== ========= =======================

   The pattern `:` for the dimensions can represent `{vertical}`, or `{time,vertical}`.

   .. math::

      z = \frac{z^{B}(2) + z^{B}(1)}{2}


#. altitude from sensor altitude

   ================= ====================== ========= =====================
   symbol            description            unit      variable name
   ================= ====================== ========= =====================
   :math:`z`         altitude               :math:`m` `altitude {:}`
   :math:`z_{instr}` altitude of the sensor :math:`m` `sensor_altitude {:}`
   ================= ====================== ========= =====================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      z = z_{instr}


#. altitude from pressure

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

   The pressures :math:`p(i)` are expected to be at higher levels than the surface pressure (i.e. lower values).
   This should normally be the case since even for pressure grids that start at the surface, :math:`p_{surf}` should
   equal the lower pressure boundary :math:`p^{B}(1,1)`, whereas :math:`p(1)` should then be between :math:`p^{B}(1,1)`
   and :math:`p^{B}(1,2)` (which is generally not equal to :math:`p^{B}(1,1)`).

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)}} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         g(1) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z_{surf} + \frac{3}{a^2}z_{surf}^2\right) \\
         g(i) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z(i-1) + \frac{3}{a^2}z(i-1)^2\right), 1 < i \leq N \\
         z(1) & = & z_{surf} + 10^{3}\frac{T(1)}{M_{air}(1)}\frac{R}{g(1)}\ln\left(\frac{p_{surf}}{p(i)}\right) \\
         z(i) & = & z(i-1) + 10^{3}\frac{T(i-1)+T(i)}{M_{air}(i-1)+M_{air}(i)}\frac{R}{g(i)}\ln\left(\frac{p(i-1)}{p(i)}\right), 1 < i \leq N
      \end{eqnarray}


#. surface altitude from surface geopotential height

   ================== ============================ ===================== =================================
   symbol             description                  unit                  variable name
   ================== ============================ ===================== =================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{wgs84}`  gravity at WGS84 ellipsoid   :math:`\frac{m}{s^2}`
   :math:`R_{wgs84}`  local earth curvature radius :math:`m`
                      at WGS84 ellipsoid
   :math:`z_{surf}`   surface altitude             :math:`m`             `surface_altitude {:}`
                      (relative to mean sea level)
   :math:`z_{g,surf}` surface geopotential height  :math:`m`             `surface_geopotential_height {:}`
                      (relative to mean sea level)
   :math:`\phi`       latitude                     :math:`degN`          `latitude {:}`
   ================== ============================ ===================== =================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   This equation approximates the mean sea level gravity and radius by that of the reference ellipsoid.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{wgs84} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R_{wgs84} & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z_{surf} & = & \frac{g_{0}R_{wgs84}z_{g,surf}}{g_{wgs84}R_{wgs84} - g_{0}z_{g,surf}}
      \end{eqnarray}


#. tropopause altitude from temperature and altitude/pressure

   ============== =================== ========== ==========================
   symbol         description         unit       variable name
   ============== =================== ========== ==========================
   :math:`p(i)`   pressure            :math:`Pa` `pressure {:,vertical}`
   :math:`T(i)`   temperature         :math:`K`  `temperature {:,vertical}`
   :math:`z(i)`   altitude            :math:`m`  `altitude {:,vertical}`
   :math:`z_{TP}` tropopause altitude :math:`m`  `tropopause_altitude {:}`
   ============== =================== ========== ==========================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The tropopause altitude :math:`z_{TP}` equals the altitude :math:`z(i)` where :math:`i` is the minimum level that satisfies:

   .. math::
      :nowrap:

      \begin{eqnarray}
         & 1 < i < N  & \wedge \\
         & 5000 <= p(i) <= 50000  & \wedge \\
         & \frac{T(i-1)-T(i)}{z(i)-z(i-1)} > 0.002 \wedge \frac{T(i)-T(i+1)}{z(i+1)-z(i)} <= 0.002 & \wedge \\
         & \frac{\sum_{j, i < j < N \wedge z(j+1)-z(i) <= 2000} \frac{T(j)-T(j+1)}{z(j+1)-z(j)}}{\sum_{j, i < j < N \wedge z(j+1)-z(i) <= 2000}{1}} <= 0.002 &
      \end{eqnarray}

   If no such :math:`i` can be found then :math:`z_{TP}` is set to `NaN`.
