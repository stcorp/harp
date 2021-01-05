datetime start derivations
==========================

   .. _derivation_datetime_start_from_datetime_and_stop:

#. datetime start from datetime and stop

   ================ ==================== ========================== =====================
   symbol           description          unit                       variable name
   ================ ==================== ========================== =====================
   :math:`t`        datetime (mid point) :math:`s` since 2000-01-01 `datetime {:}`
   :math:`t_{s}`    datetime start       :math:`s` since 2000-01-01 `datetime_start {:}`
   :math:`\Delta t` time duration        :math:`s`                  `datetime_length {:}`
   ================ ==================== ========================== =====================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      t_{s} = t - \frac{\Delta t}{2}


   .. _derivation_datetime_start_from_range:

#. datetime start from range

   ================ =========================================== ========================== =======================
   symbol           description                                 unit                       variable name
   ================ =========================================== ========================== =======================
   :math:`t^{B}(l)` datetime boundaries (:math:`l \in \{1,2\}`) :math:`s` since 2000-01-01 `datetime_bounds {:,2}`
   :math:`t_{s}`    datetime start                              :math:`s` since 2000-01-01 `datetime_start {:}`
   ================ =========================================== ========================== =======================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      t_{s} = t^{B}(1)
