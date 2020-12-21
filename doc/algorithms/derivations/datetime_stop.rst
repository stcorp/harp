datetime stop derivations
=========================

#. datetime stop from start and length

   ================ ============== ========================== =====================
   symbol           description    unit                       variable name
   ================ ============== ========================== =====================
   :math:`t_{e}`    datetime stop  :math:`s` since 2000-01-01 `datetime_stop {:}`
   :math:`t_{s}`    datetime start :math:`s` since 2000-01-01 `datetime_start {:}`
   :math:`\Delta t` time duration  :math:`s`                  `datetime_length {:}`
   ================ ============== ========================== =====================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      t_{e} = t_{s} + \Delta t


#. datetime stop from datetime_bounds

   ================ =========================================== ========================== =======================
   symbol           description                                 unit                       variable name
   ================ =========================================== ========================== =======================
   :math:`t^{B}(l)` datetime boundaries (:math:`l \in \{1,2\}`) :math:`s` since 2000-01-01 `datetime_bounds {:,2}`
   :math:`t_{e}`    datetime stop                               :math:`s` since 2000-01-01 `datetime_stop {:}`
   ================ =========================================== ========================== =======================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::

      t_{e} = t^{B}(2)
