``Console I/O Library``
=======================

.. code-block:: hexpat

    #include <std/io.pat>

| This namespace contains general utility functions.
|

------------------------

Functions
---------

``std::print(str format, auto args...)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Formats and outputs a set of values to the built-in console**

This function works identical to libfmt's ``fmt::print()`` and C++20's ``std::print()``.
Please check its documentation for further informatiom


.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``format``  A libfmt / C++20 std::format format string
    ``args...`` A list of arguments to be inserted into the format string
    =========== =========================================================

.. code-block:: hexpat

    std::print("Hello {} {}", "World", 42); // Prints "Hello World 42"

------------------------

``std::format(str format, auto args...) -> str``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Formats a set of values and returns the result as a string**

This function works identical to libfmt's ``fmt::format()`` and C++20's ``std::format()``.
Please check its `documentation <https://fmt.dev/latest/syntax.html>`_ for further informatiom


.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``format``  A libfmt / C++20 std::format format string
    ``args...`` A list of arguments to be inserted into the format string
    ``return``  Formatted string
    =========== =========================================================

.. code-block:: hexpat

    return std::format("Hello {} {}", "World", 42); // Returns "Hello World 42"

------------------------

``std::error(str message)`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Throws an error**

When run, execution is aborted immediately with a given message


.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``message`` Message to print
    =========== =========================================================

------------------------

``std::warning(str message)`` :version:`1.14.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Throws an error**

When run, a warning is thrown with a given message


.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``message`` Message to print
    =========== =========================================================

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

``std::env(str var) -> auto`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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