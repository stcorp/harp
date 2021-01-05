angstrom exponent derivations
=============================

   .. _derivation_angstrom_exponent_from_aerosol_optical_depth:

#. angstrom exponent from aerosol optical depth

   ================== ===================== ========== ====================================
   symbol             description           unit       variable name
   ================== ===================== ========== ====================================
   :math:`\alpha`     angstrom exponent     :math:`-`  `angstrom exponent {:}`
   :math:`\lambda(i)` wavelength            :math:`nm` `wavelength {:,spectral}`
   :math:`\tau(i)`    aerosol optical depth :math:`-`  `aerosol_optical_depth {:,spectral}`
   ================== ===================== ========== ====================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.
   
   The algorithm is based on the linear regression of the aerosol optical depth in the logarithmic domain (for both the aerosol optical depth and the wavelength). It requires the spectral dimension to be of length 2 or higher.

   .. math::
      :nowrap:

      \begin{eqnarray}
        \tilde{\lambda} & = & \sum_{i=1}^{N}{\frac{\ln(\lambda(i))}{N}} \\
        \tilde{\tau} & = & \sum_{i=1}^{N}{\frac{\ln(\tau(i))}{N}} \\
        \alpha & = & -\frac{\sum_{i=1}^{N}{(\ln(\lambda(i)) - \tilde{\lambda})(\ln(\tau(i)) - \tilde{\tau})}}{\sum_{i=1}^{N}{(\ln(\lambda(i)) - \tilde{\lambda})^2}}\\
      \end{eqnarray}
