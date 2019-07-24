Common formula
==============

gravity
-------

#. Newton's gravitational law

   ============= ============================ ======================
   symbol        description                  unit
   ============= ============================ ======================
   :math:`g`     normal gravity at sea level  :math:`\frac{m}{s^2}`
   :math:`g_{h}` gravity at specific height   :math:`\frac{m}{s^2}`
   :math:`z`     altitude                     :math:`m`
   :math:`R`     local earth curvature radius :math:`m`
   ============= ============================ ======================

   .. math::

      g_{h} = g\left(\frac{R}{R + z}\right)^2


#. Normal gravity at sea level ellipsoid

   This is the WGS84 ellipsoidal gravity formula as taken from NIMA TR8350.2

   ============= =========================== =====================
   symbol        name                        unit
   ============= =========================== =====================
   :math:`a`     WGS84 semi-major axis       :math:`m`
   :math:`b`     WGS84 semi-minor axis       :math:`m`
   :math:`e`     eccentricity                :math:`m`
   :math:`g`     normal gravity at sea level :math:`\frac{m}{s^2}`
   :math:`g_{e}` gravity at equator          :math:`\frac{m}{s^2}`
   :math:`g_{p}` gravity at poles            :math:`\frac{m}{s^2}`
   :math:`\phi`  latitude                    :math:`degN`
   ============= =========================== =====================

   .. math::
      :nowrap:

      \begin{eqnarray}
         e^2 & = & \frac{a^2-b^2}{a^2} \\
         k & = & \frac{bg_{p} - ag_{e}}{ag_{e}} \\
         g & = & g_{e}\frac{1 + k {\sin}^2(\frac{\pi}{180}\phi)}{\sqrt{1 - e^2{\sin}^2(\frac{\pi}{180}\phi)}} \\
         g & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}}
      \end{eqnarray}


#. Gravity at specific altitude

   This is the WGS84 ellipsoidal gravity formula as taken from NIMA TR8350.2

   ============== ==================================== =======================
   symbol         name                                 unit
   ============== ==================================== =======================
   :math:`a`      WGS84 semi-major axis                :math:`m`
   :math:`b`      WGS84 semi-minor axis                :math:`m`
   :math:`f`      WGS84 flattening                     :math:`m`
   :math:`g`      normal gravity at sea level          :math:`\frac{m}{s^2}`
   :math:`g_{h}`  gravity at specific height           :math:`\frac{m}{s^2}`
   :math:`GM`     WGS84 earth's gravitational constant :math:`\frac{m^3}{s^2}`
   :math:`z`      altitude                             :math:`m`
   :math:`\phi`   latitude                             :math:`degN`
   :math:`\omega` WGS84 earth angular velocity         :math:`rad/s`
   ============== ==================================== =======================

   The formula used is the one based on the truncated Taylor series expansion:

   .. math::
      :nowrap:

      \begin{eqnarray}
         m & = & \frac{\omega^2a^2b}{GM} \\
         g_{h} & = & g \left[ 1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2 \right] \\
      \end{eqnarray}


