column number density averaging kernel derivations
==================================================

#. column number density column AVK of air component from column number density AVK:

   ====================== =============================================== =================================== ===========================================================
   symbol                 description                                     unit                                variable name
   ====================== =============================================== =================================== ===========================================================
   :math:`A^{c}_{x}(i)`   column AVK of column number density profile of  :math:`\frac{molec/m^2}{molec/m^2}` `<species>_column_number_density_avk {:,vertical}`
                          air component x (e.g. :math:`A^{c}_{O_{3}}(i)`)
   :math:`A^{c}_{x}(i,j)` AVK of column number density profile of air     :math:`\frac{molec/m^2}{molec/m^2}` `<species>_column_number_density_avk {:,vertical,vertical}`
                          component x (e.g. :math:`A^{c}_{O_{3}}(i,j)`)
   ====================== =============================================== =================================== ===========================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      A^{c}_{x}(i) = \sum_{j}{A^{c}_{x}(j,i)}


#. column number density AVK of air component from number density AVK:

   ====================== ============================================= =================================== ===========================================================
   symbol                 description                                   unit                                variable name
   ====================== ============================================= =================================== ===========================================================
   :math:`A^{c}_{x}(i,j)` AVK of column number density profile of air   :math:`\frac{molec/m^2}{molec/m^2}` `<species>_column_number_density_avk {:,vertical,vertical}`
                          component x (e.g. :math:`A^{c}_{O_{3}}(i,j)`)
   :math:`A^{n}_{x}(i,j)` AVK of number density profile of air          :math:`\frac{molec/m^3}{molec/m^3}` `<species>_number_density_avk {:,vertical,vertical}`
                          component x (e.g. :math:`A^{n}_{O_{3}}(i,j)`)
   :math:`z^{B}(i,l)`     altitude boundaries (:math:`l \in \{1,2\}`)   :math:`m`                           `altitude_bounds {:,vertical,2}`
   ====================== ============================================= =================================== ===========================================================

   The pattern `:` for the first dimensions can represent `{latitude,longitude}`, `{time}`, `{time,latitude,longitude}`,
   or no dimensions at all.

   .. math::

      A^{c}_{x}(i,j) = \begin{cases}
        z^{B}(j,1) \neq z^{B}(j,2), & A^{n}_{x}(i,j) \frac{\lvert z^{B}(i,2) - z^{B}(i,1) \rvert}{\lvert z^{B}(j,2) - z^{B}(j,1) \rvert} \\
        z^{B}(j,1) \neq z^{B}(j,2), & 0
      \end{cases}
