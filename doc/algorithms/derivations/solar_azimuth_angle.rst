solar azimuth angle derivations
===============================

   .. _derivation_solar_azimuth_angle_from_latitude_and_solar_declination_hour_zenith_angles:

#. solar azimuth angle from latitude and solar declination/hour/zenith angles

   =================== ======================= ========================== ================================
   symbol              description             unit                       variable name
   =================== ======================= ========================== ================================
   :math:`\theta_{0}`  solar zenith angle      :math:`deg`                `solar_zenith_angle {time}`
   :math:`\delta`      solar declination angle :math:`deg`                `solar_declination_angle {time}`
   :math:`\phi`        latitude                :math:`degN`               `latitude {time}`
   :math:`\varphi_{0}` solar azimuth angle     :math:`deg`                `solar_azimuth_angle {time}`
   :math:`\omega`      solar hour angle        :math:`deg`                `solar_hour_angle {time}`
   =================== ======================= ========================== ================================

   .. math::
      :nowrap:

      \begin{eqnarray}
         \varphi_{0} & = & \begin{cases}
            \sin(\frac{\pi}{180}\theta_{0}) = 0, & 0 \\
            \sin(\frac{\pi}{180}\theta_{0}) \neq 0 \wedge \omega > 0, & -\frac{180}{\pi}\arccos(\frac{\sin(\frac{\pi}{180}\delta)\cos(\frac{\pi}{180}\phi) - \cos(\frac{\pi}{180}\omega)\cos(\frac{\pi}{180}\delta)\sin(\frac{\pi}{180}\phi)}{\sin(\frac{\pi}{180}\theta_{0})}) \\
            \sin(\frac{\pi}{180}\theta_{0}) \neq 0 \wedge \omega <= 0, & \frac{180}{\pi}\arccos(\frac{\sin(\frac{\pi}{180}\delta)\cos(\frac{\pi}{180}\phi) - \cos(\frac{\pi}{180}\omega)\cos(\frac{\pi}{180}\delta)\sin(\frac{\pi}{180}\phi)}{\sin(\frac{\pi}{180}\theta_{0})})
         \end{cases}
      \end{eqnarray}
