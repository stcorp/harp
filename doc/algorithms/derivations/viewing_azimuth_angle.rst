viewing azimuth angle derivations
=================================

#. viewing azimuth angle from sensor azimuth angle

   =================== ===================== =========== ===========================
   symbol              description           unit        variable name
   =================== ===================== =========== ===========================
   :math:`\varphi_{S}` sensor azimuth angle  :math:`deg` `sensor_azimuth_angle {:}`
   :math:`\varphi_{V}` viewing azimuth angle :math:`deg` `viewing_azimuth_angle {:}`
   =================== ===================== =========== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \varphi_{V} = 180 - \varphi_{S}
