``std::mem``
============

.. code-block:: hexpat

    #include <std/mem.pat>

| This namespace contains functions for interacting with the currently loaded data
|

------------------------

Functions
---------

``std::mem::align_to(u128 alignment, u128 value) -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::mem::base_address() -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the current base address of the loaded data**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``return``      Current base address
    =============== =========================================================================

------------------------

``std::mem::size() -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the size of the loaded data**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``return``      Size of the loaded data
    =============== =========================================================================

------------------------

``std::mem::find_sequence(u128 occurence_index, u8 bytes...) -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Searches the data for the ``occurence_index``th occurence of a sequence of ``bytes``**


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

``std::mem::read_unsigned(u128 address, u128 size) -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::mem::read_signed(u128 address, u128 size) -> s128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

------------------------

``std::mem::read_string(u128 address, u128 size) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Reads string from memory without declaring a variable**


.. table::
    :align: left

    =================== ===========================================================================
    Parameter           Description
    =================== ===========================================================================
    ``address``         Address of value to read
    ``size``            Size of string to read
    ``return``          String containing a trimmed version of the read string
    =================== ===========================================================================
    
``std::mem::eof() -> bool`` :version:`1.11.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the current offset is at or past the end of the data. Useful for letting an array grow until it encapsulates the entire data**

.. table::
    :align: left

    ============= ===========================================================================
    Parameter     Description
    ============= ===========================================================================
    ``return``    True if the current offset is at or past the end of the data
    ============= ===========================================================================

------------------------
