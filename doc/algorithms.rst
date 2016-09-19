Algorithms
==========

Note that all algorithms here are defined in their discretized form (to reflect the implementation in source code).

Definitions
-----------

Leading dimensions
~~~~~~~~~~~~~~~~~~

Many algorithms can be applied exactly the same to a variable even though it may have different dimension dependencies.
For instance, a density conversion can be the same algorithm for either `density {}`, `density {time}`,
`density {latitude,longitude}`, `density {time,vertical}`, etc.
The algorithm is just applied element-wise for each element in the dimensions that `density` depends on.
Such leading dimensions that can be handled element-wise are captured by a ':' in the variable reference in the
definitions below.
Any dimensions that are significant for the conversion (for instance, the `vertical` dimension when integrating a
vertical profile to a total column) will still be mentioned explicitly and will map to an index in the symbol used for
the quantity (e.g. :math:`\nu(:,i)`). If an algorithm has variables with a ':' in the dimension specification then the
algorithm will contain a description of which combination of dimensions are supported for ':'.


Constants
~~~~~~~~~

=================== ============================ ================================ ===============================
symbol              name                         unit                             value
=================== ============================ ================================ ===============================
:math:`a`           WGS84 semi-major axis        :math:`m`                        :math:`6378137.0`
:math:`b`           WGS84 semi-minor axis        :math:`m`                        :math:`6356752.314245`
:math:`c`           speed of light               :math:`\frac{m}{s}`              :math:`2.99792458\cdot10^{8}`
:math:`\frac{1}{f}` WGS84 inverse flatting                                        :math:`298.257223563`
:math:`g_{0}`       mean earth gravity           :math:`\frac{m}{s^2}`            :math:`9.80665`
:math:`g_{e}`       earth gravity at equator     :math:`\frac{m}{s^2}`            :math:`9.7803253359`
:math:`g_{p}`       earth gravity at poles       :math:`\frac{m}{s^2}`            :math:`9.8321849378`
:math:`GM`          WGS84 earth's gravitational  :math:`\frac{m^3}{s^2}`          :math:`3986004.418\cdot10^{8}`
                    constant
:math:`k`           Boltzmann constant           :math:`\frac{kg m^2}{K s^2}`     :math:`1.38064852\cdot10^{-23}`
:math:`N_A`         Avogadro constant            :math:`\frac{1}{mol}`            :math:`6.022140857\cdot10^{23}`
:math:`R`           universal gas constant       :math:`\frac{kg m^2}{K mol s^2}` :math:`8.3144598`
:math:`\omega`      WGS84 earth angular velocity :math:`rad/s`                    :math:`7292115.0\cdot10^{-11}`
=================== ============================ ================================ ===============================


Molar mass
~~~~~~~~~~

The following table provides for each species the molar mass :math:`M_{x}` in :math:`\frac{g}{mol}`.

See the documentation on the HARP data format for a description of all species.

======= =================================================
name    molar mass
======= =================================================
dry air 28.9644
BrO     95.9034
BrO2    111.9028
CCl2F2  120.9135
CCl3F   137.3681
CF4     88.00431
CHClF2  86.4684
CH3Cl   50.4875
CH4     16.0425
CO      28.0101
COF2    66.0069
COS     60.0751
CO2     44.0095
C2H2    26.0373
C2H2O2  58.036163
C2H6    30.0690
C2H3NO5 121.04892
C3H8    44.09562
C5H8    68.11702
ClNO3   97.4579
ClO     51.4524
HCHO    30.026
HCOOH   46.0254
HCN     27.0253
HCl     36.4609
HF      20.006343
HNO2    47.013494
HNO3    63.0129
HNO4    79.0122
HOCl    52.4603
HO2     33.00674
H2O     18.0153
H2O_161 1.00782503207 + 15.99491461956 + 1.00782503207
H2O_162 1.00782503207 + 15.99491461956 + 2.0141017778
H2O_171 1.00782503207 + 16.99913170 + 1.00782503207
H2O_181 1.00782503207 + 17.9991610 + 1.00782503207
H2O2    34.01468
IO      142.903873
NO      30.00610
NOCl    65.4591
NO2     46.00550
NO3     62.0049
N2      28.01340
N2O     44.0129
N2O5    108.0104
OClO    67.4518
OH      17.00734
O2      32.000
O3      47.99820
O3_666  15.99491461956 + 15.99491461956 + 15.99491461956
O3_667  15.99491461956 + 15.99491461956 + 16.99913170
O3_668  15.99491461956 + 15.99491461956 + 17.9991610
O3_686  15.99491461956 + 17.9991610 + 15.99491461956
O4      63.9976
SF6     146.0554
SO2     64.0638
======= =================================================


Common formula
--------------

gravity
~~~~~~~

#. Newton's gravitational law

   ================ ============================ ======================
   symbol           description                  unit
   ================ ============================ ======================
   :math:`g`        gravity                      :math:`\frac{m}{s^2}`
   :math:`g_{surf}` gravity at earth surface     :math:`\frac{m}{s^2}`
   :math:`h`        height above surface         :math:`m`
   :math:`R_{surf}` earth radius at surface      :math:`m`
   ================ ============================ ======================

   .. math::

      g = g_{surf}\left(\frac{R_{surf}}{R_{surf} + h}\right)^2


#. normal gravity at ellipsoid surface

   This is the WGS84 ellipsoidal gravity formula as taken from NIMA TR8350.2

   ================ ===================== =====================
   symbol           name                  unit
   ================ ===================== =====================
   :math:`a`        WGS84 semi-major axis :math:`m`
   :math:`b`        WGS84 semi-minor axis :math:`m`
   :math:`e`        eccentricity          :math:`m`
   :math:`g_{e}`    gravity at equator    :math:`\frac{m}{s^2}`
   :math:`g_{p}`    gravity at poles      :math:`\frac{m}{s^2}`
   :math:`g_{surf}` gravity at surface    :math:`\frac{m}{s^2}`
   :math:`\phi`     latitude              :math:`degN`
   ================ ===================== =====================

   .. math::
      :nowrap:

      \begin{eqnarray}
         e^2 & = & \frac{a^2-b^2}{a^2} \\
         k & = & \frac{bg_{p} - ag_{e}}{ag_{e}} \\
         g_{surf} & = & g_{e}\frac{1 + k {\sin}^2(\frac{\pi}{180}\phi)}{\sqrt{1 - e^2{\sin}^2(\frac{\pi}{180}\phi)}} \\
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}}
      \end{eqnarray}


#. normal gravity above ellipsoid surface

   This is the WGS84 ellipsoidal gravity formula as taken from NIMA TR8350.2

   ================ ==================================== =======================
   symbol           name                                 unit
   ================ ==================================== =======================
   :math:`a`        WGS84 semi-major axis                :math:`m`
   :math:`b`        WGS84 semi-minor axis                :math:`m`
   :math:`f`        WGS84 flattening                     :math:`m`
   :math:`g`        gravity                              :math:`\frac{m}{s^2}`
   :math:`g_{surf}` gravity at the ellipsoid surface     :math:`\frac{m}{s^2}`
   :math:`GM`       WGS84 earth's gravitational constant :math:`\frac{m^3}{s^2}`
   :math:`z`        altitude                             :math:`m`
   :math:`\phi`     latitude                             :math:`degN`
   :math:`\omega`   WGS84 earth angular velocity         :math:`rad/s`
   ================ ==================================== =======================

   The formula used is the one based on the truncated Taylor series expansion:

   .. math::
      :nowrap:

      \begin{eqnarray}
         m & = & \frac{\omega^2a^2b}{GM} \\
         g & = & g_{surf} \left[ 1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z + \frac{3}{a^2}z^2 \right] \\
      \end{eqnarray}


geopotential height
~~~~~~~~~~~~~~~~~~~

   ================ ============================ ======================
   symbol           description                  unit
   ================ ============================ ======================
   :math:`g`        gravity                      :math:`\frac{m}{s^2}`
   :math:`g_{0}`    mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{surf}` gravity at earth surface     :math:`\frac{m}{s^2}`
   :math:`p`        pressure                     :math:`hPa`
   :math:`R_{surf}` earth radius at surface      :math:`m`
   :math:`h`        height above surface         :math:`m`
   :math:`h_{g}`    geopotential height          :math:`m`
   :math:`\phi`     latitude                     :math:`degN`
   :math:`\rho`     mass density                 :math:`\frac{ug}{m^3}`
   ================ ============================ ======================

   The geopotential height allows the gravity in the hydrostatic equation

   .. math::

      dp = - 10^{-8}\rho g dh

   to be replaced by a constant gravity

   .. math::

      dp = - 10^{-8}\rho g_{0} dh_{g}

   providing

   .. math::

      dh_{g} = \frac{g}{g_{0}}dh

   With Newton's gravitational law this becomes

   .. math::

      dh_{g} = \frac{g_{surf}}{g_{0}}\left(\frac{R_{surf}}{R_{surf} + h}\right)^2dh

   And integrating this, considering that :math:`h=0` and :math:`h_{g}=0` at the surface, results in

   .. math::

      h_{g} = \frac{g_{surf}}{g_{0}}\frac{R_{surf}h}{R_{surf} + h}

   .. math::

      h = \frac{g_{0}R_{surf}h_{g}}{g_{surf}R_{surf}-g_{0}h_{g}}


gas constant
~~~~~~~~~~~~

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
~~~~~~~~~~~~~

   ========= ====================== ================================
   symbol    name                   unit
   ========= ====================== ================================
   :math:`k` Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`N` amount of substance    :math:`molec`
   :math:`p` pressure               :math:`hPa`
   :math:`R` universal gas constant :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T` temperature            :math:`K`
   :math:`V` volume                 :math:`m^3`
   ========= ====================== ================================

   .. math::

       pV = 10^{-2}\frac{NRT}{N_{A}} = 10^{-2}NkT


