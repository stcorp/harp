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
