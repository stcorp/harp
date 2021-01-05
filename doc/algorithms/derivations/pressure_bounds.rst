pressure bounds derivations
===========================

   .. _derivation_pressure_ranges_from_midpoints:

#. pressure ranges from midpoints

   ================== =========================================== ========== ================================
   symbol             description                                 unit        variable name
   ================== =========================================== ========== ================================
   :math:`p(i)`       pressure                                    :math:`Pa` `pressure {:,vertical}`
   :math:`p^{B}(i,l)` pressure boundaries (:math:`l \in \{1,2\}`) :math:`Pa` `pressure_bounds {:,vertical,2}`
   ================== =========================================== ========== ================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         p^{B}(1,1) & = & e^{\frac{3\ln(p(1)) - \ln(p(2))}{2}} \\
         p^{B}(i,1) & = & e^{\frac{\ln(p(i-1)) + \ln(p(i))}{2}}, 1 < i \leq N \\
         p^{B}(i,2) & = & p^{B}(i+1,1), 1 \leq i < N \\
         p^{B}(N,2) & = & e^{\frac{3\ln(p(N)) - \ln(p(N-1))}{2}}
      \end{eqnarray}

   This formula applies if the harp option ``regrid_out_of_bounds`` is set to ``nan`` or to ``extrapolate``.
   If the option is set to ``edge`` then the first and last boundary value are set to the midpoints
   (:math:`p^{B}(1,1) = p(1)`, :math:`p^{B}(N,2) = p(N)`).
