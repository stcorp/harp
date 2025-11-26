reflectance derivations
=======================

   .. _reflectance_from_radiance_and_solar_irradiance:

#. reflectance from radiance and solar irradiance

   ================== ================== =============== ========================
   symbol             description        unit            variable name
   ================== ================== =============== ========================
   :math:`\theta_{0}` solar zenith angle :math:`deg`     `solar_zenith_angle {:}`
   :math:`L`          radiance           :math:`W/m2/sr` `radiance {:}`
   :math:`E`          solar irradiance   :math:`W/m2`    `irradiance {:}`
   :math:`R`          reflectance        :math:`-`       `reflectance {:}`
   ================== ================== =============== ========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      R = \frac{\pi L}{E\cos(\theta_{0})}

   .. _reflectance_from_wavelength_radiance_and_wavelength_solar_irradiance:

#. reflectance from wavelength radiance and wavelength solar irradiance

   ================== ================== ================= ===========================
   symbol             description        unit              variable name
   ================== ================== ================= ===========================
   :math:`\theta_{0}` solar zenith angle :math:`deg`       `solar_zenith_angle {:}`
   :math:`L(\lambda)` radiance           :math:`W/m2/sr/m` `wavelength_radiance {:}`
   :math:`E(\lambda)` solar irradiance   :math:`W/m2/m`    `wavelength_irradiance {:}`
   :math:`R`          reflectance        :math:`-`         `reflectance {:}`
   ================== ================== ================= ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      R = \frac{\pi L(\lambda)}{E(\lambda)\cos(\theta_{0})}
