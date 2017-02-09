datetime length derivations
===========================

#. datetime length from start/stop

   ================ ============== ========================== ========================
   symbol           description    unit                       variable name
   ================ ============== ========================== ========================
   :math:`t_{e}`    datetime stop  :math:`s` since 2000-01-01 `datetime_stop {time}`
   :math:`t_{s}`    datetime start :math:`s` since 2000-01-01 `datetime_start {time}`
   :math:`\Delta t` time duration  :math:`s`                  `datetime_length {time}`
   ================ ============== ========================== ========================

   .. math::

      \Delta t = t_{s} - t_{e}
