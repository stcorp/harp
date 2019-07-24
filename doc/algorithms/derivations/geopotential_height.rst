geopotential height derivations
===============================

#. geopotential height from geopotential

   ============= =================== ======================= =========================
   symbol        description         unit                    variable name
   ============= =================== ======================= =========================
   :math:`g_{0}` mean earth gravity  :math:`\frac{m}{s^2}`
   :math:`z_{g}` geopotential height :math:`m`               `geopotential_height {:}`
   :math:`\Phi`  geopotential        :math:`\frac{m^2}{s^2}` `geopotential {:}`
   ============= =================== ======================= =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      z_{g} = \frac{\Phi}{g_{0}}


#. geopotential height from altitude

   ================= ============================ ===================== =========================
   symbol            description                  unit                  variable name
   ================= ============================ ===================== =========================
   :math:`g_{0}`     mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g`         nominal gravity at sea level :math:`\frac{m}{s^2}`
   :math:`R`         local earth curvature radius :math:`m`
   :math:`z`         altitude                     :math:`m`             `altitude {:}`
   :math:`z_{g}`     geopotential height          :math:`m`             `geopotential_height {:}`
   :math:`\phi`      latitude                     :math:`degN`          `latitude {:}`
   ================= ============================ ===================== =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{time}`, `{time,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z_{g} & = & \frac{g}{g_{0}}\frac{Rz}{z + R}
      \end{eqnarray}


#. geopotential height from pressure

   ================== ============================ ================================ ==================================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`Pa`                       `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`Pa`                       `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z_{g}(i)`   geopotential height          :math:`m`                        `geopotential_height {:,vertical}`
   :math:`z_{g,surf}` surface geopotential height  :math:`m`                        `surface_geopotential_height {:}`
   ================== ============================ ================================ ==================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{g,surf}` need to use the same definition of 'surface'.

   The pressures :math:`p(i)` are expected to be at higher levels than the surface pressure (i.e. lower values).
   This should normally be the case since even for pressure grids that start at the surface, :math:`p_{surf}` should
   equal the lower pressure boundary :math:`p^{B}(1,1)`, whereas :math:`p(1)` should then be between :math:`p^{B}(1,1)`
   and :math:`p^{B}(1,2)` (which is generally not equal to :math:`p^{B}(1,1)`).

   .. math::
      :nowrap:

      \begin{eqnarray}
         z_{g}(1) & = & z_{g,surf} + 10^{3}\frac{T(1)}{M_{air}(1)}\frac{R}{g_{0}}\ln\left(\frac{p_{surf}}{p(i)}\right) \\
         z_{g}(i) & = & z_{g}(i-1) + 10^{3}\frac{T(i-1)+T(i)}{M_{air}(i-1)+M_{air}(i)}\frac{R}{g_{0}}\ln\left(\frac{p(i-1)}{p(i)}\right), 1 < i \leq N
      \end{eqnarray}


#. surface geopotential height from surface geopotential

   =================== =========================== ======================= =================================
   symbol              description                 unit                    variable name
   =================== =========================== ======================= =================================
   :math:`g_{0}`       mean earth gravity          :math:`\frac{m}{s^2}`
   :math:`z_{g,surf}`  surface geopotential height :math:`m`               `surface_geopotential_height {:}`
   :math:`\Phi_{surf}` surface geopotential        :math:`\frac{m^2}{s^2}` `surface_geopotential {:}`
   =================== =========================== ======================= =================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      z_{g,surf} = \frac{\Phi_{surf}}{g_{0}}


#. surface geopotential height from surface altitude

   ================== ============================ ===================== =================================
   symbol             description                  unit                  variable name
   ================== ============================ ===================== =================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g`          nominal gravity at sea level :math:`\frac{m}{s^2}`
   :math:`R`          local earth curvature radius :math:`m`
   :math:`z_{surf}`   surface altitude             :math:`m`             `surface_altitude {:}`
   :math:`z_{g,surf}` surface geopotential height  :math:`m`             `surface_geopotential_height {:}`
   :math:`\phi`       latitude                     :math:`degN`          `latitude {:}`
   ================== ============================ ===================== =================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z_{g,surf} & = & \frac{g}{g_{0}}\frac{Rz_{surf}}{z_{surf} + R}
      \end{eqnarray}
