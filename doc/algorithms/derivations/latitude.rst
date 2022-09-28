latitude derivations
====================

   .. _derivation_latitude_from_polygon:

#. latitude from polygon

   ====================== =========== ============ ========================
   symbol                 description unit         variable name
   ====================== =========== ============ ========================
   :math:`\lambda`        longitude   :math:`degE` `longitude {:}`
   :math:`\lambda^{B}(i)` longitude   :math:`degE` `longitude_bounds {:,N}`
   :math:`\phi`           latitude    :math:`degN` `latitude {:}`
   :math:`\phi^{B}(i)`    latitude    :math:`degN` `latitude_bounds {:,N}`
   ====================== =========== ============ ========================

   The centroid is determined from the normal vector of the polygon area, which is the sum of the area
   weighted moments of consecutive vertices :math:`\mathbf{p}(i)`, :math:`\mathbf{p}(i+1)` for all polygon edges
   (with :math:`\mathbf{p}(N+1):=\mathbf{p}(1)`).

   Convert all polygon corner coordinates defined by :math:`\phi^{B}(i)` and
   :math:`\lambda^{B}(i)` into unit sphere points :math:`\mathbf{p}(i) = [x_{i}, y_{i}, z_{i}]`

   .. math::

      \begin{eqnarray}
        w_{i} & = & \frac{1}{2} \begin{cases}
          \mathbf{p}(i) \cdot \mathbf{p}(i+1) \lt 0, & \pi - 2 asin(\frac{\Vert\mathbf{p}(i) + \mathbf{p}(i+1)\Vert}{2}) \\
          \mathbf{p}(i) \cdot \mathbf{p}(i+1) \ge 0, & 2 asin(\frac{\Vert\mathbf{p}(i) - \mathbf{p}(i+1)\Vert}{2})
        \end{cases} \\
        \mathbf{p}_{center} & = & \sum_{i}{w_{i} \frac{\mathbf{p}(i) \times \mathbf{p}(i+1)}{\Vert\mathbf{p}(i) \times \mathbf{p}(i+1)\Vert}} \\
      \end{eqnarray}

   The vector :math:`\mathbf{p}_{center}` is converted back to :math:`\phi` and :math:`\lambda`

   .. _derivation_latitude_from_range:

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


   .. _derivation_latitude_from_vertical_profile_latitudes:

#. latitude from vertical profile latitudes

   =============== ==================================== ============ =======================
   symbol          description                          unit         variable name
   =============== ==================================== ============ =======================
   :math:`\phi`    single latitude for the full profile :math:`degN` `latitude {:}`
   :math:`\phi(i)` latitude for each profile point      :math:`degN` `latitude {:,vertical}`
   :math:`N`       number of profile points
   =============== ==================================== ============ =======================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \begin{eqnarray}
        N & = & max(i, \phi(i) \neq NaN) \\
        \phi & = & \phi(N/2)
      \end{eqnarray}


   .. _derivation_latitude_from_sensor_latitude:

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
