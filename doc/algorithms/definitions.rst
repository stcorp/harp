Definitions
===========

Leading dimensions
------------------

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
---------

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
:math:`p_{0}`       standard pressure            :math:`Pa`                       :math:`101325`
:math:`R`           universal gas constant       :math:`\frac{kg m^2}{K mol s^2}` :math:`8.3144598`
:math:`T_{0}`       standard temperature         :math:`K`                        :math:`273.15`
:math:`\omega`      WGS84 earth angular velocity :math:`rad/s`                    :math:`7292115.0\cdot10^{-11}`
=================== ============================ ================================ ===============================


Molar mass
----------

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
CCl4    153.822
CF4     88.00431
CHClF2  86.4684
CH3Cl   50.48752
CH3CN   41.05192
CH3OH   32.04186
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
NH3     17.03056
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