geopotential height
-------------------

   ============= ============================ ======================
   symbol        description                  unit
   ============= ============================ ======================
   :math:`g`     normal gravity at sea level  :math:`\frac{m}{s^2}`
   :math:`g_{0}` mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{h}` gravity at specific height   :math:`\frac{m}{s^2}`
   :math:`p`     pressure                     :math:`Pa`
   :math:`R`     local earth curvature radius :math:`m`
   :math:`z`     altitude                     :math:`m`
   :math:`z_{g}` geopotential height          :math:`m`
   :math:`\phi`  latitude                     :math:`degN`
   :math:`\rho`  mass density                 :math:`\frac{kg}{m^3}`
   ============= ============================ ======================

   The geopotential height allows the gravity in the hydrostatic equation

   .. math::

      dp = - \rho g_{h} dz

   to be replaced by a constant gravity

   .. math::

      dp = - \rho g_{0} dz_{g}

   providing

   .. math::

      dz_{g} = \frac{g_{h}}{g_{0}}dz

   With Newton's gravitational law this becomes

   .. math::

      dz_{g} = \frac{g}{g_{0}}\left(\frac{R}{R + z}\right)^2dz

   And integrating this, considering that :math:`z=0` and :math:`z_{g}=0` at sea level, results in

   .. math::

      z_{g} = \frac{g}{g_{0}}\frac{Rz}{R + z}

   .. math::

      z = \frac{g_{0}Rz_{g}}{gR-g_{0}z_{g}}


gas constant
------------

   =========== ====================== ================================
   symbol      name                   unit
   =========== ====================== ================================
   :math:`k`   Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`N_A` Avogadro constant      :math:`\frac{1}{mol}`
   :math:`R`   universal gas constant :math:`\frac{kg m^2}{K mol s^2}`
   =========== ====================== ================================

   Relation between Boltzmann constant, universal gas constant, and Avogadro constant:

   .. math::

      k = \frac{R}{N_A}


ideal gas law
-------------

   ========= ====================== ================================
   symbol    name                   unit
   ========= ====================== ================================
   :math:`k` Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`N` amount of substance    :math:`molec`
   :math:`p` pressure               :math:`Pa`
   :math:`R` universal gas constant :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T` temperature            :math:`K`
   :math:`V` volume                 :math:`m^3`
   ========= ====================== ================================

   .. math::

       pV = \frac{NRT}{N_{A}} = NkT


