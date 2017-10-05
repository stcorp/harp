solar azimuth angle derivations
===============================

.. _`solar azimuth angle from datetime derivation`:

#. solar azimuth angle from datetime and latitude/longitude

   =================== ======================= ========================== ==============================
   symbol              description             unit                       variable name
   =================== ======================= ========================== ==============================
   :math:`EOT`         equation of time        :math:`h`
   :math:`t`           datetime                :math:`s` since 2000-01-01 `datetime {time}`
   :math:`\alpha_{0}`  solar elevation angle   :math:`deg`                `solar_elevation_angle {time}`
   :math:`\delta`      solar declination angle :math:`rad`
   :math:`\eta`        orbit angle of the      :math:`rad`
                       earth around the sun
   :math:`\lambda`     longitude               :math:`degE`               `longitude {time}`
   :math:`\phi`        latitude                :math:`degN`               `latitude {time}`
   :math:`\varphi_{0}` solar azimuth angle     :math:`deg`                `solar_azimuth_angle {time}`
   :math:`\Omega`      solar hour angle        :math:`rad`
   =================== ======================= ========================== ==============================

   .. math::
      :nowrap:

      \begin{eqnarray}
         \eta & = & 2\pi \left( \frac{t}{365.2422 \cdot 86400} - \lvert \frac{t}{365.2422 \cdot 86400} \rvert \right) \\
         \delta & = & 0.006918 - 0.399912 \cos(\eta) - 0.006758 \cos(2\eta) - 0.002697 \cos(3\eta) + \\
            & & 0.070257 \sin(\eta) + 0.000907 \sin(2\eta) + 0.001480 \sin(3\eta) \\
         EOT & = & 0.0072 \cos(\eta) - 0.0528 \cos(2\eta) - 0.0012 \cos(3\eta) - \\
            & & 0.1229 \sin(\eta) - 0.1565 \sin(2\eta) - 0.0041 \sin(3\eta) \\
         f_{day} & = & \frac{t}{86400} - \lvert \frac{t}{86400} \rvert \\
         \Omega & = & 2\pi\left(f_{day} + \frac{\lambda}{360} + \frac{EOT-12}{24}\right) \\
         \alpha_{0} & = & \frac{180}{\pi}\arcsin(\sin(\delta)\sin(\frac{\pi}{180}\phi) +
            \cos(\delta)\cos(\frac{\pi}{180}\phi)\cos(\Omega)) \\
         \varphi_{0} & = & \begin{cases}
            \alpha_{0} = 0, & 0 \\
            \alpha_{0} \neq 0, & \frac{180}{\pi}\arctan(\frac{\cos(\delta)\sin(\Omega)}{\cos(\frac{\pi}{180}\alpha_{0})},
               \frac{-\sin(\delta)\cos(\frac{\pi}{180}\phi) +
               \cos(\delta)\sin(\frac{\pi}{180}\phi)\cos(\Omega)}{\cos(\frac{\pi}{180}\alpha_{0})})
         \end{cases}
      \end{eqnarray}
