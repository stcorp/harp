degrees of freedom for signal
=============================

   .. _derivation_degrees_of_freedom_for_signal_profile_from_avk:

#. degrees of freedom for signal profile from avk

   ==================== ======================================= ==== ===============================
   symbol               description                             unit variable name
   ==================== ======================================= ==== ===============================
   :math:`d^{x}_{s}(i)` degrees of freedom for signal per level      `<x>_dfs {:,vertical}`
   :math:`A^{x}(i,j)`   AVK of a profile x                           `<x>_avk {:,vertical,vertical}`
   ==================== ======================================= ==== ===============================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      d^{x}_{s}(i) = A^{x}(i,i)


   .. _derivation_degrees_of_freedom_for_signal_from_degrees_of_freedom_for_signal_profile:

#. degrees of freedom for signal from degrees of freedom for signal profile

   ==================== ======================================= ==== ======================
   symbol               description                             unit variable name
   ==================== ======================================= ==== ======================
   :math:`d^{x}_{s}`    degrees of freedom for signal                `<x>_dfs {:}`
   :math:`d^{x}_{s}(i)` degrees of freedom for signal per level      `<x>_dfs {:,vertical}`
   ==================== ======================================= ==== ======================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      d^{x}_{s} = \sum_{i}{d^{x}_{s}(i)}