barometric formula
~~~~~~~~~~~~~~~~~~

   =============== ======================= ================================
   symbol          name                    unit
   =============== ======================= ================================
   :math:`g`       gravity                 :math:`\frac{m}{s^2}`
   :math:`g_{0}`   mean earth gravity      :math:`\frac{m}{s^2}`
   :math:`k`       Boltzmann constant      :math:`\frac{kg m^2}{K s^2}`
   :math:`M_{air}` molar mass of total air :math:`\frac{g}{mol}`
   :math:`N`       amount of substance     :math:`molec`
   :math:`N_A`     Avogadro constant       :math:`\frac{1}{mol}`
   :math:`p`       pressure                :math:`hPa`
   :math:`R`       universal gas constant  :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T`       temperature             :math:`K`
   :math:`V`       volume                  :math:`m^3`
   :math:`z`       altitude                :math:`m`
   :math:`z_{g}`   geopotential height     :math:`m`
   :math:`\phi`    latitude                :math:`degN`
   :math:`\rho`    mass density            :math:`\frac{ug}{m^3}`
   =============== ======================= ================================

   From the ideal gas law we have:

   .. math::

      p = \frac{10^{-2}NkT}{V} = \frac{10^6NM_{air}}{VN_{a}}\frac{10^{-8}kTN_{a}}{M_{air}} = \rho \frac{10^{-8}RT}{M_{air}}

   And from the hydrostatic assumption we get:

   .. math::

      10^{2}dp = - 10^{-6}\rho g dz

   Dividing :math:`dp` by `p` we get:

   .. math::

      \frac{dp}{p} = -\frac{M_{air}10^{-8}\rho g dz}{\rho10^{-8}RT} = -\frac{M_{air}gdz}{RT}

   Integrating this expression from one pressure level to the next we get:

   .. math::

      p(i+1) = p(i)e^{-\int^{z(i+1)}_{z(i)}\frac{M_{air}g}{RT}dz}

   We can approximate this further by using an average value of the height dependent quantities
   :math:`M_{air}`, :math:`g` and :math:`T` for the integration over the range :math:`[z(i),z(i+1)]`.
   This gives:

   .. math::
      :nowrap:

      \begin{eqnarray}
         g & = & g(\phi,\frac{z(i)+z(i+1)}{2}) \\
         p(i+1) & = & p(i)e^{-\frac{M_{air}(i)+M_{air}(i+1)}{2}\frac{2}{T(i)+T(i+1)}\frac{g}{R}\left(z(i+1)-z(i)\right)} \\
                & = & p(i)e^{-\frac{M_{air}(i)+M_{air}(i+1)}{T(i)+T(i+1)}\frac{g}{R}\left(z(i+1)-z(i)\right)}
      \end{eqnarray}

   When using geopotential height the formula is the same except that :math:`g=g_{0}` at all levels:

   .. math::

       p(i+1) = p(i)e^{-\frac{M_{air}(i)+M_{air}(i+1)}{T(i)+T(i+1)}\frac{g_{0}}{R}\left(z_{g}(i+1)-z_{g}(i)\right)}

   
mass density
~~~~~~~~~~~~

   =============== ======================= ======================
   symbol          name                    unit
   =============== ======================= ======================
   :math:`N`       amount of substance     :math:`molec`
   :math:`N_A`     Avogadro constant       :math:`\frac{1}{mol}`
   :math:`M_{air}` molar mass of total air :math:`\frac{g}{mol}`
   :math:`V`       volume                  :math:`m^3`
   :math:`\rho`    mass density            :math:`\frac{ug}{m^3}`
   =============== ======================= ======================

   .. math::

      \rho = 10^6\frac{NM_{air}}{VN_{a}}


number density
~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~

   ==================== =========================== =========================
   symbol               name                        unit
   ==================== =========================== =========================
   :math:`n_{air}`      number density of total air :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}` number density of dry air   :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`   number density of H2O       :math:`\frac{molec}{m^3}`
   :math:`M_{air}`      molar mass of total air     :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air       :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O           :math:`\frac{g}{mol}`
   ==================== =========================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         n_{air} & = & n_{dry\_air} + n_{H_{2}O} \\
         M_{air}n_{air} & = & M_{dry\_air}n_{dry\_air} + M_{H_{2}O}n_{H_{2}O}
      \end{eqnarray}


virtual temperature
~~~~~~~~~~~~~~~~~~~

   ==================== ======================== ================================
   symbol               name                     unit
   ==================== ======================== ================================
   :math:`k`            Boltzmann constant       :math:`\frac{kg m^2}{K s^2}`
   :math:`M_{air}`      molar mass of total air  :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air    :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O        :math:`\frac{g}{mol}`
   :math:`N`            amount of substance      :math:`molec`
   :math:`N_A`          Avogadro constant        :math:`\frac{1}{mol}`
   :math:`p`            pressure                 :math:`hPa`
   :math:`p_{dry\_air}` dry air partial pressure :math:`hPa`
   :math:`p_{H_{2}O}`   H2O partial pressure     :math:`hPa`
   :math:`R`            universal gas constant   :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T`            temperature              :math:`K`
   :math:`T_{v}`        virtual temperature      :math:`K`
   :math:`V`            volume                   :math:`m^3`
   ==================== ======================== ================================

   From the ideal gas law we have:

   .. math::

      p = \frac{10^{-2}NkT}{V} = \frac{10^6NM_{air}}{VN_{a}}\frac{10^{-8}kTN_{a}}{M_{air}} = \rho \frac{10^{-8}RT}{M_{air}}

   The virtual temperature allows us to use the dry air molar mass in this equation:

   .. math::

      p = \rho\frac{10^{-8}RT_{v}}{M_{dry\_air}}

   This gives:

   .. math::

      T_{v} = \frac{M_{dry\_air}}{M_{air}}T


volume mixing ratio
~~~~~~~~~~~~~~~~~~~

   ====================== =============================== =========================
   symbol                 name                            unit
   ====================== =============================== =========================
   :math:`n_{air}`        number density of total air     :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}`   number density of dry air       :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`     number density of H2O           :math:`\frac{molec}{m^3}`
   :math:`n_{x}`          number density of quantity x    :math:`\frac{molec}{m^3}`
   :math:`\nu_{x}`        volume mixing ratio of quantity :math:`ppmv`
                          x with regard to total air
   :math:`\bar{\nu}_{x}`  volume mixing ratio of quantity :math:`ppmv`
                          x with regard to dry air
   ====================== =============================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         \nu_{x} & = & 10^{6}\frac{n_{x}}{n_{air}} \\
         \bar{\nu}_{x} & = & 10^{6}\frac{n_{x}}{n_{dry\_air}} \\
         \nu_{dry\_air} & = & 10^{6}\frac{n_{dry\_air}}{n_{air}} =
            10^{6}\frac{n_{air} - n_{H_{2}O}}{n_{air}} = 10^{6} - \nu_{H_{2}O} \\
         \nu_{air} & = & 10^{6}\frac{n_{air}}{n_{air}} = 10^{6} \\
         \bar{\nu}_{dry\_air} & = & 10^{6}\frac{n_{dry\_air}}{n_{dry\_air}} = 10^{6} \\
         \bar{\nu}_{H_{2}O} & = & 10^{6}\frac{n_{H_{2}O}}{n_{dry\_air}} =
            10^{6}\frac{\nu_{H_{2}O}}{\nu_{dry\_air}} = \frac{\nu_{H_{2}O}}{1 - 10^{-6}\nu_{H_{2}O}} \\
         \nu_{H_{2}O} & = & \frac{\bar{\nu}_{H_{2}O}}{1 + 10^{-6}\bar{\nu}_{H_{2}O}}
      \end{eqnarray}


