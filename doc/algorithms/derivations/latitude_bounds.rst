latitude bounds derivations
===========================

   .. _derivation_latitude_ranges_from_midpoints:

#. latitude ranges from midpoints

   ===================== =========================================== ============ ================================
   symbol                description                                 unit         variable name
   ===================== =========================================== ============ ================================
   :math:`\phi(i)`       latitude                                    :math:`degN` `latitude {:,latitude}`
   :math:`\phi^{B}(i,l)` latitude boundaries (:math:`l \in \{1,2\}`) :math:`degN` `latitude_bounds {:,latitude,2}`
   ===================== =========================================== ============ ================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         \phi^{B}(1,1) & = & \frac{3\phi(1) - \phi(2)}{2} \\
         \phi^{B}(i,1) & = & \frac{\phi(i-1) + \phi(i)}{2}, 1 < i \leq N \\
         \phi^{B}(i,2) & = & \phi^{B}(i+1,1), 1 \leq i < N \\
         \phi^{B}(N,2) & = & \frac{3\phi(N) - \phi(N-1)}{2}
      \end{eqnarray}

   This formula applies if the harp option ``regrid_out_of_bounds`` is set to ``nan`` or to ``extrapolate``.
   If the option is set to ``edge`` then the first and last boundary value are set to the midpoints
   (:math:`\phi^{B}(1,1) = \phi(1)`, :math:`\phi^{B}(N,2) = \phi(N)`).

   Note that all latitude values will always be clamped to the range :math:`[-90,90]`.
