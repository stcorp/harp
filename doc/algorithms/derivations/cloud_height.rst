cloud height derivations
========================

   .. _derivation_cloud_height_from_cloud_base_top_height:

#. cloud height from cloud base/top height

   ============== ======================== ========= ==========================
   symbol         description              unit      variable name
   ============== ======================== ========= ==========================
   :math:`z_{c}`  cloud height (mid point) :math:`m` `cloud_height {:}`
   :math:`z_{cb}` cloud base height        :math:`m` `cloud_base_height {:}`
   :math:`z_{ct}` cloud top height         :math:`m` `cloud_top_height {:}`
   ============== ======================== ========= ==========================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      z_{c} = \frac{z_{ct} - z_{cb}}{2}