mass mixing ratio
~~~~~~~~~~~~~~~~~

   ===================== =============================== =========================
   symbol                name                            unit
   ===================== =============================== =========================
   :math:`M_{air}`       molar mass of total air         :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}`  molar mass of dry air           :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass of quantity x        :math:`\frac{g}{mol}`
   :math:`n_{air}`       number density of total air     :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}`  number density of dry air       :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`    number density of H2O           :math:`\frac{molec}{m^3}`
   :math:`n_{x}`         number density of quantity x    :math:`\frac{molec}{m^3}`
   :math:`q_{x}`         mass mixing ratio of quantity x :math:`\frac{{\mu}g}{g}`
                         with regard to total air
   :math:`\bar{q}_{x}`   mass mixing ratio of quantity x :math:`\frac{{\mu}g}{g}`
                         with regard to dry air
   :math:`\nu_{x}`       volume mixing ratio of quantity :math:`ppmv`
                         x with regard to total air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity :math:`ppmv`
                         x with regard to dry air
   ===================== =============================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         q_{x} & = & 10^{6}\frac{n_{x}M_{x}}{n_{air}M_{air}} = \nu_{x}\frac{M_{x}}{M_{air}} \\
         \bar{q}_{x} & = & 10^{6}\frac{n_{x}M_{x}}{n_{dry\_air}M_{dry\_air}} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}} \\
         q_{dry\_air} & = & 10^{6}\frac{n_{dry\_air}M_{dry\_air}}{n_{air}M_{air}} =
            10^{6}\frac{n_{air}M_{air} - n_{H_{2}O}M_{H_{2}O}}{n_{air}M_{air}} = 10^{6} - q_{H_{2}O} \\
         q_{air} & = & 10^{6}\frac{n_{air}M_{air}}{n_{air}M_{air}} = 10^{6} \\
         \bar{q}_{dry\_air} & = & 10^{6}\frac{n_{dry\_air}M_{dry\_air}}{n_{dry\_air}M_{dry\_air}} = 10^{6} \\
         \bar{q}_{H_{2}O} & = & 10^{6}\frac{n_{H_{2}O}M_{H_{2}O}}{n_{dry\_air}M_{dry\_air}} =
            10^{6}\frac{q_{H_{2}O}}{q_{dry\_air}} = \frac{q_{H_{2}O}}{1 - 10^{-6}q_{H_{2}O}} \\
         q_{H_{2}O} & = & \frac{\bar{q}_{H_{2}O}}{1 + 10^{-6}\bar{q}_{H_{2}O}}
      \end{eqnarray}


molar mass of total air
~~~~~~~~~~~~~~~~~~~~~~~

#. molar mass of total air from H2O volume mixing ratio

   ==================== =========================== =========================
   symbol               name                        unit
   ==================== =========================== =========================
   :math:`M_{air}`      molar mass of total air     :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air       :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O           :math:`\frac{g}{mol}`
   :math:`n_{air}`      number density of total air :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}` number density of dry air   :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`   number density of H2O       :math:`\frac{molec}{m^3}`
   :math:`\nu_{H_{2}O}` volume mixing ratio of H2O  :math:`ppmv`
   ==================== =========================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         M_{air} & = & \frac{M_{dry\_air}n_{dry\_air} + M_{H_{2}O}n_{H_{2}O}}{n_{air}} \\
                 & = & M_{dry\_air}\left(1 - 10^{-6}\nu_{H_{2}O}\right) + 10^{-6}M_{H_{2}O}\nu_{H_{2}O}
      \end{eqnarray}


#. molar mass of total air from H2O mass mixing ratio

   ==================== =========================== =========================
   symbol               name                        unit
   ==================== =========================== =========================
   :math:`M_{air}`      molar mass of total air     :math:`\frac{g}{mol}`
   :math:`M_{dry\_air}` molar mass of dry air       :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O           :math:`\frac{g}{mol}`
   :math:`n_{air}`      number density of total air :math:`\frac{molec}{m^3}`
   :math:`n_{dry\_air}` number density of dry air   :math:`\frac{molec}{m^3}`
   :math:`n_{H_{2}O}`   number density of H2O       :math:`\frac{molec}{m^3}`
   :math:`q_{H_{2}O}`   mass mixing ratio of H2O    :math:`\frac{ug}{g}`
   :math:`\nu_{H_{2}O}` volume mixing ratio of H2O  :math:`\frac{ug}{g}`
   ==================== =========================== =========================

   .. math::
      :nowrap:

      \begin{eqnarray}
         M_{air} & = & M_{dry\_air}\left(1 - 10^{-6}\nu_{H_{2}O}\right) + 10^{-6}M_{H_{2}O}\nu_{H_{2}O} \\
                 & = & M_{dry\_air}\left(1 - 10^{-6}\frac{M_{air}}{M_{H_{2}O}}q_{H_{2}O}\right) + 10^{-6}M_{air}q_{H_{2}O} \\
                 & = & \frac{M_{dry\_air}}{1 + 10^{-6}\frac{M_{dry\_air}}{M_{H_{2}O}}q_{H_{2}O} - 10^{-6}q_{H_{2}O}} \\
                 & = & \frac{M_{H_{2}O}M_{dry\_air}}{M_{H_{2}O} + 10^{-6}M_{dry\_air}q_{H_{2}O} - 10^{-6}M_{H_{2}O}q_{H_{2}O}} \\
                 & = & \frac{M_{H_{2}O}M_{dry\_air}}{\left(1-10^{-6}q_{H_{2}O}\right)M_{H_{2}O} + 10^{-6}q_{H_{2}O}M_{dry\_air}} \\
      \end{eqnarray}


partial pressure
~~~~~~~~~~~~~~~~

   ===================== =============================== ============
   symbol                name                            unit
   ===================== =============================== ============
   :math:`p`             total pressure                  :math:`hPa`
   :math:`p_{x}`         partial pressure of quantity    :math:`hPa`
   :math:`\nu_{x}`       volume mixing ratio of quantity :math:`ppmv`
                         x with regard to total air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity :math:`ppmv`
                         x with regard to dry air
   ===================== =============================== ============

   .. math::
      :nowrap:

      \begin{eqnarray}
         p_{x} & = & \nu_{x}p \\
         p_{x} & = & \bar{\nu}_{x}p_{dry\_air} \\
         p_{x} & = & 10^{-2}N_{x}kT
      \end{eqnarray}


saturated water vapor pressure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ============= =============================== ===========
   symbol        name                            unit
   ============= =============================== ===========
   :math:`e_{w}` saturated water vapor pressure  :math:`hPa`
   :math:`T`     temperature                     :math:`K`
   ============= =============================== ===========

   This is the August-Roche-Magnus formula for the saturated water vapour pressure 

   .. math::

      e_{w} = 6.1094e^{\frac{17.625(T-273.15)}{(T-273.15)+243.04}}


Common variable conversions
---------------------------

#. time dependent from time independent variable:

   .. math::

      \forall t: x(t,:) = x(:)


#. total uncertainty from random and systematic uncertainty:

   =================== ================================== ========================= =======================================
   symbol              description                        unit                      variable name
   =================== ================================== ========================= =======================================
   :math:`\mu_{x}`     total uncertainty for a variable x same as that of :math:`x` `<variable>_uncertainty {:}`
   :math:`\mu^{r}_{x}` uncertainty due to random effects  same as that of :math:`x` `<variable>_uncertainty_random {:}`
                       for a variable x
   :math:`\mu^{s}_{x}` uncertainty due to systematic      same as that of :math:`x` `<variable>_uncertainty_systematic {:}`
                       effects for a variable x
   =================== ================================== ========================= =======================================

   The pattern `:` for the first dimensions can represent any combination of dimensions for which `x {:}` exists.

   .. math::

      \mu_{x} = \sqrt{{\mu^{r}_{x}}^2 + {\mu^{s}_{x}}^2}


#. total uncertainty from vertical covariance:

   ================== ================================== =========================== =============================================
   symbol             description                        unit                        variable name
   ================== ================================== =========================== =============================================
   :math:`S_{x}(i,j)` covariance for a variable x        square of that of :math:`x` `<variable>_covariance {:,vertical,vertical}`
   :math:`\mu_{x}(i)` total uncertainty for a variable x same as that of :math:`x`   `<variable>_uncertainty {:,vertical}`
   ================== ================================== =========================== =============================================

   The pattern `:` for the dimensions can represent any combination of dimensions for which `x {:,vertical}` exists.

   .. math::

      \forall i: \mu_{x}(i) = \sqrt{S(i,i)}


Axis variable conversions
-------------------------

Conversions for the vertical axis altitude, pressure and geopotential height
can be found in the Atmospheric variable conversions section.


datetime
~~~~~~~~

#. datetime from start/stop

   ============= ==================== ========================== =======================
   symbol        description          unit                       variable name
   ============= ==================== ========================== =======================
   :math:`t`     datetime (mid point) :math:`s` since 2000-01-01 `datetime {time}`
   :math:`t_{e}` datetime stop        :math:`s` since 2000-01-01 `datetime_stop {time}`
   :math:`t_{s}` datetime start       :math:`s` since 2000-01-01 `datetime_start {time}`
   ============= ==================== ========================== =======================

   .. math::

      t = \frac{t_{s} - t_{e}}{2}


datetime length
~~~~~~~~~~~~~~~

#. datetime length from start/stop

   ================ ============== ========================== ========================
   symbol           description    unit                       variable name
   ================ ============== ========================== ========================
   :math:`t_{e}`    datetime stop  :math:`s` since 2000-01-01 `datetime_stop {time}`
   :math:`t_{s}`    datetime start :math:`s` since 2000-01-01 `datetime_start {time}`
   :math:`\Delta t` time duration  :math:`s`                  `datetime_length {time}`
   ================ ============== ========================== ========================

   .. math::

      \Delta t = t_{s} - t_{e}


datetime start
~~~~~~~~~~~~~~

#. datetime start from datetime and stop

   ================ ==================== ========================== ========================
   symbol           description          unit                       variable name
   ================ ==================== ========================== ========================
   :math:`t`        datetime (mid point) :math:`s` since 2000-01-01 `datetime {time}`
   :math:`t_{s}`    datetime start       :math:`s` since 2000-01-01 `datetime_start {time}`
   :math:`\Delta t` time duration        :math:`s`                  `datetime_length {time}`
   ================ ==================== ========================== ========================

   .. math::

      t_{s} = t - \frac{\Delta t}{2}


datetime stop
~~~~~~~~~~~~~

#. datetime stop from start and length

   ================ ============== ========================== ========================
   symbol           description    unit                       variable name
   ================ ============== ========================== ========================
   :math:`t_{e}`    datetime stop  :math:`s` since 2000-01-01 `datetime_stop {time}`
   :math:`t_{s}`    datetime start :math:`s` since 2000-01-01 `datetime_start {time}`
   :math:`\Delta t` time duration  :math:`s`                  `datetime_length {time}`
   ================ ============== ========================== ========================

   .. math::

      t_{e} = t_{s} + \Delta t


latitude
~~~~~~~~

.. _`latitude from polygon`:

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


latitude bounds
~~~~~~~~~~~~~~~

#. latitude ranges from midpoints

   ===================== =========================================== ============ ================================
   symbol                description                                 unit         variable name
   ===================== =========================================== ============ ================================
   :math:`\phi(i)`       latitude                                    :math:`degN` `latitude {:,latitude}`
   :math:`\phi^{B}(i,l)` latitude boundaries (:math:`l \in \{1,2\}`) :math:`degN` `latitude_bounds {:,latitude,2}`
   ===================== =========================================== ============ ================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         \phi^{B}(1,1) & = & 2\phi(1) - \phi(2) \\
         \phi^{B}(i,1) & = & \frac{\phi(i-1) + \phi(i)}{2}, 1 < i \leq N \\
         \phi^{B}(i,2) & = & \phi^{B}(i+1,1), 1 \leq i < N \\
         \phi^{B}(N,2) & = & 2\phi(N) - \phi(N-1)
      \end{eqnarray}


longitude
~~~~~~~~~

#. longitude from polygon

   See `latitude from polygon`_

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


longitude bounds
~~~~~~~~~~~~~~~~

#. longitude ranges from midpoints

   ======================== ============================================ ============ ==================================
   symbol                   description                                  unit         variable name
   ======================== ============================================ ============ ==================================
   :math:`\lambda(i)`       longitude                                    :math:`degE` `longitude {:,longitude}`
   :math:`\lambda^{B}(i,l)` longitude boundaries (:math:`l \in \{1,2\}`) :math:`degE` `longitude_bounds {:,longitude,2}`
   ======================== ============================================ ============ ==================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         \lambda^{B}(1,1) & = & 2\lambda(1) - \lambda(2) \\
         \lambda^{B}(i,1) & = & \frac{\lambda(i-1) + \lambda(i)}{2}, 1 < i \leq N \\
         \lambda^{B}(i,2) & = & \lambda^{B}(i+1,1), 1 \leq i < N \\
         \lambda^{B}(N,2) & = & 2\lambda(N) - \lambda(N-1)
      \end{eqnarray}


Angle variable conversions
--------------------------

relative azimuth angle
~~~~~~~~~~~~~~~~~~~~~~

#. relative azimuth angle from sensor and solar azimuth angle

   =================== ====================== =========== ============================
   symbol              description            unit        variable name
   =================== ====================== =========== ============================
   :math:`\varphi_{0}` solar azimuth angle    :math:`deg` `solar_azimuth_angle {:}`
   :math:`\varphi_{r}` relative azimuth angle :math:`deg` `relative_azimuth_angle {:}`
   :math:`\varphi_{S}` sensor azimuth angle   :math:`deg` `sensor_azimuth_angle {:}`
   =================== ====================== =========== ============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
        \Delta\varphi & = & \varphi_{0} - \varphi_{S} \\
        \Delta\varphi & = & \begin{cases}
            \Delta\varphi \geq 360, & \Delta\varphi - 360 \\
            0 \leq \Delta\varphi < 360, & \Delta\varphi \\
            \Delta\varphi < 0, & \Delta\varphi + 360
         \end{cases} \\
        \varphi_{r} & = & \left|\Delta\varphi - 180 \right|
      \end{eqnarray}


scattering angle
~~~~~~~~~~~~~~~~

#. scattering angle from sensor and solar angles

   =================== ====================== =========== ==========================
   symbol              description            unit        variable name
   =================== ====================== =========== ==========================
   :math:`\theta_{0}`  solar zenith angle     :math:`deg` `solar_zenith_angle {:}`
   :math:`\Theta_{s}`  scattering angle       :math:`deg` `scattering_angle {:}`
   :math:`\theta_{S}`  sensor zenith angle    :math:`deg` `sensor_zenith_angle {:}`
   :math:`\varphi_{r}` relative azimuth angle :math:`deg` `relative_azimuth_angle {:}`
   =================== ====================== =========== ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::
      \Theta_{s} = \frac{180}{\pi}\arccos\left(-\cos\left(\frac{\pi}{180}\theta_{0}\right)\cos\left(\frac{\pi}{180}\theta_{S}\right) - 
            \sin\left(\frac{\pi}{180}\theta_{0}\right)\sin\left(\frac{\pi}{180}\theta_{S}\right)\cos\left(\frac{\pi}{180}\varphi_{r}\right)\right)


sensor azimuth angle
~~~~~~~~~~~~~~~~~~~~

#. sensor azimuth angle from viewing azimuth angle

   =================== ===================== =========== ===========================
   symbol              description           unit        variable name
   =================== ===================== =========== ===========================
   :math:`\varphi_{S}` sensor azimuth angle  :math:`deg` `sensor_azimuth_angle {:}`
   :math:`\varphi_{V}` viewing azimuth angle :math:`deg` `viewing_azimuth_angle {:}`
   =================== ===================== =========== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \varphi_{S} = 180 - \varphi_{V}


sensor elevation angle
~~~~~~~~~~~~~~~~~~~~~~

#. sensor elevation angle from sensor zenith angle

   ================== ====================== =========== ============================
   symbol             description            unit        variable name
   ================== ====================== =========== ============================
   :math:`\alpha_{S}` sensor elevation angle :math:`deg` `sensor_elevation_angle {:}`
   :math:`\theta_{S}` sensor zenith angle    :math:`deg` `sensor_zenith_angle {:}`
   ================== ====================== =========== ============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \alpha_{S} = 90 - \theta_{S}


sensor zenith angle
~~~~~~~~~~~~~~~~~~~

#. sensor zenith angle from sensor elevation angle

   ================== ====================== =========== ============================
   symbol             description            unit        variable name
   ================== ====================== =========== ============================
   :math:`\alpha_{S}` sensor elevation angle :math:`deg` `sensor_elevation_angle {:}`
   :math:`\theta_{S}` sensor zenith angle    :math:`deg` `sensor_zenith_angle {:}`
   ================== ====================== =========== ============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{S} = 90 - \alpha_{S}


#. sensor zenith angle from viewing zenith angle

   ================== ==================== =========== ==========================
   symbol             description          unit        variable name
   ================== ==================== =========== ==========================
   :math:`\theta_{S}` sensor zenith angle  :math:`deg` `sensor_zenith_angle {:}`
   :math:`\theta_{V}` viewing zenith angle :math:`deg` `viewing_zenith_angle {:}`
   ================== ==================== =========== ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{S} = 180 - \theta_{V}


solar azimuth angle
~~~~~~~~~~~~~~~~~~~

.. _`solar azimuth angle from datetime and latitude/longitude`:

#. solar azimuth angle from datetime and latitude/longitude

   =================== ======================= ========================== ===========================
   symbol              description             unit                       variable name
   =================== ======================= ========================== ===========================
   :math:`EOT`         equation of time        :math:`deg`
   :math:`t`           datetime                :math:`s` since 2000-01-01 `datetime {time}`
   :math:`\alpha_{0}`  solar elevation angle   :math:`deg`                `solar_elevation_angle {:}`
   :math:`\delta`      solar declination angle :math:`deg`
   :math:`\eta`        orbit angle of the      :math:`deg`
                       earth around the sun
   :math:`\lambda`     longitude               :math:`degE`               `longitude {:}`
   :math:`\phi`        latitude                :math:`degN`               `latitude {:}`
   :math:`\varphi_{0}` solar azimuth angle     :math:`deg`                `solar_azimuth_angle {:}`
   =================== ======================= ========================== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         \eta & = & 2\pi \left( \frac{t}{365.2422 \cdot 86400} - \lvert \frac{t}{365.2422 \cdot 86400} \rvert \right) \\
         \delta & = & 0.006918 - 0.399912 \cos(\eta) - 0.006758 \cos(2\eta) - 0.002697 \cos(3\eta) + \\
            & & 0.070257 \sin(\eta) + 0.000907 \sin(2\eta) + 0.001480 \sin(3\eta) \\
         EOT & = & 0.0072 \cos(\eta) - 0.0528 \cos(2\eta) - 0.0012 \cos(3\eta) - \\
            & & 0.1229 \sin(\eta) - 0.1565 \sin(2\eta) - 0.0041 \sin(3\eta) \\
         f_{day} & = & \frac{t}{86400} - \lvert \frac{t}{86400} \rvert \\
         \Omega & = & 2\pi\left(f_{day} + \frac{\lambda}{360} + \frac{EOT-12}{24}\right) \\
         \alpha_{0} & = & \frac{180}{\pi}\arcsin(\sin(\delta)\sin(\frac{\pi}{180}\phi) +
            \cos(\delta)\cos(\frac{\pi}{180}\phi)\cos(\Omega)) \\
         \varphi_{0} & = & \begin{cases}
            \alpha_{0} = 0, & 0 \\
            \alpha_{0} \neq 0, & \frac{180}{\pi}\arctan(\frac{\cos(\delta)\sin(\Omega)}{\cos(\frac{\pi}{180}\alpha_{0})},
               \frac{-\sin(\delta)\cos(\frac{\pi}{180}\phi) +
               \cos(\delta)\sin(\frac{\pi}{180}\phi)\cos(\Omega)}{\cos(\frac{\pi}{180}\alpha_{0})}) 
         \end{cases}
      \end{eqnarray}


solar elevation angle
~~~~~~~~~~~~~~~~~~~~~

#. solar elevation angle from solar zenith angle

   ================== ===================== =========== ===========================
   symbol             description           unit        variable name
   ================== ===================== =========== ===========================
   :math:`\alpha_{0}` solar elevation angle :math:`deg` `solar_elevation_angle {:}`
   :math:`\theta_{0}` solar zenith angle    :math:`deg` `solar_zenith_angle {:}`
   ================== ===================== =========== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \alpha_{0} = 90 - \theta_{0}


#. solar elevation angle from datetime and latitude/longitude

   See `solar azimuth angle from datetime and latitude/longitude`_

|

solar zenith angle
~~~~~~~~~~~~~~~~~~

#. solar zenith angle from solar elevation angle

   ================== ===================== =========== =========================
   symbol             description           unit        variable name
   ================== ===================== =========== =========================
   :math:`\alpha_{0}` solar elevation angle :math:`deg` `solar_azimuth_angle {:}`
   :math:`\theta_{0}` solar zenith angle    :math:`deg` `solar_zenith_angle {:}`
   ================== ===================== =========== =========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{0} = 90 - \alpha_{0}


viewing azimuth angle
~~~~~~~~~~~~~~~~~~~~~

#. viewing azimuth angle from sensor azimuth angle

   =================== ===================== =========== ===========================
   symbol              description           unit        variable name
   =================== ===================== =========== ===========================
   :math:`\varphi_{S}` sensor azimuth angle  :math:`deg` `sensor_azimuth_angle {:}`
   :math:`\varphi_{V}` viewing azimuth angle :math:`deg` `viewing_azimuth_angle {:}`
   =================== ===================== =========== ===========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \varphi_{V} = 180 - \varphi_{S}


viewing elevation angle
~~~~~~~~~~~~~~~~~~~~~~~

#. viewing elevation angle from viewing zenith angle

   ================== ======================= =========== =============================
   symbol             description             unit        variable name
   ================== ======================= =========== =============================
   :math:`\alpha_{V}` viewing elevation angle :math:`deg` `viewing_elevation_angle {:}`
   :math:`\theta_{V}` viewing zenith angle    :math:`deg` `viewing_zenith_angle {:}`
   ================== ======================= =========== =============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \alpha_{V} = 90 - \theta_{V}


viewing zenith angle
~~~~~~~~~~~~~~~~~~~~

#. viewing zenith angle from sensor zenith angle

   ================== ==================== =========== ==========================
   symbol             description          unit        variable name
   ================== ==================== =========== ==========================
   :math:`\theta_{S}` sensor zenith angle  :math:`deg` `sensor_zenith_angle {:}`
   :math:`\theta_{V}` viewing zenith angle :math:`deg` `viewing_zenith_angle {:}`
   ================== ==================== =========== ==========================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{V} = 180 - \theta_{S}


#. viewing zenith angle from viewing elevation angle

   ================== ======================= =========== =============================
   symbol             description             unit        variable name
   ================== ======================= =========== =============================
   :math:`\alpha_{V}` viewing elevation angle :math:`deg` `viewing_elevation_angle {:}`
   :math:`\theta_{V}` viewing zenith angle    :math:`deg` `viewing_zenith_angle {:}`
   ================== ======================= =========== =============================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      \theta_{V} = 90 - \alpha_{V}


Atmospheric variable conversions
--------------------------------

aerosol extinction coefficient
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. aerosol extinction coefficient from aerosol optical depth

   ================ =========================================== =================== ====================================
   symbol           description                                 unit                variable name
   ================ =========================================== =================== ====================================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`           `altitude_bounds {:,2}`
   :math:`\sigma`   aerosol extinction coefficient              :math:`\frac{1}{m}` `aerosol_extinction_coefficient {:}`
   :math:`\tau`     aerosol optical depth                       :math:`-`           `aerosol_optical_depth {:}`
   ================ =========================================== =================== ====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma = \frac{\tau}{\lvert z^{B}(2) - z^{B}(1) \rvert}


