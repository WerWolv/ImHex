``Character Type Library`` :version:`1.10.1`
============================================

.. code-block:: hexpat

    #include <std/ctype.pat>

| This namespace contains trait functions to inspect ASCII characters
|

------------------------

Functions
---------

``std::ctype::isdigit(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII digit**

.. table::
    :align: left

    =========== ===========================================================
    Parameter   Description
    =========== ===========================================================
    ``c``       Character
    ``return``  True if character is a decimal ASCII digit (``0`` to ``9``)
    =========== ===========================================================

------------------------

``std::ctype::isxdigit(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a hexadecimal ASCII digit**

.. table::
    :align: left

    =========== =================================================================================================
    Parameter   Description
    =========== =================================================================================================
    ``c``       Character
    ``return``  True if character is a hexadecimal ASCII digit (``0`` to ``9``, ``a`` to ``f`` or ``A`` to ``F``)
    =========== =================================================================================================

------------------------

``std::ctype::isupper(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a uppercase ASCII letter**

.. table::
    :align: left

    =========== ==============================================================
    Parameter   Description
    =========== ==============================================================
    ``c``       Character
    ``return``  True if character is a uppercase ASCII letter (``A`` to ``Z``)
    =========== ==============================================================

------------------------

``std::ctype::islower(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a lowercase ASCII letter**

.. table::
    :align: left

    =========== ==============================================================
    Parameter   Description
    =========== ==============================================================
    ``c``       Character
    ``return``  True if character is a lowercase ASCII letter (``a`` to ``z``)
    =========== ==============================================================

------------------------

``std::ctype::isalpha(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII letter**

.. table::
    :align: left

    =========== ======================================================================
    Parameter   Description
    =========== ======================================================================
    ``c``       Character
    ``return``  True if character is a ASCII letter (``A`` to ``Z`` or ``a`` to ``z``)
    =========== ======================================================================

------------------------

``std::ctype::isalnum(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII letter or digit**

.. table::
    :align: left

    =========== ===============================================================================================
    Parameter   Description
    =========== ===============================================================================================
    ``c``       Character
    ``return``  True if character is a ASCII letter or digit (``0`` to ``9``, ``A`` to ``Z`` or ``a`` to ``z``)
    =========== ===============================================================================================

------------------------

``std::ctype::isspace(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII whitespace character**

.. table::
    :align: left

    =========== ===============================================================================================
    Parameter   Description
    =========== ===============================================================================================
    ``c``       Character
    ``return``  True if character is a ASCII whitespace character (0x20, 0x09, 0x0A, 0x0B, 0x0C or 0x0D)
    =========== ===============================================================================================

------------------------

``std::ctype::isblank(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a blank ASCII character**

.. table::
    :align: left

    =========== ===========================================================
    Parameter   Description
    =========== ===========================================================
    ``c``       Character
    ``return``  True if character is a blank ASCII character (Space or Tab)
    =========== ===========================================================

------------------------

``std::ctype::isprint(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a printable ASCII character**

.. table::
    :align: left

    =========== =====================================================================
    Parameter   Description
    =========== =====================================================================
    ``c``       Character
    ``return``  True if character has a printable symbol (all non-control characters)
    =========== =====================================================================

------------------------

``std::ctype::iscntrl(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII control character**

.. table::
    :align: left

    =========== ========================================
    Parameter   Description
    =========== ========================================
    ``c``       Character
    ``return``  True if character is a control character
    =========== ========================================

------------------------

``std::ctype::isgraph(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII character with a graphical representation**

.. table::
    :align: left

    =========== ===========================================================================================
    Parameter   Description
    =========== ===========================================================================================
    ``c``       Character
    ``return``  True if character has a printable symbol (all printable characters except space characters)
    =========== ===========================================================================================

------------------------

``std::ctype::ispunct(char c) -> bool`` :version:`1.10.1`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Checks if the character ``c`` is a ASCII punctuation character**

.. table::
    :align: left

    =========== ================================================================================================
    Parameter   Description
    =========== ================================================================================================
    ``c``       Character
    ``return``  True if character is a ASCII punctuation character (one of ``!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~``)
    =========== ================================================================================================