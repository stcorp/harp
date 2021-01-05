sensor azimuth angle derivations
================================

   .. _derivation_sensor_azimuth_angle_from_viewing_azimuth_angle:

#. sensor azimuth angle from viewing azimuth angle

   =================== ===================== =========== ===========================
   symbol              description           unit        variable name
   =================== ===================== =========== ===========================
   :math:`\varphi_{S}` sensor azimuth angle  :math:`deg` `sensor_azimuth_angle {:}`
   :math:`\varphi_{V}` viewing azimuth angle :math:`deg` `viewing_azimuth_angle {:}`
   =================== ===================== =========== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \varphi_{S} = 180 - \varphi_{V}
