datetime length derivations
===========================

   .. _derivation_datetime_length_from_start_stop:

#. datetime length from start/stop

   ================ ============== ========================== =====================
   symbol           description    unit                       variable name
   ================ ============== ========================== =====================
   :math:`t_{e}`    datetime stop  :math:`s` since 2000-01-01 `datetime_stop {:}`
   :math:`t_{s}`    datetime start :math:`s` since 2000-01-01 `datetime_start {:}`
   :math:`\Delta t` time duration  :math:`s`                  `datetime_length {:}`
   ================ ============== ========================== =====================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      \Delta t = t_{s} - t_{e}
