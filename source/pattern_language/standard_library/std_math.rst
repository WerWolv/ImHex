``std::math`` :version:`1.10.1`
================================

| This namespace contains various math functions
|

------------------------

Functions
---------

``std::math::min(auto a, auto b) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the smaller number between ``a`` and ``b``**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``a``       First value
    ``b``       Second value
    ``return``  ``a`` if ``a`` is smaller than ``b``, otherwise ``b``
    =========== =========================================================

------------------------

``std::math::max(auto a, auto b) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the larger number between ``a`` and ``b``**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``a``       First value
    ``b``       Second value
    ``return``  ``a`` if ``a`` is larger than ``b``, otherwise ``b``
    =========== =========================================================

------------------------

``std::math::clamp(auto x, auto min, auto max) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Clamps the value ``x`` between ``min`` and ``max``**

.. table::
    :align: left

    =========== ==================================================================================================
    Parameter   Description
    =========== ==================================================================================================
    ``x``       Value
    ``min``     Minimal value allowed
    ``max``     Maximal value allowed
    ``return``  ``min`` if ``x`` is smaller than ``min``, ``max`` if ``x`` is larger than ``max``, otherwise ``x``
    =========== ==================================================================================================

------------------------

``std::math::abs(auto x) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the absolute value of ``x``**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``x``       Value
    ``return``  ``x`` if ``x`` is bigger than zero, otherwise ``-x``
    =========== ====================================================