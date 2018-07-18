wind velocity derivations
=========================

#. meridional wind velocity from wind speed and direction

   ============== ======================== =========== ================================
   symbol         description              unit        variable name
   ============== ======================== =========== ================================
   :math:`u`      zonal wind velocity      :math:`m/s` `zonal_wind_velocity {:}`
   :math:`s`      wind speed               :math:`m/s` `wind_speed {:}`
   :math:`\theta` wind direction           :math:`deg` `wind_direction {:}`
   ============== ======================== =========== ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
   
      u = s \cos((180 - \theta)\frac{\pi}{180})


#. zonal wind velocity from wind speed and direction

   ============== ======================== =========== ================================
   symbol         description              unit        variable name
   ============== ======================== =========== ================================
   :math:`v`      meridional wind velocity :math:`m/s` `meridional_wind_velocity {:}`
   :math:`s`      wind speed               :math:`m/s` `wind_speed {:}`
   :math:`\theta` wind direction           :math:`deg` `wind_direction {:}`
   ============== ======================== =========== ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
   
      v = s \sin((180 - \theta)\frac{\pi}{180})


#. wind direction from zonal and meridional wind velocity

   ============== ======================== =========== ================================
   symbol         description              unit        variable name
   ============== ======================== =========== ================================
   :math:`u`      zonal wind velocity      :math:`m/s` `zonal_wind_velocity {:}`
   :math:`v`      meridional wind velocity :math:`m/s` `meridional_wind_velocity {:}`
   :math:`\theta` wind direction           :math:`deg` `wind_direction {:}`
   ============== ======================== =========== ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
   
      \theta = 180 - \arctan2(u,v)\frac{180}{\pi}


#. wind speed from zonal and meridional wind velocity

   ============== ======================== =========== ================================
   symbol         description              unit        variable name
   ============== ======================== =========== ================================
   :math:`u`      zonal wind velocity      :math:`m/s` `zonal_wind_velocity {:}`
   :math:`v`      meridional wind velocity :math:`m/s` `meridional_wind_velocity {:}`
   :math:`s`      wind speed               :math:`m/s` `wind_speed {:}`
   ============== ======================== =========== ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
   
      s = \sqrt{u^2 + v^2}
