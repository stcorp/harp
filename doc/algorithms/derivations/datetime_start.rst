datetime start derivations
==========================

#. datetime start from datetime and stop

   ================ ==================== ========================== ========================
   symbol           description          unit                       variable name
   ================ ==================== ========================== ========================
   :math:`t`        datetime (mid point) :math:`s` since 2000-01-01 `datetime {time}`
   :math:`t_{s}`    datetime start       :math:`s` since 2000-01-01 `datetime_start {time}`
   :math:`\Delta t` time duration        :math:`s`                  `datetime_length {time}`
   ================ ==================== ========================== ========================

   .. math::

      t_{s} = t - \frac{\Delta t}{2}
