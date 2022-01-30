``std::bit`` :version:`1.10.1`
===============================

.. code-block:: hexpat

    #include <std/bit.pat>

| This namespace contains various bit-level operations
|

------------------------

Functions
---------

``std::bit::popcount(u128 value) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns the number of 1 bits in a value**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``value``   Integer value
    ``return``  Number of bits of the value set to 1
    =========== =========================================================

------------------------

``std::bit::has_single_bit(u128 value) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Returns true if only a single bit is set in the passed ``value``**

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``value``   Integer value
    ``return``  True if only one bit is set in ``value``, false otherwise
    =========== =========================================================

------------------------

``std::bit::bit_ceil(u128 value) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the smallest integral power of two that is not smaller than ``value``**

.. table::
    :align: left

    =========== =================================================================
    Parameter   Description
    =========== =================================================================
    ``value``   Integer value
    ``return``  Smallest integral power of two that is not smaller than ``value``
    =========== =================================================================

------------------------

``std::bit::bit_floor(u128 value) -> u128`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the largest integral power of two that is not greater than ``value``**

.. table::
    :align: left

    =========== =================================================================
    Parameter   Description
    =========== =================================================================
    ``value``   Integer value
    ``return``  Largest integral power of two that is not greater than ``value``
    =========== =================================================================