barometric formula
------------------

   =============== ========================== ================================
   symbol          name                       unit
   =============== ========================== ================================
   :math:`g_{0}`   mean earth gravity         :math:`\frac{m}{s^2}`
   :math:`g_{h}`   gravity at specific height :math:`\frac{m}{s^2}`
   :math:`k`       Boltzmann constant         :math:`\frac{kg m^2}{K s^2}`
   :math:`M_{air}` molar mass of total air    :math:`\frac{g}{mol}`
   :math:`N`       amount of substance        :math:`molec`
   :math:`N_A`     Avogadro constant          :math:`\frac{1}{mol}`
   :math:`p`       pressure                   :math:`Pa`
   :math:`R`       universal gas constant     :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T`       temperature                :math:`K`
   :math:`V`       volume                     :math:`m^3`
   :math:`z`       altitude                   :math:`m`
   :math:`z_{g}`   geopotential height        :math:`m`
   :math:`\phi`    latitude                   :math:`degN`
   :math:`\rho`    mass density               :math:`\frac{kg}{m^3}`
   =============== ========================== ================================

   From the ideal gas law we have:

   .. math::

      p = \frac{NkT}{V} = \frac{10^{-3}NM_{air}}{VN_{a}}\frac{kTN_{a}}{10^{-3}M_{air}} = \rho\frac{RT}{10^{-3}M_{air}}

   And from the hydrostatic assumption we get:

   .. math::

      dp = - \rho g_{h} dz

   Dividing :math:`dp` by `p` we get:

   .. math::

      \frac{dp}{p} = -\frac{10^{-3}M_{air}\rho g_{h} dz}{\rho RT} = -\frac{10^{-3}M_{air}g_{h}dz}{RT}

   Integrating this expression from one pressure level to the next we get:

   .. math::

      p(i+1) = p(i)e^{-\int^{z(i+1)}_{z(i)}\frac{10^{-3}M_{air}g_{h}}{RT}dz}

   We can approximate this further by using an average value of the height dependent quantities
   :math:`M_{air}`, :math:`g_{h}` and :math:`T` for the integration over the range :math:`[z(i),z(i+1)]`.
   This gives:

   .. math::
      :nowrap:

      \begin{eqnarray}
         g' & = & g_{h}(\phi,\frac{z(i)+z(i+1)}{2}) \\
         p(i+1) & = & p(i)e^{-10^{-3}\frac{M_{air}(i)+M_{air}(i+1)}{2}\frac{2}{T(i)+T(i+1)}\frac{g'}{R}\left(z(i+1)-z(i)\right)} \\
                & = & p(i)e^{-10^{-3}\frac{M_{air}(i)+M_{air}(i+1)}{T(i)+T(i+1)}\frac{g'}{R}\left(z(i+1)-z(i)\right)}
      \end{eqnarray}

   When using geopotential height the formula is the same except that :math:`g=g_{0}` at all levels:

   .. math::

       p(i+1) = p(i)e^{-10^{-3}\frac{M_{air}(i)+M_{air}(i+1)}{T(i)+T(i+1)}\frac{g_{0}}{R}\left(z_{g}(i+1)-z_{g}(i)\right)}


mass density
------------

   =============== ======================= ======================
   symbol          name                    unit
   =============== ======================= ======================
   :math:`N`       amount of substance     :math:`molec`
   :math:`N_A`     Avogadro constant       :math:`\frac{1}{mol}`
   :math:`M_{air}` molar mass of total air :math:`\frac{g}{mol}`
   :math:`V`       volume                  :math:`m^3`
   :math:`\rho`    mass density            :math:`\frac{kg}{m^3}`
   =============== ======================= ======================

   .. math::

      \rho = \frac{10^{-3}NM_{air}}{VN_{a}}


number density
--------------

   ========= =================== =========================
   symbol    name                unit
   ========= =================== =========================
   :math:`n` number density      :math:`\frac{molec}{m^3}`
   :math:`N` amount of substance :math:`molec`
   :math:`V` volume              :math:`m^3`
   ========= =================== =========================

   .. math::

      n = \frac{N}{V}


dry air vs. total air
---------------------

   ======================= =========================== =========================
   symbol                  name                        unit
   ======================= =========================== =========================
   :math:`n`               number density of total air :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}`    number density of dry air   :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`      number density of H2O       :math:`\frac{molec}{m^3}`
   :math:`M_{air}`         molar mass of total air     :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}`    molar mass of dry air       :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`      molar mass of H2O           :math:`\frac{g}{mol}`
   :math:`\rho`            mass density of total air   :math:`\frac{kg}{m^3}`
   :math:`\rho_{dry\_air}` mass density of dry air     :math:`\frac{kg}{m^3}`
   :math:`\rho_{H_{2}O}`   mass density of H2O         :math:`\frac{kg}{m^3}`
   ======================= =========================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         n & = & n_{dry\_air} + n_{H_{2}O} \\
         M_{air}n & = & M_{dry\_air}n_{dry\_air} + M_{H_{2}O}n_{H_{2}O} \\
         \rho & = & \rho_{dry\_air} + \rho_{H_{2}O} \\
      \end{eqnarray}


virtual temperature
-------------------

   ==================== ======================== ================================
   symbol               name                     unit
   ==================== ======================== ================================
   :math:`k`            Boltzmann constant       :math:`\frac{kg m^2}{K s^2}`
   :math:`M_{air}`      molar mass of total air  :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air    :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O        :math:`\frac{g}{mol}`
   :math:`N`            amount of substance      :math:`molec`
   :math:`N_A`          Avogadro constant        :math:`\frac{1}{mol}`
   :math:`p`            pressure                 :math:`Pa`
   :math:`p_{dry\_air}` dry air partial pressure :math:`Pa`
   :math:`p_{H_{2}O}`   H2O partial pressure     :math:`Pa`
   :math:`R`            universal gas constant   :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T`            temperature              :math:`K`
   :math:`T_{v}`        virtual temperature      :math:`K`
   :math:`V`            volume                   :math:`m^3`
   ==================== ======================== ================================

   From the ideal gas law we have:

   .. math::

      p = \frac{NkT}{V} = \frac{10^{-3}NM_{air}}{VN_{a}}\frac{kTN_{a}}{10^{-3}M_{air}} = \rho \frac{RT}{10^{-3}M_{air}}

   The virtual temperature allows us to use the dry air molar mass in this equation:

   .. math::

      p = \rho\frac{RT_{v}}{10^{-3}M_{dry\_air}}

   This gives:

   .. math::

      T_{v} = \frac{M_{dry\_air}}{M_{air}}T