aerosol optical depth
~~~~~~~~~~~~~~~~~~~~~

#. total aerosol optical depth from partial aerosol optical depth profile

   =============== =========================== ========= ====================================
   symbol          description                 unit      variable name
   =============== =========================== ========= ====================================
   :math:`\tau`    total aerosol optical depth :math:`-` `aerosol_optical_depth {:}`
   :math:`\tau(i)` aerosol optical depth       :math:`-` `aerosol_optical_depth {:,vertical}`
   =============== =========================== ========= ====================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \tau = \sum_{i}{\tau(i)}


#. aerosol optical depth from aerosol extinction coefficient

   ================ =========================================== =================== ====================================
   symbol           description                                 unit                variable name
   ================ =========================================== =================== ====================================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`           `altitude_bounds {:,2}`
   :math:`\sigma`   aerosol extinction coefficient              :math:`\frac{1}{m}` `aerosol_extinction_coefficient {:}`
   :math:`\tau`     aerosol optical depth                       :math:`-`           `aerosol_optical_depth {:}`
   ================ =========================================== =================== ====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \tau = \sigma \lvert z^{B}(2) - z^{B}(1) \rvert


altitude
~~~~~~~~

#. altitude from geopotential height

   ================= ============================ ===================== =========================
   symbol            description                  unit                  variable name
   ================= ============================ ===================== =========================
   :math:`g_{0}`     mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{wgs84}` gravity at WGS84 ellipsoid   :math:`\frac{m}{s^2}`
   :math:`R_{wgs84}` local earth curvature radius :math:`m`
                     at WGS84 ellipsoid
   :math:`z`         altitude                     :math:`m`             `altitude {:}`
   :math:`z_{g}`     geopotential height          :math:`m`             `geopotential_height {:}`
   :math:`\phi`      latitude                     :math:`degN`          `latitude {:}`
   ================= ============================ ===================== =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{time}`, `{time,vertical}`, or no dimensions at all.

   This equation approximates the mean sea level gravity and radius by that of the reference ellipsoid.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{wgs84} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R_{wgs84} & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z & = & \frac{g_{0}R_{wgs84}z_{g}}{g_{wgs84}R_{wgs84} - g_{0}z_{g}}
      \end{eqnarray}


#. altitude from bounds

   ================ =========================================== ========= =======================
   symbol           description                                 unit      variable name
   ================ =========================================== ========= =======================
   :math:`z`        altitude                                    :math:`m` `altitude {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m` `altitude_bounds {:,2}`
   ================ =========================================== ========= =======================

   The pattern `:` for the dimensions can represent `{vertical}`, or `{time,vertical}`.

   .. math::

      z = \frac{z^{B}(2) + z^{B}(1)}{2}


#. altitude from sensor altitude

   ================= ====================== ========= =====================
   symbol            description            unit      variable name
   ================= ====================== ========= =====================
   :math:`z`         altitude               :math:`m` `altitude {:}`
   :math:`z_{instr}` altitude of the sensor :math:`m` `sensor_altitude {:}`
   ================= ====================== ========= =====================

   The pattern `:` for the dimensions can represent `{time}`, or no dimensions at all.

   .. math::

      z = z_{instr}


#. altitude from pressure

   ================== ============================ ================================ ==========================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==========================
   :math:`a`          WGS84 semi-major axis        :math:`m`
   :math:`b`          WGS84 semi-minor axis        :math:`m`
   :math:`f`          WGS84 flattening             :math:`m`
   :math:`g`          gravity                      :math:`\frac{m}{s^2}`
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{surf}`   gravity at surface           :math:`\frac{m}{s^2}`
   :math:`GM`         WGS84 earth's gravitational  :math:`\frac{m^3}{s^2}`
                      constant
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`hPa`                      `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`hPa`                      `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z(i)`       altitude                     :math:`m`                        `altitude {:,vertical}`
   :math:`z_{surf}`   surface height               :math:`m`                        `surface_altitude {:}`
   :math:`\phi`       latitude                     :math:`degN`                     `latitude {:}`
   :math:`\omega`     WGS84 earth angular velocity :math:`rad/s`
   ================== ============================ ================================ ==========================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{surf}` need to use the same definition of 'surface'.

   The pressures :math:`p(i)` are expected to be at higher levels than the surface pressure (i.e. lower values).
   This should normally be the case since even for pressure grids that start at the surface, :math:`p_{surf}` should
   equal the lower pressure boundary :math:`p^{B}(1,1)`, whereas :math:`p(1)` should then be between :math:`p^{B}(1,1)`
   and :math:`p^{B}(1,2)` (which is generally not equal to :math:`p^{B}(1,1)`).

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         g(1) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z_{surf} + \frac{3}{a^2}z_{surf}^2\right) \\
         g(i) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)z(i-1) + \frac{3}{a^2}z(i-1)^2\right), 1 < i \leq N \\
         z(1) & = & z_{surf} + \frac{T(1)}{M_{air}(1)}\frac{R}{g(1)}\ln\left(\frac{p_{surf}}{p(i)}\right) \\
         z(i) & = & z(i-1) + \frac{T(i-1)+T(i)}{M_{air}(i-1)+M_{air}(i)}\frac{R}{g(i)}\ln\left(\frac{p(i-1)}{p(i)}\right), 1 < i \leq N
      \end{eqnarray}


