``std::string``
===============

| This namespace contains functions to deal with strings
|

------------------------

``std::string::length(string)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::string::at(string, index)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::string::substr(string, pos, size)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets a substring of ``string`` starting at ``pos```` of size ``size``**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to index
    ``pos``         Starting position of substring
    ``size``        Size of substring
    ``return``      Substring
    =============== =========================================================================


``std::string::parse_int(string, base)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Parses ``string`` passed into the function as an integer in base ``base`` and returns it**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to parse
    ``base``        Base of integer. Use 0 to use number prefix to determine base 
    ``return``      Parsed integer
    =============== =========================================================================


``std::string::parse_float(string)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Parses ``string`` passed into the function as a float and returns it**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to parse
    ``return``      Parsed float
    =============== =========================================================================
