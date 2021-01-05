sensor elevation angle derivations
==================================

   .. _sensor elevation_angle_from_sensor_zenith_angle:

#. sensor elevation angle from sensor zenith angle

   ================== ====================== =========== ============================
   symbol             description            unit        variable name
   ================== ====================== =========== ============================
   :math:`\alpha_{S}` sensor elevation angle :math:`deg` `sensor_elevation_angle {:}`
   :math:`\theta_{S}` sensor zenith angle    :math:`deg` `sensor_zenith_angle {:}`
   ================== ====================== =========== ============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \alpha_{S} = 90 - \theta_{S}
