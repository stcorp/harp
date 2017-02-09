datetime derivations
====================

#. datetime from start/stop

   ============= ==================== ========================== =======================
   symbol        description          unit                       variable name
   ============= ==================== ========================== =======================
   :math:`t`     datetime (mid point) :math:`s` since 2000-01-01 `datetime {time}`
   :math:`t_{e}` datetime stop        :math:`s` since 2000-01-01 `datetime_stop {time}`
   :math:`t_{s}` datetime start       :math:`s` since 2000-01-01 `datetime_start {time}`
   ============= ==================== ========================== =======================

   .. math::

      t = \frac{t_{s} - t_{e}}{2}