#. surface altitude from surface geopotential height

   ================== ============================ ===================== =================================
   symbol             description                  unit                  variable name
   ================== ============================ ===================== =================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{wgs84}`  gravity at WGS84 ellipsoid   :math:`\frac{m}{s^2}`
   :math:`R_{wgs84}`  local earth curvature radius :math:`m`
                      at WGS84 ellipsoid
   :math:`z_{surf}`   surface altitude             :math:`m`             `surface_altitude {:}`
                      (relative to mean sea level)
   :math:`z_{g,surf}` surface geopotential height  :math:`m`             `surface_geopotential_height {:}`
                      (relative to mean sea level)
   :math:`\phi`       latitude                     :math:`degN`          `latitude {:}`
   ================== ============================ ===================== =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{time}`, `{time,vertical}`, or no dimensions at all.

   This equation approximates the mean sea level gravity and radius by that of the reference ellipsoid.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{wgs84} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R_{wgs84} & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z_{surf} & = & \frac{g_{0}R_{wgs84}z_{g,surf}}{g_{wgs84}R_{wgs84} - g_{0}z_{g,surf}}
      \end{eqnarray}


altitude bounds
~~~~~~~~~~~~~~~

#. altitude ranges from midpoints

   ================== =========================================== ========= ================================
   symbol             description                                 unit      variable name
   ================== =========================================== ========= ================================
   :math:`z(i)`       altitude                                    :math:`m` `altitude {:,vertical}`
   :math:`z^{B}(i,l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m` `altitude_bounds {:,vertical,2}`
   ================== =========================================== ========= ================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         z^{B}(1,1) & = & 2z(1) - z(2) \\
         z^{B}(i,1) & = & \frac{z(i-1) + z(i)}{2}, 1 < i \leq N \\
         z^{B}(i,2) & = & z^{B}(i+1,1), 1 \leq i < N \\
         z^{B}(N,2) & = & 2z(N) - z(N-1)
      \end{eqnarray}


column mass density
~~~~~~~~~~~~~~~~~~~

#. column mass density for air component from mass density:

   ================== =========================================== ====================== ==============================
   symbol             description                                 unit                   variable name
   ================== =========================================== ====================== ==============================
   :math:`z^{B}(l)`   altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho_{x}`   mass density for air component x            :math:`\frac{ug}{m^3}` `<species>_density {:}`
                      (e.g. :math:`\rho_{O_{3}}`)
   :math:`\sigma_{x}` column mass density for air component x     :math:`\frac{ug}{m^2}` `<species>_column_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   ================== =========================================== ====================== ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma_{x} = \rho_{x} \lvert z^{B}(2) - z^{B}(1) \rvert


#. column mass density for total air from mass density:

   ================ =========================================== ====================== =======================
   symbol           description                                 unit                   variable name
   ================ =========================================== ====================== =======================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho`     mass density for total air                  :math:`\frac{ug}{m^3}` `density {:}`
   :math:`\sigma`   column mass density for total air           :math:`\frac{ug}{m^2}` `column_density {:}`
   ================ =========================================== ====================== =======================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \sigma = \rho \lvert z^{B}(2) - z^{B}(1) \rvert


#. column mass density for air component from column number density:

   This conversion applies to both total columns as well as partial column profiles.

   ================== ========================================= ========================= =====================================
   symbol             description                               unit                      variable name
   ================== ========================================= ========================= =====================================
   :math:`c_{x}`      column number density for air component x :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   :math:`M_{x}`      molar mass for air component x            :math:`\frac{g}{mol}`
   :math:`N_A`        Avogadro constant                         :math:`\frac{1}{mol}`
   :math:`\sigma_{x}` column mass density for air component x   :math:`\frac{ug}{m^2}`    `<species>_column_density {:}`
                      (e.g. :math:`\sigma_{O_{3}}`)
   ================== ========================================= ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma_{x} = \frac{10^{6}c_{x}M_{x}}{N_{A}}


#. column mass density for total air from column number density:

   This conversion applies to both total columns as well as partial column profiles.

   =============== =================================== ========================= ===========================
   symbol          description                         unit                      variable name
   =============== =================================== ========================= ===========================
   :math:`c`       column number density for total air :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`M_{air}` molar mass for total air            :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`N_A`     Avogadro constant                   :math:`\frac{1}{mol}`
   :math:`\sigma`  column mass density for total air   :math:`\frac{ug}{m^2}`    `column_density {:}`
   =============== =================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \sigma = \frac{10^{6}c M_{air}}{N_{A}}


column mass mixing ratio
~~~~~~~~~~~~~~~~~~~~~~~~

#. column mass mixing ratio from column volume mixing ratio

   =============== ======================================== ======================== ==========================================
   symbol          description                              unit                     variable name
   =============== ======================================== ======================== ==========================================
   :math:`M_{air}` molar mass for total air                 :math:`\frac{g}{mol}`    `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`q_{x}`   column mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_column_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` column volume mixing ratio of quantity x :math:`ppmv`             `<species>_column_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================== ======================== ==========================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      q_{x} = \nu_{x}\frac{M_{x}}{M_{air}}


#. column mass mixing ratio dry air from column volume mixing ratio dry air

   ===================== ======================================== ======================== ==================================================
   symbol                description                              unit                     variable name
   ===================== ======================================== ======================== ==================================================
   :math:`M_{dry\_air}`  molar mass for dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   column mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_column_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` column volume mixing ratio of quantity x :math:`ppmv`             `<species>_column_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ======================================== ======================== ==================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}


#. stratospheric column mass mixing ratio dry air from stratospheric column volume mixing ratio dry air

   ===================== ======================================== ======================== ================================================================
   symbol                description                              unit                     variable name
   ===================== ======================================== ======================== ================================================================
   :math:`M_{dry\_air}`  molar mass for dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   stratospheric column mass mixing ratio   :math:`\frac{{\mu}g}{g}` `stratospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` stratospheric column volume mixing ratio :math:`ppmv`             `stratospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================== ======================== ================================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}


#. tropospheric column mass mixing ratio dry air from tropospheric column volume mixing ratio dry air

   ===================== ======================================= ======================== ===============================================================
   symbol                description                             unit                     variable name
   ===================== ======================================= ======================== ===============================================================
   :math:`M_{dry\_air}`  molar mass for dry air                  :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x          :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   tropospheric column mass mixing ratio   :math:`\frac{{\mu}g}{g}` `tropospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` tropospheric column volume mixing ratio :math:`ppmv`             `tropospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================= ======================== ===============================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}


column number density
~~~~~~~~~~~~~~~~~~~~~

#. total column number density for air component from partial column number density profile:

   ================ ======================================= ========================= ==============================================
   symbol           description                             unit                      variable name
   ================ ======================================= ========================= ==============================================
   :math:`c_{x}`    total column number density for air     :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    component x (e.g. :math:`c_{O_{3}}`)
   :math:`c_{x}(i)` column number density profile for air   :math:`\frac{molec}{m^2}` `<species>_column_number_density {:,vertical}`
                    component x (e.g. :math:`c_{O_{3}}(i)`)
   ================ ======================================= ========================= ==============================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{c_{x}(i)}


#. total column number density for total air from partial column number density profile:

   ============ =========================================== ========================= ====================================
   symbol       description                                 unit                      variable name
   ============ =========================================== ========================= ====================================
   :math:`c`    total column number density for total air   :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`c(i)` column number density profile for total air :math:`\frac{molec}{m^2}` `column_number_density {:,vertical}`
   ============ =========================================== ========================= ====================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      c_{x} = \sum_{i}{c_{x}(i)}


#. column number density for air component from number density:

   ================ =========================================== ========================= =====================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= =====================================
   :math:`c_{x}`    column number density for air component x   :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    (e.g. :math:`c_{O_{3}}`)
   :math:`n_{x}`    number density for air component x          :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c_{x} = n_{x} \lvert z^{B}(2) - z^{B}(1) \rvert


#. column number density for total air from number density:

   ================ =========================================== ========================= ===========================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===========================
   :math:`c`        column number density for total air         :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`n`        number density for total air                :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c = n \lvert z^{B}(2) - z^{B}(1) \rvert


#. column number density for air component from column mass density:

   This conversion applies to both total columns as well as partial column profiles.

   ================== ========================================= ========================= =====================================
   symbol             description                               unit                      variable name
   ================== ========================================= ========================= =====================================
   :math:`c_{x}`      column number density for air component x :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                      (e.g. :math:`n_{O_{3}}`)
   :math:`M_{x}`      molar mass for air component x            :math:`\frac{g}{mol}`
   :math:`N_A`        Avogadro constant                         :math:`\frac{1}{mol}`
   :math:`\sigma_{x}` column mass density for air component x   :math:`\frac{ug}{m^2}`    `<species>_column_density {:}`
                      (e.g. :math:`\sigma_{O_{3}}`)
   ================== ========================================= ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c_{x} = \frac{\sigma_{x}N_{A}}{10^{6}M_{x}}


#. column number density for total air from column mass density:

   This conversion applies to both total columns as well as partial column profiles.

   =============== =================================== ========================= ===========================
   symbol          description                         unit                      variable name
   =============== =================================== ========================= ===========================
   :math:`c`       column number density for total air :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`M_{air}` molar mass for total air            :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`N_A`     Avogadro constant                   :math:`\frac{1}{mol}`
   :math:`\sigma`  column mass density for total air   :math:`\frac{ug}{m^2}`    `column_density {:}`
   =============== =================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      c = \frac{\sigma N_{A}}{10^{6}M_{air}}


column volume mixing ratio
~~~~~~~~~~~~~~~~~~~~~~~~~~

#. column volume mixing ratio from column mass mixing ratio

   =============== ======================================== ======================== ==========================================
   symbol          description                              unit                     variable name
   =============== ======================================== ======================== ==========================================
   :math:`M_{air}` molar mass for total air                 :math:`\frac{g}{mol}`    `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`q_{x}`   column mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_column_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` column volume mixing ratio of quantity x :math:`ppmv`             `<species>_column_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ======================================== ======================== ==========================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \nu_{x} = q_{x}\frac{M_{air}}{M_{x}}


#. column volume mixing ratio dry air from column mass mixing ratio dry air

   ===================== ======================================== ======================== ==================================================
   symbol                description                              unit                     variable name
   ===================== ======================================== ======================== ==================================================
   :math:`M_{dry\_air}`  molar mass for dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   column mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_column_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` column volume mixing ratio of quantity x :math:`ppmv`             `<species>_column_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ======================================== ======================== ==================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


#. stratospheric column volume mixing ratio dry air from stratospheric column mass mixing ratio dry air

   ===================== ======================================== ======================== ================================================================
   symbol                description                              unit                     variable name
   ===================== ======================================== ======================== ================================================================
   :math:`M_{dry\_air}`  molar mass for dry air                   :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x           :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   stratospheric column mass mixing ratio   :math:`\frac{{\mu}g}{g}` `stratospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` stratospheric column volume mixing ratio :math:`ppmv`             `stratospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================== ======================== ================================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


