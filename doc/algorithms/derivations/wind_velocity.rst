wind velocity derivations
=========================

   .. _derivation_meridional_wind_velocity_from_wind_speed_and_direction:

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


   .. _derivation_zonal_wind_velocity_from_wind_speed_and_direction:

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


   .. _derivation_wind_direction_from_zonal_and_meridional_wind_velocity:

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


   .. _derivation_wind_speed_from_zonal_and_meridional_wind_velocity:

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
