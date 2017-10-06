solar declination angle derivations
===================================

#. solar declination angle from datetime

   =================== ======================= ========================== ================================
   symbol              description             unit                       variable name
   =================== ======================= ========================== ================================
   :math:`t`           datetime (UTC)          :math:`s` since 2000-01-01 `datetime {time}`
   :math:`\delta`      solar declination angle :math:`deg`                `solar_declination_angle {time}`
   =================== ======================= ========================== ================================

   .. math::
      :nowrap:

      \begin{eqnarray}
         A & = & 2\pi \left( \frac{t + 10 \cdot 86400}{365.2422 \cdot 86400} - \lfloor \frac{t + 10 \cdot 86400}{365.2422 \cdot 86400} \rfloor \right) \\
         B & = & A + 2 \cdot 0.0167 \sin( 2\pi \left( \frac{t - 2 \cdot 86400}{365.2422 \cdot 86400} - \lfloor \frac{t - 2 \cdot 86400}{365.2422 \cdot 86400} \rfloor \right) ) \\
         \delta & = & -\frac{180}{\pi} \arcsin( \sin(\frac{\pi}{180} 23.44) \cos(B) )
      \end{eqnarray}
