solar zenith angle derivations
==============================

#. solar zenith angle from solar elevation angle

   ================== ===================== =========== =========================
   symbol             description           unit        variable name
   ================== ===================== =========== =========================
   :math:`\alpha_{0}` solar elevation angle :math:`deg` `solar_azimuth_angle {:}`
   :math:`\theta_{0}` solar zenith angle    :math:`deg` `solar_zenith_angle {:}`
   ================== ===================== =========== =========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{0} = 90 - \alpha_{0}

#. solar zenith angle from latitude and solar declination/hour angles

   =================== ======================= ========================== ================================
   symbol              description             unit                       variable name
   =================== ======================= ========================== ================================
   :math:`\theta_{0}`  solar zenith angle      :math:`deg`                `solar_zenith_angle {time}`
   :math:`\delta`      solar declination angle :math:`deg`                `solar_declination_angle {time}`
   :math:`\phi`        latitude                :math:`degN`               `latitude {time}`
   :math:`\omega`      solar hour angle        :math:`deg`                `solar_hour_angle {time}`
   =================== ======================= ========================== ================================

   .. math::

      \theta_{0} = \frac{180}{\pi}\arccos(\sin(\frac{\pi}{180}\delta)\sin(\frac{\pi}{180}\phi) - \cos(\frac{\pi}{180}\omega)\cos(\frac{\pi}{180}\delta)\cos(\frac{\pi}{180}\phi))
