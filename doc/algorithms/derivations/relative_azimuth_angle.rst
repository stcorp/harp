relative azimuth angle derivations
==================================

#. relative azimuth angle from sensor and solar azimuth angle

   =================== ====================== =========== ============================
   symbol              description            unit        variable name
   =================== ====================== =========== ============================
   :math:`\varphi_{0}` solar azimuth angle    :math:`deg` `solar_azimuth_angle {:}`
   :math:`\varphi_{r}` relative azimuth angle :math:`deg` `relative_azimuth_angle {:}`
   :math:`\varphi_{S}` sensor azimuth angle   :math:`deg` `sensor_azimuth_angle {:}`
   =================== ====================== =========== ============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
        \Delta\varphi & = & \varphi_{0} - \varphi_{S} \\
        \Delta\varphi & = & \begin{cases}
            \Delta\varphi \geq 360, & \Delta\varphi - 360 \\
            0 \leq \Delta\varphi < 360, & \Delta\varphi \\
            \Delta\varphi < 0, & \Delta\varphi + 360
         \end{cases} \\
        \varphi_{r} & = & \left|\Delta\varphi - 180 \right|
      \end{eqnarray}