#. tropospheric column volume mixing ratio dry air from tropospheric column mass mixing ratio dry air

   ===================== ======================================= ======================== ===============================================================
   symbol                description                             unit                     variable name
   ===================== ======================================= ======================== ===============================================================
   :math:`M_{dry\_air}`  molar mass for dry air                  :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x          :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   tropospheric column mass mixing ratio   :math:`\frac{{\mu}g}{g}` `tropospheric_<species>_column_mass_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   :math:`\bar{\nu}_{x}` tropospheric column volume mixing ratio :math:`ppmv`             `tropospheric_<species>_column_volume_mixing_ratio_dry_air {:}`
                         of quantity x with regard to dry air
   ===================== ======================================= ======================== ===============================================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


geopotential
~~~~~~~~~~~~

#. geopotential from geopotential height

   ============= =================== ======================= =========================
   symbol        description         unit                    variable name
   ============= =================== ======================= =========================
   :math:`g_{0}` mean earth gravity  :math:`\frac{m}{s^2}`
   :math:`z_{g}` geopotential height :math:`m`               `geopotential_height {:}`
   :math:`\Phi`  geopotential        :math:`\frac{m^2}{s^2}` `geopotential {:}`
   ============= =================== ======================= =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \Phi = g_{0}z_{g}

#. surface geopotential from surface geopotential height

   =================== =========================== ======================= =================================
   symbol              description                 unit                    variable name
   =================== =========================== ======================= =================================
   :math:`g_{0}`       mean earth gravity          :math:`\frac{m}{s^2}`
   :math:`z_{g,surf}`  surface geopotential height :math:`m`               `surface_geopotential_height {:}`
   :math:`\Phi_{surf}` surface geopotential        :math:`\frac{m^2}{s^2}` `surface_geopotential {:}`
   =================== =========================== ======================= =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \Phi_{surf} = g_{0}z_{g,surf}


geopotential height
~~~~~~~~~~~~~~~~~~~

#. geopotential height from geopotential

   ============= =================== ======================= =========================
   symbol        description         unit                    variable name
   ============= =================== ======================= =========================
   :math:`g_{0}` mean earth gravity  :math:`\frac{m}{s^2}`
   :math:`z_{g}` geopotential height :math:`m`               `geopotential_height {:}`
   :math:`\Phi`  geopotential        :math:`\frac{m^2}{s^2}` `geopotential {:}`
   ============= =================== ======================= =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      z_{g} = \frac{\Phi}{g_{0}}


#. geopotential height from altitude

   ================= ============================ ===================== =========================
   symbol            description                  unit                  variable name
   ================= ============================ ===================== =========================
   :math:`g_{0}`     mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{wgs84}` gravity at WGS84 ellipsoid   :math:`\frac{m}{s^2}`
   :math:`R_{wgs84}` local earth curvature radius :math:`m`
                     at WGS84 ellipsoid
   :math:`z`         altitude                     :math:`m`             `altitude {:}`
   :math:`z_{g}`     geopotential height          :math:`m`             `geopotential_height {:}`
   :math:`\phi`      latitude                     :math:`degN`          `latitude {:}`
   ================= ============================ ===================== =========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{time}`, `{time,vertical}`, or no dimensions at all.

   This equation approximates the mean sea level gravity and radius by that of the reference ellipsoid.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{wgs84} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R_{wgs84} & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z_{g} & = & \frac{g_{wgs84}}{g_{0}}\frac{R_{wgs84}z}{z + R_{wgs84}}
      \end{eqnarray}


#. geopotential height from pressure

   ================== ============================ ================================ ==================================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`hPa`                      `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`hPa`                      `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z_{g}(i)`   geopotential height          :math:`m`                        `geopotential_height {:,vertical}`
   :math:`z_{g,surf}` surface geopotential height  :math:`m`                        `surface_geopotential_height {:}`
   ================== ============================ ================================ ==================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{g,surf}` need to use the same definition of 'surface'.

   The pressures :math:`p(i)` are expected to be at higher levels than the surface pressure (i.e. lower values).
   This should normally be the case since even for pressure grids that start at the surface, :math:`p_{surf}` should
   equal the lower pressure boundary :math:`p^{B}(1,1)`, whereas :math:`p(1)` should then be between :math:`p^{B}(1,1)`
   and :math:`p^{B}(1,2)` (which is generally not equal to :math:`p^{B}(1,1)`).
   
   .. math::
      :nowrap:

      \begin{eqnarray}
         z_{g}(1) & = & z_{g,surf} + \frac{T(1)}{M_{air}(1)}\frac{R}{g_{0}}\ln\left(\frac{p_{surf}}{p(i)}\right) \\
         z_{g}(i) & = & z_{g}(i-1) + \frac{T(i-1)+T(i)}{M_{air}(i-1)+M_{air}(i)}\frac{R}{g_{0}}\ln\left(\frac{p(i-1)}{p(i)}\right), 1 < i \leq N
      \end{eqnarray}


#. surface geopotential height from surface geopotential

   =================== =========================== ======================= =================================
   symbol              description                 unit                    variable name
   =================== =========================== ======================= =================================
   :math:`g_{0}`       mean earth gravity          :math:`\frac{m}{s^2}`
   :math:`z_{g,surf}`  surface geopotential height :math:`m`               `surface_geopotential_height {:}`
   :math:`\Phi_{surf}` surface geopotential        :math:`\frac{m^2}{s^2}` `surface_geopotential {:}`
   =================== =========================== ======================= =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      z_{g,surf} = \frac{\Phi_{surf}}{g_{0}}


#. surface geopotential height from surface altitude

   ================== ============================ ===================== =================================
   symbol             description                  unit                  variable name
   ================== ============================ ===================== =================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{wgs84}`  gravity at WGS84 ellipsoid   :math:`\frac{m}{s^2}`
   :math:`R_{wgs84}`  local earth curvature radius :math:`m`
                      at WGS84 ellipsoid
   :math:`z_{surf}`   surface altitude             :math:`m`             `surface_altitude {:}`
   :math:`z_{g,surf}` surface geopotential height  :math:`m`             `surface_geopotential_height {:}`
   :math:`\phi`       latitude                     :math:`degN`          `latitude {:}`
   ================== ============================ ===================== =================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{time}`, `{time,vertical}`, or no dimensions at all.

   This equation approximates the mean sea level gravity and radius by that of the reference ellipsoid.

   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{wgs84} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {\sqrt{1 - 0.00669437999013{\sin}^2(\frac{\pi}{180}\phi)}} \\
         R_{wgs84} & = & \frac{1}{\sqrt{\left(\frac{\cos(\frac{\pi}{180}\phi)}{6356752.0}\right)^2 +
            \left(\frac{\sin(\frac{\pi}{180}\phi)}{6378137.0}\right)^2}} \\
         z_{g,surf} & = & \frac{g_{wgs84}}{g_{0}}\frac{R_{wgs84}z_{surf}}{z_{surf} + R_{wgs84}}
      \end{eqnarray}



mass density
~~~~~~~~~~~~

#. mass density for air component from number density:

   ================ ================================== ========================= ==============================
   symbol           description                        unit                      variable name
   ================ ================================== ========================= ==============================
   :math:`M_{x}`    molar mass for air component x     :math:`\frac{g}{mol}`
   :math:`n_{x}`    number density for air component x :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`N_A`      Avogadro constant                  :math:`\frac{1}{mol}`
   :math:`\rho_{x}` mass density for air component x   :math:`\frac{ug}{m^3}`    `<species>_density {:}`
                    (e.g. :math:`\rho_{O_{3}}`)
   ================ ================================== ========================= ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \rho_{x} = \frac{10^{6}n_{x}M_{x}}{N_{A}}


#. mass density for total air from number density:

   ================ ============================ ========================= ====================
   symbol           description                  unit                      variable name
   ================ ============================ ========================= ====================
   :math:`M_{air}`  molar mass for total air     :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`        number density for total air :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`      Avogadro constant            :math:`\frac{1}{mol}`
   :math:`\rho`     mass density for total air   :math:`\frac{ug}{m^3}`    `density {:}`
   ================ ============================ ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \rho = \frac{10^{6}n M_{air}}{N_{A}}


#. mass density for air component from column mass density:

   ================== =========================================== ====================== =====================================
   symbol             description                                 unit                   variable name
   ================== =========================================== ====================== =====================================
   :math:`z^{B}(l)`   altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho_{x}`   mass density for air component x            :math:`\frac{ug}{m^3}` `<species>_density {:}`
                      (e.g. :math:`\rho_{O_{3}}`)
   :math:`\sigma_{x}` column mass density for air component x     :math:`\frac{ug}{m^2}` `<species>_column_density {:}`
                      (e.g. :math:`c_{O_{3}}`)
   ================== =========================================== ====================== =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho_{x} = \frac{\sigma_{x}}{\lvert z^{B}(2) - z^{B}(1) \rvert}


#. mass density for total air from column mass density:

   ================ =========================================== ====================== =======================
   symbol           description                                 unit                   variable name
   ================ =========================================== ====================== =======================
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`              `altitude_bounds {:,2}`
   :math:`\rho`     mass density for total air                  :math:`\frac{ug}{m^3}` `density {:}`
   :math:`\sigma`   column mass density for total air           :math:`\frac{ug}{m^2}` `column_density {:}`
   ================ =========================================== ====================== =======================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     \rho = \frac{\sigma}{\lvert z^{B}(2) - z^{B}(1) \rvert}


mass mixing ratio
~~~~~~~~~~~~~~~~~

#. mass mixing ratio from volume mixing ratio

   =============== ================================= ======================== ===================================
   symbol          description                       unit                     variable name
   =============== ================================= ======================== ===================================
   :math:`M_{air}` molar mass for total air          :math:`\frac{g}{mol}`    `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`q_{x}`   mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` volume mixing ratio of quantity x :math:`ppmv`             `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ================================= ======================== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      q_{x} = \nu_{x}\frac{M_{x}}{M_{air}}


#. mass mixing ratio dry air from volume mixing ratio dry air

   ===================== ================================= ======================== ===========================================
   symbol                description                       unit                     variable name
   ===================== ================================= ======================== ===========================================
   :math:`M_{dry\_air}`  molar mass for dry air            :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity x :math:`ppmv`             `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ================================= ======================== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{q}_{x} = \bar{\nu}_{x}\frac{M_{x}}{M_{dry\_air}}


molar mass
~~~~~~~~~~

#. molar mass of total air from density and number density

   =============== ======================= ========================= ====================
   symbol          description             unit                      variable name
   =============== ======================= ========================= ====================
   :math:`M_{air}` molar mass of total air :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`       number density          :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`     Avogadro constant       :math:`\frac{1}{mol}`
   :math:`\rho`    mass density            :math:`\frac{ug}{m^3}`    `density {:}`
   =============== ======================= ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      M = \frac{\rho N_{A}}{10^{6}n}


#. molar mass of total air from H2O mass mixing ratio

   ==================== ======================== ===================== ===========================
   symbol               description              unit                  variable name
   ==================== ======================== ===================== ===========================
   :math:`M_{air}`      molar mass of total air  :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{dry\_air}` molar mass of dry air    :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O        :math:`\frac{g}{mol}`
   :math:`q_{H_{2}O}`   mass mixing ratio of H2O :math:`\frac{ug}{g}`  `H2O_mass_mixing_ratio {:}`
   ==================== ======================== ===================== ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      M_{air} = \frac{M_{H_{2}O}M_{dry\_air}}{\left(1-10^{-6}q_{H_{2}O}\right)M_{H_{2}O} + 10^{-6}q_{H_{2}O}M_{dry\_air}}


#. molar mass of total air from H2O volume mixing ratio

   ==================== ======================== ===================== =============================
   symbol               description              unit                  variable name
   ==================== ======================== ===================== =============================
   :math:`M_{air}`      molar mass of total air  :math:`\frac{g}{mol}` `molar_mass {:}`
   :math:`M_{dry\_air}` molar mass of dry air    :math:`\frac{g}{mol}`
   :math:`M_{H_{2}O}`   molar mass of H2O        :math:`\frac{g}{mol}`
   :math:`\nu_{H_{2}O}` mass mixing ratio of H2O :math:`ppmv`          `H2O_volume_mixing_ratio {:}`
   ==================== ======================== ===================== =============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      M_{air} = M_{dry\_air}\left(1 - 10^{-6}\nu_{H_{2}O}\right) + 10^{-6}M_{H_{2}O}\nu_{H_{2}O}


number density
~~~~~~~~~~~~~~

#. number density for air component from mass density:

   ================ ================================== ========================= ==============================
   symbol           description                        unit                      variable name
   ================ ================================== ========================= ==============================
   :math:`M_{x}`    molar mass for air component x     :math:`\frac{g}{mol}`
   :math:`n_{x}`    number density for air component x :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`N_A`      Avogadro constant                  :math:`\frac{1}{mol}`
   :math:`\rho_{x}` mass density for air component x   :math:`\frac{ug}{m^3}`    `<species>_density {:}`
                    (e.g. :math:`\rho_{O_{3}}`)
   ================ ================================== ========================= ==============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = \frac{\rho_{x}N_{A}}{10^{6}M_{x}}


