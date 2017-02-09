latitude derivations
====================

.. _`latitude from polygon derivation`:

#. latitude from polygon

   ====================== =========== ============ ========================
   symbol                 description unit         variable name
   ====================== =========== ============ ========================
   :math:`\lambda`        longitude   :math:`degE` `longitude {:}`
   :math:`\lambda^{B}(i)` longitude   :math:`degE` `longitude_bounds {:,N}`
   :math:`\phi`           latitude    :math:`degN` `latitude {:}`
   :math:`\phi^{B}(i)`    latitude    :math:`degN` `latitude_bounds {:,N}`
   ====================== =========== ============ ========================

   Convert all polygon corner coordinates defined by :math:`\phi^{B}(i)` and
   :math:`\lambda^{B}(i)` into unit sphere points :math:`\mathbf{p}(i) = [x_{i}, y_{i}, z_{i}]`

   :math:`x_{min} = min(x_{i}), y_{min} = min(y_{i}), z_{min} = min(z_{i})`

   :math:`x_{max} = max(x_{i}), y_{max} = max(y_{i}), z_{max} = max(z_{i})`

   :math:`\mathbf{p}_{center} = [\frac{x_{min} + x_{max}}{2}, \frac{y_{min} + y_{max}}{2}, \frac{z_{min} + z_{max}}{2}]`

   The vector :math:`\mathbf{p}_{center}` is converted back to :math:`\phi` and :math:`\lambda`

|

#. latitude from range

   =================== =========================================== ============ =======================
   symbol              description                                 unit         variable name
   =================== =========================================== ============ =======================
   :math:`\phi`        latitude                                    :math:`degN` `latitude {:}`
   :math:`\phi^{B}(l)` latitude boundaries (:math:`l \in \{1,2\}`) :math:`degN` `latitude_bounds {:,2}`
   =================== =========================================== ============ =======================

   The pattern `:` for the dimensions can represent `{latitude}`, or `{time,latitude}`.

   .. math::

      \phi = \frac{\phi^{B}(2) + \phi^{B}(1)}{2}


#. latitude from sensor latitude

   ==================== ====================== ============ =========================
   symbol               description            unit         variable name
   ==================== ====================== ============ =========================
   :math:`\phi`         latitude               :math:`degN` `latitude {:}`
   :math:`\phi_{instr}` latitude of the sensor :math:`degN` `sensor_latitude {:}`
   ==================== ====================== ============ =========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \phi = \phi_{instr}
