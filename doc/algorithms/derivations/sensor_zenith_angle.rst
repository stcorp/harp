sensor zenith angle derivations
===============================

   .. _derivation_sensor_zenith_angle_from_sensor_elevation_angle:

#. sensor zenith angle from sensor elevation angle

   ================== ====================== =========== ============================
   symbol             description            unit        variable name
   ================== ====================== =========== ============================
   :math:`\alpha_{S}` sensor elevation angle :math:`deg` `sensor_elevation_angle {:}`
   :math:`\theta_{S}` sensor zenith angle    :math:`deg` `sensor_zenith_angle {:}`
   ================== ====================== =========== ============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{S} = 90 - \alpha_{S}


   .. _derivation_sensor_zenith_angle_from_viewing_zenith_angle:

#. sensor zenith angle from viewing zenith angle

   ================== ==================== =========== ==========================
   symbol             description          unit        variable name
   ================== ==================== =========== ==========================
   :math:`\theta_{S}` sensor zenith angle  :math:`deg` `sensor_zenith_angle {:}`
   :math:`\theta_{V}` viewing zenith angle :math:`deg` `viewing_zenith_angle {:}`
   ================== ==================== =========== ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{S} = 180 - \theta_{V}
