scattering angle derivations
============================

#. scattering angle from sensor and solar angles

   =================== ====================== =========== ==========================
   symbol              description            unit        variable name
   =================== ====================== =========== ==========================
   :math:`\theta_{0}`  solar zenith angle     :math:`deg` `solar_zenith_angle {:}`
   :math:`\Theta_{s}`  scattering angle       :math:`deg` `scattering_angle {:}`
   :math:`\theta_{S}`  sensor zenith angle    :math:`deg` `sensor_zenith_angle {:}`
   :math:`\varphi_{r}` relative azimuth angle :math:`deg` `relative_azimuth_angle {:}`
   =================== ====================== =========== ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::
      \Theta_{s} = \frac{180}{\pi}\arccos\left(-\cos\left(\frac{\pi}{180}\theta_{0}\right)\cos\left(\frac{\pi}{180}\theta_{S}\right) - 
            \sin\left(\frac{\pi}{180}\theta_{0}\right)\sin\left(\frac{\pi}{180}\theta_{S}\right)\cos\left(\frac{\pi}{180}\varphi_{r}\right)\right)
