``std``
=======

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