volume mixing ratio
-------------------

   ===================== =============================== =========================
   symbol                name                            unit
   ===================== =============================== =========================
   :math:`n`             number density of total air     :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}`  number density of dry air       :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`    number density of H2O           :math:`\frac{molec}{m^3}`
   :math:`n_{x}`         number density of quantity x    :math:`\frac{molec}{m^3}`
   :math:`\nu_{x}`       volume mixing ratio of quantity :math:`ppv`
                         x with regard to total air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity :math:`ppv`
                         x with regard to dry air
   ===================== =============================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         \nu_{x} & = & \frac{n_{x}}{n} \\
         \bar{\nu}_{x} & = & \frac{n_{x}}{n_{dry\_air}} \\
         \nu_{dry\_air} & = & \frac{n_{dry\_air}}{n} =
            \frac{n - n_{H_{2}O}}{n} = 1 - \nu_{H_{2}O} \\
         \nu_{air} & = & \frac{n}{n} = 1 \\
         \bar{\nu}_{dry\_air} & = & \frac{n_{dry\_air}}{n_{dry\_air}} = 1 \\
         \bar{\nu}_{H_{2}O} & = & \frac{n_{H_{2}O}}{n_{dry\_air}} =
            \frac{\nu_{H_{2}O}}{\nu_{dry\_air}} = \frac{\nu_{H_{2}O}}{1 - \nu_{H_{2}O}} \\
         \nu_{H_{2}O} & = & \frac{\bar{\nu}_{H_{2}O}}{1 + \bar{\nu}_{H_{2}O}}
      \end{eqnarray}


