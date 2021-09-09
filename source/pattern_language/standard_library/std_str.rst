``std::str``
============

| This namespace contains functions to deal with strings
|

------------------------

``std::str::length(string)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Calculates the length of a given string**

.. table::
    :align: left

    =========== ==================================================
    Parameter   Description
    =========== ==================================================
    ``string``  String to calculate the length of
    ``return``  Length of ``string``
    =========== ==================================================

.. code-block:: hexpat

    std::print("Hello", " ", "World", " ", 42); // Logs "Hello World 42"

------------------------

``std::str::at(string, index)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Get a character at a given index withing a string**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to index
    ``index``       Index to read character from
    ``return``      Character at ``index`` of ``string``
    =============== =========================================================================

------------------------

``std::str::compare(left, right)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if two strings are identical**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``left``        First string 
    ``right``       Second string
    ``return``      True if first string and second string are identical, false otherwise
    =============== =========================================================================