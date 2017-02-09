common derivations
==================

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
