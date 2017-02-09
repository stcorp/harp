latitude bounds derivations
===========================

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
         \phi^{B}(1,1) & = & 2\phi(1) - \phi(2) \\
         \phi^{B}(i,1) & = & \frac{\phi(i-1) + \phi(i)}{2}, 1 < i \leq N \\
         \phi^{B}(i,2) & = & \phi^{B}(i+1,1), 1 \leq i < N \\
         \phi^{B}(N,2) & = & 2\phi(N) - \phi(N-1)
      \end{eqnarray}
