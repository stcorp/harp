wavenumber derivations
======================

#. wavenumber from frequency

   =================== ============== =================== ================
   symbol              description    unit                variable name
   =================== ============== =================== ================
   :math:`c`           speed of light :math:`\frac{m}{s}`
   :math:`\nu`         frequency      :math:`Hz`          `frequency {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{m}` `wavenumber {:}`
   =================== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \tilde{\nu} = \frac{\nu}{c}


#. wavenumber from wavelength

   =================== ============== =================== ================
   symbol              description    unit                variable name
   =================== ============== =================== ================
   :math:`\lambda`     wavelength     :math:`m`           `wavelength {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{m}` `wavenumber {:}`
   =================== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \tilde{\nu} = \frac{1}{\lambda}
