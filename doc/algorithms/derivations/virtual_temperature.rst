virtual temperature derivations
===============================

   .. _derivation_virtual_temperature_from_temperature:

#. virtual temperature from temperature

   ==================== ======================= ===================== =========================
   symbol               description             unit                  variable name
   ==================== ======================= ===================== =========================
   :math:`M_{air}`      molar mass of total air :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{dry\_air}` molar mass of dry air   :math:`\frac{g}{mol}`
   :math:`T`            temperature             :math:`K`             `temperature {:}`
   :math:`T_{v}`        virtual temperature     :math:`K`             `virtual_temperature {:}`
   ==================== ======================= ===================== =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      T_{v} = \frac{M_{dry\_air}}{M_{air}}T
