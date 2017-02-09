temperature derivations
=======================

#. temperature from pressure and number density

   ========= ================== ============================ ====================
   symbol    description        unit                         variable name
   ========= ================== ============================ ====================
   :math:`k` Boltzmann constant :math:`\frac{kg m^2}{K s^2}`
   :math:`n` number density     :math:`\frac{molec}{m^3}`    `number_density {:}`
   :math:`p` pressure           :math:`Pa`                   `pressure {:}`
   :math:`T` temperature        :math:`K`                    `temperature {:}`
   ========= ================== ============================ ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      T = \frac{p}{kn}


#. temperature from virtual temperature

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

      T = \frac{M_{air}}{M_{dry\_air}}T_{v}


#. surface temperature from surface pressure and surface number density

   ================ ====================== ============================ ============================
   symbol           description            unit                         variable name
   ================ ====================== ============================ ============================
   :math:`k`        Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{surf}` surface number density :math:`\frac{molec}{m^3}`    `surface_number_density {:}`
   :math:`p_{surf}` surface pressure       :math:`Pa`                   `surface_pressure {:}`
   :math:`T_{surf}` surface temperature    :math:`K`                    `surface_temperature {:}`
   ================ ====================== ============================ ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      T_{surf} = \frac{p_{surf}}{kn_{surf}}
