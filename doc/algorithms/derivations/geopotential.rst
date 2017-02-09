geopotential derivations
========================

#. geopotential from geopotential height

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

      \Phi = g_{0}z_{g}

#. surface geopotential from surface geopotential height

   =================== =========================== ======================= =================================
   symbol              description                 unit                    variable name
   =================== =========================== ======================= =================================
   :math:`g_{0}`       mean earth gravity          :math:`\frac{m}{s^2}`
   :math:`z_{g,surf}`  surface geopotential height :math:`m`               `surface_geopotential_height {:}`
   :math:`\Phi_{surf}` surface geopotential        :math:`\frac{m^2}{s^2}` `surface_geopotential {:}`
   =================== =========================== ======================= =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \Phi_{surf} = g_{0}z_{g,surf}
