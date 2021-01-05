solar hour angle derivations
============================

   .. _derivation_solar_hour_angle_from_datetime_and_longitude:

#. solar hour angle from datetime and longitude

   =================== ======================= ========================== ================================
   symbol              description             unit                       variable name
   =================== ======================= ========================== ================================
   :math:`EOT`         equation of time        :math:`minutes`
   :math:`t`           datetime (UTC)          :math:`s` since 2000-01-01 `datetime {time}`
   :math:`\eta`        orbit angle of the      :math:`rad`
                       earth around the sun
   :math:`\lambda`     longitude               :math:`degE`               `longitude {time}`
   :math:`\omega`      solar hour angle        :math:`deg`                `solar_hour_angle {time}`
   =================== ======================= ========================== ================================

   .. math::
      :nowrap:

      \begin{eqnarray}
         A & = & 2\pi \left( \frac{t + 10 \cdot 86400}{365.2422 \cdot 86400} - \lfloor \frac{t + 10 \cdot 86400}{365.2422 \cdot 86400} \rfloor \right) \\
         B & = & A + 2 \cdot 0.0167 \sin( 2\pi \left( \frac{t - 2 \cdot 86400}{365.2422 \cdot 86400} - \lfloor \frac{t - 2 \cdot 86400}{365.2422 \cdot 86400} \rfloor \right) ) \\
         C & = & \frac{A - \arctan(\frac{\tan(B)}{cos(\frac{\pi}{180} 23.44)})}{\pi} \\
         EOT & = & 720 \left( C - \lfloor C + 0.5 \rfloor \right) \\
         \omega & = & \lambda + 360 \left( \frac{t}{86400} - \lfloor \frac{t}{86400} \rfloor + \frac{EOT}{24 \cdot 60} \right) - 180
      \end{eqnarray}

   The solar hour angle will be mapped to [-180,180].