mass mixing ratio
-----------------

   ===================== =============================== =========================
   symbol                name                            unit
   ===================== =============================== =========================
   :math:`M_{air}`       molar mass of total air         :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}`  molar mass of dry air           :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass of quantity x        :math:`\frac{g}{mol}`
   :math:`n`             number density of total air     :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}`  number density of dry air       :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`    number density of H2O           :math:`\frac{molec}{m^3}`
   :math:`n_{x}`         number density of quantity x    :math:`\frac{molec}{m^3}`
   :math:`q_{x}`         mass mixing ratio of quantity x :math:`\frac{kg}{kg}`
                         with regard to total air
   :math:`\bar{q}_{x}`   mass mixing ratio of quantity x :math:`\frac{kg}{kg}`
                         with regard to dry air
   :math:`\nu_{x}`       volume mixing ratio of quantity :math:`ppv`
                         x with regard to total air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity :math:`ppv`
                         x with regard to dry air
   ===================== =============================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         q_{x} & = & \frac{n_{x}M_{x}}{nM_{air}} = \nu_{x}\frac{M_{x}}{M_{air}} \\
         \bar{q}_{x} & = & \frac{n_{x}M_{x}}{n_{dry\_air}M_{dry\_air}} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}} \\
         q_{dry\_air} & = & \frac{n_{dry\_air}M_{dry\_air}}{nM_{air}} =
            \frac{nM_{air} - n_{H_{2}O}M_{H_{2}O}}{nM_{air}} = 1 - q_{H_{2}O} \\
         q_{air} & = & \frac{nM_{air}}{nM_{air}} = 1 \\
         \bar{q}_{dry\_air} & = & \frac{n_{dry\_air}M_{dry\_air}}{n_{dry\_air}M_{dry\_air}} = 1 \\
         \bar{q}_{H_{2}O} & = & \frac{n_{H_{2}O}M_{H_{2}O}}{n_{dry\_air}M_{dry\_air}} =
            \frac{q_{H_{2}O}}{q_{dry\_air}} = \frac{q_{H_{2}O}}{1 - q_{H_{2}O}} \\
         q_{H_{2}O} & = & \frac{\bar{q}_{H_{2}O}}{1 + \bar{q}_{H_{2}O}}
      \end{eqnarray}


molar mass of total air
-----------------------

#. molar mass of total air from H2O volume mixing ratio

   ==================== =========================== =========================
   symbol               name                        unit
   ==================== =========================== =========================
   :math:`M_{air}`      molar mass of total air     :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air       :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O           :math:`\frac{g}{mol}`
   :math:`n`            number density of total air :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}` number density of dry air   :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`   number density of H2O       :math:`\frac{molec}{m^3}`
   :math:`\nu_{H_{2}O}` volume mixing ratio of H2O  :math:`ppv`
   ==================== =========================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         M_{air} & = & \frac{M_{dry\_air}n_{dry\_air} + M_{H_{2}O}n_{H_{2}O}}{n} \\
                 & = & M_{dry\_air}\left(1 - \nu_{H_{2}O}\right) + M_{H_{2}O}\nu_{H_{2}O}
      \end{eqnarray}


#. molar mass of total air from H2O mass mixing ratio

   ==================== =========================== =========================
   symbol               name                        unit
   ==================== =========================== =========================
   :math:`M_{air}`      molar mass of total air     :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air       :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O           :math:`\frac{g}{mol}`
   :math:`n`            number density of total air :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}` number density of dry air   :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`   number density of H2O       :math:`\frac{molec}{m^3}`
   :math:`q_{H_{2}O}`   mass mixing ratio of H2O    :math:`\frac{kg}{kg}`
   :math:`\nu_{H_{2}O}` volume mixing ratio of H2O  :math:`\frac{kg}{kg}`
   ==================== =========================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         M_{air} & = & M_{dry\_air}\left(1 - \nu_{H_{2}O}\right) + M_{H_{2}O}\nu_{H_{2}O} \\
                 & = & M_{dry\_air}\left(1 - \frac{M_{air}}{M_{H_{2}O}}q_{H_{2}O}\right) + M_{air}q_{H_{2}O} \\
                 & = & \frac{M_{dry\_air}}{1 + \frac{M_{dry\_air}}{M_{H_{2}O}}q_{H_{2}O} - q_{H_{2}O}} \\
                 & = & \frac{M_{H_{2}O}M_{dry\_air}}{M_{H_{2}O} + M_{dry\_air}q_{H_{2}O} - M_{H_{2}O}q_{H_{2}O}} \\
                 & = & \frac{M_{H_{2}O}M_{dry\_air}}{\left(1-q_{H_{2}O}\right)M_{H_{2}O} + q_{H_{2}O}M_{dry\_air}} \\
      \end{eqnarray}


partial pressure
----------------

   ===================== =============================== ===========
   symbol                name                            unit
   ===================== =============================== ===========
   :math:`p`             total pressure                  :math:`Pa`
   :math:`p_{x}`         partial pressure of quantity    :math:`Pa`
   :math:`\nu_{x}`       volume mixing ratio of quantity :math:`ppv`
                         x with regard to total air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity :math:`ppv`
                         x with regard to dry air
   ===================== =============================== ===========

   .. math::
      :nowrap:

      \begin{eqnarray}
         p_{x} & = & \nu_{x}p \\
         p_{x} & = & \bar{\nu}_{x}p_{dry\_air} \\
         p_{x} & = & N_{x}kT
      \end{eqnarray}


saturated water vapor pressure
------------------------------

   ============= =============================== ===========
   symbol        name                            unit
   ============= =============================== ===========
   :math:`e_{w}` saturated water vapor pressure  :math:`Pa`
   :math:`T`     temperature                     :math:`K`
   ============= =============================== ===========

   This is the August-Roche-Magnus formula for the saturated water vapour pressure

   .. math::

      e_{w} = 610.94e^{\frac{17.625(T-273.15)}{(T-273.15)+243.04}}


