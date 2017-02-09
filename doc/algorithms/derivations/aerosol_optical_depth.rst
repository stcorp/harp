aerosol optical depth derivations
=================================

#. total aerosol optical depth from partial aerosol optical depth profile

   =============== =========================== ========= ====================================
   symbol          description                 unit      variable name
   =============== =========================== ========= ====================================
   :math:`\tau`    total aerosol optical depth :math:`-` `aerosol_optical_depth {:}`
   :math:`\tau(i)` aerosol optical depth       :math:`-` `aerosol_optical_depth {:,vertical}`
   =============== =========================== ========= ====================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \tau = \sum_{i}{\tau(i)}


#. aerosol optical depth from aerosol extinction coefficient

   ================ =========================================== =================== ====================================
   symbol           description                                 unit                variable name
   ================ =========================================== =================== ====================================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`           `altitude_bounds {:,2}`
   :math:`\sigma`   aerosol extinction coefficient              :math:`\frac{1}{m}` `aerosol_extinction_coefficient {:}`
   :math:`\tau`     aerosol optical depth                       :math:`-`           `aerosol_optical_depth {:}`
   ================ =========================================== =================== ====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \tau = \sigma \lvert z^{B}(2) - z^{B}(1) \rvert
