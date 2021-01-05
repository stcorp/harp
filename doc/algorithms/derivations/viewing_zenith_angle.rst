viewing zenith angle derivations
================================

   .. _derivation_viewing_zenith_angle_from_sensor_zenith_angle:

#. viewing zenith angle from sensor zenith angle

   ================== ==================== =========== ==========================
   symbol             description          unit        variable name
   ================== ==================== =========== ==========================
   :math:`\theta_{S}` sensor zenith angle  :math:`deg` `sensor_zenith_angle {:}`
   :math:`\theta_{V}` viewing zenith angle :math:`deg` `viewing_zenith_angle {:}`
   ================== ==================== =========== ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{V} = 180 - \theta_{S}


   .. _derivation_viewing_zenith_angle_from_viewing_elevation_angle:

#. viewing zenith angle from viewing elevation angle

   ================== ======================= =========== =============================
   symbol             description             unit        variable name
   ================== ======================= =========== =============================
   :math:`\alpha_{V}` viewing elevation angle :math:`deg` `viewing_elevation_angle {:}`
   :math:`\theta_{V}` viewing zenith angle    :math:`deg` `viewing_zenith_angle {:}`
   ================== ======================= =========== =============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{V} = 90 - \alpha_{V}
