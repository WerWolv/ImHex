``std::math`` :version:`1.10.1`
================================

.. code-block:: hexpat

    #include <std/math.pat>

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

------------------------

``std::math::sign(auto x) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the sign of a value**

.. table::
    :align: left

    =========== ======================================================
    Parameter   Description
    =========== ======================================================
    ``x``       Value
    ``return``  ``-1`` if ``x`` is smaller than ``0``, ``1`` otherwise
    =========== ======================================================

------------------------

``std::math::copy_sign(auto x, auto y) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Applies the sign of y and applies it to x**

.. table::
    :align: left

    =========== ======================================================
    Parameter   Description
    =========== ======================================================
    ``x``       Value
    ``y``       Sign value
    ``return``  ``-x`` if ``y`` is smaller than ``0``, ``x`` otherwise
    =========== ======================================================

------------------------

``std::math::factorial(auto x) -> auto`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the factorial of x**

.. table::
    :align: left

    =========== ======================================================
    Parameter   Description
    =========== ======================================================
    ``x``       Value
    ``return``  Factorial of ``x``
    =========== ======================================================

------------------------

``std::math::comb(u128 n, u128 k) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the binomial coefficient of k and n. (n-choose-k)**

.. table::
    :align: left

    =========== ======================================================
    Parameter   Description
    =========== ======================================================
    ``n``       n value
    ``k``       k value
    ``return``  n-choose-k
    =========== ======================================================

------------------------

``std::math::perm(u128 n, u128 k) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the number of ways to choose k items from n items without repetition and with order**

.. table::
    :align: left

    =========== ======================================================
    Parameter   Description
    =========== ======================================================
    ``n``       n value
    ``k``       k value
    ``return``  Result
    =========== ======================================================

------------------------

``std::math::floor(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Floors the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Value rounded down to the next integer
    =========== ====================================================

------------------------

``std::math::ceil(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Ceils the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Value rounded up to the next integer
    =========== ====================================================

------------------------

``std::math::round(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Rounds the value**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``value``   Value
    ``return``  Value rounded towards the next integer rounding up at 0.5
    =========== =========================================================

------------------------

``std::math::trunc(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Truncates the fractional part of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Value with the fractional part removed
    =========== ====================================================

------------------------

``std::math::log10(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the log with base 10 of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  ``log`` with base ``10`` of the value
    =========== ====================================================

------------------------

``std::math::log2(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the log with base 2 of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  ``log`` with base ``2`` of the value
    =========== ====================================================

------------------------

``std::math::ln(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the log with base e of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  ``log`` with base ``e`` of the value
    =========== ====================================================

------------------------

``std::math::fmod(auto x, auto y) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the floating-point remainder of the division of x / y**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``x``       Value 1
    ``y``       Value 2
    ``return``  Remainder of ``x / y``
    =========== ====================================================

------------------------

``std::math::pow(auto base, auto exp) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the floating-point remainder of the division of x / y**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``base``    Base
    ``exp``     Exponent
    ``return``  ``base`` raised to the ``exp`` th power 
    =========== ====================================================

------------------------

``std::math::sqrt(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the square root of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Square root of value
    =========== ====================================================

------------------------

``std::math::cbrt(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the cube root of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Cube root of value
    =========== ====================================================

------------------------

``std::math::sin(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the sine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Sine of value
    =========== ====================================================

------------------------

``std::math::cos(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the cosine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Cosine of value
    =========== ====================================================

------------------------

``std::math::tan(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the tangent of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Tangent of value
    =========== ====================================================

------------------------

``std::math::asin(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the arc-sine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Arc-sine of value
    =========== ====================================================

------------------------

``std::math::acos(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the arc-cosine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Arc-cosine of value
    =========== ====================================================

------------------------

``std::math::atan(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the arc-tangent of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Arc-tangent of value
    =========== ====================================================

------------------------

``std::math::sinh(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the hyperbolic sine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Sine of value
    =========== ====================================================

------------------------

``std::math::cosh(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the hyperbolic cosine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Cosine of value
    =========== ====================================================

------------------------

``std::math::tanh(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the hyperbolic tangent of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Tangent of value
    =========== ====================================================

------------------------

``std::math::asinh(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the hyperbolic arc-sine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Arc-sine of value
    =========== ====================================================

------------------------

``std::math::acosh(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the hyperbolic arc-cosine of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Arc-cosine of value
    =========== ====================================================

------------------------

``std::math::atanh(auto value) -> auto`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the hyperbolic arc-tangent of the value**

.. table::
    :align: left

    =========== ====================================================
    Parameter   Description
    =========== ====================================================
    ``value``   Value
    ``return``  Arc-tangent of value
    =========== ====================================================