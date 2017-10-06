solar elevation angle derivations
=================================

#. solar elevation angle from solar zenith angle

   ================== ===================== =========== ===========================
   symbol             description           unit        variable name
   ================== ===================== =========== ===========================
   :math:`\alpha_{0}` solar elevation angle :math:`deg` `solar_elevation_angle {:}`
   :math:`\theta_{0}` solar zenith angle    :math:`deg` `solar_zenith_angle {:}`
   ================== ===================== =========== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \alpha_{0} = 90 - \theta_{0}
