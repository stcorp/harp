aerosol extinction coefficient derivations
==========================================

   .. _derivation_aerosol_extinction_coefficient_from_aerosol_optical_depth:

#. aerosol extinction coefficient from aerosol optical depth

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

      \sigma = \frac{\tau}{\lvert z^{B}(2) - z^{B}(1) \rvert}
