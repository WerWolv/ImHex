``std::fxpt`` :version:`1.10.1`
================================

| This namespace contains various fixed-point calculation functions
|

------------------------

Types
-----

``fixed``
^^^^^^^^^

**A type to represent a fixed-point number**

Functions
---------

``std::fxpt::to_float(fixed fxt, u32 precision) -> double`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Converts a fixed point number ``fxt`` with a precision of ``precision`` bits into a floating point number**

.. table::
    :align: left

    ============== =========================================================
    Parameter      Description
    ============== =========================================================
    ``fxt``        Fixed point number
    ``precision``  Bits of precision
    ``return``     Floating point representation of ``fxt``
    ============== =========================================================

------------------------

``std::fxpt::to_fixed(double flt, u32 precision) -> fixed`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Converts a floating point number ``flt`` into a fixed point number with a precision of ``precision`` bits**

.. table::
    :align: left

    ============== =========================================================
    Parameter      Description
    ============== =========================================================
    ``flt``        Floating point number
    ``precision``  Bits of precision
    ``return``     Fixed point representation of ``flt``
    ============== =========================================================

------------------------

``std::fxpt::change_precision(fixed value, u32 start_precision, u32 end_precision) -> fixed`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Converts a fixed point number ``value`` with a precision of ``start_precision`` bits into a fixed point number with a precision of ``end_precision`` bits**

.. table::
    :align: left

    =================== =========================================================
    Parameter            Description
    =================== =========================================================
    ``value``           Fixed point number
    ``start_precision`` Bits of precision of ``value``
    ``end_precision``   Bits of precision of the result value
    ``return``          Fixed point number with adjusted precision
    =================== =========================================================

------------------------

``std::fxpt::add(fixed a, fixed b, u32 precision) -> fixed`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Performs a fixed point addition of values ``a`` and ``b`` with a precision of ``precision`` bits**

.. warning::
    
    Both ``a`` and ``b`` need to have the same precision

.. table::
    :align: left

    ============== =========================================================
    Parameter      Description
    ============== =========================================================
    ``a``          Left operand
    ``b``          Right operand
    ``precision``  Bits of precision of both ``a`` and ``b``
    ``return``     Result of ``a`` + ``b``
    ============== =========================================================

------------------------

``std::fxpt::subtract(fixed a, fixed b, u32 precision) -> fixed`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Performs a fixed point subtraction of values ``a`` and ``b`` with a precision of ``precision`` bits**

.. warning::
    
    Both ``a`` and ``b`` need to have the same precision

.. table::
    :align: left

    ============== =========================================================
    Parameter      Description
    ============== =========================================================
    ``a``          Left operand
    ``b``          Right operand
    ``precision``  Bits of precision of both ``a`` and ``b``
    ``return``     Result of ``a`` - ``b``
    ============== =========================================================

------------------------

``std::fxpt::multiply(fixed a, fixed b, u32 precision) -> fixed`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Performs a fixed point multiplication of values ``a`` and ``b`` with a precision of ``precision`` bits**

.. warning::
    
    Both ``a`` and ``b`` need to have the same precision

.. table::
    :align: left

    ============== =========================================================
    Parameter      Description
    ============== =========================================================
    ``a``          Left operand
    ``b``          Right operand
    ``precision``  Bits of precision of both ``a`` and ``b``
    ``return``     Result of ``a`` * ``b``
    ============== =========================================================

------------------------

``std::fxpt::divide(fixed a, fixed b, u32 precision) -> fixed`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Performs a fixed point division of values ``a`` and ``b`` with a precision of ``precision`` bits**

.. warning::
    
    Both ``a`` and ``b`` need to have the same precision

.. table::
    :align: left

    ============== =========================================================
    Parameter      Description
    ============== =========================================================
    ``a``          Left operand
    ``b``          Right operand
    ``precision``  Bits of precision of both ``a`` and ``b``
    ``return``     Result of ``a`` / ``b``
    ============== =========================================================