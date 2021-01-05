gravity derivations
===================

   .. _derivation_normal_gravity_at_sea_level_from_latitude:

#. normal gravity at sea level from latitude

   ============= =========================== ===================== =====================
   symbol        description                 unit                  variable name
   ============= =========================== ===================== =====================
   :math:`g`     normal gravity at sea level :math:`\frac{m}{s^2}` `gravity {:}`
   :math:`\phi`  latitude                    :math:`degN`          `latitude {:}`
   ============= =========================== ===================== =====================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

     \begin{eqnarray}
         g & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}}
      \end{eqnarray}


   .. _derivation_gravity_at_specific_altitude:

#. gravity at specific altitude

   ============== ==================================== ======================= =================================
   symbol         name                                 unit                    variable name
   ============== ==================================== ======================= =================================
   :math:`a`      WGS84 semi-major axis                :math:`m`
   :math:`b`      WGS84 semi-minor axis                :math:`m`
   :math:`f`      WGS84 flattening                     :math:`m`
   :math:`g_{h}`  gravity at specific height           :math:`\frac{m}{s^2}`   `gravity {:,vertical}`
   :math:`g`      normal gravity at sea level          :math:`\frac{m}{s^2}`   `gravity {:}`
   :math:`GM`     WGS84 earth's gravitational constant :math:`\frac{m^3}{s^2}`
   :math:`z`      altitude                             :math:`m`               `altitude {:,vertical}`
   :math:`\phi`   latitude                             :math:`degN`            `latitude {:}`
   :math:`\omega` WGS84 earth angular velocity         :math:`rad/s`
   ============== ==================================== ======================= =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         m & = & \frac{\omega^2a^2b}{GM} \\
         g_{h} & = & g \left[ 1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2 \right] \\
      \end{eqnarray}


   .. _derivation_gravity_at_earth_surface:

#. gravity at earth surface

   ================ ==================================== ======================= =================================
   symbol           name                                 unit                    variable name
   ================ ==================================== ======================= =================================
   :math:`a`        WGS84 semi-major axis                :math:`m`
   :math:`b`        WGS84 semi-minor axis                :math:`m`
   :math:`f`        WGS84 flattening                     :math:`m`
   :math:`g_{surf}` gravity at surface altitude          :math:`\frac{m}{s^2}`   `surface_gravity {:}`
   :math:`g`        normal gravity at sea level          :math:`\frac{m}{s^2}`   `gravity {:}`
   :math:`GM`       WGS84 earth's gravitational constant :math:`\frac{m^3}{s^2}`
   :math:`z_{surf}` surface altitude                     :math:`m`               `surface_altitude {:}`
   :math:`\phi`     latitude                             :math:`degN`            `latitude {:}`
   :math:`\omega`   WGS84 earth angular velocity         :math:`rad/s`
   ================ ==================================== ======================= =================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         m & = & \frac{\omega^2a^2b}{GM} \\
         g_{surf} & = & g \left[ 1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2 \right] \\
      \end{eqnarray}