#. number density for total air from mass density:

   =============== ============================ ========================= ====================
   symbol          description                  unit                      variable name
   =============== ============================ ========================= ====================
   :math:`M_{air}` molar mass for total air     :math:`\frac{g}{mol}`     `molar_mass {:}`
   :math:`n`       number density for total air :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`N_A`     Avogadro constant            :math:`\frac{1}{mol}`
   :math:`\rho`    mass density for total air   :math:`\frac{ug}{m^3}`    `density {:}`
   =============== ============================ ========================= ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n = \frac{\rho N_{A}}{10^{6}M_{air}}


#. number density for total air from pressure and temperature

   ========= ================== ============================ ====================
   symbol    description        unit                         variable name
   ========= ================== ============================ ====================
   :math:`k` Boltzmann constant :math:`\frac{kg m^2}{K s^2}`
   :math:`n` number density     :math:`\frac{molec}{m^3}`    `number_density {:}`
   :math:`p` pressure           :math:`hPa`                  `pressure {:}`
   :math:`T` temperature        :math:`K`                    `temperature {:}`
   ========= ================== ============================ ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n = 10^{-2}\frac{p}{kT}


#. number density from volume mixing ratio

   =============== ======================================= ========================= ==================================
   symbol          description                             unit                      variable name
   =============== ======================================= ========================= ==================================
   :math:`n_{air}` number density of total air             :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{x}`   number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                   (e.g. :math:`n_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppmv`              `<species>_volum_mixing_ratio {:}`
                   (e.g. :math:`n_{O_{3}}`)
   =============== ======================================= ========================= ==================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = 10^{-6}\nu_{x}n_{air}


#. number density from volume mixing ratio dry air

   ===================== ======================================= ========================= ==========================================
   symbol                description                             unit                      variable name
   ===================== ======================================= ========================= ==========================================
   :math:`n_{dry\_air}`  number density of total air             :math:`\frac{molec}{m^3}` `<species>_number_density_dry_air {:}`
   :math:`n_{x}`         number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                         (e.g. :math:`n_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppmv`              `<species>_volum_mixing_ratio_dry_air {:}`
                         (e.g. :math:`n_{O_{3}}`)
   ===================== ======================================= ========================= ==========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = 10^{-6}\bar{\nu}_{x}n_{dry\_air}


#. number density for air component from column number density

   ================ =========================================== ========================= =====================================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= =====================================
   :math:`c_{x}`    column number density for air component x   :math:`\frac{molec}{m^2}` `<species>_column_number_density {:}`
                    (e.g. :math:`c_{O_{3}}`)
   :math:`n_{x}`    number density for air component x          :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                    (e.g. :math:`n_{O_{3}}`)
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= =====================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n_{x} = \frac{c_{x}}{\lvert z^{B}(2) - z^{B}(1) \rvert}


#. number density for total air column number density

   ================ =========================================== ========================= ===========================
   symbol           description                                 unit                      variable name
   ================ =========================================== ========================= ===========================
   :math:`c`        column number density for air component x   :math:`\frac{molec}{m^2}` `column_number_density {:}`
   :math:`n`        number density for air component x          :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`z^{B}(l)` altitude boundaries (:math:`l \in \{1,2\}`) :math:`m`                 `altitude_bounds {:,2}`
   ================ =========================================== ========================= ===========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

     n = \frac{c}{\lvert z^{B}(2) - z^{B}(1) \rvert}


#. number density for air component from partial pressure and temperature

   ============= ==================================== ============================ ================================
   symbol        description                          unit                         variable name
   ============= ==================================== ============================ ================================
   :math:`k`     Boltzmann constant                   :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{x}` number density for air component x   :math:`\frac{molec}{m^3}`    `<species>_number_density {:}`
                 (e.g. :math:`n_{O_{3}}`)
   :math:`p_{x}` partial pressure for air component x :math:`hPa`                  `<species>_partial_pressure {:}`
                 (e.g. :math:`p_{O_{3}}`)
   :math:`T`     temperature                          :math:`K`                    `temperature {:}`
   ============= ==================================== ============================ ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{x} = 10^{-2}\frac{p_{x}}{kT}


#. surface number density for total air from surface pressure and surface temperature

   ================ ====================== ============================ ============================
   symbol           description            unit                         variable name
   ================ ====================== ============================ ============================
   :math:`k`        Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{surf}` surface number density :math:`\frac{molec}{m^3}`    `surface_number_density {:}`
   :math:`p_{surf}` surface pressure       :math:`hPa`                  `surface_pressure {:}`
   :math:`T_{surf}` surface temperature    :math:`K`                    `surface_temperature {:}`
   ================ ====================== ============================ ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      n_{surf}= 10^{-2}\frac{p_{surf}}{kT_{surf}}


partial pressure
~~~~~~~~~~~~~~~~

#. partial pressure from number density and temperature

   ============= ==================================== ============================ ================================
   symbol        description                          unit                         variable name
   ============= ==================================== ============================ ================================
   :math:`k`     Boltzmann constant                   :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{x}` number density for air component x   :math:`\frac{molec}{m^3}`    `<species>_number_density {:}`
                 (e.g. :math:`n_{O_{3}}`)
   :math:`p_{x}` partial pressure for air component x :math:`hPa`                  `<species>_partial_pressure {:}`
                 (e.g. :math:`p_{O_{3}}`)
   :math:`T`     temperature                          :math:`K`                    `temperature {:}`
   ============= ==================================== ============================ ================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{x} = 10^{2}n_{x}kT


#. partial pressure from volume mixing ratio

   =============== ======================================= ============ ===================================
   symbol          description                             unit         variable name
   =============== ======================================= ============ ===================================
   :math:`p`       pressure                                :math:`hPa`  `pressure {:}`
   :math:`p_{x}`   partial pressure for air component x    :math:`hPa`  `<species>_partial_pressure {:}`
                   (e.g. :math:`p_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppmv` `<species>_volume_mixing_ratio {:}`
                   (e.g. :math:`\nu_{O_{3}}`)
   =============== ======================================= ============ ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{x} = 10^{-6}\nu_{x}p


#. partial pressure from volume mixing ratio dry air

   ===================== ======================================= ============ ===========================================
   symbol                description                             unit         variable name
   ===================== ======================================= ============ ===========================================
   :math:`p_{x}`         partial pressure for air component x    :math:`hPa`  `<species>_partial_pressure {:}`
                         (e.g. :math:`p_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppmv` `<species>_volume_mixing_ratio_dry_air {:}`
                         (e.g. :math:`\nu_{O_{3}}`)
   ===================== ======================================= ============ ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{x} = 10^{-6}\bar{\nu}_{x}p_{dry\_air}


pressure
~~~~~~~~

#. pressure from bounds

   ================ =========================================== =========== =======================
   symbol           description                                 unit        variable name
   ================ =========================================== =========== =======================
   :math:`p`        pressure                                    :math:`hPa` `pressure {:}`
   :math:`p^{B}(l)` pressure boundaries (:math:`l \in \{1,2\}`) :math:`hPa` `pressure_bounds {:,2}`
   ================ =========================================== =========== =======================

   The pattern `:` for the dimensions can represent `{vertical}`, or `{time,vertical}`.

   .. math::

      p = e^{\frac{ln(z^{B}(2)) + ln(z^{B}(1))}{2}}


#. pressure from altitude

   ================== ============================ ================================ ==========================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==========================
   :math:`a`          WGS84 semi-major axis        :math:`m`
   :math:`b`          WGS84 semi-minor axis        :math:`m`
   :math:`f`          WGS84 flattening             :math:`m`
   :math:`g`          gravity                      :math:`\frac{m}{s^2}`
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`g_{surf}`   gravity at surface           :math:`\frac{m}{s^2}`
   :math:`GM`         WGS84 earth's gravitational  :math:`\frac{m^3}{s^2}`
                      constant
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`hPa`                      `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`hPa`                      `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z(i)`       altitude                     :math:`m`                        `altitude {:,vertical}`
   :math:`z_{surf}`   surface height               :math:`m`                        `surface_altitude {:}`
   :math:`\phi`       latitude                     :math:`degN`                     `latitude {:}`
   :math:`\omega`     WGS84 earth angular velocity :math:`rad/s`
   ================== ============================ ================================ ==========================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{surf}` need to use the same definition of 'surface'.

   The altitudes :math:`z(i)` are expected to be above the surface height. This should normally be the case
   since even for altitude grids that start at the surface, :math:`z_{surf}` should equal the lower altitude boundary
   :math:`z^{B}(1,1)`, whereas :math:`z(1)` should then be between :math:`z^{B}(1,1)` and :math:`z^{B}(1,2)`
   (which is generally not equal to :math:`z^{B}(1,1)`).
   
   .. math::
      :nowrap:

      \begin{eqnarray}
         g_{surf} & = & 9.7803253359 \frac{1 + 0.00193185265241{\sin}^2(\frac{\pi}{180}\phi)}
            {1 - 0.00669437999013 {\sin}^2(\frac{\pi}{180}\phi)} \\
         m & = & \frac{\omega^2a^2b}{GM} \\
         g(1) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)\frac{z_{surf}+z(1)}{2} + \frac{3}{a^2}\left(\frac{z_{surf}+z(1)}{2}\right)^2\right) \\
         g(i) & = & g_{surf} \left(1 - \frac{2}{a}\left(1+f+m-2f{\sin}^2(\frac{\pi}{180}\phi)\right)\frac{z(i-1)+z(i)}{2} + \frac{3}{a^2}\left(\frac{z(i-1)+z(i)}{2}\right)^2\right), 1 < i \leq N \\
         p(1) & = & p_{surf}e^{-\frac{M_{air}(1)}{T(1)}\frac{g(1)}{R}\left(z(i)-z_{surf}\right)} \\
         p(i) & = & p(i-1)e^{-\frac{M_{air}(i-1)+M_{air}(i)}{T(i-1)+T(i)}\frac{g(i)}{R}\left(z(i)-z(i-1)\right)}, 1 < i \leq N
      \end{eqnarray}


#. pressure from geopotential height

   ================== ============================ ================================ ==================================
   symbol             description                  unit                             variable name
   ================== ============================ ================================ ==================================
   :math:`g_{0}`      mean earth gravity           :math:`\frac{m}{s^2}`
   :math:`M_{air}(i)` molar mass of total air      :math:`\frac{g}{mol}`            `molar_mass {:,vertical}`
   :math:`p(i)`       pressure                     :math:`hPa`                      `pressure {:,vertical}`
   :math:`p_{surf}`   surface pressure             :math:`hPa`                      `surface_pressure {:}`
   :math:`R`          universal gas constant       :math:`\frac{kg m^2}{K mol s^2}`
   :math:`T(i)`       temperature                  :math:`K`                        `temperature {:,vertical}`
   :math:`z_{g}(i)`   geopotential height          :math:`m`                        `geopotential_height {:,vertical}`
   :math:`z_{g,surf}` surface geopotential height  :math:`m`                        `surface_geopotential_height {:}`
   ================== ============================ ================================ ==================================

   The pattern `:` for the dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   The surface pressure :math:`p_{surf}` and surface height :math:`z_{g,surf}` need to use the same definition of 'surface'.

   The geopotential heights :math:`z_{g}(i)` are expected to be above the surface geopotential height. This should
   normally be the case since even for geopotential height grids that start at the surface, :math:`z_{g,surf}` should
   equal the lower altitude boundary :math:`z^{B}_{g}(1,1)`, whereas :math:`z_{g}(1)` should then be between
   :math:`z^{B}_{g}(1,1)` and :math:`z^{B}_{g}(1,2)` (which is generally not equal to :math:`z^{B}_{g}(1,1)`).
   
   .. math::
      :nowrap:

      \begin{eqnarray}
         p(1) & = & p_{surf}e^{-\frac{M_{air}(1)}{T(1)}\frac{g_{0}}{R}\left(z_{g}(i)-z_{g,surf}\right)} \\
         p(i) & = & p(i-1)e^{-\frac{M_{air}(i-1)+M_{air}(i)}{T(i-1)+T(i)}\frac{g_{0}}{R}\left(z_{g}(i)-z_{g}(i-1)\right)}, 1 < i \leq N
      \end{eqnarray}


