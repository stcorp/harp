frequency derivations
=====================

#. frequency from wavelength

   =============== ============== =================== ================
   symbol          description    unit                variable name
   =============== ============== =================== ================
   :math:`c`       speed of light :math:`\frac{m}{s}`
   :math:`\lambda` wavelength     :math:`m`           `wavelength {:}`
   :math:`\nu`     frequency      :math:`Hz`          `frequency {:}`
   =============== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \nu = \frac{c}{\lambda}


#. frequency from wavenumber

   =================== ============== =================== ================
   symbol              description    unit                variable name
   =================== ============== =================== ================
   :math:`c`           speed of light :math:`\frac{m}{s}`
   :math:`\nu`         frequency      :math:`Hz`          `frequency {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{m}` `wavenumber {:}`
   =================== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \nu = c\tilde{\nu}
