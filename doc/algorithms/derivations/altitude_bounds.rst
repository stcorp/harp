altitude bounds derivations
===========================

#. altitude ranges from midpoints

   ================== =========================================== ========= ================================
   symbol             description                                 unit      variable name
   ================== =========================================== ========= ================================
   :math:`z(i)`       altitude                                    :math:`m` `altitude {:,vertical}`
   :math:`z^{B}(i,l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m` `altitude_bounds {:,vertical,2}`
   ================== =========================================== ========= ================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         z^{B}(1,1) & = & \frac{3z(1) - z(2)}{2} \\
         z^{B}(i,1) & = & \frac{z(i-1) + z(i)}{2}, 1 < i \leq N \\
         z^{B}(i,2) & = & z^{B}(i+1,1), 1 \leq i < N \\
         z^{B}(N,2) & = & \frac{3z(N) - z(N-1)}{2}
      \end{eqnarray}

   This formula applies if the harp option ``regrid_out_of_bounds`` is set to ``nan`` or to ``extrapolate``.
   If the option is set to ``edge`` then the first and last boundary value are set to the midpoints
   (:math:`z^{B}(1,1) = z(1)`, :math:`z^{B}(N,2) = z(N)`).
