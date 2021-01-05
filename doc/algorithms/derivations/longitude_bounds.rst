longitude bounds derivations
============================

   .. _derivation_longitude_ranges_from_midpoints:

#. longitude ranges from midpoints

   ======================== ============================================ ============ ==================================
   symbol                   description                                  unit         variable name
   ======================== ============================================ ============ ==================================
   :math:`\lambda(i)`       longitude                                    :math:`degE` `longitude {:,longitude}`
   :math:`\lambda^{B}(i,l)` longitude boundaries (:math:`l \in \{1,2\}`) :math:`degE` `longitude_bounds {:,longitude,2}`
   ======================== ============================================ ============ ==================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         \lambda^{B}(1,1) & = & \frac{3\lambda(1) - \lambda(2)}{2} \\
         \lambda^{B}(i,1) & = & \frac{\lambda(i-1) + \lambda(i)}{2}, 1 < i \leq N \\
         \lambda^{B}(i,2) & = & \lambda^{B}(i+1,1), 1 \leq i < N \\
         \lambda^{B}(N,2) & = & \frac{3\lambda(N) - \lambda(N-1)}{2}
      \end{eqnarray}

   This formula applies if the harp option ``regrid_out_of_bounds`` is set to ``nan`` or to ``extrapolate``.
   If the option is set to ``edge`` then the first and last boundary value are set to the midpoints
   (:math:`\lambda^{B}(1,1) = \lambda(1)`, :math:`\lambda^{B}(N,2) = \lambda(N)`).
