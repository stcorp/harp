relative humidity derivations
=============================

   .. _derivation_relative_humidity_from_H2O_partial_pressure:

#. relative humidity from H2O partial pressure

   ================== ============================== ===================== ==========================
   symbol             description                    unit                  variable name
   ================== ============================== ===================== ==========================
   :math:`e_{w}`      saturated water vapor pressure :math:`Pa`
   :math:`p_{H_{2}O}` partial pressure of H2O        :math:`Pa`            `H2O_partial_pressure {:}`
   :math:`T`          temperature                    :math:`K`             `temperature {:}`
   :math:`\phi`       relative humidity              :math:`\frac{Pa}{Pa}` `relative_humidity {:}`
   ================== ============================== ===================== ==========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         e_{w} & = & 610.94e^{\frac{17.625(T-273.15)}{(T-273.15)+243.04}} \\
         \phi & = & \frac{p_{H_{2}O}}{e_{w}}
      \end{eqnarray}
