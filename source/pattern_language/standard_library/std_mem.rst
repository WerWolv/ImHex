``std::mem``
============

| This namespace contains functions for interacting with the currently loaded data
|

------------------------

``std::mem::align_to(alignment, value)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Aligns a given value upwards to the next valid value**

.. table::
    :align: left

    ============= ==================================================
    Parameter     Description
    ============= ==================================================
    ``alignment`` Alignment of new value
    ``value``     Value to align
    ``return``    Aligned value
    ============= ==================================================

------------------------

``std::mem::base_address()``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the current base address of the loaded data**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``return``      Current base address
    =============== =========================================================================

------------------------

``std::mem::size()``
^^^^^^^^^^^^^^^^^^^^

**Gets the size of the loaded data**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``return``      Size of the loaded data
    =============== =========================================================================

------------------------

``std::mem::find_sequence(occurence_index, bytes...)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the size of the loaded data**


.. table::
    :align: left

    =================== ===========================================================================
    Parameter           Description
    =================== ===========================================================================
    ``occurence_index`` Index of found sequence if there's more than one. Zero yields the first one
    ``bytes...``        One or more bytes to search for
    ``return``          Start address of the sequence
    =================== ===========================================================================

------------------------

``std::mem::read_unsigned(address, size)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Reads an unsigned value from memory without declaring a variable**


.. table::
    :align: left

    =================== ===========================================================================
    Parameter           Description
    =================== ===========================================================================
    ``address``         Address of value to read
    ``size``            Size of value to read. This can be 1, 2, 4, 8 or 16
    ``return``          Value as the smalest unsigned type that can fit this many bytes
    =================== ===========================================================================

------------------------

``std::mem::read_signed(address, size)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Reads an signed value from memory without declaring a variable**


.. table::
    :align: left

    =================== ===========================================================================
    Parameter           Description
    =================== ===========================================================================
    ``address``         Address of value to read
    ``size``            Size of value to read. This can be 1, 2, 4, 8 or 16
    ``return``          Value as the smalest signed type that can fit this many bytes
    =================== ===========================================================================