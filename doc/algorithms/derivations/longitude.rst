longitude derivations
=====================

#. longitude from polygon

   See :ref:`latitude from polygon <latitude from polygon derivation>`

|

#. longitude from range

   ====================== ============================================ ============ ========================
   symbol                 description                                  unit         variable name
   ====================== ============================================ ============ ========================
   :math:`\lambda`        longitude                                    :math:`degE` `longitude {:}`
   :math:`\lambda^{B}(l)` longitude boundaries (:math:`l \in \{1,2\}`) :math:`degE` `longitude_bounds {:,2}`
   ====================== ============================================ ============ ========================

   The pattern `:` for the dimensions can represent `{longitude}`, or `{time,longitude}`.

   .. math::

      \lambda = \frac{\lambda^{B}(2) + \lambda^{B}(1)}{2}


#. longitude from vertical profile latitudes

   ================== ===================================== ============ ========================
   symbol             description                           unit         variable name
   ================== ===================================== ============ ========================
   :math:`\lambda`    single longitude for the full profile :math:`degE` `longitude {:}`
   :math:`\lambda(i)` longitude for each profile point      :math:`degE` `longitude {:,vertical}`
   :math:`N`          number of profile points
   ================== ===================================== ============ ========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
        N & = & max(i, \lambda(i) \neq NaN) \\
        \lambda & = & \lambda(N/2)
      \end{eqnarray}


#. longitude from sensor longitude

   ======================= ======================= ============ ==========================
   symbol                  description             unit         variable name
   ======================= ======================= ============ ==========================
   :math:`\lambda`         longitude               :math:`degE` `longitude {:}`
   :math:`\lambda_{instr}` longitude of the sensor :math:`degE` `sensor_longitude {:}`
   ======================= ======================= ============ ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \lambda = \lambda_{instr}
