pressure bounds derivations
===========================

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
         p^{B}(1,1) & = & e^{2\ln(p(1)) - \ln(p(2))} \\
         p^{B}(i,1) & = & e^{\frac{\ln(p(i-1)) + \ln(p(i))}{2}}, 1 < i \leq N \\
         p^{B}(i,2) & = & e^{\ln(p^{B}(i+1,1))}, 1 \leq i < N \\
         p^{B}(N,2) & = & e^{2\ln(p(N)) - \ln(p(N-1))}
      \end{eqnarray}
