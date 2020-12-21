datetime derivations
====================

#. datetime from start/stop

   ============= ==================== ========================== ====================
   symbol        description          unit                       variable name
   ============= ==================== ========================== ====================
   :math:`t`     datetime (mid point) :math:`s` since 2000-01-01 `datetime {:}`
   :math:`t_{e}` datetime stop        :math:`s` since 2000-01-01 `datetime_stop {:}`
   :math:`t_{s}` datetime start       :math:`s` since 2000-01-01 `datetime_start {:}`
   ============= ==================== ========================== ====================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      t = \frac{t_{s} + t_{e}}{2}


#. datetime from range

   ================ =========================================== ========================== =======================
   symbol           description                                 unit                       variable name
   ================ =========================================== ========================== =======================
   :math:`t`        datetime (mid point)                        :math:`s` since 2000-01-01 `datetime {:}`
   :math:`t^{B}(l)` datetime boundaries (:math:`l \in \{1,2\}`) :math:`s` since 2000-01-01 `datetime_bounds {:,2}`
   ================ =========================================== ========================== =======================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      t = \frac{t^{B}(1) + t^{B}(2)}{2}
