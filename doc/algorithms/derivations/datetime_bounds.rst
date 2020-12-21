datetime bounds derivations
===========================

#. datetime ranges from start and stop

   ================ =========================================== ========================== =======================
   symbol           description                                 unit                       variable name
   ================ =========================================== ========================== =======================
   :math:`t^{B}(l)` datetime boundaries (:math:`l \in \{1,2\}`) :math:`s` since 2000-01-01 `datetime_bounds {:,2}`
   :math:`t_{e}`    datetime stop                               :math:`s` since 2000-01-01 `datetime_stop {:}`
   :math:`t_{s}`    datetime start                              :math:`s` since 2000-01-01 `datetime_start {:}`
   ================ =========================================== ========================== =======================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         \phi^{B}(1) & = & t_{s} \\
         \phi^{B}(2) & = & t_{e}
      \end{eqnarray}
