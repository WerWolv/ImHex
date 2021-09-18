``std``
=======

| This namespace contains general utility functions.
|

------------------------

``std::print(format, args...)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

``std::format(format, args...)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Formats a set of values and returns the result as a string**

This function works identical to libfmt's ``fmt::format()`` and C++20's ``std::format()``.
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

    return std::format("Hello {} {}", "World", 42); // Returns "Hello World 42"

------------------------

``std::assert(condition, message)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Aborts evaluation and prints an error to the console if ``condition`` evaluates to false**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``condition``   Condition that needs to be met
    ``message``     Message to be printed to the console if ``condition`` evaluated to false.
    =============== =========================================================================

------------------------

``std::assert_warn(condition, message)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Prints a warning to the console if ``condition`` evaluates to false**


.. table::
    :align: left

    =============== =========================================================================
    Parameter       Description
    =============== =========================================================================
    ``condition``   Condition that needs to be met
    ``message``     Message to be printed to the console if ``condition`` evaluated to false.
    =============== =========================================================================