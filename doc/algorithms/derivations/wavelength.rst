wavelength derivations
======================

   .. _derivation_wavelength_from_frequency:

#. wavelength from frequency

   =============== ============== =================== ================
   symbol          description    unit                variable name
   =============== ============== =================== ================
   :math:`c`       speed of light :math:`\frac{m}{s}`
   :math:`\nu`     frequency      :math:`Hz`          `frequency {:}`
   :math:`\lambda` wavelength     :math:`m`           `wavelength {:}`
   =============== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \lambda = \frac{c}{\nu}


   .. _derivation_wavelength_from_wavenumber:

#. wavelength from wavenumber

   =================== ============== =================== ================
   symbol              description    unit                variable name
   =================== ============== =================== ================
   :math:`\lambda`     wavelength     :math:`m`           `wavelength {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{m}` `wavenumber {:}`
   =================== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \lambda = \frac{1}{\tilde{\nu}}
