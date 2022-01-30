``std::sys`` :version:`1.10.1`
=================================

.. code-block:: hexpat

    #include <std/sys.pat>

| This namespace contains language support functions
|

------------------------

Functions
---------

------------------------

``std::assert(bool condition, str message)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Aborts evaluation and prints an error to the console if condition evaluates to false**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``condition``   Condition that needs to be met
    ``message``     Message to be printed to the console if ``condition`` evaluated to false.
    =============== =========================================================================

------------------------

``std::assert_warn(bool condition, str message)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Prints a warning to the console if condition evaluates to false**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``condition``   Condition that needs to be met
    ``message``     Message to be printed to the console if ``condition`` evaluated to false.
    =============== =========================================================================

------------------------

``std::env(str name) -> auto`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the value of a environment variable specified in the Environment Variables tab**

.. note::

    The type returned from this function depends on the type specified in the Environment Variables tab.

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``var``         Name of environment variable to query
    ``return``      Integer, floating point, bool or string with the value of that env var
    =============== =========================================================================

------------------------

``std::sizeof_pack(auto ... pack) -> u128`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Gets the number of values contained in a parameter pack**

.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``pack``        Parameter pack
    ``return``      Number of values contained in parameter pack
    =============== =========================================================================