#. pressure from number density and temperature

   ========= ================== ============================ ====================
   symbol    description        unit                         variable name
   ========= ================== ============================ ====================
   :math:`k` Boltzmann constant :math:`\frac{kg m^2}{K s^2}`
   :math:`n` number density     :math:`\frac{molec}{m^3}`    `number_density {:}`
   :math:`p` pressure           :math:`hPa`                  `pressure {:}`
   :math:`T` temperature        :math:`K`                    `temperature {:}`
   ========= ================== ============================ ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p = 10^{2}nkT


#. surface pressure from surface number density and surface temperature

   ================ ====================== ============================ ============================
   symbol           description            unit                         variable name
   ================ ====================== ============================ ============================
   :math:`k`        Boltzmann constant     :math:`\frac{kg m^2}{K s^2}`
   :math:`n_{surf}` surface number density :math:`\frac{molec}{m^3}`    `surface_number_density {:}`
   :math:`p_{surf}` surface pressure       :math:`hPa`                  `surface_pressure {:}`
   :math:`T_{surf}` surface temperature    :math:`K`                    `surface_temperature {:}`
   ================ ====================== ============================ ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      p_{surf} = 10^{2}n_{surf}kT_{surf}


pressure bounds
~~~~~~~~~~~~~~~

#. pressure ranges from midpoints

   ================== =========================================== =========== ================================
   symbol             description                                 unit         variable name
   ================== =========================================== =========== ================================
   :math:`p(i)`       pressure                                    :math:`hPa` `pressure {:,vertical}`
   :math:`p^{B}(i,l)` pressure boundaries (:math:`l \in \{1,2\}`) :math:`hPa` `pressure_bounds {:,vertical,2}`
   ================== =========================================== =========== ================================

   The pattern `:` for the dimensions can represent `{time}`, or no dimension at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         p^{B}(1,1) & = & e^{2ln(p(1)) - ln(p(2))} \\
         p^{B}(i,1) & = & e^{\frac{ln(p(i-1)) + ln(p(i))}{2}}, 1 < i \leq N \\
         p^{B}(i,2) & = & e^{ln(p^{B}(i+1,1))}, 1 \leq i < N \\
         p^{B}(N,2) & = & e^{2ln(p(N)) - ln(p(N-1))}
      \end{eqnarray}


relative humidity
~~~~~~~~~~~~~~~~~

#. relative humidity from H2O partial pressure

   ================== ============================== ======================= ==========================
   symbol             description                    unit                    variable name
   ================== ============================== ======================= ==========================
   :math:`e_{w}`      saturated water vapor pressure :math:`hPa`
   :math:`p_{H_{2}O}` partial pressure of H2O        :math:`hPa`             `H2O_partial_pressure {:}`
   :math:`T`          temperature                    :math:`K`               `temperature {:}`
   :math:`\phi`       relative humidity              :math:`\frac{hPa}{hPa}` `relative_humidity {:}`
   ================== ============================== ======================= ==========================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::
      :nowrap:

      \begin{eqnarray}
         e_{w} & = & 6.1094e^{\frac{17.625(T-273.15)}{(T-273.15)+243.04}} \\
         \phi & = & \frac{p_{H_{2}O}}{e_{w}}
      \end{eqnarray}


temperature
~~~~~~~~~~~

#. temperature from pressure and number density

   ========= ================== ============================ ====================
   symbol    description        unit                         variable name
   ========= ================== ============================ ====================
   :math:`k` Boltzmann constant :math:`\frac{kg m^2}{K s^2}`
   :math:`n` number density     :math:`\frac{molec}{m^3}`    `number_density {:}`
   :math:`p` pressure           :math:`hPa`                  `pressure {:}`
   :math:`T` temperature        :math:`K`                    `temperature {:}`
   ========= ================== ============================ ====================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      T = 10^{-2}\frac{p}{kn}


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
   :math:`p_{surf}` surface pressure       :math:`hPa`                  `surface_pressure {:}`
   :math:`T_{surf}` surface temperature    :math:`K`                    `surface_temperature {:}`
   ================ ====================== ============================ ============================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      T_{surf} = 10^{-2}\frac{p_{surf}}{kn_{surf}}


virtual temperature
~~~~~~~~~~~~~~~~~~~

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


volume mixing ratio
~~~~~~~~~~~~~~~~~~~

#. volume mixing ratio from number density

   =============== ======================================= ========================= ==================================
   symbol          description                             unit                      variable name
   =============== ======================================= ========================= ==================================
   :math:`n_{air}` number density of total air             :math:`\frac{molec}{m^3}` `number_density {:}`
   :math:`n_{x}`   number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                   (e.g. :math:`n_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppmv`              `<species>_volum_mixing_ratio {:}`
                   (e.g. :math:`n_{O_{3}}`)
   =============== ======================================= ========================= ==================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = 10^{6}\frac{n_{x}}{n_{air}}


#. volume mixing ratio from mass mixing ratio

   =============== ================================= ======================== ===================================
   symbol          description                       unit                     variable name
   =============== ================================= ======================== ===================================
   :math:`M_{air}` molar mass for total air          :math:`\frac{g}{mol}`    `molar_mass {:}`
   :math:`M_{x}`   molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`q_{x}`   mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_mass_mixing_ratio {:}`
                   with regard to total air
   :math:`\nu_{x}` volume mixing ratio of quantity x :math:`ppmv`             `<species>_volume_mixing_ratio {:}`
                   with regard to total air
   =============== ================================= ======================== ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = q_{x}\frac{M_{air}}{M_{x}}


#. volume mixing ratio from partial pressure

   =============== ======================================= ============ ===================================
   symbol          description                             unit         variable name
   =============== ======================================= ============ ===================================
   :math:`p`       pressure                                :math:`hPa`  `pressure {:}`
   :math:`p_{x}`   partial pressure for air component x    :math:`hPa`  `<species>_partial_pressure {:}`
                   (e.g. :math:`p_{O_{3}}`)
   :math:`\nu_{x}` volume mixing ratio for air component x :math:`ppmv` `<species>_volume_mixing_ratio {:}`
                   (e.g. :math:`\nu_{O_{3}}`)
   =============== ======================================= ============ ===================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \nu_{x} = 10^{6}\frac{p_{x}}{p}


#. volume mixing ratio dry air from number density

   ===================== ======================================= ========================= ==========================================
   symbol                description                             unit                      variable name
   ===================== ======================================= ========================= ==========================================
   :math:`n_{dry\_air}`  number density of total air             :math:`\frac{molec}{m^3}` `<species>_number_density_dry_air {:}`
   :math:`n_{x}`         number density for air component x      :math:`\frac{molec}{m^3}` `<species>_number_density {:}`
                         (e.g. :math:`n_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppmv`              `<species>_volum_mixing_ratio_dry_air {:}`
                         (e.g. :math:`n_{O_{3}}`)
   ===================== ======================================= ========================= ==========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = 10^{6}\frac{n_{x}}{n_{dry\_air}}


#. volume mixing ratio dry air from mass mixing ratio dry air

   ===================== ================================= ======================== ===========================================
   symbol                description                       unit                     variable name
   ===================== ================================= ======================== ===========================================
   :math:`M_{dry\_air}`  molar mass for dry air            :math:`\frac{g}{mol}`
   :math:`M_{x}`         molar mass for air component x    :math:`\frac{g}{mol}`
   :math:`\bar{q}_{x}`   mass mixing ratio of quantity x   :math:`\frac{{\mu}g}{g}` `<species>_mass_mixing_ratio_dry_air {:}`
                         with regard to dry air
   :math:`\bar{\nu}_{x}` volume mixing ratio of quantity x :math:`ppmv`             `<species>_volume_mixing_ratio_dry_air {:}`
                         with regard to dry air
   ===================== ================================= ======================== ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = \bar{q}_{x}\frac{M_{dry\_air}}{M_{x}}


#. volume mixing ratio dry air from partial pressure

   ===================== ======================================= ============ ===========================================
   symbol                description                             unit         variable name
   ===================== ======================================= ============ ===========================================
   :math:`p_{x}`         partial pressure for air component x    :math:`hPa`  `<species>_partial_pressure {:}`
                         (e.g. :math:`p_{O_{3}}`)
   :math:`\bar{\nu}_{x}` volume mixing ratio for air component x :math:`ppmv` `<species>_volume_mixing_ratio_dry_air {:}`
                         (e.g. :math:`\nu_{O_{3}}`)
   ===================== ======================================= ============ ===========================================

   The pattern `:` for the dimensions can represent `{vertical}`, `{latitude,longitude}`, `{latitude,longitude,vertical}`,
   `{time}`, `{time,vertical}`, `{time,latitude,longitude}`, `{time,latitude,longitude,vertical}`, or no dimensions at all.

   .. math::

      \bar{\nu}_{x} = 10^{6}\frac{p_{x}}{p_{dry\_air}}


Optical variable conversions
----------------------------

frequency
~~~~~~~~~

#. frequency from wavelength

   =============== ============== =================== ================
   symbol          description    unit                variable name
   =============== ============== =================== ================
   :math:`c`       speed of light :math:`\frac{m}{s}`
   :math:`\lambda` wavelength     :math:`nm`          `wavelength {:}`
   :math:`\nu`     frequency      :math:`Hz`          `frequency {:}`
   =============== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \nu = 10^{9}\frac{c}{\lambda}


#. frequency from wavenumber

   =================== ============== ==================== ================
   symbol              description    unit                 variable name
   =================== ============== ==================== ================
   :math:`c`           speed of light :math:`\frac{m}{s}`
   :math:`\nu`         frequency      :math:`Hz`           `frequency {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{cm}` `wavenumber {:}`
   =================== ============== ==================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \nu = 10^{2}c\tilde{\nu}


wavelength
~~~~~~~~~~

#. wavelength from frequency

   =============== ============== =================== ================
   symbol          description    unit                variable name
   =============== ============== =================== ================
   :math:`c`       speed of light :math:`\frac{m}{s}`
   :math:`\nu`     frequency      :math:`Hz`           `frequency {:}`
   :math:`\lambda` wavelength     :math:`nm`          `wavelength {:}`
   =============== ============== =================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \lambda = 10^{9}\frac{c}{\nu}


#. wavelength from wavenumber

   =================== ============== ==================== ================
   symbol              description    unit                 variable name
   =================== ============== ==================== ================
   :math:`\lambda`     wavelength     :math:`nm`           `wavelength {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{cm}` `wavenumber {:}`
   =================== ============== ==================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \lambda = \frac{10^{-7}}{\tilde{\nu}}


wavenumber
~~~~~~~~~~

#. wavenumber from frequency

   =================== ============== ==================== ================
   symbol              description    unit                 variable name
   =================== ============== ==================== ================
   :math:`c`           speed of light :math:`\frac{m}{s}`
   :math:`\nu`         frequency      :math:`Hz`           `frequency {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{cm}` `wavenumber {:}`
   =================== ============== ==================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \tilde{\nu} = 10^{-2}\frac{\nu}{c}


#. wavenumber from wavelength

   =================== ============== ==================== ================
   symbol              description    unit                 variable name
   =================== ============== ==================== ================
   :math:`\lambda`     wavelength     :math:`nm`           `wavelength {:}`
   :math:`\tilde{\nu}` wavenumber     :math:`\frac{1}{cm}` `wavenumber {:}`
   =================== ============== ==================== ================

   The pattern `:` for the first dimensions can represent `{spectral}`, `{time}`, `{time,spectral}`, or no dimensions at all.

   .. math::

      \tilde{\nu} = \frac{10^{-7}}{\lambda}

