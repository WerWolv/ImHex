``std::string`` :version:`Nightly`
==================================

| This namespace contains functions to deal with strings
|

------------------------

Functions
---------

``std::string::length(str string) -> u128``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::string::at(str string, u128 index) -> char``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::string::substr(str string, u128 pos, u128 size) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

------------------------

``std::string::parse_int(str string, u128 base) -> s128`` :version:`Nightly`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

------------------------

``std::string::parse_float(str string) -> double`` :version:`Nightly`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Parses ``string`` passed into the function as a float and returns it**

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to parse
    ``return``      Parsed float
    =============== =========================================================================

------------------------

``std::string::to_string(auto x) -> str`` :version:`Nightly`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Turns a integer, floating point, character or bool into a string**

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``x``           Integral, floating point, character of bool value
    ``return``      Representation of ``x`` as a string
    =============== =========================================================================

------------------------

``std::string::starts_with(str string, str part) -> bool`` :version:`Nightly`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the string ``string`` starts with the string ``part``**

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to inspect
    ``part``        String to match at the beginning of ``string``
    ``return``      True if the string ``string`` starts with the string ``part``
    =============== =========================================================================

------------------------

``std::string::ends_with(str string, str part) -> bool`` :version:`Nightly`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the string ``string`` ends with the string ``part``**

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``string``      String to inspect
    ``part``        String to match at the end of ``string``
    ``return``      True if the string ``string`` ends with the string ``part``
    =============== =========================================================================

------------------------

``std::string::contains(str a, str b) -> bool`` :version:`Nightly`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the string ``string`` ends with the string ``part``**

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``a``           String to inspect
    ``b``           String to find in ``a``
    ``return``      True if the string ``b`` can be found inside of ``a``
    =============== =========================================================================
